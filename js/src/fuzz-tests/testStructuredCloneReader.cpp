/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "mozilla/ScopeExit.h"

#include "jsapi.h"

#include "fuzz-tests/tests.h"
#include "vm/Interpreter.h"

#include "vm/JSContext-inl.h"

using namespace js;

// These are defined and pre-initialized by the harness (in tests.cpp).
extern JS::PersistentRootedObject gGlobal;
extern JSContext* gCx;

static int
testStructuredCloneReaderInit(int *argc, char ***argv) {
  return 0;
}

static int
testStructuredCloneReaderFuzz(const uint8_t* buf, size_t size) {
    auto gcGuard = mozilla::MakeScopeExit([&] {
        JS::PrepareForFullGC(gCx);
        JS::GCForReason(gCx, GC_NORMAL, JS::gcreason::API);
    });

    if (!size) return 0;

    // Make sure to pad the buffer to a multiple of kSegmentAlignment
    const size_t kSegmentAlignment = 8;
    size_t buf_size = JS_ROUNDUP(size, kSegmentAlignment);

    auto clonebuf = MakeUnique<JSStructuredCloneData>();
    if (!clonebuf || !clonebuf->Init(buf_size)) {
        ReportOutOfMemory(gCx);
        return 0;
    }

    // Copy buffer then pad with zeroes.
    clonebuf->AppendBytes((const char*)buf, size);
    char padding[kSegmentAlignment] = {0};
    clonebuf->AppendBytes(padding, buf_size - size);

    JS::StructuredCloneScope scope = JS::StructuredCloneScope::DifferentProcess;

    RootedValue deserialized(gCx);
    if (!JS_ReadStructuredClone(gCx, *clonebuf,
    			JS_STRUCTURED_CLONE_VERSION,
    			scope,
    			&deserialized, nullptr, nullptr))
    {
        return 0;
    }

    /* If we succeeded in deserializing, we should try to reserialize the data.
       This has two main advantages:

       1) It tests parts of the serializer as well.
       2) The deserialized data is actually used, making it more likely to detect
          further memory-related problems.

       Tests show that this also doesn't cause a serious performance penalty.
    */
    mozilla::Maybe<JSAutoStructuredCloneBuffer> clonebufOut;
    JS::CloneDataPolicy policy;

    clonebufOut.emplace(scope, nullptr, nullptr);
    if (!clonebufOut->write(gCx, deserialized, UndefinedHandleValue, policy))
        return 0;

    return 0;
}

MOZ_FUZZING_INTERFACE_RAW(
    testStructuredCloneReaderInit,
    testStructuredCloneReaderFuzz,
    StructuredCloneReader
);
