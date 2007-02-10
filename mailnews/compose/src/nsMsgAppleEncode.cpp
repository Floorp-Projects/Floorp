/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   apple_double_encode.c
 *	 ---------------------
 *
 *    The routines doing the Apple Double Encoding.
 *		
 *			2aug95	mym	Created.
 *		
 */

#include "nscore.h"
#include "msgCore.h"
#include "nsMimeTypes.h"
#include "prprf.h"

#include "nsMsgAppleDouble.h"
#include "nsMsgAppleCodes.h"

#ifdef XP_MACOSX

#include <Errors.h>

/*
**	Local Functions prototypes.
*/
static int output64chunk( appledouble_encode_object* p_ap_encode_obj, 
				int c1, int c2, int c3, int pads);
				
static int to64(appledouble_encode_object* p_ap_encode_obj, 
				char	*p, 
				int 	in_size);
 
static int finish64(appledouble_encode_object* p_ap_encode_obj);


#define BUFF_LEFT(p)	((p)->s_outbuff - (p)->pos_outbuff)	

/*
**	write_stream.
*/
int write_stream(
	appledouble_encode_object *p_ap_encode_obj,
	char 	*out_string,
	int	 	len)			
{	
	if (p_ap_encode_obj->pos_outbuff + len < p_ap_encode_obj->s_outbuff)
	{
		memcpy(p_ap_encode_obj->outbuff + p_ap_encode_obj->pos_outbuff, 
		       out_string, 
		       len);
		p_ap_encode_obj->pos_outbuff += len;
		return noErr;
	}
	else
	{
		/*
		**	If the buff doesn't have enough space, use the overflow buffer then.
		*/
		int s_len = p_ap_encode_obj->s_outbuff - p_ap_encode_obj->pos_outbuff;
		
		memcpy(p_ap_encode_obj->outbuff + p_ap_encode_obj->pos_outbuff, 
		       out_string, 
		       s_len);
		memcpy(p_ap_encode_obj->b_overflow + p_ap_encode_obj->s_overflow,
		       out_string + s_len,
		       p_ap_encode_obj->s_overflow += (len - s_len));
		p_ap_encode_obj->pos_outbuff += s_len;
		return errEOB;
	}
}

int fill_apple_mime_header(
	appledouble_encode_object *p_ap_encode_obj)
{
	int  status;
	
	char tmpstr[266];
	
#if 0	
//	strcpy(tmpstr, "Content-Type: multipart/mixed; boundary=\"-\"\n\n---\n");
//	status = write_stream(p_ap_encode_env, 
//						tmpstr,
//						strlen(tmpstr));
//	if (status != noErr)
//		return status;

	PR_snprintf(tmpstr, sizeof(tmpstr),
			"Content-Type: multipart/appledouble; boundary=\"=\"; name=\"");
	status = write_stream(p_ap_encode_obj, 
						tmpstr,
						strlen(tmpstr));
	if (status != noErr)
		return status;
		
	status = write_stream(p_ap_encode_obj,
						p_ap_encode_obj->fname,
						nsCRT::strlen(p_ap_encode_obj->fname));
	if (status != noErr)
		return status;
		
	PR_snprintf(tmpstr, sizeof(tmpstr),
			"\"\r\nContent-Disposition: inline; filename=\"%s\"\r\n\r\n\r\n--=\r\n",
			p_ap_encode_obj->fname);
#endif
	PR_snprintf(tmpstr, sizeof(tmpstr), "--%s"CRLF, p_ap_encode_obj->boundary);
	status = write_stream(p_ap_encode_obj, 
						tmpstr, 
						strlen(tmpstr));
	return status;
} 

int ap_encode_file_infor(
	appledouble_encode_object *p_ap_encode_obj)
{
	CInfoPBRec cipbr;
	HFileInfo *fpb = (HFileInfo *)&cipbr;
	ap_header	head;
	ap_entry 	entries[NUM_ENTRIES];
	ap_dates 	dates;
	short 		i;
	long 		comlen;
	DateTimeRec cur_time;
	unsigned 	long cur_secs;
	char 		comment[256];
	Str63		fname;
	int	 		status;

	strcpy((char *)fname+1,p_ap_encode_obj->fname);
	fname[0] = strlen(p_ap_encode_obj->fname);
	
	fpb->ioNamePtr = fname;
	fpb->ioDirID   = p_ap_encode_obj->dirId;
	fpb->ioVRefNum = p_ap_encode_obj->vRefNum;
	fpb->ioFDirIndex = 0;
	if (PBGetCatInfoSync(&cipbr) != noErr) 
	{
		return errFileOpen;
	}

	/* get a file comment, if possible */
#if TARGET_CARBON
    // not sure why working directories are needed here...
    comlen = 0;
#else
	long 		procID;
	procID = 0;
	GetWDInfo(p_ap_encode_obj->vRefNum, &fpb->ioVRefNum, &fpb->ioDirID, &procID);
	IOParam 	vinfo;
	memset((void *) &vinfo, '\0', sizeof (vinfo));
	GetVolParmsInfoBuffer vp;
	vinfo.ioCompletion  = nil;
	vinfo.ioVRefNum 	= fpb->ioVRefNum;
	vinfo.ioBuffer 		= (Ptr) &vp;
	vinfo.ioReqCount 	= sizeof (vp);
	comlen = 0;
	if (PBHGetVolParmsSync((HParmBlkPtr) &vinfo) == noErr &&
		((vp.vMAttrib >> bHasDesktopMgr) & 1)) 
	{
		DTPBRec 	dtp;
		memset((void *) &dtp, '\0', sizeof (dtp));
		dtp.ioVRefNum = fpb->ioVRefNum;
		if (PBDTGetPath(&dtp) == noErr) 
		{
			dtp.ioCompletion = nil;
			dtp.ioDTBuffer = (Ptr) comment;
			dtp.ioNamePtr  = fpb->ioNamePtr;
			dtp.ioDirID    = fpb->ioFlParID;
			if (PBDTGetCommentSync(&dtp) == noErr) 
				comlen = dtp.ioDTActCount;
		}
	}
#endif
	
	/* write header */
//	head.magic = dfork ? APPLESINGLE_MAGIC : APPLEDOUBLE_MAGIC;
	head.magic   = APPLEDOUBLE_MAGIC;		/* always do apple double */
	head.version = VERSION;
	memset(head.fill, '\0', sizeof (head.fill));
	head.entries = NUM_ENTRIES - 1;
	status = to64(p_ap_encode_obj,
					(char *) &head,
					sizeof (head));				
	if (status != noErr)
		return status;

	/* write entry descriptors */
	entries[0].offset = sizeof (head) + sizeof (ap_entry) * head.entries;
	entries[0].id 	= ENT_NAME;
	entries[0].length = *fpb->ioNamePtr;
	entries[1].id 	= ENT_FINFO;
	entries[1].length = sizeof (FInfo) + sizeof (FXInfo);
	entries[2].id 	= ENT_DATES;
	entries[2].length = sizeof (ap_dates);
	entries[3].id 	= ENT_COMMENT;
	entries[3].length = comlen;
	entries[4].id 	= ENT_RFORK;
	entries[4].length = fpb->ioFlRLgLen;
	entries[5].id 	= ENT_DFORK;
	entries[5].length = fpb->ioFlLgLen;

	/* correct the link in the entries. */
	for (i = 1; i < NUM_ENTRIES; ++i) 
	{
		entries[i].offset = entries[i-1].offset + entries[i-1].length;
	}
	status = to64(p_ap_encode_obj,
					(char *) entries,
					sizeof (ap_entry) * head.entries); 
	if (status != noErr)
		return status;

	/* write name */
	status = to64(p_ap_encode_obj,
					(char *) fpb->ioNamePtr + 1,
					*fpb->ioNamePtr); 
	if (status != noErr)
		return status;
	
	/* write finder info */
	status = to64(p_ap_encode_obj,
					(char *) &fpb->ioFlFndrInfo,
					sizeof (FInfo));
	if (status != noErr)
		return status;
					  
	status = to64(p_ap_encode_obj,
					(char *) &fpb->ioFlXFndrInfo,
					sizeof (FXInfo));
	if (status != noErr)
		return status;

	/* write dates */
	GetTime(&cur_time);
	DateToSeconds(&cur_time, &cur_secs);
	dates.create = fpb->ioFlCrDat + CONVERT_TIME;
	dates.modify = fpb->ioFlMdDat + CONVERT_TIME;
	dates.backup = fpb->ioFlBkDat + CONVERT_TIME;
	dates.access = cur_secs + CONVERT_TIME;
	status = to64(p_ap_encode_obj,
					(char *) &dates,
					sizeof (ap_dates)); 
	if (status != noErr)
		return status;
	
	/* write comment */
	if (comlen)
	{
		status = to64(p_ap_encode_obj,
					comment,
					comlen * sizeof(char));
	}
	/*
	**	Get some help information on deciding the file type.
	*/
	if (fpb->ioFlFndrInfo.fdType == 'TEXT' || fpb->ioFlFndrInfo.fdType == 'text')
	{
		p_ap_encode_obj->text_file_type = true;
	}
	
	return status;	
}
/*
**	ap_encode_header
**
**		encode the file header and the resource fork.
**
*/
int ap_encode_header(
	appledouble_encode_object* p_ap_encode_obj, 
	PRBool  firstime)
{
	Str255 	name;
	char   	rd_buff[256];
	short   fileId;
	OSErr	retval = noErr;
	int    	status;
	long	inCount;
	
	if (firstime)
	{
    PL_strcpy(rd_buff, 
			"Content-Type: application/applefile\r\nContent-Transfer-Encoding: base64\r\n\r\n");
		status = write_stream(p_ap_encode_obj,
			 				rd_buff, 
			 				strlen(rd_buff)); 
		if (status != noErr)
			return status;
			
		status = ap_encode_file_infor(p_ap_encode_obj); 
		if (status != noErr)
			return status;
		
		/*
		** preparing to encode the resource fork.
		*/
		name[0] = strlen(p_ap_encode_obj->fname);
		strcpy((char *)name+1, p_ap_encode_obj->fname);
		if (HOpenRF(p_ap_encode_obj->vRefNum, p_ap_encode_obj->dirId,
					name, fsRdPerm,
					&p_ap_encode_obj->fileId) != noErr)
		{
			return errFileOpen;			
		}
	}

	fileId = p_ap_encode_obj->fileId;
	while (retval == noErr)
	{
		if (BUFF_LEFT(p_ap_encode_obj) < 400)
			break;
			
		inCount = 256;
		retval = FSRead(fileId, &inCount, rd_buff);
		if (inCount)
		{
			status = to64(p_ap_encode_obj,
							rd_buff,
							inCount);
			if (status != noErr)
				return status;
		}
	}
	
	if (retval == eofErr)
	{
		FSClose(fileId);

		status = finish64(p_ap_encode_obj);
		if (status != noErr)
			return status;
		
		/*
		** write out the boundary 
		*/
		PR_snprintf(rd_buff, sizeof(rd_buff),
						CRLF"--%s"CRLF, 
						p_ap_encode_obj->boundary);
					
		status = write_stream(p_ap_encode_obj,
						rd_buff,
						strlen(rd_buff));
		if (status == noErr)
			status = errDone;
	}
	return status;
}

static void replace(char *p, int len, char frm, char to)
{
	for (; len > 0; len--, p++)
		if (*p == frm)	*p = to;
}

/* Description of the various file formats and their magic numbers 		*/
struct magic 
{
    char 	*name;			/* Name of the file format 					*/
    char 	*num;			/* The magic number 						*/
    int 	len;			/* Length (0 means strlen(magicnum)) 		*/
};

/* The magic numbers of the file formats we know about */
static struct magic magic[] = 
{
    { "image/gif", 	"GIF", 			  0 },
    { "image/jpeg", "\377\330\377",   0 },
    { "video/mpeg", "\0\0\001\263",	  4 },
    { "application/postscript", "%!", 0 },
};
static int 	num_magic = (sizeof(magic)/sizeof(magic[0]));

static char *text_type    = TEXT_PLAIN;					/* the text file type.	*/		
static char *default_type = APPLICATION_OCTET_STREAM;


/*
 * Determins the format of the file "inputf".  The name
 * of the file format (or NULL on error) is returned.
 */
static char *magic_look(char *inbuff, int numread)
{
    int i, j;

	for (i=0; i<num_magic; i++) 
	{
	   	if (magic[i].len == 0) 
	   		magic[i].len = strlen(magic[i].num);
	}

    for (i=0; i<num_magic; i++) 
    {
		if (numread >= magic[i].len) 
		{
	    	for (j=0; j<magic[i].len; j++) 
	    	{
				if (inbuff[j] != magic[i].num[j]) break;
	    	}
	    	
	    	if (j == magic[i].len) 
	    		return magic[i].name;
		}
    }

    return default_type;
}
/*
**	ap_encode_data
**
**	---------------
**
**		encode on the data fork.
**
*/
int ap_encode_data(
	appledouble_encode_object* p_ap_encode_obj, 
	PRBool firstime)
{
	Str255		name;
	char   		rd_buff[256];
	short		fileId;
	OSErr		retval = noErr;
	long		in_count;
	int			status;
	
	if (firstime)
	{	
		char* magic_type;
			
		/*
		** preparing to encode the data fork.
		*/
		name[0] = strlen(p_ap_encode_obj->fname);
    PL_strcpy((char*)name+1, p_ap_encode_obj->fname);
		if (HOpen( 	p_ap_encode_obj->vRefNum,
					p_ap_encode_obj->dirId, 
					name, 
					fsRdPerm, 
					&fileId) != noErr)
		{
			return errFileOpen;
		}
		p_ap_encode_obj->fileId = fileId;
			
		
		if (!p_ap_encode_obj->text_file_type)
		{	
      /*
      **	do a smart check for the file type.
      */
      in_count = 256;
      retval 	 = FSRead(fileId, &in_count, rd_buff);
      magic_type = magic_look(rd_buff, in_count);
      
      /* don't forget to rewind the index to start point. */ 
      SetFPos(fileId, fsFromStart, 0L);
      /* and reset retVal just in case... */
      if (retval == eofErr)
        retval = noErr;
		}
		else
		{
			magic_type = text_type;		/* we already know it is a text type.	*/
		}

		/*
		**	the data portion header information.
		*/
		PR_snprintf(rd_buff, sizeof(rd_buff),
			"Content-Type: %s; name=\"%s\"" CRLF "Content-Transfer-Encoding: base64" CRLF "Content-Disposition: inline; filename=\"%s\""CRLF CRLF,
			magic_type,
			p_ap_encode_obj->fname,
			p_ap_encode_obj->fname);
			
		status = write_stream(p_ap_encode_obj, 
					rd_buff, 
					strlen(rd_buff)); 
		if (status != noErr)
			return status;
	}
	
	while (retval == noErr)
	{
		if (BUFF_LEFT(p_ap_encode_obj) < 400)
			break;
			
		in_count = 256;
		retval = FSRead(p_ap_encode_obj->fileId, 
						&in_count, 
						rd_buff);
		if (in_count)
		{
/*			replace(rd_buff, in_count, '\r', '\n');	 						*/
/* ** may be need to do character set conversion here for localization.	**  */		
			status = to64(p_ap_encode_obj,
						rd_buff,
						in_count);
			if (status != noErr)
				return status;
		}
	}
	
	if (retval == eofErr)
	{
		FSClose(p_ap_encode_obj->fileId);

		status = finish64(p_ap_encode_obj);
		if (status != noErr)
			return status;
		
		/* write out the boundary 	*/
		
		PR_snprintf(rd_buff, sizeof(rd_buff),
						CRLF"--%s--"CRLF CRLF, 
						p_ap_encode_obj->boundary);
	
		status = write_stream(p_ap_encode_obj,
						rd_buff,
						strlen(rd_buff));
	
		if (status == noErr)				
			status = errDone;
	}
	return status;
}

static char basis_64[] =
   "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* 
**	convert the stream in the inbuff to 64 format and put it in the out buff.
**  To make the life easier, the caller will responcable of the cheking of the outbuff's bundary.
*/
static int 
to64(appledouble_encode_object* p_ap_encode_obj, 
	char	*p, 
	int 	in_size) 
{
	int 	status;
    int c1, c2, c3, ct;
    unsigned char *inbuff = (unsigned char*)p;
    
	ct = p_ap_encode_obj->ct;			/* the char count left last time. */
	
	/*
	**	 resume the left state of the last conversion.
	*/
	switch (p_ap_encode_obj->state64)
	{
		case 0:
			p_ap_encode_obj->c1 = c1 = *inbuff ++;
			if (--in_size <= 0)
			{
				p_ap_encode_obj->state64 = 1;
				return noErr;
			}
			p_ap_encode_obj->c2 = c2 = *inbuff ++;
			if (--in_size <= 0)
			{
				p_ap_encode_obj->state64 = 2;
				return noErr;
			}
			c3 = *inbuff ++;		--in_size;
			break;
		case 1:
			c1 = p_ap_encode_obj->c1;
			p_ap_encode_obj->c2 = c2 = *inbuff ++;
			if (--in_size <= 0)
			{
				p_ap_encode_obj->state64 = 2;
				return noErr;
			}
			c3 = *inbuff ++;		--in_size;
			break;
		case 2:
			c1 = p_ap_encode_obj->c1;
			c2 = p_ap_encode_obj->c2;
			c3 = *inbuff ++;		--in_size;
			break;
	}
	
    while (in_size >= 0) 
    {
    	status = output64chunk(p_ap_encode_obj, 
    							c1, 
    							c2, 
    							c3, 
    							0);
    	if (status != noErr)
    		return status;
    		
    	ct += 4;
        if (ct > 71) 
        { 
        	status = write_stream(p_ap_encode_obj, 
        						CRLF, 
        						2);
        	if (status != noErr)
        		return status;
        		
            ct = 0;
        }

		if (in_size <= 0)
		{
			p_ap_encode_obj->state64 = 0;
			break;
		}
		
		c1 = (int)*inbuff++;
		if (--in_size <= 0)
		{
			p_ap_encode_obj->c1 = c1;
			p_ap_encode_obj->state64 = 1;
			break;
		}
		c2 = *inbuff++;
		if (--in_size <= 0)
		{
			p_ap_encode_obj->c1 	 = c1;
			p_ap_encode_obj->c2 	 = c2;
			p_ap_encode_obj->state64 = 2;
			break;
		}
		c3 = *inbuff++;
		in_size--;
    }
    p_ap_encode_obj->ct = ct;
    return status;
}

/*
** clear the left base64 encodes.
*/
static int 
finish64(appledouble_encode_object* p_ap_encode_obj)
{
	int status;
	
	switch (p_ap_encode_obj->state64)
	{
		case 0:
			break;
		case 1:
			status = output64chunk(p_ap_encode_obj, 
									p_ap_encode_obj->c1, 
									0, 
									0, 
									2);
			break;
		case 2:
			status = output64chunk(p_ap_encode_obj, 
									p_ap_encode_obj->c1, 
									p_ap_encode_obj->c2, 
									0, 
									1);
			break;
	}
	status = write_stream(p_ap_encode_obj, CRLF, 2);
	p_ap_encode_obj->state64 = 0;
	p_ap_encode_obj->ct	  	 = 0;
	return status;
}

static int output64chunk(
	appledouble_encode_object* p_ap_encode_obj, 
	int c1, int c2, int c3, int pads)
{
	char tmpstr[32];
	char *p = tmpstr;
	
    *p++ = basis_64[c1>>2];
    *p++ = basis_64[((c1 & 0x3)<< 4) | ((c2 & 0xF0) >> 4)];
    if (pads == 2) 
    {
        *p++ = '=';
        *p++ = '=';
    } 
    else if (pads) 
    {
        *p++ = basis_64[((c2 & 0xF) << 2) | ((c3 & 0xC0) >>6)];
        *p++ = '=';
    } 
    else 
    {
        *p++ = basis_64[((c2 & 0xF) << 2) | ((c3 & 0xC0) >>6)];
        *p++ = basis_64[c3 & 0x3F];
    }
	return write_stream(p_ap_encode_obj,
						tmpstr,
						p-tmpstr);
}

#endif /* XP_MACOSX */
