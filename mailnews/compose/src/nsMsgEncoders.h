/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef _nsMsgEncoders_H_
#define _nsMsgEncoders_H_

extern "C" MimeEncoderData *
MIME_B64EncoderInit(nsresult (*output_fn) (const char *buf, PRInt32 size, void *closure), void *closure); 

extern "C" MimeEncoderData *	
MIME_QPEncoderInit(nsresult (*output_fn) (const char *buf, PRInt32 size, void *closure), void *closure);

extern "C" MimeEncoderData *	
MIME_UUEncoderInit(char *filename, nsresult (*output_fn) (const char *buf, PRInt32 size, void *closure), void *closure);

extern "C" nsresult
MIME_EncoderDestroy(MimeEncoderData *data, PRBool abort_p);


#endif /* _nsMsgEncoders_H_ */
