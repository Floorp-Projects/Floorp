/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_BigIntType_h
#define vm_BigIntType_h

#include <gmp.h>

#include "gc/Barrier.h"
#include "gc/GC.h"
#include "gc/Heap.h"
#include "js/AllocPolicy.h"
#include "js/GCHashTable.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "vm/StringType.h"

namespace JS {

class BigInt final : public js::gc::TenuredCell
{
  private:
    // The minimum allocation size is currently 16 bytes (see
    // SortedArenaList in gc/ArenaList.h).
    union {
        mpz_t num_;
        uint8_t unused_[js::gc::MinCellSize];
    };

  public:
    // Allocate and initialize a BigInt value
    static BigInt* create(JSContext* cx);

    static BigInt* createFromDouble(JSContext* cx, double d);

    static BigInt* createFromBoolean(JSContext* cx, bool b);

    static const JS::TraceKind TraceKind = JS::TraceKind::BigInt;

    void traceChildren(JSTracer* trc);

    void finalize(js::FreeOp* fop);

    js::HashNumber hash();

    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

    bool toBoolean();

    static void init();

    static BigInt* copy(JSContext* cx, Handle<BigInt*> x);

    static JSLinearString* toString(JSContext* cx, BigInt* x, uint8_t radix);
};

static_assert(sizeof(BigInt) >= js::gc::MinCellSize,
              "sizeof(BigInt) must be greater than the minimum allocation size");

} // namespace JS

namespace js {

extern JSAtom*
BigIntToAtom(JSContext* cx, JS::BigInt* bi);

extern JS::BigInt*
NumberToBigInt(JSContext* cx, double d);

extern JS::BigInt*
ToBigInt(JSContext* cx, JS::Handle<JS::Value> v);

} // namespace js

#endif
