/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_BaselineCacheIR_h
#define jit_BaselineCacheIR_h

#include "gc/Barrier.h"
#include "jit/CacheIR.h"

namespace js {
namespace jit {

class ICFallbackStub;
class ICStub;

// See the 'Sharing Baseline stub code' comment in CacheIR.h for a description
// of this class.
class CacheIRStubInfo
{
    // These fields don't require 8 bits, but GCC complains if these fields are
    // smaller than the size of the enums.
    CacheKind kind_ : 8;
    ICStubEngine engine_ : 8;
    bool makesGCCalls_ : 1;
    uint8_t stubDataOffset_;

    const uint8_t* code_;
    uint32_t length_;
    const uint8_t* gcTypes_;

    CacheIRStubInfo(CacheKind kind, ICStubEngine engine, bool makesGCCalls,
                    uint32_t stubDataOffset, const uint8_t* code, uint32_t codeLength,
                    const uint8_t* gcTypes)
      : kind_(kind),
        engine_(engine),
        makesGCCalls_(makesGCCalls),
        stubDataOffset_(stubDataOffset),
        code_(code),
        length_(codeLength),
        gcTypes_(gcTypes)
    {
        MOZ_ASSERT(kind_ == kind, "Kind must fit in bitfield");
        MOZ_ASSERT(engine_ == engine, "Engine must fit in bitfield");
        MOZ_ASSERT(stubDataOffset_ == stubDataOffset, "stubDataOffset must fit in uint8_t");
    }

    CacheIRStubInfo(const CacheIRStubInfo&) = delete;
    CacheIRStubInfo& operator=(const CacheIRStubInfo&) = delete;

  public:
    CacheKind kind() const { return kind_; }
    ICStubEngine engine() const { return engine_; }
    bool makesGCCalls() const { return makesGCCalls_; }

    const uint8_t* code() const { return code_; }
    uint32_t codeLength() const { return length_; }
    uint32_t stubDataOffset() const { return stubDataOffset_; }

    size_t stubDataSize() const;

    StubField::GCType gcType(uint32_t i) const { return (StubField::GCType)gcTypes_[i]; }

    static CacheIRStubInfo* New(CacheKind kind, ICStubEngine engine, bool canMakeCalls,
                                uint32_t stubDataOffset, const CacheIRWriter& writer);

    template <class T>
    js::GCPtr<T>& getStubField(ICStub* stub, uint32_t field) const;

    void copyStubData(ICStub* src, ICStub* dest) const;
};

void TraceBaselineCacheIRStub(JSTracer* trc, ICStub* stub, const CacheIRStubInfo* stubInfo);

ICStub* AttachBaselineCacheIRStub(JSContext* cx, const CacheIRWriter& writer,
                                  CacheKind kind, ICStubEngine engine, JSScript* outerScript,
                                  ICFallbackStub* stub);

} // namespace jit
} // namespace js

#endif /* jit_BaselineCacheIR_h */
