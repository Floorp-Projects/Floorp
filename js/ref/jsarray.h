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

#ifndef jsarray_h___
#define jsarray_h___
/*
 * JS Array interface.
 */
#include "jsprvtd.h"
#include "jspubtd.h"

PR_BEGIN_EXTERN_C

extern JSClass js_ArrayClass;

extern JSObject *
js_InitArrayClass(JSContext *cx, JSObject *obj);

extern JSObject *
js_NewArrayObject(JSContext *cx, jsuint length, jsval *vector);

extern JSBool
js_GetLengthProperty(JSContext *cx, JSObject *obj, jsuint *lengthp);

extern JSBool
js_SetLengthProperty(JSContext *cx, JSObject *obj, jsuint length);

extern JSBool
js_HasLengthProperty(JSContext *cx, JSObject *obj, jsuint *lengthp);

/*
 * JS-specific qsort function.
 */
typedef int (*JSComparator)(const void *a, const void *b, void *arg);

extern PRBool
js_qsort(void *vec, size_t nel, size_t elsize, JSComparator cmp, void *arg);

PR_END_EXTERN_C

#endif /* jsarray_h___ */
