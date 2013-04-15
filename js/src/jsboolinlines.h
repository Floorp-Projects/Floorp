/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsboolinlines_h___
#define jsboolinlines_h___

#include "mozilla/Assertions.h"
#include "mozilla/Likely.h"

#include "js/RootingAPI.h"

#include "jsobjinlines.h"

#include "vm/BooleanObject-inl.h"

namespace js {

bool BooleanGetPrimitiveValueSlow(JSContext *, HandleObject, Value *);

inline bool
BooleanGetPrimitiveValue(JSContext *cx, HandleObject obj, Value *vp)
{
    if (obj->isBoolean()) {
        *vp = BooleanValue(obj->asBoolean().unbox());
        return true;
    }

    return BooleanGetPrimitiveValueSlow(cx, obj, vp);
}

inline bool
EmulatesUndefined(RawObject obj)
{
    RawObject actual = MOZ_LIKELY(!obj->isWrapper()) ? obj : UncheckedUnwrap(obj);
    bool emulatesUndefined = actual->getClass()->emulatesUndefined();
    MOZ_ASSERT_IF(emulatesUndefined, obj->type()->flags & types::OBJECT_FLAG_EMULATES_UNDEFINED);
    return emulatesUndefined;
}

} /* namespace js */

#endif /* jsboolinlines_h___ */
