/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/BigIntType.h"

#include "mozilla/FloatingPoint.h"
#include "mozilla/HashFunctions.h"

#include "jsapi.h"

#include "gc/Allocator.h"
#include "gc/Tracer.h"
#include "vm/JSContext.h"
#include "vm/SelfHosting.h"

using namespace js;

BigInt*
BigInt::create(JSContext* cx)
{
    BigInt* x = Allocate<BigInt>(cx);
    if (!x)
        return nullptr;
    return x;
}

BigInt*
BigInt::copy(JSContext* cx, HandleBigInt x)
{
    BigInt* bi = create(cx);
    if (!bi)
        return nullptr;
    return bi;
}

JSLinearString*
BigInt::toString(JSContext* cx, BigInt* x)
{
    return nullptr;
}

void
BigInt::finalize(js::FreeOp* fop)
{
    return;
}

JSAtom*
js::BigIntToAtom(JSContext* cx, BigInt* bi)
{
    JSString* str = BigInt::toString(cx, bi);
    if (!str)
        return nullptr;
    return AtomizeString(cx, str);
}

bool
BigInt::toBoolean()
{
    return false;
}

js::HashNumber
BigInt::hash()
{
    return 0;
}

size_t
BigInt::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
    return 0;
}

JS::ubi::Node::Size
JS::ubi::Concrete<BigInt>::size(mozilla::MallocSizeOf mallocSizeOf) const
{
    MOZ_ASSERT(get().isTenured());
    return js::gc::Arena::thingSize(get().asTenured().getAllocKind());
}
