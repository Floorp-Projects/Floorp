/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

/*
** applefile definitions used 
*/
#ifdef XP_MAC
#if PRAGMA_STRUCT_ALIGN
#pragma options align=mac68k
#endif
#endif /* XP_MAC */

#if defined(XP_MACOSX)
#pragma options align=mac68k
#endif /* XP_MACOSX */

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
	PRInt32 	magic;
	PRInt32	version;
	char 	fill[16];
	PRInt16 	entries;

} ap_header;

typedef struct ap_entry 
{
	PRInt32  id;
	PRInt32	offset;
	PRInt32	length;
	
} ap_entry;

typedef struct ap_dates 
{
	PRInt32 create, modify, backup, access;

} ap_dates;

typedef struct myFInfo			/* the mac FInfo structure for the cross platform. */
{	
	PRInt32	fdType, fdCreator;
	PRInt16	fdFlags;
	PRInt32	fdLocation;			/* it really should  be a pointer, but just a place-holder  */
	PRInt16	fdFldr;	

}	myFInfo;

PR_BEGIN_EXTERN_C
/*
**	string utils.
*/
int write_stream(appledouble_encode_object *p_ap_encode_obj,char *s,int	 len);

int fill_apple_mime_header(appledouble_encode_object *p_ap_encode_obj);
int ap_encode_file_infor(appledouble_encode_object *p_ap_encode_obj);
int ap_encode_header(appledouble_encode_object* p_ap_encode_obj, PRBool firstTime);
int ap_encode_data(  appledouble_encode_object* p_ap_encode_obj, PRBool firstTime);

/*
**	the prototypes for the ap_decoder.
*/
int  fetch_a_line(appledouble_decode_object* p_ap_decode_obj, char *buff);
int  ParseFileHeader(appledouble_decode_object* p_ap_decode_obj);
int  ap_seek_part_start(appledouble_decode_object* p_ap_decode_obj);
void parse_param(char *p, char **param, char**define, char **np);
int  ap_seek_to_boundary(appledouble_decode_object* p_ap_decode_obj, PRBool firstime);
int  ap_parse_header(appledouble_decode_object* p_ap_decode_obj,PRBool firstime);
int  ap_decode_file_infor(appledouble_decode_object* p_ap_decode_obj);
int  ap_decode_process_header(appledouble_decode_object* p_ap_decode_obj, PRBool firstime);
int  ap_decode_process_data(  appledouble_decode_object* p_ap_decode_obj, PRBool firstime);

PR_END_EXTERN_C
 
#ifdef XP_MAC
#if PRAGMA_STRUCT_ALIGN
#pragma options align=reset
#endif
#endif /* XP_MAC */

#if defined(XP_MACOSX)
#pragma options align=reset
#endif /* XP_MACOSX */

#endif /* ad_codes_h */
