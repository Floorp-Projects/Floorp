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
#ifndef __UNIPRIV__
#define __UNIPRIV__

#include "ubase.h"
#include "umap.h"
#include "uconvutil.h"

#ifdef __cplusplus
extern "C" {
#endif

PRBool	uMapCode(   uTable *uT, 
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
