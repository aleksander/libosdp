/*
 * Copyright (c) 2019 Siddharth Chandrasekaran
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"

void pack_pd_capabilities(struct pd_cap *cap)
{
	struct pd_cap c[CAP_SENTINEL];
	int i, j = 0;

	for (i = 1; i < CAP_SENTINEL; i++) {
		if (cap[i].function_code == 0)
			continue;
		c[j].function_code = cap[i].function_code;
		c[j].compliance_level = cap[i].compliance_level;
		c[j].num_items = cap[i].num_items;
		j++;
	}
	c[j].function_code = -1;
	c[j].compliance_level = 0;
	c[j].num_items = 0;
	j++;

	memcpy(cap, c, j * sizeof(struct pd_cap));
}

int load_scbk(struct config_pd_s *c, uint8_t *buf)
{
	int len;
	FILE *fd;
	char hstr[64 + 1] = { 0 };

	fd = fopen(c->key_store, "r");
	if (fd == NULL)
		return -1;
	fgets(hstr, 64, fd);
	len = strlen(hstr);
	if ((len/2) > 16)
		return -1;
	hstrtoa(buf, hstr);

	return 0;
}

int store_scbk(struct osdp_cmd_keyset *p)
{
	printf("PD got a call to store scbk\n");
	FILE *fd;
	char hstr[64 + 1];
	struct config_pd_s *c;

	c = g_config.pd;
	if (c == NULL)
		return -1;
	fd = fopen(c->key_store, "w");
	if (fd == NULL)
		return -1;
	atohstr(hstr, p->data, p->len);
	fprintf(fd, "%s\n", hstr);

	return 0;
}

int cmd_handler_start(int argc, char *argv[], struct config_s *c)
{
	int i;
	osdp_pd_info_t *info_arr, *info;
	struct config_pd_s *pd;
	osdp_cp_t *cp_ctx;
	osdp_pd_t *pd_ctx;
	uint8_t *scbk, scbk_buf[16];

        ARG_UNUSED(argc);
        ARG_UNUSED(argv);

	info_arr = calloc(c->num_pd, sizeof(osdp_pd_info_t));
	if (info_arr == NULL)
	{
		printf("Failed to alloc info struct\n");
		return -1;
	}

	for (i = 0; i < c->num_pd; i++) {
		info = info_arr + i;
		pd = c->pd + i;
		info->address = pd->address;
		info->baud_rate = pd->channel_speed;
		if (channel_setup(pd)) {
			printf("Failed to setup channel\n");
			return -1;
		}
		if (pd->channel.flush)
			pd->channel.flush(pd->channel.data);
		memcpy(&info->channel, &pd->channel, sizeof(struct osdp_channel));

		if (c->mode == CONFIG_MODE_CP)
			continue;

		memcpy(&info->id, &pd->id, sizeof(struct pd_id));
		pack_pd_capabilities(pd->cap);
		info->cap = pd->cap;
	}

	osdp_set_log_level(c->log_level);

	if (c->mode == CONFIG_MODE_CP) {
		cp_ctx = osdp_cp_setup(c->num_pd, info_arr, c->cp.master_key);
		if (cp_ctx == NULL) {
			printf("Failed to setup CP context\n");
			return -1;
		}
	} else {
		scbk = NULL;
		if (load_scbk(c->pd, scbk_buf) == 0)
			scbk = scbk_buf;
		pd_ctx = osdp_pd_setup(info_arr, scbk);
		if (pd_ctx == NULL) {
			printf("Failed to setup PD context\n");
			return -1;
		}
		osdp_pd_set_callback_cmd_keyset(pd_ctx, store_scbk);
	}

	free(info_arr);

	while (1)
	{
		if (c->mode == CONFIG_MODE_CP)
			osdp_cp_refresh(cp_ctx);
		else
			osdp_pd_refresh(pd_ctx);
		usleep(1000);
	}

	return 0;
}
