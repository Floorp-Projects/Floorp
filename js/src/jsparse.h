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

#ifndef jsparse_h___
#define jsparse_h___
/*
 * JS parser definitions.
 */
#include "jsprvtd.h"
#include "jspubtd.h"

PR_BEGIN_EXTERN_C

extern JS_FRIEND_API(JSBool)
js_Parse(JSContext *cx, JSObject *slink, JSTokenStream *ts,
	 JSCodeGenerator *cg);

extern JSBool
js_ParseFunctionBody(JSContext *cx, JSTokenStream *ts, JSFunction *fun,
		     JSSymbol *args);

PR_END_EXTERN_C

#endif /* jsparse_h___ */
