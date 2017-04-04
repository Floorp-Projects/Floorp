/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS shell. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/mozalloc.h"
#include "mozilla/PodOperations.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/SizePrintfMacros.h"
#include "mozilla/Sprintf.h"
#include "mozilla/TimeStamp.h"

#ifdef XP_WIN
# include <direct.h>
# include <process.h>
#endif
#include <errno.h>
#include <fcntl.h>
#if defined(XP_WIN)
# include <io.h>     /* for isatty() */
#endif
#include <locale.h>
#if defined(MALLOC_H)
# include MALLOC_H    /* for malloc_usable_size, malloc_size, _msize */
#endif
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef XP_UNIX
# include <sys/mman.h>
# include <sys/stat.h>
# include <sys/wait.h>
# include <unistd.h>
#endif

#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsfriendapi.h"
#include "jsfun.h"
#include "jsobj.h"
#include "jsprf.h"
#include "jsscript.h"
#include "jstypes.h"
#include "jsutil.h"
#ifdef XP_WIN
# include "jswin.h"
#endif
#include "jswrapper.h"
#include "shellmoduleloader.out.h"

#include "builtin/ModuleObject.h"
#include "builtin/TestingFunctions.h"
#include "frontend/Parser.h"
#include "gc/GCInternals.h"
#include "jit/arm/Simulator-arm.h"
#include "jit/InlinableNatives.h"
#include "jit/Ion.h"
#include "jit/JitcodeMap.h"
#include "jit/OptimizationTracking.h"
#include "js/Debug.h"
#include "js/GCAPI.h"
#include "js/GCVector.h"
#include "js/Initialization.h"
#include "js/StructuredClone.h"
#include "js/SweepingAPI.h"
#include "js/TrackedOptimizationInfo.h"
#include "perf/jsperf.h"
#include "shell/jsoptparse.h"
#include "shell/jsshell.h"
#include "shell/OSObject.h"
#include "threading/ConditionVariable.h"
#include "threading/LockGuard.h"
#include "threading/Thread.h"
#include "vm/ArgumentsObject.h"
#include "vm/AsyncFunction.h"
#include "vm/AsyncIteration.h"
#include "vm/Compression.h"
#include "vm/Debugger.h"
#include "vm/HelperThreads.h"
#include "vm/Monitor.h"
#include "vm/MutexIDs.h"
#include "vm/Shape.h"
#include "vm/SharedArrayObject.h"
#include "vm/StringBuffer.h"
#include "vm/Time.h"
#include "vm/TypedArrayObject.h"
#include "vm/WrapperObject.h"
#include "wasm/WasmJS.h"

#include "jscompartmentinlines.h"
#include "jsobjinlines.h"

#include "vm/ErrorObject-inl.h"
#include "vm/Interpreter-inl.h"
#include "vm/Stack-inl.h"

using namespace js;
using namespace js::cli;
using namespace js::shell;

using mozilla::ArrayLength;
using mozilla::Atomic;
using mozilla::MakeScopeExit;
using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::NumberEqualsInt32;
using mozilla::PodCopy;
using mozilla::PodEqual;
using mozilla::TimeDuration;
using mozilla::TimeStamp;

enum JSShellExitCode {
    EXITCODE_RUNTIME_ERROR      = 3,
    EXITCODE_FILE_NOT_FOUND     = 4,
    EXITCODE_OUT_OF_MEMORY      = 5,
    EXITCODE_TIMEOUT            = 6
};

static const size_t gStackChunkSize = 8192;

/*
 * Note: This limit should match the stack limit set by the browser in
 *       js/xpconnect/src/XPCJSContext.cpp
 */
#if defined(MOZ_ASAN) || (defined(DEBUG) && !defined(XP_WIN))
static const size_t gMaxStackSize = 2 * 128 * sizeof(size_t) * 1024;
#else
static const size_t gMaxStackSize = 128 * sizeof(size_t) * 1024;
#endif

/*
 * Limit the timeout to 30 minutes to prevent an overflow on platfoms
 * that represent the time internally in microseconds using 32-bit int.
 */
static const TimeDuration MAX_TIMEOUT_INTERVAL = TimeDuration::FromSeconds(1800.0);

// SharedArrayBuffer and Atomics are enabled by default (tracking Firefox).
#define SHARED_MEMORY_DEFAULT 1

// Some platform hooks must be implemented for single-step profiling.
#if defined(JS_SIMULATOR_ARM) || defined(JS_SIMULATOR_MIPS64)
# define SINGLESTEP_PROFILING
#endif

using JobQueue = GCVector<JSObject*, 0, SystemAllocPolicy>;

struct ShellAsyncTasks
{
    explicit ShellAsyncTasks(JSContext* cx)
      : outstanding(0),
        finished(cx)
    {}

    size_t outstanding;
    Vector<JS::AsyncTask*> finished;
};

enum class ScriptKind
{
    Script,
    DecodeScript,
    Module
};

class OffThreadState {
    enum State {
        IDLE,           /* ready to work; no token, no source */
        COMPILING,      /* working; no token, have source */
        DONE            /* compilation done: have token and source */
    };

  public:
    OffThreadState()
      : monitor(mutexid::ShellOffThreadState),
        state(IDLE),
        token(),
        source(nullptr)
    { }

    bool startIfIdle(JSContext* cx, ScriptKind kind,
                     ScopedJSFreePtr<char16_t>& newSource)
    {
        AutoLockMonitor alm(monitor);
        if (state != IDLE)
            return false;

        MOZ_ASSERT(!token);

        source = newSource.forget();

        scriptKind = kind;
        state = COMPILING;
        return true;
    }

    bool startIfIdle(JSContext* cx, ScriptKind kind,
                     JS::TranscodeBuffer&& newXdr)
    {
        AutoLockMonitor alm(monitor);
        if (state != IDLE)
            return false;

        MOZ_ASSERT(!token);

        xdr = mozilla::Move(newXdr);

        scriptKind = kind;
        state = COMPILING;
        return true;
    }

    void abandon(JSContext* cx) {
        AutoLockMonitor alm(monitor);
        MOZ_ASSERT(state == COMPILING);
        MOZ_ASSERT(!token);
        MOZ_ASSERT(source || !xdr.empty());

        if (source)
            js_free(source);
        source = nullptr;
        xdr.clearAndFree();

        state = IDLE;
    }

    void markDone(void* newToken) {
        AutoLockMonitor alm(monitor);
        MOZ_ASSERT(state == COMPILING);
        MOZ_ASSERT(!token);
        MOZ_ASSERT(source || !xdr.empty());
        MOZ_ASSERT(newToken);

        token = newToken;
        state = DONE;
        alm.notify();
    }

    void* waitUntilDone(JSContext* cx, ScriptKind kind) {
        AutoLockMonitor alm(monitor);
        if (state == IDLE || scriptKind != kind)
            return nullptr;

        if (state == COMPILING) {
            while (state != DONE)
                alm.wait();
        }

        MOZ_ASSERT(source || !xdr.empty());
        if (source)
            js_free(source);
        source = nullptr;
        xdr.clearAndFree();

        MOZ_ASSERT(token);
        void* holdToken = token;
        token = nullptr;
        state = IDLE;
        return holdToken;
    }

    JS::TranscodeBuffer& xdrBuffer() { return xdr; }

  private:
    Monitor monitor;
    ScriptKind scriptKind;
    State state;
    void* token;
    char16_t* source;
    JS::TranscodeBuffer xdr;
};

#ifdef SINGLESTEP_PROFILING
typedef Vector<char16_t, 0, SystemAllocPolicy> StackChars;
#endif

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

struct ShellCompartmentPrivate {
    JS::Heap<JSObject*> grayRoot;
};

// Per-context shell state.
struct ShellContext
{
    explicit ShellContext(JSContext* cx);
    bool isWorker;
    double timeoutInterval;
    double startTime;
    Atomic<bool> serviceInterrupt;
    Atomic<bool> haveInterruptFunc;
    JS::PersistentRootedValue interruptFunc;
    bool lastWarningEnabled;
    JS::PersistentRootedValue lastWarning;
    JS::PersistentRootedValue promiseRejectionTrackerCallback;
    JS::PersistentRooted<JobQueue> jobQueue;
    ExclusiveData<ShellAsyncTasks> asyncTasks;
    bool drainingJobQueue;
#ifdef SINGLESTEP_PROFILING
    Vector<StackChars, 0, SystemAllocPolicy> stacks;
#endif

    /*
     * Watchdog thread state.
     */
    Mutex watchdogLock;
    ConditionVariable watchdogWakeup;
    Maybe<Thread> watchdogThread;
    Maybe<TimeStamp> watchdogTimeout;

    ConditionVariable sleepWakeup;

    int exitCode;
    bool quitting;

    UniqueChars readLineBuf;
    size_t readLineBufPos;

    static const uint32_t GeckoProfilingMaxStackSize = 1000;
    ProfileEntry geckoProfilingStack[GeckoProfilingMaxStackSize];
    uint32_t geckoProfilingStackSize;

    OffThreadState offThreadState;

    UniqueChars moduleLoadPath;
    UniquePtr<MarkBitObservers> markObservers;
};

struct MOZ_STACK_CLASS EnvironmentPreparer : public js::ScriptEnvironmentPreparer {
    JSContext* cx;
    explicit EnvironmentPreparer(JSContext* cx)
      : cx(cx)
    {
        js::SetScriptEnvironmentPreparer(cx, this);
    }
    void invoke(JS::HandleObject scope, Closure& closure) override;
};

// Shell state set once at startup.
static bool enableCodeCoverage = false;
static bool enableDisassemblyDumps = false;
static bool offthreadCompilation = false;
static bool enableBaseline = false;
static bool enableIon = false;
static bool enableAsmJS = false;
static bool enableWasm = false;
static bool enableNativeRegExp = false;
static bool enableUnboxedArrays = false;
static bool enableSharedMemory = SHARED_MEMORY_DEFAULT;
static bool enableWasmAlwaysBaseline = false;
#ifdef JS_GC_ZEAL
static uint32_t gZealBits = 0;
static uint32_t gZealFrequency = 0;
#endif
static bool printTiming = false;
static const char* jsCacheDir = nullptr;
static const char* jsCacheAsmJSPath = nullptr;
static RCFile* gErrFile = nullptr;
static RCFile* gOutFile = nullptr;
static bool reportWarnings = true;
static bool compileOnly = false;
static bool fuzzingSafe = false;
static bool disableOOMFunctions = false;

#ifdef DEBUG
static bool dumpEntrainedVariables = false;
static bool OOM_printAllocationCount = false;
#endif

// Shell state this is only accessed on the main thread.
bool jsCachingEnabled = false;
mozilla::Atomic<bool> jsCacheOpened(false);

static bool
SetTimeoutValue(JSContext* cx, double t);

static void
KillWatchdog(JSContext* cx);

static bool
ScheduleWatchdog(JSContext* cx, double t);

static void
CancelExecution(JSContext* cx);

static JSObject*
NewGlobalObject(JSContext* cx, JS::CompartmentOptions& options,
                JSPrincipals* principals);

/*
 * A toy principals type for the shell.
 *
 * In the shell, a principal is simply a 32-bit mask: P subsumes Q if the
 * set bits in P are a superset of those in Q. Thus, the principal 0 is
 * subsumed by everything, and the principal ~0 subsumes everything.
 *
 * As a special case, a null pointer as a principal is treated like 0xffff.
 *
 * The 'newGlobal' function takes an option indicating which principal the
 * new global should have; 'evaluate' does for the new code.
 */
class ShellPrincipals final : public JSPrincipals {
    uint32_t bits;

    static uint32_t getBits(JSPrincipals* p) {
        if (!p)
            return 0xffff;
        return static_cast<ShellPrincipals*>(p)->bits;
    }

  public:
    explicit ShellPrincipals(uint32_t bits, int32_t refcount = 0) : bits(bits) {
        this->refcount = refcount;
    }

    bool write(JSContext* cx, JSStructuredCloneWriter* writer) override {
        // The shell doesn't have a read principals hook, so it doesn't really
        // matter what we write here, but we have to write something so the
        // fuzzer is happy.
        return JS_WriteUint32Pair(writer, bits, 0);
    }

    static void destroy(JSPrincipals* principals) {
        MOZ_ASSERT(principals != &fullyTrusted);
        MOZ_ASSERT(principals->refcount == 0);
        js_delete(static_cast<const ShellPrincipals*>(principals));
    }

    static bool subsumes(JSPrincipals* first, JSPrincipals* second) {
        uint32_t firstBits  = getBits(first);
        uint32_t secondBits = getBits(second);
        return (firstBits | secondBits) == firstBits;
    }

    static JSSecurityCallbacks securityCallbacks;

    // Fully-trusted principals singleton.
    static ShellPrincipals fullyTrusted;
};

JSSecurityCallbacks ShellPrincipals::securityCallbacks = {
    nullptr, // contentSecurityPolicyAllows
    subsumes
};

// The fully-trusted principal subsumes all other principals.
ShellPrincipals ShellPrincipals::fullyTrusted(-1, 1);

#ifdef EDITLINE
extern "C" {
extern JS_EXPORT_API(char*) readline(const char* prompt);
extern JS_EXPORT_API(void)   add_history(char* line);
} // extern "C"
#endif

ShellContext::ShellContext(JSContext* cx)
  : isWorker(false),
    timeoutInterval(-1.0),
    startTime(PRMJ_Now()),
    serviceInterrupt(false),
    haveInterruptFunc(false),
    interruptFunc(cx, NullValue()),
    lastWarningEnabled(false),
    lastWarning(cx, NullValue()),
    promiseRejectionTrackerCallback(cx, NullValue()),
    asyncTasks(mutexid::ShellAsyncTasks, cx),
    drainingJobQueue(false),
    watchdogLock(mutexid::ShellContextWatchdog),
    exitCode(0),
    quitting(false),
    readLineBufPos(0),
    geckoProfilingStackSize(0)
{}

static ShellContext*
GetShellContext(JSContext* cx)
{
    ShellContext* sc = static_cast<ShellContext*>(JS_GetContextPrivate(cx));
    MOZ_ASSERT(sc);
    return sc;
}

static void
TraceGrayRoots(JSTracer* trc, void* data)
{
    JSRuntime* rt = trc->runtime();
    for (ZonesIter zone(rt, SkipAtoms); !zone.done(); zone.next()) {
        for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next()) {
            auto priv = static_cast<ShellCompartmentPrivate*>(JS_GetCompartmentPrivate(comp.get()));
            if (priv)
                JS::TraceEdge(trc, &priv->grayRoot, "test gray root");
        }
    }
}

static char*
GetLine(FILE* file, const char * prompt)
{
#ifdef EDITLINE
    /*
     * Use readline only if file is stdin, because there's no way to specify
     * another handle.  Are other filehandles interactive?
     */
    if (file == stdin) {
        char* linep = readline(prompt);
        /*
         * We set it to zero to avoid complaining about inappropriate ioctl
         * for device in the case of EOF. Looks like errno == 251 if line is
         * finished with EOF and errno == 25 (EINVAL on Mac) if there is
         * nothing left to read.
         */
        if (errno == 251 || errno == 25 || errno == EINVAL)
            errno = 0;
        if (!linep)
            return nullptr;
        if (linep[0] != '\0')
            add_history(linep);
        return linep;
    }
#endif

    size_t len = 0;
    if (*prompt != '\0' && gOutFile->isOpen()) {
        fprintf(gOutFile->fp, "%s", prompt);
        fflush(gOutFile->fp);
    }

    size_t size = 80;
    char* buffer = static_cast<char*>(malloc(size));
    if (!buffer)
        return nullptr;

    char* current = buffer;
    do {
        while (true) {
            if (fgets(current, size - len, file))
                break;
            if (errno != EINTR) {
                free(buffer);
                return nullptr;
            }
        }

        len += strlen(current);
        char* t = buffer + len - 1;
        if (*t == '\n') {
            /* Line was read. We remove '\n' and exit. */
            *t = '\0';
            return buffer;
        }

        if (len + 1 == size) {
            size = size * 2;
            char* tmp = static_cast<char*>(realloc(buffer, size));
            if (!tmp) {
                free(buffer);
                return nullptr;
            }
            buffer = tmp;
        }
        current = buffer + len;
    } while (true);
    return nullptr;
}

static bool
ShellInterruptCallback(JSContext* cx)
{
    ShellContext* sc = GetShellContext(cx);
    if (!sc->serviceInterrupt)
        return true;

    // Reset serviceInterrupt. CancelExecution or InterruptIf will set it to
    // true to distinguish watchdog or user triggered interrupts.
    // Do this first to prevent other interrupts that may occur while the
    // user-supplied callback is executing from re-entering the handler.
    sc->serviceInterrupt = false;

    bool result;
    if (sc->haveInterruptFunc) {
        bool wasAlreadyThrowing = cx->isExceptionPending();
        JS::AutoSaveExceptionState savedExc(cx);
        JSAutoCompartment ac(cx, &sc->interruptFunc.toObject());
        RootedValue rval(cx);

        // Report any exceptions thrown by the JS interrupt callback, but do
        // *not* keep it on the cx. The interrupt handler is invoked at points
        // that are not expected to throw catchable exceptions, like at
        // JSOP_RETRVAL.
        //
        // If the interrupted JS code was already throwing, any exceptions
        // thrown by the interrupt handler are silently swallowed.
        {
            Maybe<AutoReportException> are;
            if (!wasAlreadyThrowing)
                are.emplace(cx);
            result = JS_CallFunctionValue(cx, nullptr, sc->interruptFunc,
                                          JS::HandleValueArray::empty(), &rval);
        }
        savedExc.restore();

        if (rval.isBoolean())
            result = rval.toBoolean();
        else
            result = false;
    } else {
        result = false;
    }

    if (!result && sc->exitCode == 0)
        sc->exitCode = EXITCODE_TIMEOUT;

    return result;
}

/*
 * Some UTF-8 files, notably those written using Notepad, have a Unicode
 * Byte-Order-Mark (BOM) as their first character. This is useless (byte-order
 * is meaningless for UTF-8) but causes a syntax error unless we skip it.
 */
static void
SkipUTF8BOM(FILE* file)
{
    int ch1 = fgetc(file);
    int ch2 = fgetc(file);
    int ch3 = fgetc(file);

    // Skip the BOM
    if (ch1 == 0xEF && ch2 == 0xBB && ch3 == 0xBF)
        return;

    // No BOM - revert
    if (ch3 != EOF)
        ungetc(ch3, file);
    if (ch2 != EOF)
        ungetc(ch2, file);
    if (ch1 != EOF)
        ungetc(ch1, file);
}

void
EnvironmentPreparer::invoke(HandleObject scope, Closure& closure)
{
    MOZ_ASSERT(!JS_IsExceptionPending(cx));

    AutoCompartment ac(cx, scope);
    AutoReportException are(cx);
    if (!closure(cx))
        return;
}

static MOZ_MUST_USE bool
RunFile(JSContext* cx, const char* filename, FILE* file, bool compileOnly)
{
    SkipUTF8BOM(file);

    // To support the UNIX #! shell hack, gobble the first line if it starts
    // with '#'.
    int ch = fgetc(file);
    if (ch == '#') {
        while ((ch = fgetc(file)) != EOF) {
            if (ch == '\n' || ch == '\r')
                break;
        }
    }
    ungetc(ch, file);

    int64_t t1 = PRMJ_Now();
    RootedScript script(cx);

    {
        CompileOptions options(cx);
        options.setIntroductionType("js shell file")
               .setUTF8(true)
               .setFileAndLine(filename, 1)
               .setIsRunOnce(true)
               .setNoScriptRval(true);

        if (!JS::Compile(cx, options, file, &script))
            return false;
        MOZ_ASSERT(script);
    }

    #ifdef DEBUG
        if (dumpEntrainedVariables)
            AnalyzeEntrainedVariables(cx, script);
    #endif
    if (!compileOnly) {
        if (!JS_ExecuteScript(cx, script))
            return false;
        int64_t t2 = PRMJ_Now() - t1;
        if (printTiming)
            printf("runtime = %.3f ms\n", double(t2) / PRMJ_USEC_PER_MSEC);
    }
    return true;
}

static bool
InitModuleLoader(JSContext* cx)
{
    // Decompress and evaluate the embedded module loader source to initialize
    // the module loader for the current compartment.

    uint32_t srcLen = moduleloader::GetRawScriptsSize();
    ScopedJSFreePtr<char> src(cx->pod_malloc<char>(srcLen));
    if (!src || !DecompressString(moduleloader::compressedSources, moduleloader::GetCompressedSize(),
                                  reinterpret_cast<unsigned char*>(src.get()), srcLen))
    {
        return false;
    }

    CompileOptions options(cx);
    options.setIntroductionType("shell module loader");
    options.setFileAndLine("shell/ModuleLoader.js", 1);
    options.setSelfHostingMode(false);
    options.setCanLazilyParse(false);
    options.setVersion(JSVERSION_LATEST);
    options.werrorOption = true;
    options.strictOption = true;

    RootedValue rv(cx);
    return Evaluate(cx, options, src, srcLen, &rv);
}

static bool
GetLoaderObject(JSContext* cx, MutableHandleObject resultOut)
{
    // Look up the |Reflect.Loader| object that has been defined by the module
    // loader.

    RootedObject object(cx, cx->global());
    RootedValue value(cx);
    if (!JS_GetProperty(cx, object, "Reflect", &value) || !value.isObject())
        return false;

    object = &value.toObject();
    if (!JS_GetProperty(cx, object, "Loader", &value) || !value.isObject())
        return false;

    resultOut.set(&value.toObject());
    return true;
}

static bool
GetImportRootMethod(JSContext* cx, HandleObject loader, MutableHandleFunction resultOut)
{
    // Look up the module loader's |importRoot| method.

    RootedValue value(cx);
    if (!JS_GetProperty(cx, loader, "importRoot", &value) || !value.isObject())
        return false;

    RootedObject object(cx, &value.toObject());
    if (!object->is<JSFunction>())
        return false;

    resultOut.set(&object->as<JSFunction>());
    return true;
}

static MOZ_MUST_USE bool
RunModule(JSContext* cx, const char* filename, FILE* file, bool compileOnly)
{
    // Execute a module by calling Reflect.Loader.importRoot on the resolved
    // filename.

    RootedObject loaderObj(cx);
    if (!GetLoaderObject(cx, &loaderObj)) {
        JS_ReportErrorASCII(cx, "Failed to get Reflect.Loader");
        return false;
    }

    RootedFunction importFun(cx);
    if (!GetImportRootMethod(cx, loaderObj, &importFun)) {
        JS_ReportErrorASCII(cx, "Failed to get Reflect.Loader.importRoot method");
        return false;
    }

    RootedString path(cx, JS_NewStringCopyZ(cx, filename));
    if (!path)
        return false;

    path = ResolvePath(cx, path, RootRelative);
    if (!path)
        return false;

    JS::AutoValueArray<1> args(cx);
    args[0].setString(path);

    RootedValue value(cx);
    return JS_CallFunction(cx, loaderObj, importFun, args, &value);
}

static JSObject*
ShellGetIncumbentGlobalCallback(JSContext* cx)
{
    return JS::CurrentGlobalOrNull(cx);
}

static bool
ShellEnqueuePromiseJobCallback(JSContext* cx, JS::HandleObject job, JS::HandleObject allocationSite,
                               JS::HandleObject incumbentGlobal, void* data)
{
    ShellContext* sc = GetShellContext(cx);
    MOZ_ASSERT(job);
    return sc->jobQueue.append(job);
}

static bool
ShellStartAsyncTaskCallback(JSContext* cx, JS::AsyncTask* task)
{
    ShellContext* sc = GetShellContext(cx);
    task->user = sc;

    ExclusiveData<ShellAsyncTasks>::Guard asyncTasks = sc->asyncTasks.lock();
    asyncTasks->outstanding++;
    return true;
}

static bool
ShellFinishAsyncTaskCallback(JS::AsyncTask* task)
{
    ShellContext* sc = (ShellContext*)task->user;

    ExclusiveData<ShellAsyncTasks>::Guard asyncTasks = sc->asyncTasks.lock();
    MOZ_ASSERT(asyncTasks->outstanding > 0);
    asyncTasks->outstanding--;
    return asyncTasks->finished.append(task);
}

static void
DrainJobQueue(JSContext* cx)
{
    ShellContext* sc = GetShellContext(cx);
    if (sc->quitting || sc->drainingJobQueue)
        return;

    while (true) {
        // Wait for any outstanding async tasks to finish so that the
        // finishedAsyncTasks list is fixed.
        while (true) {
            AutoLockHelperThreadState lock;
            if (!sc->asyncTasks.lock()->outstanding)
                break;
            HelperThreadState().wait(lock, GlobalHelperThreadState::CONSUMER);
        }

        // Lock the whole time while copying back the asyncTasks finished queue
        // so that any new tasks created during finish() cannot racily join the
        // job queue.  Call finish() only thereafter, to avoid a circular mutex
        // dependency (see also bug 1297901).
        Vector<JS::AsyncTask*> finished(cx);
        {
            ExclusiveData<ShellAsyncTasks>::Guard asyncTasks = sc->asyncTasks.lock();
            finished = Move(asyncTasks->finished);
            asyncTasks->finished.clear();
        }

        for (JS::AsyncTask* task : finished)
            task->finish(cx);

        // It doesn't make sense for job queue draining to be reentrant. At the
        // same time we don't want to assert against it, because that'd make
        // drainJobQueue unsafe for fuzzers. We do want fuzzers to test this,
        // so we simply ignore nested calls of drainJobQueue.
        sc->drainingJobQueue = true;

        RootedObject job(cx);
        JS::HandleValueArray args(JS::HandleValueArray::empty());
        RootedValue rval(cx);

        // Execute jobs in a loop until we've reached the end of the queue.
        // Since executing a job can trigger enqueuing of additional jobs,
        // it's crucial to re-check the queue length during each iteration.
        for (size_t i = 0; i < sc->jobQueue.length(); i++) {
            job = sc->jobQueue[i];
            AutoCompartment ac(cx, job);
            {
                AutoReportException are(cx);
                JS::Call(cx, UndefinedHandleValue, job, args, &rval);
            }
            sc->jobQueue[i].set(nullptr);
        }
        sc->jobQueue.clear();
        sc->drainingJobQueue = false;

        // It's possible a job added an async task, and it's also possible
        // that task has already finished.
        {
            ExclusiveData<ShellAsyncTasks>::Guard asyncTasks = sc->asyncTasks.lock();
            if (asyncTasks->outstanding == 0 && asyncTasks->finished.length() == 0)
                break;
        }
    }
}

static bool
DrainJobQueue(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    DrainJobQueue(cx);

    args.rval().setUndefined();
    return true;
}

static void
ForwardingPromiseRejectionTrackerCallback(JSContext* cx, JS::HandleObject promise,
                                          PromiseRejectionHandlingState state, void* data)
{
    RootedValue callback(cx, GetShellContext(cx)->promiseRejectionTrackerCallback);
    if (callback.isNull()) {
        return;
    }

    AutoCompartment ac(cx, &callback.toObject());

    FixedInvokeArgs<2> args(cx);
    args[0].setObject(*promise);
    args[1].setInt32(static_cast<int32_t>(state));

    if (!JS_WrapValue(cx, args[0]))
        return;

    RootedValue rval(cx);
    if (!Call(cx, callback, UndefinedHandleValue, args, &rval))
        JS_ClearPendingException(cx);
}

static bool
SetPromiseRejectionTrackerCallback(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!IsCallable(args.get(0))) {
        JS_ReportErrorASCII(cx,
                            "setPromiseRejectionTrackerCallback expects a function as its sole "
                            "argument");
        return false;
    }

    GetShellContext(cx)->promiseRejectionTrackerCallback = args[0];
    JS::SetPromiseRejectionTrackerCallback(cx, ForwardingPromiseRejectionTrackerCallback);

    args.rval().setUndefined();
    return true;
}

#ifdef ENABLE_INTL_API
static bool
AddIntlExtras(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.get(0).isObject()) {
        JS_ReportErrorASCII(cx, "addIntlExtras must be passed an object");
        return false;
    }
    JS::RootedObject intl(cx, &args[0].toObject());

    static const JSFunctionSpec funcs[] = {
        JS_SELF_HOSTED_FN("getCalendarInfo", "Intl_getCalendarInfo", 1, 0),
        JS_SELF_HOSTED_FN("getLocaleInfo", "Intl_getLocaleInfo", 1, 0),
        JS_SELF_HOSTED_FN("getDisplayNames", "Intl_getDisplayNames", 2, 0),
        JS_FS_END
    };

    if (!JS_DefineFunctions(cx, intl, funcs))
        return false;

    if (!js::AddPluralRulesConstructor(cx, intl))
        return false;

    if (!js::AddMozDateTimeFormatConstructor(cx, intl))
        return false;

    args.rval().setUndefined();
    return true;
}
#endif // ENABLE_INTL_API

static bool
EvalAndPrint(JSContext* cx, const char* bytes, size_t length,
             int lineno, bool compileOnly)
{
    // Eval.
    JS::CompileOptions options(cx);
    options.setIntroductionType("js shell interactive")
           .setUTF8(true)
           .setIsRunOnce(true)
           .setFileAndLine("typein", lineno);
    RootedScript script(cx);
    if (!JS::Compile(cx, options, bytes, length, &script))
        return false;
    if (compileOnly)
        return true;
    RootedValue result(cx);
    if (!JS_ExecuteScript(cx, script, &result))
        return false;

    if (!result.isUndefined() && gOutFile->isOpen()) {
        // Print.
        RootedString str(cx);
        str = JS_ValueToSource(cx, result);
        if (!str)
            return false;

        char* utf8chars = JS_EncodeStringToUTF8(cx, str);
        if (!utf8chars)
            return false;
        fprintf(gOutFile->fp, "%s\n", utf8chars);
        JS_free(cx, utf8chars);
    }
    return true;
}

static MOZ_MUST_USE bool
ReadEvalPrintLoop(JSContext* cx, FILE* in, bool compileOnly)
{
    ShellContext* sc = GetShellContext(cx);
    int lineno = 1;
    bool hitEOF = false;

    do {
        /*
         * Accumulate lines until we get a 'compilable unit' - one that either
         * generates an error (before running out of source) or that compiles
         * cleanly.  This should be whenever we get a complete statement that
         * coincides with the end of a line.
         */
        int startline = lineno;
        typedef Vector<char, 32> CharBuffer;
        RootedObject globalLexical(cx, &cx->global()->lexicalEnvironment());
        CharBuffer buffer(cx);
        do {
            ScheduleWatchdog(cx, -1);
            sc->serviceInterrupt = false;
            errno = 0;

            char* line = GetLine(in, startline == lineno ? "js> " : "");
            if (!line) {
                if (errno) {
                    /*
                     * Use Latin1 variant here because strerror(errno)'s
                     * encoding depends on the user's C locale.
                     */
                    JS_ReportErrorLatin1(cx, "%s", strerror(errno));
                    return false;
                }
                hitEOF = true;
                break;
            }

            if (!buffer.append(line, strlen(line)) || !buffer.append('\n'))
                return false;

            lineno++;
            if (!ScheduleWatchdog(cx, sc->timeoutInterval)) {
                hitEOF = true;
                break;
            }
        } while (!JS_BufferIsCompilableUnit(cx, cx->global(), buffer.begin(), buffer.length()));

        if (hitEOF && buffer.empty())
            break;

        {
            // Report exceptions but keep going.
            AutoReportException are(cx);
            (void) EvalAndPrint(cx, buffer.begin(), buffer.length(), startline, compileOnly);
        }

        // If a let or const fail to initialize they will remain in an unusable
        // without further intervention. This call cleans up the global scope,
        // setting uninitialized lexicals to undefined so that they may still
        // be used. This behavior is _only_ acceptable in the context of the repl.
        if (JS::ForceLexicalInitialization(cx, globalLexical) && gErrFile->isOpen()) {
            fputs("Warning: According to the standard, after the above exception,\n"
                  "Warning: the global bindings should be permanently uninitialized.\n"
                  "Warning: We have non-standard-ly initialized them to `undefined`"
                  "for you.\nWarning: This nicety only happens in the JS shell.\n",
                  stderr);
        }

        DrainJobQueue(cx);

    } while (!hitEOF && !sc->quitting);

    if (gOutFile->isOpen())
        fprintf(gOutFile->fp, "\n");

    return true;
}

enum FileKind
{
    FileScript,
    FileModule
};

static void
ReportCantOpenErrorUnknownEncoding(JSContext* cx, const char* filename)
{
    /*
     * Filenames are in some random system encoding.  *Probably* it's UTF-8,
     * but no guarantees.
     *
     * strerror(errno)'s encoding, in contrast, depends on the user's C locale.
     *
     * Latin-1 is possibly wrong for both of these -- but at least if it's
     * wrong it'll produce mojibake *safely*.  Run with Latin-1 til someone
     * complains.
     */
    JS_ReportErrorNumberLatin1(cx, my_GetErrorMessage, nullptr, JSSMSG_CANT_OPEN,
                               filename, strerror(errno));
}

static MOZ_MUST_USE bool
Process(JSContext* cx, const char* filename, bool forceTTY, FileKind kind = FileScript)
{
    FILE* file;
    if (forceTTY || !filename || strcmp(filename, "-") == 0) {
        file = stdin;
    } else {
        file = fopen(filename, "rb");
        if (!file) {
            ReportCantOpenErrorUnknownEncoding(cx, filename);
            return false;
        }
    }
    AutoCloseFile autoClose(file);

    if (!forceTTY && !isatty(fileno(file))) {
        // It's not interactive - just execute it.
        if (kind == FileScript) {
            if (!RunFile(cx, filename, file, compileOnly))
                return false;
        } else {
            if (!RunModule(cx, filename, file, compileOnly))
                return false;
        }
    } else {
        // It's an interactive filehandle; drop into read-eval-print loop.
        MOZ_ASSERT(kind == FileScript);
        if (!ReadEvalPrintLoop(cx, file, compileOnly))
            return false;
    }
    return true;
}

static bool
Version(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSVersion origVersion = JS_GetVersion(cx);
    if (args.length() == 0 || args[0].isUndefined()) {
        /* Get version. */
        args.rval().setInt32(origVersion);
    } else {
        /* Set version. */
        int32_t v = -1;
        if (args[0].isInt32()) {
            v = args[0].toInt32();
        } else if (args[0].isDouble()) {
            double fv = args[0].toDouble();
            int32_t fvi;
            if (NumberEqualsInt32(fv, &fvi))
                v = fvi;
        }
        if (v < 0 || v > JSVERSION_LATEST) {
            JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS,
                                      "version");
            return false;
        }
        JS_SetVersionForCompartment(js::GetContextCompartment(cx), JSVersion(v));
        args.rval().setInt32(origVersion);
    }
    return true;
}

#ifdef XP_WIN
#  define GET_FD_FROM_FILE(a) int(_get_osfhandle(fileno(a)))
#else
#  define GET_FD_FROM_FILE(a) fileno(a)
#endif

static bool
CreateMappedArrayBuffer(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1 || args.length() > 3) {
        JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                                  args.length() < 1 ? JSSMSG_NOT_ENOUGH_ARGS : JSSMSG_TOO_MANY_ARGS,
                                  "createMappedArrayBuffer");
        return false;
    }

    RootedString rawFilenameStr(cx, JS::ToString(cx, args[0]));
    if (!rawFilenameStr)
        return false;
    // It's a little bizarre to resolve relative to the script, but for testing
    // I need a file at a known location, and the only good way I know of to do
    // that right now is to include it in the repo alongside the test script.
    // Bug 944164 would introduce an alternative.
    JSString* filenameStr = ResolvePath(cx, rawFilenameStr, ScriptRelative);
    if (!filenameStr)
        return false;
    JSAutoByteString filename(cx, filenameStr);
    if (!filename)
        return false;

    uint32_t offset = 0;
    if (args.length() >= 2) {
        if (!JS::ToUint32(cx, args[1], &offset))
            return false;
    }

    bool sizeGiven = false;
    uint32_t size;
    if (args.length() >= 3) {
        if (!JS::ToUint32(cx, args[2], &size))
            return false;
        sizeGiven = true;
        if (size == 0) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_BAD_ARRAY_LENGTH);
            return false;
        }
    }

    FILE* file = fopen(filename.ptr(), "rb");
    if (!file) {
        ReportCantOpenErrorUnknownEncoding(cx, filename.ptr());
        return false;
    }
    AutoCloseFile autoClose(file);

    if (!sizeGiven) {
        struct stat st;
        if (fstat(fileno(file), &st) < 0) {
            JS_ReportErrorASCII(cx, "Unable to stat file");
            return false;
        }
        if (off_t(offset) >= st.st_size) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                      JSMSG_ARG_INDEX_OUT_OF_RANGE, "2");
            return false;
        }
        size = st.st_size - offset;
    }

    void* contents = JS_CreateMappedArrayBufferContents(GET_FD_FROM_FILE(file), offset, size);
    if (!contents) {
        JS_ReportErrorASCII(cx, "failed to allocate mapped array buffer contents (possibly due to bad alignment)");
        return false;
    }

    RootedObject obj(cx, JS_NewMappedArrayBufferWithContents(cx, size, contents));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

#undef GET_FD_FROM_FILE

static bool
AddPromiseReactions(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() != 3) {
        JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                                  args.length() < 3 ? JSSMSG_NOT_ENOUGH_ARGS : JSSMSG_TOO_MANY_ARGS,
                                  "addPromiseReactions");
        return false;
    }

    RootedObject promise(cx);
    if (args[0].isObject())
        promise = &args[0].toObject();

    if (!promise || !JS::IsPromiseObject(promise)) {
        JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS,
                                  "addPromiseReactions");
        return false;
    }

    RootedObject onResolve(cx);
    if (args[1].isObject())
        onResolve = &args[1].toObject();

    RootedObject onReject(cx);
    if (args[2].isObject())
        onReject = &args[2].toObject();

    if (!onResolve || !onResolve->is<JSFunction>() || !onReject || !onReject->is<JSFunction>()) {
        JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS,
                                  "addPromiseReactions");
        return false;
    }

    return JS::AddPromiseReactions(cx, promise, onResolve, onReject);
}

static bool
Options(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JS::ContextOptions oldContextOptions = JS::ContextOptionsRef(cx);
    for (unsigned i = 0; i < args.length(); i++) {
        RootedString str(cx, JS::ToString(cx, args[i]));
        if (!str)
            return false;
        args[i].setString(str);

        JSAutoByteString opt;
        if (!opt.encodeUtf8(cx, str))
            return false;

        if (strcmp(opt.ptr(), "strict") == 0)
            JS::ContextOptionsRef(cx).toggleExtraWarnings();
        else if (strcmp(opt.ptr(), "werror") == 0)
            JS::ContextOptionsRef(cx).toggleWerror();
        else if (strcmp(opt.ptr(), "throw_on_asmjs_validation_failure") == 0)
            JS::ContextOptionsRef(cx).toggleThrowOnAsmJSValidationFailure();
        else if (strcmp(opt.ptr(), "strict_mode") == 0)
            JS::ContextOptionsRef(cx).toggleStrictMode();
        else {
            JS_ReportErrorUTF8(cx,
                               "unknown option name '%s'."
                               " The valid names are strict,"
                               " werror, and strict_mode.",
                               opt.ptr());
            return false;
        }
    }

    char* names = strdup("");
    bool found = false;
    if (names && oldContextOptions.extraWarnings()) {
        names = JS_sprintf_append(names, "%s%s", found ? "," : "", "strict");
        found = true;
    }
    if (names && oldContextOptions.werror()) {
        names = JS_sprintf_append(names, "%s%s", found ? "," : "", "werror");
        found = true;
    }
    if (names && oldContextOptions.throwOnAsmJSValidationFailure()) {
        names = JS_sprintf_append(names, "%s%s", found ? "," : "", "throw_on_asmjs_validation_failure");
        found = true;
    }
    if (names && oldContextOptions.strictMode()) {
        names = JS_sprintf_append(names, "%s%s", found ? "," : "", "strict_mode");
        found = true;
    }
    if (!names) {
        JS_ReportOutOfMemory(cx);
        return false;
    }

    JSString* str = JS_NewStringCopyZ(cx, names);
    free(names);
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

static bool
LoadScript(JSContext* cx, unsigned argc, Value* vp, bool scriptRelative)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedString str(cx);
    for (unsigned i = 0; i < args.length(); i++) {
        str = JS::ToString(cx, args[i]);
        if (!str) {
            JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS,
                                      "load");
            return false;
        }
        str = ResolvePath(cx, str, scriptRelative ? ScriptRelative : RootRelative);
        if (!str) {
            JS_ReportErrorASCII(cx, "unable to resolve path");
            return false;
        }
        JSAutoByteString filename(cx, str);
        if (!filename)
            return false;
        errno = 0;
        CompileOptions opts(cx);
        opts.setIntroductionType("js shell load")
            .setUTF8(true)
            .setIsRunOnce(true)
            .setNoScriptRval(true);
        RootedScript script(cx);
        RootedValue unused(cx);
        if ((compileOnly && !Compile(cx, opts, filename.ptr(), &script)) ||
            !Evaluate(cx, opts, filename.ptr(), &unused))
        {
            return false;
        }
    }

    args.rval().setUndefined();
    return true;
}

static bool
Load(JSContext* cx, unsigned argc, Value* vp)
{
    return LoadScript(cx, argc, vp, false);
}

static bool
LoadScriptRelativeToScript(JSContext* cx, unsigned argc, Value* vp)
{
    return LoadScript(cx, argc, vp, true);
}

// Populate |options| with the options given by |opts|'s properties. If we
// need to convert a filename to a C string, let fileNameBytes own the
// bytes.
static bool
ParseCompileOptions(JSContext* cx, CompileOptions& options, HandleObject opts,
                    JSAutoByteString& fileNameBytes)
{
    RootedValue v(cx);
    RootedString s(cx);

    if (!JS_GetProperty(cx, opts, "isRunOnce", &v))
        return false;
    if (!v.isUndefined())
        options.setIsRunOnce(ToBoolean(v));

    if (!JS_GetProperty(cx, opts, "noScriptRval", &v))
        return false;
    if (!v.isUndefined())
        options.setNoScriptRval(ToBoolean(v));

    if (!JS_GetProperty(cx, opts, "fileName", &v))
        return false;
    if (v.isNull()) {
        options.setFile(nullptr);
    } else if (!v.isUndefined()) {
        s = ToString(cx, v);
        if (!s)
            return false;
        char* fileName = fileNameBytes.encodeLatin1(cx, s);
        if (!fileName)
            return false;
        options.setFile(fileName);
    }

    if (!JS_GetProperty(cx, opts, "element", &v))
        return false;
    if (v.isObject())
        options.setElement(&v.toObject());

    if (!JS_GetProperty(cx, opts, "elementAttributeName", &v))
        return false;
    if (!v.isUndefined()) {
        s = ToString(cx, v);
        if (!s)
            return false;
        options.setElementAttributeName(s);
    }

    if (!JS_GetProperty(cx, opts, "lineNumber", &v))
        return false;
    if (!v.isUndefined()) {
        uint32_t u;
        if (!ToUint32(cx, v, &u))
            return false;
        options.setLine(u);
    }

    if (!JS_GetProperty(cx, opts, "columnNumber", &v))
        return false;
    if (!v.isUndefined()) {
        int32_t c;
        if (!ToInt32(cx, v, &c))
            return false;
        options.setColumn(c);
    }

    if (!JS_GetProperty(cx, opts, "sourceIsLazy", &v))
        return false;
    if (v.isBoolean())
        options.setSourceIsLazy(v.toBoolean());

    return true;
}

static void
my_LargeAllocFailCallback()
{
    JSContext* cx = TlsContext.get();
    if (!cx || cx->helperThread())
        return;

    MOZ_ASSERT(!JS::CurrentThreadIsHeapBusy());

    JS::PrepareForFullGC(cx);
    cx->runtime()->gc.gc(GC_NORMAL, JS::gcreason::SHARED_MEMORY_LIMIT);
}

static const uint32_t CacheEntry_SOURCE = 0;
static const uint32_t CacheEntry_BYTECODE = 1;

static const JSClass CacheEntry_class = {
    "CacheEntryObject", JSCLASS_HAS_RESERVED_SLOTS(2)
};

static bool
CacheEntry(JSContext* cx, unsigned argc, JS::Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() != 1 || !args[0].isString()) {
        JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS,
                                  "CacheEntry");
        return false;
    }

    RootedObject obj(cx, JS_NewObject(cx, &CacheEntry_class));
    if (!obj)
        return false;

    SetReservedSlot(obj, CacheEntry_SOURCE, args[0]);
    SetReservedSlot(obj, CacheEntry_BYTECODE, UndefinedValue());
    args.rval().setObject(*obj);
    return true;
}

static bool
CacheEntry_isCacheEntry(JSObject* cache)
{
    return JS_GetClass(cache) == &CacheEntry_class;
}

static JSString*
CacheEntry_getSource(HandleObject cache)
{
    MOZ_ASSERT(CacheEntry_isCacheEntry(cache));
    Value v = JS_GetReservedSlot(cache, CacheEntry_SOURCE);
    if (!v.isString())
        return nullptr;

    return v.toString();
}

static uint8_t*
CacheEntry_getBytecode(HandleObject cache, uint32_t* length)
{
    MOZ_ASSERT(CacheEntry_isCacheEntry(cache));
    Value v = JS_GetReservedSlot(cache, CacheEntry_BYTECODE);
    if (!v.isObject() || !v.toObject().is<ArrayBufferObject>())
        return nullptr;

    ArrayBufferObject* arrayBuffer = &v.toObject().as<ArrayBufferObject>();
    *length = arrayBuffer->byteLength();
    return arrayBuffer->dataPointer();
}

static bool
CacheEntry_setBytecode(JSContext* cx, HandleObject cache, uint8_t* buffer, uint32_t length)
{
    MOZ_ASSERT(CacheEntry_isCacheEntry(cache));

    ArrayBufferObject::BufferContents contents =
        ArrayBufferObject::BufferContents::create<ArrayBufferObject::PLAIN>(buffer);
    Rooted<ArrayBufferObject*> arrayBuffer(cx, ArrayBufferObject::create(cx, length, contents));
    if (!arrayBuffer)
        return false;

    SetReservedSlot(cache, CacheEntry_BYTECODE, ObjectValue(*arrayBuffer));
    return true;
}

static bool
ConvertTranscodeResultToJSException(JSContext* cx, JS::TranscodeResult rv)
{
    switch (rv) {
      case JS::TranscodeResult_Ok:
        return true;

      default:
        MOZ_FALLTHROUGH;
      case JS::TranscodeResult_Failure:
        MOZ_ASSERT(!cx->isExceptionPending());
        JS_ReportErrorASCII(cx, "generic warning");
        return false;
      case JS::TranscodeResult_Failure_BadBuildId:
        MOZ_ASSERT(!cx->isExceptionPending());
        JS_ReportErrorASCII(cx, "the build-id does not match");
        return false;
      case JS::TranscodeResult_Failure_RunOnceNotSupported:
        MOZ_ASSERT(!cx->isExceptionPending());
        JS_ReportErrorASCII(cx, "run-once script are not supported by XDR");
        return false;
      case JS::TranscodeResult_Failure_AsmJSNotSupported:
        MOZ_ASSERT(!cx->isExceptionPending());
        JS_ReportErrorASCII(cx, "Asm.js is not supported by XDR");
        return false;
      case JS::TranscodeResult_Failure_UnknownClassKind:
        MOZ_ASSERT(!cx->isExceptionPending());
        JS_ReportErrorASCII(cx, "Unknown class kind, go fix it.");
        return false;
      case JS::TranscodeResult_Failure_WrongCompileOption:
        MOZ_ASSERT(!cx->isExceptionPending());
        JS_ReportErrorASCII(cx, "Compile options differs from Compile options of the encoding");
        return false;
      case JS::TranscodeResult_Failure_NotInterpretedFun:
        MOZ_ASSERT(!cx->isExceptionPending());
        JS_ReportErrorASCII(cx, "Only interepreted functions are supported by XDR");
        return false;

      case JS::TranscodeResult_Throw:
        MOZ_ASSERT(cx->isExceptionPending());
        return false;
    }
}

static bool
Evaluate(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1 || args.length() > 2) {
        JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                                  args.length() < 1 ? JSSMSG_NOT_ENOUGH_ARGS : JSSMSG_TOO_MANY_ARGS,
                                  "evaluate");
        return false;
    }

    RootedString code(cx, nullptr);
    RootedObject cacheEntry(cx, nullptr);
    if (args[0].isString()) {
        code = args[0].toString();
    } else if (args[0].isObject() && CacheEntry_isCacheEntry(&args[0].toObject())) {
        cacheEntry = &args[0].toObject();
        code = CacheEntry_getSource(cacheEntry);
    }

    if (!code || (args.length() == 2 && args[1].isPrimitive())) {
        JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS, "evaluate");
        return false;
    }

    CompileOptions options(cx);
    JSAutoByteString fileNameBytes;
    RootedString displayURL(cx);
    RootedString sourceMapURL(cx);
    RootedObject global(cx, nullptr);
    bool catchTermination = false;
    bool loadBytecode = false;
    bool saveBytecode = false;
    bool saveIncrementalBytecode = false;
    bool assertEqBytecode = false;
    RootedObject callerGlobal(cx, cx->global());

    options.setIntroductionType("js shell evaluate")
           .setFileAndLine("@evaluate", 1);

    global = JS_GetGlobalForObject(cx, &args.callee());
    if (!global)
        return false;

    if (args.length() == 2) {
        RootedObject opts(cx, &args[1].toObject());
        RootedValue v(cx);

        if (!ParseCompileOptions(cx, options, opts, fileNameBytes))
            return false;

        if (!JS_GetProperty(cx, opts, "displayURL", &v))
            return false;
        if (!v.isUndefined()) {
            displayURL = ToString(cx, v);
            if (!displayURL)
                return false;
        }

        if (!JS_GetProperty(cx, opts, "sourceMapURL", &v))
            return false;
        if (!v.isUndefined()) {
            sourceMapURL = ToString(cx, v);
            if (!sourceMapURL)
                return false;
        }

        if (!JS_GetProperty(cx, opts, "global", &v))
            return false;
        if (!v.isUndefined()) {
            if (v.isObject()) {
                global = js::UncheckedUnwrap(&v.toObject());
                if (!global)
                    return false;
            }
            if (!global || !(JS_GetClass(global)->flags & JSCLASS_IS_GLOBAL)) {
                JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_UNEXPECTED_TYPE,
                                          "\"global\" passed to evaluate()", "not a global object");
                return false;
            }
        }

        if (!JS_GetProperty(cx, opts, "catchTermination", &v))
            return false;
        if (!v.isUndefined())
            catchTermination = ToBoolean(v);

        if (!JS_GetProperty(cx, opts, "loadBytecode", &v))
            return false;
        if (!v.isUndefined())
            loadBytecode = ToBoolean(v);

        if (!JS_GetProperty(cx, opts, "saveBytecode", &v))
            return false;
        if (!v.isUndefined())
            saveBytecode = ToBoolean(v);

        if (!JS_GetProperty(cx, opts, "saveIncrementalBytecode", &v))
            return false;
        if (!v.isUndefined())
            saveIncrementalBytecode = ToBoolean(v);

        if (!JS_GetProperty(cx, opts, "assertEqBytecode", &v))
            return false;
        if (!v.isUndefined())
            assertEqBytecode = ToBoolean(v);

        // We cannot load or save the bytecode if we have no object where the
        // bytecode cache is stored.
        if (loadBytecode || saveBytecode || saveIncrementalBytecode) {
            if (!cacheEntry) {
                JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS,
                                          "evaluate");
                return false;
            }
            if (saveIncrementalBytecode && saveBytecode) {
                JS_ReportErrorASCII(cx, "saveIncrementalBytecode and saveBytecode cannot be used"
                                    " at the same time.");
                return false;
            }
        }
    }

    AutoStableStringChars codeChars(cx);
    if (!codeChars.initTwoByte(cx, code))
        return false;

    JS::TranscodeBuffer loadBuffer;
    JS::TranscodeBuffer saveBuffer;

    if (loadBytecode) {
        uint32_t loadLength = 0;
        uint8_t* loadData = nullptr;
        loadData = CacheEntry_getBytecode(cacheEntry, &loadLength);
        if (!loadData)
            return false;
        if (!loadBuffer.append(loadData, loadLength)) {
            JS_ReportOutOfMemory(cx);
            return false;
        }
    }

    {
        JSAutoCompartment ac(cx, global);
        RootedScript script(cx);

        {
            if (saveBytecode) {
                if (!JS::CompartmentCreationOptionsRef(cx).cloneSingletons()) {
                    JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                                              JSSMSG_CACHE_SINGLETON_FAILED);
                    return false;
                }

                // cloneSingletons implies that singletons are used as template objects.
                MOZ_ASSERT(JS::CompartmentBehaviorsRef(cx).getSingletonsAsTemplates());
            }

            if (loadBytecode) {
                JS::TranscodeResult rv = JS::DecodeScript(cx, loadBuffer, &script);
                if (!ConvertTranscodeResultToJSException(cx, rv))
                    return false;
            } else {
                mozilla::Range<const char16_t> chars = codeChars.twoByteRange();
                (void) JS::Compile(cx, options, chars.begin().get(), chars.length(), &script);
            }

            if (!script)
                return false;
        }

        if (displayURL && !script->scriptSource()->hasDisplayURL()) {
            JSFlatString* flat = displayURL->ensureFlat(cx);
            if (!flat)
                return false;

            AutoStableStringChars chars(cx);
            if (!chars.initTwoByte(cx, flat))
                return false;

            const char16_t* durl = chars.twoByteRange().begin().get();
            if (!script->scriptSource()->setDisplayURL(cx, durl))
                return false;
        }
        if (sourceMapURL && !script->scriptSource()->hasSourceMapURL()) {
            JSFlatString* flat = sourceMapURL->ensureFlat(cx);
            if (!flat)
                return false;

            AutoStableStringChars chars(cx);
            if (!chars.initTwoByte(cx, flat))
                return false;

            const char16_t* smurl = chars.twoByteRange().begin().get();
            if (!script->scriptSource()->setSourceMapURL(cx, smurl))
                return false;
        }

        // If we want to save the bytecode incrementally, then we should
        // register ahead the fact that every JSFunction which is being
        // delazified should be encoded at the end of the delazification.
        if (saveIncrementalBytecode) {
            if (!StartIncrementalEncoding(cx, saveBuffer, script))
                return false;
        }

        if (!JS_ExecuteScript(cx, script, args.rval())) {
            if (catchTermination && !JS_IsExceptionPending(cx)) {
                JSAutoCompartment ac1(cx, callerGlobal);
                JSString* str = JS_NewStringCopyZ(cx, "terminated");
                if (!str)
                    return false;
                args.rval().setString(str);
                return true;
            }
            return false;
        }

        // Encode the bytecode after the execution of the script.
        if (saveBytecode) {
            JS::TranscodeResult rv = JS::EncodeScript(cx, saveBuffer, script);
            if (!ConvertTranscodeResultToJSException(cx, rv))
                return false;
        }

        // Serialize the encoded bytecode, recorded before the execution, into a
        // buffer which can be deserialized linearly.
        if (saveIncrementalBytecode) {
            if (!FinishIncrementalEncoding(cx, script))
                return false;
        }
    }

    if (saveBytecode || saveIncrementalBytecode) {
        // If we are both loading and saving, we assert that we are going to
        // replace the current bytecode by the same stream of bytes.
        if (loadBytecode && assertEqBytecode) {
            if (saveBuffer.length() != loadBuffer.length()) {
                char loadLengthStr[16];
                SprintfLiteral(loadLengthStr, "%" PRIuSIZE, loadBuffer.length());
                char saveLengthStr[16];
                SprintfLiteral(saveLengthStr,"%" PRIuSIZE, saveBuffer.length());

                JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr, JSSMSG_CACHE_EQ_SIZE_FAILED,
                                          loadLengthStr, saveLengthStr);
                return false;
            }

            if (!PodEqual(loadBuffer.begin(), saveBuffer.begin(), loadBuffer.length())) {
                JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                                          JSSMSG_CACHE_EQ_CONTENT_FAILED);
                return false;
            }
        }

        size_t saveLength = saveBuffer.length();
        if (saveLength >= INT32_MAX) {
            JS_ReportErrorASCII(cx, "Cannot save large cache entry content");
            return false;
        }
        uint8_t* saveData = saveBuffer.extractOrCopyRawBuffer();
        if (!CacheEntry_setBytecode(cx, cacheEntry, saveData, saveLength)) {
            js_free(saveData);
            return false;
        }
    }

    return JS_WrapValue(cx, args.rval());
}

JSString*
js::shell::FileAsString(JSContext* cx, JS::HandleString pathnameStr)
{
    JSAutoByteString pathname(cx, pathnameStr);
    if (!pathname)
        return nullptr;

    FILE* file;

    file = fopen(pathname.ptr(), "rb");
    if (!file) {
        ReportCantOpenErrorUnknownEncoding(cx, pathname.ptr());
        return nullptr;
    }

    AutoCloseFile autoClose(file);

    if (fseek(file, 0, SEEK_END) != 0) {
        pathname.clear();
        if (!pathname.encodeUtf8(cx, pathnameStr))
            return nullptr;
        JS_ReportErrorUTF8(cx, "can't seek end of %s", pathname.ptr());
        return nullptr;
    }

    size_t len = ftell(file);
    if (fseek(file, 0, SEEK_SET) != 0) {
        pathname.clear();
        if (!pathname.encodeUtf8(cx, pathnameStr))
            return nullptr;
        JS_ReportErrorUTF8(cx, "can't seek start of %s", pathname.ptr());
        return nullptr;
    }

    UniqueChars buf(static_cast<char*>(js_malloc(len + 1)));
    if (!buf)
        return nullptr;

    size_t cc = fread(buf.get(), 1, len, file);
    if (cc != len) {
        if (ptrdiff_t(cc) < 0) {
            ReportCantOpenErrorUnknownEncoding(cx, pathname.ptr());
        } else {
            pathname.clear();
            if (!pathname.encodeUtf8(cx, pathnameStr))
                return nullptr;
            JS_ReportErrorUTF8(cx, "can't read %s: short read", pathname.ptr());
        }
        return nullptr;
    }

    UniqueTwoByteChars ucbuf(
        JS::LossyUTF8CharsToNewTwoByteCharsZ(cx, JS::UTF8Chars(buf.get(), len), &len).get()
    );
    if (!ucbuf) {
        pathname.clear();
        if (!pathname.encodeUtf8(cx, pathnameStr))
            return nullptr;
        JS_ReportErrorUTF8(cx, "Invalid UTF-8 in file '%s'", pathname.ptr());
        return nullptr;
    }

    return JS_NewUCStringCopyN(cx, ucbuf.get(), len);
}

/*
 * Function to run scripts and return compilation + execution time. Semantics
 * are closely modelled after the equivalent function in WebKit, as this is used
 * to produce benchmark timings by SunSpider.
 */
static bool
Run(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 1) {
        JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS, "run");
        return false;
    }

    RootedString str(cx, JS::ToString(cx, args[0]));
    if (!str)
        return false;
    args[0].setString(str);

    str = FileAsString(cx, str);
    if (!str)
        return false;

    AutoStableStringChars chars(cx);
    if (!chars.initTwoByte(cx, str))
        return false;

    const char16_t* ucbuf = chars.twoByteRange().begin().get();
    size_t buflen = str->length();

    RootedScript script(cx);
    int64_t startClock = PRMJ_Now();
    {
        /* FIXME: This should use UTF-8 (bug 987069). */
        JSAutoByteString filename(cx, str);
        if (!filename)
            return false;

        JS::CompileOptions options(cx);
        options.setIntroductionType("js shell run")
               .setFileAndLine(filename.ptr(), 1)
               .setIsRunOnce(true)
               .setNoScriptRval(true);
        if (!JS_CompileUCScript(cx, ucbuf, buflen, options, &script))
            return false;
    }

    if (!JS_ExecuteScript(cx, script))
        return false;

    int64_t endClock = PRMJ_Now();

    args.rval().setDouble((endClock - startClock) / double(PRMJ_USEC_PER_MSEC));
    return true;
}

/*
 * function readline()
 * Provides a hook for scripts to read a line from stdin.
 */
static bool
ReadLine(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

#define BUFSIZE 256
    FILE* from = stdin;
    size_t buflength = 0;
    size_t bufsize = BUFSIZE;
    char* buf = (char*) JS_malloc(cx, bufsize);
    if (!buf)
        return false;

    bool sawNewline = false;
    size_t gotlength;
    while ((gotlength = js_fgets(buf + buflength, bufsize - buflength, from)) > 0) {
        buflength += gotlength;

        /* Are we done? */
        if (buf[buflength - 1] == '\n') {
            buf[buflength - 1] = '\0';
            sawNewline = true;
            break;
        } else if (buflength < bufsize - 1) {
            break;
        }

        /* Else, grow our buffer for another pass. */
        char* tmp;
        bufsize *= 2;
        if (bufsize > buflength) {
            tmp = static_cast<char*>(JS_realloc(cx, buf, bufsize / 2, bufsize));
        } else {
            JS_ReportOutOfMemory(cx);
            tmp = nullptr;
        }

        if (!tmp) {
            JS_free(cx, buf);
            return false;
        }

        buf = tmp;
    }

    /* Treat the empty string specially. */
    if (buflength == 0) {
        args.rval().set(feof(from) ? NullValue() : JS_GetEmptyStringValue(cx));
        JS_free(cx, buf);
        return true;
    }

    /* Shrink the buffer to the real size. */
    char* tmp = static_cast<char*>(JS_realloc(cx, buf, bufsize, buflength));
    if (!tmp) {
        JS_free(cx, buf);
        return false;
    }

    buf = tmp;

    /*
     * Turn buf into a JSString. Note that buflength includes the trailing null
     * character.
     */
    JSString* str = JS_NewStringCopyN(cx, buf, sawNewline ? buflength - 1 : buflength);
    JS_free(cx, buf);
    if (!str)
        return false;

    args.rval().setString(str);
    return true;
}

/*
 * function readlineBuf()
 * Provides a hook for scripts to emulate readline() using a string object.
 */
static bool
ReadLineBuf(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    ShellContext* sc = GetShellContext(cx);

    if (!args.length()) {
        if (!sc->readLineBuf) {
            JS_ReportErrorASCII(cx, "No source buffer set. You must initially "
                                "call readlineBuf with an argument.");
            return false;
        }

        char* currentBuf = sc->readLineBuf.get() + sc->readLineBufPos;
        size_t buflen = strlen(currentBuf);

        if (!buflen) {
            args.rval().setNull();
            return true;
        }

        size_t len = 0;
        while(len < buflen) {
            if (currentBuf[len] == '\n')
                break;
            len++;
        }

        JSString* str = JS_NewStringCopyN(cx, currentBuf, len);
        if (!str)
            return false;

        if (currentBuf[len] == '\0')
            sc->readLineBufPos += len;
        else
            sc->readLineBufPos += len + 1;

        args.rval().setString(str);
        return true;
    }

    if (args.length() == 1) {
        if (sc->readLineBuf)
            sc->readLineBuf.reset();

        RootedString str(cx, JS::ToString(cx, args[0]));
        if (!str)
            return false;
        sc->readLineBuf = UniqueChars(JS_EncodeStringToUTF8(cx, str));
        if (!sc->readLineBuf)
            return false;

        sc->readLineBufPos = 0;
        return true;
    }

    JS_ReportErrorASCII(cx, "Must specify at most one argument");
    return false;
}

static bool
PutStr(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() != 0) {
        if (!gOutFile->isOpen()) {
            JS_ReportErrorASCII(cx, "output file is closed");
            return false;
        }

        RootedString str(cx, JS::ToString(cx, args[0]));
        if (!str)
            return false;
        char* bytes = JS_EncodeStringToUTF8(cx, str);
        if (!bytes)
            return false;
        fputs(bytes, gOutFile->fp);
        JS_free(cx, bytes);
        fflush(gOutFile->fp);
    }

    args.rval().setUndefined();
    return true;
}

static bool
Now(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    double now = PRMJ_Now() / double(PRMJ_USEC_PER_MSEC);
    args.rval().setDouble(now);
    return true;
}

static bool
PrintInternal(JSContext* cx, const CallArgs& args, RCFile* file)
{
    if (!file->isOpen()) {
        JS_ReportErrorASCII(cx, "output file is closed");
        return false;
    }

    for (unsigned i = 0; i < args.length(); i++) {
        RootedString str(cx, JS::ToString(cx, args[i]));
        if (!str)
            return false;
        char* bytes = JS_EncodeStringToUTF8(cx, str);
        if (!bytes)
            return false;
        fprintf(file->fp, "%s%s", i ? " " : "", bytes);
        JS_free(cx, bytes);
    }

    fputc('\n', file->fp);
    fflush(file->fp);

    args.rval().setUndefined();
    return true;
}

static bool
Print(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return PrintInternal(cx, args, gOutFile);
}

static bool
PrintErr(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return PrintInternal(cx, args, gErrFile);
}

static bool
Help(JSContext* cx, unsigned argc, Value* vp);

static bool
Quit(JSContext* cx, unsigned argc, Value* vp)
{
    ShellContext* sc = GetShellContext(cx);

#ifdef JS_MORE_DETERMINISTIC
    // Print a message to stderr in more-deterministic builds to help jsfunfuzz
    // find uncatchable-exception bugs.
    fprintf(stderr, "quit called\n");
#endif

    CallArgs args = CallArgsFromVp(argc, vp);
    int32_t code;
    if (!ToInt32(cx, args.get(0), &code))
        return false;

    // The fuzzers check the shell's exit code and assume a value >= 128 means
    // the process crashed (for instance, SIGSEGV will result in code 139). On
    // POSIX platforms, the exit code is 8-bit and negative values can also
    // result in an exit code >= 128. We restrict the value to range [0, 127] to
    // avoid false positives.
    if (code < 0 || code >= 128) {
        JS_ReportErrorASCII(cx, "quit exit code should be in range 0-127");
        return false;
    }

    sc->exitCode = code;
    sc->quitting = true;
    return false;
}

static bool
StartTimingMutator(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() > 0) {
        JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr, JSSMSG_TOO_MANY_ARGS,
                                  "startTimingMutator");
        return false;
    }

    if (!cx->runtime()->gc.stats().startTimingMutator()) {
        JS_ReportErrorASCII(cx, "StartTimingMutator should only be called from outside of GC");
        return false;
    }

    args.rval().setUndefined();
    return true;
}

static bool
StopTimingMutator(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() > 0) {
        JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr, JSSMSG_TOO_MANY_ARGS,
                                  "stopTimingMutator");
        return false;
    }

    double mutator_ms, gc_ms;
    if (!cx->runtime()->gc.stats().stopTimingMutator(mutator_ms, gc_ms)) {
        JS_ReportErrorASCII(cx, "stopTimingMutator called when not timing the mutator");
        return false;
    }
    double total_ms = mutator_ms + gc_ms;
    if (total_ms > 0 && gOutFile->isOpen()) {
        fprintf(gOutFile->fp, "Mutator: %.3fms (%.1f%%), GC: %.3fms (%.1f%%)\n",
                mutator_ms, mutator_ms / total_ms * 100.0, gc_ms, gc_ms / total_ms * 100.0);
    }

    args.rval().setUndefined();
    return true;
}

static const char*
ToSource(JSContext* cx, MutableHandleValue vp, JSAutoByteString* bytes)
{
    JSString* str = JS_ValueToSource(cx, vp);
    if (str) {
        vp.setString(str);
        if (bytes->encodeLatin1(cx, str))
            return bytes->ptr();
    }
    JS_ClearPendingException(cx);
    return "<<error converting value to string>>";
}

static bool
AssertEq(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (!(args.length() == 2 || (args.length() == 3 && args[2].isString()))) {
        JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                                  (args.length() < 2)
                                  ? JSSMSG_NOT_ENOUGH_ARGS
                                  : (args.length() == 3)
                                  ? JSSMSG_INVALID_ARGS
                                  : JSSMSG_TOO_MANY_ARGS,
                                  "assertEq");
        return false;
    }

    bool same;
    if (!JS_SameValue(cx, args[0], args[1], &same))
        return false;
    if (!same) {
        JSAutoByteString bytes0, bytes1;
        const char* actual = ToSource(cx, args[0], &bytes0);
        const char* expected = ToSource(cx, args[1], &bytes1);
        if (args.length() == 2) {
            JS_ReportErrorNumberLatin1(cx, my_GetErrorMessage, nullptr, JSSMSG_ASSERT_EQ_FAILED,
                                       actual, expected);
        } else {
            JSAutoByteString bytes2(cx, args[2].toString());
            if (!bytes2)
                return false;
            JS_ReportErrorNumberLatin1(cx, my_GetErrorMessage, nullptr,
                                       JSSMSG_ASSERT_EQ_FAILED_MSG,
                                       actual, expected, bytes2.ptr());
        }
        return false;
    }
    args.rval().setUndefined();
    return true;
}

static JSScript*
ValueToScript(JSContext* cx, HandleValue v, JSFunction** funp = nullptr)
{
    if (v.isString()) {
        // To convert a string to a script, compile it. Parse it as an ES6 Program.
        RootedLinearString linearStr(cx, StringToLinearString(cx, v.toString()));
        if (!linearStr)
            return nullptr;
        size_t len = GetLinearStringLength(linearStr);
        AutoStableStringChars linearChars(cx);
        if (!linearChars.initTwoByte(cx, linearStr))
            return nullptr;
        const char16_t* chars = linearChars.twoByteRange().begin().get();

        RootedScript script(cx);
        CompileOptions options(cx);
        if (!JS::Compile(cx, options, chars, len, &script))
            return nullptr;
        return script;
    }

    RootedFunction fun(cx, JS_ValueToFunction(cx, v));
    if (!fun)
        return nullptr;

    // Unwrap bound functions.
    while (fun->isBoundFunction()) {
        JSObject* target = fun->getBoundFunctionTarget();
        if (target && target->is<JSFunction>())
            fun = &target->as<JSFunction>();
        else
            break;
    }

    // Get unwrapped async function.
    if (IsWrappedAsyncFunction(fun))
        fun = GetUnwrappedAsyncFunction(fun);
    if (IsWrappedAsyncGenerator(fun))
        fun = GetUnwrappedAsyncGenerator(fun);

    if (!fun->isInterpreted()) {
        JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr, JSSMSG_SCRIPTS_ONLY);
        return nullptr;
    }

    JSScript* script = JSFunction::getOrCreateScript(cx, fun);
    if (!script)
        return nullptr;

    if (funp)
        *funp = fun;

    return script;
}

static JSScript*
GetTopScript(JSContext* cx)
{
    NonBuiltinScriptFrameIter iter(cx);
    return iter.done() ? nullptr : iter.script();
}

static bool
GetScriptAndPCArgs(JSContext* cx, CallArgs& args, MutableHandleScript scriptp,
                   int32_t* ip)
{
    RootedScript script(cx, GetTopScript(cx));
    *ip = 0;
    if (!args.get(0).isUndefined()) {
        HandleValue v = args[0];
        unsigned intarg = 0;
        if (v.isObject() &&
            JS_GetClass(&v.toObject()) == Jsvalify(&JSFunction::class_)) {
            script = ValueToScript(cx, v);
            if (!script)
                return false;
            intarg++;
        }
        if (!args.get(intarg).isUndefined()) {
            if (!JS::ToInt32(cx, args[intarg], ip))
                return false;
            if ((uint32_t)*ip >= script->length()) {
                JS_ReportErrorASCII(cx, "Invalid PC");
                return false;
            }
        }
    }

    scriptp.set(script);

    return true;
}

static bool
LineToPC(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() == 0) {
        JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr, JSSMSG_LINE2PC_USAGE);
        return false;
    }

    RootedScript script(cx, GetTopScript(cx));
    int32_t lineArg = 0;
    if (args[0].isObject() && args[0].toObject().is<JSFunction>()) {
        script = ValueToScript(cx, args[0]);
        if (!script)
            return false;
        lineArg++;
    }

    uint32_t lineno;
    if (!ToUint32(cx, args.get(lineArg), &lineno))
         return false;

    jsbytecode* pc = LineNumberToPC(script, lineno);
    if (!pc)
        return false;
    args.rval().setInt32(script->pcToOffset(pc));
    return true;
}

static bool
PCToLine(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedScript script(cx);
    int32_t i;
    unsigned lineno;

    if (!GetScriptAndPCArgs(cx, args, &script, &i))
        return false;
    lineno = PCToLineNumber(script, script->offsetToPC(i));
    if (!lineno)
        return false;
    args.rval().setInt32(lineno);
    return true;
}

#ifdef DEBUG

static void
UpdateSwitchTableBounds(JSContext* cx, HandleScript script, unsigned offset,
                        unsigned* start, unsigned* end)
{
    jsbytecode* pc;
    JSOp op;
    ptrdiff_t jmplen;
    int32_t low, high, n;

    pc = script->offsetToPC(offset);
    op = JSOp(*pc);
    switch (op) {
      case JSOP_TABLESWITCH:
        jmplen = JUMP_OFFSET_LEN;
        pc += jmplen;
        low = GET_JUMP_OFFSET(pc);
        pc += JUMP_OFFSET_LEN;
        high = GET_JUMP_OFFSET(pc);
        pc += JUMP_OFFSET_LEN;
        n = high - low + 1;
        break;

      default:
        /* [condswitch] switch does not have any jump or lookup tables. */
        MOZ_ASSERT(op == JSOP_CONDSWITCH);
        return;
    }

    *start = script->pcToOffset(pc);
    *end = *start + (unsigned)(n * jmplen);
}

static MOZ_MUST_USE bool
SrcNotes(JSContext* cx, HandleScript script, Sprinter* sp)
{
    if (!sp->put("\nSource notes:\n") ||
        !sp->jsprintf("%4s %4s %5s %6s %-8s %s\n",
                      "ofs", "line", "pc", "delta", "desc", "args") ||
        !sp->put("---- ---- ----- ------ -------- ------\n"))
    {
        return false;
    }

    unsigned offset = 0;
    unsigned colspan = 0;
    unsigned lineno = script->lineno();
    jssrcnote* notes = script->notes();
    unsigned switchTableEnd = 0, switchTableStart = 0;
    for (jssrcnote* sn = notes; !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn)) {
        unsigned delta = SN_DELTA(sn);
        offset += delta;
        SrcNoteType type = (SrcNoteType) SN_TYPE(sn);
        const char* name = js_SrcNoteSpec[type].name;
        if (!sp->jsprintf("%3u: %4u %5u [%4u] %-8s",
                          unsigned(sn - notes), lineno, offset, delta, name))
        {
            return false;
        }

        switch (type) {
          case SRC_NULL:
          case SRC_IF:
          case SRC_CONTINUE:
          case SRC_BREAK:
          case SRC_BREAK2LABEL:
          case SRC_SWITCHBREAK:
          case SRC_ASSIGNOP:
          case SRC_XDELTA:
            break;

          case SRC_COLSPAN:
            colspan = SN_OFFSET_TO_COLSPAN(GetSrcNoteOffset(sn, 0));
            if (!sp->jsprintf("%d", colspan))
                return false;
            break;

          case SRC_SETLINE:
            lineno = GetSrcNoteOffset(sn, 0);
            if (!sp->jsprintf(" lineno %u", lineno))
                return false;
            break;

          case SRC_NEWLINE:
            ++lineno;
            break;

          case SRC_FOR:
            if (!sp->jsprintf(" cond %u update %u tail %u",
                              unsigned(GetSrcNoteOffset(sn, 0)),
                              unsigned(GetSrcNoteOffset(sn, 1)),
                              unsigned(GetSrcNoteOffset(sn, 2))))
            {
                return false;
            }
            break;

          case SRC_IF_ELSE:
            if (!sp->jsprintf(" else %u", unsigned(GetSrcNoteOffset(sn, 0))))
                return false;
            break;

          case SRC_FOR_IN:
          case SRC_FOR_OF:
            if (!sp->jsprintf(" closingjump %u", unsigned(GetSrcNoteOffset(sn, 0))))
                return false;
            break;

          case SRC_COND:
          case SRC_WHILE:
          case SRC_NEXTCASE:
            if (!sp->jsprintf(" offset %u", unsigned(GetSrcNoteOffset(sn, 0))))
                return false;
            break;

          case SRC_TABLESWITCH: {
            JSOp op = JSOp(script->code()[offset]);
            MOZ_ASSERT(op == JSOP_TABLESWITCH);
            if (!sp->jsprintf(" length %u", unsigned(GetSrcNoteOffset(sn, 0))))
                return false;
            UpdateSwitchTableBounds(cx, script, offset,
                                    &switchTableStart, &switchTableEnd);
            break;
          }
          case SRC_CONDSWITCH: {
            JSOp op = JSOp(script->code()[offset]);
            MOZ_ASSERT(op == JSOP_CONDSWITCH);
            if (!sp->jsprintf(" length %u", unsigned(GetSrcNoteOffset(sn, 0))))
                return false;
            if (unsigned caseOff = (unsigned) GetSrcNoteOffset(sn, 1)) {
                if (!sp->jsprintf(" first case offset %u", caseOff))
                    return false;
            }
            UpdateSwitchTableBounds(cx, script, offset,
                                    &switchTableStart, &switchTableEnd);
            break;
          }

          case SRC_TRY:
            MOZ_ASSERT(JSOp(script->code()[offset]) == JSOP_TRY);
            if (!sp->jsprintf(" offset to jump %u", unsigned(GetSrcNoteOffset(sn, 0))))
                return false;
            break;

          case SRC_CLASS_SPAN: {
            unsigned startOffset = GetSrcNoteOffset(sn, 0);
            unsigned endOffset = GetSrcNoteOffset(sn, 1);
            if (!sp->jsprintf(" %u %u", startOffset, endOffset))
                return false;
            break;
          }

          default:
            MOZ_ASSERT_UNREACHABLE("unrecognized srcnote");
        }
        if (!sp->put("\n"))
            return false;
    }

    return true;
}

static bool
Notes(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    Sprinter sprinter(cx);
    if (!sprinter.init())
        return false;

    for (unsigned i = 0; i < args.length(); i++) {
        RootedScript script (cx, ValueToScript(cx, args[i]));
        if (!script)
            return false;

        if (!SrcNotes(cx, script, &sprinter))
            return false;
    }

    JSString* str = JS_NewStringCopyZ(cx, sprinter.string());
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

static const char*
TryNoteName(JSTryNoteKind kind)
{
    switch (kind) {
      case JSTRY_CATCH:
        return "catch";
      case JSTRY_FINALLY:
        return "finally";
      case JSTRY_FOR_IN:
        return "for-in";
      case JSTRY_FOR_OF:
        return "for-of";
      case JSTRY_LOOP:
        return "loop";
      case JSTRY_FOR_OF_ITERCLOSE:
        return "for-of-iterclose";
      case JSTRY_DESTRUCTURING_ITERCLOSE:
        return "dstr-iterclose";
    }

    MOZ_CRASH("Bad JSTryNoteKind");
}

static MOZ_MUST_USE bool
TryNotes(JSContext* cx, HandleScript script, Sprinter* sp)
{
    if (!script->hasTrynotes())
        return true;

    if (!sp->put("\nException table:\nkind               stack    start      end\n"))
        return false;

    JSTryNote* tn = script->trynotes()->vector;
    JSTryNote* tnlimit = tn + script->trynotes()->length;
    do {
        uint32_t startOff = script->pcToOffset(script->main()) + tn->start;
        if (!sp->jsprintf(" %-16s %6u %8u %8u\n",
                          TryNoteName(static_cast<JSTryNoteKind>(tn->kind)),
                          tn->stackDepth, startOff, startOff + tn->length))
        {
            return false;
        }
    } while (++tn != tnlimit);
    return true;
}

static MOZ_MUST_USE bool
ScopeNotes(JSContext* cx, HandleScript script, Sprinter* sp)
{
    if (!script->hasScopeNotes())
        return true;

    if (!sp->put("\nScope notes:\n   index   parent    start      end\n"))
        return false;

    ScopeNoteArray* notes = script->scopeNotes();
    for (uint32_t i = 0; i < notes->length; i++) {
        const ScopeNote* note = &notes->vector[i];
        if (note->index == ScopeNote::NoScopeIndex) {
            if (!sp->jsprintf("%8s ", "(none)"))
                return false;
        } else {
            if (!sp->jsprintf("%8u ", note->index))
                return false;
        }
        if (note->parent == ScopeNote::NoScopeIndex) {
            if (!sp->jsprintf("%8s ", "(none)"))
                return false;
        } else {
            if (!sp->jsprintf("%8u ", note->parent))
                return false;
        }
        if (!sp->jsprintf("%8u %8u\n", note->start, note->start + note->length))
            return false;
    }
    return true;
}

static MOZ_MUST_USE bool
DisassembleScript(JSContext* cx, HandleScript script, HandleFunction fun,
                  bool lines, bool recursive, bool sourceNotes, Sprinter* sp)
{
    if (fun) {
        if (!sp->put("flags:"))
            return false;
        if (fun->isLambda()) {
            if (!sp->put(" LAMBDA"))
                return false;
        }
        if (fun->needsCallObject()) {
            if (!sp->put(" NEEDS_CALLOBJECT"))
                return false;
        }
        if (fun->needsExtraBodyVarEnvironment()) {
            if (!sp->put(" NEEDS_EXTRABODYVARENV"))
                return false;
        }
        if (fun->needsNamedLambdaEnvironment()) {
            if (!sp->put(" NEEDS_NAMEDLAMBDAENV"))
                return false;
        }
        if (fun->isConstructor()) {
            if (!sp->put(" CONSTRUCTOR"))
                return false;
        }
        if (script->isExprBody()) {
            if (!sp->put(" EXPRESSION_CLOSURE"))
                return false;
        }
        if (fun->isSelfHostedBuiltin()) {
            if (!sp->put(" SELF_HOSTED"))
                return false;
        }
        if (fun->isArrow()) {
            if (!sp->put(" ARROW"))
                return false;
        }
        if (!sp->put("\n"))
            return false;
    }

    if (!Disassemble(cx, script, lines, sp))
        return false;
    if (sourceNotes) {
        if (!SrcNotes(cx, script, sp))
            return false;
    }
    if (!TryNotes(cx, script, sp))
        return false;
    if (!ScopeNotes(cx, script, sp))
        return false;

    if (recursive && script->hasObjects()) {
        ObjectArray* objects = script->objects();
        for (unsigned i = 0; i != objects->length; ++i) {
            JSObject* obj = objects->vector[i];
            if (obj->is<JSFunction>()) {
                if (!sp->put("\n"))
                    return false;

                RootedFunction fun(cx, &obj->as<JSFunction>());
                if (fun->isInterpreted()) {
                    RootedScript script(cx, JSFunction::getOrCreateScript(cx, fun));
                    if (script) {
                        if (!DisassembleScript(cx, script, fun, lines, recursive, sourceNotes, sp))
                            return false;
                    }
                } else {
                    if (!sp->put("[native code]\n"))
                        return false;
                }
            }
        }
    }

    return true;
}

namespace {

struct DisassembleOptionParser {
    unsigned argc;
    Value* argv;
    bool lines;
    bool recursive;
    bool sourceNotes;

    DisassembleOptionParser(unsigned argc, Value* argv)
      : argc(argc), argv(argv), lines(false), recursive(false), sourceNotes(true) {}

    bool parse(JSContext* cx) {
        /* Read options off early arguments */
        while (argc > 0 && argv[0].isString()) {
            JSString* str = argv[0].toString();
            JSFlatString* flatStr = JS_FlattenString(cx, str);
            if (!flatStr)
                return false;
            if (JS_FlatStringEqualsAscii(flatStr, "-l"))
                lines = true;
            else if (JS_FlatStringEqualsAscii(flatStr, "-r"))
                recursive = true;
            else if (JS_FlatStringEqualsAscii(flatStr, "-S"))
                sourceNotes = false;
            else
                break;
            argv++; argc--;
        }
        return true;
    }
};

} /* anonymous namespace */

static bool
DisassembleToSprinter(JSContext* cx, unsigned argc, Value* vp, Sprinter* sprinter)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    DisassembleOptionParser p(args.length(), args.array());
    if (!p.parse(cx))
        return false;

    if (p.argc == 0) {
        /* Without arguments, disassemble the current script. */
        RootedScript script(cx, GetTopScript(cx));
        if (script) {
            JSAutoCompartment ac(cx, script);
            if (!Disassemble(cx, script, p.lines, sprinter))
                return false;
            if (!SrcNotes(cx, script, sprinter))
                return false;
            if (!TryNotes(cx, script, sprinter))
                return false;
            if (!ScopeNotes(cx, script, sprinter))
                return false;
        }
    } else {
        for (unsigned i = 0; i < p.argc; i++) {
            RootedFunction fun(cx);
            RootedScript script(cx);
            RootedValue value(cx, p.argv[i]);
            if (value.isObject() && value.toObject().is<ModuleObject>())
                script = value.toObject().as<ModuleObject>().script();
            else
                script = ValueToScript(cx, value, fun.address());
            if (!script)
                return false;
            if (!DisassembleScript(cx, script, fun, p.lines, p.recursive, p.sourceNotes, sprinter))
                return false;
        }
    }

    return !sprinter->hadOutOfMemory();
}

static bool
DisassembleToString(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    Sprinter sprinter(cx);
    if (!sprinter.init())
        return false;
    if (!DisassembleToSprinter(cx, args.length(), vp, &sprinter))
        return false;

    JSString* str = JS_NewStringCopyZ(cx, sprinter.string());
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

static bool
Disassemble(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!gOutFile->isOpen()) {
        JS_ReportErrorASCII(cx, "output file is closed");
        return false;
    }

    Sprinter sprinter(cx);
    if (!sprinter.init())
        return false;
    if (!DisassembleToSprinter(cx, args.length(), vp, &sprinter))
        return false;

    fprintf(gOutFile->fp, "%s\n", sprinter.string());
    args.rval().setUndefined();
    return true;
}

static bool
DisassFile(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!gOutFile->isOpen()) {
        JS_ReportErrorASCII(cx, "output file is closed");
        return false;
    }

    /* Support extra options at the start, just like Disassemble. */
    DisassembleOptionParser p(args.length(), args.array());
    if (!p.parse(cx))
        return false;

    if (!p.argc) {
        args.rval().setUndefined();
        return true;
    }

    // We should change DisassembleOptionParser to store CallArgs.
    JSString* str = JS::ToString(cx, HandleValue::fromMarkedLocation(&p.argv[0]));
    if (!str)
        return false;
    JSAutoByteString filename(cx, str);
    if (!filename)
        return false;
    RootedScript script(cx);

    {
        CompileOptions options(cx);
        options.setIntroductionType("js shell disFile")
               .setUTF8(true)
               .setFileAndLine(filename.ptr(), 1)
               .setIsRunOnce(true)
               .setNoScriptRval(true);

        if (!JS::Compile(cx, options, filename.ptr(), &script))
            return false;
    }

    Sprinter sprinter(cx);
    if (!sprinter.init())
        return false;
    bool ok = DisassembleScript(cx, script, nullptr, p.lines, p.recursive, p.sourceNotes, &sprinter);
    if (ok)
        fprintf(gOutFile->fp, "%s\n", sprinter.string());
    if (!ok)
        return false;

    args.rval().setUndefined();
    return true;
}

static bool
DisassWithSrc(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!gOutFile->isOpen()) {
        JS_ReportErrorASCII(cx, "output file is closed");
        return false;
    }

    const size_t lineBufLen = 512;
    unsigned len, line1, line2, bupline;
    char linebuf[lineBufLen];
    static const char sep[] = ";-------------------------";

    RootedScript script(cx);
    for (unsigned i = 0; i < args.length(); i++) {
        script = ValueToScript(cx, args[i]);
        if (!script)
           return false;

        if (!script->filename()) {
            JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr, JSSMSG_FILE_SCRIPTS_ONLY);
            return false;
        }

        FILE* file = fopen(script->filename(), "rb");
        if (!file) {
            /* FIXME: script->filename() should become UTF-8 (bug 987069). */
            ReportCantOpenErrorUnknownEncoding(cx, script->filename());
            return false;
        }
        auto closeFile = MakeScopeExit([file]() { fclose(file); });

        jsbytecode* pc = script->code();
        jsbytecode* end = script->codeEnd();

        Sprinter sprinter(cx);
        if (!sprinter.init())
            return false;

        /* burn the leading lines */
        line2 = PCToLineNumber(script, pc);
        for (line1 = 0; line1 < line2 - 1; line1++) {
            char* tmp = fgets(linebuf, lineBufLen, file);
            if (!tmp) {
                /* FIXME: This should use UTF-8 (bug 987069). */
                JS_ReportErrorLatin1(cx, "failed to read %s fully", script->filename());
                return false;
            }
        }

        bupline = 0;
        while (pc < end) {
            line2 = PCToLineNumber(script, pc);

            if (line2 < line1) {
                if (bupline != line2) {
                    bupline = line2;
                    if (!sprinter.jsprintf("%s %3u: BACKUP\n", sep, line2))
                        return false;
                }
            } else {
                if (bupline && line1 == line2) {
                    if (!sprinter.jsprintf("%s %3u: RESTORE\n", sep, line2))
                        return false;
                }
                bupline = 0;
                while (line1 < line2) {
                    if (!fgets(linebuf, lineBufLen, file)) {
                        /*
                         * FIXME: script->filename() should become UTF-8
                         *        (bug 987069).
                         */
                        JS_ReportErrorNumberLatin1(cx, my_GetErrorMessage, nullptr,
                                                   JSSMSG_UNEXPECTED_EOF,
                                                   script->filename());
                        return false;
                    }
                    line1++;
                    if (!sprinter.jsprintf("%s %3u: %s", sep, line1, linebuf))
                        return false;
                }
            }

            len = Disassemble1(cx, script, pc, script->pcToOffset(pc), true, &sprinter);
            if (!len)
                return false;

            pc += len;
        }

        fprintf(gOutFile->fp, "%s\n", sprinter.string());
    }

    args.rval().setUndefined();
    return true;
}

#endif /* DEBUG */

/* Pretend we can always preserve wrappers for dummy DOM objects. */
static bool
DummyPreserveWrapperCallback(JSContext* cx, JSObject* obj)
{
    return true;
}

static bool
Intern(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSString* str = JS::ToString(cx, args.get(0));
    if (!str)
        return false;

    AutoStableStringChars strChars(cx);
    if (!strChars.initTwoByte(cx, str))
        return false;

    mozilla::Range<const char16_t> chars = strChars.twoByteRange();

    if (!JS_AtomizeAndPinUCStringN(cx, chars.begin().get(), chars.length()))
        return false;

    args.rval().setUndefined();
    return true;
}

static bool
Clone(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject parent(cx);
    RootedObject funobj(cx);

    if (!args.length()) {
        JS_ReportErrorASCII(cx, "Invalid arguments to clone");
        return false;
    }

    {
        Maybe<JSAutoCompartment> ac;
        RootedObject obj(cx, args[0].isPrimitive() ? nullptr : &args[0].toObject());

        if (obj && obj->is<CrossCompartmentWrapperObject>()) {
            obj = UncheckedUnwrap(obj);
            ac.emplace(cx, obj);
            args[0].setObject(*obj);
        }
        if (obj && obj->is<JSFunction>()) {
            funobj = obj;
        } else {
            JSFunction* fun = JS_ValueToFunction(cx, args[0]);
            if (!fun)
                return false;
            funobj = JS_GetFunctionObject(fun);
        }
    }

    if (args.length() > 1) {
        if (!JS_ValueToObject(cx, args[1], &parent))
            return false;
    } else {
        parent = js::GetGlobalForObjectCrossCompartment(&args.callee());
    }

    // Should it worry us that we might be getting with wrappers
    // around with wrappers here?
    JS::AutoObjectVector scopeChain(cx);
    if (!parent->is<GlobalObject>() && !scopeChain.append(parent))
        return false;
    JSObject* clone = JS::CloneFunctionObject(cx, funobj, scopeChain);
    if (!clone)
        return false;
    args.rval().setObject(*clone);
    return true;
}

static bool
Crash(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() == 0)
        MOZ_CRASH("forced crash");
    RootedString message(cx, JS::ToString(cx, args[0]));
    if (!message)
        return false;
    char* utf8chars = JS_EncodeStringToUTF8(cx, message);
    if (!utf8chars)
        return false;
#ifndef DEBUG
    MOZ_ReportCrash(utf8chars, __FILE__, __LINE__);
#endif
    MOZ_CRASH_UNSAFE_OOL(utf8chars);
}

static bool
GetSLX(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedScript script(cx);

    script = ValueToScript(cx, args.get(0));
    if (!script)
        return false;
    args.rval().setInt32(GetScriptLineExtent(script));
    return true;
}

static bool
ThrowError(JSContext* cx, unsigned argc, Value* vp)
{
    JS_ReportErrorASCII(cx, "This is an error");
    return false;
}

#define LAZY_STANDARD_CLASSES

/* A class for easily testing the inner/outer object callbacks. */
typedef struct ComplexObject {
    bool isInner;
    bool frozen;
    JSObject* inner;
    JSObject* outer;
} ComplexObject;

static bool
sandbox_enumerate(JSContext* cx, HandleObject obj)
{
    RootedValue v(cx);

    if (!JS_GetProperty(cx, obj, "lazy", &v))
        return false;

    if (!ToBoolean(v))
        return true;

    return JS_EnumerateStandardClasses(cx, obj);
}

static bool
sandbox_resolve(JSContext* cx, HandleObject obj, HandleId id, bool* resolvedp)
{
    RootedValue v(cx);
    if (!JS_GetProperty(cx, obj, "lazy", &v))
        return false;

    if (ToBoolean(v))
        return JS_ResolveStandardClass(cx, obj, id, resolvedp);
    return true;
}

static const JSClassOps sandbox_classOps = {
    nullptr, nullptr, nullptr, nullptr,
    sandbox_enumerate, sandbox_resolve,
    nullptr, nullptr,
    nullptr, nullptr, nullptr,
    JS_GlobalObjectTraceHook
};

static const JSClass sandbox_class = {
    "sandbox",
    JSCLASS_GLOBAL_FLAGS,
    &sandbox_classOps
};

static void
SetStandardCompartmentOptions(JS::CompartmentOptions& options)
{
    options.behaviors().setVersion(JSVERSION_DEFAULT);
    options.creationOptions().setSharedMemoryAndAtomicsEnabled(enableSharedMemory);
}

static JSObject*
NewSandbox(JSContext* cx, bool lazy)
{
    JS::CompartmentOptions options;
    SetStandardCompartmentOptions(options);
    RootedObject obj(cx, JS_NewGlobalObject(cx, &sandbox_class, nullptr,
                                            JS::DontFireOnNewGlobalHook, options));
    if (!obj)
        return nullptr;

    {
        JSAutoCompartment ac(cx, obj);
        if (!lazy && !JS_InitStandardClasses(cx, obj))
            return nullptr;

        RootedValue value(cx, BooleanValue(lazy));
        if (!JS_SetProperty(cx, obj, "lazy", value))
            return nullptr;

        JS_FireOnNewGlobalObject(cx, obj);
    }

    if (!cx->compartment()->wrap(cx, &obj))
        return nullptr;
    return obj;
}

static bool
EvalInContext(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.requireAtLeast(cx, "evalcx", 1))
        return false;

    RootedString str(cx, ToString(cx, args[0]));
    if (!str)
        return false;

    RootedObject sobj(cx);
    if (args.hasDefined(1)) {
        sobj = ToObject(cx, args[1]);
        if (!sobj)
            return false;
    }

    AutoStableStringChars strChars(cx);
    if (!strChars.initTwoByte(cx, str))
        return false;

    mozilla::Range<const char16_t> chars = strChars.twoByteRange();
    size_t srclen = chars.length();
    const char16_t* src = chars.begin().get();

    bool lazy = false;
    if (srclen == 4) {
        if (src[0] == 'l' && src[1] == 'a' && src[2] == 'z' && src[3] == 'y') {
            lazy = true;
            srclen = 0;
        }
    }

    if (!sobj) {
        sobj = NewSandbox(cx, lazy);
        if (!sobj)
            return false;
    }

    if (srclen == 0) {
        args.rval().setObject(*sobj);
        return true;
    }

    JS::AutoFilename filename;
    unsigned lineno;

    DescribeScriptedCaller(cx, &filename, &lineno);
    {
        Maybe<JSAutoCompartment> ac;
        unsigned flags;
        JSObject* unwrapped = UncheckedUnwrap(sobj, true, &flags);
        if (flags & Wrapper::CROSS_COMPARTMENT) {
            sobj = unwrapped;
            ac.emplace(cx, sobj);
        }

        sobj = ToWindowIfWindowProxy(sobj);

        if (!(sobj->getClass()->flags & JSCLASS_IS_GLOBAL)) {
            JS_ReportErrorASCII(cx, "Invalid scope argument to evalcx");
            return false;
        }
        JS::CompileOptions opts(cx);
        opts.setFileAndLine(filename.get(), lineno);
        if (!JS::Evaluate(cx, opts, src, srclen, args.rval())) {
            return false;
        }
    }

    if (!cx->compartment()->wrap(cx, args.rval()))
        return false;

    return true;
}

struct CooperationState
{
    CooperationState()
      : lock(mutexid::ShellThreadCooperation)
      , idle(false)
      , numThreads(0)
      , yieldCount(0)
      , singleThreaded(false)
    {}

    Mutex lock;
    ConditionVariable cvar;
    bool idle;
    size_t numThreads;
    uint64_t yieldCount;
    bool singleThreaded;
};
static CooperationState* cooperationState = nullptr;

static void
CooperativeBeginWait(JSContext* cx)
{
    MOZ_ASSERT(cx == TlsContext.get());
    JS_YieldCooperativeContext(cx);
}

static void
CooperativeEndWait(JSContext* cx)
{
    MOZ_ASSERT(cx == TlsContext.get());
    LockGuard<Mutex> lock(cooperationState->lock);

    cooperationState->cvar.wait(lock, [&] { return cooperationState->idle; });

    JS_ResumeCooperativeContext(cx);
    cooperationState->idle = false;
    cooperationState->yieldCount++;
    cooperationState->cvar.notify_all();
}

static void
CooperativeYield()
{
    LockGuard<Mutex> lock(cooperationState->lock);
    MOZ_ASSERT(!cooperationState->idle);
    cooperationState->idle = true;
    cooperationState->cvar.notify_all();

    // Wait until another thread takes over control before returning, if there
    // is another thread to do so.
    if (cooperationState->numThreads) {
        uint64_t count = cooperationState->yieldCount;
        cooperationState->cvar.wait(lock, [&] { return cooperationState->yieldCount != count; });
    }
}

static bool
CooperativeYieldThread(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!cx->runtime()->gc.canChangeActiveContext(cx)) {
        JS_ReportErrorASCII(cx, "Cooperating multithreading context switches are not currently allowed");
        return false;
    }

    if (GetShellContext(cx)->isWorker) {
        JS_ReportErrorASCII(cx, "Worker threads cannot yield");
        return false;
    }

    if (cooperationState->singleThreaded) {
        JS_ReportErrorASCII(cx, "Yielding is not allowed while single threaded");
        return false;
    }

    CooperativeBeginWait(cx);
    CooperativeYield();
    CooperativeEndWait(cx);

    args.rval().setUndefined();
    return true;
}

static void
CooperativeBeginSingleThreadedExecution(JSContext* cx)
{
    MOZ_ASSERT(!cooperationState->singleThreaded);

    // Yield until all other threads have exited any zone groups they are in.
    while (true) {
        bool done = true;
        for (ZoneGroupsIter group(cx->runtime()); !group.done(); group.next()) {
            if (!group->ownedByCurrentThread() && group->ownerContext().context())
                done = false;
        }
        if (done)
            break;
        CooperativeBeginWait(cx);
        CooperativeYield();
        CooperativeEndWait(cx);
    }

    cooperationState->singleThreaded = true;
}

static void
CooperativeEndSingleThreadedExecution(JSContext* cx)
{
    if (cooperationState)
        cooperationState->singleThreaded = false;
}

struct WorkerInput
{
    JSRuntime* parentRuntime;
    JSContext* siblingContext;
    char16_t* chars;
    size_t length;

    WorkerInput(JSRuntime* parentRuntime, char16_t* chars, size_t length)
      : parentRuntime(parentRuntime), siblingContext(nullptr), chars(chars), length(length)
    {}

    WorkerInput(JSContext* siblingContext, char16_t* chars, size_t length)
      : parentRuntime(nullptr), siblingContext(siblingContext), chars(chars), length(length)
    {}

    ~WorkerInput() {
        js_free(chars);
    }
};

static void SetWorkerContextOptions(JSContext* cx);

static void
WorkerMain(void* arg)
{
    WorkerInput* input = (WorkerInput*) arg;
    MOZ_ASSERT(!!input->parentRuntime != !!input->siblingContext);

    JSContext* cx = nullptr;

    auto guard = mozilla::MakeScopeExit([&] {
            if (cx)
                JS_DestroyContext(cx);
            if (input->siblingContext) {
                cooperationState->numThreads--;
                CooperativeYield();
            }
            js_delete(input);
        });

    cx = input->parentRuntime
         ? JS_NewContext(8L * 1024L * 1024L, 2L * 1024L * 1024L, input->parentRuntime)
         : JS_NewCooperativeContext(input->siblingContext);
    if (!cx)
        return;

    UniquePtr<ShellContext> sc = MakeUnique<ShellContext>(cx);
    if (!sc)
        return;

    if (input->parentRuntime)
        sc->isWorker = true;
    JS_SetContextPrivate(cx, sc.get());
    JS_SetGrayGCRootsTracer(cx, TraceGrayRoots, nullptr);
    SetWorkerContextOptions(cx);
    sc->jobQueue.init(cx, JobQueue(SystemAllocPolicy()));

    Maybe<EnvironmentPreparer> environmentPreparer;
    if (input->parentRuntime) {
        JS_SetFutexCanWait(cx);
        JS::SetWarningReporter(cx, WarningReporter);
        js::SetPreserveWrapperCallback(cx, DummyPreserveWrapperCallback);
        JS_InitDestroyPrincipalsCallback(cx, ShellPrincipals::destroy);

        if (!JS::InitSelfHostedCode(cx))
            return;

        JS::SetEnqueuePromiseJobCallback(cx, ShellEnqueuePromiseJobCallback);
        JS::SetGetIncumbentGlobalCallback(cx, ShellGetIncumbentGlobalCallback);
        JS::SetAsyncTaskCallbacks(cx, ShellStartAsyncTaskCallback, ShellFinishAsyncTaskCallback);

        environmentPreparer.emplace(cx);
    } else {
        JS_AddInterruptCallback(cx, ShellInterruptCallback);
    }

    do {
        JSAutoRequest ar(cx);

        JS::CompartmentOptions compartmentOptions;
        SetStandardCompartmentOptions(compartmentOptions);
        if (input->siblingContext)
            compartmentOptions.creationOptions().setNewZoneInNewZoneGroup();

        RootedObject global(cx, NewGlobalObject(cx, compartmentOptions, nullptr));
        if (!global)
            break;

        JSAutoCompartment ac(cx, global);

        JS::CompileOptions options(cx);
        options.setFileAndLine("<string>", 1)
               .setIsRunOnce(true);

        AutoReportException are(cx);
        RootedScript script(cx);
        if (!JS::Compile(cx, options, input->chars, input->length, &script))
            break;
        RootedValue result(cx);
        JS_ExecuteScript(cx, script, &result);
    } while (0);

    if (input->parentRuntime) {
        JS::SetGetIncumbentGlobalCallback(cx, nullptr);
        JS::SetEnqueuePromiseJobCallback(cx, nullptr);
    }

    sc->jobQueue.reset();

    KillWatchdog(cx);
    JS_SetGrayGCRootsTracer(cx, nullptr, nullptr);
}

// Workers can spawn other workers, so we need a lock to access workerThreads.
static Mutex* workerThreadsLock = nullptr;
static Vector<js::Thread*, 0, SystemAllocPolicy> workerThreads;

class MOZ_RAII AutoLockWorkerThreads : public LockGuard<Mutex>
{
    using Base = LockGuard<Mutex>;
  public:
    AutoLockWorkerThreads()
      : Base(*workerThreadsLock)
    {
        MOZ_ASSERT(workerThreadsLock);
    }
};

static bool
EvalInThread(JSContext* cx, unsigned argc, Value* vp, bool cooperative)
{
    if (!CanUseExtraThreads()) {
        JS_ReportErrorASCII(cx, "Can't create threads with --no-threads");
        return false;
    }

    CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.get(0).isString()) {
        JS_ReportErrorASCII(cx, "Invalid arguments");
        return false;
    }

#if defined(DEBUG) || defined(JS_OOM_BREAKPOINT)
    if (cx->runningOOMTest) {
        JS_ReportErrorASCII(cx, "Can't create threads while running simulated OOM test");
        return false;
    }
#endif

    if (cooperative && GetShellContext(cx)->isWorker) {
        // Disallowing cooperative multithreading in worker runtimes allows
        // yield state to be process wide, and some other simplifications.
        // When we have a better idea of how cooperative multithreading will be
        // used in the browser this restriction might be relaxed.
        JS_ReportErrorASCII(cx, "Cooperative multithreading in worker runtimes is not supported");
        return false;
    }

    if (cooperative && !cx->runtime()->gc.canChangeActiveContext(cx)) {
        JS_ReportErrorASCII(cx, "Cooperating multithreading context switches are not currently allowed");
        return false;
    }

    if (cooperative && cooperationState->singleThreaded) {
        JS_ReportErrorASCII(cx, "Creating cooperative threads is not allowed while single threaded");
        return false;
    }

    if (!args[0].toString()->ensureLinear(cx))
        return false;

    if (!workerThreadsLock) {
        workerThreadsLock = js_new<Mutex>(mutexid::ShellWorkerThreads);
        if (!workerThreadsLock) {
            ReportOutOfMemory(cx);
            return false;
        }
    }

    JSLinearString* str = &args[0].toString()->asLinear();

    char16_t* chars = (char16_t*) js_malloc(str->length() * sizeof(char16_t));
    if (!chars) {
        ReportOutOfMemory(cx);
        return false;
    }

    CopyChars(chars, *str);

    WorkerInput* input =
        cooperative
        ? js_new<WorkerInput>(cx, chars, str->length())
        : js_new<WorkerInput>(JS_GetParentRuntime(cx), chars, str->length());
    if (!input) {
        ReportOutOfMemory(cx);
        return false;
    }

    if (cooperative) {
        cooperationState->numThreads++;
        CooperativeBeginWait(cx);
    }

    auto thread = js_new<Thread>(Thread::Options().setStackSize(gMaxStackSize + 128 * 1024));
    if (!thread || !thread->init(WorkerMain, input)) {
        ReportOutOfMemory(cx);
        if (cooperative) {
            cooperationState->numThreads--;
            CooperativeYield();
            CooperativeEndWait(cx);
        }
        return false;
    }

    if (cooperative) {
        CooperativeEndWait(cx);
    } else {
        AutoLockWorkerThreads alwt;
        if (!workerThreads.append(thread)) {
            ReportOutOfMemory(cx);
            thread->join();
            return false;
        }
    }

    args.rval().setUndefined();
    return true;
}

static bool
EvalInWorker(JSContext* cx, unsigned argc, Value* vp)
{
    return EvalInThread(cx, argc, vp, false);
}

static bool
EvalInCooperativeThread(JSContext* cx, unsigned argc, Value* vp)
{
    return EvalInThread(cx, argc, vp, true);
}

static bool
ShapeOf(JSContext* cx, unsigned argc, JS::Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.get(0).isObject()) {
        JS_ReportErrorASCII(cx, "shapeOf: object expected");
        return false;
    }
    JSObject* obj = &args[0].toObject();
    args.rval().set(JS_NumberValue(double(uintptr_t(obj->maybeShape()) >> 3)));
    return true;
}

static bool
GroupOf(JSContext* cx, unsigned argc, JS::Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.get(0).isObject()) {
        JS_ReportErrorASCII(cx, "groupOf: object expected");
        return false;
    }
    RootedObject obj(cx, &args[0].toObject());
    ObjectGroup* group = JSObject::getGroup(cx, obj);
    if (!group)
        return false;
    args.rval().set(JS_NumberValue(double(uintptr_t(group) >> 3)));
    return true;
}

static bool
UnwrappedObjectsHaveSameShape(JSContext* cx, unsigned argc, JS::Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.get(0).isObject() || !args.get(1).isObject()) {
        JS_ReportErrorASCII(cx, "2 objects expected");
        return false;
    }

    RootedObject obj1(cx, UncheckedUnwrap(&args[0].toObject()));
    RootedObject obj2(cx, UncheckedUnwrap(&args[1].toObject()));

    if (!obj1->is<ShapedObject>() || !obj2->is<ShapedObject>()) {
        JS_ReportErrorASCII(cx, "object does not have a Shape");
        return false;
    }

    args.rval().setBoolean(obj1->as<ShapedObject>().shape() == obj2->as<ShapedObject>().shape());
    return true;
}

static bool
Sleep_fn(JSContext* cx, unsigned argc, Value* vp)
{
    ShellContext* sc = GetShellContext(cx);
    CallArgs args = CallArgsFromVp(argc, vp);

    TimeDuration duration = TimeDuration::FromSeconds(0.0);
    if (args.length() > 0) {
        double t_secs;
        if (!ToNumber(cx, args[0], &t_secs))
            return false;
        if (mozilla::IsNaN(t_secs)) {
            JS_ReportErrorASCII(cx, "sleep interval is not a number");
            return false;
        }

        duration = TimeDuration::FromSeconds(Max(0.0, t_secs));
        if (duration > MAX_TIMEOUT_INTERVAL) {
            JS_ReportErrorASCII(cx, "Excessive sleep interval");
            return false;
        }
    }
    {
        LockGuard<Mutex> guard(sc->watchdogLock);
        TimeStamp toWakeup = TimeStamp::Now() + duration;
        for (;;) {
            sc->sleepWakeup.wait_for(guard, duration);
            if (sc->serviceInterrupt)
                break;
            auto now = TimeStamp::Now();
            if (now >= toWakeup)
                break;
            duration = toWakeup - now;
        }
    }
    args.rval().setUndefined();
    return !sc->serviceInterrupt;
}

static void
KillWatchdog(JSContext* cx)
{
    ShellContext* sc = GetShellContext(cx);
    Maybe<Thread> thread;

    {
        LockGuard<Mutex> guard(sc->watchdogLock);
        Swap(sc->watchdogThread, thread);
        if (thread) {
            // The watchdog thread becoming Nothing is its signal to exit.
            sc->watchdogWakeup.notify_one();
        }
    }
    if (thread)
        thread->join();

    MOZ_ASSERT(!sc->watchdogThread);
}

static void
WatchdogMain(JSContext* cx)
{
    ThisThread::SetName("JS Watchdog");

    ShellContext* sc = GetShellContext(cx);

    LockGuard<Mutex> guard(sc->watchdogLock);
    while (sc->watchdogThread) {
        auto now = TimeStamp::Now();
        if (sc->watchdogTimeout && now >= sc->watchdogTimeout.value()) {
            /*
             * The timeout has just expired. Request an interrupt callback
             * outside the lock.
             */
            sc->watchdogTimeout = Nothing();
            {
                UnlockGuard<Mutex> unlock(guard);
                CancelExecution(cx);
            }

            /* Wake up any threads doing sleep. */
            sc->sleepWakeup.notify_all();
        } else {
            if (sc->watchdogTimeout) {
                /*
                 * Time hasn't expired yet. Simulate an interrupt callback
                 * which doesn't abort execution.
                 */
                JS_RequestInterruptCallback(cx);
            }

            TimeDuration sleepDuration = sc->watchdogTimeout
                                         ? TimeDuration::FromSeconds(0.1)
                                         : TimeDuration::Forever();
            sc->watchdogWakeup.wait_for(guard, sleepDuration);
        }
    }
}

static bool
ScheduleWatchdog(JSContext* cx, double t)
{
    ShellContext* sc = GetShellContext(cx);

    if (t <= 0) {
        LockGuard<Mutex> guard(sc->watchdogLock);
        sc->watchdogTimeout = Nothing();
        return true;
    }

    auto interval = TimeDuration::FromSeconds(t);
    auto timeout = TimeStamp::Now() + interval;
    LockGuard<Mutex> guard(sc->watchdogLock);
    if (!sc->watchdogThread) {
        MOZ_ASSERT(!sc->watchdogTimeout);
        sc->watchdogThread.emplace();
        AutoEnterOOMUnsafeRegion oomUnsafe;
        if (!sc->watchdogThread->init(WatchdogMain, cx))
            oomUnsafe.crash("watchdogThread.init");
    } else if (!sc->watchdogTimeout || timeout < sc->watchdogTimeout.value()) {
        sc->watchdogWakeup.notify_one();
    }
    sc->watchdogTimeout = Some(timeout);
    return true;
}

static void
KillWorkerThreads(JSContext* cx)
{
    MOZ_ASSERT_IF(!CanUseExtraThreads(), workerThreads.empty());

    if (!workerThreadsLock) {
        MOZ_ASSERT(workerThreads.empty());
        return;
    }

    while (true) {
        // We need to leave the AutoLockWorkerThreads scope before we call
        // js::Thread::join, to avoid deadlocks when AutoLockWorkerThreads is
        // used by the worker thread.
        Thread* thread;
        {
            AutoLockWorkerThreads alwt;
            if (workerThreads.empty())
                break;
            thread = workerThreads.popCopy();
        }
        thread->join();
    }

    js_delete(workerThreadsLock);
    workerThreadsLock = nullptr;

    // Yield until all other cooperative threads in the main runtime finish.
    while (cooperationState->numThreads) {
        CooperativeBeginWait(cx);
        CooperativeYield();
        CooperativeEndWait(cx);
    }

    js_delete(cooperationState);
    cooperationState = nullptr;
}

static void
CancelExecution(JSContext* cx)
{
    ShellContext* sc = GetShellContext(cx);
    sc->serviceInterrupt = true;
    JS_RequestInterruptCallback(cx);

    if (sc->haveInterruptFunc) {
        static const char msg[] = "Script runs for too long, terminating.\n";
        fputs(msg, stderr);
    }
}

static bool
SetTimeoutValue(JSContext* cx, double t)
{
    if (mozilla::IsNaN(t)) {
        JS_ReportErrorASCII(cx, "timeout is not a number");
        return false;
    }
    if (TimeDuration::FromSeconds(t) > MAX_TIMEOUT_INTERVAL) {
        JS_ReportErrorASCII(cx, "Excessive timeout value");
        return false;
    }
    GetShellContext(cx)->timeoutInterval = t;
    if (!ScheduleWatchdog(cx, t)) {
        JS_ReportErrorASCII(cx, "Failed to create the watchdog");
        return false;
    }
    return true;
}

static bool
Timeout(JSContext* cx, unsigned argc, Value* vp)
{
    ShellContext* sc = GetShellContext(cx);
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() == 0) {
        args.rval().setNumber(sc->timeoutInterval);
        return true;
    }

    if (args.length() > 2) {
        JS_ReportErrorASCII(cx, "Wrong number of arguments");
        return false;
    }

    double t;
    if (!ToNumber(cx, args[0], &t))
        return false;

    if (args.length() > 1) {
        RootedValue value(cx, args[1]);
        if (!value.isObject() || !value.toObject().is<JSFunction>()) {
            JS_ReportErrorASCII(cx, "Second argument must be a timeout function");
            return false;
        }
        sc->interruptFunc = value;
        sc->haveInterruptFunc = true;
    }

    args.rval().setUndefined();
    return SetTimeoutValue(cx, t);
}

static bool
InterruptIf(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() != 1) {
        JS_ReportErrorASCII(cx, "Wrong number of arguments");
        return false;
    }

    if (ToBoolean(args[0])) {
        GetShellContext(cx)->serviceInterrupt = true;
        JS_RequestInterruptCallback(cx);
    }

    args.rval().setUndefined();
    return true;
}

static bool
InvokeInterruptCallbackWrapper(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 1) {
        JS_ReportErrorASCII(cx, "Wrong number of arguments");
        return false;
    }

    GetShellContext(cx)->serviceInterrupt = true;
    JS_RequestInterruptCallback(cx);
    bool interruptRv = CheckForInterrupt(cx);

    // The interrupt handler could have set a pending exception. Since we call
    // back into JS, don't have it see the pending exception. If we have an
    // uncatchable exception that's not propagating a debug mode forced
    // return, return.
    if (!interruptRv && !cx->isExceptionPending() && !cx->isPropagatingForcedReturn())
        return false;

    JS::AutoSaveExceptionState savedExc(cx);

    FixedInvokeArgs<1> iargs(cx);

    iargs[0].setBoolean(interruptRv);

    RootedValue rv(cx);
    if (!js::Call(cx, args[0], UndefinedHandleValue, iargs, &rv))
        return false;

    args.rval().setUndefined();
    return interruptRv;
}

static bool
SetInterruptCallback(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() != 1) {
        JS_ReportErrorASCII(cx, "Wrong number of arguments");
        return false;
    }

    RootedValue value(cx, args[0]);
    if (!value.isObject() || !value.toObject().is<JSFunction>()) {
        JS_ReportErrorASCII(cx, "Argument must be a function");
        return false;
    }
    GetShellContext(cx)->interruptFunc = value;
    GetShellContext(cx)->haveInterruptFunc = true;

    args.rval().setUndefined();
    return true;
}

static bool
EnableLastWarning(JSContext* cx, unsigned argc, Value* vp)
{
    ShellContext* sc = GetShellContext(cx);
    CallArgs args = CallArgsFromVp(argc, vp);

    sc->lastWarningEnabled = true;
    sc->lastWarning.setNull();

    args.rval().setUndefined();
    return true;
}

static bool
DisableLastWarning(JSContext* cx, unsigned argc, Value* vp)
{
    ShellContext* sc = GetShellContext(cx);
    CallArgs args = CallArgsFromVp(argc, vp);

    sc->lastWarningEnabled = false;
    sc->lastWarning.setNull();

    args.rval().setUndefined();
    return true;
}

static bool
GetLastWarning(JSContext* cx, unsigned argc, Value* vp)
{
    ShellContext* sc = GetShellContext(cx);
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!sc->lastWarningEnabled) {
        JS_ReportErrorASCII(cx, "Call enableLastWarning first.");
        return false;
    }

    if (!JS_WrapValue(cx, &sc->lastWarning))
        return false;

    args.rval().set(sc->lastWarning);
    return true;
}

static bool
ClearLastWarning(JSContext* cx, unsigned argc, Value* vp)
{
    ShellContext* sc = GetShellContext(cx);
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!sc->lastWarningEnabled) {
        JS_ReportErrorASCII(cx, "Call enableLastWarning first.");
        return false;
    }

    sc->lastWarning.setNull();

    args.rval().setUndefined();
    return true;
}

#ifdef DEBUG
static bool
StackDump(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!gOutFile->isOpen()) {
        JS_ReportErrorASCII(cx, "output file is closed");
        return false;
    }

    bool showArgs = ToBoolean(args.get(0));
    bool showLocals = ToBoolean(args.get(1));
    bool showThisProps = ToBoolean(args.get(2));

    char* buf = JS::FormatStackDump(cx, nullptr, showArgs, showLocals, showThisProps);
    if (!buf) {
        fputs("Failed to format JavaScript stack for dump\n", gOutFile->fp);
        JS_ClearPendingException(cx);
    } else {
        fputs(buf, gOutFile->fp);
        JS_smprintf_free(buf);
    }

    args.rval().setUndefined();
    return true;
}
#endif

static bool
StackPointerInfo(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Copy the truncated stack pointer to the result.  This value is not used
    // as a pointer but as a way to measure frame-size from JS.
    args.rval().setInt32(int32_t(reinterpret_cast<size_t>(&args) & 0xfffffff));
    return true;
}


static bool
Elapsed(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() == 0) {
        double d = PRMJ_Now() - GetShellContext(cx)->startTime;
        args.rval().setDouble(d);
        return true;
    }
    JS_ReportErrorASCII(cx, "Wrong number of arguments");
    return false;
}

static bool
Compile(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() < 1) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                                  "compile", "0", "s");
        return false;
    }
    if (!args[0].isString()) {
        const char* typeName = InformalValueTypeName(args[0]);
        JS_ReportErrorASCII(cx, "expected string to compile, got %s", typeName);
        return false;
    }

    RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
    JSFlatString* scriptContents = args[0].toString()->ensureFlat(cx);
    if (!scriptContents)
        return false;

    AutoStableStringChars stableChars(cx);
    if (!stableChars.initTwoByte(cx, scriptContents))
        return false;

    JS::CompileOptions options(cx);
    options.setIntroductionType("js shell compile")
           .setFileAndLine("<string>", 1)
           .setIsRunOnce(true)
           .setNoScriptRval(true);
    RootedScript script(cx);
    const char16_t* chars = stableChars.twoByteRange().begin().get();
    bool ok = JS_CompileUCScript(cx, chars, scriptContents->length(), options, &script);
    args.rval().setUndefined();
    return ok;
}

static bool
ParseModule(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() == 0) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                                  "parseModule", "0", "s");
        return false;
    }

    if (!args[0].isString()) {
        const char* typeName = InformalValueTypeName(args[0]);
        JS_ReportErrorASCII(cx, "expected string to compile, got %s", typeName);
        return false;
    }

    JSFlatString* scriptContents = args[0].toString()->ensureFlat(cx);
    if (!scriptContents)
        return false;

    UniqueChars filename;
    CompileOptions options(cx);
    if (args.length() > 1) {
        if (!args[1].isString()) {
            const char* typeName = InformalValueTypeName(args[1]);
            JS_ReportErrorASCII(cx, "expected filename string, got %s", typeName);
            return false;
        }

        RootedString str(cx, args[1].toString());
        filename.reset(JS_EncodeString(cx, str));
        if (!filename)
            return false;

        options.setFileAndLine(filename.get(), 1);
    } else {
        options.setFileAndLine("<string>", 1);
    }

    AutoStableStringChars stableChars(cx);
    if (!stableChars.initTwoByte(cx, scriptContents))
        return false;

    const char16_t* chars = stableChars.twoByteRange().begin().get();
    SourceBufferHolder srcBuf(chars, scriptContents->length(),
                              SourceBufferHolder::NoOwnership);

    RootedObject module(cx, frontend::CompileModule(cx, options, srcBuf));
    if (!module)
        return false;

    args.rval().setObject(*module);
    return true;
}

static bool
SetModuleResolveHook(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 1) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                                  "setModuleResolveHook", "0", "s");
        return false;
    }

    if (!args[0].isObject() || !args[0].toObject().is<JSFunction>()) {
        const char* typeName = InformalValueTypeName(args[0]);
        JS_ReportErrorASCII(cx, "expected hook function, got %s", typeName);
        return false;
    }

    RootedFunction hook(cx, &args[0].toObject().as<JSFunction>());
    Rooted<GlobalObject*> global(cx, cx->global());
    global->setModuleResolveHook(hook);
    args.rval().setUndefined();
    return true;
}

static bool
GetModuleLoadPath(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    ShellContext* sc = GetShellContext(cx);
    if (sc->moduleLoadPath) {
        JSString* str = JS_NewStringCopyZ(cx, sc->moduleLoadPath.get());
        if (!str)
            return false;

        args.rval().setString(str);
    } else {
        args.rval().setNull();
    }
    return true;
}

static bool
Parse(JSContext* cx, unsigned argc, Value* vp)
{
    using namespace js::frontend;

    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                                  "parse", "0", "s");
        return false;
    }
    if (!args[0].isString()) {
        const char* typeName = InformalValueTypeName(args[0]);
        JS_ReportErrorASCII(cx, "expected string to parse, got %s", typeName);
        return false;
    }

    JSFlatString* scriptContents = args[0].toString()->ensureFlat(cx);
    if (!scriptContents)
        return false;

    AutoStableStringChars stableChars(cx);
    if (!stableChars.initTwoByte(cx, scriptContents))
        return false;

    size_t length = scriptContents->length();
    const char16_t* chars = stableChars.twoByteRange().begin().get();

    CompileOptions options(cx);
    options.setIntroductionType("js shell parse")
           .setFileAndLine("<string>", 1);
    UsedNameTracker usedNames(cx);
    if (!usedNames.init())
        return false;
    Parser<FullParseHandler, char16_t> parser(cx, cx->tempLifoAlloc(), options, chars, length,
                                              /* foldConstants = */ true, usedNames, nullptr,
                                              nullptr);
    if (!parser.checkOptions())
        return false;

    ParseNode* pn = parser.parse();
    if (!pn)
        return false;
#ifdef DEBUG
    DumpParseTree(pn);
    fputc('\n', stderr);
#endif
    args.rval().setUndefined();
    return true;
}

static bool
SyntaxParse(JSContext* cx, unsigned argc, Value* vp)
{
    using namespace js::frontend;

    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                                  "parse", "0", "s");
        return false;
    }
    if (!args[0].isString()) {
        const char* typeName = InformalValueTypeName(args[0]);
        JS_ReportErrorASCII(cx, "expected string to parse, got %s", typeName);
        return false;
    }

    JSFlatString* scriptContents = args[0].toString()->ensureFlat(cx);
    if (!scriptContents)
        return false;
    CompileOptions options(cx);
    options.setIntroductionType("js shell syntaxParse")
           .setFileAndLine("<string>", 1);

    AutoStableStringChars stableChars(cx);
    if (!stableChars.initTwoByte(cx, scriptContents))
        return false;

    const char16_t* chars = stableChars.twoByteRange().begin().get();
    size_t length = scriptContents->length();
    UsedNameTracker usedNames(cx);
    if (!usedNames.init())
        return false;
    Parser<frontend::SyntaxParseHandler, char16_t> parser(cx, cx->tempLifoAlloc(),
                                                          options, chars, length, false,
                                                          usedNames, nullptr, nullptr);
    if (!parser.checkOptions())
        return false;

    bool succeeded = parser.parse();
    if (cx->isExceptionPending())
        return false;

    if (!succeeded && !parser.hadAbortedSyntaxParse()) {
        // If no exception is posted, either there was an OOM or a language
        // feature unhandled by the syntax parser was encountered.
        MOZ_ASSERT(cx->runtime()->hadOutOfMemory);
        return false;
    }

    args.rval().setBoolean(succeeded);
    return true;
}

static void
OffThreadCompileScriptCallback(void* token, void* callbackData)
{
    ShellContext* sc = static_cast<ShellContext*>(callbackData);
    sc->offThreadState.markDone(token);
}

static bool
OffThreadCompileScript(JSContext* cx, unsigned argc, Value* vp)
{
    if (!CanUseExtraThreads()) {
        JS_ReportErrorASCII(cx, "Can't use offThreadCompileScript with --no-threads");
        return false;
    }

    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                                  "offThreadCompileScript", "0", "s");
        return false;
    }
    if (!args[0].isString()) {
        const char* typeName = InformalValueTypeName(args[0]);
        JS_ReportErrorASCII(cx, "expected string to parse, got %s", typeName);
        return false;
    }

    JSAutoByteString fileNameBytes;
    CompileOptions options(cx);
    options.setIntroductionType("js shell offThreadCompileScript")
           .setFileAndLine("<string>", 1);

    if (args.length() >= 2) {
        if (args[1].isPrimitive()) {
            JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS,
                                      "evaluate");
            return false;
        }

        RootedObject opts(cx, &args[1].toObject());
        if (!ParseCompileOptions(cx, options, opts, fileNameBytes))
            return false;
    }

    // These option settings must override whatever the caller requested.
    options.setIsRunOnce(true)
           .setSourceIsLazy(false);

    // We assume the caller wants caching if at all possible, ignoring
    // heuristics that make sense for a real browser.
    options.forceAsync = true;

    JSString* scriptContents = args[0].toString();
    AutoStableStringChars stableChars(cx);
    if (!stableChars.initTwoByte(cx, scriptContents))
        return false;

    size_t length = scriptContents->length();
    const char16_t* chars = stableChars.twoByteRange().begin().get();

    // Make sure we own the string's chars, so that they are not freed before
    // the compilation is finished.
    ScopedJSFreePtr<char16_t> ownedChars;
    if (stableChars.maybeGiveOwnershipToCaller()) {
        ownedChars = const_cast<char16_t*>(chars);
    } else {
        char16_t* copy = cx->pod_malloc<char16_t>(length);
        if (!copy)
            return false;

        mozilla::PodCopy(copy, chars, length);
        ownedChars = copy;
        chars = copy;
    }

    if (!JS::CanCompileOffThread(cx, options, length)) {
        JS_ReportErrorASCII(cx, "cannot compile code on worker thread");
        return false;
    }

    ShellContext* sc = GetShellContext(cx);
    if (!sc->offThreadState.startIfIdle(cx, ScriptKind::Script, ownedChars)) {
        JS_ReportErrorASCII(cx, "called offThreadCompileScript without calling runOffThreadScript"
                            " to receive prior off-thread compilation");
        return false;
    }

    if (!JS::CompileOffThread(cx, options, chars, length,
                              OffThreadCompileScriptCallback, sc))
    {
        sc->offThreadState.abandon(cx);
        return false;
    }

    args.rval().setUndefined();
    return true;
}

static bool
runOffThreadScript(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (OffThreadParsingMustWaitForGC(cx->runtime()))
        gc::FinishGC(cx);

    ShellContext* sc = GetShellContext(cx);
    void* token = sc->offThreadState.waitUntilDone(cx, ScriptKind::Script);
    if (!token) {
        JS_ReportErrorASCII(cx, "called runOffThreadScript when no compilation is pending");
        return false;
    }

    RootedScript script(cx, JS::FinishOffThreadScript(cx, token));
    if (!script)
        return false;

    return JS_ExecuteScript(cx, script, args.rval());
}

static bool
OffThreadCompileModule(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() != 1 || !args[0].isString()) {
        JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS,
                                  "offThreadCompileModule");
        return false;
    }

    JSAutoByteString fileNameBytes;
    CompileOptions options(cx);
    options.setIntroductionType("js shell offThreadCompileModule")
           .setFileAndLine("<string>", 1);
    options.setIsRunOnce(true)
           .setSourceIsLazy(false);
    options.forceAsync = true;

    JSString* scriptContents = args[0].toString();
    AutoStableStringChars stableChars(cx);
    if (!stableChars.initTwoByte(cx, scriptContents))
        return false;

    size_t length = scriptContents->length();
    const char16_t* chars = stableChars.twoByteRange().begin().get();

    // Make sure we own the string's chars, so that they are not freed before
    // the compilation is finished.
    ScopedJSFreePtr<char16_t> ownedChars;
    if (stableChars.maybeGiveOwnershipToCaller()) {
        ownedChars = const_cast<char16_t*>(chars);
    } else {
        char16_t* copy = cx->pod_malloc<char16_t>(length);
        if (!copy)
            return false;

        mozilla::PodCopy(copy, chars, length);
        ownedChars = copy;
        chars = copy;
    }

    if (!JS::CanCompileOffThread(cx, options, length)) {
        JS_ReportErrorASCII(cx, "cannot compile code on worker thread");
        return false;
    }

    ShellContext* sc = GetShellContext(cx);
    if (!sc->offThreadState.startIfIdle(cx, ScriptKind::Module, ownedChars)) {
        JS_ReportErrorASCII(cx, "called offThreadCompileModule without receiving prior off-thread "
                            "compilation");
        return false;
    }

    if (!JS::CompileOffThreadModule(cx, options, chars, length,
                                    OffThreadCompileScriptCallback, sc))
    {
        sc->offThreadState.abandon(cx);
        return false;
    }

    args.rval().setUndefined();
    return true;
}

static bool
FinishOffThreadModule(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (OffThreadParsingMustWaitForGC(cx->runtime()))
        gc::FinishGC(cx);

    ShellContext* sc = GetShellContext(cx);
    void* token = sc->offThreadState.waitUntilDone(cx, ScriptKind::Module);
    if (!token) {
        JS_ReportErrorASCII(cx, "called finishOffThreadModule when no compilation is pending");
        return false;
    }

    RootedObject module(cx, JS::FinishOffThreadModule(cx, token));
    if (!module)
        return false;

    args.rval().setObject(*module);
    return true;
}

static bool
OffThreadDecodeScript(JSContext* cx, unsigned argc, Value* vp)
{
    if (!CanUseExtraThreads()) {
        JS_ReportErrorASCII(cx, "Can't use offThreadDecodeScript with --no-threads");
        return false;
    }

    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                                  "offThreadDecodeScript", "0", "s");
        return false;
    }
    if (!args[0].isObject() || !CacheEntry_isCacheEntry(&args[0].toObject())) {
        const char* typeName = InformalValueTypeName(args[0]);
        JS_ReportErrorASCII(cx, "expected cache entry, got %s", typeName);
        return false;
    }
    RootedObject cacheEntry(cx, &args[0].toObject());

    JSAutoByteString fileNameBytes;
    CompileOptions options(cx);
    options.setIntroductionType("js shell offThreadDecodeScript")
           .setFileAndLine("<string>", 1);

    if (args.length() >= 2) {
        if (args[1].isPrimitive()) {
            JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS,
                                      "evaluate");
            return false;
        }

        RootedObject opts(cx, &args[1].toObject());
        if (!ParseCompileOptions(cx, options, opts, fileNameBytes))
            return false;
    }

    // These option settings must override whatever the caller requested.
    options.setIsRunOnce(true)
           .setSourceIsLazy(false);

    // We assume the caller wants caching if at all possible, ignoring
    // heuristics that make sense for a real browser.
    options.forceAsync = true;

    JS::TranscodeBuffer loadBuffer;
    uint32_t loadLength = 0;
    uint8_t* loadData = nullptr;
    loadData = CacheEntry_getBytecode(cacheEntry, &loadLength);
    if (!loadData)
        return false;
    if (!loadBuffer.append(loadData, loadLength)) {
        JS_ReportOutOfMemory(cx);
        return false;
    }

    if (!JS::CanCompileOffThread(cx, options, loadLength)) {
        JS_ReportErrorASCII(cx, "cannot compile code on worker thread");
        return false;
    }

    ShellContext* sc = GetShellContext(cx);
    if (!sc->offThreadState.startIfIdle(cx, ScriptKind::DecodeScript, mozilla::Move(loadBuffer))) {
        JS_ReportErrorASCII(cx, "called offThreadDecodeScript without calling "
                            "runOffThreadDecodedScript to receive prior off-thread compilation");
        return false;
    }

    if (!JS::DecodeOffThreadScript(cx, options, sc->offThreadState.xdrBuffer(), 0,
                                   OffThreadCompileScriptCallback, sc))
    {
        sc->offThreadState.abandon(cx);
        return false;
    }

    args.rval().setUndefined();
    return true;
}

static bool
runOffThreadDecodedScript(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (OffThreadParsingMustWaitForGC(cx->runtime()))
        gc::FinishGC(cx);

    ShellContext* sc = GetShellContext(cx);
    void* token = sc->offThreadState.waitUntilDone(cx, ScriptKind::DecodeScript);
    if (!token) {
        JS_ReportErrorASCII(cx, "called runOffThreadDecodedScript when no compilation is pending");
        return false;
    }

    RootedScript script(cx, JS::FinishOffThreadScriptDecoder(cx, token));
    if (!script)
        return false;

    return JS_ExecuteScript(cx, script, args.rval());
}

struct MOZ_RAII FreeOnReturn
{
    JSContext* cx;
    const char* ptr;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

    explicit FreeOnReturn(JSContext* cx, const char* ptr = nullptr
                 MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : cx(cx), ptr(ptr)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    void init(const char* ptr) {
        MOZ_ASSERT(!this->ptr);
        this->ptr = ptr;
    }

    ~FreeOnReturn() {
        JS_free(cx, (void*)ptr);
    }
};

static int sArgc;
static char** sArgv;

class AutoCStringVector
{
    Vector<char*> argv_;
  public:
    explicit AutoCStringVector(JSContext* cx) : argv_(cx) {}
    ~AutoCStringVector() {
        for (size_t i = 0; i < argv_.length(); i++)
            js_free(argv_[i]);
    }
    bool append(char* arg) {
        if (!argv_.append(arg)) {
            js_free(arg);
            return false;
        }
        return true;
    }
    char* const* get() const {
        return argv_.begin();
    }
    size_t length() const {
        return argv_.length();
    }
    char* operator[](size_t i) const {
        return argv_[i];
    }
    void replace(size_t i, char* arg) {
        js_free(argv_[i]);
        argv_[i] = arg;
    }
    char* back() const {
        return argv_.back();
    }
    void replaceBack(char* arg) {
        js_free(argv_.back());
        argv_.back() = arg;
    }
};

#if defined(XP_WIN)
static bool
EscapeForShell(AutoCStringVector& argv)
{
    // Windows will break arguments in argv by various spaces, so we wrap each
    // argument in quotes and escape quotes within. Even with quotes, \ will be
    // treated like an escape character, so inflate each \ to \\.

    for (size_t i = 0; i < argv.length(); i++) {
        if (!argv[i])
            continue;

        size_t newLen = 3;  // quotes before and after and null-terminator
        for (char* p = argv[i]; *p; p++) {
            newLen++;
            if (*p == '\"' || *p == '\\')
                newLen++;
        }

        char* escaped = (char*)js_malloc(newLen);
        if (!escaped)
            return false;

        char* src = argv[i];
        char* dst = escaped;
        *dst++ = '\"';
        while (*src) {
            if (*src == '\"' || *src == '\\')
                *dst++ = '\\';
            *dst++ = *src++;
        }
        *dst++ = '\"';
        *dst++ = '\0';
        MOZ_ASSERT(escaped + newLen == dst);

        argv.replace(i, escaped);
    }
    return true;
}
#endif

static Vector<const char*, 4, js::SystemAllocPolicy> sPropagatedFlags;

#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
static bool
PropagateFlagToNestedShells(const char* flag)
{
    return sPropagatedFlags.append(flag);
}
#endif

static bool
NestedShell(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    AutoCStringVector argv(cx);

    // The first argument to the shell is its path, which we assume is our own
    // argv[0].
    if (sArgc < 1) {
        JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr, JSSMSG_NESTED_FAIL);
        return false;
    }
    if (!argv.append(strdup(sArgv[0])))
        return false;

    // Propagate selected flags from the current shell
    for (unsigned i = 0; i < sPropagatedFlags.length(); i++) {
        char* cstr = strdup(sPropagatedFlags[i]);
        if (!cstr || !argv.append(cstr))
            return false;
    }

    // The arguments to nestedShell are stringified and append to argv.
    RootedString str(cx);
    for (unsigned i = 0; i < args.length(); i++) {
        str = ToString(cx, args[i]);
        if (!str || !argv.append(JS_EncodeString(cx, str)))
            return false;

        // As a special case, if the caller passes "--js-cache", replace that
        // with "--js-cache=$(jsCacheDir)"
        if (!strcmp(argv.back(), "--js-cache") && jsCacheDir) {
            char* newArg = JS_smprintf("--js-cache=%s", jsCacheDir);
            if (!newArg)
                return false;
            argv.replaceBack(newArg);
        }
    }

    // execv assumes argv is null-terminated
    if (!argv.append(nullptr))
        return false;

    int status = 0;
#if defined(XP_WIN)
    if (!EscapeForShell(argv))
        return false;
    status = _spawnv(_P_WAIT, sArgv[0], argv.get());
#else
    pid_t pid = fork();
    switch (pid) {
      case -1:
        JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr, JSSMSG_NESTED_FAIL);
        return false;
      case 0:
        (void)execv(sArgv[0], argv.get());
        exit(-1);
      default: {
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
            continue;
        break;
      }
    }
#endif

    if (status != 0) {
        JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr, JSSMSG_NESTED_FAIL);
        return false;
    }

    args.rval().setUndefined();
    return true;
}

static bool
DecompileFunction(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() < 1 || !args[0].isObject() || !args[0].toObject().is<JSFunction>()) {
        args.rval().setUndefined();
        return true;
    }
    RootedFunction fun(cx, &args[0].toObject().as<JSFunction>());
    JSString* result = JS_DecompileFunction(cx, fun, 0);
    if (!result)
        return false;
    args.rval().setString(result);
    return true;
}

static bool
DecompileThisScript(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    NonBuiltinScriptFrameIter iter(cx);
    if (iter.done()) {
        args.rval().setString(cx->runtime()->emptyString);
        return true;
    }

    {
        JSAutoCompartment ac(cx, iter.script());

        RootedScript script(cx, iter.script());
        JSString* result = JS_DecompileScript(cx, script, "test", 0);
        if (!result)
            return false;

        args.rval().setString(result);
    }

    return JS_WrapValue(cx, args.rval());
}

static bool
ThisFilename(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JS::AutoFilename filename;
    if (!DescribeScriptedCaller(cx, &filename) || !filename.get()) {
        args.rval().setString(cx->runtime()->emptyString);
        return true;
    }

    JSString* str = JS_NewStringCopyZ(cx, filename.get());
    if (!str)
        return false;

    args.rval().setString(str);
    return true;
}

static bool
WrapWithProto(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    Value obj = UndefinedValue(), proto = UndefinedValue();
    if (args.length() == 2) {
        obj = args[0];
        proto = args[1];
    }
    if (!obj.isObject() || !proto.isObjectOrNull()) {
        JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS,
                                  "wrapWithProto");
        return false;
    }

    WrapperOptions options(cx);
    options.setProto(proto.toObjectOrNull());
    JSObject* wrapped = Wrapper::New(cx, &obj.toObject(),
                                     &Wrapper::singletonWithPrototype, options);
    if (!wrapped)
        return false;

    args.rval().setObject(*wrapped);
    return true;
}

static bool
NewGlobal(JSContext* cx, unsigned argc, Value* vp)
{
    JSPrincipals* principals = nullptr;

    JS::CompartmentOptions options;
    JS::CompartmentCreationOptions& creationOptions = options.creationOptions();
    JS::CompartmentBehaviors& behaviors = options.behaviors();

    SetStandardCompartmentOptions(options);
    options.creationOptions().setNewZoneInExistingZoneGroup(cx->global());

    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() == 1 && args[0].isObject()) {
        RootedObject opts(cx, &args[0].toObject());
        RootedValue v(cx);

        if (!JS_GetProperty(cx, opts, "invisibleToDebugger", &v))
            return false;
        if (v.isBoolean())
            creationOptions.setInvisibleToDebugger(v.toBoolean());

        if (!JS_GetProperty(cx, opts, "cloneSingletons", &v))
            return false;
        if (v.isBoolean())
            creationOptions.setCloneSingletons(v.toBoolean());

        if (!JS_GetProperty(cx, opts, "experimentalNumberFormatFormatToPartsEnabled", &v))
            return false;
        if (v.isBoolean())
            creationOptions.setExperimentalNumberFormatFormatToPartsEnabled(v.toBoolean());

        if (!JS_GetProperty(cx, opts, "sameZoneAs", &v))
            return false;
        if (v.isObject())
            creationOptions.setExistingZone(UncheckedUnwrap(&v.toObject()));

        if (!JS_GetProperty(cx, opts, "disableLazyParsing", &v))
            return false;
        if (v.isBoolean())
            behaviors.setDisableLazyParsing(v.toBoolean());

        if (!JS_GetProperty(cx, opts, "principal", &v))
            return false;
        if (!v.isUndefined()) {
            uint32_t bits;
            if (!ToUint32(cx, v, &bits))
                return false;
            principals = cx->new_<ShellPrincipals>(bits);
            if (!principals)
                return false;
            JS_HoldPrincipals(principals);
        }
    }

    RootedObject global(cx, NewGlobalObject(cx, options, principals));
    if (principals)
        JS_DropPrincipals(cx, principals);
    if (!global)
        return false;

    if (!JS_WrapObject(cx, &global))
        return false;

    args.rval().setObject(*global);
    return true;
}

static bool
NukeCCW(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() != 1 || !args[0].isObject() ||
        !IsCrossCompartmentWrapper(&args[0].toObject()))
    {
        JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS,
                                  "nukeCCW");
        return false;
    }

    NukeCrossCompartmentWrapper(cx, &args[0].toObject());
    args.rval().setUndefined();
    return true;
}

static bool
GetMaxArgs(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setInt32(ARGS_LENGTH_MAX);
    return true;
}

static bool
ObjectEmulatingUndefined(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    static const JSClass cls = {
        "ObjectEmulatingUndefined",
        JSCLASS_EMULATES_UNDEFINED
    };

    RootedObject obj(cx, JS_NewObject(cx, &cls));
    if (!obj)
        return false;
    args.rval().setObject(*obj);
    return true;
}

static bool
GetSelfHostedValue(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() != 1 || !args[0].isString()) {
        JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr, JSSMSG_INVALID_ARGS,
                                  "getSelfHostedValue");
        return false;
    }
    RootedAtom srcAtom(cx, ToAtom<CanGC>(cx, args[0]));
    if (!srcAtom)
        return false;
    RootedPropertyName srcName(cx, srcAtom->asPropertyName());
    return cx->runtime()->cloneSelfHostedValue(cx, srcName, args.rval());
}

class ShellSourceHook: public SourceHook {
    // The function we should call to lazily retrieve source code.
    PersistentRootedFunction fun;

  public:
    ShellSourceHook(JSContext* cx, JSFunction& fun) : fun(cx, &fun) {}

    bool load(JSContext* cx, const char* filename, char16_t** src, size_t* length) {
        RootedString str(cx, JS_NewStringCopyZ(cx, filename));
        if (!str)
            return false;
        RootedValue filenameValue(cx, StringValue(str));

        RootedValue result(cx);
        if (!Call(cx, UndefinedHandleValue, fun, HandleValueArray(filenameValue), &result))
            return false;

        str = JS::ToString(cx, result);
        if (!str)
            return false;

        *length = JS_GetStringLength(str);
        *src = cx->pod_malloc<char16_t>(*length);
        if (!*src)
            return false;

        JSLinearString* linear = str->ensureLinear(cx);
        if (!linear)
            return false;

        CopyChars(*src, *linear);
        return true;
    }
};

static bool
WithSourceHook(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject callee(cx, &args.callee());

    if (args.length() != 2) {
        ReportUsageErrorASCII(cx, callee, "Wrong number of arguments.");
        return false;
    }

    if (!args[0].isObject() || !args[0].toObject().is<JSFunction>()
        || !args[1].isObject() || !args[1].toObject().is<JSFunction>()) {
        ReportUsageErrorASCII(cx, callee, "First and second arguments must be functions.");
        return false;
    }

    mozilla::UniquePtr<ShellSourceHook> hook =
        mozilla::MakeUnique<ShellSourceHook>(cx, args[0].toObject().as<JSFunction>());
    if (!hook)
        return false;

    mozilla::UniquePtr<SourceHook> savedHook = js::ForgetSourceHook(cx);
    js::SetSourceHook(cx, Move(hook));

    RootedObject fun(cx, &args[1].toObject());
    bool result = Call(cx, UndefinedHandleValue, fun, JS::HandleValueArray::empty(), args.rval());
    js::SetSourceHook(cx, Move(savedHook));
    return result;
}

static bool
IsCachingEnabled(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setBoolean(jsCachingEnabled && jsCacheAsmJSPath != nullptr);
    return true;
}

static bool
SetCachingEnabled(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (GetShellContext(cx)->isWorker) {
        JS_ReportErrorASCII(cx, "Caching is not supported in workers");
        return false;
    }

    jsCachingEnabled = ToBoolean(args.get(0));
    args.rval().setUndefined();
    return true;
}

static void
PrintProfilerEvents_Callback(const char* msg)
{
    fprintf(stderr, "PROFILER EVENT: %s\n", msg);
}

static bool
PrintProfilerEvents(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (cx->runtime()->geckoProfiler().enabled())
        js::RegisterContextProfilingEventMarker(cx, &PrintProfilerEvents_Callback);
    args.rval().setUndefined();
    return true;
}

#ifdef SINGLESTEP_PROFILING
static void
SingleStepCallback(void* arg, jit::Simulator* sim, void* pc)
{
    JSContext* cx = reinterpret_cast<JSContext*>(arg);

    // If profiling is not enabled, don't do anything.
    if (!cx->runtime()->geckoProfiler().enabled())
        return;

    JS::ProfilingFrameIterator::RegisterState state;
    state.pc = pc;
#if defined(JS_SIMULATOR_ARM)
    state.sp = (void*)sim->get_register(jit::Simulator::sp);
    state.lr = (void*)sim->get_register(jit::Simulator::lr);
    state.fp = (void*)sim->get_register(jit::Simulator::fp);
#elif defined(JS_SIMULATOR_MIPS64)
    state.sp = (void*)sim->getRegister(jit::Simulator::sp);
    state.lr = (void*)sim->getRegister(jit::Simulator::ra);
    state.fp = (void*)sim->getRegister(jit::Simulator::fp);
#else
#  error "NYI: Single-step profiling support"
#endif

    mozilla::DebugOnly<void*> lastStackAddress = nullptr;
    StackChars stack;
    uint32_t frameNo = 0;
    AutoEnterOOMUnsafeRegion oomUnsafe;
    for (JS::ProfilingFrameIterator i(cx, state); !i.done(); ++i) {
        MOZ_ASSERT(i.stackAddress() != nullptr);
        MOZ_ASSERT(lastStackAddress <= i.stackAddress());
        lastStackAddress = i.stackAddress();
        JS::ProfilingFrameIterator::Frame frames[16];
        uint32_t nframes = i.extractStack(frames, 0, 16);
        for (uint32_t i = 0; i < nframes; i++) {
            if (frameNo > 0) {
                if (!stack.append(",", 1))
                    oomUnsafe.crash("stack.append");
            }
            if (!stack.append(frames[i].label, strlen(frames[i].label)))
                oomUnsafe.crash("stack.append");
            frameNo++;
        }
    }

    ShellContext* sc = GetShellContext(cx);

    // Only append the stack if it differs from the last stack.
    if (sc->stacks.empty() ||
        sc->stacks.back().length() != stack.length() ||
        !PodEqual(sc->stacks.back().begin(), stack.begin(), stack.length()))
    {
        if (!sc->stacks.append(Move(stack)))
            oomUnsafe.crash("stacks.append");
    }
}
#endif

static bool
EnableSingleStepProfiling(JSContext* cx, unsigned argc, Value* vp)
{
#ifdef SINGLESTEP_PROFILING
    CallArgs args = CallArgsFromVp(argc, vp);

    jit::Simulator* sim = cx->simulator();
    sim->enable_single_stepping(SingleStepCallback, cx);

    args.rval().setUndefined();
    return true;
#else
    JS_ReportErrorASCII(cx, "single-step profiling not enabled on this platform");
    return false;
#endif
}

static bool
DisableSingleStepProfiling(JSContext* cx, unsigned argc, Value* vp)
{
#ifdef SINGLESTEP_PROFILING
    CallArgs args = CallArgsFromVp(argc, vp);

    jit::Simulator* sim = cx->simulator();
    sim->disable_single_stepping();

    ShellContext* sc = GetShellContext(cx);

    AutoValueVector elems(cx);
    for (size_t i = 0; i < sc->stacks.length(); i++) {
        JSString* stack = JS_NewUCStringCopyN(cx, sc->stacks[i].begin(), sc->stacks[i].length());
        if (!stack)
            return false;
        if (!elems.append(StringValue(stack)))
            return false;
    }

    JSObject* array = JS_NewArrayObject(cx, elems);
    if (!array)
        return false;

    sc->stacks.clear();
    args.rval().setObject(*array);
    return true;
#else
    JS_ReportErrorASCII(cx, "single-step profiling not enabled on this platform");
    return false;
#endif
}

static bool
IsLatin1(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    bool isLatin1 = args.get(0).isString() && args[0].toString()->hasLatin1Chars();
    args.rval().setBoolean(isLatin1);
    return true;
}

static bool
EnableGeckoProfiling(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    ShellContext* sc = GetShellContext(cx);

    // Disable before re-enabling; see the assertion in |GeckoProfiler::setProfilingStack|.
    if (cx->runtime()->geckoProfiler().installed())
        MOZ_ALWAYS_TRUE(cx->runtime()->geckoProfiler().enable(false));

    SetContextProfilingStack(cx, sc->geckoProfilingStack, &sc->geckoProfilingStackSize,
                             ShellContext::GeckoProfilingMaxStackSize);
    cx->runtime()->geckoProfiler().enableSlowAssertions(false);
    if (!cx->runtime()->geckoProfiler().enable(true))
        JS_ReportErrorASCII(cx, "Cannot ensure single threaded execution in profiler");

    args.rval().setUndefined();
    return true;
}

static bool
EnableGeckoProfilingWithSlowAssertions(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setUndefined();

    ShellContext* sc = GetShellContext(cx);

    if (cx->runtime()->geckoProfiler().enabled()) {
        // If profiling already enabled with slow assertions disabled,
        // this is a no-op.
        if (cx->runtime()->geckoProfiler().slowAssertionsEnabled())
            return true;

        // Slow assertions are off.  Disable profiling before re-enabling
        // with slow assertions on.
        MOZ_ALWAYS_TRUE(cx->runtime()->geckoProfiler().enable(false));
    }

    // Disable before re-enabling; see the assertion in |GeckoProfiler::setProfilingStack|.
    if (cx->runtime()->geckoProfiler().installed())
        MOZ_ALWAYS_TRUE(cx->runtime()->geckoProfiler().enable(false));

    SetContextProfilingStack(cx, sc->geckoProfilingStack, &sc->geckoProfilingStackSize,
                             ShellContext::GeckoProfilingMaxStackSize);
    cx->runtime()->geckoProfiler().enableSlowAssertions(true);
    if (!cx->runtime()->geckoProfiler().enable(true))
        JS_ReportErrorASCII(cx, "Cannot ensure single threaded execution in profiler");

    return true;
}

static bool
DisableGeckoProfiling(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (cx->runtime()->geckoProfiler().installed())
        MOZ_ALWAYS_TRUE(cx->runtime()->geckoProfiler().enable(false));
    args.rval().setUndefined();
    return true;
}

// Global mailbox that is used to communicate a SharedArrayBuffer
// value from one worker to another.
//
// For simplicity we store only the SharedArrayRawBuffer; retaining
// the SAB object would require per-runtime storage, and would have no
// real benefits.
//
// Invariant: when a SARB is in the mailbox its reference count is at
// least 1, accounting for the reference from the mailbox.
//
// The lock guards the mailbox variable and prevents a race where two
// workers try to set the mailbox at the same time to replace a SARB
// that is only referenced from the mailbox: the workers will both
// decrement the reference count on the old SARB, and one of those
// decrements will be on a garbage object.  We could implement this
// with atomics and a CAS loop but it's not worth the bother.

static Mutex* sharedArrayBufferMailboxLock;
static SharedArrayRawBuffer* sharedArrayBufferMailbox;

static bool
InitSharedArrayBufferMailbox()
{
    sharedArrayBufferMailboxLock = js_new<Mutex>(mutexid::ShellArrayBufferMailbox);
    return sharedArrayBufferMailboxLock != nullptr;
}

static void
DestructSharedArrayBufferMailbox()
{
    // All workers need to have terminated at this point.
    if (sharedArrayBufferMailbox)
        sharedArrayBufferMailbox->dropReference();
    js_delete(sharedArrayBufferMailboxLock);
}

static bool
GetSharedArrayBuffer(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSObject* newObj = nullptr;

    {
        sharedArrayBufferMailboxLock->lock();
        auto unlockMailbox = MakeScopeExit([]() { sharedArrayBufferMailboxLock->unlock(); });

        SharedArrayRawBuffer* buf = sharedArrayBufferMailbox;
        if (buf) {
            if (!buf->addReference()) {
                JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_SC_SAB_REFCNT_OFLO);
                return false;
            }

            // Shared memory is enabled globally in the shell: there can't be a worker
            // that does not enable it if the main thread has it.
            MOZ_ASSERT(cx->compartment()->creationOptions().getSharedMemoryAndAtomicsEnabled());
            newObj = SharedArrayBufferObject::New(cx, buf);
            if (!newObj) {
                buf->dropReference();
                return false;
            }
        }
    }

    args.rval().setObjectOrNull(newObj);
    return true;
}

static bool
SetSharedArrayBuffer(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    SharedArrayRawBuffer* newBuffer = nullptr;

    if (argc == 0 || args.get(0).isNullOrUndefined()) {
        // Clear out the mailbox
    }
    else if (args.get(0).isObject() && args[0].toObject().is<SharedArrayBufferObject>()) {
        newBuffer = args[0].toObject().as<SharedArrayBufferObject>().rawBufferObject();
        if (!newBuffer->addReference()) {
            JS_ReportErrorASCII(cx, "Reference count overflow on SharedArrayBuffer");
            return false;
        }
    } else {
        JS_ReportErrorASCII(cx, "Only a SharedArrayBuffer can be installed in the global mailbox");
        return false;
    }

    {
        sharedArrayBufferMailboxLock->lock();
        auto unlockMailbox = MakeScopeExit([]() { sharedArrayBufferMailboxLock->unlock(); });

        SharedArrayRawBuffer* oldBuffer = sharedArrayBufferMailbox;
        if (oldBuffer)
            oldBuffer->dropReference();
        sharedArrayBufferMailbox = newBuffer;
    }

    args.rval().setUndefined();
    return true;
}

class SprintOptimizationTypeInfoOp : public JS::ForEachTrackedOptimizationTypeInfoOp
{
    Sprinter* sp;
    bool startedTypes_;
    bool hadError_;

  public:
    explicit SprintOptimizationTypeInfoOp(Sprinter* sp)
      : sp(sp),
        startedTypes_(false),
        hadError_(false)
    { }

    void readType(const char* keyedBy, const char* name,
                  const char* location, const Maybe<unsigned>& lineno) override
    {
        if (hadError_)
            return;

        do {
            if (!startedTypes_) {
                startedTypes_ = true;
                if (!sp->put("{\"typeset\": ["))
                    break;
            }

            if (!sp->jsprintf("{\"keyedBy\":\"%s\"", keyedBy))
                break;

            if (name) {
                if (!sp->jsprintf(",\"name\":\"%s\"", name))
                    break;
            }

            if (location) {
                char buf[512];
                PutEscapedString(buf, mozilla::ArrayLength(buf), location, strlen(location), '"');
                if (!sp->jsprintf(",\"location\":%s", buf))
                    break;
            }
            if (lineno.isSome()) {
                if (!sp->jsprintf(",\"line\":%u", *lineno))
                    break;
            }
            if (!sp->put("},"))
                break;

            return;
        } while (false);

        hadError_ = true;
    }

    void operator()(JS::TrackedTypeSite site, const char* mirType) override {
        if (hadError_)
            return;

        do {
            if (startedTypes_) {
                // Clear trailing ,
                if ((*sp)[sp->getOffset() - 1] == ',')
                    (*sp)[sp->getOffset() - 1] = ' ';
                if (!sp->put("],"))
                    break;

                startedTypes_ = false;
            } else {
                if (!sp->put("{"))
                    break;
            }

            if (!sp->jsprintf("\"site\":\"%s\",\"mirType\":\"%s\"},",
                              TrackedTypeSiteString(site), mirType))
            {
                break;
            }

            return;
        } while (false);

        hadError_ = true;
    }

    bool hadError() const {
        return hadError_;
    }
};

class SprintOptimizationAttemptsOp : public JS::ForEachTrackedOptimizationAttemptOp
{
    Sprinter* sp;
    bool hadError_;

  public:
    explicit SprintOptimizationAttemptsOp(Sprinter* sp)
      : sp(sp), hadError_(false)
    { }

    void operator()(JS::TrackedStrategy strategy, JS::TrackedOutcome outcome) override {
        if (hadError_)
            return;

        hadError_ = !sp->jsprintf("{\"strategy\":\"%s\",\"outcome\":\"%s\"},",
                                  TrackedStrategyString(strategy), TrackedOutcomeString(outcome));
    }

    bool hadError() const {
        return hadError_;
    }
};

static bool
ReflectTrackedOptimizations(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject callee(cx, &args.callee());
    JSRuntime* rt = cx->runtime();

    if (!rt->hasJitRuntime() || !rt->jitRuntime()->isOptimizationTrackingEnabled(cx->zone()->group())) {
        JS_ReportErrorASCII(cx, "Optimization tracking is off.");
        return false;
    }

    if (args.length() != 1) {
        ReportUsageErrorASCII(cx, callee, "Wrong number of arguments");
        return false;
    }

    if (!args[0].isObject() || !args[0].toObject().is<JSFunction>()) {
        ReportUsageErrorASCII(cx, callee, "Argument must be a function");
        return false;
    }

    RootedFunction fun(cx, &args[0].toObject().as<JSFunction>());
    if (!fun->hasScript() || !fun->nonLazyScript()->hasIonScript()) {
        args.rval().setNull();
        return true;
    }

    // Suppress GC for the unrooted JitcodeGlobalEntry below.
    gc::AutoSuppressGC suppress(cx);

    jit::JitcodeGlobalTable* table = rt->jitRuntime()->getJitcodeGlobalTable();
    jit::IonScript* ion = fun->nonLazyScript()->ionScript();
    jit::JitcodeGlobalEntry& entry = table->lookupInfallible(ion->method()->raw());

    if (!entry.hasTrackedOptimizations()) {
        JSObject* obj = JS_NewPlainObject(cx);
        if (!obj)
            return false;
        args.rval().setObject(*obj);
        return true;
    }

    Sprinter sp(cx);
    if (!sp.init())
        return false;

    const jit::IonTrackedOptimizationsRegionTable* regions =
        entry.ionEntry().trackedOptimizationsRegionTable();

    if (!sp.put("{\"regions\": ["))
        return false;

    for (uint32_t i = 0; i < regions->numEntries(); i++) {
        jit::IonTrackedOptimizationsRegion region = regions->entry(i);
        jit::IonTrackedOptimizationsRegion::RangeIterator iter = region.ranges();
        while (iter.more()) {
            uint32_t startOffset, endOffset;
            uint8_t index;
            iter.readNext(&startOffset, &endOffset, &index);

            JSScript* script;
            jsbytecode* pc;
            // Use endOffset, as startOffset may be associated with a
            // previous, adjacent region ending exactly at startOffset. That
            // is, suppose we have two regions [0, startOffset], [startOffset,
            // endOffset]. Since we are not querying a return address, we want
            // the second region and not the first.
            uint8_t* addr = ion->method()->raw() + endOffset;
            entry.youngestFrameLocationAtAddr(rt, addr, &script, &pc);

            if (!sp.jsprintf("{\"location\":\"%s:%" PRIuSIZE "\",\"offset\":%" PRIuSIZE ",\"index\":%u}%s",
                             script->filename(), script->lineno(), script->pcToOffset(pc), index,
                             iter.more() ? "," : ""))
            {
                return false;
            }
        }
    }

    if (!sp.put("],"))
        return false;

    if (!sp.put("\"opts\": ["))
        return false;

    for (uint8_t i = 0; i < entry.ionEntry().numOptimizationAttempts(); i++) {
        if (!sp.jsprintf("%s{\"typeinfo\":[", i == 0 ? "" : ","))
            return false;

        SprintOptimizationTypeInfoOp top(&sp);
        jit::IonTrackedOptimizationsTypeInfo::ForEachOpAdapter adapter(top);
        entry.trackedOptimizationTypeInfo(i).forEach(adapter, entry.allTrackedTypes());
        if (top.hadError())
            return false;

        // Clear the trailing ,
        if (sp[sp.getOffset() - 1] == ',')
            sp[sp.getOffset() - 1] = ' ';

        if (!sp.put("],\"attempts\":["))
            return false;

        SprintOptimizationAttemptsOp aop(&sp);
        entry.trackedOptimizationAttempts(i).forEach(aop);
        if (aop.hadError())
            return false;

        // Clear the trailing ,
        if (sp[sp.getOffset() - 1] == ',')
            sp[sp.getOffset() - 1] = ' ';

        if (!sp.put("]}"))
            return false;
    }

    if (!sp.put("]}"))
        return false;

    if (sp.hadOutOfMemory())
        return false;

    RootedString str(cx, JS_NewStringCopyZ(cx, sp.string()));
    if (!str)
        return false;
    RootedValue jsonVal(cx);
    if (!JS_ParseJSON(cx, str, &jsonVal))
        return false;

    args.rval().set(jsonVal);
    return true;
}

static bool
DumpScopeChain(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject callee(cx, &args.callee());

    if (args.length() != 1) {
        ReportUsageErrorASCII(cx, callee, "Wrong number of arguments");
        return false;
    }

    if (!args[0].isObject() ||
        !(args[0].toObject().is<JSFunction>() || args[0].toObject().is<ModuleObject>()))
    {
        ReportUsageErrorASCII(cx, callee, "Argument must be an interpreted function or a module");
        return false;
    }

    RootedObject obj(cx, &args[0].toObject());
    RootedScript script(cx);

    if (obj->is<JSFunction>()) {
        RootedFunction fun(cx, &obj->as<JSFunction>());
        if (!fun->isInterpreted()) {
            ReportUsageErrorASCII(cx, callee, "Argument must be an interpreted function");
            return false;
        }
        script = JSFunction::getOrCreateScript(cx, fun);
    } else {
        script = obj->as<ModuleObject>().script();
    }

    script->bodyScope()->dump();

    args.rval().setUndefined();
    return true;
}

// For testing gray marking, grayRoot() will heap-allocate an address
// where we can store a JSObject*, and create a new object if one doesn't
// already exist.
//
// Note that ensureGrayRoot() will automatically blacken the returned object,
// so it will not actually end up marked gray until the following GC clears the
// black bit (assuming nothing is holding onto it.)
//
// The idea is that you can set up a whole graph of objects to be marked gray,
// hanging off of the object returned from grayRoot(). Then you GC to clear the
// black bits and set the gray bits.
//
// To test grayness, register the objects of interest with addMarkObservers(),
// which takes an Array of objects (which will be marked black at the time
// they're passed in). Their mark bits may be retrieved at any time with
// getMarks(), in the form of an array of strings with each index corresponding
// to the original objects passed to addMarkObservers().

static ShellCompartmentPrivate*
EnsureShellCompartmentPrivate(JSContext* cx)
{
    auto priv = static_cast<ShellCompartmentPrivate*>(JS_GetCompartmentPrivate(cx->compartment()));
    if (!priv) {
        priv = cx->new_<ShellCompartmentPrivate>();
        JS_SetCompartmentPrivate(cx->compartment(), priv);
    }
    return priv;
}

static bool
EnsureGrayRoot(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    auto priv = EnsureShellCompartmentPrivate(cx);
    if (!priv->grayRoot) {
        if (!(priv->grayRoot = NewDenseEmptyArray(cx, nullptr, TenuredObject)))
            return false;
    }

    args.rval().setObject(*priv->grayRoot);
    return true;
}

static MarkBitObservers*
EnsureMarkBitObservers(JSContext* cx)
{
    ShellContext* sc = GetShellContext(cx);
    if (!sc->markObservers) {
        sc->markObservers.reset(cx->new_<MarkBitObservers>(cx->runtime(),
                                                         NonshrinkingGCObjectVector()));
    }
    return sc->markObservers.get();
}

static bool
ClearMarkObservers(JSContext* cx, unsigned argc, Value* vp)
{
    auto markObservers = EnsureMarkBitObservers(cx);
    markObservers->get().clear();
    return true;
}

static bool
AddMarkObservers(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    auto markObservers = EnsureMarkBitObservers(cx);

    if (!args.get(0).isObject()) {
        JS_ReportErrorASCII(cx, "argument must be an Array of objects");
        return false;
    }

    // WeakCaches are not swept during a minor GC. To prevent nursery-allocated
    // contents from having the mark bits be deceptively black until the second
    // GC, they would need to be marked weakly (cf NurseryAwareHashMap). It is
    // simpler to evict the nursery to prevent nursery objects from being
    // observed.
    cx->runtime()->gc.evictNursery();

    RootedObject observersArg(cx, &args[0].toObject());
    RootedValue v(cx);
    uint32_t length;
    if (!GetLengthProperty(cx, observersArg, &length))
        return false;
    for (uint32_t i = 0; i < length; i++) {
        if (!JS_GetElement(cx, observersArg, i, &v))
            return false;
        if (!v.isObject()) {
            JS_ReportErrorASCII(cx, "argument must be an Array of objects");
            return false;
        }
        if (!markObservers->get().append(&v.toObject()))
            return false;
    }

    args.rval().setInt32(length);
    return true;
}

static bool
GetMarks(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    auto& observers = GetShellContext(cx)->markObservers;
    if (!observers) {
        args.rval().setUndefined();
        return true;
    }

    size_t length = observers->get().length();
    Rooted<ArrayObject*> ret(cx, js::NewDenseEmptyArray(cx));
    if (!ret)
        return false;

    for (uint32_t i = 0; i < length; i++) {
        const char* color;
        JSObject* obj = observers->get()[i];
        if (!obj) {
            color = "dead";
        } else {
            gc::TenuredCell* cell = &obj->asTenured();
            if (cell->isMarked(gc::GRAY))
                color = "gray";
            else if (cell->isMarked(gc::BLACK))
                color = "black";
            else
                color = "unmarked";
        }
        JSString* s = JS_NewStringCopyZ(cx, color);
        if (!s)
            return false;
        if (!NewbornArrayPush(cx, ret, StringValue(s)))
            return false;
    }

    args.rval().setObject(*ret);
    return true;
}

namespace js {
namespace shell {

class ShellAutoEntryMonitor : JS::dbg::AutoEntryMonitor {
    Vector<UniqueChars, 1, js::SystemAllocPolicy> log;
    bool oom;
    bool enteredWithoutExit;

  public:
    explicit ShellAutoEntryMonitor(JSContext *cx)
      : AutoEntryMonitor(cx),
        oom(false),
        enteredWithoutExit(false)
    { }

    ~ShellAutoEntryMonitor() {
        MOZ_ASSERT(!enteredWithoutExit);
    }

    void Entry(JSContext* cx, JSFunction* function, JS::HandleValue asyncStack,
               const char* asyncCause) override {
        MOZ_ASSERT(!enteredWithoutExit);
        enteredWithoutExit = true;

        RootedString displayId(cx, JS_GetFunctionDisplayId(function));
        if (displayId) {
            UniqueChars displayIdStr(JS_EncodeStringToUTF8(cx, displayId));
            if (!displayIdStr) {
                // We report OOM in buildResult.
                cx->recoverFromOutOfMemory();
                oom = true;
                return;
            }
            oom = !log.append(mozilla::Move(displayIdStr));
            return;
        }

        oom = !log.append(DuplicateString("anonymous"));
    }

    void Entry(JSContext* cx, JSScript* script, JS::HandleValue asyncStack,
               const char* asyncCause) override {
        MOZ_ASSERT(!enteredWithoutExit);
        enteredWithoutExit = true;

        UniqueChars label(JS_smprintf("eval:%s", JS_GetScriptFilename(script)));
        oom = !label || !log.append(mozilla::Move(label));
    }

    void Exit(JSContext* cx) override {
        MOZ_ASSERT(enteredWithoutExit);
        enteredWithoutExit = false;
    }

    bool buildResult(JSContext *cx, MutableHandleValue resultValue) {
        if (oom) {
            JS_ReportOutOfMemory(cx);
            return false;
        }

        RootedObject result(cx, JS_NewArrayObject(cx, log.length()));
        if (!result)
            return false;

        for (size_t i = 0; i < log.length(); i++) {
            char *name = log[i].get();
            RootedString string(cx, Atomize(cx, name, strlen(name)));
            if (!string)
                return false;
            RootedValue value(cx, StringValue(string));
            if (!JS_SetElement(cx, result, i, value))
                return false;
        }

        resultValue.setObject(*result.get());
        return true;
    }
};

} // namespace shell
} // namespace js

static bool
EntryPoints(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() != 1) {
        JS_ReportErrorASCII(cx, "Wrong number of arguments");
        return false;
    }

    RootedObject opts(cx, ToObject(cx, args[0]));
    if (!opts)
        return false;

    // { function: f } --- Call f.
    {
        RootedValue fun(cx), dummy(cx);

        if (!JS_GetProperty(cx, opts, "function", &fun))
            return false;
        if (!fun.isUndefined()) {
            js::shell::ShellAutoEntryMonitor sarep(cx);
            if (!Call(cx, UndefinedHandleValue, fun, JS::HandleValueArray::empty(), &dummy))
                return false;
            return sarep.buildResult(cx, args.rval());
        }
    }

    // { object: o, property: p, value: v } --- Fetch o[p], or if
    // v is present, assign o[p] = v.
    {
        RootedValue objectv(cx), propv(cx), valuev(cx);

        if (!JS_GetProperty(cx, opts, "object", &objectv) ||
            !JS_GetProperty(cx, opts, "property", &propv))
            return false;
        if (!objectv.isUndefined() && !propv.isUndefined()) {
            RootedObject object(cx, ToObject(cx, objectv));
            if (!object)
                return false;

            RootedString string(cx, ToString(cx, propv));
            if (!string)
                return false;
            RootedId id(cx);
            if (!JS_StringToId(cx, string, &id))
                return false;

            if (!JS_GetProperty(cx, opts, "value", &valuev))
                return false;

            js::shell::ShellAutoEntryMonitor sarep(cx);

            if (!valuev.isUndefined()) {
                if (!JS_SetPropertyById(cx, object, id, valuev))
                    return false;
            } else {
                if (!JS_GetPropertyById(cx, object, id, &valuev))
                    return false;
            }

            return sarep.buildResult(cx, args.rval());
        }
    }

    // { ToString: v } --- Apply JS::ToString to v.
    {
        RootedValue v(cx);

        if (!JS_GetProperty(cx, opts, "ToString", &v))
            return false;
        if (!v.isUndefined()) {
            js::shell::ShellAutoEntryMonitor sarep(cx);
            if (!JS::ToString(cx, v))
                return false;
            return sarep.buildResult(cx, args.rval());
        }
    }

    // { ToNumber: v } --- Apply JS::ToNumber to v.
    {
        RootedValue v(cx);
        double dummy;

        if (!JS_GetProperty(cx, opts, "ToNumber", &v))
            return false;
        if (!v.isUndefined()) {
            js::shell::ShellAutoEntryMonitor sarep(cx);
            if (!JS::ToNumber(cx, v, &dummy))
                return false;
            return sarep.buildResult(cx, args.rval());
        }
    }

    // { eval: code } --- Apply ToString and then Evaluate to code.
    {
        RootedValue code(cx), dummy(cx);

        if (!JS_GetProperty(cx, opts, "eval", &code))
            return false;
        if (!code.isUndefined()) {
            RootedString codeString(cx, ToString(cx, code));
            if (!codeString || !codeString->ensureFlat(cx))
                return false;

            AutoStableStringChars stableChars(cx);
            if (!stableChars.initTwoByte(cx, codeString))
                return false;
            const char16_t* chars = stableChars.twoByteRange().begin().get();
            size_t length = codeString->length();

            CompileOptions options(cx);
            options.setIntroductionType("entryPoint eval")
                   .setFileAndLine("entryPoint eval", 1);

            js::shell::ShellAutoEntryMonitor sarep(cx);
            if (!JS::Evaluate(cx, options, chars, length, &dummy))
                return false;
            return sarep.buildResult(cx, args.rval());
        }
    }

    JS_ReportErrorASCII(cx, "bad 'params' object");
    return false;
}

static bool
SetARMHwCapFlags(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() != 1) {
        JS_ReportErrorASCII(cx, "Wrong number of arguments");
        return false;
    }

    RootedString flagsListString(cx, JS::ToString(cx, args.get(0)));
    if (!flagsListString)
        return false;

#if defined(JS_CODEGEN_ARM)
    JSAutoByteString flagsList(cx, flagsListString);
    if (!flagsList)
        return false;

    jit::ParseARMHwCapFlags(flagsList.ptr());
#endif

    args.rval().setUndefined();
    return true;
}

#ifndef __AFL_HAVE_MANUAL_CONTROL
# define __AFL_LOOP(x) true
#endif

static bool
WasmLoop(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject callee(cx, &args.callee());

    if (args.length() < 1 || args.length() > 2) {
        ReportUsageErrorASCII(cx, callee, "Wrong number of arguments");
        return false;
    }

    if (!args[0].isString()) {
        ReportUsageErrorASCII(cx, callee, "First argument must be a String");
        return false;
    }

    RootedObject importObj(cx);
    if (!args.get(1).isUndefined()) {
        if (!args.get(1).isObject()) {
            ReportUsageErrorASCII(cx, callee, "Second argument, if present, must be an Object");
            return false;
        }
        importObj = &args[1].toObject();
    }

    RootedString givenPath(cx, args[0].toString());
    RootedString filename(cx, ResolvePath(cx, givenPath, RootRelative));
    if (!filename)
        return false;

    while (__AFL_LOOP(1000)) {
        Rooted<JSObject*> ret(cx, FileAsTypedArray(cx, filename));
        if (!ret)
            return false;

        Rooted<TypedArrayObject*> typedArray(cx, &ret->as<TypedArrayObject>());
        RootedWasmInstanceObject instanceObj(cx);
        if (!wasm::Eval(cx, typedArray, importObj, &instanceObj)) {
            // Clear any pending exceptions, we don't care about them
            cx->clearPendingException();
        }
    }

    return true;
}

static const JSFunctionSpecWithHelp shell_functions[] = {
    JS_FN_HELP("version", Version, 0, 0,
"version([number])",
"  Get or force a script compilation version number."),

    JS_FN_HELP("options", Options, 0, 0,
"options([option ...])",
"  Get or toggle JavaScript options."),

    JS_FN_HELP("load", Load, 1, 0,
"load(['foo.js' ...])",
"  Load files named by string arguments. Filename is relative to the\n"
"      current working directory."),

    JS_FN_HELP("loadRelativeToScript", LoadScriptRelativeToScript, 1, 0,
"loadRelativeToScript(['foo.js' ...])",
"  Load files named by string arguments. Filename is relative to the\n"
"      calling script."),

    JS_FN_HELP("evaluate", Evaluate, 2, 0,
"evaluate(code[, options])",
"  Evaluate code as though it were the contents of a file.\n"
"  options is an optional object that may have these properties:\n"
"      isRunOnce: use the isRunOnce compiler option (default: false)\n"
"      noScriptRval: use the no-script-rval compiler option (default: false)\n"
"      fileName: filename for error messages and debug info\n"
"      lineNumber: starting line number for error messages and debug info\n"
"      columnNumber: starting column number for error messages and debug info\n"
"      global: global in which to execute the code\n"
"      newContext: if true, create and use a new cx (default: false)\n"
"      catchTermination: if true, catch termination (failure without\n"
"         an exception value, as for slow scripts or out-of-memory)\n"
"         and return 'terminated'\n"
"      element: if present with value |v|, convert |v| to an object |o| and\n"
"         mark the source as being attached to the DOM element |o|. If the\n"
"         property is omitted or |v| is null, don't attribute the source to\n"
"         any DOM element.\n"
"      elementAttributeName: if present and not undefined, the name of\n"
"         property of 'element' that holds this code. This is what\n"
"         Debugger.Source.prototype.elementAttributeName returns.\n"
"      sourceMapURL: if present with value |v|, convert |v| to a string, and\n"
"         provide that as the code's source map URL. If omitted, attach no\n"
"         source map URL to the code (although the code may provide one itself,\n"
"         via a //#sourceMappingURL comment).\n"
"      sourceIsLazy: if present and true, indicates that, after compilation, \n"
          "script source should not be cached by the JS engine and should be \n"
          "lazily loaded from the embedding as-needed.\n"
"      loadBytecode: if true, and if the source is a CacheEntryObject,\n"
"         the bytecode would be loaded and decoded from the cache entry instead\n"
"         of being parsed, then it would be executed as usual.\n"
"      saveBytecode: if true, and if the source is a CacheEntryObject,\n"
"         the bytecode would be encoded and saved into the cache entry after\n"
"         the script execution.\n"
"      assertEqBytecode: if true, and if both loadBytecode and saveBytecode are \n"
"         true, then the loaded bytecode and the encoded bytecode are compared.\n"
"         and an assertion is raised if they differ.\n"
),

    JS_FN_HELP("run", Run, 1, 0,
"run('foo.js')",
"  Run the file named by the first argument, returning the number of\n"
"  of milliseconds spent compiling and executing it."),

    JS_FN_HELP("readline", ReadLine, 0, 0,
"readline()",
"  Read a single line from stdin."),

    JS_FN_HELP("readlineBuf", ReadLineBuf, 1, 0,
"readlineBuf([ buf ])",
"  Emulate readline() on the specified string. The first call with a string\n"
"  argument sets the source buffer. Subsequent calls without an argument\n"
"  then read from this buffer line by line.\n"),

    JS_FN_HELP("print", Print, 0, 0,
"print([exp ...])",
"  Evaluate and print expressions to stdout."),

    JS_FN_HELP("printErr", PrintErr, 0, 0,
"printErr([exp ...])",
"  Evaluate and print expressions to stderr."),

    JS_FN_HELP("putstr", PutStr, 0, 0,
"putstr([exp])",
"  Evaluate and print expression without newline."),

    JS_FN_HELP("dateNow", Now, 0, 0,
"dateNow()",
"  Return the current time with sub-ms precision."),

    JS_FN_HELP("help", Help, 0, 0,
"help([name ...])",
"  Display usage and help messages."),

    JS_FN_HELP("quit", Quit, 0, 0,
"quit()",
"  Quit the shell."),

    JS_FN_HELP("assertEq", AssertEq, 2, 0,
"assertEq(actual, expected[, msg])",
"  Throw if the first two arguments are not the same (both +0 or both -0,\n"
"  both NaN, or non-zero and ===)."),

    JS_FN_HELP("startTimingMutator", StartTimingMutator, 0, 0,
"startTimingMutator()",
"  Start accounting time to mutator vs GC."),

    JS_FN_HELP("stopTimingMutator", StopTimingMutator, 0, 0,
"stopTimingMutator()",
"  Stop accounting time to mutator vs GC and dump the results."),

    JS_FN_HELP("throwError", ThrowError, 0, 0,
"throwError()",
"  Throw an error from JS_ReportError."),

#ifdef DEBUG
    JS_FN_HELP("disassemble", DisassembleToString, 1, 0,
"disassemble([fun/code])",
"  Return the disassembly for the given function or code.\n"
"  All disassembly functions take these options as leading string arguments:\n"
"    \"-r\" (disassemble recursively)\n"
"    \"-l\" (show line numbers)\n"
"    \"-S\" (omit source notes)"),

    JS_FN_HELP("dis", Disassemble, 1, 0,
"dis([fun/code])",
"  Disassemble functions into bytecodes."),

    JS_FN_HELP("disfile", DisassFile, 1, 0,
"disfile('foo.js')",
"  Disassemble script file into bytecodes.\n"),

    JS_FN_HELP("dissrc", DisassWithSrc, 1, 0,
"dissrc([fun/code])",
"  Disassemble functions with source lines."),

    JS_FN_HELP("notes", Notes, 1, 0,
"notes([fun])",
"  Show source notes for functions."),

    JS_FN_HELP("stackDump", StackDump, 3, 0,
"stackDump(showArgs, showLocals, showThisProps)",
"  Tries to print a lot of information about the current stack. \n"
"  Similar to the DumpJSStack() function in the browser."),

#endif
    JS_FN_HELP("intern", Intern, 1, 0,
"intern(str)",
"  Internalize str in the atom table."),

    JS_FN_HELP("getslx", GetSLX, 1, 0,
"getslx(obj)",
"  Get script line extent."),

    JS_FN_HELP("evalcx", EvalInContext, 1, 0,
"evalcx(s[, o])",
"  Evaluate s in optional sandbox object o.\n"
"  if (s == '' && !o) return new o with eager standard classes\n"
"  if (s == 'lazy' && !o) return new o with lazy standard classes"),

    JS_FN_HELP("evalInWorker", EvalInWorker, 1, 0,
"evalInWorker(str)",
"  Evaluate 'str' in a separate thread with its own runtime.\n"),

    JS_FN_HELP("evalInCooperativeThread", EvalInCooperativeThread, 1, 0,
"evalInCooperativeThread(str)",
"  Evaluate 'str' in a separate cooperatively scheduled thread using the same runtime.\n"),

    JS_FN_HELP("cooperativeYield", CooperativeYieldThread, 0, 0,
"evalInCooperativeThread()",
"  Yield execution to another cooperatively scheduled thread using the same runtime.\n"),

    JS_FN_HELP("getSharedArrayBuffer", GetSharedArrayBuffer, 0, 0,
"getSharedArrayBuffer()",
"  Retrieve the SharedArrayBuffer object from the cross-worker mailbox.\n"
"  The object retrieved may not be identical to the object that was\n"
"  installed, but it references the same shared memory.\n"
"  getSharedArrayBuffer performs an ordering memory barrier.\n"),

    JS_FN_HELP("setSharedArrayBuffer", SetSharedArrayBuffer, 0, 0,
"setSharedArrayBuffer()",
"  Install the SharedArrayBuffer object in the cross-worker mailbox.\n"
"  setSharedArrayBuffer performs an ordering memory barrier.\n"),

    JS_FN_HELP("shapeOf", ShapeOf, 1, 0,
"shapeOf(obj)",
"  Get the shape of obj (an implementation detail)."),

    JS_FN_HELP("groupOf", GroupOf, 1, 0,
"groupOf(obj)",
"  Get the group of obj (an implementation detail)."),

    JS_FN_HELP("unwrappedObjectsHaveSameShape", UnwrappedObjectsHaveSameShape, 2, 0,
"unwrappedObjectsHaveSameShape(obj1, obj2)",
"  Returns true iff obj1 and obj2 have the same shape, false otherwise. Both\n"
"  objects are unwrapped first, so this can be used on objects from different\n"
"  globals."),

#ifdef DEBUG
    JS_FN_HELP("arrayInfo", ArrayInfo, 1, 0,
"arrayInfo(a1, a2, ...)",
"  Report statistics about arrays."),
#endif

    JS_FN_HELP("sleep", Sleep_fn, 1, 0,
"sleep(dt)",
"  Sleep for dt seconds."),

    JS_FN_HELP("compile", Compile, 1, 0,
"compile(code)",
"  Compiles a string to bytecode, potentially throwing."),

    JS_FN_HELP("parseModule", ParseModule, 1, 0,
"parseModule(code)",
"  Parses source text as a module and returns a Module object."),

    JS_FN_HELP("setModuleResolveHook", SetModuleResolveHook, 1, 0,
"setModuleResolveHook(function(module, specifier) {})",
"  Set the HostResolveImportedModule hook to |function|.\n"
"  This hook is used to look up a previously loaded module object.  It should\n"
"  be implemented by the module loader."),

    JS_FN_HELP("getModuleLoadPath", GetModuleLoadPath, 0, 0,
"getModuleLoadPath()",
"  Return any --module-load-path argument passed to the shell.  Used by the\n"
"  module loader.\n"),

    JS_FN_HELP("parse", Parse, 1, 0,
"parse(code)",
"  Parses a string, potentially throwing."),

    JS_FN_HELP("syntaxParse", SyntaxParse, 1, 0,
"syntaxParse(code)",
"  Check the syntax of a string, returning success value"),

    JS_FN_HELP("offThreadCompileScript", OffThreadCompileScript, 1, 0,
"offThreadCompileScript(code[, options])",
"  Compile |code| on a helper thread. To wait for the compilation to finish\n"
"  and run the code, call |runOffThreadScript|. If present, |options| may\n"
"  have properties saying how the code should be compiled:\n"
"      noScriptRval: use the no-script-rval compiler option (default: false)\n"
"      fileName: filename for error messages and debug info\n"
"      lineNumber: starting line number for error messages and debug info\n"
"      columnNumber: starting column number for error messages and debug info\n"
"      element: if present with value |v|, convert |v| to an object |o| and\n"
"         mark the source as being attached to the DOM element |o|. If the\n"
"         property is omitted or |v| is null, don't attribute the source to\n"
"         any DOM element.\n"
"      elementAttributeName: if present and not undefined, the name of\n"
"         property of 'element' that holds this code. This is what\n"
"         Debugger.Source.prototype.elementAttributeName returns.\n"),

    JS_FN_HELP("runOffThreadScript", runOffThreadScript, 0, 0,
"runOffThreadScript()",
"  Wait for off-thread compilation to complete. If an error occurred,\n"
"  throw the appropriate exception; otherwise, run the script and return\n"
"  its value."),

    JS_FN_HELP("offThreadCompileModule", OffThreadCompileModule, 1, 0,
"offThreadCompileModule(code)",
"  Compile |code| on a helper thread. To wait for the compilation to finish\n"
"  and get the module object, call |finishOffThreadModule|."),

    JS_FN_HELP("finishOffThreadModule", FinishOffThreadModule, 0, 0,
"finishOffThreadModule()",
"  Wait for off-thread compilation to complete. If an error occurred,\n"
"  throw the appropriate exception; otherwise, return the module object"),

    JS_FN_HELP("offThreadDecodeScript", OffThreadDecodeScript, 1, 0,
"offThreadDecodeScript(cacheEntry[, options])",
"  Decode |code| on a helper thread. To wait for the compilation to finish\n"
"  and run the code, call |runOffThreadScript|. If present, |options| may\n"
"  have properties saying how the code should be compiled.\n"
"  (see also offThreadCompileScript)\n"),

    JS_FN_HELP("runOffThreadDecodedScript", runOffThreadDecodedScript, 0, 0,
"runOffThreadDecodedScript()",
"  Wait for off-thread decoding to complete. If an error occurred,\n"
"  throw the appropriate exception; otherwise, run the script and return\n"
"  its value."),

    JS_FN_HELP("timeout", Timeout, 1, 0,
"timeout([seconds], [func])",
"  Get/Set the limit in seconds for the execution time for the current context.\n"
"  A negative value (default) means that the execution time is unlimited.\n"
"  If a second argument is provided, it will be invoked when the timer elapses.\n"
"  Calling this function will replace any callback set by |setInterruptCallback|.\n"),

    JS_FN_HELP("interruptIf", InterruptIf, 1, 0,
"interruptIf(cond)",
"  Requests interrupt callback if cond is true. If a callback function is set via\n"
"  |timeout| or |setInterruptCallback|, it will be called. No-op otherwise."),

    JS_FN_HELP("invokeInterruptCallback", InvokeInterruptCallbackWrapper, 0, 0,
"invokeInterruptCallback(fun)",
"  Forcefully set the interrupt flag and invoke the interrupt handler. If a\n"
"  callback function is set via |timeout| or |setInterruptCallback|, it will\n"
"  be called. Before returning, fun is called with the return value of the\n"
"  interrupt handler."),

    JS_FN_HELP("setInterruptCallback", SetInterruptCallback, 1, 0,
"setInterruptCallback(func)",
"  Sets func as the interrupt callback function.\n"
"  Calling this function will replace any callback set by |timeout|.\n"),

    JS_FN_HELP("enableLastWarning", EnableLastWarning, 0, 0,
"enableLastWarning()",
"  Enable storing the last warning."),

    JS_FN_HELP("disableLastWarning", DisableLastWarning, 0, 0,
"disableLastWarning()",
"  Disable storing the last warning."),

    JS_FN_HELP("getLastWarning", GetLastWarning, 0, 0,
"getLastWarning()",
"  Returns an object that represents the last warning."),

    JS_FN_HELP("clearLastWarning", ClearLastWarning, 0, 0,
"clearLastWarning()",
"  Clear the last warning."),

    JS_FN_HELP("elapsed", Elapsed, 0, 0,
"elapsed()",
"  Execution time elapsed for the current thread."),

    JS_FN_HELP("decompileFunction", DecompileFunction, 1, 0,
"decompileFunction(func)",
"  Decompile a function."),

    JS_FN_HELP("decompileThis", DecompileThisScript, 0, 0,
"decompileThis()",
"  Decompile the currently executing script."),

    JS_FN_HELP("thisFilename", ThisFilename, 0, 0,
"thisFilename()",
"  Return the filename of the current script"),

    JS_FN_HELP("newGlobal", NewGlobal, 1, 0,
"newGlobal([options])",
"  Return a new global object in a new compartment. If options\n"
"  is given, it may have any of the following properties:\n"
"      sameZoneAs: the compartment will be in the same zone as the given object (defaults to a new zone)\n"
"      invisibleToDebugger: the global will be invisible to the debugger (default false)\n"
"      principal: if present, its value converted to a number must be an\n"
"         integer that fits in 32 bits; use that as the new compartment's\n"
"         principal. Shell principals are toys, meant only for testing; one\n"
"         shell principal subsumes another if its set bits are a superset of\n"
"         the other's. Thus, a principal of 0 subsumes nothing, while a\n"
"         principals of ~0 subsumes all other principals. The absence of a\n"
"         principal is treated as if its bits were 0xffff, for subsumption\n"
"         purposes. If this property is omitted, supply no principal."),

    JS_FN_HELP("nukeCCW", NukeCCW, 1, 0,
"nukeCCW(wrapper)",
"  Nuke a CrossCompartmentWrapper, which turns it into a DeadProxyObject."),

    JS_FN_HELP("createMappedArrayBuffer", CreateMappedArrayBuffer, 1, 0,
"createMappedArrayBuffer(filename, [offset, [size]])",
"  Create an array buffer that mmaps the given file."),

    JS_FN_HELP("addPromiseReactions", AddPromiseReactions, 3, 0,
"addPromiseReactions(promise, onResolve, onReject)",
"  Calls the JS::AddPromiseReactions JSAPI function with the given arguments."),

    JS_FN_HELP("getMaxArgs", GetMaxArgs, 0, 0,
"getMaxArgs()",
"  Return the maximum number of supported args for a call."),

    JS_FN_HELP("objectEmulatingUndefined", ObjectEmulatingUndefined, 0, 0,
"objectEmulatingUndefined()",
"  Return a new object obj for which typeof obj === \"undefined\", obj == null\n"
"  and obj == undefined (and vice versa for !=), and ToBoolean(obj) === false.\n"),

    JS_FN_HELP("isCachingEnabled", IsCachingEnabled, 0, 0,
"isCachingEnabled()",
"  Return whether JS caching is enabled."),

    JS_FN_HELP("setCachingEnabled", SetCachingEnabled, 1, 0,
"setCachingEnabled(b)",
"  Enable or disable JS caching."),

    JS_FN_HELP("cacheEntry", CacheEntry, 1, 0,
"cacheEntry(code)",
"  Return a new opaque object which emulates a cache entry of a script.  This\n"
"  object encapsulates the code and its cached content. The cache entry is filled\n"
"  and read by the \"evaluate\" function by using it in place of the source, and\n"
"  by setting \"saveBytecode\" and \"loadBytecode\" options."),

    JS_FN_HELP("printProfilerEvents", PrintProfilerEvents, 0, 0,
"printProfilerEvents()",
"  Register a callback with the profiler that prints javascript profiler events\n"
"  to stderr.  Callback is only registered if profiling is enabled."),

    JS_FN_HELP("enableSingleStepProfiling", EnableSingleStepProfiling, 0, 0,
"enableSingleStepProfiling()",
"  This function will fail on platforms that don't support single-step profiling\n"
"  (currently ARM and MIPS64 support it). When enabled, at every instruction a\n"
"  backtrace will be recorded and stored in an array. Adjacent duplicate backtraces\n"
"  are discarded."),

    JS_FN_HELP("disableSingleStepProfiling", DisableSingleStepProfiling, 0, 0,
"disableSingleStepProfiling()",
"  Return the array of backtraces recorded by enableSingleStepProfiling."),

    JS_FN_HELP("enableGeckoProfiling", EnableGeckoProfiling, 0, 0,
"enableGeckoProfiling()",
"  Enables Gecko Profiler instrumentation and corresponding assertions, with slow\n"
"  assertions disabled.\n"),

    JS_FN_HELP("enableGeckoProfilingWithSlowAssertions", EnableGeckoProfilingWithSlowAssertions, 0, 0,
"enableGeckoProfilingWithSlowAssertions()",
"  Enables Gecko Profiler instrumentation and corresponding assertions, with slow\n"
"  assertions enabled.\n"),

    JS_FN_HELP("disableGeckoProfiling", DisableGeckoProfiling, 0, 0,
"disableGeckoProfiling()",
"  Disables Gecko Profiler instrumentation"),

    JS_FN_HELP("isLatin1", IsLatin1, 1, 0,
"isLatin1(s)",
"  Return true iff the string's characters are stored as Latin1."),

    JS_FN_HELP("stackPointerInfo", StackPointerInfo, 0, 0,
"stackPointerInfo()",
"  Return an int32 value which corresponds to the offset of the latest stack\n"
"  pointer, such that one can take the differences of 2 to estimate a frame-size."),

    JS_FN_HELP("entryPoints", EntryPoints, 1, 0,
"entryPoints(params)",
"Carry out some JSAPI operation as directed by |params|, and return an array of\n"
"objects describing which JavaScript entry points were invoked as a result.\n"
"|params| is an object whose properties indicate what operation to perform. Here\n"
"are the recognized groups of properties:\n"
"\n"
"{ function }: Call the object |params.function| with no arguments.\n"
"\n"
"{ object, property }: Fetch the property named |params.property| of\n"
"|params.object|.\n"
"\n"
"{ ToString }: Apply JS::ToString to |params.toString|.\n"
"\n"
"{ ToNumber }: Apply JS::ToNumber to |params.toNumber|.\n"
"\n"
"{ eval }: Apply JS::Evaluate to |params.eval|.\n"
"\n"
"The return value is an array of strings, with one element for each\n"
"JavaScript invocation that occurred as a result of the given\n"
"operation. Each element is the name of the function invoked, or the\n"
"string 'eval:FILENAME' if the code was invoked by 'eval' or something\n"
"similar.\n"),

    JS_FN_HELP("drainJobQueue", DrainJobQueue, 0, 0,
"drainJobQueue()",
"Take jobs from the shell's job queue in FIFO order and run them until the\n"
"queue is empty.\n"),

    JS_FN_HELP("setPromiseRejectionTrackerCallback", SetPromiseRejectionTrackerCallback, 1, 0,
"setPromiseRejectionTrackerCallback()",
"Sets the callback to be invoked whenever a Promise rejection is unhandled\n"
"or a previously-unhandled rejection becomes handled."),

#ifdef ENABLE_INTL_API
    JS_FN_HELP("addIntlExtras", AddIntlExtras, 1, 0,
"addIntlExtras(obj)",
"Adds various not-yet-standardized Intl functions as properties on the\n"
"provided object (this should generally be Intl itself).  The added\n"
"functions and their behavior are experimental: don't depend upon them\n"
"unless you're willing to update your code if these experimental APIs change\n"
"underneath you.  Calling this function more than once in a realm/global\n"
"will throw."),
#endif // ENABLE_INTL_API

    JS_FS_HELP_END
};

static const JSFunctionSpecWithHelp fuzzing_unsafe_functions[] = {
    JS_FN_HELP("clone", Clone, 1, 0,
"clone(fun[, scope])",
"  Clone function object."),

    JS_FN_HELP("getSelfHostedValue", GetSelfHostedValue, 1, 0,
"getSelfHostedValue()",
"  Get a self-hosted value by its name. Note that these values don't get \n"
"  cached, so repeatedly getting the same value creates multiple distinct clones."),

    JS_FN_HELP("line2pc", LineToPC, 0, 0,
"line2pc([fun,] line)",
"  Map line number to PC."),

    JS_FN_HELP("pc2line", PCToLine, 0, 0,
"pc2line(fun[, pc])",
"  Map PC to line number."),

    JS_FN_HELP("nestedShell", NestedShell, 0, 0,
"nestedShell(shellArgs...)",
"  Execute the given code in a new JS shell process, passing this nested shell\n"
"  the arguments passed to nestedShell. argv[0] of the nested shell will be argv[0]\n"
"  of the current shell (which is assumed to be the actual path to the shell.\n"
"  arguments[0] (of the call to nestedShell) will be argv[1], arguments[1] will\n"
"  be argv[2], etc."),

    JS_INLINABLE_FN_HELP("assertFloat32", testingFunc_assertFloat32, 2, 0, TestAssertFloat32,
"assertFloat32(value, isFloat32)",
"  In IonMonkey only, asserts that value has (resp. hasn't) the MIRType::Float32 if isFloat32 is true (resp. false)."),

    JS_INLINABLE_FN_HELP("assertRecoveredOnBailout", testingFunc_assertRecoveredOnBailout, 2, 0,
TestAssertRecoveredOnBailout,
"assertRecoveredOnBailout(var)",
"  In IonMonkey only, asserts that variable has RecoveredOnBailout flag."),

    JS_FN_HELP("withSourceHook", WithSourceHook, 1, 0,
"withSourceHook(hook, fun)",
"  Set this JS runtime's lazy source retrieval hook (that is, the hook\n"
"  used to find sources compiled with |CompileOptions::LAZY_SOURCE|) to\n"
"  |hook|; call |fun| with no arguments; and then restore the runtime's\n"
"  original hook. Return or throw whatever |fun| did. |hook| gets\n"
"  passed the requested code's URL, and should return a string.\n"
"\n"
"  Notes:\n"
"\n"
"  1) SpiderMonkey may assert if the returned code isn't close enough\n"
"  to the script's real code, so this function is not fuzzer-safe.\n"
"\n"
"  2) The runtime can have only one source retrieval hook active at a\n"
"  time. If |fun| is not careful, |hook| could be asked to retrieve the\n"
"  source code for compilations that occurred long before it was set,\n"
"  and that it knows nothing about. The reverse applies as well: the\n"
"  original hook, that we reinstate after the call to |fun| completes,\n"
"  might be asked for the source code of compilations that |fun|\n"
"  performed, and which, presumably, only |hook| knows how to find.\n"),

    JS_FN_HELP("wrapWithProto", WrapWithProto, 2, 0,
"wrapWithProto(obj)",
"  Wrap an object into a noop wrapper with prototype semantics.\n"
"  Note: This is not fuzzing safe because it can be used to construct\n"
"        deeply nested wrapper chains that cannot exist in the wild."),

    JS_FN_HELP("trackedOpts", ReflectTrackedOptimizations, 1, 0,
"trackedOpts(fun)",
"  Returns an object describing the tracked optimizations of |fun|, if\n"
"  any. If |fun| is not a scripted function or has not been compiled by\n"
"  Ion, null is returned."),

    JS_FN_HELP("dumpScopeChain", DumpScopeChain, 1, 0,
"dumpScopeChain(obj)",
"  Prints the scope chain of an interpreted function or a module."),

    JS_FN_HELP("grayRoot", EnsureGrayRoot, 0, 0,
"grayRoot()",
"  Create a gray root Array, if needed, for the current compartment, and\n"
"  return it."),

    JS_FN_HELP("addMarkObservers", AddMarkObservers, 1, 0,
"addMarkObservers(array_of_objects)",
"  Register an array of objects whose mark bits will be tested by calls to\n"
"  getMarks. The objects will be in calling compartment. Objects from\n"
"  multiple compartments may be monitored by calling this function in\n"
"  different compartments."),

    JS_FN_HELP("clearMarkObservers", ClearMarkObservers, 1, 0,
"clearMarkObservers()",
"  Clear out the list of objects whose mark bits will be tested.\n"),

    JS_FN_HELP("getMarks", GetMarks, 0, 0,
"getMarks()",
"  Return an array of strings representing the current state of the mark\n"
"  bits ('gray' or 'black', or 'dead' if the object has been collected)\n"
"  for the objects registered via addMarkObservers. Note that some of the\n"
"  objects tested may be from different compartments than the one in which\n"
"  this function runs."),

    JS_FN_HELP("crash", Crash, 0, 0,
"crash([message])",
"  Crashes the process with a MOZ_CRASH."),

    JS_FN_HELP("setARMHwCapFlags", SetARMHwCapFlags, 1, 0,
"setARMHwCapFlags(\"flag1,flag2 flag3\")",
"  On non-ARM, no-op. On ARM, set the hardware capabilities. The list of \n"
"  flags is available by calling this function with \"help\" as the flag's name"),

    JS_FN_HELP("wasmLoop", WasmLoop, 2, 0,
"wasmLoop(filename, imports)",
"  Performs an AFL-style persistent loop reading data from the given file and passing it\n"
"  to the 'wasmEval' function together with the specified imports object."),

    JS_FS_HELP_END
};

static const JSFunctionSpecWithHelp console_functions[] = {
    JS_FN_HELP("log", Print, 0, 0,
"log([exp ...])",
"  Evaluate and print expressions to stdout.\n"
"  This function is an alias of the print() function."),
    JS_FS_HELP_END
};

bool
DefineConsole(JSContext* cx, HandleObject global)
{
    RootedObject obj(cx, JS_NewPlainObject(cx));
    return obj &&
           JS_DefineFunctionsWithHelp(cx, obj, console_functions) &&
           JS_DefineProperty(cx, global, "console", obj, 0);
}

#ifdef MOZ_PROFILING
# define PROFILING_FUNCTION_COUNT 5
# ifdef MOZ_CALLGRIND
#  define CALLGRIND_FUNCTION_COUNT 3
# else
#  define CALLGRIND_FUNCTION_COUNT 0
# endif
# ifdef MOZ_VTUNE
#  define VTUNE_FUNCTION_COUNT 4
# else
#  define VTUNE_FUNCTION_COUNT 0
# endif
# define EXTERNAL_FUNCTION_COUNT (PROFILING_FUNCTION_COUNT + CALLGRIND_FUNCTION_COUNT + VTUNE_FUNCTION_COUNT)
#else
# define EXTERNAL_FUNCTION_COUNT 0
#endif

#undef PROFILING_FUNCTION_COUNT
#undef CALLGRIND_FUNCTION_COUNT
#undef VTUNE_FUNCTION_COUNT
#undef EXTERNAL_FUNCTION_COUNT

static bool
PrintHelpString(JSContext* cx, HandleValue v)
{
    JSString* str = v.toString();
    MOZ_ASSERT(gOutFile->isOpen());

    JSLinearString* linear = str->ensureLinear(cx);
    if (!linear)
        return false;

    JS::AutoCheckCannotGC nogc;
    if (linear->hasLatin1Chars()) {
        for (const Latin1Char* p = linear->latin1Chars(nogc); *p; p++)
            fprintf(gOutFile->fp, "%c", char(*p));
    } else {
        for (const char16_t* p = linear->twoByteChars(nogc); *p; p++)
            fprintf(gOutFile->fp, "%c", char(*p));
    }
    fprintf(gOutFile->fp, "\n");

    return true;
}

static bool
PrintHelp(JSContext* cx, HandleObject obj)
{
    RootedValue usage(cx);
    if (!JS_GetProperty(cx, obj, "usage", &usage))
        return false;
    RootedValue help(cx);
    if (!JS_GetProperty(cx, obj, "help", &help))
        return false;

    if (!usage.isString() || !help.isString())
        return true;

    return PrintHelpString(cx, usage) && PrintHelpString(cx, help);
}

static bool
PrintEnumeratedHelp(JSContext* cx, HandleObject obj, bool brief)
{
    AutoIdVector idv(cx);
    if (!GetPropertyKeys(cx, obj, JSITER_OWNONLY | JSITER_HIDDEN, &idv))
        return false;

    for (size_t i = 0; i < idv.length(); i++) {
        RootedValue v(cx);
        RootedId id(cx, idv[i]);
        if (!JS_GetPropertyById(cx, obj, id, &v))
            return false;
        if (v.isObject()) {
            RootedObject funcObj(cx, &v.toObject());
            if (!PrintHelp(cx, funcObj))
                return false;
        }
    }

    return true;
}

static bool
Help(JSContext* cx, unsigned argc, Value* vp)
{
    if (!gOutFile->isOpen()) {
        JS_ReportErrorASCII(cx, "output file is closed");
        return false;
    }

    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject obj(cx);
    if (args.length() == 0) {
        fprintf(gOutFile->fp, "%s\n", JS_GetImplementationVersion());

        RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
        if (!PrintEnumeratedHelp(cx, global, false))
            return false;
    } else {
        for (unsigned i = 0; i < args.length(); i++) {
            if (args[i].isPrimitive()) {
                JS_ReportErrorASCII(cx, "primitive arg");
                return false;
            }
            obj = args[i].toObjectOrNull();
            if (!PrintHelp(cx, obj))
                return false;
        }
    }

    args.rval().setUndefined();
    return true;
}

static const JSErrorFormatString jsShell_ErrorFormatString[JSShellErr_Limit] = {
#define MSG_DEF(name, count, exception, format) \
    { #name, format, count, JSEXN_ERR } ,
#include "jsshell.msg"
#undef MSG_DEF
};

const JSErrorFormatString*
js::shell::my_GetErrorMessage(void* userRef, const unsigned errorNumber)
{
    if (errorNumber == 0 || errorNumber >= JSShellErr_Limit)
        return nullptr;

    return &jsShell_ErrorFormatString[errorNumber];
}

static bool
CreateLastWarningObject(JSContext* cx, JSErrorReport* report)
{
    RootedObject warningObj(cx, JS_NewObject(cx, nullptr));
    if (!warningObj)
        return false;

    RootedString nameStr(cx);
    if (report->exnType == JSEXN_WARN)
        nameStr = JS_NewStringCopyZ(cx, "Warning");
    else
        nameStr = GetErrorTypeName(cx, report->exnType);
    if (!nameStr)
        return false;
    RootedValue nameVal(cx, StringValue(nameStr));
    if (!DefineProperty(cx, warningObj, cx->names().name, nameVal))
        return false;

    RootedString messageStr(cx, report->newMessageString(cx));
    if (!messageStr)
        return false;
    RootedValue messageVal(cx, StringValue(messageStr));
    if (!DefineProperty(cx, warningObj, cx->names().message, messageVal))
        return false;

    RootedValue linenoVal(cx, Int32Value(report->lineno));
    if (!DefineProperty(cx, warningObj, cx->names().lineNumber, linenoVal))
        return false;

    RootedValue columnVal(cx, Int32Value(report->column));
    if (!DefineProperty(cx, warningObj, cx->names().columnNumber, columnVal))
        return false;

    RootedObject notesArray(cx, CreateErrorNotesArray(cx, report));
    if (!notesArray)
        return false;

    RootedValue notesArrayVal(cx, ObjectValue(*notesArray));
    if (!DefineProperty(cx, warningObj, cx->names().notes, notesArrayVal))
        return false;

    GetShellContext(cx)->lastWarning.setObject(*warningObj);
    return true;
}

static FILE*
ErrorFilePointer()
{
    if (gErrFile->isOpen())
        return gErrFile->fp;

    fprintf(stderr, "error file is closed; falling back to stderr\n");
    return stderr;
}

static bool
PrintStackTrace(JSContext* cx, HandleValue exn)
{
    if (!exn.isObject())
        return false;

    Maybe<JSAutoCompartment> ac;
    RootedObject exnObj(cx, &exn.toObject());
    if (IsCrossCompartmentWrapper(exnObj)) {
        exnObj = UncheckedUnwrap(exnObj);
        ac.emplace(cx, exnObj);
    }

    // Ignore non-ErrorObject thrown by |throw| statement.
    if (!exnObj->is<ErrorObject>())
        return true;

    // Exceptions thrown while compiling top-level script have no stack.
    RootedObject stackObj(cx, exnObj->as<ErrorObject>().stack());
    if (!stackObj)
        return true;

    RootedString stackStr(cx);
    if (!BuildStackString(cx, stackObj, &stackStr, 2))
        return false;

    UniqueChars stack(JS_EncodeStringToUTF8(cx, stackStr));
    if (!stack)
        return false;

    FILE* fp = ErrorFilePointer();
    fputs("Stack:\n", fp);
    fputs(stack.get(), fp);

    return true;
}

js::shell::AutoReportException::~AutoReportException()
{
    if (!JS_IsExceptionPending(cx))
        return;

    // Get exception object before printing and clearing exception.
    RootedValue exn(cx);
    (void) JS_GetPendingException(cx, &exn);

    JS_ClearPendingException(cx);

    ShellContext* sc = GetShellContext(cx);
    js::ErrorReport report(cx);
    if (!report.init(cx, exn, js::ErrorReport::WithSideEffects)) {
        fprintf(stderr, "out of memory initializing ErrorReport\n");
        fflush(stderr);
        JS_ClearPendingException(cx);
        return;
    }

    MOZ_ASSERT(!JSREPORT_IS_WARNING(report.report()->flags));

    FILE* fp = ErrorFilePointer();
    PrintError(cx, fp, report.toStringResult(), report.report(), reportWarnings);

    {
        JS::AutoSaveExceptionState savedExc(cx);
        if (!PrintStackTrace(cx, exn))
            fputs("(Unable to print stack trace)\n", fp);
        savedExc.restore();
    }

    if (report.report()->errorNumber == JSMSG_OUT_OF_MEMORY)
        sc->exitCode = EXITCODE_OUT_OF_MEMORY;
    else
        sc->exitCode = EXITCODE_RUNTIME_ERROR;

    JS_ClearPendingException(cx);
}

void
js::shell::WarningReporter(JSContext* cx, JSErrorReport* report)
{
    ShellContext* sc = GetShellContext(cx);
    FILE* fp = ErrorFilePointer();

    MOZ_ASSERT(report);
    MOZ_ASSERT(JSREPORT_IS_WARNING(report->flags));

    if (sc->lastWarningEnabled) {
        JS::AutoSaveExceptionState savedExc(cx);
        if (!CreateLastWarningObject(cx, report)) {
            fputs("Unhandled error happened while creating last warning object.\n", fp);
            fflush(fp);
        }
        savedExc.restore();
    }

    // Print the warning.
    PrintError(cx, fp, JS::ConstUTF8CharsZ(), report, reportWarnings);
}

static bool
global_enumerate(JSContext* cx, HandleObject obj)
{
#ifdef LAZY_STANDARD_CLASSES
    return JS_EnumerateStandardClasses(cx, obj);
#else
    return true;
#endif
}

static bool
global_resolve(JSContext* cx, HandleObject obj, HandleId id, bool* resolvedp)
{
#ifdef LAZY_STANDARD_CLASSES
    if (!JS_ResolveStandardClass(cx, obj, id, resolvedp))
        return false;
#endif
    return true;
}

static bool
global_mayResolve(const JSAtomState& names, jsid id, JSObject* maybeObj)
{
    return JS_MayResolveStandardClass(names, id, maybeObj);
}

static const JSClassOps global_classOps = {
    nullptr, nullptr, nullptr, nullptr,
    global_enumerate, global_resolve, global_mayResolve,
    nullptr,
    nullptr, nullptr, nullptr,
    JS_GlobalObjectTraceHook
};

static const JSClass global_class = {
    "global", JSCLASS_GLOBAL_FLAGS,
    &global_classOps
};

/*
 * Define a FakeDOMObject constructor. It returns an object with a getter,
 * setter and method with attached JitInfo. This object can be used to test
 * IonMonkey DOM optimizations in the shell.
 */
static const uint32_t DOM_OBJECT_SLOT = 0;

static bool
dom_genericGetter(JSContext* cx, unsigned argc, JS::Value* vp);

static bool
dom_genericSetter(JSContext* cx, unsigned argc, JS::Value* vp);

static bool
dom_genericMethod(JSContext* cx, unsigned argc, JS::Value* vp);

#ifdef DEBUG
static const JSClass* GetDomClass();
#endif

static bool
dom_get_x(JSContext* cx, HandleObject obj, void* self, JSJitGetterCallArgs args)
{
    MOZ_ASSERT(JS_GetClass(obj) == GetDomClass());
    MOZ_ASSERT(self == (void*)0x1234);
    args.rval().set(JS_NumberValue(double(3.14)));
    return true;
}

static bool
dom_set_x(JSContext* cx, HandleObject obj, void* self, JSJitSetterCallArgs args)
{
    MOZ_ASSERT(JS_GetClass(obj) == GetDomClass());
    MOZ_ASSERT(self == (void*)0x1234);
    return true;
}

static bool
dom_doFoo(JSContext* cx, HandleObject obj, void* self, const JSJitMethodCallArgs& args)
{
    MOZ_ASSERT(JS_GetClass(obj) == GetDomClass());
    MOZ_ASSERT(self == (void*)0x1234);

    /* Just return args.length(). */
    args.rval().setInt32(args.length());
    return true;
}

static const JSJitInfo dom_x_getterinfo = {
    { (JSJitGetterOp)dom_get_x },
    { 0 },    /* protoID */
    { 0 },    /* depth */
    JSJitInfo::AliasNone, /* aliasSet */
    JSJitInfo::Getter,
    JSVAL_TYPE_UNKNOWN, /* returnType */
    true,     /* isInfallible. False in setters. */
    true,     /* isMovable */
    true,     /* isEliminatable */
    false,    /* isAlwaysInSlot */
    false,    /* isLazilyCachedInSlot */
    false,    /* isTypedMethod */
    0         /* slotIndex */
};

static const JSJitInfo dom_x_setterinfo = {
    { (JSJitGetterOp)dom_set_x },
    { 0 },    /* protoID */
    { 0 },    /* depth */
    JSJitInfo::Setter,
    JSJitInfo::AliasEverything, /* aliasSet */
    JSVAL_TYPE_UNKNOWN, /* returnType */
    false,    /* isInfallible. False in setters. */
    false,    /* isMovable. */
    false,    /* isEliminatable. */
    false,    /* isAlwaysInSlot */
    false,    /* isLazilyCachedInSlot */
    false,    /* isTypedMethod */
    0         /* slotIndex */
};

static const JSJitInfo doFoo_methodinfo = {
    { (JSJitGetterOp)dom_doFoo },
    { 0 },    /* protoID */
    { 0 },    /* depth */
    JSJitInfo::Method,
    JSJitInfo::AliasEverything, /* aliasSet */
    JSVAL_TYPE_UNKNOWN, /* returnType */
    false,    /* isInfallible. False in setters. */
    false,    /* isMovable */
    false,    /* isEliminatable */
    false,    /* isAlwaysInSlot */
    false,    /* isLazilyCachedInSlot */
    false,    /* isTypedMethod */
    0         /* slotIndex */
};

static const JSPropertySpec dom_props[] = {
    {"x",
     JSPROP_SHARED | JSPROP_ENUMERATE,
     { {
        { { dom_genericGetter, &dom_x_getterinfo } },
        { { dom_genericSetter, &dom_x_setterinfo } }
     } },
    },
    JS_PS_END
};

static const JSFunctionSpec dom_methods[] = {
    JS_FNINFO("doFoo", dom_genericMethod, &doFoo_methodinfo, 3, JSPROP_ENUMERATE),
    JS_FS_END
};

static const JSClass dom_class = {
    "FakeDOMObject", JSCLASS_IS_DOMJSCLASS | JSCLASS_HAS_RESERVED_SLOTS(2)
};

#ifdef DEBUG
static const JSClass* GetDomClass() {
    return &dom_class;
}
#endif

static bool
dom_genericGetter(JSContext* cx, unsigned argc, JS::Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
    if (!obj)
        return false;

    if (JS_GetClass(obj) != &dom_class) {
        args.rval().set(UndefinedValue());
        return true;
    }

    JS::Value val = js::GetReservedSlot(obj, DOM_OBJECT_SLOT);

    const JSJitInfo* info = FUNCTION_VALUE_TO_JITINFO(args.calleev());
    MOZ_ASSERT(info->type() == JSJitInfo::Getter);
    JSJitGetterOp getter = info->getter;
    return getter(cx, obj, val.toPrivate(), JSJitGetterCallArgs(args));
}

static bool
dom_genericSetter(JSContext* cx, unsigned argc, JS::Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
    if (!obj)
        return false;

    MOZ_ASSERT(args.length() == 1);

    if (JS_GetClass(obj) != &dom_class) {
        args.rval().set(UndefinedValue());
        return true;
    }

    JS::Value val = js::GetReservedSlot(obj, DOM_OBJECT_SLOT);

    const JSJitInfo* info = FUNCTION_VALUE_TO_JITINFO(args.calleev());
    MOZ_ASSERT(info->type() == JSJitInfo::Setter);
    JSJitSetterOp setter = info->setter;
    if (!setter(cx, obj, val.toPrivate(), JSJitSetterCallArgs(args)))
        return false;
    args.rval().set(UndefinedValue());
    return true;
}

static bool
dom_genericMethod(JSContext* cx, unsigned argc, JS::Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
    if (!obj)
        return false;

    if (JS_GetClass(obj) != &dom_class) {
        args.rval().set(UndefinedValue());
        return true;
    }

    JS::Value val = js::GetReservedSlot(obj, DOM_OBJECT_SLOT);

    const JSJitInfo* info = FUNCTION_VALUE_TO_JITINFO(args.calleev());
    MOZ_ASSERT(info->type() == JSJitInfo::Method);
    JSJitMethodOp method = info->method;
    return method(cx, obj, val.toPrivate(), JSJitMethodCallArgs(args));
}

static void
InitDOMObject(HandleObject obj)
{
    /* Fow now just initialize to a constant we can check. */
    SetReservedSlot(obj, DOM_OBJECT_SLOT, PrivateValue((void*)0x1234));
}

static bool
dom_constructor(JSContext* cx, unsigned argc, JS::Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject callee(cx, &args.callee());
    RootedValue protov(cx);
    if (!GetProperty(cx, callee, callee, cx->names().prototype, &protov))
        return false;

    if (!protov.isObject()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_BAD_PROTOTYPE,
                                  "FakeDOMObject");
        return false;
    }

    RootedObject proto(cx, &protov.toObject());
    RootedObject domObj(cx, JS_NewObjectWithGivenProto(cx, &dom_class, proto));
    if (!domObj)
        return false;

    InitDOMObject(domObj);

    args.rval().setObject(*domObj);
    return true;
}

static bool
InstanceClassHasProtoAtDepth(const Class* clasp, uint32_t protoID, uint32_t depth)
{
    /* There's only a single (fake) DOM object in the shell, so just return true. */
    return true;
}

class ScopedFileDesc
{
    intptr_t fd_;
  public:
    enum LockType { READ_LOCK, WRITE_LOCK };
    ScopedFileDesc(int fd, LockType lockType)
      : fd_(fd)
    {
        if (fd == -1)
            return;
        if (!jsCacheOpened.compareExchange(false, true)) {
            close(fd_);
            fd_ = -1;
            return;
        }
    }
    ~ScopedFileDesc() {
        if (fd_ == -1)
            return;
        MOZ_ASSERT(jsCacheOpened == true);
        jsCacheOpened = false;
        close(fd_);
    }
    operator intptr_t() const {
        return fd_;
    }
    intptr_t forget() {
        intptr_t ret = fd_;
        fd_ = -1;
        return ret;
    }
};

// To guard against corrupted cache files generated by previous crashes, write
// asmJSCacheCookie to the first uint32_t of the file only after the file is
// fully serialized and flushed to disk.
static const uint32_t asmJSCacheCookie = 0xabbadaba;

static bool
ShellOpenAsmJSCacheEntryForRead(HandleObject global, const char16_t* begin, const char16_t* limit,
                                size_t* serializedSizeOut, const uint8_t** memoryOut,
                                intptr_t* handleOut)
{
    if (!jsCachingEnabled || !jsCacheAsmJSPath)
        return false;

    ScopedFileDesc fd(open(jsCacheAsmJSPath, O_RDWR), ScopedFileDesc::READ_LOCK);
    if (fd == -1)
        return false;

    // Get the size and make sure we can dereference at least one uint32_t.
    off_t off = lseek(fd, 0, SEEK_END);
    if (off == -1 || off < (off_t)sizeof(uint32_t))
        return false;

    // Map the file into memory.
    void* memory;
#ifdef XP_WIN
    HANDLE fdOsHandle = (HANDLE)_get_osfhandle(fd);
    HANDLE fileMapping = CreateFileMapping(fdOsHandle, nullptr, PAGE_READWRITE, 0, 0, nullptr);
    if (!fileMapping)
        return false;

    memory = MapViewOfFile(fileMapping, FILE_MAP_READ, 0, 0, 0);
    CloseHandle(fileMapping);
    if (!memory)
        return false;
#else
    memory = mmap(nullptr, off, PROT_READ, MAP_SHARED, fd, 0);
    if (memory == MAP_FAILED)
        return false;
#endif

    // Perform check described by asmJSCacheCookie comment.
    if (*(uint32_t*)memory != asmJSCacheCookie) {
#ifdef XP_WIN
        UnmapViewOfFile(memory);
#else
        munmap(memory, off);
#endif
        return false;
    }

    // The embedding added the cookie so strip it off of the buffer returned to
    // the JS engine.
    *serializedSizeOut = off - sizeof(uint32_t);
    *memoryOut = (uint8_t*)memory + sizeof(uint32_t);
    *handleOut = fd.forget();
    return true;
}

static void
ShellCloseAsmJSCacheEntryForRead(size_t serializedSize, const uint8_t* memory, intptr_t handle)
{
    // Undo the cookie adjustment done when opening the file.
    memory -= sizeof(uint32_t);
    serializedSize += sizeof(uint32_t);

    // Release the memory mapping and file.
#ifdef XP_WIN
    UnmapViewOfFile(const_cast<uint8_t*>(memory));
#else
    munmap(const_cast<uint8_t*>(memory), serializedSize);
#endif

    MOZ_ASSERT(jsCacheOpened == true);
    jsCacheOpened = false;
    close(handle);
}

static JS::AsmJSCacheResult
ShellOpenAsmJSCacheEntryForWrite(HandleObject global, const char16_t* begin,
                                 const char16_t* end, size_t serializedSize,
                                 uint8_t** memoryOut, intptr_t* handleOut)
{
    if (!jsCachingEnabled || !jsCacheAsmJSPath)
        return JS::AsmJSCache_Disabled_ShellFlags;

    // Create the cache directory if it doesn't already exist.
    struct stat dirStat;
    if (stat(jsCacheDir, &dirStat) == 0) {
        if (!(dirStat.st_mode & S_IFDIR))
            return JS::AsmJSCache_InternalError;
    } else {
#ifdef XP_WIN
        if (mkdir(jsCacheDir) != 0)
            return JS::AsmJSCache_InternalError;
#else
        if (mkdir(jsCacheDir, 0777) != 0)
            return JS::AsmJSCache_InternalError;
#endif
    }

    ScopedFileDesc fd(open(jsCacheAsmJSPath, O_CREAT|O_RDWR, 0660), ScopedFileDesc::WRITE_LOCK);
    if (fd == -1)
        return JS::AsmJSCache_InternalError;

    // Include extra space for the asmJSCacheCookie.
    serializedSize += sizeof(uint32_t);

    // Resize the file to the appropriate size after zeroing their contents.
#ifdef XP_WIN
    if (chsize(fd, 0))
        return JS::AsmJSCache_InternalError;
    if (chsize(fd, serializedSize))
        return JS::AsmJSCache_InternalError;
#else
    if (ftruncate(fd, 0))
        return JS::AsmJSCache_InternalError;
    if (ftruncate(fd, serializedSize))
        return JS::AsmJSCache_InternalError;
#endif

    // Map the file into memory.
    void* memory;
#ifdef XP_WIN
    HANDLE fdOsHandle = (HANDLE)_get_osfhandle(fd);
    HANDLE fileMapping = CreateFileMapping(fdOsHandle, nullptr, PAGE_READWRITE, 0, 0, nullptr);
    if (!fileMapping)
        return JS::AsmJSCache_InternalError;

    memory = MapViewOfFile(fileMapping, FILE_MAP_WRITE, 0, 0, 0);
    CloseHandle(fileMapping);
    if (!memory)
        return JS::AsmJSCache_InternalError;
    MOZ_ASSERT(*(uint32_t*)memory == 0);
#else
    memory = mmap(nullptr, serializedSize, PROT_READ, MAP_SHARED, fd, 0);
    if (memory == MAP_FAILED)
        return JS::AsmJSCache_InternalError;
    MOZ_ASSERT(*(uint32_t*)memory == 0);
    if (mprotect(memory, serializedSize, PROT_WRITE))
        return JS::AsmJSCache_InternalError;
#endif

    // The embedding added the cookie so strip it off of the buffer returned to
    // the JS engine. The asmJSCacheCookie will be written on close, below.
    *memoryOut = (uint8_t*)memory + sizeof(uint32_t);
    *handleOut = fd.forget();
    return JS::AsmJSCache_Success;
}

static void
ShellCloseAsmJSCacheEntryForWrite(size_t serializedSize, uint8_t* memory, intptr_t handle)
{
    // Undo the cookie adjustment done when opening the file.
    memory -= sizeof(uint32_t);
    serializedSize += sizeof(uint32_t);

    // Write the magic cookie value after flushing the entire cache entry.
#ifdef XP_WIN
    FlushViewOfFile(memory, serializedSize);
    FlushFileBuffers(HANDLE(_get_osfhandle(handle)));
#else
    msync(memory, serializedSize, MS_SYNC);
#endif

    MOZ_ASSERT(*(uint32_t*)memory == 0);
    *(uint32_t*)memory = asmJSCacheCookie;

    // Free the memory mapping and file.
#ifdef XP_WIN
    UnmapViewOfFile(const_cast<uint8_t*>(memory));
#else
    munmap(memory, serializedSize);
#endif

    MOZ_ASSERT(jsCacheOpened == true);
    jsCacheOpened = false;
    close(handle);
}

static bool
ShellBuildId(JS::BuildIdCharVector* buildId)
{
    // The browser embeds the date into the buildid and the buildid is embedded
    // in the binary, so every 'make' necessarily builds a new firefox binary.
    // Fortunately, the actual firefox executable is tiny -- all the code is in
    // libxul.so and other shared modules -- so this isn't a big deal. Not so
    // for the statically-linked JS shell. To avoid recompiling js.cpp and
    // re-linking 'js' on every 'make', we use a constant buildid and rely on
    // the shell user to manually clear the cache (deleting the dir passed to
    // --js-cache) between cache-breaking updates. Note: jit_tests.py does this
    // on every run).
    const char buildid[] = "JS-shell";
    return buildId->append(buildid, sizeof(buildid));
}

static const JS::AsmJSCacheOps asmJSCacheOps = {
    ShellOpenAsmJSCacheEntryForRead,
    ShellCloseAsmJSCacheEntryForRead,
    ShellOpenAsmJSCacheEntryForWrite,
    ShellCloseAsmJSCacheEntryForWrite
};

static bool
TimesAccessed(JSContext* cx, unsigned argc, Value* vp)
{
    static int32_t accessed = 0;
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setInt32(++accessed);
    return true;
}

static const JSPropertySpec TestingProperties[] = {
    JS_PSG("timesAccessed", TimesAccessed, 0),
    JS_PS_END
};

static JSObject*
NewGlobalObject(JSContext* cx, JS::CompartmentOptions& options,
                JSPrincipals* principals)
{
    RootedObject glob(cx, JS_NewGlobalObject(cx, &global_class, principals,
                                             JS::DontFireOnNewGlobalHook, options));
    if (!glob)
        return nullptr;

    {
        JSAutoCompartment ac(cx, glob);

#ifndef LAZY_STANDARD_CLASSES
        if (!JS_InitStandardClasses(cx, glob))
            return nullptr;
#endif

        bool succeeded;
        if (!JS_SetImmutablePrototype(cx, glob, &succeeded))
            return nullptr;
        MOZ_ASSERT(succeeded,
                   "a fresh, unexposed global object is always capable of "
                   "having its [[Prototype]] be immutable");

#ifdef JS_HAS_CTYPES
        if (!JS_InitCTypesClass(cx, glob))
            return nullptr;
#endif
        if (!JS_InitReflectParse(cx, glob))
            return nullptr;
        if (!JS_DefineDebuggerObject(cx, glob))
            return nullptr;
        if (!JS::RegisterPerfMeasurement(cx, glob))
            return nullptr;
        if (!JS_DefineFunctionsWithHelp(cx, glob, shell_functions) ||
            !JS_DefineProfilingFunctions(cx, glob))
        {
            return nullptr;
        }
        if (!js::DefineTestingFunctions(cx, glob, fuzzingSafe, disableOOMFunctions))
            return nullptr;
        if (!JS_DefineProperties(cx, glob, TestingProperties))
            return nullptr;

        if (!fuzzingSafe) {
            if (!JS_DefineFunctionsWithHelp(cx, glob, fuzzing_unsafe_functions))
                return nullptr;
            if (!DefineConsole(cx, glob))
                return nullptr;
        }

        if (!DefineOS(cx, glob, fuzzingSafe, &gOutFile, &gErrFile))
            return nullptr;

        RootedObject performanceObj(cx, JS_NewObject(cx, nullptr));
        if (!performanceObj)
            return nullptr;
        RootedObject mozMemoryObj(cx, JS_NewObject(cx, nullptr));
        if (!mozMemoryObj)
            return nullptr;
        RootedObject gcObj(cx, gc::NewMemoryInfoObject(cx));
        if (!gcObj)
            return nullptr;
        if (!JS_DefineProperty(cx, glob, "performance", performanceObj, JSPROP_ENUMERATE))
            return nullptr;
        if (!JS_DefineProperty(cx, performanceObj, "mozMemory", mozMemoryObj, JSPROP_ENUMERATE))
            return nullptr;
        if (!JS_DefineProperty(cx, mozMemoryObj, "gc", gcObj, JSPROP_ENUMERATE))
            return nullptr;

        /* Initialize FakeDOMObject. */
        static const js::DOMCallbacks DOMcallbacks = {
            InstanceClassHasProtoAtDepth
        };
        SetDOMCallbacks(cx, &DOMcallbacks);

        RootedObject domProto(cx, JS_InitClass(cx, glob, nullptr, &dom_class, dom_constructor,
                                               0, dom_props, dom_methods, nullptr, nullptr));
        if (!domProto)
            return nullptr;

        /* Initialize FakeDOMObject.prototype */
        InitDOMObject(domProto);

        JS_FireOnNewGlobalObject(cx, glob);
    }

    return glob;
}

static bool
BindScriptArgs(JSContext* cx, OptionParser* op)
{
    AutoReportException are(cx);

    MultiStringRange msr = op->getMultiStringArg("scriptArgs");
    RootedObject scriptArgs(cx);
    scriptArgs = JS_NewArrayObject(cx, 0);
    if (!scriptArgs)
        return false;

    if (!JS_DefineProperty(cx, cx->global(), "scriptArgs", scriptArgs, 0))
        return false;

    for (size_t i = 0; !msr.empty(); msr.popFront(), ++i) {
        const char* scriptArg = msr.front();
        JS::RootedString str(cx, JS_NewStringCopyZ(cx, scriptArg));
        if (!str ||
            !JS_DefineElement(cx, scriptArgs, i, str, JSPROP_ENUMERATE))
        {
            return false;
        }
    }

    const char* scriptPath = op->getStringArg("script");
    RootedValue scriptPathValue(cx);
    if (scriptPath) {
        RootedString scriptPathString(cx, JS_NewStringCopyZ(cx, scriptPath));
        if (!scriptPathString)
            return false;
        scriptPathValue = StringValue(scriptPathString);
    } else {
        scriptPathValue = UndefinedValue();
    }

    if (!JS_DefineProperty(cx, cx->global(), "scriptPath", scriptPathValue, 0))
        return false;

    return true;
}

static bool
OptionFailure(const char* option, const char* str)
{
    fprintf(stderr, "Unrecognized option for %s: %s\n", option, str);
    return false;
}

static MOZ_MUST_USE bool
ProcessArgs(JSContext* cx, OptionParser* op)
{
    ShellContext* sc = GetShellContext(cx);

    if (op->getBoolOption('s'))
        JS::ContextOptionsRef(cx).toggleExtraWarnings();

    /* |scriptArgs| gets bound on the global before any code is run. */
    if (!BindScriptArgs(cx, op))
        return false;

    MultiStringRange filePaths = op->getMultiStringOption('f');
    MultiStringRange codeChunks = op->getMultiStringOption('e');
    MultiStringRange modulePaths = op->getMultiStringOption('m');

    if (filePaths.empty() &&
        codeChunks.empty() &&
        modulePaths.empty() &&
        !op->getStringArg("script"))
    {
        return Process(cx, nullptr, true); /* Interactive. */
    }

    if (const char* path = op->getStringOption("module-load-path")) {
        RootedString jspath(cx, JS_NewStringCopyZ(cx, path));
        if (!jspath)
            return false;

        JSString* absolutePath = js::shell::ResolvePath(cx, jspath, RootRelative);
        if (!absolutePath)
            return false;

        sc->moduleLoadPath = UniqueChars(JS_EncodeString(cx, absolutePath));
    } else {
        sc->moduleLoadPath = js::shell::GetCWD();
    }

    if (!sc->moduleLoadPath)
        return false;

    if (!modulePaths.empty() && !InitModuleLoader(cx))
        return false;

    while (!filePaths.empty() || !codeChunks.empty() || !modulePaths.empty()) {
        size_t fpArgno = filePaths.empty() ? SIZE_MAX : filePaths.argno();
        size_t ccArgno = codeChunks.empty() ? SIZE_MAX : codeChunks.argno();
        size_t mpArgno = modulePaths.empty() ? SIZE_MAX : modulePaths.argno();

        if (fpArgno < ccArgno && fpArgno < mpArgno) {
            char* path = filePaths.front();
            if (!Process(cx, path, false, FileScript))
                return false;
            filePaths.popFront();
        } else if (ccArgno < fpArgno && ccArgno < mpArgno) {
            const char* code = codeChunks.front();
            RootedValue rval(cx);
            JS::CompileOptions opts(cx);
            opts.setFileAndLine("-e", 1);
            if (!JS::Evaluate(cx, opts, code, strlen(code), &rval))
                return false;
            codeChunks.popFront();
            if (sc->quitting)
                break;
        } else {
            MOZ_ASSERT(mpArgno < fpArgno && mpArgno < ccArgno);
            char* path = modulePaths.front();
            if (!Process(cx, path, false, FileModule))
                return false;
            modulePaths.popFront();
        }
    }

    if (sc->quitting)
        return false;

    /* The |script| argument is processed after all options. */
    if (const char* path = op->getStringArg("script")) {
        if (!Process(cx, path, false))
            return false;
    }

    if (op->getBoolOption('i')) {
        if (!Process(cx, nullptr, true))
            return false;
    }

    return true;
}

static bool
SetContextOptions(JSContext* cx, const OptionParser& op)
{
    enableBaseline = !op.getBoolOption("no-baseline");
    enableIon = !op.getBoolOption("no-ion");
    enableAsmJS = !op.getBoolOption("no-asmjs");
    enableWasm = !op.getBoolOption("no-wasm");
    enableNativeRegExp = !op.getBoolOption("no-native-regexp");
    enableUnboxedArrays = op.getBoolOption("unboxed-arrays");
    enableWasmAlwaysBaseline = op.getBoolOption("wasm-always-baseline");

    JS::ContextOptionsRef(cx).setBaseline(enableBaseline)
                             .setIon(enableIon)
                             .setAsmJS(enableAsmJS)
                             .setWasm(enableWasm)
                             .setWasmAlwaysBaseline(enableWasmAlwaysBaseline)
                             .setNativeRegExp(enableNativeRegExp)
                             .setUnboxedArrays(enableUnboxedArrays);

    if (op.getBoolOption("wasm-check-bce"))
        jit::JitOptions.wasmAlwaysCheckBounds = true;

    if (op.getBoolOption("wasm-test-mode"))
        jit::JitOptions.wasmTestMode = true;

    if (op.getBoolOption("no-unboxed-objects"))
        jit::JitOptions.disableUnboxedObjects = true;

    if (const char* str = op.getStringOption("cache-ir-stubs")) {
        if (strcmp(str, "on") == 0)
            jit::JitOptions.disableCacheIR = false;
        else if (strcmp(str, "off") == 0)
            jit::JitOptions.disableCacheIR = true;
        else
            return OptionFailure("cache-ir-stubs", str);
    }

    if (const char* str = op.getStringOption("ion-scalar-replacement")) {
        if (strcmp(str, "on") == 0)
            jit::JitOptions.disableScalarReplacement = false;
        else if (strcmp(str, "off") == 0)
            jit::JitOptions.disableScalarReplacement = true;
        else
            return OptionFailure("ion-scalar-replacement", str);
    }

    if (const char* str = op.getStringOption("ion-shared-stubs")) {
        if (strcmp(str, "on") == 0)
            jit::JitOptions.disableSharedStubs = false;
        else if (strcmp(str, "off") == 0)
            jit::JitOptions.disableSharedStubs = true;
        else
            return OptionFailure("ion-shared-stubs", str);
    }

    if (const char* str = op.getStringOption("ion-gvn")) {
        if (strcmp(str, "off") == 0) {
            jit::JitOptions.disableGvn = true;
        } else if (strcmp(str, "on") != 0 &&
                   strcmp(str, "optimistic") != 0 &&
                   strcmp(str, "pessimistic") != 0)
        {
            // We accept "pessimistic" and "optimistic" as synonyms for "on"
            // for backwards compatibility.
            return OptionFailure("ion-gvn", str);
        }
    }

    if (const char* str = op.getStringOption("ion-aa")) {
        if (strcmp(str, "flow-sensitive") == 0)
            jit::JitOptions.disableFlowAA = false;
        else if (strcmp(str, "flow-insensitive") == 0)
            jit::JitOptions.disableFlowAA = true;
        else
            return OptionFailure("ion-aa", str);
    }

    if (const char* str = op.getStringOption("ion-licm")) {
        if (strcmp(str, "on") == 0)
            jit::JitOptions.disableLicm = false;
        else if (strcmp(str, "off") == 0)
            jit::JitOptions.disableLicm = true;
        else
            return OptionFailure("ion-licm", str);
    }

    if (const char* str = op.getStringOption("ion-edgecase-analysis")) {
        if (strcmp(str, "on") == 0)
            jit::JitOptions.disableEdgeCaseAnalysis = false;
        else if (strcmp(str, "off") == 0)
            jit::JitOptions.disableEdgeCaseAnalysis = true;
        else
            return OptionFailure("ion-edgecase-analysis", str);
    }

    if (const char* str = op.getStringOption("ion-pgo")) {
        if (strcmp(str, "on") == 0)
            jit::JitOptions.disablePgo = false;
        else if (strcmp(str, "off") == 0)
            jit::JitOptions.disablePgo = true;
        else
            return OptionFailure("ion-pgo", str);
    }

    if (const char* str = op.getStringOption("ion-range-analysis")) {
        if (strcmp(str, "on") == 0)
            jit::JitOptions.disableRangeAnalysis = false;
        else if (strcmp(str, "off") == 0)
            jit::JitOptions.disableRangeAnalysis = true;
        else
            return OptionFailure("ion-range-analysis", str);
    }

    if (const char *str = op.getStringOption("ion-sincos")) {
        if (strcmp(str, "on") == 0)
            jit::JitOptions.disableSincos = false;
        else if (strcmp(str, "off") == 0)
            jit::JitOptions.disableSincos = true;
        else
            return OptionFailure("ion-sincos", str);
    }

    if (const char* str = op.getStringOption("ion-sink")) {
        if (strcmp(str, "on") == 0)
            jit::JitOptions.disableSink = false;
        else if (strcmp(str, "off") == 0)
            jit::JitOptions.disableSink = true;
        else
            return OptionFailure("ion-sink", str);
    }

    if (const char* str = op.getStringOption("ion-loop-unrolling")) {
        if (strcmp(str, "on") == 0)
            jit::JitOptions.disableLoopUnrolling = false;
        else if (strcmp(str, "off") == 0)
            jit::JitOptions.disableLoopUnrolling = true;
        else
            return OptionFailure("ion-loop-unrolling", str);
    }

    if (const char* str = op.getStringOption("ion-instruction-reordering")) {
        if (strcmp(str, "on") == 0)
            jit::JitOptions.disableInstructionReordering = false;
        else if (strcmp(str, "off") == 0)
            jit::JitOptions.disableInstructionReordering = true;
        else
            return OptionFailure("ion-instruction-reordering", str);
    }

    if (op.getBoolOption("ion-check-range-analysis"))
        jit::JitOptions.checkRangeAnalysis = true;

    if (op.getBoolOption("ion-extra-checks"))
        jit::JitOptions.runExtraChecks = true;

    if (const char* str = op.getStringOption("ion-inlining")) {
        if (strcmp(str, "on") == 0)
            jit::JitOptions.disableInlining = false;
        else if (strcmp(str, "off") == 0)
            jit::JitOptions.disableInlining = true;
        else
            return OptionFailure("ion-inlining", str);
    }

    if (const char* str = op.getStringOption("ion-osr")) {
        if (strcmp(str, "on") == 0)
            jit::JitOptions.osr = true;
        else if (strcmp(str, "off") == 0)
            jit::JitOptions.osr = false;
        else
            return OptionFailure("ion-osr", str);
    }

    if (const char* str = op.getStringOption("ion-limit-script-size")) {
        if (strcmp(str, "on") == 0)
            jit::JitOptions.limitScriptSize = true;
        else if (strcmp(str, "off") == 0)
            jit::JitOptions.limitScriptSize = false;
        else
            return OptionFailure("ion-limit-script-size", str);
    }

    int32_t warmUpThreshold = op.getIntOption("ion-warmup-threshold");
    if (warmUpThreshold >= 0)
        jit::JitOptions.setCompilerWarmUpThreshold(warmUpThreshold);

    warmUpThreshold = op.getIntOption("baseline-warmup-threshold");
    if (warmUpThreshold >= 0)
        jit::JitOptions.baselineWarmUpThreshold = warmUpThreshold;

    if (op.getBoolOption("baseline-eager"))
        jit::JitOptions.baselineWarmUpThreshold = 0;

    if (const char* str = op.getStringOption("ion-regalloc")) {
        jit::JitOptions.forcedRegisterAllocator = jit::LookupRegisterAllocator(str);
        if (!jit::JitOptions.forcedRegisterAllocator.isSome())
            return OptionFailure("ion-regalloc", str);
    }

    if (op.getBoolOption("ion-eager"))
        jit::JitOptions.setEagerCompilation();

    offthreadCompilation = true;
    if (const char* str = op.getStringOption("ion-offthread-compile")) {
        if (strcmp(str, "off") == 0)
            offthreadCompilation = false;
        else if (strcmp(str, "on") != 0)
            return OptionFailure("ion-offthread-compile", str);
    }
    cx->runtime()->setOffthreadIonCompilationEnabled(offthreadCompilation);

    if (op.getStringOption("ion-parallel-compile")) {
        fprintf(stderr, "--ion-parallel-compile is deprecated. Please use --ion-offthread-compile instead.\n");
        return false;
    }

#ifdef ENABLE_SHARED_ARRAY_BUFFER
    if (const char* str = op.getStringOption("shared-memory")) {
        if (strcmp(str, "off") == 0)
            enableSharedMemory = false;
        else if (strcmp(str, "on") == 0)
            enableSharedMemory = true;
        else
            return OptionFailure("shared-memory", str);
    }
#endif

#if defined(JS_CODEGEN_ARM)
    if (const char* str = op.getStringOption("arm-hwcap"))
        jit::ParseARMHwCapFlags(str);

    int32_t fill = op.getIntOption("arm-asm-nop-fill");
    if (fill >= 0)
        jit::Assembler::NopFill = fill;

    int32_t poolMaxOffset = op.getIntOption("asm-pool-max-offset");
    if (poolMaxOffset >= 5 && poolMaxOffset <= 1024)
        jit::Assembler::AsmPoolMaxOffset = poolMaxOffset;
#endif

#if defined(JS_SIMULATOR_ARM)
    if (op.getBoolOption("arm-sim-icache-checks"))
        jit::SimulatorProcess::ICacheCheckingDisableCount = 0;

    int32_t stopAt = op.getIntOption("arm-sim-stop-at");
    if (stopAt >= 0)
        jit::Simulator::StopSimAt = stopAt;
#elif defined(JS_SIMULATOR_MIPS32) || defined(JS_SIMULATOR_MIPS64)
    if (op.getBoolOption("mips-sim-icache-checks"))
        jit::Simulator::ICacheCheckingEnabled = true;

    int32_t stopAt = op.getIntOption("mips-sim-stop-at");
    if (stopAt >= 0)
        jit::Simulator::StopSimAt = stopAt;
#endif

    reportWarnings = op.getBoolOption('w');
    compileOnly = op.getBoolOption('c');
    printTiming = op.getBoolOption('b');
    enableCodeCoverage = op.getBoolOption("code-coverage");
    enableDisassemblyDumps = op.getBoolOption('D');
    cx->runtime()->profilingScripts = enableCodeCoverage || enableDisassemblyDumps;

    jsCacheDir = op.getStringOption("js-cache");
    if (jsCacheDir) {
        if (!op.getBoolOption("no-js-cache-per-process"))
            jsCacheDir = JS_smprintf("%s/%u", jsCacheDir, (unsigned)getpid());
        else
            jsCacheDir = JS_strdup(cx, jsCacheDir);
        if (!jsCacheDir)
            return false;
        jsCacheAsmJSPath = JS_smprintf("%s/asmjs.cache", jsCacheDir);
    }

#ifdef DEBUG
    dumpEntrainedVariables = op.getBoolOption("dump-entrained-variables");
#endif

#ifdef JS_GC_ZEAL
    const char* zealStr = op.getStringOption("gc-zeal");
    if (zealStr) {
        if (!cx->runtime()->gc.parseAndSetZeal(zealStr))
            return false;
        uint32_t nextScheduled;
        cx->runtime()->gc.getZealBits(&gZealBits, &gZealFrequency, &nextScheduled);
    }
#endif

    return true;
}

static void
SetWorkerContextOptions(JSContext* cx)
{
    // Copy option values from the main thread.
    JS::ContextOptionsRef(cx).setBaseline(enableBaseline)
                             .setIon(enableIon)
                             .setAsmJS(enableAsmJS)
                             .setWasm(enableWasm)
                             .setWasmAlwaysBaseline(enableWasmAlwaysBaseline)
                             .setNativeRegExp(enableNativeRegExp)
                             .setUnboxedArrays(enableUnboxedArrays);
    cx->runtime()->setOffthreadIonCompilationEnabled(offthreadCompilation);
    cx->runtime()->profilingScripts = enableCodeCoverage || enableDisassemblyDumps;

#ifdef JS_GC_ZEAL
    if (gZealBits && gZealFrequency) {
#define ZEAL_MODE(_, value)                        \
        if (gZealBits & (1 << value))              \
            cx->runtime()->gc.setZeal(value, gZealFrequency);
        JS_FOR_EACH_ZEAL_MODE(ZEAL_MODE)
#undef ZEAL_MODE
    }
#endif

    JS_SetNativeStackQuota(cx, gMaxStackSize);
}

static int
Shell(JSContext* cx, OptionParser* op, char** envp)
{
    Maybe<JS::AutoDisableGenerationalGC> noggc;
    if (op->getBoolOption("no-ggc"))
        noggc.emplace(cx);

    Maybe<AutoDisableCompactingGC> nocgc;
    if (op->getBoolOption("no-cgc"))
        nocgc.emplace(cx);

    JSAutoRequest ar(cx);

    if (op->getBoolOption("fuzzing-safe"))
        fuzzingSafe = true;
    else
        fuzzingSafe = (getenv("MOZ_FUZZING_SAFE") && getenv("MOZ_FUZZING_SAFE")[0] != '0');

    if (op->getBoolOption("disable-oom-functions"))
        disableOOMFunctions = true;

    JS::CompartmentOptions options;
    SetStandardCompartmentOptions(options);
    RootedObject glob(cx, NewGlobalObject(cx, options, nullptr));
    if (!glob)
        return 1;

    JSAutoCompartment ac(cx, glob);

    ShellContext* sc = GetShellContext(cx);
    int result = EXIT_SUCCESS;
    {
        AutoReportException are(cx);
        if (!ProcessArgs(cx, op) && !sc->quitting)
            result = EXITCODE_RUNTIME_ERROR;
    }

    /*
     * The job queue must be drained even on error to finish outstanding async
     * tasks before the main thread JSRuntime is torn down. Drain after
     * uncaught exceptions have been reported since draining runs callbacks.
     */
    DrainJobQueue(cx);

    if (sc->exitCode)
        result = sc->exitCode;

    if (enableDisassemblyDumps) {
        AutoReportException are(cx);
        if (!js::DumpCompartmentPCCounts(cx))
            result = EXITCODE_OUT_OF_MEMORY;
    }

    if (!op->getBoolOption("no-js-cache-per-process")) {
        if (jsCacheAsmJSPath) {
            unlink(jsCacheAsmJSPath);
            JS_free(cx, const_cast<char*>(jsCacheAsmJSPath));
        }
        if (jsCacheDir) {
            rmdir(jsCacheDir);
            JS_free(cx, const_cast<char*>(jsCacheDir));
        }
    }

    /*
     * Dump remaining type inference results while we still have a context.
     * This printing depends on atoms still existing.
     */
    for (CompartmentsIter c(cx->runtime(), SkipAtoms); !c.done(); c.next())
        PrintTypes(cx, c, false);

    return result;
}

static void
SetOutputFile(const char* const envVar,
              RCFile* defaultOut,
              RCFile** outFileP)
{
    RCFile* outFile;

    const char* outPath = getenv(envVar);
    FILE* newfp;
    if (outPath && *outPath && (newfp = fopen(outPath, "w")))
        outFile = js_new<RCFile>(newfp);
    else
        outFile = defaultOut;

    outFile->acquire();
    *outFileP = outFile;
}

static void
PreInit()
{
#ifdef XP_WIN
    const char* crash_option = getenv("XRE_NO_WINDOWS_CRASH_DIALOG");
    if (crash_option && crash_option[0] == '1') {
        // Disable the segfault dialog. We want to fail the tests immediately
        // instead of hanging automation.
        UINT newMode = SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX;
        UINT prevMode = SetErrorMode(newMode);
        SetErrorMode(prevMode | newMode);
    }
#endif
}

int
main(int argc, char** argv, char** envp)
{
    PreInit();

    sArgc = argc;
    sArgv = argv;

    int result;

#ifdef HAVE_SETLOCALE
    setlocale(LC_ALL, "");
#endif

    // Special-case stdout and stderr. We bump their refcounts to prevent them
    // from getting closed and then having some printf fail somewhere.
    RCFile rcStdout(stdout);
    rcStdout.acquire();
    RCFile rcStderr(stderr);
    rcStderr.acquire();

    SetOutputFile("JS_STDOUT", &rcStdout, &gOutFile);
    SetOutputFile("JS_STDERR", &rcStderr, &gErrFile);

    OptionParser op("Usage: {progname} [options] [[script] scriptArgs*]");

    op.setDescription("The SpiderMonkey shell provides a command line interface to the "
        "JavaScript engine. Code and file options provided via the command line are "
        "run left to right. If provided, the optional script argument is run after "
        "all options have been processed. Just-In-Time compilation modes may be enabled via "
        "command line options.");
    op.setDescriptionWidth(72);
    op.setHelpWidth(80);
    op.setVersion(JS_GetImplementationVersion());

    if (!op.addMultiStringOption('f', "file", "PATH", "File path to run")
        || !op.addMultiStringOption('m', "module", "PATH", "Module path to run")
        || !op.addMultiStringOption('e', "execute", "CODE", "Inline code to run")
        || !op.addBoolOption('i', "shell", "Enter prompt after running code")
        || !op.addBoolOption('c', "compileonly", "Only compile, don't run (syntax checking mode)")
        || !op.addBoolOption('w', "warnings", "Emit warnings")
        || !op.addBoolOption('W', "nowarnings", "Don't emit warnings")
        || !op.addBoolOption('s', "strict", "Check strictness")
        || !op.addBoolOption('D', "dump-bytecode", "Dump bytecode with exec count for all scripts")
        || !op.addBoolOption('b', "print-timing", "Print sub-ms runtime for each file that's run")
        || !op.addStringOption('\0', "js-cache", "[path]",
                               "Enable the JS cache by specifying the path of the directory to use "
                               "to hold cache files")
        || !op.addBoolOption('\0', "no-js-cache-per-process",
                               "Deactivates cache per process. Otherwise, generate a separate cache"
                               "sub-directory for this process inside the cache directory"
                               "specified by --js-cache. This cache directory will be removed"
                               "when the js shell exits. This is useful for running tests in"
                               "parallel.")
        || !op.addBoolOption('\0', "code-coverage", "Enable code coverage instrumentation.")
#ifdef DEBUG
        || !op.addBoolOption('O', "print-alloc", "Print the number of allocations at exit")
#endif
        || !op.addOptionalStringArg("script", "A script to execute (after all options)")
        || !op.addOptionalMultiStringArg("scriptArgs",
                                         "String arguments to bind as |scriptArgs| in the "
                                         "shell's global")
        || !op.addIntOption('\0', "thread-count", "COUNT", "Use COUNT auxiliary threads "
                            "(default: # of cores - 1)", -1)
        || !op.addBoolOption('\0', "ion", "Enable IonMonkey (default)")
        || !op.addBoolOption('\0', "no-ion", "Disable IonMonkey")
        || !op.addBoolOption('\0', "no-asmjs", "Disable asm.js compilation")
        || !op.addBoolOption('\0', "no-wasm", "Disable WebAssembly compilation")
        || !op.addBoolOption('\0', "no-native-regexp", "Disable native regexp compilation")
        || !op.addBoolOption('\0', "no-unboxed-objects", "Disable creating unboxed plain objects")
        || !op.addBoolOption('\0', "unboxed-arrays", "Allow creating unboxed arrays")
        || !op.addBoolOption('\0', "wasm-always-baseline", "Enable wasm baseline compiler when possible")
        || !op.addBoolOption('\0', "wasm-check-bce", "Always generate wasm bounds check, even redundant ones.")
        || !op.addBoolOption('\0', "wasm-test-mode", "Enable wasm testing mode, creating synthetic "
                                   "objects for non-canonical NaNs and i64 returned from wasm.")
#ifdef ENABLE_SHARED_ARRAY_BUFFER
        || !op.addStringOption('\0', "shared-memory", "on/off",
                               "SharedArrayBuffer and Atomics "
#  if SHARED_MEMORY_DEFAULT
                               "(default: on, off to disable)"
#  else
                               "(default: off, on to enable)"
#  endif
            )
#endif
        || !op.addStringOption('\0', "cache-ir-stubs", "on/off",
                               "Use CacheIR stubs (default: on, off to disable)")
        || !op.addStringOption('\0', "ion-shared-stubs", "on/off",
                               "Use shared stubs (default: on, off to disable)")
        || !op.addStringOption('\0', "ion-scalar-replacement", "on/off",
                               "Scalar Replacement (default: on, off to disable)")
        || !op.addStringOption('\0', "ion-gvn", "[mode]",
                               "Specify Ion global value numbering:\n"
                               "  off: disable GVN\n"
                               "  on:  enable GVN (default)\n")
        || !op.addStringOption('\0', "ion-licm", "on/off",
                               "Loop invariant code motion (default: on, off to disable)")
        || !op.addStringOption('\0', "ion-aa", "flow-sensitive/flow-insensitive",
                               "Specify wheter or not to use flow sensitive Alias Analysis"
                               "(default: flow-insensitive)")
        || !op.addStringOption('\0', "ion-edgecase-analysis", "on/off",
                               "Find edge cases where Ion can avoid bailouts (default: on, off to disable)")
        || !op.addStringOption('\0', "ion-pgo", "on/off",
                               "Profile guided optimization (default: on, off to disable)")
        || !op.addStringOption('\0', "ion-range-analysis", "on/off",
                               "Range analysis (default: on, off to disable)")
#if defined(__APPLE__)
        || !op.addStringOption('\0', "ion-sincos", "on/off",
                               "Replace sin(x)/cos(x) to sincos(x) (default: on, off to disable)")
#else
        || !op.addStringOption('\0', "ion-sincos", "on/off",
                               "Replace sin(x)/cos(x) to sincos(x) (default: off, on to enable)")
#endif
        || !op.addStringOption('\0', "ion-sink", "on/off",
                               "Sink code motion (default: off, on to enable)")
        || !op.addStringOption('\0', "ion-loop-unrolling", "on/off",
                               "Loop unrolling (default: off, on to enable)")
        || !op.addStringOption('\0', "ion-instruction-reordering", "on/off",
                               "Instruction reordering (default: off, on to enable)")
        || !op.addBoolOption('\0', "ion-check-range-analysis",
                               "Range analysis checking")
        || !op.addBoolOption('\0', "ion-extra-checks",
                               "Perform extra dynamic validation checks")
        || !op.addStringOption('\0', "ion-inlining", "on/off",
                               "Inline methods where possible (default: on, off to disable)")
        || !op.addStringOption('\0', "ion-osr", "on/off",
                               "On-Stack Replacement (default: on, off to disable)")
        || !op.addStringOption('\0', "ion-limit-script-size", "on/off",
                               "Don't compile very large scripts (default: on, off to disable)")
        || !op.addIntOption('\0', "ion-warmup-threshold", "COUNT",
                            "Wait for COUNT calls or iterations before compiling "
                            "(default: 1000)", -1)
        || !op.addStringOption('\0', "ion-regalloc", "[mode]",
                               "Specify Ion register allocation:\n"
                               "  backtracking: Priority based backtracking register allocation (default)\n"
                               "  testbed: Backtracking allocator with experimental features\n"
                               "  stupid: Simple block local register allocation")
        || !op.addBoolOption('\0', "ion-eager", "Always ion-compile methods (implies --baseline-eager)")
        || !op.addStringOption('\0', "ion-offthread-compile", "on/off",
                               "Compile scripts off thread (default: on)")
        || !op.addStringOption('\0', "ion-parallel-compile", "on/off",
                               "--ion-parallel compile is deprecated. Use --ion-offthread-compile.")
        || !op.addBoolOption('\0', "baseline", "Enable baseline compiler (default)")
        || !op.addBoolOption('\0', "no-baseline", "Disable baseline compiler")
        || !op.addBoolOption('\0', "baseline-eager", "Always baseline-compile methods")
        || !op.addIntOption('\0', "baseline-warmup-threshold", "COUNT",
                            "Wait for COUNT calls or iterations before baseline-compiling "
                            "(default: 10)", -1)
        || !op.addBoolOption('\0', "non-writable-jitcode", "(NOP for fuzzers) Allocate JIT code as non-writable memory.")
        || !op.addBoolOption('\0', "no-sse3", "Pretend CPU does not support SSE3 instructions and above "
                             "to test JIT codegen (no-op on platforms other than x86 and x64).")
        || !op.addBoolOption('\0', "no-sse4", "Pretend CPU does not support SSE4 instructions"
                             "to test JIT codegen (no-op on platforms other than x86 and x64).")
        || !op.addBoolOption('\0', "enable-avx", "AVX is disabled by default. Enable AVX. "
                             "(no-op on platforms other than x86 and x64).")
        || !op.addBoolOption('\0', "no-avx", "No-op. AVX is currently disabled by default.")
        || !op.addBoolOption('\0', "fuzzing-safe", "Don't expose functions that aren't safe for "
                             "fuzzers to call")
        || !op.addBoolOption('\0', "disable-oom-functions", "Disable functions that cause "
                            "artificial OOMs")
        || !op.addBoolOption('\0', "no-threads", "Disable helper threads")
#ifdef DEBUG
        || !op.addBoolOption('\0', "dump-entrained-variables", "Print variables which are "
                             "unnecessarily entrained by inner functions")
#endif
        || !op.addBoolOption('\0', "no-ggc", "Disable Generational GC")
        || !op.addBoolOption('\0', "no-cgc", "Disable Compacting GC")
        || !op.addBoolOption('\0', "no-incremental-gc", "Disable Incremental GC")
        || !op.addIntOption('\0', "available-memory", "SIZE",
                            "Select GC settings based on available memory (MB)", 0)
        || !op.addStringOption('\0', "arm-hwcap", "[features]",
                               "Specify ARM code generation features, or 'help' to list all features.")
        || !op.addIntOption('\0', "arm-asm-nop-fill", "SIZE",
                            "Insert the given number of NOP instructions at all possible pool locations.", 0)
        || !op.addIntOption('\0', "asm-pool-max-offset", "OFFSET",
                            "The maximum pc relative OFFSET permitted in pool reference instructions.", 1024)
#if defined(JS_SIMULATOR_ARM)
        || !op.addBoolOption('\0', "arm-sim-icache-checks", "Enable icache flush checks in the ARM "
                             "simulator.")
        || !op.addIntOption('\0', "arm-sim-stop-at", "NUMBER", "Stop the ARM simulator after the given "
                            "NUMBER of instructions.", -1)
#elif defined(JS_SIMULATOR_MIPS32) || defined(JS_SIMULATOR_MIPS64)
        || !op.addBoolOption('\0', "mips-sim-icache-checks", "Enable icache flush checks in the MIPS "
                             "simulator.")
        || !op.addIntOption('\0', "mips-sim-stop-at", "NUMBER", "Stop the MIPS simulator after the given "
                            "NUMBER of instructions.", -1)
#endif
        || !op.addIntOption('\0', "nursery-size", "SIZE-MB", "Set the maximum nursery size in MB", 16)
#ifdef JS_GC_ZEAL
        || !op.addStringOption('z', "gc-zeal", "LEVEL(;LEVEL)*[,N]", gc::ZealModeHelpText)
#endif
        || !op.addStringOption('\0', "module-load-path", "DIR", "Set directory to load modules from")
    )
    {
        return EXIT_FAILURE;
    }

    op.setArgTerminatesOptions("script", true);
    op.setArgCapturesRest("scriptArgs");

    switch (op.parseArgs(argc, argv)) {
      case OptionParser::EarlyExit:
        return EXIT_SUCCESS;
      case OptionParser::ParseError:
        op.printHelp(argv[0]);
        return EXIT_FAILURE;
      case OptionParser::Fail:
        return EXIT_FAILURE;
      case OptionParser::Okay:
        break;
    }

    if (op.getHelpOption())
        return EXIT_SUCCESS;

#ifdef DEBUG
    /*
     * Process OOM options as early as possible so that we can observe as many
     * allocations as possible.
     */
    OOM_printAllocationCount = op.getBoolOption('O');
#endif

#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    if (op.getBoolOption("no-sse3")) {
        js::jit::CPUInfo::SetSSE3Disabled();
        PropagateFlagToNestedShells("--no-sse3");
    }
    if (op.getBoolOption("no-sse4")) {
        js::jit::CPUInfo::SetSSE4Disabled();
        PropagateFlagToNestedShells("--no-sse4");
    }
    if (op.getBoolOption("enable-avx")) {
        js::jit::CPUInfo::SetAVXEnabled();
        PropagateFlagToNestedShells("--enable-avx");
    }
#endif

    if (op.getBoolOption("no-threads"))
        js::DisableExtraThreads();

    // Start the engine.
    if (!JS_Init())
        return 1;

    if (!InitSharedArrayBufferMailbox())
        return 1;

    // The fake thread count must be set before initializing the Runtime,
    // which spins up the thread pool.
    int32_t threadCount = op.getIntOption("thread-count");
    if (threadCount >= 0)
        SetFakeCPUCount(threadCount);

    size_t nurseryBytes = JS::DefaultNurseryBytes;
    nurseryBytes = op.getIntOption("nursery-size") * 1024L * 1024L;

    /* Use the same parameters as the browser in xpcjsruntime.cpp. */
    JSContext* cx = JS_NewContext(JS::DefaultHeapMaxBytes, nurseryBytes);
    if (!cx)
        return 1;

    UniquePtr<ShellContext> sc = MakeUnique<ShellContext>(cx);
    if (!sc)
        return 1;

    JS_SetContextPrivate(cx, sc.get());
    JS_SetGrayGCRootsTracer(cx, TraceGrayRoots, nullptr);
    // Waiting is allowed on the shell's main thread, for now.
    JS_SetFutexCanWait(cx);
    JS::SetWarningReporter(cx, WarningReporter);
    if (!SetContextOptions(cx, op))
        return 1;

    JS_SetGCParameter(cx, JSGC_MAX_BYTES, 0xffffffff);

    size_t availMem = op.getIntOption("available-memory");
    if (availMem > 0)
        JS_SetGCParametersBasedOnAvailableMemory(cx, availMem);

    JS_SetTrustedPrincipals(cx, &ShellPrincipals::fullyTrusted);
    JS_SetSecurityCallbacks(cx, &ShellPrincipals::securityCallbacks);
    JS_InitDestroyPrincipalsCallback(cx, ShellPrincipals::destroy);

    JS_AddInterruptCallback(cx, ShellInterruptCallback);
    JS::SetBuildIdOp(cx, ShellBuildId);
    JS::SetAsmJSCacheOps(cx, &asmJSCacheOps);

    JS_SetNativeStackQuota(cx, gMaxStackSize);

    JS::dbg::SetDebuggerMallocSizeOf(cx, moz_malloc_size_of);

    if (!JS::InitSelfHostedCode(cx))
        return 1;

    sc->jobQueue.init(cx, JobQueue(SystemAllocPolicy()));
    JS::SetEnqueuePromiseJobCallback(cx, ShellEnqueuePromiseJobCallback);
    JS::SetGetIncumbentGlobalCallback(cx, ShellGetIncumbentGlobalCallback);
    JS::SetAsyncTaskCallbacks(cx, ShellStartAsyncTaskCallback, ShellFinishAsyncTaskCallback);

    EnvironmentPreparer environmentPreparer(cx);

    JS_SetGCParameter(cx, JSGC_MODE, JSGC_MODE_INCREMENTAL);

    JS::SetProcessLargeAllocationFailureCallback(my_LargeAllocFailCallback);

    // Set some parameters to allow incremental GC in low memory conditions,
    // as is done for the browser, except in more-deterministic builds or when
    // disabled by command line options.
#ifndef JS_MORE_DETERMINISTIC
    if (!op.getBoolOption("no-incremental-gc")) {
        JS_SetGCParameter(cx, JSGC_DYNAMIC_HEAP_GROWTH, 1);
        JS_SetGCParameter(cx, JSGC_DYNAMIC_MARK_SLICE, 1);
        JS_SetGCParameter(cx, JSGC_SLICE_TIME_BUDGET, 10);
    }
#endif

    js::SetPreserveWrapperCallback(cx, DummyPreserveWrapperCallback);

    cooperationState = js_new<CooperationState>();
    JS::SetSingleThreadedExecutionCallbacks(cx,
                                            CooperativeBeginSingleThreadedExecution,
                                            CooperativeEndSingleThreadedExecution);

    result = Shell(cx, &op, envp);

#ifdef DEBUG
    if (OOM_printAllocationCount)
        printf("OOM max count: %" PRIu64 "\n", js::oom::counter);
#endif

    JS::SetGetIncumbentGlobalCallback(cx, nullptr);
    JS::SetEnqueuePromiseJobCallback(cx, nullptr);
    JS_SetGrayGCRootsTracer(cx, nullptr, nullptr);

    // Must clear out some of sc's pointer containers before JS_DestroyContext.
    sc->markObservers.reset();
    sc->jobQueue.reset();

    KillWatchdog(cx);

    KillWorkerThreads(cx);

    DestructSharedArrayBufferMailbox();

    JS_DestroyContext(cx);
    JS_ShutDown();
    return result;
}
