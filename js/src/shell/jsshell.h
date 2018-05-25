/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsshell_js_h
#define jsshell_js_h

#include "mozilla/Atomics.h"
#include "mozilla/Maybe.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Variant.h"

#include "jsapi.h"

#include "js/GCVector.h"
#include "threading/ConditionVariable.h"
#include "threading/LockGuard.h"
#include "threading/Mutex.h"
#include "threading/Thread.h"
#include "vm/GeckoProfiler.h"
#include "vm/Monitor.h"

// Some platform hooks must be implemented for single-step profiling.
#if defined(JS_SIMULATOR_ARM) || defined(JS_SIMULATOR_MIPS64) || defined(JS_SIMULATOR_MIPS32)
# define SINGLESTEP_PROFILING
#endif

namespace js {
namespace shell {

enum JSShellErrNum {
#define MSG_DEF(name, count, exception, format) \
    name,
#include "jsshell.msg"
#undef MSG_DEF
    JSShellErr_Limit
};

const JSErrorFormatString*
my_GetErrorMessage(void* userRef, const unsigned errorNumber);

void
WarningReporter(JSContext* cx, JSErrorReport* report);

class MOZ_STACK_CLASS AutoReportException
{
    JSContext* cx;
  public:
    explicit AutoReportException(JSContext* cx)
      : cx(cx)
    {}
    ~AutoReportException();
};

bool
GenerateInterfaceHelp(JSContext* cx, JS::HandleObject obj, const char* name);

JSString*
FileAsString(JSContext* cx, JS::HandleString pathnameStr);

class AutoCloseFile
{
  private:
    FILE* f_;
  public:
    explicit AutoCloseFile(FILE* f) : f_(f) {}
    ~AutoCloseFile() {
        (void) release();
    }
    bool release() {
        bool success = true;
        if (f_ && f_ != stdin && f_ != stdout && f_ != stderr)
            success = !fclose(f_);
        f_ = nullptr;
        return success;
    }
};

// Reference counted file.
struct RCFile {
    FILE* fp;
    uint32_t numRefs;

    RCFile() : fp(nullptr), numRefs(0) {}
    explicit RCFile(FILE* fp) : fp(fp), numRefs(0) {}

    void acquire() { numRefs++; }

    // Starts out with a ref count of zero.
    static RCFile* create(JSContext* cx, const char* filename, const char* mode);

    void close();
    bool isOpen() const { return fp; }
    bool release();
};

// Alias the global dstName to namespaceObj.srcName. For example, if dstName is
// "snarf", namespaceObj represents "os.file", and srcName is "readFile", then
// this is equivalent to the JS code:
//
//   snarf = os.file.readFile;
//
// This provides a mechanism for namespacing the various JS shell helper
// functions without breaking backwards compatibility with things that use the
// global names.
bool
CreateAlias(JSContext* cx, const char* dstName, JS::HandleObject namespaceObj, const char* srcName);

enum class ScriptKind
{
    Script,
    DecodeScript,
    Module
};

class NonshrinkingGCObjectVector : public GCVector<JSObject*, 0, SystemAllocPolicy>
{
  public:
    void sweep() {
        for (uint32_t i = 0; i < this->length(); i++) {
            if (JS::GCPolicy<JSObject*>::needsSweep(&(*this)[i]))
                (*this)[i] = nullptr;
        }
    }
};

using MarkBitObservers = JS::WeakCache<NonshrinkingGCObjectVector>;

#ifdef SINGLESTEP_PROFILING
using StackChars = Vector<char16_t, 0, SystemAllocPolicy>;
#endif

class OffThreadJob;

// Per-context shell state.
struct ShellContext
{
    explicit ShellContext(JSContext* cx);
    ~ShellContext();

    bool isWorker;
    double timeoutInterval;
    double startTime;
    mozilla::Atomic<bool> serviceInterrupt;
    mozilla::Atomic<bool> haveInterruptFunc;
    JS::PersistentRootedValue interruptFunc;
    bool lastWarningEnabled;
    JS::PersistentRootedValue lastWarning;
    JS::PersistentRootedValue promiseRejectionTrackerCallback;
#ifdef SINGLESTEP_PROFILING
    Vector<StackChars, 0, SystemAllocPolicy> stacks;
#endif

    /*
     * Watchdog thread state.
     */
    js::Mutex watchdogLock;
    js::ConditionVariable watchdogWakeup;
    mozilla::Maybe<js::Thread> watchdogThread;
    mozilla::Maybe<mozilla::TimeStamp> watchdogTimeout;

    js::ConditionVariable sleepWakeup;

    int exitCode;
    bool quitting;

    JS::UniqueChars readLineBuf;
    size_t readLineBufPos;

    js::shell::RCFile** errFilePtr;
    js::shell::RCFile** outFilePtr;

    UniquePtr<ProfilingStack> geckoProfilingStack;

    JS::UniqueChars moduleLoadPath;
    UniquePtr<MarkBitObservers> markObservers;

    // Off-thread parse state.
    js::Monitor offThreadMonitor;
    Vector<OffThreadJob*, 0, SystemAllocPolicy> offThreadJobs;
};

extern ShellContext*
GetShellContext(JSContext* cx);

} /* namespace shell */
} /* namespace js */

#endif
