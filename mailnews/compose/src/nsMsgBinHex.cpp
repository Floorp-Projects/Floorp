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
*
*	Mac_BinHex.c
*	------------
*
*	The decode and encode for BinHex 4.0
*
*		09sep95	mym		Created
*		18sep95	mym		Added the functions to do encoding from 
*						the input stream instead of file.
*/

#include "nscore.h"
#include "msgCore.h"
#include "nsCRT.h"

#include "nsMsgAppleDouble.h"
#include "nsMsgAppleCodes.h"
#include "nsMsgBinHex.h"

/* for XP_GetString() */
#include "xpgetstr.h"

#ifdef XP_MAC
#include <StandardFile.h>
#pragma warn_unusedarg off
#endif

static char BinHexTable[64] = 
{
	0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
	0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x30, 0x31, 0x32,
	0x33, 0x34, 0x35, 0x36, 0x38, 0x39, 0x40, 0x41,
	0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
	0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x50, 0x51, 0x52,
	0x53, 0x54, 0x55, 0x56, 0x58, 0x59, 0x5a, 0x5b,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x68,
	0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x70, 0x71, 0x72 
};

/*
 * 	The encode for bin hex format.
 */
 
PRIVATE int binhex_fill_data(
	binhex_encode_object* p_bh_encode_obj, char c)
{
	int i;
	
	if (p_bh_encode_obj->pos_outbuff >= p_bh_encode_obj->s_outbuff)
	{
		p_bh_encode_obj->overflow[p_bh_encode_obj->s_overflow++] = c;
	}
	else
	{
		p_bh_encode_obj->outbuff[p_bh_encode_obj->pos_outbuff++] =c;
	}
	
	if (++p_bh_encode_obj->line_length == 64)
	{
		/*
		**  Incase the new line is 2 character. LRCR
		*/
		for(i = 1; i <= p_bh_encode_obj->newline[0]; i++)
			binhex_fill_data(p_bh_encode_obj, p_bh_encode_obj->newline[i]);
			
		p_bh_encode_obj->line_length = 0;
	}
	
	return p_bh_encode_obj->s_overflow ? errEOB : NOERR;
}

/************************************************************************
 * EncodeDataChar - encode an 8-bit data char into a six-bit buffer
 * returns the number of valid encoded characters generated
 ************************************************************************/
PRIVATE int binhex_encode_data_char(
	binhex_encode_object *p_bh_encode_obj, 
	unsigned char c)
{
	int status = 0;

	switch (p_bh_encode_obj->state86++)
	{
		case 0:
			status = binhex_fill_data(p_bh_encode_obj,
							BinHexTable[(c>>2)&0x3f]);
			p_bh_encode_obj->saved_bits = (c&0x3)<<4;
			break;
		case 1:
			status = binhex_fill_data(p_bh_encode_obj, 
							BinHexTable[p_bh_encode_obj->saved_bits | ((c>>4)&0xf)]);
			p_bh_encode_obj->saved_bits = (c&0xf)<<2;
			break;
		case 2:
			status = binhex_fill_data(p_bh_encode_obj, 
							BinHexTable[p_bh_encode_obj->saved_bits | ((c>>6)&0x3)]);
			if (status != NOERR)
				break;
			
			status = binhex_fill_data(p_bh_encode_obj, 
							BinHexTable[c&0x3f]);
			p_bh_encode_obj->state86 = 0;
			break;
	}
	return status;
} 

#define BYTEMASK 	0xff
#define BYTEBIT 	0x100
#define WORDMASK 	0xffff
#define WORDBIT 	0x10000
#define CRCCONSTANT 0x1021

#define WOW { \
		c <<= 1; \
		if ((temp <<= 1) & WORDBIT) \
			temp = (temp & WORDMASK) ^ CRCCONSTANT; \
		temp ^= (c >> 8); \
		c &= BYTEMASK; \
	}

PRIVATE void binhex_comp_q_crc_out(
	binhex_encode_object *p_bh_encode_obj, uint16 c)
{
	register uint32 temp = p_bh_encode_obj->CRC;

	WOW;
	WOW;
	WOW;
	WOW;
	WOW;
	WOW;
	WOW;
	WOW;
	p_bh_encode_obj->CRC = temp;
}

PRIVATE int binhex_encode_buff(
	binhex_encode_object *p_bh_encode_obj,
	unsigned char* 	data, 
	int 	size)
{
	int  i, status = 0;
	unsigned char dc;
	
	for (i = 0; i < size; i++)
	{
		dc = *data++;

		status = binhex_encode_data_char(p_bh_encode_obj, dc);
		if ((char)dc == (char)0x90)
			status = binhex_encode_data_char(p_bh_encode_obj, 0);
			
		if (status != NOERR)
			break;
			
		binhex_comp_q_crc_out(p_bh_encode_obj, dc);		/* and compute the CRC too */ 
	}
	return status;
}

PRIVATE int binhex_encode_end_a_part(
	binhex_encode_object* p_bh_encode_obj)
{
	int		status;
	uint16  tempCrc;
	
	/*
	** write the CRC to the encode.
	*/
	binhex_comp_q_crc_out(p_bh_encode_obj, 0);
	binhex_comp_q_crc_out(p_bh_encode_obj, 0);
	tempCrc = (uint16)(p_bh_encode_obj->CRC & 0xffff);
	tempCrc = htons(tempCrc);
	status = binhex_encode_buff(p_bh_encode_obj,
					(unsigned char*)&tempCrc, 
					sizeof(uint16));
	p_bh_encode_obj->CRC = 0;
	return status;
}

PRIVATE int binhex_encode_finishing(
	binhex_encode_object *p_bh_encode_obj)
{
	int i, status = 0;
	
	if (p_bh_encode_obj->state86) 
		status = binhex_encode_buff(p_bh_encode_obj, (unsigned char*)&status, 1);
	
	/*
	**	The close token.
	*/	
	status = binhex_fill_data(p_bh_encode_obj, ':');

	for (i=1; i <= p_bh_encode_obj->newline[0]; i++)
		status = binhex_fill_data(p_bh_encode_obj, 
					p_bh_encode_obj->newline[i]);

	return errDone;		
}


int binhex_encode_init(binhex_encode_object *p_bh_encode_obj)
{	
	/*
	** init all the status.
	*/
	memset(p_bh_encode_obj, 0, sizeof(binhex_encode_object));
	
	p_bh_encode_obj->line_length = 1;
	
	p_bh_encode_obj->newline[0] = 2;
	p_bh_encode_obj->newline[1] = CR;
	p_bh_encode_obj->newline[2] = LF;		/*	to confirm with rfc822, use CRLF	*/
	
	return NOERR;
}

int binhex_encode_next(
	binhex_encode_object* p_bh_encode_obj, 
	char	*in_buff,
	int32	in_size,
	char 	*out_buff, 
	int32	buff_size,
	int32	*real_size)
{
	int status = 0;
	/*
	** setup the buffer information.
	*/
	p_bh_encode_obj->outbuff 	 = out_buff;
	p_bh_encode_obj->s_outbuff 	 = buff_size;
	p_bh_encode_obj->pos_outbuff = 0;
	
	/*
	** copy over the left over from last time.
	*/
	if (p_bh_encode_obj->s_overflow)
	{
		memcpy(p_bh_encode_obj->overflow, 
		       p_bh_encode_obj->outbuff, 
		       p_bh_encode_obj->s_overflow);
				
		p_bh_encode_obj->pos_outbuff = p_bh_encode_obj->s_overflow;
		p_bh_encode_obj->s_overflow = 0;
	} 
	
	/*
	** 	Jump to the right state.
	*/
	if ( p_bh_encode_obj->state < BINHEX_STATE_DONE)
	{
		if (in_buff == NULL && in_size == 0)
		{
			/* this is our special token of end of a part, time to append crc codes	*/
			if (p_bh_encode_obj->state != BINHEX_STATE_FINISH)			
				status = binhex_encode_end_a_part(p_bh_encode_obj);
			else
				status = binhex_encode_finishing(p_bh_encode_obj);
			
			p_bh_encode_obj->state += 2;		/* so we can jump to the next state.*/
		}
		else
		{
			if  (p_bh_encode_obj->state == BINHEX_STATE_START)
			{
				PL_strcpy(p_bh_encode_obj->outbuff + p_bh_encode_obj->pos_outbuff,
							"\r\n(This file must be converted with BinHex 4.0)\r\n\r\n:");
				p_bh_encode_obj->pos_outbuff += 52;
			
				p_bh_encode_obj->state = BINHEX_STATE_HEADER;
				
				memcpy(p_bh_encode_obj->name,
				       in_buff, 
				       in_size);	
			}
			else if  (p_bh_encode_obj->state == BINHEX_STATE_HEADER)
			{
				memcpy(&(p_bh_encode_obj->head),
				       in_buff, 
				       sizeof(binhex_header));
				
				if (in_size == 20)	/* in the platform that alignment is 4-bytes. */
					in_size = 18;
							
				p_bh_encode_obj->head.dlen = 0;		/* we just can't trust the dlen from 	*/
													/* apple double decoder told us. 		*/
													/* do our own counting.					*/				
			}			
			else if  (p_bh_encode_obj->state == BINHEX_STATE_DFORK)
			{
				if (p_bh_encode_obj->head.dlen == 0)
				{
					p_bh_encode_obj->c[0] = in_buff[0];	/* save the first 2 bytes, in case  */
					p_bh_encode_obj->c[1] = in_buff[1]; /* head and data share 1 code block */
				}
				p_bh_encode_obj->head.dlen += in_size;
			}
			
			status 	= binhex_encode_buff(p_bh_encode_obj, 
							(unsigned char *)in_buff, 
							in_size);
		}
	}				
	*real_size = p_bh_encode_obj->pos_outbuff;
	return status;
}

/*
**	Only generate the header part of the encoding,
**	 	so we can fix up the 
*/
int binhex_reencode_head(
	binhex_encode_object *p_bh_encode_obj,
	char* 	outbuff,
	int32	buff_size,
	int32*  real_size)
{
	int32	size, dlen;
	int 	status;
	char	buff[64];
	
	p_bh_encode_obj->state	 	= 0;	
	p_bh_encode_obj->state86 	= 0;	
	p_bh_encode_obj->CRC		= 0;
	p_bh_encode_obj->line_length= 1;
	p_bh_encode_obj->saved_bits = 0;
	p_bh_encode_obj->s_overflow = 0	;
	
	status = binhex_encode_next(
						p_bh_encode_obj, 
						p_bh_encode_obj->name,		
						p_bh_encode_obj->name[0]+2,		/* in_size */
						outbuff, 
						buff_size,
						real_size);
	if (status != NOERR)
		return status;
		
	size = *real_size;
	
	/* now we should have the right data length in the head structure, but don't 		*/
	/* forget convert it back to the net byte order (i.e., Motolora) before write it	*/
	/*																					*/
	/* Note:	since we don't change the size of rlen, so don't need to worry about it	*/
	
	p_bh_encode_obj->head.dlen = htonl(dlen = p_bh_encode_obj->head.dlen);
	
	/* make a copy before do the encoding, -- it may modify the head!!!. */
	memcpy(buff, (char*)&p_bh_encode_obj->head, 
	       sizeof(binhex_header));
	if (18 < sizeof(binhex_header))
	{
		/* we get an alignment problem here.	*/
        memcpy(buff + 10, buff + 12, 8);
	}
									
	status = binhex_encode_next(
						p_bh_encode_obj, 
						(char*)buff,		
						18,						/* sizeof(binhex_header),*/
						outbuff   + size, 
						buff_size - size,
						real_size);
	if (status != NOERR)
		return status;
	
	size += *real_size;
		
	status = binhex_encode_next(					/* for CRC */
						p_bh_encode_obj, 
						NULL,		
						0,							/* in_size */
						outbuff  + size, 
						buff_size - size,
						real_size);
	
	if (status != NOERR)
		return status;
		
	size += *real_size;

	if (p_bh_encode_obj->state86 != 0)
	{
		/*	
		**	Make sure we don't destroy the orignal valid coding.
		**
		**	(Keep in mind that 3 characters share 4 coding chars,
		**	 so it is possible for the head and data stream share one 4 code group.
		**
		**	How about only one or zero character in the data fork?
		**		---- just rerun the encoding, not a big deal.
		*/
		if (dlen <= 1)
		{
			/* why just rerun the encoding once more.	*/
			status = binhex_encode_next(
						p_bh_encode_obj,
						p_bh_encode_obj->c,
						dlen,
						outbuff   + size,
						buff_size - size,
						real_size);
			if (status != NOERR)
				return status;
				
			size += *real_size;							/* encode the data fork		*/
			
			status = binhex_encode_next(
						p_bh_encode_obj,
						NULL,
						0,
						outbuff   + size,
						buff_size - size,
						real_size);
			if (status != NOERR)
				return status;

			size += *real_size;							/* for the end up data fork */
			
			status = binhex_encode_next(
						p_bh_encode_obj,
						NULL,
						0,
						outbuff   + size,
						buff_size - size,
						real_size);						/* for the end up encoding*/
		}
		else
		{
			status = binhex_encode_next(
						p_bh_encode_obj, 
						p_bh_encode_obj->c,		
						3 - p_bh_encode_obj->state86,	/* in_size */
						outbuff   + size, 
						buff_size - size,
						real_size);
		}
		size += *real_size;	
	}
	*real_size = size;
	
	return status;
}

int binhex_encode_end (
	binhex_encode_object *p_bh_encode_obj, 
	XP_Bool is_aborting)
{
	return NOERR;
}


/*
**	The decode's.
*/
static char binhex_decode[256] = 
{
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, -1, -1,
	13, 14, 15, 16, 17, 18, 19, -1, 20, 21, -1, -1, -1, -1, -1, -1,
	22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, -1,
	37, 38, 39, 40, 41, 42, 43, -1, 44, 45, 46, 47, -1, -1, -1, -1,
	48, 49, 50, 51, 52, 53, 54, -1, 55, 56, 57, 58, 59, 60, -1, -1,
	61, 62, 63, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

#define BHEXVAL(c) (binhex_decode[(unsigned char) c])

/*
**	the decode for bin hex format.
*/
int binhex_decode_init (
	binhex_decode_object *p_bh_decode_obj)
{
	memset(p_bh_decode_obj, 0, sizeof(binhex_decode_object));
	
	p_bh_decode_obj->octetin 	= 26;
	p_bh_decode_obj->donepos 	= 3;

	return NOERR;
}

PRIVATE void binhex_process(
	binhex_decode_object *p_bh_decode_obj)
{
	int32  	status;
	uint16 	tmpcrc, cval;
	unsigned char  ctmp, c = p_bh_decode_obj->rlebuf;
	
	/* do CRC */
	ctmp = p_bh_decode_obj->inCRC ? c : 0;
	cval = p_bh_decode_obj->CRC   & 0xf000;
	tmpcrc = ((uint16) (p_bh_decode_obj->CRC << 4) | 
						 (ctmp >> 4))
					 ^ (cval | (cval >> 7) | 
					 	(cval >> 12));
	cval = tmpcrc & 0xf000;
	p_bh_decode_obj->CRC = ((uint16) (tmpcrc << 4) | 
							(ctmp & 0x0f))
						 ^ (cval | (cval >> 7) | 
						 	(cval >> 12));

	/* handle state */
	switch (p_bh_decode_obj->state) 
	{
		case BINHEX_STATE_START:
			p_bh_decode_obj->state 		= BINHEX_STATE_FNAME;
			p_bh_decode_obj->count 		= 1;
#ifndef XP_MAC
			p_bh_decode_obj->name = XP_ALLOC(64);
#endif
			*(p_bh_decode_obj->name) = (c & 63);
			break;
			
		case BINHEX_STATE_FNAME:
			p_bh_decode_obj->name[p_bh_decode_obj->count] = c;
			
			if (p_bh_decode_obj->count++ > *(p_bh_decode_obj->name)) 
			{
#if 0			
				char* p;
				/* convert it to the c-string too. 		*/
				c = *(p_bh_decode_obj->name);
				p = p_bh_decode_obj->name;
			
				while (c--)
				{
					*p = *(p+1); p++;
				}
					
				*p = '\0';
#endif				
				p_bh_decode_obj->state = BINHEX_STATE_HEADER;
				p_bh_decode_obj->count = 0;
			}
			break;
			
		case BINHEX_STATE_HEADER:
			((char *)&p_bh_decode_obj->head)[p_bh_decode_obj->count] = c;
			if (++p_bh_decode_obj->count == 18) 
			{
#ifndef XP_MAC
				if (sizeof(binhex_header) != 18)	/* fix the alignment problem in some OS */
				{
					char *p = (char *)&p_bh_decode_obj->head;
					p += 19;
					for (c = 0; c < 8; c++)
					{
						*p = *(p-2);	p--;
					}
 				}
#endif
				p_bh_decode_obj->state = BINHEX_STATE_HCRC;
				p_bh_decode_obj->inCRC = 1;
				p_bh_decode_obj->count = 0;
			}
			break;
			
		case BINHEX_STATE_DFORK:
		case BINHEX_STATE_RFORK:
			p_bh_decode_obj->outbuff[p_bh_decode_obj->pos_outbuff++] = c;
			if (-- p_bh_decode_obj->count == 0) 
			{
#ifdef XP_MAC	
				long howMuch = p_bh_decode_obj->pos_outbuff;
				status = FSWrite(p_bh_decode_obj->fileId, 
								&howMuch,
								p_bh_decode_obj->outbuff);								
				FSClose(p_bh_decode_obj->fileId);
#else				
								/* only output data fork in the non-mac system.			*/
				if (p_bh_decode_obj->state == BINHEX_STATE_DFORK)
				{
					status = p_bh_decode_obj->fileId->write(p_bh_decode_obj->outbuff, 
                                                  p_bh_decode_obj->pos_outbuff)
									    == p_bh_decode_obj->pos_outbuff ? NOERR : errFileWrite;
								
					p_bh_decode_obj->fileId->close();
				}
				else
				{
					status = NOERR;				/* do nothing for resource fork.	*/
				}
#endif
				p_bh_decode_obj->pos_outbuff = 0;
				
				if (status != NOERR)
					p_bh_decode_obj->state = status;
				else
				{
					p_bh_decode_obj->state ++;
					p_bh_decode_obj->fileId = 0;
				}
				p_bh_decode_obj->inCRC = 1;
			}
			else if (p_bh_decode_obj->pos_outbuff >= MAX_BUFF_SIZE)
			{				
#ifdef XP_MAC	
				long howMuch = p_bh_decode_obj->pos_outbuff;
				status = FSWrite(p_bh_decode_obj->fileId, 
								&howMuch,
								p_bh_decode_obj->outbuff);
#else
				if (p_bh_decode_obj->state == BINHEX_STATE_DFORK)
				{
					status = p_bh_decode_obj->fileId->write(p_bh_decode_obj->outbuff,
								                                  p_bh_decode_obj->pos_outbuff) 
									    == p_bh_decode_obj->pos_outbuff ? NOERR : errFileWrite; 
				}
				else
				{
					status = NOERR;			/* don't care about the resource fork. */
				}
#endif							
				if (status != NOERR)
					p_bh_decode_obj->state = status;
					
				p_bh_decode_obj->pos_outbuff = 0;
			}
			break;
			
		case BINHEX_STATE_HCRC:
		case BINHEX_STATE_DCRC:
		case BINHEX_STATE_RCRC:
			if (!p_bh_decode_obj->count++) 
			{
				p_bh_decode_obj->fileCRC = (unsigned short) c << 8;
			} 
			else 
			{
				if ((p_bh_decode_obj->fileCRC | c) != p_bh_decode_obj->CRC) 
				{
					if (p_bh_decode_obj->state > BINHEX_STATE_HCRC) 
					{
#ifdef XP_MAC
						HDelete(p_bh_decode_obj->vRefNum, 
								p_bh_decode_obj->parID,
								(unsigned char*)p_bh_decode_obj->name);
#else
						p_bh_decode_obj->name->Delete(PR_FALSE);
#endif
					}
					p_bh_decode_obj->state = errDecoding;
					break;
				}
				
				/*
				** passed the CRC check!!!
				*/
				p_bh_decode_obj->CRC = 0;
				if (++ p_bh_decode_obj->state == BINHEX_STATE_FINISH) 
				{
#ifdef XP_MAC
					FInfo 	finfo;

					/* set back the file information.before we declare done ! */
					finfo.fdType 	= p_bh_decode_obj->head.type;
					finfo.fdCreator = p_bh_decode_obj->head.creator;
					finfo.fdFlags 	= p_bh_decode_obj->head.flags & 0xf800;
					
					HSetFInfo(p_bh_decode_obj->vRefNum, 
								p_bh_decode_obj->parID, 
								(unsigned char *)p_bh_decode_obj->name, 
								&finfo);
#endif
					/* 	now We are done with everything.	*/		
					p_bh_decode_obj->state++;
					break;
				}
				
				if (p_bh_decode_obj->state == BINHEX_STATE_DFORK) 
				{
#ifdef XP_MAC
					StandardFileReply	reply;
					if( !p_bh_decode_obj->mSpec )
					{	
						StandardPutFile("\pSave decoded file as:", 
									(unsigned char *)p_bh_decode_obj->name, 
									&reply);
									
						if (!reply.sfGood) 
						{
							p_bh_decode_obj->state = errUsrCancel;
							break;
						}
					}
					else
					{
						reply.sfFile.vRefNum = p_bh_decode_obj->mSpec->vRefNum;
						reply.sfFile.parID = p_bh_decode_obj->mSpec->parID;
						memcpy(&reply.sfFile.name, p_bh_decode_obj->mSpec->name , 63 );		
					}					
					
					    memcpy(p_bh_decode_obj->name, 
					           reply.sfFile.name, 
					           *(reply.sfFile.name)+1);	/* save the new file name.	*/
						
					p_bh_decode_obj->vRefNum = reply.sfFile.vRefNum;
					p_bh_decode_obj->parID   = reply.sfFile.parID;
						
					HDelete(reply.sfFile.vRefNum,
									reply.sfFile.parID, 
									reply.sfFile.name);
					
					status = HCreate(p_bh_decode_obj->vRefNum, 
									 p_bh_decode_obj->parID, 
									 reply.sfFile.name,
									 p_bh_decode_obj->head.creator, 
									 p_bh_decode_obj->head.type);
#else	/* non-mac OS case */
					char* filename;
					
					filename = XP_ALLOC(1024);
					if (filename == NULL 
/*JFD Do we still need this?
					  ||
						FE_PromptForFileName(p_bh_decode_obj->context, 
										XP_GetString(MK_MSG_SAVE_DECODED_AS),
										0,
										FALSE,
										FALSE,
										simple_copy,
										filename) == -1
*/
										)
					{
						FREEIF(filename);
						p_bh_decode_obj->state = errUsrCancel;
						break;
					}

					if (p_bh_decode_obj->name)
            delete p_bh_decode_obj->fileId;

					p_bh_decode_obj->name = new nsFileSpec(filename);
          if (!p_bh_decode_obj->name)
            status = errFileOpen;
          else
          {
					  p_bh_decode_obj->fileId = new nsIOFileStream(*(p_bh_decode_obj->name), (PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE));
					  if (p_bh_decode_obj->fileId == NULL)
						  status = errFileOpen;
					  else
						  status = NOERR;
          }

					XP_FREE(filename);
					
#endif
					if (status != NOERR)
						p_bh_decode_obj->state = status;
						
					p_bh_decode_obj->count 
						= ntohl(p_bh_decode_obj->head.dlen);
				}
				else
				{
					p_bh_decode_obj->count 
						= ntohl(p_bh_decode_obj->head.rlen);	/* it should in host byte order */
				}
				
				if (p_bh_decode_obj->count) 
				{
					p_bh_decode_obj->inCRC = 0;
#ifdef XP_MAC
					if (p_bh_decode_obj->state == BINHEX_STATE_DFORK)
						status = HOpen(p_bh_decode_obj->vRefNum,
									   p_bh_decode_obj->parID,
									   (unsigned char*)p_bh_decode_obj->name,
									   fsWrPerm,
									   &(p_bh_decode_obj->fileId));
					else
						status = HOpenRF(p_bh_decode_obj->vRefNum,
									   p_bh_decode_obj->parID,
									   (unsigned char*)p_bh_decode_obj->name,
									   fsWrPerm,
									   &(p_bh_decode_obj->fileId));
					if (status != NOERR) 
					{
						p_bh_decode_obj->state = errFileOpen;
						HDelete(p_bh_decode_obj->vRefNum,
										p_bh_decode_obj->parID, 
										(unsigned char*)p_bh_decode_obj->name);
						break;
					}
#else
					/* for None Mac OS -- nothing is required, file already open.	*/
					
#endif							
				} 
				else 
				{
					/* nothing inside, so skip to the next state. */
					p_bh_decode_obj->state ++;
				}
			}
			break;
	}
	
	return;
}

static int get_next_char(binhex_decode_object *p_bh_decode_obj)
{
	char c = 0;
	
	while (p_bh_decode_obj->pos_inbuff < p_bh_decode_obj->s_inbuff)
	{
		c = p_bh_decode_obj->inbuff[p_bh_decode_obj->pos_inbuff++];
		if (c != LF && c != CR)
			break;
	}
	return (c == LF || c == CR) ? 0 : (int) c;
}

int binhex_decode_next (
	binhex_decode_object *p_bh_decode_obj, 
	const char *in_buff, 
	int32 	buff_size)
{
	int	found_start;
	int octetpos, c = 0;
	uint32 		val;
	
	/*
	**	reset the buff first. 
	*/
	p_bh_decode_obj->inbuff 	= (char*)in_buff;
	p_bh_decode_obj->s_inbuff 	= buff_size;
	p_bh_decode_obj->pos_inbuff	= 0;
	
	/*
	**	if it is the first time, seek to the right start place. 
	*/
	if (p_bh_decode_obj->state == BINHEX_STATE_START)
	{
		 found_start = FALSE;
		/*
		**	go through the line, until we get a ':'
		*/
		while (p_bh_decode_obj->pos_inbuff < p_bh_decode_obj->s_inbuff)
		{
			c = p_bh_decode_obj->inbuff[p_bh_decode_obj->pos_inbuff++];
			while (c == CR || c == LF)
			{
				if (p_bh_decode_obj->pos_inbuff >= p_bh_decode_obj->s_inbuff)
					break;
																
				c = p_bh_decode_obj->inbuff[p_bh_decode_obj->pos_inbuff++];
				if (c == ':')
				{
					found_start = TRUE;
					break;
				}
			}
			if (found_start)	break;		/* we got the start point.				*/
		}
		
		if (p_bh_decode_obj->pos_inbuff >= p_bh_decode_obj->s_inbuff)
			return NOERR;			/* we meet buff end before we get the 	*/
									/* start point, wait till next fills.	*/
		
		if (c != ':')
			return errDecoding;		/* can't find the start character.	*/
	}
	
	/*
	**	run - through the in-stream now.
	*/
	while (p_bh_decode_obj->state >= 0 && 
		   p_bh_decode_obj->state < BINHEX_STATE_DONE) 
	{
		/* fill in octetbuf */
		do 
		{
			if (p_bh_decode_obj->pos_inbuff >= p_bh_decode_obj->s_inbuff)
				return NOERR;			/* end of buff, go on for the nxet calls. */
					
			c = get_next_char(p_bh_decode_obj);
			if (c == 0)
				return NOERR;
				 
			if ((val = BHEXVAL(c)) == -1) 
			{
				/*
				** we incount an invalid character.
				*/
				if (c) 
				{
					/*
					** rolling back.
					*/
					p_bh_decode_obj->donepos --;
					if (p_bh_decode_obj->octetin >= 14)		p_bh_decode_obj->donepos--;
					if (p_bh_decode_obj->octetin >= 20) 	p_bh_decode_obj->donepos--;
				}
				break;
			}
			p_bh_decode_obj->octetbuf.val |= val << p_bh_decode_obj->octetin;
		} 
		while ((p_bh_decode_obj->octetin -= 6) > 2);
			
		/* handle decoded characters -- run length encoding (rle) detection */

#ifndef XP_MAC		
		p_bh_decode_obj->octetbuf.val 
			= ntohl(p_bh_decode_obj->octetbuf.val);
#endif

		for (octetpos = 0; octetpos < p_bh_decode_obj->donepos; ++octetpos) 
		{
			c = p_bh_decode_obj->octetbuf.c[octetpos];
			
			if (c == 0x90 && !p_bh_decode_obj->marker++) 
				continue;
						
			if (p_bh_decode_obj->marker) 
			{
				if (c == 0) 
				{
					p_bh_decode_obj->rlebuf = 0x90;
					binhex_process(p_bh_decode_obj);
				} 
				else 
				{
					while (--c > 0)				/* we are in the run lenght mode */ 
					{
						binhex_process(p_bh_decode_obj);
					}
				}
				p_bh_decode_obj->marker = 0;
			} 
			else 
			{
				p_bh_decode_obj->rlebuf = (unsigned char) c;
				binhex_process(p_bh_decode_obj);
			}
			
			
			if (p_bh_decode_obj->state >= BINHEX_STATE_FINISH) 
				break;
		}
		
		/* prepare for next 3 characters.	*/
		if (p_bh_decode_obj->donepos < 3 && p_bh_decode_obj->state < BINHEX_STATE_FINISH) 
			p_bh_decode_obj->state = errDecoding;
					
		p_bh_decode_obj->octetin 		= 26;
		p_bh_decode_obj->octetbuf.val 	= 0;
	}
	
	/* 
	**	Error clean-ups 
	*/
	if (p_bh_decode_obj->state < 0 && p_bh_decode_obj->fileId) 
	{
#ifdef XP_MAC
		FSClose(p_bh_decode_obj->fileId);
		p_bh_decode_obj->fileId = 0;
		HDelete(p_bh_decode_obj->vRefNum, 
				p_bh_decode_obj->parID, 
				(unsigned char*)p_bh_decode_obj->name);
#else
		p_bh_decode_obj->fileId->close();
		delete p_bh_decode_obj->fileId;
		p_bh_decode_obj->name->delete(PR_FALSE);
#endif
	}
	
	
	return 	p_bh_decode_obj->state < 0  				  ? (p_bh_decode_obj->state) : 
			p_bh_decode_obj->state >= BINHEX_STATE_FINISH ? errDone : NOERR;
}

int binhex_decode_end (
	binhex_decode_object *p_bh_decode_obj, 
	XP_Bool 	is_aborting)
{
#ifdef XP_MAC
	if (p_bh_decode_obj->fileId)
	{
		FSClose(p_bh_decode_obj->fileId);
		p_bh_decode_obj->fileId = 0;
		
		if (is_aborting)
		{
			HDelete(p_bh_decode_obj->vRefNum, 
					p_bh_decode_obj->parID, 
					(unsigned char*)p_bh_decode_obj->name);
		}
	}
	
#else

	if (p_bh_decode_obj->fileId)
	{
		p_bh_decode_obj->fileId->close();
		
		if (is_aborting)
			p_bh_decode_obj->name->Delete(PR_FALSE);
	}
	FREEIF(p_bh_decode_obj->name);
#endif

	return NOERR;
}

 
