/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef jsregexp_h___
#define jsregexp_h___
/*
 * JS regular expression interface.
 */
#include <stddef.h>
#include "jspubtd.h"
#include "jsstr.h"

#ifdef JS_THREADSAFE
#include "jsdhash.h"
#endif

/* Forward declarations. */
namespace js {
class RegExp;
class RegExpStatics;
class AutoValueRooter;
}

JS_BEGIN_EXTERN_C

extern JSClass js_RegExpClass;

static inline bool
VALUE_IS_REGEXP(JSContext *cx, jsval v)
{
    return !JSVAL_IS_PRIMITIVE(v) && JSVAL_TO_OBJECT(v)->isRegExp();
}

inline bool
JSObject::isRegExp() const
{
    return getClass() == &js_RegExpClass;
}

extern JS_FRIEND_API(JSBool)
js_ObjectIsRegExp(JSObject *obj);

extern JSObject *
js_InitRegExpClass(JSContext *cx, JSObject *obj);

/*
 * Export js_regexp_toString to the decompiler.
 */
extern JSBool
js_regexp_toString(JSContext *cx, JSObject *obj, jsval *vp);

extern JS_FRIEND_API(JSObject *) JS_FASTCALL
js_CloneRegExpObject(JSContext *cx, JSObject *obj, JSObject *proto);

/*
 * Move data from |cx|'s regexp statics to |statics| and root the input string in |tvr| if it's
 * available.
 */
extern JS_FRIEND_API(void)
js_SaveAndClearRegExpStatics(JSContext *cx, js::RegExpStatics *statics, js::AutoValueRooter *tvr);

/* Move the data from |statics| into |cx|. */
extern JS_FRIEND_API(void)
js_RestoreRegExpStatics(JSContext *cx, js::RegExpStatics *statics, js::AutoValueRooter *tvr);

extern JSBool
js_XDRRegExpObject(JSXDRState *xdr, JSObject **objp);

JS_END_EXTERN_C

#endif /* jsregexp_h___ */
