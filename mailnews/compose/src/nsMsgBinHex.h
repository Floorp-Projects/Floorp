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
*	The head file for Bin Hex 4.0 encode/decode 
* 	-------------------------------------------
*	
*		10sep95	mym	created
*/

#ifndef	binhex_h
#define	binhex_h

#include "nsFileSpec.h"
#include "nsFileStream.h"

#ifdef XP_MAC
#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=mac68k
#endif
#endif /* XP_MAC */

#define BINHEX_STATE_START  	0
#define BINHEX_STATE_FNAME  	1
#define BINHEX_STATE_HEADER 	2
#define BINHEX_STATE_HCRC   	3
#define BINHEX_STATE_DFORK  	4
#define BINHEX_STATE_DCRC   	5
#define BINHEX_STATE_RFORK  	6
#define BINHEX_STATE_RCRC   	7
#define BINHEX_STATE_FINISH   	8
#define BINHEX_STATE_DONE  		9
/* #define BINHEX_STATE_ERROR		10 */

/*
**	The Definitions for the binhex encoder
*/
typedef struct _binhex_header
{
	uint32	type, creator;
	uint16  flags;
	int32	dlen, rlen ;

} binhex_header;

typedef	struct _binhex_encode_object
{
	int		state;				/*	progress state.				*/
	
	int 	state86;			/*	binhex encode state.		*/
	unsigned long CRC;			/*  accumulated CRC				*/
	int		line_length;		/*	the line length count 		*/
	char	saved_bits;
	
	int		s_inbuff;			/*	the inbuff size				*/
	int		pos_inbuff;			/*  the inbuff position			*/
	char*	inbuff;				/*	the inbuff pool				*/
	
	int		s_outbuff;			/*	the outbuff size			*/
	int		pos_outbuff;		/*	the outbuff position		*/
	char*	outbuff;			/*	the outbuf pool				*/
	
	int		s_overflow;			/*	the real size of overflow	*/
	char	overflow[32];		/*	a small overflow buffer		*/
	
	char	c[2];
	char	newline[4];			/*	the new line char seq.		*/

	/* -- for last fix up. -- */

	char 			name[64];
	binhex_header	head;
	
} binhex_encode_object;

/*
**	The defination for the binhex decoder.
**	NOTE:	This define is for Mac only.
*/

typedef union 
{
	unsigned char c[4];
	uint32 		  val;

} longbuf;

#define MAX_BUFF_SIZE			256

typedef struct _binhex_decode_object
{
	int	    state;			/* current state 						*/
	uint16  CRC;			/* cumulative CRC 						*/
	uint16  fileCRC;		/* CRC value from file 					*/

	longbuf octetbuf;		/* buffer for decoded 6-bit values 		*/
	int16 	octetin;		/* current input position in octetbuf 	*/
	int16 	donepos;		/* ending position in octetbuf 			*/
	int16 	inCRC;			/* flag set when reading a CRC 			*/

	int32 	count;			/* generic counter 						*/
	int16 	marker;			/* flag indicating maker 				*/
	unsigned char rlebuf;	/* buffer for last run length encoding value */

	binhex_header head;		/* buffer for header 					*/

#ifdef XP_MAC
	FSSpec*  mSpec;
	char 	name[64];		/* fsspec for the output file 		*/
	int16	vRefNum;
	int32	parID ;
	int16	fileId;			/* the refnum of the output file 		*/
#else
	nsFileSpec      *name;			/* file spec for the output file in non-mac OS */
	nsIOFileStream  *fileId;			/* the file if for the outpur file. non-mac OS */
#endif
	
	int32	s_inbuff;		/* the valid size of the inbuff			*/
	int32	pos_inbuff;		/* the index of the inbuff.				*/
	char*	inbuff;			/* the inbuff pointer.					*/
	int32	pos_outbuff;	/* the position of the out buff.		*/		
	char	outbuff[MAX_BUFF_SIZE];
	
} binhex_decode_object;

XP_BEGIN_PROTOS

/*
**  The binhex file encode prototypes. 
*/
int binhex_encode_init(binhex_encode_object *p_bh_encode_obj);

int binhex_encode_next(binhex_encode_object *p_bh_encode_obj, 
					char	*in_buff,
					int32	in_size,
					char 	*out_buff, 
					int32	buff_size,
					int32	*real_size);
					
int binhex_encode_end (binhex_encode_object *p_bh_encode_obj, 
					XP_Bool is_aborting);
					
int binhex_reencode_head(
					binhex_encode_object *p_bh_encode_obj,
					char* 	outbuff,
					int32	buff_size,
					int32*  real_size);


/*
**	The binhex stream decode prototypes. 
*/

int binhex_decode_init(binhex_decode_object *p_bh_decode_env);
					
int binhex_decode_next(binhex_decode_object *p_bh_decode_env, 
					const char 	*in_buff, 
					int32 		buff_size); 
int binhex_decode_end (binhex_decode_object *p_bh_decode_env, 
					XP_Bool 	is_aborting);

XP_END_PROTOS

#ifdef XP_MAC
#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=reset
#endif
#endif /* XP_MAC */

#endif /* binhex_h */
