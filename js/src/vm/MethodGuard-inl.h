/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
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
 * The Original Code is SpiderMonkey method-guard code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2012
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

#ifndef MethodGuard_inl_h___
#define MethodGuard_inl_h___

#include "jsobj.h"

#include "MethodGuard.h"

#include "jsobjinlines.h"

#include "BooleanObject-inl.h"
#include "NumberObject-inl.h"
#include "StringObject-inl.h"

namespace js {

namespace detail {

template<typename T> class PrimitiveBehavior { };

template<>
class PrimitiveBehavior<bool> {
  public:
    static inline bool isType(const Value &v) { return v.isBoolean(); }
    static inline bool extract(const Value &v) { return v.toBoolean(); }
    static inline bool extract(JSObject &obj) { return obj.asBoolean().unbox(); }
    static inline Class *getClass() { return &BooleanClass; }
};

template<>
class PrimitiveBehavior<double> {
  public:
    static inline bool isType(const Value &v) { return v.isNumber(); }
    static inline double extract(const Value &v) { return v.toNumber(); }
    static inline double extract(JSObject &obj) { return obj.asNumber().unbox(); }
    static inline Class *getClass() { return &NumberClass; }
};

template<>
class PrimitiveBehavior<JSString *> {
  public:
    static inline bool isType(const Value &v) { return v.isString(); }
    static inline JSString *extract(const Value &v) { return v.toString(); }
    static inline JSString *extract(JSObject &obj) { return obj.asString().unbox(); }
    static inline Class *getClass() { return &StringClass; }
};

} /* namespace detail */

inline JSObject *
NonGenericMethodGuard(JSContext *cx, CallArgs args, Native native, Class *clasp, bool *ok)
{
    const Value &thisv = args.thisv();
    if (thisv.isObject()) {
        JSObject &obj = thisv.toObject();
        if (obj.getClass() == clasp) {
            *ok = true;  /* quell gcc overwarning */
            return &obj;
        }
    }

    *ok = HandleNonGenericMethodClassMismatch(cx, args, native, clasp);
    return NULL;
}

template <typename T>
inline bool
BoxedPrimitiveMethodGuard(JSContext *cx, CallArgs args, Native native, T *v, bool *ok)
{
    typedef detail::PrimitiveBehavior<T> Behavior;

    const Value &thisv = args.thisv();
    if (Behavior::isType(thisv)) {
        *v = Behavior::extract(thisv);
        return true;
    }

    if (!NonGenericMethodGuard(cx, args, native, Behavior::getClass(), ok))
        return false;

    *v = Behavior::extract(thisv.toObject());
    return true;
}

} /* namespace js */

#endif /* MethodGuard_inl_h___ */
