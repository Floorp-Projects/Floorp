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
#ifndef __UNIPRIV__
#define __UNIPRIV__

#include "ubase.h"
#include "umap.h"
#include "uconvutil.h"

#ifdef __cplusplus
extern "C" {
#endif

void	uFillInfo(const uTable *uT, 
                    PRUint32 *info);

PRBool	uMapCode(const uTable *uT, 
                    PRUint16 in, 
                    PRUint16* out);

PRBool 	uGenerate(  uShiftTable *shift,
                    PRInt32* state, 
                    PRUint16 in, 
                    unsigned char* out, 
                    PRUint32 outbuflen, 
                    PRUint32* outlen);

PRBool 	uScan(      uShiftTable *shift, 
                    PRInt32 *state, 
                    unsigned char *in,
                    PRUint16 *out, 
                    PRUint32 inbuflen, 
                    PRUint32* inscanlen);

#ifdef __cplusplus
}
#endif

#endif /* __UNIPRIV__ */
