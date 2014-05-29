/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_AsmJS_h
#define jit_AsmJS_h

#include "mozilla/MathAlgorithms.h"

#include <stddef.h>

#include "jsutil.h"

#include "js/TypeDecls.h"
#include "vm/ObjectImpl.h"

namespace js {

class ExclusiveContext;
namespace frontend {
    template <typename ParseHandler> struct Parser;
    template <typename ParseHandler> struct ParseContext;
    class FullParseHandler;
    struct ParseNode;
}

typedef frontend::Parser<frontend::FullParseHandler> AsmJSParser;
typedef frontend::ParseContext<frontend::FullParseHandler> AsmJSParseContext;

// Takes over parsing of a function starting with "use asm". The return value
// indicates whether an error was reported which the caller should propagate.
// If no error was reported, the function may still fail to validate as asm.js.
// In this case, the parser.tokenStream has been advanced an indeterminate
// amount and the entire function should be reparsed from the beginning.
extern bool
CompileAsmJS(ExclusiveContext *cx, AsmJSParser &parser, frontend::ParseNode *stmtList,
             bool *validated);

// The assumed page size; dynamically checked in CompileAsmJS.
const size_t AsmJSPageSize = 4096;

// The asm.js spec requires that the ArrayBuffer's byteLength be a multiple of 4096.
static const size_t AsmJSAllocationGranularity = 4096;

#ifdef JS_CODEGEN_X64
// On x64, the internal ArrayBuffer data array is inflated to 4GiB (only the
// byteLength portion of which is accessible) so that out-of-bounds accesses
// (made using a uint32 index) are guaranteed to raise a SIGSEGV.
static const size_t AsmJSBufferProtectedSize = 4 * 1024ULL * 1024ULL * 1024ULL;

// To avoid dynamically checking bounds on each load/store, asm.js code relies
// on the SIGSEGV handler in AsmJSSignalHandlers.cpp. However, this only works
// if we can guarantee that *any* out-of-bounds access generates a fault. This
// isn't generally true since an out-of-bounds access could land on other
// Mozilla data. To overcome this on x64, we reserve an entire 4GB space,
// making only the range [0, byteLength) accessible, and use a 32-bit unsigned
// index into this space. (x86 and ARM require different tricks.)
//
// One complication is that we need to put an ObjectElements struct immediately
// before the data array (as required by the general JSObject data structure).
// Thus, we must stick a page before the elements to hold ObjectElements.
//
//   |<------------------------------ 4GB + 1 pages --------------------->|
//           |<--- sizeof --->|<------------------- 4GB ----------------->|
//
//   | waste | ObjectElements | data array | inaccessible reserved memory |
//                            ^            ^                              ^
//                            |            \                             /
//                      obj->elements       required to be page boundaries
//
static const size_t AsmJSMappedSize = AsmJSPageSize + AsmJSBufferProtectedSize;
#endif // JS_CODEGEN_X64

#ifdef JS_ION

// Return whether asm.js optimization is inhibitted by the platform or
// dynamically disabled:
extern bool
IsAsmJSCompilationAvailable(JSContext *cx, unsigned argc, JS::Value *vp);

#else // JS_ION

inline bool
IsAsmJSCompilationAvailable(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().set(BooleanValue(false));
    return true;
}

#endif // JS_ION

// To succesfully link an asm.js module to an ArrayBuffer heap, the
// ArrayBuffer's byteLength must be:
//  - greater or equal to 4096
//  - either a power of 2 OR a multiple of 16MB
inline bool
IsValidAsmJSHeapLength(uint32_t length)
{
    if (length < 4096)
        return false;

    if (IsPowerOfTwo(length))
        return true;

    return (length & 0x00ffffff) == 0;
}

inline uint32_t
RoundUpToNextValidAsmJSHeapLength(uint32_t length)
{
    if (length < 4096)
        return 4096;

    if (length < 16 * 1024 * 1024)
        return mozilla::RoundUpPow2(length);

    JS_ASSERT(length <= 0xff000000);
    return (length + 0x00ffffff) & ~0x00ffffff;
}

} // namespace js

#endif // jit_AsmJS_h
