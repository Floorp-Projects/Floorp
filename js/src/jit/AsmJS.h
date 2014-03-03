/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_AsmJS_h
#define jit_AsmJS_h

#include <stddef.h>

#include "js/TypeDecls.h"
#include "vm/ObjectImpl.h"

namespace js {

class ExclusiveContext;
class AsmJSModule;
class SPSProfiler;
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

// The JSRuntime maintains a stack of AsmJSModule activations. An "activation"
// of module A is an initial call from outside A into a function inside A,
// followed by a sequence of calls inside A, and terminated by a call that
// leaves A. The AsmJSActivation stack serves three purposes:
//  - record the correct cx to pass to VM calls from asm.js;
//  - record enough information to pop all the frames of an activation if an
//    exception is thrown;
//  - record the information necessary for asm.js signal handlers to safely
//    recover from (expected) out-of-bounds access, the operation callback,
//    stack overflow, division by zero, etc.
class AsmJSActivation
{
    JSContext *cx_;
    AsmJSModule &module_;
    AsmJSActivation *prev_;
    void *errorRejoinSP_;
    SPSProfiler *profiler_;
    void *resumePC_;

  public:
    AsmJSActivation(JSContext *cx, AsmJSModule &module);
    ~AsmJSActivation();

    JSContext *cx() { return cx_; }
    AsmJSModule &module() const { return module_; }
    AsmJSActivation *prev() const { return prev_; }

    // Read by JIT code:
    static unsigned offsetOfContext() { return offsetof(AsmJSActivation, cx_); }
    static unsigned offsetOfResumePC() { return offsetof(AsmJSActivation, resumePC_); }

    // Initialized by JIT code:
    static unsigned offsetOfErrorRejoinSP() { return offsetof(AsmJSActivation, errorRejoinSP_); }

    // Set from SIGSEGV handler:
    void setResumePC(void *pc) { resumePC_ = pc; }
};

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

// The Asm.js heap length is constrained by the x64 backend heap access scheme
// to be a multiple of the page size which is 4096 bytes, and also constrained
// by the limits of ARM backends 'cmp immediate' instruction which supports a
// complex range for the immediate argument.
//
// ARMv7 mode supports the following immediate constants, and the Thumb T2
// instruction encoding also supports the subset of immediate constants used.
//  abcdefgh 00000000 00000000 00000000
//  00abcdef gh000000 00000000 00000000
//  0000abcd efgh0000 00000000 00000000
//  000000ab cdefgh00 00000000 00000000
//  00000000 abcdefgh 00000000 00000000
//  00000000 00abcdef gh000000 00000000
//  00000000 0000abcd efgh0000 00000000
//  ...
//
// The 4096 page size constraint restricts the length to:
//  xxxxxxxx xxxxxxxx xxxx0000 00000000
//
// Intersecting all the above constraints gives:
//  Heap length 0x40000000 to 0xff000000 quanta 0x01000000
//  Heap length 0x10000000 to 0x3fc00000 quanta 0x00400000
//  Heap length 0x04000000 to 0x0ff00000 quanta 0x00100000
//  Heap length 0x01000000 to 0x03fc0000 quanta 0x00040000
//  Heap length 0x00400000 to 0x00ff0000 quanta 0x00010000
//  Heap length 0x00100000 to 0x003fc000 quanta 0x00004000
//  Heap length 0x00001000 to 0x000ff000 quanta 0x00001000
//
inline uint32_t
RoundUpToNextValidAsmJSHeapLength(uint32_t length)
{
    if (length < 0x00001000u) // Minimum length is the pages size of 4096.
        return 0x1000u;
    if (length < 0x00100000u) // < 1M quanta 4K
        return (length + 0x00000fff) & ~0x00000fff;
    if (length < 0x00400000u) // < 4M quanta 16K
        return (length + 0x00003fff) & ~0x00003fff;
    if (length < 0x01000000u) // < 16M quanta 64K
        return (length + 0x0000ffff) & ~0x0000ffff;
    if (length < 0x04000000u) // < 64M quanta 256K
        return (length + 0x0003ffff) & ~0x0003ffff;
    if (length < 0x10000000u) // < 256M quanta 1M
        return (length + 0x000fffff) & ~0x000fffff;
    if (length < 0x40000000u) // < 1024M quanta 4M
        return (length + 0x003fffff) & ~0x003fffff;
    // < 4096M quanta 16M.  Note zero is returned if over 0xff000000 but such
    // lengths are not currently valid.
    JS_ASSERT(length <= 0xff000000);
    return (length + 0x00ffffff) & ~0x00ffffff;
}

inline bool
IsValidAsmJSHeapLength(uint32_t length)
{
    if (length <  AsmJSAllocationGranularity)
        return false;
    if (length <= 0x00100000u)
        return (length & 0x00000fff) == 0;
    if (length <= 0x00400000u)
        return (length & 0x00003fff) == 0;
    if (length <= 0x01000000u)
        return (length & 0x0000ffff) == 0;
    if (length <= 0x04000000u)
        return (length & 0x0003ffff) == 0;
    if (length <= 0x10000000u)
        return (length & 0x000fffff) == 0;
    if (length <= 0x40000000u)
        return (length & 0x003fffff) == 0;
    if (length <= 0xff000000u)
        return (length & 0x00ffffff) == 0;
    return false;
}

} // namespace js

#endif // jit_AsmJS_h
