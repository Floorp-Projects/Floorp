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
*   AppleDouble.h
*	-------------
*
*  	  The header file for a stream based apple single/double encodor/decodor.
*		
*		2aug95	mym		
*		
*/


#ifndef AppleDouble_h
#define AppleDouble_h

#include "xp.h"
#include "xp_file.h"
#include "msg.h"

#define NOERR			0
#define errDone			1
								/* Done with current operation.	*/
#define errEOB			2
								/* 	End of a buffer.			*/
#define errEOP			3	
								/* 	End of a Part.				*/

					
#define errMemoryAlloc 	MK_OUT_OF_MEMORY
#define errDataCrupt	-1
#define errDiskFull		MK_DISK_FULL
#define errFileOpen		MK_UNABLE_TO_OPEN_TMP_FILE

#define errVersion		-1
#define errFileWrite	MK_MIME_ERROR_WRITING_FILE
#define errDecoding		-1

#define errUsrCancel	MK_INTERRUPTED
/*
** The envirment block data type. 
*/
enum 
{ 
	kInit, 
	kDoingHeaderPortion, 
	kDoneHeaderPortion, 
	kDoingDataPortion, 
	kDoneDataPortion 
};

typedef struct _appledouble_encode_object 
{
	char	fname[64];
	int32	dirId; 
	int16	vRefNum;
	int16	fileId;				/* the id for the open file (data/resource fork) */

	int 	state;
	int		text_file_type;		/* if the file has a text file type with it.	*/
	char	*boundary;			/* the boundary string.							*/

	int		status;				/* the error code if anyerror happens.			*/
	char 	b_overflow[200];
	int		s_overflow;
	
	int		state64;			/* the left over state of base64 enocding 		*/
	int		ct;					/* the character count of base64 encoding		*/
	int 	c1, c2;				/* the left of the last base64 encoding 		*/		

	char 	*outbuff;			/* the outbuff by the caller.           		*/
	int		s_outbuff;			/* the size of the buffer.						*/
	int		pos_outbuff;		/* the offset in the current buffer.			*/ 

} appledouble_encode_object;

/* The possible content transfer encodings */

enum 
{ 
	kEncodeNone,
	kEncodeQP,
	kEncodeBase64,
	kEncodeUU
};

enum 
{ 
	kGeneralMine,
	kAppleDouble,
	kAppleSingle
};

enum 
{ 
	kInline,
	kDontCare
};

enum 
{ 
	kHeaderPortion,
	kDataPortion
};

/* the decode states.	*/
enum 
{ 
	kBeginParseHeader = 3,
	kParsingHeader,
	kBeginSeekBoundary,
	kSeekingBoundary,
	kBeginHeaderPortion, 
	kProcessingHeaderPortion, 
	kBeginDataPortion, 
	kProcessingDataPortion, 
	kFinishing
};

/* uuencode states */
enum
{
	kWaitingForBegin = (int) 0,
	kBegin,
	kMainBody,
	kEnd
};

typedef struct _appledouble_decode_object 
{
	int		is_binary;
	int		is_apple_single;	/* if the object encoded is in apple single		*/
	int		write_as_binhex;
	
	int		messagetype;
	char*	boundary0;			/* the boundary for the enclosure.				*/
	int		deposition;			/* the deposition.								*/
	int		encoding;			/* the encoding method.							*/
	int		which_part;
	
	char	fname[256];
#ifdef XP_MAC
	FSSpec* mSpec;			/* the filespec to save the file to*/
	int16	vRefNum;
	int32	dirId; 
	int16	fileId;				/* the id for the open file (data/resource fork) */
#endif
	XP_File	fd;					/* the fd for data fork work.					 */

	MWContext *context;
    NET_StreamClass* binhex_stream;		/* the stream to output as binhex output.*/

	int 	state;
	
	int		rksize;				/* the resource fork size count.				*/
	int		dksize;				/* the data fork size count.					*/
	 
	int		status;				/* the error code if anyerror happens.			*/
	char 	b_leftover[256];
	int		s_leftover;
	
	int		encode;				/* the encode type of the message.				*/
	int		state64;			/* the left over state of base64 enocding 		*/
	int		left;				/* the character count of base64 encoding		*/
	int 	c[4];				/* the left of the last base64 encoding 		*/		
	int		uu_starts_line;		/* is decoder at the start of a line? (uuencode)	*/
	int		uu_state;			/* state w/r/t the uuencode body */
	int		uu_bytes_written;	/* bytes written from the current tuple (uuencode) */
	int		uu_line_bytes;		/* encoded bytes remaining in the current line (uuencode) */

	char 	*inbuff;			/* the outbuff by the caller.           		*/
	int		s_inbuff;			/* the size of the buffer.						*/
	int		pos_inbuff;			/* the offset in the current buffer.			*/ 


	char*	tmpfname;			/* the temp file to hold the decode data fork 	*/
								/* when doing the binhex exporting.				*/
	XP_File	tmpfd;
	int32	data_size;			/* the size of the data in the tmp file.		*/

} appledouble_decode_object;


/*
**	The protypes.
*/

XP_BEGIN_PROTOS

int ap_encode_init(appledouble_encode_object *p_ap_encode_obj, 
					char* fname,
					char* separator);

int ap_encode_next(appledouble_encode_object* p_ap_encode_obj, 
					char 	*to_buff,
					int32 	buff_size,
					int32*	real_size);

int ap_encode_end(appledouble_encode_object* p_ap_encode_obj,
					XP_Bool	is_aborting);

int ap_decode_init(appledouble_decode_object* p_ap_decode_obj,
					XP_Bool	is_apple_single,
					XP_Bool	write_as_bin_hex,
					void  	*closure);

int ap_decode_next(appledouble_decode_object* p_ap_decode_obj, 
					char 	*in_buff, 
					int32 	buff_size);

int ap_decode_end(appledouble_decode_object* p_ap_decode_obj, 
				 	XP_Bool is_aborting);

XP_END_PROTOS

#endif
