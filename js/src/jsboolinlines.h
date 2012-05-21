/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsboolinlines_h___
#define jsboolinlines_h___

#include "jsobjinlines.h"

#include "vm/BooleanObject-inl.h"

namespace js {

inline bool
BooleanGetPrimitiveValue(JSContext *cx, JSObject &obj, Value *vp)
{
    if (obj.isBoolean()) {
        *vp = BooleanValue(obj.asBoolean().unbox());
        return true;
    }

    extern bool BooleanGetPrimitiveValueSlow(JSContext *, JSObject &, Value *);
    return BooleanGetPrimitiveValueSlow(cx, obj, vp);
}

} /* namespace js */

#endif /* jsboolinlines_h___ */
