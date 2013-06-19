/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsboolinlines_h___
#define jsboolinlines_h___

#include "mozilla/Assertions.h"
#include "mozilla/Likely.h"

#include "js/RootingAPI.h"

#include "vm/BooleanObject-inl.h"

namespace js {

bool
BooleanGetPrimitiveValueSlow(HandleObject, JSContext *);

inline bool
BooleanGetPrimitiveValue(HandleObject obj, JSContext *cx)
{
    if (obj->isBoolean())
        return obj->asBoolean().unbox();

    return BooleanGetPrimitiveValueSlow(obj, cx);
}

inline bool
EmulatesUndefined(JSObject *obj)
{
    JSObject *actual = MOZ_LIKELY(!obj->isWrapper()) ? obj : UncheckedUnwrap(obj);
    bool emulatesUndefined = actual->getClass()->emulatesUndefined();
    MOZ_ASSERT_IF(emulatesUndefined, obj->type()->flags & types::OBJECT_FLAG_EMULATES_UNDEFINED);
    return emulatesUndefined;
}

} /* namespace js */

#endif /* jsboolinlines_h___ */
