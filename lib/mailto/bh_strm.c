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
**
**	 Binhex_stream.c
**	 ---------------
**
**		The code for the binhex encode/decode stream.
**
**		20sep95		mym		Created.
**
*/

#include "msg.h"
#include "appledbl.h"
#include "m_binhex.h"
#include "m_cvstrm.h"
#include "ad_codes.h"

#ifdef XP_MAC
#pragma warn_unusedarg off
#endif /* XP_MAC */

/*
**	-----------------------------------------------------------
**
**		The BinHex encode writer stream.
**
**	-----------------------------------------------------------
*/
extern int MK_MIME_ERROR_WRITING_FILE;

#define WORKING_BUFF_SIZE	8192

typedef struct 
{
	binhex_encode_object	bh_encode_obj;

	char 	*buff;					/* the working buff							  */
	int32	s_buff;					/* the size of workiong buff.				  */			

	XP_File	fp;
	char	*fname;					/* the filename for file holding the encoding */

} BinHexEncodeObject ;


/*
**	Let's go "l" characters forward of the encoding for this write.
**	Note:	
**		"s" is just a dummy paramter. 
*/
PRIVATE int net_BinHex_Encode_Write (void *dataObject, const char* s, int32 l)
{
	int  	status = 0;
	BinHexEncodeObject * obj = (BinHexEncodeObject*)dataObject;
	int32 	count;
	int32	size;	
	
	do	
	{
		size = obj->s_buff * 11 / 16;
		size = MIN(l, size);
		status = binhex_encode_next(&(obj->bh_encode_obj), 
							(char*)s,
							size,
							obj->buff, 
							obj->s_buff, 
							&count);
		if (status == NOERR || status == errDone)
		{
			/*
			 * we get the encode data, so call the next stream to write it to the disk.
			 */
			if ((int) XP_FileWrite(obj->buff, count, obj->fp) != count )
				return errFileWrite;
		}
		
		if (status < 0) 			/* abort */
			break;
	
		l -= size;
		s += size;
		
	} while (l > 0);
	
	return status;
}

PRIVATE unsigned int net_BinHex_Encode_Ready (NET_StreamClass *dataObject)
{	
   return(MAX_WRITE_READY);  					/* always ready for writing 		*/ 
}


PRIVATE void net_BinHex_Encode_Complete (void *dataObject)
{
	int32	count, len = 0;
	BinHexEncodeObject *obj = (BinHexEncodeObject *)dataObject;	
	
	/*
	**	push the close part.
	*/
	len = binhex_encode_next(&(obj->bh_encode_obj), 
							NULL,
							0,
							obj->buff, 
							obj->s_buff, 
							&count);		/* this help us generate the finishing */
								
	len = XP_FileWrite(obj->buff, count, obj->fp);
	
	/*
	**	time to do some dirty work -- fix the real file size. 
	**			(since we can only know by now)
	*/
		
	binhex_reencode_head(&(obj->bh_encode_obj), 
							obj->buff, 
							obj->s_buff, 
							&count);			/* get the head part encoded again 	*/
	
	XP_FileSeek(obj->fp, 0L, SEEK_SET);			/* and override the previous dummy	*/
	XP_FileWrite(obj->buff, count, obj->fp);

    binhex_encode_end(&(obj->bh_encode_obj), FALSE);	/* now we get a real ending */
 	
 	if (obj->fp)
 	{
    	XP_FileClose(obj->fp);					/* we are done with the target file	*/
        FREEIF(obj->fname);						/* free the space for the file name */
    }
								
   	FREEIF(obj->buff);							/* and free the working buff.	*/
    XP_FREE(obj);
}

PRIVATE void net_BinHex_Encode_Abort (void *dataObject, int status)
{
	BinHexEncodeObject * obj = (BinHexEncodeObject*)dataObject;	
	
    binhex_encode_end(&(obj->bh_encode_obj), TRUE);	/* it is an aborting exist... 	*/

 	if (obj->fp)
 	{
    	XP_FileClose(obj->fp);					/* we are aboring with the decoding	*/
        XP_FileRemove(obj->fname, xpURL);	
        										/* remove the incomplete file.		*/
        FREEIF (obj->fname);					/* free the space for the file name */
	}
	
    FREEIF(obj->buff);							/* free the working buff.			*/
    XP_FREE(obj);
}


/*
**	Will create a apple double encode stream:
**
**		-> take the filename as the input source (it needs to be a mac file.)
**		-> tkae a stream to take care of the writing to a temp file.
*/
PUBLIC NET_StreamClass * 
fe_MakeBinHexEncodeStream (int  format_out,
                         void       *data_obj,
                         URL_Struct *URL_s,
                         MWContext  *window_id,
                         char*		dst_filename )
{
    BinHexEncodeObject* obj;
    NET_StreamClass* 	stream;
    char*				working_buff = NULL;
	int					bSize = WORKING_BUFF_SIZE;
    
    TRACEMSG(("Setting up apple encode stream. Have URL: %s\n", URL_s->address));

    stream = XP_NEW(NET_StreamClass);
    if(stream == NULL) 
        return(NULL);

    obj = XP_NEW(BinHexEncodeObject);
    if (obj == NULL) 
    {
        XP_FREE (stream);
        return(NULL);
    }
   
	while (!working_buff && (bSize >= 512))
	{
   		working_buff = (char *)XP_ALLOC(bSize);
		if (!working_buff)
			bSize /= 2;
	}
   	if (working_buff == NULL)
   	{
   		XP_FREE (obj);
   		XP_FREE (stream);
		return (NULL);
   	}
   	
    stream->name           = "BinHex Encode";
    stream->complete       = (MKStreamCompleteFunc) 	net_BinHex_Encode_Complete;
    stream->abort          = (MKStreamAbortFunc) 		net_BinHex_Encode_Abort;
    stream->put_block      = (MKStreamWriteFunc) 		net_BinHex_Encode_Write;
    stream->is_write_ready = (MKStreamWriteReadyFunc) 	net_BinHex_Encode_Ready;
    stream->data_object    = obj;  							/* document info object */
    stream->window_id      = window_id;

	obj->fname = XP_STRDUP(dst_filename);	
	obj->fp  =  XP_FileOpen(obj->fname, xpURL, XP_FILE_TRUNCATE);		
								/* this file will hold all the encoded data			*/	
	if (obj->fp == NULL)
	{
		XP_FREE(working_buff);	/* if we can't open the target file, roll back then */ 
		if(obj->fname) XP_FREE(obj->fname);
   		XP_FREE (obj);
   		XP_FREE (stream);
   		return (NULL);
	}
	
	obj->buff	= working_buff;
	obj->s_buff	= WORKING_BUFF_SIZE;
	
	/*
	** 	setup all the need information on the apple double encoder.
	*/
	binhex_encode_init(&(obj->bh_encode_obj));		/* pass the file name of the source.*/

    TRACEMSG(("Returning stream from NET_BinHexEncoder\n"));

    return stream;
}

/*
**	-----------------------------------------------------------
**
**		The BinHex decode writer stream.
**
**	-----------------------------------------------------------
*/

typedef	struct BinHexDecodeObject
{
	binhex_decode_object	bh_decode_obj;
	
	char*	in_buff;
	int32	bytes_in_buff;
	
} BinHexDecodeObject;


PRIVATE int 
net_BinHex_Decode_Write (
	void *dataObject, const char* s, int32 l)
{
	int		status = NOERR;
	BinHexDecodeObject * obj = (BinHexDecodeObject*)dataObject;
	int32 	size;	
	
	if (obj->bytes_in_buff + l > 1024)
	{
		size = 1024 - obj->bytes_in_buff;
		XP_MEMCPY(obj->in_buff+obj->bytes_in_buff, 
					s, 
					size);
		s += size;
		l -= size;
		
		status = binhex_decode_next(&(obj->bh_decode_obj), 
					obj->in_buff, 
					1024);
		if (status != NOERR)
			return status;
							
		obj->bytes_in_buff = 0;
	}
	
	if (l > 1024)		
	{
		/* we are sure that obj->bytes_in_buff == 0 at this point. */
		status = binhex_decode_next(&(obj->bh_decode_obj), 
					s, 
					l);
	}
	else
	{
		XP_MEMCPY(obj->in_buff+obj->bytes_in_buff, 
					s, 
					l);
		obj->bytes_in_buff += l;
	}
	return status;
}

/*
 * is the stream ready for writeing?
 */
PRIVATE unsigned int net_BinHex_Decode_Ready (void *stream)
{	
   return(MAX_WRITE_READY); 	 						/* always ready for writing */ 
}


PRIVATE void net_BinHex_Decode_Complete (void *dataObject)
{
	BinHexDecodeObject *obj = (BinHexDecodeObject *)	dataObject;	

	if (obj->bytes_in_buff)
	{
		/* do the last calls.	*/
		binhex_decode_next(&(obj->bh_decode_obj), 
				(char *)obj->in_buff,
				obj->bytes_in_buff);
		obj->bytes_in_buff = 0;
	}
	
    binhex_decode_end(&(obj->bh_decode_obj), FALSE);	/* it is a normal clean up classes.	*/

	if (obj->in_buff)
		XP_FREE(obj->in_buff);

	XP_FREE(obj);
}

PRIVATE void net_BinHex_Decode_Abort (void *dataObject, int status)
{
	BinHexDecodeObject *obj = (BinHexDecodeObject *)dataObject;

	binhex_decode_end(&(obj->bh_decode_obj), TRUE);		/* it is an abort. 	*/
	
	if (obj->in_buff)
		XP_FREE(obj->in_buff);
		
	XP_FREE(obj);
}

/*
**	Create the bin hex decode stream.
**
*/
PUBLIC NET_StreamClass * 
fe_MakeBinHexDecodeStream (int  format_out,
                         void       *data_obj,
                         URL_Struct *URL_s,
                         MWContext  *window_id )
{
    BinHexDecodeObject* obj;
    NET_StreamClass* 	stream;
    
    TRACEMSG(("Setting up bin hex decode stream. Have URL: %s\n", URL_s->address));

    stream = XP_NEW(NET_StreamClass);
    if(stream == NULL) 
        return(NULL);

    obj = XP_NEW(BinHexDecodeObject);
    if (obj == NULL)
    {
    	XP_FREE(stream);
        return(NULL);
    }
    
 	if ((obj->in_buff = (char *)XP_ALLOC(1024)) == NULL)
 	{
 		XP_FREE(obj);
 		XP_FREE(stream);
 		return (NULL);
   	}
   	
    stream->name           = "BinHex Decoder";
    stream->complete       = (MKStreamCompleteFunc) 	net_BinHex_Decode_Complete;
    stream->abort          = (MKStreamAbortFunc) 		net_BinHex_Decode_Abort;
    stream->put_block      = (MKStreamWriteFunc) 		net_BinHex_Decode_Write;
    stream->is_write_ready = (MKStreamWriteReadyFunc) 	net_BinHex_Decode_Ready;
    stream->data_object    = obj;
    stream->window_id      = window_id;

	/*
	**	Some initial to the object.
	*/
	obj->bytes_in_buff	   = 0;	
	
	/*
	** setup all the need information on the apple double encoder.
	*/
	binhex_decode_init(&(obj->bh_decode_obj),window_id);
	#ifdef XP_MAC
	obj->bh_decode_obj.mSpec = (FSSpec*)( URL_s->fe_data );
    #endif
    TRACEMSG(("Returning stream from NET_BinHexDecode\n"));

    return stream;
}


