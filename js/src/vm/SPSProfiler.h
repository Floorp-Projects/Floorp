/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SPSProfiler_h__
#define SPSProfiler_h__

#include "mozilla/HashFunctions.h"

#include <stddef.h>

#include "jsfriendapi.h"
#include "jsfun.h"
#include "jsscript.h"

/*
 * SPS Profiler integration with the JS Engine
 * https://developer.mozilla.org/en/Performance/Profiling_with_the_Built-in_Profiler
 *
 * The SPS profiler (found in tools/profiler) is an implementation of a profiler
 * which has the ability to walk the C++ stack as well as use instrumentation to
 * gather information. When dealing with JS, however, SPS needs integration
 * with the engine because otherwise it is very difficult to figure out what
 * javascript is executing.
 *
 * The current method of integration with SPS is a form of instrumentation:
 * every time a JS function is entered, a bit of information is pushed onto a
 * stack that SPS owns and maintains. This information is then popped at the end
 * of the JS function. SPS informs the JS engine of this stack at runtime, and
 * it can by turned on/off dynamically.
 *
 * The SPS stack has three parameters: a base pointer, a size, and a maximum
 * size. The stack is the ProfileEntry stack which will have information written
 * to it. The size location is a pointer to an integer which represents the
 * current size of the stack (number of valid frames). This size will be
 * modified when JS functions are called. The maximum specified is the maximum
 * capacity of the ProfileEntry stack.
 *
 * Throughout execution, the size of the stack recorded in memory may exceed the
 * maximum. The JS engine will not write any information past the maximum limit,
 * but it will still maintain the size of the stack. SPS code is aware of this
 * and iterates the stack accordingly.
 *
 * There are two pointers of information pushed on the SPS stack for every JS
 * function that is entered. First is a char* pointer of a description of what
 * function was entered. Currently this string is of the form
 * "function (file:line)" if there's a function name, or just "file:line" if
 * there's no function name available. The other bit of information is the
 * relevant C++ (native) stack pointer. This stack pointer is what enables the
 * interleaving of the C++ and the JS stack.
 *
 * = Profile Strings
 *
 * The profile strings' allocations and deallocation must be carefully
 * maintained, and ideally at a very low overhead cost. For this reason, the JS
 * engine maintains a mapping of all known profile strings. These strings are
 * keyed in lookup by a JSScript*, but are serialized with a JSFunction*,
 * JSScript* pair. A JSScript will destroy its corresponding profile string when
 * the script is finalized.
 *
 * For this reason, a char* pointer pushed on the SPS stack is valid only while
 * it is on the SPS stack. SPS uses sampling to read off information from this
 * instrumented stack, and it therefore copies the string byte for byte when a
 * JS function is encountered during sampling.
 *
 * = Native Stack Pointer
 *
 * The actual value pushed as the native pointer is NULL for most JS functions.
 * The reason for this is that there's actually very little correlation between
 * the JS stack and the C++ stack because many JS functions all run in the same
 * C++ frame, or can even go backwards in C++ when going from the JIT back to
 * the interpreter.
 *
 * To alleviate this problem, all JS functions push NULL as their "native stack
 * pointer" to indicate that it's a JS function call. The function RunScript(),
 * however, pushes an actual C++ stack pointer onto the SPS stack. This way when
 * interleaving C++ and JS, if SPS sees a NULL native stack pointer on the SPS
 * stack, it looks backwards for the first non-NULL pointer and uses that for
 * all subsequent NULL native stack pointers.
 */

namespace js {

typedef HashMap<JSScript*, const char*, DefaultHasher<JSScript*>, SystemAllocPolicy>
        ProfileStringMap;

class SPSEntryMarker;

class SPSProfiler
{
    friend class SPSEntryMarker;

    JSRuntime            *rt;
    ProfileStringMap     strings;
    ProfileEntry         *stack_;
    uint32_t             *size_;
    uint32_t             max_;
    bool                 slowAssertions;
    bool                 enabled_;

    static const char *allocProfileString(JSContext *cx, JSScript *script,
                                          JSFunction *function);
    void push(const char *string, void *sp);
    void pop();

  public:
    SPSProfiler(JSRuntime *rt)
        : rt(rt),
          stack_(NULL),
          size_(NULL),
          max_(0),
          slowAssertions(false),
          enabled_(false)
    {}
    ~SPSProfiler();

    uint32_t *size() { return size_; }
    uint32_t maxSize() { return max_; }
    ProfileEntry *stack() { return stack_; }

    bool enabled() { JS_ASSERT_IF(enabled_, installed()); return enabled_; }
    bool installed() { return stack_ != NULL && size_ != NULL; }
    void enable(bool enabled);
    void enableSlowAssertions(bool enabled) { slowAssertions = enabled; }
    bool slowAssertionsEnabled() { return slowAssertions; }
    bool enter(JSContext *cx, JSScript *script, JSFunction *maybeFun);
    void exit(JSContext *cx, JSScript *script, JSFunction *maybeFun);
    void setProfilingStack(ProfileEntry *stack, uint32_t *size, uint32_t max);
    const char *profileString(JSContext *cx, JSScript *script, JSFunction *maybeFun);
    void onScriptFinalized(JSScript *script);

    /* meant to be used for testing, not recommended to call in normal code */
    size_t stringsCount() { return strings.count(); }
    void stringsReset() { strings.clear(); }
};

/*
 * This class is used in RunScript() to push the marker onto the sampling stack
 * that we're about to enter JS function calls. This is the only time in which a
 * valid stack pointer is pushed to the sampling stack.
 */
class SPSEntryMarker
{
    SPSProfiler *profiler;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
  public:
    SPSEntryMarker(JSRuntime *rt JS_GUARD_OBJECT_NOTIFIER_PARAM);
    ~SPSEntryMarker();
};

} /* namespace js */

#endif /* SPSProfiler_h__ */
