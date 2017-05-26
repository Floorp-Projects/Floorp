/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_ProfilingStack_h
#define js_ProfilingStack_h

#include <algorithm>
#include <stdint.h>

#include "jsbytecode.h"
#include "jstypes.h"
#include "js/TypeDecls.h"
#include "js/Utility.h"

struct JSRuntime;
class JSTracer;

class PseudoStack;

namespace js {

// A call stack can be specified to the JS engine such that all JS entry/exits
// to functions push/pop an entry to/from the specified stack.
//
// For more detailed information, see vm/GeckoProfiler.h.
//
class ProfileEntry
{
    // All fields are marked volatile to prevent the compiler from re-ordering
    // instructions. Namely this sequence:
    //
    //    entry[size] = ...;
    //    size++;
    //
    // If the size modification were somehow reordered before the stores, then
    // if a sample were taken it would be examining bogus information.
    //
    // A ProfileEntry represents both a C++ profile entry and a JS one.

    // Descriptive label for this entry. Must be a static string! Can be an
    // empty string, but not a null pointer.
    const char * volatile label_;

    // An additional descriptive string of this entry which is combined with
    // |label_| in profiler output. Need not be (and usually isn't) static. Can
    // be null.
    const char * volatile dynamicString_;

    // Stack pointer for non-JS entries, the script pointer otherwise.
    void * volatile spOrScript;

    // Line number for non-JS entries, the bytecode offset otherwise.
    int32_t volatile lineOrPcOffset;

    // Flags are in the low bits. The category is in the high bits.
    uint32_t volatile flagsAndCategory_;

    static int32_t pcToOffset(JSScript* aScript, jsbytecode* aPc);

  public:
    // These traits are bit masks. Make sure they're powers of 2.
    enum Flags : uint32_t {
        // Indicate whether a profile entry represents a CPP frame. If not set,
        // a JS frame is assumed by default. You're not allowed to publicly
        // change the frame type. Instead, initialize the ProfileEntry as either
        // a JS or CPP frame with `initJsFrame` or `initCppFrame` respectively.
        IS_CPP_ENTRY = 1u << 0,

        // This ProfileEntry is a dummy entry indicating the start of a run
        // of JS pseudostack entries.
        BEGIN_PSEUDO_JS = 1u << 1,

        // This flag is used to indicate that an interpreter JS entry has OSR-ed
        // into baseline.
        OSR = 1u << 2,

        // Union of all flags.
        ALL = IS_CPP_ENTRY|BEGIN_PSEUDO_JS|OSR,

        // Mask for removing all flags except the category information.
        CATEGORY_MASK = ~ALL
    };

    // Keep these in sync with devtools/client/performance/modules/categories.js
    enum class Category : uint32_t {
        OTHER    = 1u << 4,
        CSS      = 1u << 5,
        JS       = 1u << 6,
        GC       = 1u << 7,
        CC       = 1u << 8,
        NETWORK  = 1u << 9,
        GRAPHICS = 1u << 10,
        STORAGE  = 1u << 11,
        EVENTS   = 1u << 12,

        FIRST    = OTHER,
        LAST     = EVENTS
    };

    static_assert((static_cast<int>(Category::FIRST) & Flags::ALL) == 0,
                  "The category bitflags should not intersect with the other flags!");

    // All of these methods are marked with the 'volatile' keyword because the
    // Gecko Profiler's representation of the stack is stored such that all
    // ProfileEntry instances are volatile. These methods would not be
    // available unless they were marked as volatile as well.

    bool isCpp() const volatile { return hasFlag(IS_CPP_ENTRY); }
    bool isJs() const volatile { return !isCpp(); }

    void setLabel(const char* aLabel) volatile { label_ = aLabel; }
    const char* label() const volatile { return label_; }

    const char* dynamicString() const volatile { return dynamicString_; }

    void initCppFrame(const char* aLabel, const char* aDynamicString, void* sp, uint32_t aLine,
                      js::ProfileEntry::Flags aFlags, js::ProfileEntry::Category aCategory)
                      volatile
    {
        label_ = aLabel;
        dynamicString_ = aDynamicString;
        spOrScript = sp;
        lineOrPcOffset = static_cast<int32_t>(aLine);
        flagsAndCategory_ = aFlags | js::ProfileEntry::IS_CPP_ENTRY | uint32_t(aCategory);
    }

    void initJsFrame(const char* aLabel, const char* aDynamicString, JSScript* aScript,
                     jsbytecode* aPc) volatile
    {
        label_ = aLabel;
        dynamicString_ = aDynamicString;
        spOrScript = aScript;
        lineOrPcOffset = pcToOffset(aScript, aPc);
        flagsAndCategory_ = uint32_t(js::ProfileEntry::Category::JS);  // No flags needed.
    }

    void setFlag(uint32_t flag) volatile {
        MOZ_ASSERT(flag != IS_CPP_ENTRY);
        flagsAndCategory_ |= flag;
    }
    void unsetFlag(uint32_t flag) volatile {
        MOZ_ASSERT(flag != IS_CPP_ENTRY);
        flagsAndCategory_ &= ~flag;
    }
    bool hasFlag(uint32_t flag) const volatile {
        return bool(flagsAndCategory_ & flag);
    }

    uint32_t category() const volatile {
        return flagsAndCategory_ & CATEGORY_MASK;
    }

    void setOSR() volatile {
        MOZ_ASSERT(isJs());
        setFlag(OSR);
    }
    void unsetOSR() volatile {
        MOZ_ASSERT(isJs());
        unsetFlag(OSR);
    }
    bool isOSR() const volatile {
        return hasFlag(OSR);
    }

    void* stackAddress() const volatile {
        MOZ_ASSERT(!isJs());
        return spOrScript;
    }

    JS_PUBLIC_API(JSScript*) script() const volatile;

    uint32_t line() const volatile {
        MOZ_ASSERT(!isJs());
        return static_cast<uint32_t>(lineOrPcOffset);
    }

    // Note that the pointer returned might be invalid.
    JSScript* rawScript() const volatile {
        MOZ_ASSERT(isJs());
        return (JSScript*)spOrScript;
    }

    // We can't know the layout of JSScript, so look in vm/GeckoProfiler.cpp.
    JS_FRIEND_API(jsbytecode*) pc() const volatile;
    void setPC(jsbytecode* pc) volatile;

    void trace(JSTracer* trc) volatile;

    // The offset of a pc into a script's code can actually be 0, so to
    // signify a nullptr pc, use a -1 index. This is checked against in
    // pc() and setPC() to set/get the right pc.
    static const int32_t NullPCOffset = -1;
};

JS_FRIEND_API(void)
SetContextProfilingStack(JSContext* cx, PseudoStack* pseudoStack);

JS_FRIEND_API(void)
EnableContextProfilingStack(JSContext* cx, bool enabled);

JS_FRIEND_API(void)
RegisterContextProfilingEventMarker(JSContext* cx, void (*fn)(const char*));

} // namespace js

// The PseudoStack members are accessed in parallel by multiple threads: the
// profiler's sampler thread reads these members while other threads modify
// them.
class PseudoStack
{
  public:
    PseudoStack()
      : stackPointer(0)
    {}

    ~PseudoStack() {
        // The label macros keep a reference to the PseudoStack to avoid a TLS
        // access. If these are somehow not all cleared we will get a
        // use-after-free so better to crash now.
        MOZ_RELEASE_ASSERT(stackPointer == 0);
    }

    void pushCppFrame(const char* label, const char* dynamicString, void* sp, uint32_t line,
                      js::ProfileEntry::Category category,
                      js::ProfileEntry::Flags flags = js::ProfileEntry::Flags(0)) {
        if (stackPointer < MaxEntries) {
            entries[stackPointer].initCppFrame(label, dynamicString, sp, line, flags, category);
        }

        // This must happen at the end! The compiler will not reorder this
        // update because stackPointer is Atomic.
        stackPointer++;
    }

    void pushJsFrame(const char* label, const char* dynamicString, JSScript* script,
                     jsbytecode* pc) {
        if (stackPointer < MaxEntries) {
            entries[stackPointer].initJsFrame(label, dynamicString, script, pc);
        }

        // This must happen at the end! The compiler will not reorder this
        // update because stackPointer is Atomic.
        stackPointer++;
    }

    void pop() {
        MOZ_ASSERT(stackPointer > 0);
        stackPointer--;
    }

    uint32_t stackSize() const { return std::min(uint32_t(stackPointer), uint32_t(MaxEntries)); }

  private:
    // No copying.
    PseudoStack(const PseudoStack&) = delete;
    void operator=(const PseudoStack&) = delete;

  public:
    static const uint32_t MaxEntries = 1024;

    // The stack entries.
    js::ProfileEntry volatile entries[MaxEntries];

    // This may exceed MaxEntries, so instead use the stackSize() method to
    // determine the number of valid samples in entries. When this is less
    // than MaxEntries, it refers to the first free entry past the top of the
    // in-use stack (i.e. entries[stackPointer - 1] is the top stack entry).
    mozilla::Atomic<uint32_t> stackPointer;
};

#endif  /* js_ProfilingStack_h */
