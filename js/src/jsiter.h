/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=80:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1 *
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

#ifndef jsiter_h___
#define jsiter_h___
/*
 * JavaScript iterators.
 */
#include "jsprvtd.h"
#include "jspubtd.h"

#define JSITER_FOREACH  0x1     /* return [key, value] pair rather than key */
#define JSITER_COMPAT   0x2     /* compatibility flag for old XDR'd bytecode */
#define JSITER_HIDDEN   0x4     /* internal iterator hidden from user view */

extern JSBool
js_NewNativeIterator(JSContext *cx, JSObject *obj, uintN flags, jsval *vp);

extern uintN
js_GetNativeIteratorFlags(JSContext *cx, JSObject *iterobj);

extern void
js_FinishNativeIterator(JSContext *cx, JSObject *iterobj);

extern JSBool
js_DefaultIterator(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                   jsval *rval);

extern JSObject *
js_ValueToIterator(JSContext *cx, jsval v, uintN flags);

/*
 * Given iterobj, call iterobj.next().
 *
 * If idp is non-null, we are enumerating an iterable other than iterobj using
 * a for-in or for-each-in loop, so we must return the id of the property being
 * enumerated for shadowing and deleted-property checks.  In this case, *rval
 * will be either the key or the value from a [key, value] pair returned by the
 * iterator, depending on whether for-in or for-each-in is being used.
 *
 * But if idp is null, then we are iterating iterobj itself -- the iterator is
 * the right operand of 'in' in the for-in or for-each-in loop -- and we should
 * pass its return value back unchanged.
 */
extern JSBool
js_CallIteratorNext(JSContext *cx, JSObject *iterobj, uintN flags,
                    jsid *idp, jsval *rval);

#define VALUE_IS_STOP_ITERATION(cx,v)                                         \
    (!JSVAL_IS_PRIMITIVE(v) &&                                                \
     OBJ_GET_CLASS(cx, JSVAL_TO_OBJECT(v)) == &js_StopIterationClass)

extern JSBool
js_ThrowStopIteration(JSContext *cx, JSObject *obj);

extern JSObject *
js_NewGenerator(JSContext *cx, JSStackFrame *fp);

extern JSClass js_GeneratorClass;
extern JSClass js_IteratorClass;
extern JSClass js_StopIterationClass;

extern JSObject *
js_InitIteratorClasses(JSContext *cx, JSObject *obj);

#endif /* jsiter_h___ */
