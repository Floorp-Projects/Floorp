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
 * XP-COM Bridges for C function calls
 */
#ifndef _MIMECOM_H_
#define _MIMECOM_H_

/*
 * These functions are exposed by libmime to be used by content type
 * handler plugins for processing stream data. 
 */
/*
 * This is the write call for outputting processed stream data.
 */ 
extern int  XPCOM_MimeObject_write(void *mimeObject, char *data, 
                                  PRInt32 length, 
                                  PRBool user_visible_p);
/*
 * The following group of calls expose the pointers for the object
 * system within libmime. 
 */                                                        
extern void *XPCOM_GetmimeInlineTextClass(void);
extern void *XPCOM_GetmimeLeafClass(void);
extern void *XPCOM_GetmimeObjectClass(void);
extern void *XPCOM_GetmimeContainerClass(void);
extern void *XPCOM_GetmimeMultipartClass(void);
extern void *XPCOM_GetmimeMultipartSignedClass(void);

#endif /* _MIMECOM_H_ */
