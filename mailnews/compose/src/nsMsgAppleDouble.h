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

#include "msgCore.h"
#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "nsMsgComposeStringBundle.h"


#define NOERR			0
#define errDone			1
								/* Done with current operation.	*/
#define errEOB			2
								/* 	End of a buffer.			*/
#define errEOP			3	
								/* 	End of a Part.				*/

					
#define errFileOpen		NS_MSG_UNABLE_TO_OPEN_TMP_FILE
#define errFileWrite	-202 /*Error writing temporary file.*/
#define errUsrCancel	-2  /*MK_INTERRUPTED */
#define errDecoding		-1

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
	PRInt32	dirId; 
	PRInt16	vRefNum;
	PRInt16	fileId;				/* the id for the open file (data/resource fork) */

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
	PRInt16	vRefNum;
	PRInt32	dirId; 
	PRInt16	fileId;				/* the id for the open file (data/resource fork) */
#endif
	nsIOFileStream *fileSpec;					/* the stream for data fork work.					 */

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


	nsFileSpec          *tmpFileSpec;		/* the temp file to hold the decode data fork 	*/
								                      /* when doing the binhex exporting.				*/
  nsIOFileStream      *tmpFileStream; /* The output File Stream */
	PRInt32	            data_size;			/* the size of the data in the tmp file.		*/

} appledouble_decode_object;


/*
**	The protypes.
*/

PR_BEGIN_EXTERN_C

int ap_encode_init(appledouble_encode_object *p_ap_encode_obj, 
					char* fname,
					char* separator);

int ap_encode_next(appledouble_encode_object* p_ap_encode_obj, 
					char 	*to_buff,
					PRInt32 	buff_size,
					PRInt32*	real_size);

int ap_encode_end(appledouble_encode_object* p_ap_encode_obj,
					PRBool	is_aborting);

int ap_decode_init(appledouble_decode_object* p_ap_decode_obj,
					PRBool	is_apple_single,
					PRBool	write_as_bin_hex,
					void  	*closure);

int ap_decode_next(appledouble_decode_object* p_ap_decode_obj, 
					char 	*in_buff, 
					PRInt32 	buff_size);

int ap_decode_end(appledouble_decode_object* p_ap_decode_obj, 
				 	PRBool is_aborting);

PR_END_EXTERN_C

#endif
