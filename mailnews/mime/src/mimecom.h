/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/* 
 * XP-COM Bridges for C function calls
 */
#ifndef _MIMECOM_H_
#define _MIMECOM_H_

#include "prtypes.h"

/*
 * These functions are exposed by libmime to be used by content type
 * handler plugins for processing stream data. 
 */
/*
 * This is the write call for outputting processed stream data.
 */ 
extern "C" int  XPCOM_MimeObject_write(void *mimeObject, char *data, 
                                  PRInt32 length, 
                                  PRBool user_visible_p);
/*
 * The following group of calls expose the pointers for the object
 * system within libmime. 
 */                                                        
extern "C" void *XPCOM_GetmimeInlineTextClass(void);
extern "C" void *XPCOM_GetmimeLeafClass(void);
extern "C" void *XPCOM_GetmimeObjectClass(void);
extern "C" void *XPCOM_GetmimeContainerClass(void);
extern "C" void *XPCOM_GetmimeMultipartClass(void);
extern "C" void *XPCOM_GetmimeMultipartSignedClass(void);

#endif /* _MIMECOM_H_ */
