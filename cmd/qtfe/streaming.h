/* $Id: streaming.h,v 1.1 1998/09/25 18:01:41 ramiro%netscape.com Exp $
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
 * Copyright (C) 1998 Netscape Communications Corporation.  Portions
 * created by Warwick Allison, Kalle Dalheimer, Eirik Eng, Matthias
 * Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav Tvete are
 * Copyright (C) 1998 Warwick Allison, Kalle Dalheimer, Eirik Eng,
 * Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav
 * Tvete.  All Rights Reserved.
 */

#ifndef _STREAMING_H
#define _STREAMING_H

#include <fe_proto.h>

class QtContext;

struct save_as_data
{
  MWContext *context;
  char *name;
  FILE *file;
  int type;
  int insert_base_tag;
  bool use_dialog_p;
  void (*done)(struct save_as_data *);
  void* data;
  int content_length;
  int bytes_read;
  URL_Struct *url;
};

NET_StreamClass *
fe_MakeSaveAsStream ( int /*format_out*/, void * /*data_obj*/,
		      URL_Struct *url_struct, MWContext *context );

NET_StreamClass *
fe_MakeSaveAsStreamNoPrompt( int format_out, void *data_obj,
			     URL_Struct *url_struct, MWContext *context );

struct save_as_data *
make_save_as_data (MWContext *context, bool allow_conversion_p,
		   int type, URL_Struct *url, const char *output_file);

void
fe_save_as_stream_abort_method (NET_StreamClass *stream, int /*status*/);

void
fe_save_as_stream_complete_method (NET_StreamClass *stream);

unsigned int
fe_save_as_stream_write_ready_method (NET_StreamClass *stream);

int
fe_save_as_stream_write_method (NET_StreamClass *stream, const char
				*str, int32 len);  

void fe_save_as_nastiness( QtContext *context, URL_Struct *url,
			   struct save_as_data *sad, int synchronous );

#endif
