DON'T COMPILE ME!!!
RICHIE
RICHIE
RICHIE      OLD FE INTERFACES TO APPLEDOUBLE ENCODE/DECODE
RICHIE
RICHIE
RICHIE

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

/**
*	Apple Double encode/decode stream
*	----------------------------------
*
*		11sep95		mym		created.
*/

#include "nscore.h"
#include "msgCore.h"
#include "prio.h"

#include "nsMsgAppleDouble.h"
#include "nsMsgAppleCodes.h"

#include "nsMsgBinHex.h"
#include "m_cvstrm.h"

extern int MK_MSG_SAVE_ATTACH_AS;

#ifdef RICHIE_XP_MAC
#pragma warn_unusedarg off

extern int MK_UNABLE_TO_OPEN_TMP_FILE;
extern int MK_MIME_ERROR_WRITING_FILE;

/*	---------------------------------------------------------------------------------
**
**		The codes for Apple-double encoding stream. --- it's only useful on Mac OS
**
**	---------------------------------------------------------------------------------
*/

#define WORKING_BUFF_SIZE	8192

typedef struct	_AppledoubleEncodeObject
{
	appledouble_encode_object	ap_encode_obj;
	
	char*	buff;							/*	the working buff.					*/
	PRInt32	s_buff;							/*	the working buff size.				*/

	RICHIE_XP_File	fp;								/*  file to hold the encoding			*/
	char	*fname;							/*	and the file name.					*/
	
} AppleDoubleEncodeObject;

/*
	Let's go "l" characters forward of the encoding for this write.
	Note:	
		"s" is just a dummy paramter. 
 */
PRIVATE int 
net_AppleDouble_Encode_Write (
	void *stream, const char* s, PRInt32 l)
{
	int  status = 0;
	AppleDoubleEncodeObject * obj = (AppleDoubleEncodeObject*)stream;
	PRInt32 count, size;	
			
	while (l > 0)
	{
		size = obj->s_buff * 11 / 16;
		size = PR_MIN(l, size);
		status = ap_encode_next(&(obj->ap_encode_obj), 
							obj->buff, 
							size, 
							&count);
		if (status == noErr || status == errDone)
		{
			/*
			 * we get the encode data, so call the next stream to write it to the disk.
			 */
			if (RICHIE_XP_FileWrite(obj->buff, count, obj->fp) != count)
				return errFileWrite;
		}
		
		if (status != noErr ) 							/* abort when error	/ done?		*/
			break;
			
		l -= size;
	}
	return status;
}

/*
** is the stream ready for writing?
 */
PRIVATE unsigned int net_AppleDouble_Encode_Ready (void *stream)
{	
   return(PR_MAX_WRITE_READY);  						/* always ready for writing 	*/ 
}


PRIVATE void net_AppleDouble_Encode_Complete (void *stream)
{
	AppleDoubleEncodeObject * obj = (AppleDoubleEncodeObject*)stream;	

    ap_encode_end(&(obj->ap_encode_obj), false);	/* this is a normal ending 		*/
 	
 	if (obj->fp)
 	{
    	RICHIE_XP_FileClose(obj->fp);						/* done with the target file	*/

        PR_FREEIF(obj->fname);							/* and the file name too		*/
    }
								
    PR_FREEIF(obj->buff);								/* free the working buff.		*/
    PR_FREEIF(obj);
}

PRIVATE void net_AppleDouble_Encode_Abort (void *stream, int status)
{
	AppleDoubleEncodeObject * obj = (AppleDoubleEncodeObject*)stream;	
	
    ap_encode_end(&(obj->ap_encode_obj), true);		/* it is an aborting exist... 	*/

 	if (obj->fp)
 	{   
 		RICHIE_XP_FileClose(obj->fp);

        RICHIE_XP_FileRemove (obj->fname, xpURL);			/* remove the partial file.		*/
        
        PR_FREEIF(obj->fname);
	}
    PR_FREEIF(obj->buff);								/* free the working buff.		*/
    PR_FREEIF(obj);
}

/*
**	fe_MakeAppleDoubleEncodeStream
**	------------------------------
**
**	Will create a apple double encode stream:
**
**		-> take the filename as the input source (it needs to be a mac file.)
**		-> take a file name for the temp file we are generating.
*/

PUBLIC NET_StreamClass * 
fe_MakeAppleDoubleEncodeStream (int  format_out,
                         void       *data_obj,
                         URL_Struct *URL_s,
                         MWContext  *window_id,
                         char*		src_filename,
                         char* 		dst_filename,
                         char*		separator)
{
    AppleDoubleEncodeObject* obj;
    NET_StreamClass* 	  stream;
    char*				  working_buff = NULL;
	int					  bSize = WORKING_BUFF_SIZE;
    
    TRACEMSG(("Setting up apple encode stream. Have URL: %s\n", URL_s->address));

    stream = new (NET_StreamClass);
    if(stream == NULL) 
        return(NULL);

    obj = new (AppleDoubleEncodeObject);
    if (obj == NULL) 
    {
        PR_FREEIF (stream);
        return(NULL);
    }
   
	while (!working_buff && (bSize >= 512))
	{
   		working_buff = (char *)PR_CALLOC(bSize);
		if (!working_buff)
			bSize /= 2;
	}
   	if (working_buff == NULL)
   	{
   		PR_FREEIF (obj);
   		PR_FREEIF (stream);
		return (NULL);
   	}
  	
    stream->name           = "Apple Double Encode";
    stream->complete       = (MKStreamCompleteFunc) 	net_AppleDouble_Encode_Complete;
    stream->abort          = (MKStreamAbortFunc) 		net_AppleDouble_Encode_Abort;
    stream->put_block      = (MKStreamWriteFunc) 		net_AppleDouble_Encode_Write;
    stream->is_write_ready = (MKStreamWriteReadyFunc) 	net_AppleDouble_Encode_Ready;
    stream->data_object    = obj;
    stream->window_id      = window_id;

	obj->fp  = RICHIE_XP_FileOpen(dst_filename, xpFileToPost, RICHIE_XP_FILE_WRITE_BIN);
	if (obj->fp == NULL)
	{
		PR_FREEIF (working_buff);
   		PR_FREEIF (obj);
   		PR_FREEIF (stream);
		return (NULL);
	}
	
  obj->fname = nsCRT::strdup(dst_filename);
	 
	obj->buff	= working_buff;
	obj->s_buff = bSize;
	
	/*
	** 	setup all the need information on the apple double encoder.
	*/
	ap_encode_init(&(obj->ap_encode_obj), 
					src_filename,					/* pass the file name of the source.	*/
					separator);
					
    TRACEMSG(("Returning stream from NET_AppleDoubleEncoder\n"));

    return stream;
}
#endif

/*
**	---------------------------------------------------------------------------------
**
**		The codes for the Apple sigle/double decoding.
**
**	---------------------------------------------------------------------------------
*/
typedef	struct AppleDoubleDecodeObject
{
	appledouble_decode_object	ap_decode_obj;
	
	char*	in_buff;						/* the temporary buff to accumulate  	*/
											/* the input, make sure the call to  	*/
											/* the dedcoder engine big enough buff 	*/
	PRInt32	bytes_in_buff;					/* the count for the temporary buff.	*/
	
    NET_StreamClass*	binhex_stream;		/* a binhex encode stream to convert 	*/
    										/* the decoded mac file to binhex.		*/
    										
} AppleDoubleDecodeObject;

PRIVATE int
net_AppleDouble_Decode_Write (
	void *stream, const char* s, PRInt32 l)
{
	int 	status = NOERR;
	AppleDoubleDecodeObject * obj = (AppleDoubleDecodeObject*) stream;
	PRInt32	size;	

	/*
	**	To force an effecient decoding, we should	
	**  make sure that the buff pass to the decode next is great than 1024 bytes.
	*/
	if (obj->bytes_in_buff + l > 1024)
	{
		size = 1024 - obj->bytes_in_buff;
		memcpy(obj->in_buff+obj->bytes_in_buff, 
		       s, 
		       size);
		s += size;
		l -= size;
		
		status = ap_decode_next(&(obj->ap_decode_obj), 
					obj->in_buff, 
					1024);
		obj->bytes_in_buff = 0;
	}
	
	if (l > 1024)
	{
		/* we are sure that obj->bytes_in_buff == 0 at this point. */
		status = ap_decode_next(&(obj->ap_decode_obj), 
					(char *)s, 
					l);		
	}
	else
	{
		/* and we are sure we will not get overflow with the buff. */ 
		memcpy(obj->in_buff+obj->bytes_in_buff, 
		       s, 
		       l);
		obj->bytes_in_buff += l;
	}
	return status;
}

PRIVATE unsigned int 
net_AppleDouble_Decode_Ready (NET_StreamClass *stream)
{	
   return(PR_MAX_WRITE_READY); 	 						/* always ready for writing 	*/ 
}


PRIVATE void 
net_AppleDouble_Decode_Complete (void *stream)
{
	AppleDoubleDecodeObject *obj = (AppleDoubleDecodeObject *)stream;	
	
	if (obj->bytes_in_buff)
	{
		
		ap_decode_next(&(obj->ap_decode_obj), 			/* do the last calls.			*/
				(char *)obj->in_buff,
				obj->bytes_in_buff);
		obj->bytes_in_buff = 0;
	}

    ap_decode_end(&(obj->ap_decode_obj), FALSE);		/* it is a normal clean up cases.*/

	if (obj->binhex_stream)
		PR_FREEIF(obj->binhex_stream);
	
	if (obj->in_buff)
		PR_FREEIF(obj->in_buff);
		
	PR_FREEIF(obj);
}

PRIVATE void 
net_AppleDouble_Decode_Abort (
	void *stream, int status)
{
	AppleDoubleDecodeObject *obj = (AppleDoubleDecodeObject *)stream;	

	ap_decode_end(&(obj->ap_decode_obj), TRUE);			/* it is an abort. 				*/
	
	if (obj->binhex_stream)
		PR_FREEIF(obj->binhex_stream);
	
	if (obj->in_buff)
		PR_FREEIF(obj->in_buff);
		
	PR_FREEIF(obj);
}


/*
**	fe_MakeAppleDoubleDecodeStream_1
**	---------------------------------
**	
**		Create the apple double decode stream.
**
**		In the Mac OS, it will create a stream to decode to an apple file;
**
**		In other OS,  the stream will decode apple double object, 
**					  then encode it in binhex format, and save to the file.
*/
#ifndef RICHIE_XP_MAC
static void
simple_copy(MWContext* context, char* saveName, void* closure)
{
	/* just copy the filename to the closure, so the caller can get it.	*/
  PL_strcpy(closure, saveName);
}
#endif

PUBLIC NET_StreamClass * 
fe_MakeAppleDoubleDecodeStream_1 (int  format_out,
                         void       *data_obj,
                         URL_Struct *URL_s,
                         MWContext  *window_id)
{
#ifdef RICHIE_XP_MAC
	return fe_MakeAppleDoubleDecodeStream(format_out, 
						data_obj, 
						URL_s, 
						window_id, 
						false,
						NULL);
#else

#if 0		/* just a test in the mac OS	*/
	NET_StreamClass *p;
	char*	url; 
	StandardFileReply	reply;
		
	StandardPutFile("\pSave binhex encoded file as:", "\pUntitled", &reply);
	if (!reply.sfGood) 
	{
		return NULL;
	}
	url = my_PathnameFromFSSpec(&(reply.sfFile));
	
	p = fe_MakeAppleDoubleDecodeStream(format_out, 
						data_obj, 
						URL_s, 
						window_id, 
						true,
						url+7);
	PR_FREEIF(url);
	return (p);

#else	/* for the none mac-os to get a file name */

	NET_StreamClass *p;
	char* filename;
	
	filename = PR_CALLOC(1024);
	if (filename == NULL)
		return NULL;

/***** RICHIE - CONVERT THIS
	if (FE_PromptForFileName(window_id, 
				RICHIE_XP_GetString(MK_MSG_SAVE_ATTACH_AS),
				0,
				FALSE,
				FALSE,
				simple_copy,
				filename) == -1)
	{
		return	NULL;
	}
***/

	p = fe_MakeAppleDoubleDecodeStream(format_out, 
						data_obj, 
						URL_s, 
						window_id, 
						TRUE,
						filename);
	PR_FREEIF(filename);
	return (p);	

#endif
	
#endif
}

                    
PUBLIC NET_StreamClass * 
fe_MakeAppleDoubleDecodeStream (int  format_out,
                         void       *data_obj,
                         URL_Struct *URL_s,
                         MWContext  *window_id,
                         PRBool	write_as_binhex,
                         char 		*dst_filename)	
{
    AppleDoubleDecodeObject* obj;
    NET_StreamClass* 	  stream;
    
    TRACEMSG(("Setting up apple double decode stream. Have URL: %s\n", URL_s->address));

    stream = new (NET_StreamClass);
    if(stream == NULL) 
        return(NULL);

    obj = new (AppleDoubleDecodeObject);
    if (obj == NULL)
    {
    	PR_FREEIF(stream);
        return(NULL);
    }
    
    stream->name           = "AppleDouble Decode";
    stream->complete       = (MKStreamCompleteFunc) 	net_AppleDouble_Decode_Complete;
    stream->abort          = (MKStreamAbortFunc) 		net_AppleDouble_Decode_Abort;
    stream->put_block      = (MKStreamWriteFunc) 		net_AppleDouble_Decode_Write;
    stream->is_write_ready = (MKStreamWriteReadyFunc) 	net_AppleDouble_Decode_Ready;
    stream->data_object    = obj;
    stream->window_id      = window_id;

	/*
	** setup all the need information on the apple double encoder.
	*/
	obj->in_buff = (char *)PR_CALLOC(1024);
	if (obj->in_buff == NULL)
	{
		PR_FREEIF(obj);
		PR_FREEIF(stream);
		return (NULL);
	}
	
	obj->bytes_in_buff = 0;
	
	if (write_as_binhex)
	{
		obj->binhex_stream = 
			fe_MakeBinHexEncodeStream(format_out,
							data_obj,
							URL_s,
							window_id,
							dst_filename);
		if (obj->binhex_stream == NULL)
		{
			PR_FREEIF(obj);
			PR_FREEIF(stream);
			PR_FREEIF(obj->in_buff);
			return NULL;
		}
		
		ap_decode_init(&(obj->ap_decode_obj), 
							FALSE,
							TRUE, 
							obj->binhex_stream);		
	}
	else
	{
		obj->binhex_stream = NULL;
		ap_decode_init(&(obj->ap_decode_obj), 
							FALSE,
							FALSE, 
							window_id);
		/*
		 * jt 8/8/97 -- I think this should be set to true. But'
		 * let's not touch it for now.
		 *
		 * obj->ap_decode_obj.is_binary = TRUE;
		 */
	}

	if (dst_filename) 
	{
		RICHIE_XP_STRNCPY_SAFE(obj->ap_decode_obj.fname, dst_filename, 
						sizeof(obj->ap_decode_obj.fname));
	}
	#ifdef RICHIE_XP_MAC		
	obj->ap_decode_obj.mSpec = (FSSpec*)( URL_s->fe_data );
	#endif
    TRACEMSG(("Returning stream from NET_AppleDoubleDecode\n"));

    return stream;
}

/*
**	fe_MakeAppleSingleDecodeStream_1
**	--------------------------------
**	
**		Create the apple single decode stream.
**
**		In the Mac OS, it will create a stream to decode object to an apple file;
**
**		In other OS,  the stream will decode apple single object, 
**					  then encode context in binhex format, and save to the file.
*/

PUBLIC NET_StreamClass * 
fe_MakeAppleSingleDecodeStream_1 (int  format_out,
                         void       *data_obj,
                         URL_Struct *URL_s,
                         MWContext  *window_id)
{
#ifdef RICHIE_XP_MAC
	return fe_MakeAppleSingleDecodeStream(format_out, 
						data_obj, 
						URL_s, 
						window_id, 
						FALSE,
						NULL);
#else

#if 0		/* just a test in the mac OS	*/
	NET_StreamClass *p;
	char*	url; 
	StandardFileReply	reply;
		
	StandardPutFile("\pSave binhex encoded file as:", "\pUntitled", &reply);
	if (!reply.sfGood) 
	{
		return NULL;
	}
	url = my_PathnameFromFSSpec(&(reply.sfFile));
	
	p = fe_MakeAppleSingleDecodeStream(format_out, 
						data_obj, 
						URL_s, 
						window_id, 
						true,
						url+7);
	PR_FREEIF(url);
	return (p);

#else	/* for the none mac-os to get a file name */

	NET_StreamClass *p;
	char* filename;
	char* defaultPath = 0;

	defaultPath = URL_s->content_name;

#ifdef RICHIE_XP_WIN16
	if (RICHIE_XP_FileNameContainsBadChars(defaultPath))
		defaultPath = 0;
#endif
	
	filename = PR_CALLOC(1024);
	if (filename == NULL)
		return NULL;

/***** RICHIE - CONVERT THIS
	if (FE_PromptForFileName(window_id, 
				RICHIE_XP_GetString(MK_MSG_SAVE_ATTACH_AS),
				defaultPath,
				FALSE,
				FALSE,
				simple_copy,
				filename) == -1)
	{
		return	NULL;
	}
*********/
  
	p = fe_MakeAppleSingleDecodeStream(format_out, 
						data_obj, 
						URL_s, 
						window_id, 
						FALSE,
						filename);
	PR_FREEIF(filename);
	return (p);	

#endif

#endif
}

/*
**	Create the Apple Doube Decode stream.
**
*/
PUBLIC NET_StreamClass * 
fe_MakeAppleSingleDecodeStream (int  format_out,
                         void       *data_obj,
                         URL_Struct *URL_s,
                         MWContext  *window_id,
                         PRBool	write_as_binhex,
                         char 		*dst_filename)	
{
    AppleDoubleDecodeObject* obj;
    NET_StreamClass* 	  stream;
    int encoding = kEncodeNone; /* default is that we don't know the encoding */
    
    TRACEMSG(("Setting up apple single decode stream. Have URL: %s\n", URL_s->address));

    stream = RICHIE_XP_NEW(NET_StreamClass);
    if(stream == NULL) 
        return(NULL);

    obj = RICHIE_XP_NEW(AppleDoubleDecodeObject);
    if (obj == NULL)
    {
    	PR_FREEIF(stream);
        return(NULL);
    }
    
    stream->name           = "AppleSingle Decode";
    stream->complete       = (MKStreamCompleteFunc) 	net_AppleDouble_Decode_Complete;
    stream->abort          = (MKStreamAbortFunc) 		net_AppleDouble_Decode_Abort;
    stream->put_block      = (MKStreamWriteFunc) 		net_AppleDouble_Decode_Write;
    stream->is_write_ready = (MKStreamWriteReadyFunc) 	net_AppleDouble_Decode_Ready;
    stream->data_object    = obj;
    stream->window_id      = window_id;

	/*
	** setup all the need information on the apple double encoder.
	*/
	obj->in_buff = (char *)PR_CALLOC(1024);
	if (obj->in_buff == NULL)
	{
		PR_FREEIF(obj);
		PR_FREEIF(stream);
		return (NULL);
	}
	
	obj->bytes_in_buff = 0;
	
	if (write_as_binhex)
	{
		obj->binhex_stream = 
			fe_MakeBinHexEncodeStream(format_out,
							data_obj,
							URL_s,
							window_id,
							dst_filename);
		if (obj->binhex_stream == NULL)
		{
			PR_FREEIF(obj);
			PR_FREEIF(stream);
			PR_FREEIF(obj->in_buff);
			return NULL;
		}
		
		ap_decode_init(&(obj->ap_decode_obj), 
							TRUE,
							TRUE, 
							obj->binhex_stream);		
	}
	else
	{
		obj->binhex_stream = NULL;
		ap_decode_init(&(obj->ap_decode_obj), 
							TRUE,
							FALSE, 
							window_id);
#ifndef RICHIE_XP_MAC
		obj->ap_decode_obj.is_binary = TRUE;
#endif
	}

	if (dst_filename) 
	{
		RICHIE_XP_STRNCPY_SAFE(obj->ap_decode_obj.fname, dst_filename, 
						sizeof(obj->ap_decode_obj.fname));
	}
	/* If we are of a broken content-type, impose its encoding. */
	if (URL_s->content_type 
		&& !RICHIE_XP_STRNCASECMP(URL_s->content_type, "x-uuencode-apple-single", 23))
		obj->ap_decode_obj.encoding = kEncodeUU;
	else
		obj->ap_decode_obj.encoding = kEncodeNone;

    TRACEMSG(("Returning stream from NET_AppleSingleDecode\n"));

    return stream;
}
