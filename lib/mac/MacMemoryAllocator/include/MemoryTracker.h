/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#if DEBUG_MAC_MEMORY
#include "xp_tracker.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Some mac specific routines and constants
 */
 
#define	kNameMaxLength			30


/* allocType */
typedef enum {
	kMallocBlock = 'mllc',
	kHandleBlock = 'hndl',
	kPointerBlock = 'pntr'
} AllocatorType;

/*
 * Stack Crawl Cheese
 */

void *GetCurrentStackPointer();

/* walk past any 68K stack frames and try to get a native only trace */
void GetCurrentNativeStackTrace(void **stackCrawl);

/* get a stack crawl from the current stack frame */
void GetCurrentStackTrace(void **stackCrawl);


#ifdef __cplusplus
}
#endif
