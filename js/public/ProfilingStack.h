/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_ProfilingStack_h
#define js_ProfilingStack_h

#include "jsbytecode.h"
#include "jstypes.h"
#include "js/TypeDecls.h"

#include "js/Utility.h"

struct JSRuntime;
class JSTracer;

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

    // General purpose storage describing this frame.
    uint32_t volatile flags_;

  public:
    // These traits are bit masks. Make sure they're powers of 2.
    enum Flags : uint32_t {
        // Indicate whether a profile entry represents a CPP frame. If not set,
        // a JS frame is assumed by default. You're not allowed to publicly
        // change the frame type. Instead, initialize the ProfileEntry as either
        // a JS or CPP frame with `initJsFrame` or `initCppFrame` respectively.
        IS_CPP_ENTRY = 1 << 0,

        // This ProfileEntry is a dummy entry indicating the start of a run
        // of JS pseudostack entries.
        BEGIN_PSEUDO_JS = 1 << 1,

        // This flag is used to indicate that an interpreter JS entry has OSR-ed
        // into baseline.
        OSR = 1 << 2,

        // Union of all flags.
        ALL = IS_CPP_ENTRY|BEGIN_PSEUDO_JS|OSR,

        // Mask for removing all flags except the category information.
        CATEGORY_MASK = ~ALL
    };

    // Keep these in sync with devtools/client/performance/modules/categories.js
    enum class Category : uint32_t {
        OTHER    = 0x10,
        CSS      = 0x20,
        JS       = 0x40,
        GC       = 0x80,
        CC       = 0x100,
        NETWORK  = 0x200,
        GRAPHICS = 0x400,
        STORAGE  = 0x800,
        EVENTS   = 0x1000,

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

    void setDynamicString(const char* aDynamicString) volatile { dynamicString_ = aDynamicString; }
    const char* dynamicString() const volatile { return dynamicString_; }

    void initJsFrame(JSScript* aScript, jsbytecode* aPc) volatile {
        flags_ = 0;
        spOrScript = aScript;
        setPC(aPc);
    }
    void initCppFrame(void* aSp, uint32_t aLine) volatile {
        flags_ = IS_CPP_ENTRY;
        spOrScript = aSp;
        lineOrPcOffset = static_cast<int32_t>(aLine);
    }

    void setFlag(uint32_t flag) volatile {
        MOZ_ASSERT(flag != IS_CPP_ENTRY);
        flags_ |= flag;
    }
    void unsetFlag(uint32_t flag) volatile {
        MOZ_ASSERT(flag != IS_CPP_ENTRY);
        flags_ &= ~flag;
    }
    bool hasFlag(uint32_t flag) const volatile {
        return bool(flags_ & flag);
    }

    uint32_t flags() const volatile {
        return flags_;
    }

    uint32_t category() const volatile {
        return flags_ & CATEGORY_MASK;
    }
    void setCategory(Category c) volatile {
        MOZ_ASSERT(c >= Category::FIRST);
        MOZ_ASSERT(c <= Category::LAST);
        flags_ &= ~CATEGORY_MASK;
        setFlag(static_cast<uint32_t>(c));
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
    JS_FRIEND_API(void) setPC(jsbytecode* pc) volatile;

    void trace(JSTracer* trc);

    // The offset of a pc into a script's code can actually be 0, so to
    // signify a nullptr pc, use a -1 index. This is checked against in
    // pc() and setPC() to set/get the right pc.
    static const int32_t NullPCOffset = -1;

    static size_t offsetOfLabel() { return offsetof(ProfileEntry, label_); }
    static size_t offsetOfSpOrScript() { return offsetof(ProfileEntry, spOrScript); }
    static size_t offsetOfLineOrPcOffset() { return offsetof(ProfileEntry, lineOrPcOffset); }
    static size_t offsetOfFlags() { return offsetof(ProfileEntry, flags_); }
};

JS_FRIEND_API(void)
SetContextProfilingStack(JSContext* cx, ProfileEntry* stack, mozilla::Atomic<uint32_t>* size,
                         uint32_t max);

JS_FRIEND_API(void)
EnableContextProfilingStack(JSContext* cx, bool enabled);

JS_FRIEND_API(void)
RegisterContextProfilingEventMarker(JSContext* cx, void (*fn)(const char*));

} // namespace js

#endif  /* js_ProfilingStack_h */
