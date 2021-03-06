/*
MX3100RV - portable driver for the MX-3100 gaming mouse
Copyright (C) 2017  Dan Panzarella

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* memcpy */
#include "usb.h"
#include "mx.h"

#define ENFORCE_ONOFF(arg) do{\
	if (!is_on_off(arg)) {\
		fprintf(stderr, "valid values are 'on' or 'off'.\n"); return -2; \
	} } while(0)

/* internal helpers */
static int write_full_memory(unsigned char *buf);
static int read_section(unsigned char addr, unsigned char *buf);
static int write_section(unsigned char addr, unsigned char *buf);
static int read_settings(unsigned char *config, unsigned char *buttons);
static int write_settings(unsigned char *config, unsigned char *buttons);

static int valid_hex(char *s);
static int is_on_off(char *s);
static int on(char *s);

MXCOMMAND(angle_snap) {
	int err;
	unsigned char settings[SECTION_LEN],
				  buttons[SECTION_LEN];
	err = read_settings(settings, buttons);
	if (err != 0) { return err; }

	if (argc == 0) {
		if (settings[ANGLE_SNAP_ADDR] == ANGLE_SNAP_ENABLED) {
			printf("on\n");
		} else {
			printf("off\n");
		}
		return 0;
	}
	ENFORCE_ONOFF(argv[0]);
	if (on(argv[0])) {
		settings[ANGLE_SNAP_ADDR] = ANGLE_SNAP_ENABLED;
	} else {
		settings[ANGLE_SNAP_ADDR] = ANGLE_SNAP_DISABLED;
	}

	if ( (err=write_settings(settings, buttons)) != 0){
		fprintf(stderr, "Error changing angle snap\n");
	}
	return err;
}

MXCOMMAND(angle_correct) {
	int err, angle;
	unsigned char settings[SECTION_LEN],
				  buttons[SECTION_LEN];
	err = read_settings(settings, buttons);
	if (err != 0) { return err; }

	if (argc == 0) {
		switch (settings[ANGLE_CORRECT_ADDR]) {
			case ANGLE_CORRECT_NEG2: printf("-2\n"); break;
			case ANGLE_CORRECT_NEG1: printf("-1\n"); break;
			case ANGLE_CORRECT_ZERO: printf("0\n"); break;
			case ANGLE_CORRECT_POS1: printf("1\n"); break;
			case ANGLE_CORRECT_POS2: printf("2\n"); break;
			default:
				printf("unknown value: 0x%02x\n", settings[ANGLE_CORRECT_ADDR]);
				break;
		}
		return 0;
	}

	angle = atoi(argv[0]);

	switch (angle) {
		case -2: settings[ANGLE_CORRECT_ADDR] = ANGLE_CORRECT_NEG2; break;
		case -1: settings[ANGLE_CORRECT_ADDR] = ANGLE_CORRECT_NEG1; break;
		case 0: settings[ANGLE_CORRECT_ADDR] = ANGLE_CORRECT_ZERO; break;
		case 1: settings[ANGLE_CORRECT_ADDR] = ANGLE_CORRECT_POS1; break;
		case 2: settings[ANGLE_CORRECT_ADDR] = ANGLE_CORRECT_POS2; break;
		default:
			fprintf(stderr, "invalid number provided. Must be between -2 and 2. %d given\n", angle);
			return -2;
	}
	if ( (err = write_settings(settings,buttons)) != 0){
		fprintf(stderr, "Error changing angle snap\n");
	}
	return err;
}

MXCOMMAND(led_mode) {
	int err;
	unsigned char settings[SECTION_LEN],
				  buttons[SECTION_LEN];
	err = read_settings(settings, buttons);
	if (err != 0) { return err; }

	if (argc == 0) {
		switch (settings[LED_MODE_ADDR]) {
			case LED_MODE_OFF: printf("off\n"); break;
			case LED_MODE_STD: printf("standard\n"); break;
			case LED_MODE_BREATHE: printf("breathe\n"); break;
			case LED_MODE_NEON: printf("neon\n"); break;
			default: printf("unknown value: 0x%02x\n", settings[LED_MODE_ADDR]); break;
		}
		return 0;
	}

	if (strcmp(argv[0],"off") == 0) {
		settings[LED_MODE_ADDR] = LED_MODE_OFF;
	} else if (strcmp(argv[0],"standard") == 0) {
		settings[LED_MODE_ADDR] = LED_MODE_STD;
		settings[LED_CFG_ADDR] = LED_BRIGHT_MAX;
	} else if (strcmp(argv[0],"breathe") == 0) {
		settings[LED_MODE_ADDR] = LED_MODE_BREATHE;
		settings[LED_CFG_ADDR] = LED_SPEED_MIN;
	} else if (strcmp(argv[0],"neon") == 0) {
		settings[LED_MODE_ADDR] = LED_MODE_NEON;
		settings[LED_CFG_ADDR] = LED_SPEED_MIN;
	} else {
		fprintf(stderr, "Invalid argument. Must be one of: off, standard, breathe, neon\n");
		return -2;
	}

	if ((err = write_settings(settings,buttons)) != 0){
		fprintf(stderr, "Error changing LED mode\n");
	}
	return err;
}



MXCOMMAND(led_brightness) {
	int err, value;
	unsigned char settings[SECTION_LEN],
				  buttons[SECTION_LEN];
	err = read_settings(settings, buttons);
	if (err != 0) { return err; }

	if (settings[LED_MODE_ADDR] != LED_MODE_STD) {
		fprintf(stderr, "Brightness is only valid when 'standard' LED mode is active\n");
		return -2;
	}

	if (argc == 0) {
		printf("%d\n", settings[LED_CFG_ADDR]);
		return 0;
	}

	value = atoi(argv[0]);
	if (value < LED_BRIGHT_MIN || value > LED_BRIGHT_MAX) {
		fprintf(stderr, "Brightness value out of range. Must be %d to %d\n", LED_BRIGHT_MIN, LED_BRIGHT_MAX);
		return -2;
	}

	settings[LED_CFG_ADDR] = value;

	if ((err = write_settings(settings,buttons)) != 0){
		fprintf(stderr, "Error changing LED brightness\n");
	}
	return err;
}

MXCOMMAND(led_speed) {
	int err, value;
	unsigned char settings[SECTION_LEN],
				  buttons[SECTION_LEN];
	err = read_settings(settings, buttons);
	if (err != 0) { return err; }

	if (settings[LED_MODE_ADDR] != LED_MODE_NEON && settings[LED_MODE_ADDR] != LED_MODE_BREATHE) {
		fprintf(stderr, "Speed is only valid when 'neon' or 'breathe' LED modes are active\n");
		return -2;
	}

	if (argc == 0) {
		printf("%d\n", settings[LED_CFG_ADDR]);
		return 0;
	}

	value = atoi(argv[0]);
	if (value < LED_SPEED_MIN || value > LED_SPEED_MAX) {
		fprintf(stderr, "Speed value out of range. Must be %d to %d\n", LED_SPEED_MIN, LED_SPEED_MAX);
		return -2;
	}
	settings[LED_CFG_ADDR] = value;

	return write_settings(settings,buttons);
}

MXCOMMAND(sensitivity) {
	int err, value;
	unsigned char addr = 0;
	unsigned char settings[SECTION_LEN],
				  buttons[SECTION_LEN];
	err = read_settings(settings, buttons);
	if (err != 0) { return err; }

	if (argc == 0) {
		fprintf(stderr, "argument X or Y required\n");
		return -2;
	}

	if ( (argv[0][0] == 'x' || argv[0][0] == 'X') && argv[0][1] == '\0' ) {
		addr = SENSITIVITY_X_ADDR;
	} else if ( (argv[0][0] == 'y' || argv[0][0] == 'Y') && argv[0][1] == '\0' ) {
		addr = SENSITIVITY_Y_ADDR;
	} else {
		fprintf(stderr, "invalid argument. Must specify X or Y here\n");
		return -2;
	}

	if (argc == 1) {
		printf("%d\n", settings[addr] / SENSITIVITY_STEP);
		return 0;
	}

	value = atoi(argv[1]);
	if ( value < SENSITIVITY_MIN || value > SENSITIVITY_MAX) {
		fprintf(stderr, "sensitivity out of range. Must be number from %d to %d\n", SENSITIVITY_MIN, SENSITIVITY_MAX);
		return -2;
	}

	settings[addr] = value*SENSITIVITY_STEP;

	if ((err = write_settings(settings,buttons)) != 0){
		fprintf(stderr, "Error changing sensitivity\n");
	}
	return err;
}

MXCOMMAND(dpi_enable) {
	int err, profile;
	unsigned char settings[SECTION_LEN],
				  buttons[SECTION_LEN];
	err = read_settings(settings, buttons);
	if (err != 0) { return err; }
	profile = atoi(argv[0]);
	if (profile < DPI_PROFILE_MIN || profile > DPI_PROFILE_MAX) {
		fprintf(stderr, "DPI profile out of range. Must be %d-%d\n", DPI_PROFILE_MIN,DPI_PROFILE_MAX);
		return -2;
	}

	/* One byte stores the enable flags for all 7 profiles
		Each bit 0-6 represents each profile
	*/
	profile--; 
	if (argc == 1) {
		if ( settings[DPI_ENABLE_ADDR] & (1<<profile) ) {
			printf("on\n");
		} else {
			printf("off\n");
		}
		return 0;
	}

	if (strcmp(argv[1],"on") == 0) {
		settings[DPI_ENABLE_ADDR] |= (1<<profile);
	} else {
		settings[DPI_ENABLE_ADDR] &= ~(1<<profile);
	}
	if ((err = write_settings(settings,buttons)) != 0){
		fprintf(stderr, "Error changing DPI profile\n");
	}
	return err;
}

MXCOMMAND(dpi_color) {
	int err, profile;
	unsigned long new_color;
	char *end;
	unsigned char settings[SECTION_LEN],
				  buttons[SECTION_LEN];
	err = read_settings(settings, buttons);
	if (err != 0) { return err; }
	profile = atoi(argv[0]);
	if (profile < DPI_PROFILE_MIN || profile > DPI_PROFILE_MAX) {
		fprintf(stderr, "DPI profile out of range. Must be %d-%d\n", DPI_PROFILE_MIN,DPI_PROFILE_MAX);
		return -2;
	}
	profile--;

	if (argc == 2) {
		printf("%02x%02x%02x\n",
		       settings[DPI_COLOR_ADDR_START+profile*DPI_COLOR_ADDR_STEP],
		       settings[DPI_COLOR_ADDR_START+profile*DPI_COLOR_ADDR_STEP+1],
		       settings[DPI_COLOR_ADDR_START+profile*DPI_COLOR_ADDR_STEP+2]);
		return 0;
	}

	if (!valid_hex(argv[2])){
		fprintf(stderr, "invalid color. Please specify a 6-character Hex string without '#'\n");
		return -2;
	}

	new_color = strtoul(argv[2],&end,16);
	if (*end != '\0'){
		fprintf(stderr, "Error: failed to parse color, was not valid hex\n");
		return -2;
	}

	settings[DPI_COLOR_ADDR_START+profile*DPI_COLOR_ADDR_STEP] = ( (new_color & 0xff0000) >> 16 );
	settings[DPI_COLOR_ADDR_START+profile*DPI_COLOR_ADDR_STEP+1]=( (new_color & 0x00ff00) >> 8 );
	settings[DPI_COLOR_ADDR_START+profile*DPI_COLOR_ADDR_STEP+2]=( (new_color & 0x0000ff) );

	if ((err = write_settings(settings,buttons)) != 0){
		fprintf(stderr, "Error changing DPI color\n");
	}
	return err;
}

MXCOMMAND(dpi_value) {
	int err, profile, value;
	unsigned char settings[SECTION_LEN],
				  buttons[SECTION_LEN];
	err = read_settings(settings, buttons);
	if (err != 0) { return err; }
	profile = atoi(argv[0]);
	if (profile < DPI_PROFILE_MIN || profile > DPI_PROFILE_MAX) {
		fprintf(stderr, "DPI profile out of range. Must be %d-%d\n", DPI_PROFILE_MIN,DPI_PROFILE_MAX);
		return -2;
	}
	profile--;

	if (argc == 2) {
		printf("%d\n", (settings[DPI_VALUE_ADDR_X+profile]+1)*100);
		return 0;
	}

	value = atoi(argv[2]);
	if (value < DPI_VALUE_MIN || value > DPI_VALUE_MAX) {
		fprintf(stderr, "DPI value out of range. Must be %d-%d\n", DPI_VALUE_MIN, DPI_VALUE_MAX);
		return -2;
	}
	if (value%100 != 0) {
		fprintf(stderr, "DPI must be an even multiple of 100. (100,200,300,..12000)\n");
		return -2;
	}
	value = (value/100)-1;

	settings[DPI_VALUE_ADDR_X+profile] = value;
	settings[DPI_VALUE_ADDR_Y+profile] = value;

	if ((err = write_settings(settings,buttons)) != 0){
		fprintf(stderr, "Error changing DPI value\n");
	}
	return err;
}


MXCOMMAND(factory_reset) {
	unsigned char factory_settings[FULL_BUF] = {0}; /* zero out macro mem */
	(void) argc;
	(void) argv;

	memcpy(factory_settings,factory_config,SECTION_LEN);
	memcpy(factory_settings+SECTION_LEN,factory_buttons, SECTION_LEN );

	return write_full_memory(factory_settings);
}

MXCOMMAND(save_info) {
	FILE *fp;
	int err, i;
	unsigned char *bufp,
				  buf[FULL_BUF];

	if (argc == 0 || (argv[0][0]=='-'&&argv[0][1]=='\0' ) ) {
		fp = stdout;
	} else {
		fp = fopen(argv[0],"wb");
		if (fp == NULL) {
			fprintf(stderr, "Error opening file for writing\n");
			return -1;
		}
	}

	bufp = buf;

	err = read_section(CONFIGS_ADDR,buf);
	if (err !=0) { fprintf(stderr, "Error reading mouse memory\n"); return err; }
	bufp +=SECTION_LEN;
	err = read_section(BUTTONS_ADDR,bufp);
	if (err !=0) { fprintf(stderr, "Error reading mouse memory\n"); return err; }
	bufp += SECTION_LEN;

	for (i=0; i<NUM_MACROS; i++) {
		err = read_section(MACRO_ADDR_START-i,bufp);
		if (err!=0) { fprintf(stderr, "Error reading mouse memory\n"); return err; }
		bufp += SECTION_LEN;
	}

	fwrite(buf, sizeof(unsigned char), FULL_BUF, fp);

	fclose(fp);	/* word of caution:
		we may have just closed stdout here, if no filename was given (or it was '-').
		So any printfs or fprintf(stdout) will be quite a problem. But since to reach
		this scenario, we are printing binary info to stdout, so it's likely being piped
		into a file or into something like xxd, we likely *don't* want to print anything
		else to stdout, since that would mess with the output. So closing is fine, but
		we need to make sure we strictly refrain from printing to stdout. Almost everything
		should go to stderr.
	*/

	return err;
}


int send_startup_cmds(void) {
	int err;

	unsigned char start1[CMD_MSG_LEN] = {0x02,0x00,0x00,0x00,0x00,0x00,0x00,0xfd};
	unsigned char start2[CMD_MSG_LEN] = {0x03,0x00,0x02,0x00,0x00,0x00,0x00,0xfa}; /* @todo poll rate */
	err = send_ctl(start1);
	err = send_ctl(start2);
	return err;
}

static int write_full_memory(unsigned char *buf) {
	int err, i;
	unsigned char config_buf[SECTION_LEN];

	err = read_section(CONFIGS_ADDR,config_buf);

	// Configs
	err = write_section(CONFIGS_ADDR,buf);
	if (err != 0) { fprintf(stderr, "Error writing to memory\n"); return err; }
	buf += SECTION_LEN;
	// Buttons
	err = write_section(BUTTONS_ADDR,buf);
	if (err != 0) { fprintf(stderr, "Error writing to memory\n"); return err; }
	buf += SECTION_LEN;

	for (i=0; i<NUM_MACROS; i++) {
		err = write_section(MACRO_ADDR_START-i,buf);
		if (err != 0) { fprintf(stderr, "Error writing to memory\n"); return err; }
		buf += SECTION_LEN;
	}
	return err;
}

static int read_settings(unsigned char *config, unsigned char *buttons) {
	int err;
	err = read_section(CONFIGS_ADDR, config);
	if (err != 0) {
		fprintf(stderr, "Error retrieving mouse info\n");
		return -2;
	}
	err = read_section(BUTTONS_ADDR, buttons);
	if (err != 0) {
		fprintf(stderr, "Error retrieving mouse info\n");
		return -2;
	}
	return 0;
}
static int write_settings(unsigned char *config, unsigned char *buttons) {
	int err;
	err = write_section(CONFIGS_ADDR, config);
	if (err != 0) { return err; }
	return write_section(BUTTONS_ADDR, buttons);
}

static int read_section(unsigned char addr, unsigned char *buf) {
	unsigned char cmd[CMD_MSG_LEN] = {ADDR_READ,0x00,0x00,0x00,0x00,0x00,0x00,addr};
	unsigned char rsp[CMD_MSG_LEN];
	int err;

	if ( addr == CONFIGS_ADDR || addr == BUTTONS_ADDR ) {
		cmd[0] |= SETTINGS_ADDR_MAX-addr + SETTINGS_ADDR_PARITY;
	} else {
		cmd[0] |= MACRO_MEM_FLAG;
		cmd[1] = MACRO_ADDR_PARITY-addr;
	}

	err = send_ctl(cmd);
	if (err != 0){
		return err;
	}
	err = read_ctl(rsp);
	if (err != 0) {
		return err;
	}

	if (rsp[0] != cmd[0]) {
		fprintf(stderr, "CMD 0x%02x%02x%02x%02x%02x%02x%02x%02x received weird ACK 0x%02x%02x%02x%02x%02x%02x%02x%02x\n", 
		        cmd[0],cmd[1],cmd[2],cmd[3],cmd[4],cmd[5],cmd[6],cmd[7],
		        rsp[0],rsp[1],rsp[2],rsp[3],rsp[4],rsp[5],rsp[6],rsp[7]);
	}

	err = read_data( buf );
	err = read_data( &buf[DATA_LINE_LEN] );

	return err;
}

static int write_section(unsigned char addr, unsigned char *buf) {
	unsigned char cmd[CMD_MSG_LEN] = {0x00,0x00,SECTION_LEN,0x00,0x00,0x00,0x00,addr};
	int err;

	if ( addr == CONFIGS_ADDR || addr == BUTTONS_ADDR ) {
		cmd[0] = SETTINGS_ADDR_MAX-addr + SETTINGS_ADDR_PARITY;
	} else {
		cmd[0] = MACRO_MEM_FLAG;
		cmd[1] = MACRO_ADDR_PARITY-addr;
	}

	err = send_ctl(cmd);
	if (err != 0) {
		return err;
	}

	err = send_data(buf);
	err = send_data(&buf[DATA_LINE_LEN]);

	return err;
}

static int valid_hex(char *s) { return strlen(s) == 6 && strspn(s,"0123456789abcdefABCDEF") == 6; }
static int on(char *s) { return s[1] == 'n'; }

static int is_on_off(char *s) {
	int len = strlen(s);
	if (len < 2 || len > 3 || (s[0] != 'o' && s[0] != 'O') ) {
		return 0;
	}
	if ( (s[1] == 'n' || s[1] =='N') && s[2] == '\0' ) {
		return 1;
	} else if ( (s[1]=='f'||s[1]=='F') && (s[2]=='f'||s[2]=='F') ){
		return 1;
	}
	return 0;
}
