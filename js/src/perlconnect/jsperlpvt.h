/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "EXTERN.h"
#include "perl.h"
#include "jsperl.h"
/* Copyright © 1998 Netscape Communications Corporation, All Rights Reserved.*/
/* This is the private header, which means that it shouldn't be included     */
/* unless you need to use some of the jsval<->SV* conversion functions       */
/* provided by PerlConnect needs to include to enable the Perl object. See   */
/* README.html for more documentation                                        */

/*
    This and the following function are used to convert
    between Perl's "SV*" and JS's "jsval" types.
*/
JS_EXTERN_API(SV*)
JSVALToSV(JSContext *cx, JSObject *obj, jsval v, SV** sv);

JS_EXTERN_API(JSBool)
SVToJSVAL(JSContext *cx, JSObject *obj, SV *ref, jsval *rval);
