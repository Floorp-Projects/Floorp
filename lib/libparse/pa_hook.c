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

#include "pa_parse.h"
#include <stdio.h>
#include "net.h"


#ifdef XP_WIN16
#define	HOLD_BUF_UNIT		32000
#define	SIZE_LIMIT		32000
#else
#define	HOLD_BUF_UNIT		16384
#endif /* XP_WIN16 */

#define WRITE_READY_SIZE	(unsigned int) 2048;


intn
PA_ParseStringToTags(MWContext *context, char *buf, int32 len,
		void *output_func)
{
	intn ret;
	pa_DocData *fake_doc_data;
	NET_StreamClass s;

	fake_doc_data = XP_NEW_ZAP(pa_DocData);
	if (fake_doc_data == NULL)
	{
		return(-1);
	}

	fake_doc_data->doc_id = 100164;
	fake_doc_data->window_id = context;
	fake_doc_data->output_tag = (PA_OutputFunction *)output_func;
	fake_doc_data->hold_buf = XP_ALLOC(HOLD_BUF_UNIT * sizeof(char));
	if (fake_doc_data->hold_buf == NULL)
	{
		XP_DELETE(fake_doc_data);
		return(-1);
	}
	fake_doc_data->hold_size = HOLD_BUF_UNIT;

	fake_doc_data->brute_tag = P_UNKNOWN;
	fake_doc_data->format_out = FO_PRESENT_INLINE;
	fake_doc_data->parser_stream = XP_NEW(NET_StreamClass);
	if (fake_doc_data->parser_stream == NULL)
	{
		XP_FREE(fake_doc_data->hold_buf);
		XP_DELETE(fake_doc_data);
		return(-1);
	}
	/* We don't need most of the fields in the fake stream */
	fake_doc_data->parser_stream->complete = PA_MDLComplete;
	fake_doc_data->parser_stream->data_object = (void *)fake_doc_data;
	fake_doc_data->is_inline_stream = TRUE;

	s.data_object=fake_doc_data;

	ret = PA_ParseBlock(&s, (const char *)buf, (int)len);
	if (ret > 0)
	{
		PA_MDLComplete(&s);
	}

	return(ret);
}


const char *
PA_TagString(int32 tag_type)
{
	return pa_PrintTagToken(tag_type);
}


int32
PA_TagIndex(char *str)
{
	return((int32)pa_tokenize_tag(str));
}

