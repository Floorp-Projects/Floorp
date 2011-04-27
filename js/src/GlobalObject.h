/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
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
 * The Original Code is SpiderMonkey global object code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jeff Walden <jwalden+code@mit.edu> (original author)
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

#ifndef GlobalObject_h___
#define GlobalObject_h___

#include "jsfun.h"

extern JSObject *
js_InitFunctionAndObjectClasses(JSContext *cx, JSObject *obj);

namespace js {

/*
 * Global object slots are reserved as follows:
 *
 * [0, JSProto_LIMIT)
 *   Stores the original value of the constructor for the corresponding
 *   JSProtoKey.
 * [JSProto_LIMIT, 2 * JSProto_LIMIT)
 *   Stores the prototype, if any, for the constructor for the corresponding
 *   JSProtoKey offset from JSProto_LIMIT.
 * [2 * JSProto_LIMIT, 3 * JSProto_LIMIT)
 *   Stores the current value of the global property named for the JSProtoKey
 *   for the corresponding JSProtoKey offset from 2 * JSProto_LIMIT.
 * [3 * JSProto_LIMIT, RESERVED_SLOTS)
 *   Various one-off values: ES5 13.2.3's [[ThrowTypeError]], RegExp statics,
 *   the Namespace object for E4X's function::, the original eval for this
 *   global object (implementing |var eval = otherWindow.eval; eval(...)| as an
 *   indirect eval), a bit indicating whether this object has been cleared
 *   (see JS_ClearScope), and a cache for whether eval is allowed (per the
 *   global's Content Security Policy).
 *
 * The first two ranges are necessary to implement js::FindClassObject,
 * js::FindClassPrototype, and spec language speaking in terms of "the original
 * Array prototype object", or "as if by the expression new Array()" referring
 * to the original Array constructor.  The third range stores the (writable and
 * even deletable) Object, Array, &c. properties (although a slot won't be used
 * again if its property is deleted and readded).
 */
class GlobalObject : public ::JSObject {
    /*
     * Count of slots to store built-in constructors, prototypes, and initial
     * visible properties for the constructors.
     */
    static const uintN STANDARD_CLASS_SLOTS  = JSProto_LIMIT * 3;

    /* One-off properties stored after slots for built-ins. */
    static const uintN THROWTYPEERROR        = STANDARD_CLASS_SLOTS;
    static const uintN REGEXP_STATICS        = THROWTYPEERROR + 1;
    static const uintN FUNCTION_NS           = REGEXP_STATICS + 1;
    static const uintN EVAL_ALLOWED          = FUNCTION_NS + 1;
    static const uintN EVAL                  = EVAL_ALLOWED + 1;
    static const uintN FLAGS                 = EVAL + 1;

    /* Total reserved-slot count for global objects. */
    static const uintN RESERVED_SLOTS = FLAGS + 1;

    void staticAsserts() {
        /*
         * The slot count must be in the public API for JSCLASS_GLOBAL_FLAGS,
         * and we aren't going to expose GlobalObject, so just assert that the
         * two values are synchronized.
         */
        JS_STATIC_ASSERT(JSCLASS_GLOBAL_SLOT_COUNT == RESERVED_SLOTS);
    }

    static const int32 FLAGS_CLEARED = 0x1;

    void setFlags(int32 flags) {
        setSlot(FLAGS, Int32Value(flags));
    }

  public:
    static GlobalObject *create(JSContext *cx, Class *clasp);

    void setThrowTypeError(JSFunction *fun) {
        Value &v = getSlotRef(THROWTYPEERROR);
        // Our bootstrapping code is currently too convoluted to correctly and
        // confidently assert this.
        // JS_ASSERT(v.isUndefined());
        v.setObject(*fun);
    }

    JSObject *getThrowTypeError() const {
        return &getSlot(THROWTYPEERROR).toObject();
    }

    Value getRegExpStatics() const {
        return getSlot(REGEXP_STATICS);
    }

    void clear(JSContext *cx);

    bool isCleared() const {
        return getSlot(FLAGS).toInt32() & FLAGS_CLEARED;
    }

    bool isEvalAllowed(JSContext *cx);

    const Value &getOriginalEval() const {
        return getSlot(EVAL);
    }

    void setOriginalEval(JSObject *evalobj) {
        Value &v = getSlotRef(EVAL);
        // Our bootstrapping code is currently too convoluted to correctly and
        // confidently assert this.
        // JS_ASSERT(v.isUndefined());
        v.setObject(*evalobj);
    }

    bool getFunctionNamespace(JSContext *cx, Value *vp);

    bool initStandardClasses(JSContext *cx);
};

} // namespace js

js::GlobalObject *
JSObject::asGlobal()
{
    JS_ASSERT(isGlobal());
    return reinterpret_cast<js::GlobalObject *>(this);
}

#endif /* GlobalObject_h___ */
