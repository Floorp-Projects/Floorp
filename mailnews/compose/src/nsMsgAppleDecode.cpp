/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *
 *	apple_double_decode.c
 *
 *	---------------------
 *
 *		Codes for decoding Apple Single/Double object parts.
 *
 *			05aug95		mym		Created.
 *			25sep95		mym		Add support to write to binhex encoding on non-mac system.
 */

#include "nscore.h"
#include "msgCore.h"

#include "nsMsgAppleDouble.h"
#include "nsMsgAppleCodes.h"
#include "nsMsgBinHex.h"
#ifdef XP_MAC
#include <StandardFile.h>
#endif

extern int MK_UNABLE_TO_OPEN_TMP_FILE;
extern int MK_MIME_ERROR_WRITING_FILE;

/*
**	Static functions.
*/
PRIVATE int from_decoder(appledouble_decode_object* p_ap_decode_obj, 
			char 	*buff, 
			int 	buff_size, 
			PRInt32	*in_count);
PRIVATE int from_64(appledouble_decode_object* p_ap_decode_obj, 
			char 	*buff, 
			int 	size, 
			PRInt32 	*real_size);
PRIVATE int from_qp(appledouble_decode_object* p_ap_decode_obj, 
			char 	*buff, 
			int 	size, 
			PRInt32 	*real_size);
PRIVATE int from_uu(appledouble_decode_object* p_ap_decode_obj, 
			char 	*buff, 
			int 	size, 
			PRInt32 	*real_size);
PRIVATE int from_none(appledouble_decode_object* p_ap_decode_obj, 
			char 	*buff, 
			int 	size, 
			PRInt32 	*real_size);

PRIVATE int decoder_seek(appledouble_decode_object* p_ap_decode_obj, 
			int 	seek_pos, 
			int 	start_pos);
			
/*
**	fetch_a_line
**	-------------
**
**		get a line from the in stream..
*/
int fetch_a_line(
	appledouble_decode_object* p_ap_decode_obj,
	char 	*buff)
{
	int  i, left;
	char *p, c = 0;
	
	if (p_ap_decode_obj->s_leftover == 0 && 
		p_ap_decode_obj->s_inbuff <= p_ap_decode_obj->pos_inbuff)
	{
		*buff = '\0';
		return errEOB;
	}
	
	if (p_ap_decode_obj->s_leftover)
	{
		for (p = p_ap_decode_obj->b_leftover, i = p_ap_decode_obj->s_leftover; i>0; i--)
			*buff++ = *p++;
			
		p_ap_decode_obj->s_leftover = 0;
	}
	
	p    = p_ap_decode_obj->inbuff   + p_ap_decode_obj->pos_inbuff;
	left = p_ap_decode_obj->s_inbuff - p_ap_decode_obj->pos_inbuff;
	
	for (i = 0; i < left; )
	{
		c = *p++; i++;
		
		if (c == CR && *p == LF)
		{
			p++; i++;	/* make sure skip both LF & CR	*/
		}

		if (c == LF || c == CR)
			break;
		
		*buff++ = c; 
	}
	p_ap_decode_obj->pos_inbuff += i;
	
	if (i == left && c != LF && c != CR)
	{
		/*
		**  we meet the buff end before we can terminate the string,
		**	save the string to the left_over buff.
		*/
		p_ap_decode_obj->s_leftover = i;
		
		for (p = p_ap_decode_obj->b_leftover; i>0; i--)
			*p++ = *(buff-i);
			
		return errEOB;
	}
	*buff = '\0';
	return NOERR;
}

void parse_param(
	char *p, 
	char **param, 			/* the param		*/
	char **define, 			/* the defination.	*/
	char **np)				/* next position.	*/
{
	while (*p == ' ' || *p == '\"' || *p == ';') p++;
	*param = p;
	
	while (*p != ' ' && *p != '=' ) p++;
	if (*p == ' ')
		*define = p+1;
	else
		*define = p+2;
		
	while (*p && *p != ';') p++;

	if (*p == ';')
		*np = p + 1;
	else
		*np = p;
}

int ap_seek_part_start(
	appledouble_decode_object* p_ap_decode_obj)
{
	int  status;
	char newline[256];
	
	while (1)
	{
		status = fetch_a_line(p_ap_decode_obj, newline);
		if(status != NOERR)
			break;
			
		if (newline[0] == '\0' && p_ap_decode_obj->boundary0 != NULL)
			return errDone;
		
		if (!PL_strncasecmp(newline, "--", 2))
		{
			/* we meet the start separator, copy it and it will be our boundary */
      p_ap_decode_obj->boundary0 = nsCRT::strdup(newline+2);
			return errDone;
		}
	}
	return status;
}

int ParseFileHeader(
	appledouble_decode_object *p_ap_decode_obj)
{
	int  status;
	int	 i;
	char newline[256], *p;
	char *param, *define;
	
	while (1)
	{
		status = fetch_a_line(p_ap_decode_obj, newline);
		if (newline[0] == '\0')
			break;				/* we get the end of a defination section.	*/
		
		p = newline; 
		while (1)
		{
			parse_param(p, &param, &define, &p);
			/*
			**	we only care about these params.
			*/
			if (!PL_strncasecmp(param, "Content-Type:", 13))
			{
				if (!PL_strncasecmp(define, MULTIPART_APPLEDOUBLE,
									nsCRT::strlen(MULTIPART_APPLEDOUBLE)) ||
					!PL_strncasecmp(define, MULTIPART_HEADER_SET,
									nsCRT::strlen(MULTIPART_HEADER_SET)))
					p_ap_decode_obj->messagetype = kAppleDouble;
				else
					p_ap_decode_obj->messagetype = kGeneralMine;
			}
			else if (!PL_strncasecmp(param, "boundary=", 9))
			{
				for (i = 0; *define && *define != '\"'; )
					p_ap_decode_obj->boundary0[i++] = *define++;
					
				p_ap_decode_obj->boundary0[i] = '\0';
			}
			else if (!PL_strncasecmp(param, "Content-Disposition:", 20))
			{
				if (!PL_strncasecmp(define, "inline", 5))
					p_ap_decode_obj->deposition = kInline;
				else
					p_ap_decode_obj->deposition = kDontCare;
			}
			else if (!PL_strncasecmp(param, "filename=", 9))
			{
				for (i = 0, p=define; *p && *p != '\"'; )
					p_ap_decode_obj->fname[i++] = *p++;
					
				p_ap_decode_obj->fname[i] = '\0';
			}
			
			if (*p == '\0')
				break;
		}
	}
		
	return NOERR;
}

int ap_seek_to_boundary(
	appledouble_decode_object *p_ap_decode_obj, 
	PRBool firstime)
{
	int  status = NOERR;
	char buff[256];
	
	while (status == NOERR)
	{
		status = fetch_a_line(p_ap_decode_obj, buff);
		if (status != NOERR)
			break;
				
		if ((!PL_strncasecmp(buff, "--", 2) &&
			!PL_strncasecmp(	buff+2, 
						p_ap_decode_obj->boundary0, 
						nsCRT::strlen(p_ap_decode_obj->boundary0))) 
		  ||!PL_strncasecmp(	buff, 
						p_ap_decode_obj->boundary0, 
						nsCRT::strlen(p_ap_decode_obj->boundary0)))
		{
			TRACEMSG(("Found boundary: %s", p_ap_decode_obj->boundary0));
			status = errDone;
			break;
		}
	}
	
	if (firstime && status == errEOB)
		status = NOERR;				/* so we can do it again.	*/

	return	status;
}

int ap_parse_header(
	appledouble_decode_object *p_ap_decode_obj, 
	PRBool firstime) 
{
	int	 	status, i;
	char 	newline[256], *p;
	char 	*param, *define;
	
	if (firstime)
	{
		/* do the clean ups. */
		p_ap_decode_obj->encoding = kEncodeNone;
		p_ap_decode_obj->which_part = kFinishing;
	}
		
	while (1)
	{
		status = fetch_a_line(p_ap_decode_obj, newline);
		if (status != NOERR)
			return status;		/* a possible end of buff happened.			*/
			
		if (newline[0] == '\0')
			break;				/* we get the end of a defination section.	*/
		
		p = newline;
		while (1)
		{
			parse_param(p, &param, &define, &p);
			/*
			**	we only care about these params.
			*/
			if (!PL_strncasecmp(param, "Content-Type:", 13))
			{
				if (!PL_strncasecmp(define, "application/applefile", 21))
					p_ap_decode_obj->which_part = kHeaderPortion;
				else
				{
					p_ap_decode_obj->which_part = kDataPortion;
					if (!PL_strncasecmp(define, "text/plain", 10))
						p_ap_decode_obj->is_binary = FALSE;
					else
						p_ap_decode_obj->is_binary = TRUE; 
				}
				
				/* Broken QuickMail messages */
				if (!PL_strncasecmp(define, "x-uuencode-apple-single", 23))
					p_ap_decode_obj->encoding = kEncodeUU;
			}
			else if (!PL_strncasecmp(param, "Content-Transfer-Encoding:",26))
			{
				if (!PL_strncasecmp(define, "base64", 6))
					p_ap_decode_obj->encoding = kEncodeBase64;
				else if (!PL_strncasecmp(define, "quoted-printable", 16))
					p_ap_decode_obj->encoding = kEncodeQP;
				else 
					p_ap_decode_obj->encoding = kEncodeNone;
			}
			else if (!PL_strncasecmp(param, "Content-Disposition:", 20))
			{
				if (!PL_strncasecmp(define, "inline", 5))
					p_ap_decode_obj->deposition = kInline;
				else
					p_ap_decode_obj->deposition = kDontCare;
			}
			else if (!PL_strncasecmp(param, "filename=", 9))
			{
				if (p_ap_decode_obj->fname[0] == '\0')
				{
					for (i = 0; *define && *define != '\"'; )
						p_ap_decode_obj->fname[i++] = *define++;
					
					p_ap_decode_obj->fname[i] = '\0';
				}
			}
			
			if (*p == '\0')
				break;
		}
	}
	return errDone;
}


/*
**	decode the head portion.
*/


int ap_decode_file_infor(appledouble_decode_object *p_ap_decode_obj)
{
	ap_header 	head;
	ap_entry 	entries[NUM_ENTRIES + 1];
	int 		i, j;
	int			st_pt;
	PRInt32		in_count;
	int			status;
	char		name[256];
	PRBool		positionedAtRFork = FALSE;
	
	st_pt = p_ap_decode_obj->pos_inbuff;
	
	/*
	** 	Read & verify header 
	*/
	status = from_decoder(
					p_ap_decode_obj, 
					(char *) &head, 
					26, 		/* sizeof (head), 	*/
					&in_count);
	if (status != NOERR)
		return status;
	
	if (p_ap_decode_obj->is_apple_single)
	{
		if (ntohl(head.magic) != APPLESINGLE_MAGIC)
			return errVersion;
	}
	else
	{
		if(ntohl(head.magic) != APPLEDOUBLE_MAGIC) 
			return errVersion;
	}
		
	if (ntohl(head.version) != VERSION) 
	{
		return errVersion;
	}
	
	/* read entries */
	head.entries = ntohs(head.entries);
	for (i = j = 0; i < head.entries; ++i) 
	{
		status = from_decoder(
					p_ap_decode_obj,
					(char *) (entries + j), 
					sizeof (ap_entry), 
					&in_count);
		if (status != NOERR)
			return errDecoding;
		
		/*
		**	correct the byte order now.
		*/
		entries[j].id 	  = ntohl(entries[j].id);
		entries[j].offset = ntohl(entries[j].offset);
		entries[j].length = ntohl(entries[j].length);
		/*
		**	only care about these entries...
		*/
		if (j < NUM_ENTRIES) 
		switch (entries[j].id) 
		{
			case ENT_NAME:
			case ENT_FINFO:
			case ENT_DATES:
			case ENT_COMMENT:
			case ENT_RFORK:
			case ENT_DFORK:
				++j;
				break;
		}
	}
	
	in_count = nsCRT::strlen(p_ap_decode_obj->fname);

		/* if the user has not provided the output file name, read it
		 * from the ENT_NAME entry
		 */

	if (in_count == 0) 
	{
		/* read name */
		for (i = 0; i < j && entries[i].id != ENT_NAME; ++i)
			;
		if (i == j) 
			return errDecoding;
			
		status = decoder_seek(
						p_ap_decode_obj, 
						entries[i].offset,
						st_pt);	
		if (status != NOERR)
			return status;
			
		if (entries[i].length > 63) 
			entries[i].length = 63;
			
		status = from_decoder(
						p_ap_decode_obj, 
						p_ap_decode_obj->fname, 
						entries[i].length,
						&in_count);
		if (status != NOERR)
			return status;

		p_ap_decode_obj->fname[in_count] = '\0';
	}

	/* P_String version of the file name. */
  PL_strcpy((char *)name+1, p_ap_decode_obj->fname);
	name[0] = (char) in_count;
	
	if (p_ap_decode_obj->write_as_binhex)
	{
		/*
		**	fill out the simple the binhex head.
		*/
		binhex_header head;
		myFInfo		  myFInfo;

		status = (*p_ap_decode_obj->binhex_stream->put_block)
					(p_ap_decode_obj->binhex_stream->data_object,
					name, 
					name[0] + 2);
		if (status != NOERR)
			return status;
		
		/* get finder info */
		for (i = 0; i < j && entries[i].id != ENT_FINFO; ++i)
			;
		if (i < j) 
		{
			status = decoder_seek(p_ap_decode_obj, 
						entries[i].offset, 
						st_pt);
			if (status != NOERR)
				return status;
				
			status = from_decoder(p_ap_decode_obj, 
						(char *) &myFInfo, 
						sizeof (myFInfo), 
						&in_count);
			if (status != NOERR)
				return status;				
		}
		
		head.type 		= myFInfo.fdType;
		head.creator 	= myFInfo.fdCreator;
		head.flags 		= myFInfo.fdFlags;

		for (i = 0; i < j && entries[i].id != ENT_DFORK; ++i)
			;
		if (i < j && entries[i].length != 0)
		{
			head.dlen = entries[i].length;	/* set the data fork length 	*/
		}	
		else
		{
			head.dlen = 0;
		}
	
		for (i = 0; i < j && entries[i].id != ENT_RFORK; ++i)
			;
		if (i < j && entries[i].length != 0)
		{
			head.rlen = entries[i].length;	/* set the resource fork length */
		}
		else
		{
			head.rlen = 0;
		}

		/*
		**	and the dlen, rlen is in the host byte order, correct it if needed ...
		*/
		head.dlen = htonl(head.dlen);
		head.rlen = htonl(head.rlen);
		/*
		**	then encode them in binhex.
		*/
		status = (*p_ap_decode_obj->binhex_stream->put_block)
					(p_ap_decode_obj->binhex_stream->data_object,
					(char*)&head,
					sizeof(binhex_header));
		if (status != NOERR)
			return status;
		
		/*
		**	after we have done with the header, end the binhex header part.
		*/
		status = (*p_ap_decode_obj->binhex_stream->put_block)
					(p_ap_decode_obj->binhex_stream->data_object,
					NULL,
					0);
	}
	else
	{
	
#ifdef	XP_MAC

		ap_dates 	dates;
		HFileInfo 	*fpb;
		CInfoPBRec 	cipbr;
		IOParam 	vinfo;
		GetVolParmsInfoBuffer vp;
		DTPBRec 	dtp;
		char 		comment[256];

		unsigned char filename[256]; /* this is a pascal string - should be unsigned char. */
		StandardFileReply reply;

		/* convert char* p_ap_decode_obj->fname to a pascal string */
		PL_strcpy((char*)filename + 1, p_ap_decode_obj->fname);
		filename[0] = nsCRT::strlen(p_ap_decode_obj->fname);
														
		if( !p_ap_decode_obj->mSpec )
		{
			StandardPutFile("\pSave decoded file as:", 
						(const unsigned char*)filename, 
						&reply);
						
			if (!reply.sfGood) 
			{
				return errUsrCancel;
			}
		}
		else
		{
			reply.sfFile.vRefNum = p_ap_decode_obj->mSpec->vRefNum;
			reply.sfFile.parID = p_ap_decode_obj->mSpec->parID;
			memcpy(&reply.sfFile.name, p_ap_decode_obj->mSpec->name , 63 );		
		}
		
		memcpy(p_ap_decode_obj->fname, 
		       reply.sfFile.name+1, 
		       *(reply.sfFile.name)+1);
		p_ap_decode_obj->fname[*(reply.sfFile.name)] = '\0';
			
		p_ap_decode_obj->vRefNum = reply.sfFile.vRefNum;
		p_ap_decode_obj->dirId   = reply.sfFile.parID;
	
		/* create & get info for file */
		HDelete(reply.sfFile.vRefNum, 
					reply.sfFile.parID, 
					reply.sfFile.name);

#define DONT_CARE_TYPE	0x3f3f3f3f
	
		if (HCreate(reply.sfFile.vRefNum,
					reply.sfFile.parID, 
					reply.sfFile.name, 
					DONT_CARE_TYPE, 
					DONT_CARE_TYPE) != NOERR) 
		{
			return errFileOpen;
		}
		
		fpb = (HFileInfo *) &cipbr;
		fpb->ioVRefNum = reply.sfFile.vRefNum;
		fpb->ioDirID   = reply.sfFile.parID;
		fpb->ioNamePtr = reply.sfFile.name;
		fpb->ioFDirIndex = 0;
		PBGetCatInfoSync(&cipbr);
	
		/* get finder info */
		for (i = 0; i < j && entries[i].id != ENT_FINFO; ++i)
			;
		if (i < j) 
		{
			status = decoder_seek(p_ap_decode_obj, 
							entries[i].offset, 
							st_pt);
			if (status != NOERR)
				return status;
				
			status = from_decoder(p_ap_decode_obj, 
							(char *) &fpb->ioFlFndrInfo, 
							sizeof (FInfo), 
							&in_count);
			if (status != NOERR)
				return status;
				
			status = from_decoder(p_ap_decode_obj,
							(char *) &fpb->ioFlXFndrInfo, 
							sizeof (FXInfo),
							&in_count);
		
			if (status != NOERR && status != errEOP )
				return status;
				
			fpb->ioFlFndrInfo.fdFlags &= 0xfc00; /* clear flags maintained by finder */
		}
	
		/*
		** 	get file date info 
		*/
		for (i = 0; i < j && entries[i].id != ENT_DATES; ++i)
			;
		if (i < j) 
		{
			status = decoder_seek(p_ap_decode_obj, 
							entries[i].offset, 
							st_pt);
			if (status != NOERR && status != errEOP )
				return status;
				
			status = from_decoder(p_ap_decode_obj,
							(char *) &dates, 
							sizeof (dates), 
							&in_count);
			if (status != NOERR)
				return status;
				
			fpb->ioFlCrDat = dates.create - CONVERT_TIME;
			fpb->ioFlMdDat = dates.modify - CONVERT_TIME;
			fpb->ioFlBkDat = dates.backup - CONVERT_TIME;
		}
		
		/*
		** 	update info 
		*/
		fpb->ioDirID = fpb->ioFlParID;
		PBSetCatInfoSync(&cipbr);
		
		/*
		** 	get comment & save it 
		*/
		for (i = 0; i < j && entries[i].id != ENT_COMMENT; ++i)
			;
		if (i < j && entries[i].length != 0) 
		{
			memset((void *) &vinfo, '\0', sizeof (vinfo));
			vinfo.ioVRefNum = fpb->ioVRefNum;
			vinfo.ioBuffer  = (Ptr) &vp;
			vinfo.ioReqCount = sizeof (vp);
			if (PBHGetVolParmsSync((HParmBlkPtr) &vinfo) == NOERR &&
				((vp.vMAttrib >> bHasDesktopMgr) & 1)) 
			{
				memset((void *) &dtp, '\0', sizeof (dtp));
				dtp.ioVRefNum = fpb->ioVRefNum;
				if (PBDTGetPath(&dtp) == NOERR) 
				{
					if (entries[i].length > 255) 
						entries[i].length = 255;
						
					status = decoder_seek(p_ap_decode_obj, 
								entries[i].offset, 
								st_pt);
					if (status != NOERR)
						return status;
						
					status = from_decoder(p_ap_decode_obj, 
								comment, 
								entries[i].length, 
								&in_count);
					if (status != NOERR)
						return status;
						
					dtp.ioDTBuffer = (Ptr) comment;
					dtp.ioNamePtr  = fpb->ioNamePtr;
					dtp.ioDirID    = fpb->ioDirID;
					dtp.ioDTReqCount = entries[i].length;
					if (PBDTSetCommentSync(&dtp) == NOERR) 
					{
						PBDTFlushSync(&dtp);
					}
				}
			}
		}
		
#else
		/*
		**	in non-mac system, creating a data fork file will be it.
		*/
#endif
	}
	
	/* 
	** Get the size of the resource fork, and (maybe) position to the beginning of it. 
	*/
	for (i = 0; i < j && entries[i].id != ENT_RFORK; ++i)
		;
	if (i < j && entries[i].length != 0)
	{
#ifdef XP_MAC
		/* Seek to the start of the resource fork only if we're on a Mac */
		status = decoder_seek(p_ap_decode_obj, 
								entries[i].offset, 
								st_pt);
		positionedAtRFork = TRUE;
#endif
		p_ap_decode_obj->rksize = entries[i].length;
	}
	else
		p_ap_decode_obj->rksize = 0;
	
	/*
	** Get the size of the data fork, and (maybe) position to the beginning of it.
	*/
	for (i = 0; i < j && entries[i].id != ENT_DFORK; ++i)
		;
	if (i < j && entries[i].length != 0)
	{
		if (p_ap_decode_obj->is_apple_single && !positionedAtRFork)
			status = decoder_seek(p_ap_decode_obj, 
									entries[i].offset, 
									st_pt);
		p_ap_decode_obj->dksize = entries[i].length;
	}	
	else
		p_ap_decode_obj->dksize = 0;

	/*
	**	Prepare a tempfile to hold the resource fork decoded by the decoder,
	**		because in binhex, resource fork appears after the data fork!!!
	*/
	if (p_ap_decode_obj->write_as_binhex)
	{
		if (p_ap_decode_obj->rksize != 0)
		{
			/* we need a temp file to hold all the resource data, because the */			
			p_ap_decode_obj->tmpFileSpec = nsMsgCreateTempFileSpec("apmail");
      if (!p_ap_decode_obj->tmpFileSpec)
        return errFileOpen;

      p_ap_decode_obj->tmpFileStream = new nsIOFileStream(*(p_ap_decode_obj->tmpFileSpec));
      if ( (!p_ap_decode_obj->tmpFileStream) ||
           (p_ap_decode_obj->tmpFileStream->is_open()) )
      {
        delete p_ap_decode_obj->tmpFileSpec;
        return errFileOpen;
      }
		}
	}

	return NOERR;
}

/*
**	ap_decode_process_header
**
**
*/
int ap_decode_process_header(
	appledouble_decode_object* p_ap_decode_obj, 
	PRBool firstime)
{
	PRInt32 	in_count;
	int		status = NOERR;
	char	wr_buff[1024];
	
	if (firstime)
	{
		status = ap_decode_file_infor(p_ap_decode_obj);
		if (status != NOERR)
			return status;
		
		if (p_ap_decode_obj->rksize > 0)
		{
#ifdef XP_MAC
			if(!p_ap_decode_obj->write_as_binhex)
			{
				Str63	fname;
				short	refNum;
					
				fname[0] = nsCRT::strlen(p_ap_decode_obj->fname);
				PL_strcpy((char*)fname+1, p_ap_decode_obj->fname);
				 
				if (HOpenRF(p_ap_decode_obj->vRefNum,
							p_ap_decode_obj->dirId,
							fname,
							fsWrPerm,
							&refNum) != NOERR)
				{
					return (errFileOpen);
				}
				p_ap_decode_obj->fileId = refNum;
			}
#endif
		}
		else
		{
			status = errDone;
		}
	}
	
	/*
	**	Time to continue decoding all the resource data.
	*/	
	while (status == NOERR && p_ap_decode_obj->rksize > 0)
	{
		in_count = PR_MIN(1024, p_ap_decode_obj->rksize);
		
		status = from_decoder(p_ap_decode_obj, 
							wr_buff, 
							in_count, 
							&in_count);
		
		if (p_ap_decode_obj->write_as_binhex)
		{	
			/*
			**	Write to the temp file first, because the resource fork appears after
			**	 the data fork in the binhex encoding.
			*/
      if (p_ap_decode_obj->tmpFileStream->write(wr_buff, in_count) != in_count)
      {
				status = errFileWrite;						
				break;
			}

			p_ap_decode_obj->data_size += in_count;
		}
		else
		{
#ifdef XP_MAC
			long howMuch = in_count;
			
			if (FSWrite(p_ap_decode_obj->fileId, &howMuch, wr_buff) != NOERR)
			{
				status = errFileWrite;
				break;
			}
#else
			/* ======	Write nothing in a non mac file system	============ */
#endif
		}
		
		p_ap_decode_obj->rksize -= in_count;
	}
	
	if (p_ap_decode_obj->rksize <= 0 || status == errEOP)
	{
		if (p_ap_decode_obj->write_as_binhex)
		{
			/*
			**	No more resource data, but we are not done
			**  with tempfile yet, just seek back to the start point, 
			**		-- ready for a readback later 
			*/
			if (p_ap_decode_obj->tmpFileStream)
				p_ap_decode_obj->tmpFileStream->seek(PR_SEEK_SET, 0);
		}
		
#ifdef XP_MAC
		else if (p_ap_decode_obj->fileId)	/* close the resource fork of the macfile	*/
		{
			FSClose(p_ap_decode_obj->fileId);
			p_ap_decode_obj->fileId = 0;
		}
#endif
		if (!p_ap_decode_obj->is_apple_single)
		{
			p_ap_decode_obj->left 	 = 0;
			p_ap_decode_obj->state64 = 0;
		}
		status = errDone;
	}	
	return status;
}

int ap_decode_process_data(
	appledouble_decode_object* p_ap_decode_obj, 
	PRBool 	firstime)
{
	char 	wr_buff[1024];
	PRInt32  in_count;
	int	 	status = NOERR;
	int  	retval = NOERR;
	
	if (firstime)
	{
		if (!p_ap_decode_obj->write_as_binhex)
		{
#ifdef XP_MAC
			char	*filename;
			FSSpec	fspec;
			
			fspec.vRefNum = p_ap_decode_obj->vRefNum;
			fspec.parID   = p_ap_decode_obj->dirId;
			fspec.name[0] = nsCRT::strlen(p_ap_decode_obj->fname);
			PL_strcpy((char*)fspec.name+1, p_ap_decode_obj->fname);
			
      nsFileSpec    tFileSpec(fspec, PR_TRUE);

			if (p_ap_decode_obj->is_binary)
				p_ap_decode_obj->fileStream = new nsIOFileStream(tFileSpec, (PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE));
			else
				p_ap_decode_obj->fileStream = new nsIOFileStream(tFileSpec, (PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE));
						// RICHIE XP_FileOpen(filename+7, xpURL, XP_FILE_TRUNCATE);

			PR_FREEIF(filename);				
#else			
      nsFileSpec    tFileSpec(p_ap_decode_obj->fname);
			if (p_ap_decode_obj->is_binary)
				p_ap_decode_obj->fileStream = new nsIOFileStream(tFileSpec, (PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE));
						// RICHIE XP_FileOpen(p_ap_decode_obj->fname, xpURL, XP_FILE_TRUNCATE_BIN);
			else
				p_ap_decode_obj->fileStream = new nsIOFileStream(tFileSpec, (PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE)); 
						// RICHIE XP_FileOpen(p_ap_decode_obj->fname, xpURL, XP_FILE_TRUNCATE);
#endif
		}
		else
		{
			;	/*	== don't need do anything to binhex stream, it is ready already  == */
		}
	}
	
	if (p_ap_decode_obj->is_apple_single && 
		p_ap_decode_obj->dksize == 0)
	{
		/* if no data in apple single, we already done then.	*/
		status = errDone;
	}

	while (status == NOERR && retval == NOERR)
	{	
		retval = from_decoder(p_ap_decode_obj,
						wr_buff, 
						1024, 
						&in_count);

		if (p_ap_decode_obj->is_apple_single)		/* we know the data fork size in 			*/
			p_ap_decode_obj->dksize -= in_count;	/*	apple single, use it to decide the end	*/
						
		if (p_ap_decode_obj->write_as_binhex)		
			status = (*p_ap_decode_obj->binhex_stream->put_block)
						(p_ap_decode_obj->binhex_stream->data_object, 
						wr_buff, 
						in_count);
		else
			status = p_ap_decode_obj->fileStream(wr_buff, in_count) == in_count ? NOERR : errFileWrite;
								
		if (retval == errEOP ||						/* for apple double, we meet the boundary	*/
			( p_ap_decode_obj->is_apple_single && 
			  p_ap_decode_obj->dksize <= 0))		/* for apple single, we know it is ending	*/
		{
			status = errDone;
			break;
		}
	}
	
	if (status == errDone)
	{
		if (p_ap_decode_obj->write_as_binhex)		
		{
			/* CALL with data == NULL && size == 0 to end a part object in binhex encoding */			
			status = (*p_ap_decode_obj->binhex_stream->put_block)
						(p_ap_decode_obj->binhex_stream->data_object, 
						 NULL, 
						 0);
			if (status != NOERR)
				return status;
		}
		else if (p_ap_decode_obj->fileStream)
		{
			p_ap_decode_obj->fileStream->close();
			delete p_ap_decode_obj->fileStream;
		}

		status = errDone;
	}
	return status;
}

/*
**	Fill the data from the decoder stream.
*/
PRIVATE int from_decoder(
	appledouble_decode_object* p_ap_decode_obj, 
	char 	*buff, 
	int 	buff_size, 
	PRInt32	*in_count)
{
	int 	status;
	
	switch (p_ap_decode_obj->encoding)
	{
		case kEncodeQP:
			status = from_qp(p_ap_decode_obj,
						buff,
						buff_size,
						in_count);
			break;
		case kEncodeBase64:
			status = from_64(p_ap_decode_obj,
						buff,
						buff_size,
						in_count);
			break;
		case kEncodeUU:
			status = from_uu(p_ap_decode_obj,
						buff,
						buff_size,
						in_count);
			break;
		case kEncodeNone:
		default:
			status = from_none(p_ap_decode_obj, 
						buff,
						buff_size,
						in_count);
			break;
	}
	return status;
}

/*
**	decoder_seek
**
**	simulate a stream seeking on the encoded stream.
*/
PRIVATE int decoder_seek(
	appledouble_decode_object* p_ap_decode_obj, 
	int 	seek_pos, 
	int 	start_pos)
{
	char 	tmp[1024];
	int  	status = NOERR;
	PRInt32	 in_count;
	
	/*
	**	 force a reset on the in buffer.
	*/
	p_ap_decode_obj->state64 	= 0;
	p_ap_decode_obj->left    	= 0;
	p_ap_decode_obj->pos_inbuff = start_pos;
	p_ap_decode_obj->uu_starts_line = TRUE;
	p_ap_decode_obj->uu_bytes_written = p_ap_decode_obj->uu_line_bytes = 0;
	p_ap_decode_obj->uu_state = kWaitingForBegin;
	
	while (seek_pos > 0)
	{
		status = from_decoder(p_ap_decode_obj, 
						tmp, 
						PR_MIN(1024, seek_pos), 
						&in_count);
		if (status != NOERR)
			break;
		
		seek_pos -= in_count;
	}
	return status;
}

#define XX 127
/*
 * Table for decoding base64
 */
static char index_64[256] = {
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,62, XX,XX,XX,63,
    52,53,54,55, 56,57,58,59, 60,61,XX,XX, XX,XX,XX,XX,
    XX, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
    15,16,17,18, 19,20,21,22, 23,24,25,XX, XX,XX,XX,XX,
    XX,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
    41,42,43,44, 45,46,47,48, 49,50,51,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
};

#define CHAR64(c)  (index_64[(unsigned char)(c)])
#define EndOfBuff(p)	((p)->pos_inbuff >= (p)->s_inbuff)

PRIVATE int fetch_next_char_64(
	appledouble_decode_object* p_ap_decode_obj)
{
	char c;
	
	c = p_ap_decode_obj->inbuff[p_ap_decode_obj->pos_inbuff++];
	if (c == '-')
		--p_ap_decode_obj->pos_inbuff;		/* put back					*/
	
	while (c == LF || c == CR)				/* skip the CR character.	*/
	{
		if (EndOfBuff(p_ap_decode_obj))
		{
			c = 0;
			break;
		}
		
		c = p_ap_decode_obj->inbuff[p_ap_decode_obj->pos_inbuff++];
		if (c == '-')
		{
			--p_ap_decode_obj->pos_inbuff;	/* put back					*/
		}
	}
	return (int)c;
}


PRIVATE int from_64(
	appledouble_decode_object* p_ap_decode_obj, 
	char	 *buff, 
	int 	size, 
	PRInt32 	*real_size)
{
	int 	i, j, buf[4];
	int 	c1, c2, c3, c4;
	
	(*real_size) = 0;
	
	/*
	** 	decode 4 by 4s characters
	*/
	for (i = p_ap_decode_obj->state64; i<4; i++)
	{
		if (EndOfBuff(p_ap_decode_obj))
		{
			p_ap_decode_obj->state64 = i;
			break;
		}
		if ((p_ap_decode_obj->c[i] = fetch_next_char_64(p_ap_decode_obj)) == 0)
			break;
	}
	
	if (i != 4)
	{
		/*
		** not enough data to fill the decode buff.
		*/
		return errEOB;						/* end of buff	*/
	}
		
	while (size > 0)
	{
		c1 = p_ap_decode_obj->c[0];
		c2 = p_ap_decode_obj->c[1];
		c3 = p_ap_decode_obj->c[2];
		c4 = p_ap_decode_obj->c[3];
		
		if (c1 == '-' || c2 == '-' || c3 == '-' || c4 == '-')
		{
			return errEOP;			/* we meet the part boundary.	*/
		}
		
        if (c1 == '=' || c2 == '=') 
        {
            return errDecoding;
        }
        
        c1 = CHAR64(c1);
        c2 = CHAR64(c2);
		buf[0] = ((c1<<2) | ((c2&0x30)>>4));
		
        if (c3 != '=') 
        {
            c3 = CHAR64(c3);
		    buf[1] = (((c2&0x0F) << 4) | ((c3&0x3C) >> 2));

            if (c4 != '=') 
            {
                c4 = CHAR64(c4);
				buf[2] = (((c3&0x03) << 6) | c4);
            }
            else
            {
            	if (p_ap_decode_obj->left == 0)
            	{
	        		*buff++ = buf[0]; (*real_size)++;
	        	}
	        	*buff++ = buf[1]; (*real_size)++;
				/* return errEOP; */ /* bug 87784 */
				return EndOfBuff(p_ap_decode_obj) ? errEOP : NOERR;
            }
        }
        else
        {
        	*buff++ = *buf;
        	(*real_size)++;
			/* return errEOP; *bug 87784*/	/* we meet the the end	*/	
			return EndOfBuff(p_ap_decode_obj) ? errEOP : NOERR;
        }
        /*
		** copy the content
		*/
		for (j = p_ap_decode_obj->left; j<3; )
		{
			*buff++ = buf[j++];	
			(*real_size)++;
			if (--size <= 0)
				break;
		}
		p_ap_decode_obj->left = j % 3;
			
		if (size <=0)
		{
			if (j == 3)
				p_ap_decode_obj->state64 = 0;	/* See if we used up all data, 	*/
												/* ifnot, keep the data, 		*/
												/* we need it for next time.	*/
			else
				p_ap_decode_obj->state64 = 4;
				
			break;
		}
			
		/*
		**	fetch the next 4 character group. 
		*/
		for (i = 0; i < 4; i++)
		{
			if (EndOfBuff(p_ap_decode_obj))
				break;

			if ((p_ap_decode_obj->c[i] = fetch_next_char_64(p_ap_decode_obj)) == 0)
				break;
		}

		p_ap_decode_obj->state64 = i % 4;
	
		if (i != 4)
			break;								/* some kind of end of buff met.*/
	}

	/*
	**	decide the size and status. 
	*/
	return EndOfBuff(p_ap_decode_obj) ? errEOB : NOERR;
}

/*
 * Table for decoding hexadecimal in quoted-printable
 */
static char index_hex[256] = {
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
     0, 1, 2, 3,  4, 5, 6, 7,  8, 9,XX,XX, XX,XX,XX,XX,
    XX,10,11,12, 13,14,15,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,10,11,12, 13,14,15,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
};

#define HEXCHAR(c)  	(index_hex[(unsigned char)(c)])
#define NEXT_CHAR(p)	((int)((p)->inbuff[(p)->pos_inbuff++]))
#define CURRENT_CHAR(p)	((int)((p)->inbuff[(p)->pos_inbuff]))
/* 
**	quoted printable decode, as defined in RFC 1521, page18 - 20
*/
PRIVATE int from_qp(
	appledouble_decode_object* p_ap_decode_obj, 
	char	*buff, 
	int 	size, 
	PRInt32 	*real_size)
{
    char c;
    int c1, c2;
	
	*real_size = 0;
	
	if (p_ap_decode_obj->c[0] == '=')
	{
		/*
		** continue with the last time's left over.
		*/
		p_ap_decode_obj->c[0] = 0;
		
		c1 = p_ap_decode_obj->c[1];		p_ap_decode_obj->c[1] = 0;
		
		if ( c1 == 0)
		{
			c1 = NEXT_CHAR(p_ap_decode_obj);
			c2 = NEXT_CHAR(p_ap_decode_obj);
		}
		else
		{
			c2 = NEXT_CHAR(p_ap_decode_obj);
		}
		c = HEXCHAR(c1) << 4 | HEXCHAR(c2);
		
		size --;
		*buff ++ = c;
		(*real_size) ++;
	}

	/* 
	**	Then start to work on the new data 
	*/
	while (size > 0)
	{
		if (EndOfBuff(p_ap_decode_obj))
			break;

		c1 = NEXT_CHAR(p_ap_decode_obj);
	
		if (c1 == '=')
		{
			if (EndOfBuff(p_ap_decode_obj))
			{
				p_ap_decode_obj->c[0] = c1;
				break;				
			}
			
			c1 = NEXT_CHAR(p_ap_decode_obj);
			if (c1 != '\n') 
			{
				/*
				**	Rule #2	
				*/
				c1 = HEXCHAR(c1);
				if (EndOfBuff(p_ap_decode_obj))
				{
					p_ap_decode_obj->c[0] = '=';
					p_ap_decode_obj->c[1] = c1;
					break;
				}
				
				c2 = NEXT_CHAR(p_ap_decode_obj);
				c2 = HEXCHAR(c2);
				c = c1 << 4 | c2;
				if (c != '\r')
				{
					size --;
					*buff++ = c;
					(*real_size)++;
				}
			}
			else
			{
				/* ignore the line break -- soft line break, rule #5	*/
			}
		}
		else 
		{
			if (c1 == CR || c1 == LF)
			{
				if (p_ap_decode_obj->pos_inbuff < p_ap_decode_obj->s_inbuff)
				{
					if (p_ap_decode_obj->boundary0 && 
					 	(!PL_strncasecmp(p_ap_decode_obj->pos_inbuff+p_ap_decode_obj->inbuff, 
									"--",
									2) 
					&&
						!PL_strncasecmp(p_ap_decode_obj->pos_inbuff+p_ap_decode_obj->inbuff+2, 
									p_ap_decode_obj->boundary0, 
									nsCRT::strlen(p_ap_decode_obj->boundary0))))
						{
							return errEOP;
						} 
				}
			}
			
			/* 
			**	general 8bits case, Rule #1
			*/ 
			size -- ;
			*buff++ = c1;
			(*real_size) ++;
		}
	}
	return EndOfBuff(p_ap_decode_obj) ? errEOB : NOERR;
}

#define UUEOL(c) (((c) == CR) || ((c) == LF))
# undef UUDEC
# define UUDEC(c) (((c) - ' ') & 077)

/* Check for and skip past the "begin" line of a uuencode body. */
PRIVATE void ensure_uu_body_state(appledouble_decode_object* p)
{
	char *end = &(p->inbuff[p->s_inbuff]);
	char *current = &(p->inbuff[p->pos_inbuff]);

	if (p->uu_state == kMainBody && p->uu_starts_line
		&& !PL_strncasecmp(current, "end", PR_MIN(3, end - current)))
		p->uu_state = kEnd;

	while (p->uu_state != kMainBody && (current < end))
	{
		switch(p->uu_state)
		{
			case kWaitingForBegin:
			case kBegin:
				/* If we're not at the beginning of a line, move to the next line. */
				if (! p->uu_starts_line)
				{
					while(current < end && !UUEOL(*current))
						current++;
					while(current < end && UUEOL(*current))
						current++;

					p->uu_starts_line = TRUE; /* we reached the start of a line */
					if (p->uu_state == kBegin) 
						p->uu_state = kMainBody;

					continue;
				}
				else
				{
					/*
						At the start of a line. Test for "begin". 

						### mwelch:

						There is a potential danger here. If a buffer ends with a line
						starting with some substring of "begin", this code will be fooled
						into thinking that the uuencode body starts with the following line.
						If the message itself contains lines that begin with a substring of
						"begin", such as "be", "because", or "bezoar", and if those lines happen
						to end a 1024-byte chunk, this becomes Really Bad. However, there is 
						no good, safe way to overcome this problem. So, for now, I hope and 
						pray that the 1024 character limit will always incorporate the entire 
						first line of a uuencode body.

						It should be noted that broken messages that have the body text in 
						the same MIME part as the uuencode attachment also risk this same 
						pitfall if any line in the message starts with "begin". 
					*/

					if ((p->uu_state == kWaitingForBegin)
						&& !PL_strncasecmp(current, "begin", PR_MIN(5, end - current)))
						p->uu_state = kBegin;
					p->uu_starts_line = FALSE; /* make us advance to next line */
				}
				break;
			case kEnd:
				/* Run out the buffer. */
				current = end;
		}
	}

	/* Record where we stopped scanning. */	
	p->pos_inbuff = p->s_inbuff - (end - current);
}

#define UU_VOID_CHAR 0

PRIVATE int fetch_next_char_uu(appledouble_decode_object* p, PRBool newBunch)
{
	char c=0;
	PRBool gotChar = FALSE;

	if (EndOfBuff(p))
		return 0;

	while(!gotChar)
	{
		if (EndOfBuff(p))
		{
			c = 0;
			gotChar = TRUE;
		}
		else if (p->uu_starts_line)
		{
			char *end = &(p->inbuff[p->s_inbuff]);
			char *current = &(p->inbuff[p->pos_inbuff]);
			
			/* Look here for 'end' line signifying end of uuencode body. */
			if (!PL_strncasecmp(current, "end", PR_MIN(3, end - current)))
			{
				p->uu_state = kEnd; /* set the uuencode state to end */
				p->pos_inbuff = p->s_inbuff; /* run out the current buffer */

				c = 0;				/* return a 0 to uudecoder */
				gotChar = TRUE;
			}
		}
		if (gotChar) 
			continue;

		c = NEXT_CHAR(p);
		
		if ((c == CR) || (c == LF))
		{
			if (newBunch)
			{
				/*	A new line could immediately follow either a CR or an LF. 
					If we reach the end of a buffer, simply assume the next buffer
					will start a line (as it should in the current libmime implementation).
					If it starts with CR or LF, that line will be skipped as well. */
				if (EndOfBuff(p) || ((CURRENT_CHAR(p) != CR) && (CURRENT_CHAR(p) != LF)))
					p->uu_starts_line = TRUE;

				continue;
			}
			
			/* End of line, but we have to finish a 4-tuple. Stop here. */
			-- p->pos_inbuff; /* give back the end-of-line character */
			c = UU_VOID_CHAR; /* flag as truncated */
			gotChar = TRUE;
		}

		/* At this point, we have a valid char. */

		else if (p->uu_starts_line)
		{
			/* read length char at start of each line */
			p->uu_line_bytes = UUDEC(c);
			p->uu_starts_line = FALSE;
			continue;
		}

		else if (p->uu_line_bytes <= 0)
			/* We ran out of bytes to decode on this line. Skip spare chars until
				we reach the end of (line or buffer). */
			continue;

		else
			gotChar = TRUE; /* valid returnable char */
	}
	
	return (int) c;
}

/* 
**	uudecode
*/

PRIVATE int from_uu(
	appledouble_decode_object* p_ap_decode_obj, 
	char	*buff, 
	int 	size, 
	PRInt32 	*real_size)
{
    char c;
    int i;
    int returnVal = NOERR;
	int 	c1, c2, c3, c4;
	
	*real_size = 0;
	
	/*	Make sure that we're in the uuencode body, or run out the buffer if
		we don't have any body text in this buffer. */
	ensure_uu_body_state(p_ap_decode_obj);
	
	if (p_ap_decode_obj->uu_state == kEnd)
		return errEOP;
	
	/* Continue with what was left over last time. */
	for (i = p_ap_decode_obj->state64; i<4; i++)
	{
		if (EndOfBuff(p_ap_decode_obj))
		{
			p_ap_decode_obj->state64 = i;
			break;
		}
		if ((p_ap_decode_obj->c[i] = fetch_next_char_uu(p_ap_decode_obj, (i==0))) == 0)
			break;
	}

	if ( (i < p_ap_decode_obj->uu_line_bytes+1)
		&& (EndOfBuff(p_ap_decode_obj)))
		/* not enough data to decode, return here. */
		return errEOB;

	while((size > 0) && (!EndOfBuff(p_ap_decode_obj)))
	{
		c1 = p_ap_decode_obj->c[0];
		c2 = p_ap_decode_obj->c[1];
		c3 = p_ap_decode_obj->c[2];
		c4 = p_ap_decode_obj->c[3];
		
		/*
			At this point we have characters ready to decode.
			Convert them to binary bytes.
		*/
		if ((i > 1) 
			&& (p_ap_decode_obj->uu_bytes_written < 1)
			&& (p_ap_decode_obj->uu_line_bytes > 0))
		{
			c = UUDEC(c1) << 2 | UUDEC(c2) >> 4;
			size --;
			*buff ++ = c;
			(*real_size) ++;
			p_ap_decode_obj->uu_line_bytes--;
			p_ap_decode_obj->uu_bytes_written++;
		}
		
		if ((i > 2) && (size > 0) 
			&& (p_ap_decode_obj->uu_bytes_written < 2)
			&& (p_ap_decode_obj->uu_line_bytes > 0))
		{
			c = UUDEC(c2) << 4 | UUDEC(c3) >> 2;
			size --;
			*buff ++ = c;
			(*real_size) ++;
			p_ap_decode_obj->uu_line_bytes--;
			p_ap_decode_obj->uu_bytes_written++;
		}

		if ((i > 3) && (size > 0)
			&& (p_ap_decode_obj->uu_line_bytes > 0))
		{
			c = UUDEC(c3) << 6 | UUDEC(c4);
			size --;
			*buff ++ = c;
			(*real_size) ++;
			p_ap_decode_obj->uu_line_bytes--;
			p_ap_decode_obj->uu_bytes_written = 0;
		}

		if (p_ap_decode_obj->uu_state == kEnd)
			continue;

		/* If this line is finished, this tuple is also finished. */
		if (p_ap_decode_obj->uu_line_bytes <= 0)
			p_ap_decode_obj->uu_bytes_written = 0;

		if (p_ap_decode_obj->uu_bytes_written > 0)
		{
			/* size == 0, but we have bytes left in current tuple */
			p_ap_decode_obj->state64 = i;
			continue;
		}
			
		/*
		**	fetch the next 4 character group. 
		*/
			
		for (i = 0; i < 4; i++)
		{
			if (EndOfBuff(p_ap_decode_obj))
				break;

			if ((p_ap_decode_obj->c[i] = fetch_next_char_uu(p_ap_decode_obj, (i == 0))) == 0)
				break;
		}

		p_ap_decode_obj->state64 = i;

		if ( (i < p_ap_decode_obj->uu_line_bytes+1)
			&& (EndOfBuff(p_ap_decode_obj)))
			/* not enough data to decode, return here. */
			continue;
	}

	if (p_ap_decode_obj->uu_state == kEnd)
		returnVal = errEOP;
	else if (EndOfBuff(p_ap_decode_obj))
		returnVal = errEOB;
		
	return returnVal;
}

/*
**	from_none
**
**	plain text transfer.
*/
PRIVATE int from_none(
	appledouble_decode_object* p_ap_decode_obj, 
	char 	*buff, 
	int 	size, 
	PRInt32 	*real_size)
{
	char 	c;
    int  	i, status = NOERR; 
    int		left = p_ap_decode_obj->s_inbuff - p_ap_decode_obj->pos_inbuff;
    int		total = PR_MIN(size, left);
    
	for (i = 0; i < total; i++)
	{	
		*buff ++ = c = NEXT_CHAR(p_ap_decode_obj);
		if (c == CR || c == LF)
		{
			/* make sure the next thing is not a boundary string */
			if (p_ap_decode_obj->pos_inbuff < p_ap_decode_obj->s_inbuff)
			{
				if (p_ap_decode_obj->boundary0 && 
				 	(!PL_strncasecmp(p_ap_decode_obj->pos_inbuff+p_ap_decode_obj->inbuff, 
								"--",
								2) 
				&&
					!PL_strncasecmp(p_ap_decode_obj->pos_inbuff+p_ap_decode_obj->inbuff+2, 
								p_ap_decode_obj->boundary0, 
								nsCRT::strlen(p_ap_decode_obj->boundary0))))
					{
						status = errEOP;
						break;
					} 
			}
		}
	}
	
	*real_size = i;
	if (status == NOERR)
		status = (left == i) ? errEOB : status;
	return status;
}

