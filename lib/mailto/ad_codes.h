/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
**	AD_Codes.h
**
**	---------------
**
**		Head file for Apple Decode/Encode enssential codes.
**
**
*/

#ifndef ad_codes_h
#define ad_codes_h

#include "xp_core.h"

/*
** applefile definitions used 
*/
#ifdef XP_MAC
#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=mac68k
#endif
#endif /* XP_MAC */

#define APPLESINGLE_MAGIC	0x00051600L
#define APPLEDOUBLE_MAGIC 	0x00051607L
#define VERSION 			0x00020000

#define NUM_ENTRIES 		6

#define ENT_DFORK   		1L
#define ENT_RFORK   		2L
#define ENT_NAME    		3L
#define ENT_COMMENT 		4L
#define ENT_DATES   		8L
#define ENT_FINFO   		9L
#define CONVERT_TIME 		1265437696L

/*
** data type used in the encoder/decoder.
*/
typedef struct ap_header 
{
	int32 	magic;
	int32	version;
	char 	fill[16];
	int16 	entries;

} ap_header;

typedef struct ap_entry 
{
	uint32  id;
	uint32	offset;
	uint32	length;
	
} ap_entry;

typedef struct ap_dates 
{
	int32 create, modify, backup, access;

} ap_dates;

typedef struct myFInfo			/* the mac FInfo structure for the cross platform. */
{	
	int32	fdType, fdCreator;
	int16	fdFlags;
	int32	fdLocation;			/* it really should  be a pointer, but just a place-holder  */
	int16	fdFldr;	

}	myFInfo;

XP_BEGIN_PROTOS
/*
**	string utils.
*/
int write_stream(appledouble_encode_object *p_ap_encode_obj,char *s,int	 len);

int fill_apple_mime_header(appledouble_encode_object *p_ap_encode_obj);
int ap_encode_file_infor(appledouble_encode_object *p_ap_encode_obj);
int ap_encode_header(appledouble_encode_object* p_ap_encode_obj, XP_Bool firstTime);
int ap_encode_data(  appledouble_encode_object* p_ap_encode_obj, XP_Bool firstTime);

/*
**	the prototypes for the ap_decoder.
*/
int  fetch_a_line(appledouble_decode_object* p_ap_decode_obj, char *buff);
int  ParseFileHeader(appledouble_decode_object* p_ap_decode_obj);
int  ap_seek_part_start(appledouble_decode_object* p_ap_decode_obj);
void parse_param(char *p, char **param, char**define, char **np);
int  ap_seek_to_boundary(appledouble_decode_object* p_ap_decode_obj, XP_Bool firstime);
int  ap_parse_header(appledouble_decode_object* p_ap_decode_obj,XP_Bool firstime);
int  ap_decode_file_infor(appledouble_decode_object* p_ap_decode_obj);
int  ap_decode_process_header(appledouble_decode_object* p_ap_decode_obj, XP_Bool firstime);
int  ap_decode_process_data(  appledouble_decode_object* p_ap_decode_obj, XP_Bool firstime);

#ifdef XP_MAC
OSErr my_FSSpecFromPathname(char* src_filename, FSSpec* fspec);
char* my_PathnameFromFSSpec(FSSpec* fspec);

#endif
 XP_END_PROTOS
 
#ifdef XP_MAC
#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=reset
#endif
#endif /* XP_MAC */

#endif /* ad_codes_h */
