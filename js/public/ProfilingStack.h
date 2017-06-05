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

    // Bits 0..1 hold the Kind. Bits 2..3 are unused. Bits 4..12 hold the
    // Category.
    uint32_t volatile kindAndCategory_;

    static int32_t pcToOffset(JSScript* aScript, jsbytecode* aPc);

  public:
    enum class Kind : uint32_t {
        // A normal C++ frame.
        CPP_NORMAL = 0,

        // A special C++ frame indicating the start of a run of JS pseudostack
        // entries. CPP_MARKER_FOR_JS frames are ignored, except for the sp
        // field.
        CPP_MARKER_FOR_JS = 1,

        // A normal JS frame.
        JS_NORMAL = 2,

        // An interpreter JS frame that has OSR-ed into baseline. JS_NORMAL
        // frames can be converted to JS_OSR and back. JS_OSR frames are
        // ignored.
        JS_OSR = 3,

        KIND_MASK = 0x3,
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
        LAST     = EVENTS,

        CATEGORY_MASK = ~uint32_t(Kind::KIND_MASK),
    };

    static_assert((uint32_t(Category::FIRST) & uint32_t(Kind::KIND_MASK)) == 0,
                  "Category overlaps with Kind");

    // All of these methods are marked with the 'volatile' keyword because the
    // Gecko Profiler's representation of the stack is stored such that all
    // ProfileEntry instances are volatile. These methods would not be
    // available unless they were marked as volatile as well.

    bool isCpp() const volatile
    {
        Kind k = kind();
        return k == Kind::CPP_NORMAL || k == Kind::CPP_MARKER_FOR_JS;
    }

    bool isJs() const volatile
    {
        Kind k = kind();
        return k == Kind::JS_NORMAL || k == Kind::JS_OSR;
    }

    void setLabel(const char* aLabel) volatile { label_ = aLabel; }
    const char* label() const volatile { return label_; }

    const char* dynamicString() const volatile { return dynamicString_; }

    void initCppFrame(const char* aLabel, const char* aDynamicString, void* sp, uint32_t aLine,
                      Kind aKind, Category aCategory) volatile
    {
        label_ = aLabel;
        dynamicString_ = aDynamicString;
        spOrScript = sp;
        lineOrPcOffset = static_cast<int32_t>(aLine);
        kindAndCategory_ = uint32_t(aKind) | uint32_t(aCategory);
        MOZ_ASSERT(isCpp());
    }

    void initJsFrame(const char* aLabel, const char* aDynamicString, JSScript* aScript,
                     jsbytecode* aPc) volatile
    {
        label_ = aLabel;
        dynamicString_ = aDynamicString;
        spOrScript = aScript;
        lineOrPcOffset = pcToOffset(aScript, aPc);
        kindAndCategory_ = uint32_t(Kind::JS_NORMAL) | uint32_t(Category::JS);
        MOZ_ASSERT(isJs());
    }

    void setKind(Kind aKind) volatile {
        kindAndCategory_ = uint32_t(aKind) | uint32_t(category());
    }

    Kind kind() const volatile {
        return Kind(kindAndCategory_ & uint32_t(Kind::KIND_MASK));
    }

    Category category() const volatile {
        return Category(kindAndCategory_ & uint32_t(Category::CATEGORY_MASK));
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
                      js::ProfileEntry::Kind kind, js::ProfileEntry::Category category) {
        if (stackPointer < MaxEntries) {
            entries[stackPointer].initCppFrame(label, dynamicString, sp, line, kind, category);
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
