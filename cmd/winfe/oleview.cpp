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

/********************************XXXM12N**************************************/
#include "net.h"
#include "merrors.h"
#include "xp_mem.h"
#include "xp_mcom.h"
#include "xpassert.h"

static unsigned int
ole_view_write_ready(NET_StreamClass *stream)
{	
	return 1;
}

/* Abort the stream if we get this far */
static int ole_view_write(NET_StreamClass *stream, const  char *str, int32 len)
{	
	return MK_INTERRUPTED;
}

static void
ole_view_complete(NET_StreamClass *stream)
{	
}

static void 
ole_view_abort(NET_StreamClass *stream, int status)
{	
}


static char fakehtml[] = "<EMBED SRC=\"internal-ole-viewer:%s\"\n>";

NET_StreamClass *
OLE_ViewStream(int format_out, void *pDataObj, URL_Struct *urls,
              MWContext *pContext)
{
    NET_StreamClass *stream = nil, *viewstream;
	char *org_content_type;
    
    if (!(stream = XP_NEW_ZAP(NET_StreamClass))) {
		XP_TRACE(("OLE_ViewStream memory lossage"));
		return 0;
	}
	
    stream->name           = "ole viewer";
    stream->complete       = ole_view_complete;
    stream->abort          = ole_view_abort;
    stream->is_write_ready = ole_view_write_ready;
    stream->data_object    = NULL;
    stream->window_id      = pContext;
    stream->put_block      = (MKStreamWriteFunc)ole_view_write;

	org_content_type = urls->content_type; 
	urls->content_type = 0;
	StrAllocCopy(urls->content_type, TEXT_HTML);
	urls->is_binary = 1;	/* secret flag for mail-to save as */

	if((viewstream=NET_StreamBuilder(format_out, urls, pContext)) != 0)
	{
        char *buffer = (char*)
            XP_ALLOC(XP_STRLEN(fakehtml) + XP_STRLEN(urls->address) + 1);

        if (buffer)
        {
            XP_SPRINTF(buffer, fakehtml, urls->address);
            (*viewstream->put_block)(viewstream,
                                     buffer, XP_STRLEN(buffer));
            (*viewstream->complete)(viewstream);
            XP_FREE(buffer);
        }
	}

	/* XXX hack alert - this has to be set back for abort to work
       correctly */
	XP_FREE(urls->content_type);
	urls->content_type = org_content_type;

    return stream;
}

