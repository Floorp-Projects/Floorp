/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _nsMsgEncoders_H_
#define _nsMsgEncoders_H_

extern "C" MimeEncoderData *
MIME_B64EncoderInit(int (*output_fn) (const char *buf, PRInt32 size, void *closure), void *closure); 

extern "C" MimeEncoderData *	
MIME_QPEncoderInit(int (*output_fn) (const char *buf, PRInt32 size, void *closure), void *closure);

extern "C" MimeEncoderData *	
MIME_UUEncoderInit(char *filename, int (*output_fn) (const char *buf, PRInt32 size, void *closure), void *closure);

extern "C" nsresult
MIME_EncoderDestroy(MimeEncoderData *data, PRBool abort_p);


#endif /* _nsMsgEncoders_H_ */
