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
 
#ifndef _MIMEBUF_H_
#define _MIMEBUF_H_

/*	Very similar to strdup except it free's too
 */
extern "C" char * 
mime_SACopy (char **destination, const char *source);

extern "C"  char *
mime_SACat (char **destination, const char *source);

extern "C" int mime_GrowBuffer (PRUint32 desired_size,
						   PRUint32 element_size, PRUint32 quantum,
						   char **buffer, PRInt32 *size);

extern "C" int mime_LineBuffer (const char *net_buffer, PRInt32 net_buffer_size,
						   char **bufferP, PRInt32 *buffer_sizeP,
						   PRInt32 *buffer_fpP,
						   PRBool convert_newlines_p,
						   PRInt32 (*per_line_fn) (char *line, PRInt32
												 line_length, void *closure),
						   void *closure);
						   
extern "C" int mime_ReBuffer (const char *net_buffer, PRInt32 net_buffer_size,
						 PRUint32 desired_buffer_size,
						 char **bufferP, PRUint32 *buffer_sizeP,
						 PRUint32 *buffer_fpP,
						 PRInt32 (*per_buffer_fn) (char *buffer,
												 PRUint32 buffer_size,
												 void *closure),
						 void *closure);


#endif /* _MIMEBUF_H_ */