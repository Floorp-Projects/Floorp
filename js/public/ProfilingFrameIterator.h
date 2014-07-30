/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_ProfilingFrameIterator_h
#define js_ProfilingFrameIterator_h

#include "mozilla/Alignment.h"

#include <stdint.h>

#include "js/Utility.h"

class JSAtom;
struct JSRuntime;
namespace js { class AsmJSActivation; class AsmJSProfilingFrameIterator; }

namespace JS {

// This iterator can be used to walk the stack of a thread suspended at an
// arbitrary pc. To provide acurate results, profiling must have been enabled
// (via EnableRuntimeProfilingStack) before executing the callstack being
// unwound.
class JS_PUBLIC_API(ProfilingFrameIterator)
{
    js::AsmJSActivation *activation_;

    static const unsigned StorageSpace = 6 * sizeof(void*);
    mozilla::AlignedStorage<StorageSpace> storage_;
    js::AsmJSProfilingFrameIterator &iter() {
        JS_ASSERT(!done());
        return *reinterpret_cast<js::AsmJSProfilingFrameIterator*>(storage_.addr());
    }
    const js::AsmJSProfilingFrameIterator &iter() const {
        JS_ASSERT(!done());
        return *reinterpret_cast<const js::AsmJSProfilingFrameIterator*>(storage_.addr());
    }

    void settle();

  public:
    struct RegisterState
    {
        RegisterState() : pc(nullptr), sp(nullptr), lr(nullptr) {}
        void *pc;
        void *sp;
        void *lr;
    };

    ProfilingFrameIterator(JSRuntime *rt, const RegisterState &state);
    ~ProfilingFrameIterator();
    void operator++();
    bool done() const { return !activation_; }

    // Assuming the stack grows down (we do), the return value:
    //  - always points into the stack
    //  - is weakly monotonically increasing (may be equal for successive frames)
    //  - will compare greater than newer native and psuedo-stack frame addresses
    //    and less than older native and psuedo-stack frame addresses
    void *stackAddress() const;

    // Return a label suitable for regexp-matching as performed by
    // browser/devtools/profiler/cleopatra/js/parserWorker.js
    const char *label() const;
};

} // namespace JS

#endif  /* js_ProfilingFrameIterator_h */
