/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
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
#include "mozilla/Sprintf.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtrExtensions.h"  // UniqueFreePtr
#include "mozilla/Unused.h"
#include "mozilla/Utf8.h"
#include "mozilla/Variant.h"

#include <algorithm>
#include <chrono>
#ifdef XP_WIN
#  include <direct.h>
#  include <process.h>
#endif
#include <errno.h>
#include <fcntl.h>
#if defined(XP_WIN)
#  include <io.h> /* for isatty() */
#endif
#include <locale.h>
#if defined(MALLOC_H)
#  include MALLOC_H /* for malloc_usable_size, malloc_size, _msize */
#endif
#include <ctime>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <utility>
#ifdef XP_UNIX
#  include <sys/mman.h>
#  include <sys/stat.h>
#  include <sys/wait.h>
#  include <unistd.h>
#endif
#ifdef XP_LINUX
#  include <sys/prctl.h>
#endif

#include "jsapi.h"
#include "jsfriendapi.h"
#include "jstypes.h"
#ifndef JS_WITHOUT_NSPR
#  include "prerror.h"
#  include "prlink.h"
#endif
#include "shellmoduleloader.out.h"

#include "builtin/Array.h"
#include "builtin/MapObject.h"
#include "builtin/ModuleObject.h"
#include "builtin/RegExp.h"
#include "builtin/TestingFunctions.h"
#include "debugger/DebugAPI.h"
#if defined(JS_BUILD_BINAST)
#  include "frontend/BinASTParser.h"
#endif  // defined(JS_BUILD_BINAST)
#include "frontend/CompilationInfo.h"
#ifdef JS_ENABLE_SMOOSH
#  include "frontend/Frontend2.h"
#endif
#include "frontend/ModuleSharedContext.h"
#include "frontend/Parser.h"
#include "frontend/SourceNotes.h"  // SrcNote, SrcNoteType, SrcNoteIterator
#include "gc/PublicIterators.h"
#include "jit/arm/Simulator-arm.h"
#include "jit/InlinableNatives.h"
#include "jit/Ion.h"
#include "jit/JitcodeMap.h"
#include "jit/JitRealm.h"
#include "jit/shared/CodeGenerator-shared.h"
#include "js/Array.h"        // JS::NewArrayObject
#include "js/ArrayBuffer.h"  // JS::{CreateMappedArrayBufferContents,NewMappedArrayBufferWithContents,IsArrayBufferObject,GetArrayBufferLengthAndData}
#include "js/BuildId.h"      // JS::BuildIdCharVector, JS::SetProcessBuildIdOp
#include "js/CharacterEncoding.h"
#include "js/CompilationAndEvaluation.h"
#include "js/CompileOptions.h"
#include "js/ContextOptions.h"  // JS::ContextOptions{,Ref}
#include "js/Debug.h"
#include "js/Equality.h"                 // JS::SameValue
#include "js/experimental/SourceHook.h"  // js::{Set,Forget,}SourceHook
#include "js/GCVector.h"
#include "js/Initialization.h"
#include "js/JSON.h"
#include "js/MemoryFunctions.h"
#include "js/Modules.h"  // JS::GetModulePrivate, JS::SetModule{DynamicImport,Metadata,Resolve}Hook, JS::SetModulePrivate
#include "js/Printf.h"
#include "js/PropertySpec.h"
#include "js/Realm.h"
#include "js/RegExp.h"  // JS::ObjectIsRegExp
#include "js/SourceText.h"
#include "js/StableStringChars.h"
#include "js/StructuredClone.h"
#include "js/SweepingAPI.h"
#include "js/Warnings.h"  // JS::SetWarningReporter
#include "js/Wrapper.h"
#include "shell/jsoptparse.h"
#include "shell/jsshell.h"
#include "shell/OSObject.h"
#include "threading/ConditionVariable.h"
#include "threading/ExclusiveData.h"
#include "threading/LockGuard.h"
#include "threading/Thread.h"
#include "util/CompleteFile.h"  // js::FileContents, js::ReadCompleteFile
#include "util/StringBuffer.h"
#include "util/Text.h"
#include "util/Windows.h"
#include "vm/ArgumentsObject.h"
#include "vm/Compression.h"
#include "vm/HelperThreads.h"
#include "vm/JSAtom.h"
#include "vm/JSContext.h"
#include "vm/JSFunction.h"
#include "vm/JSObject.h"
#include "vm/JSScript.h"
#include "vm/ModuleBuilder.h"  // js::ModuleBuilder
#include "vm/Monitor.h"
#include "vm/MutexIDs.h"
#include "vm/Printer.h"
#include "vm/PromiseObject.h"  // js::PromiseObject
#include "vm/Shape.h"
#include "vm/SharedArrayObject.h"
#include "vm/Time.h"
#include "vm/ToSource.h"  // js::ValueToSource
#include "vm/TypedArrayObject.h"
#include "vm/WrapperObject.h"
#include "wasm/WasmJS.h"

#include "vm/Compartment-inl.h"
#include "vm/ErrorObject-inl.h"
#include "vm/Interpreter-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/Realm-inl.h"
#include "vm/Stack-inl.h"

using namespace js;
using namespace js::cli;
using namespace js::shell;

using JS::AutoStableStringChars;
using JS::CompileOptions;

using js::shell::RCFile;

using mozilla::ArrayEqual;
using mozilla::ArrayLength;
using mozilla::AsVariant;
using mozilla::Atomic;
using mozilla::MakeScopeExit;
using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::NumberEqualsInt32;
using mozilla::TimeDuration;
using mozilla::TimeStamp;
using mozilla::Utf8Unit;
using mozilla::Variant;

enum JSShellExitCode {
  EXITCODE_RUNTIME_ERROR = 3,
  EXITCODE_FILE_NOT_FOUND = 4,
  EXITCODE_OUT_OF_MEMORY = 5,
  EXITCODE_TIMEOUT = 6
};

// Define use of application-specific slots on the shell's global object.
enum GlobalAppSlot {
  GlobalAppSlotModuleLoadHook,           // Shell-specific; load a module graph
  GlobalAppSlotModuleResolveHook,        // HostResolveImportedModule
  GlobalAppSlotModuleMetadataHook,       // HostPopulateImportMeta
  GlobalAppSlotModuleDynamicImportHook,  // HostImportModuleDynamically
  GlobalAppSlotCount
};
static_assert(GlobalAppSlotCount <= JSCLASS_GLOBAL_APPLICATION_SLOTS,
              "Too many applications slots defined for shell global");

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
static const double MAX_TIMEOUT_SECONDS = 1800.0;

// Not necessarily in sync with the browser
#ifdef ENABLE_SHARED_MEMORY
#  define SHARED_MEMORY_DEFAULT 1
#else
#  define SHARED_MEMORY_DEFAULT 0
#endif

// Fuzzing support for JS runtime fuzzing
#ifdef FUZZING_INTERFACES
#  include "shell/jsrtfuzzing/jsrtfuzzing.h"
static bool fuzzDoDebug = !!getenv("MOZ_FUZZ_DEBUG");
static bool fuzzHaveModule = !!getenv("FUZZER");
#endif  // FUZZING_INTERFACES

// Code to support GCOV code coverage measurements on standalone shell
#ifdef MOZ_CODE_COVERAGE
#  if defined(__GNUC__) && !defined(__clang__)
extern "C" void __gcov_dump();
extern "C" void __gcov_reset();

void counters_dump(int) { __gcov_dump(); }

void counters_reset(int) { __gcov_reset(); }
#  else
void counters_dump(int) { /* Do nothing */
}

void counters_reset(int) { /* Do nothing */
}
#  endif

static void InstallCoverageSignalHandlers() {
#  ifndef XP_WIN
  fprintf(stderr, "[CodeCoverage] Setting handlers for process %d.\n",
          getpid());

  struct sigaction dump_sa;
  dump_sa.sa_handler = counters_dump;
  dump_sa.sa_flags = SA_RESTART;
  sigemptyset(&dump_sa.sa_mask);
  mozilla::DebugOnly<int> r1 = sigaction(SIGUSR1, &dump_sa, nullptr);
  MOZ_ASSERT(r1 == 0, "Failed to install GCOV SIGUSR1 handler");

  struct sigaction reset_sa;
  reset_sa.sa_handler = counters_reset;
  reset_sa.sa_flags = SA_RESTART;
  sigemptyset(&reset_sa.sa_mask);
  mozilla::DebugOnly<int> r2 = sigaction(SIGUSR2, &reset_sa, nullptr);
  MOZ_ASSERT(r2 == 0, "Failed to install GCOV SIGUSR2 handler");
#  endif
}
#endif

// An off-thread parse or decode job.
class js::shell::OffThreadJob {
  enum State {
    RUNNING,   // Working; no token.
    DONE,      // Finished; have token.
    CANCELLED  // Cancelled due to error.
  };

 public:
  using Source = mozilla::Variant<JS::UniqueTwoByteChars, JS::TranscodeBuffer>;

  OffThreadJob(ShellContext* sc, ScriptKind kind, Source&& source);
  ~OffThreadJob();

  void cancel();
  void markDone(JS::OffThreadToken* newToken);
  JS::OffThreadToken* waitUntilDone(JSContext* cx);

  char16_t* sourceChars() { return source.as<UniqueTwoByteChars>().get(); }
  JS::TranscodeBuffer& xdrBuffer() { return source.as<JS::TranscodeBuffer>(); }

 public:
  const int32_t id;
  const ScriptKind kind;

 private:
  js::Monitor& monitor;
  State state;
  JS::OffThreadToken* token;
  Source source;
};

static OffThreadJob* NewOffThreadJob(JSContext* cx, ScriptKind kind,
                                     OffThreadJob::Source&& source) {
  ShellContext* sc = GetShellContext(cx);
  UniquePtr<OffThreadJob> job(
      cx->new_<OffThreadJob>(sc, kind, std::move(source)));
  if (!job) {
    return nullptr;
  }

  if (!sc->offThreadJobs.append(job.get())) {
    job->cancel();
    JS_ReportErrorASCII(cx, "OOM adding off-thread job");
    return nullptr;
  }

  return job.release();
}

static OffThreadJob* GetSingleOffThreadJob(JSContext* cx, ScriptKind kind) {
  ShellContext* sc = GetShellContext(cx);
  const auto& jobs = sc->offThreadJobs;
  if (jobs.empty()) {
    JS_ReportErrorASCII(cx, "No off-thread jobs are pending");
    return nullptr;
  }

  if (jobs.length() > 1) {
    JS_ReportErrorASCII(
        cx, "Multiple off-thread jobs are pending: must specify job ID");
    return nullptr;
  }

  OffThreadJob* job = jobs[0];
  if (job->kind != kind) {
    JS_ReportErrorASCII(cx, "Off-thread job is the wrong kind");
    return nullptr;
  }

  return job;
}

static OffThreadJob* LookupOffThreadJobByID(JSContext* cx, ScriptKind kind,
                                            int32_t id) {
  if (id <= 0) {
    JS_ReportErrorASCII(cx, "Bad off-thread job ID");
    return nullptr;
  }

  ShellContext* sc = GetShellContext(cx);
  const auto& jobs = sc->offThreadJobs;
  if (jobs.empty()) {
    JS_ReportErrorASCII(cx, "No off-thread jobs are pending");
    return nullptr;
  }

  OffThreadJob* job = nullptr;
  for (auto someJob : jobs) {
    if (someJob->id == id) {
      job = someJob;
      break;
    }
  }

  if (!job) {
    JS_ReportErrorASCII(cx, "Off-thread job not found");
    return nullptr;
  }

  if (job->kind != kind) {
    JS_ReportErrorASCII(cx, "Off-thread job is the wrong kind");
    return nullptr;
  }

  return job;
}

static OffThreadJob* LookupOffThreadJobForArgs(JSContext* cx, ScriptKind kind,
                                               const CallArgs& args,
                                               size_t arg) {
  // If the optional ID argument isn't present, get the single pending job.
  if (args.length() <= arg) {
    return GetSingleOffThreadJob(cx, kind);
  }

  // Lookup the job using the specified ID.
  int32_t id = 0;
  RootedValue value(cx, args[arg]);
  if (!ToInt32(cx, value, &id)) {
    return nullptr;
  }

  return LookupOffThreadJobByID(cx, kind, id);
}

static void DeleteOffThreadJob(JSContext* cx, OffThreadJob* job) {
  ShellContext* sc = GetShellContext(cx);
  for (size_t i = 0; i < sc->offThreadJobs.length(); i++) {
    if (sc->offThreadJobs[i] == job) {
      sc->offThreadJobs.erase(&sc->offThreadJobs[i]);
      js_delete(job);
      return;
    }
  }

  MOZ_CRASH("Off-thread job not found");
}

static void CancelOffThreadJobsForContext(JSContext* cx) {
  // Parse jobs may be blocked waiting on GC.
  gc::FinishGC(cx);

  // Wait for jobs belonging to this context.
  ShellContext* sc = GetShellContext(cx);
  while (!sc->offThreadJobs.empty()) {
    OffThreadJob* job = sc->offThreadJobs.popCopy();
    job->waitUntilDone(cx);
    js_delete(job);
  }
}

static void CancelOffThreadJobsForRuntime(JSContext* cx) {
  // Parse jobs may be blocked waiting on GC.
  gc::FinishGC(cx);

  // Cancel jobs belonging to this runtime.
  CancelOffThreadParses(cx->runtime());
  ShellContext* sc = GetShellContext(cx);
  while (!sc->offThreadJobs.empty()) {
    js_delete(sc->offThreadJobs.popCopy());
  }
}

mozilla::Atomic<int32_t> gOffThreadJobSerial(1);

OffThreadJob::OffThreadJob(ShellContext* sc, ScriptKind kind, Source&& source)
    : id(gOffThreadJobSerial++),
      kind(kind),
      monitor(sc->offThreadMonitor),
      state(RUNNING),
      token(nullptr),
      source(std::move(source)) {
  MOZ_RELEASE_ASSERT(id > 0, "Off-thread job IDs exhausted");
}

OffThreadJob::~OffThreadJob() { MOZ_ASSERT(state != RUNNING); }

void OffThreadJob::cancel() {
  MOZ_ASSERT(state == RUNNING);
  MOZ_ASSERT(!token);

  state = CANCELLED;
}

void OffThreadJob::markDone(JS::OffThreadToken* newToken) {
  AutoLockMonitor alm(monitor);
  MOZ_ASSERT(state == RUNNING);
  MOZ_ASSERT(!token);
  MOZ_ASSERT(newToken);

  token = newToken;
  state = DONE;
  alm.notifyAll();
}

JS::OffThreadToken* OffThreadJob::waitUntilDone(JSContext* cx) {
  AutoLockMonitor alm(monitor);
  MOZ_ASSERT(state != CANCELLED);

  while (state != DONE) {
    alm.wait();
  }

  MOZ_ASSERT(token);
  return token;
}

struct ShellCompartmentPrivate {
  GCPtrObject grayRoot;
};

struct MOZ_STACK_CLASS EnvironmentPreparer
    : public js::ScriptEnvironmentPreparer {
  explicit EnvironmentPreparer(JSContext* cx) {
    js::SetScriptEnvironmentPreparer(cx, this);
  }
  void invoke(JS::HandleObject global, Closure& closure) override;
};

bool shell::enableDeferredMode = true;
bool shell::enableCodeCoverage = false;
bool shell::enableDisassemblyDumps = false;
bool shell::offthreadCompilation = false;
bool shell::enableAsmJS = false;
bool shell::enableWasm = false;
bool shell::enableSharedMemory = SHARED_MEMORY_DEFAULT;
bool shell::enableWasmBaseline = false;
bool shell::enableWasmIon = false;
bool shell::enableWasmCranelift = false;
#ifdef ENABLE_WASM_GC
bool shell::enableWasmGc = false;
#endif
#ifdef ENABLE_WASM_MULTI_VALUE
bool shell::enableWasmMultiValue = true;
#endif
bool shell::enableWasmVerbose = false;
bool shell::enableTestWasmAwaitTier2 = false;
#ifdef ENABLE_WASM_BIGINT
bool shell::enableWasmBigInt = true;
#endif
bool shell::enableAsyncStacks = false;
bool shell::enableStreams = false;
bool shell::enableReadableByteStreams = false;
bool shell::enableBYOBStreamReaders = false;
bool shell::enableWritableStreams = false;
bool shell::enableReadableStreamPipeTo = false;
bool shell::enableWeakRefs = false;
bool shell::enableToSource = false;
bool shell::enablePropertyErrorMessageFix = false;
#ifdef JS_GC_ZEAL
uint32_t shell::gZealBits = 0;
uint32_t shell::gZealFrequency = 0;
#endif
bool shell::printTiming = false;
RCFile* shell::gErrFile = nullptr;
RCFile* shell::gOutFile = nullptr;
bool shell::reportWarnings = true;
bool shell::compileOnly = false;
bool shell::fuzzingSafe = false;
bool shell::disableOOMFunctions = false;
bool shell::defaultToSameCompartment = true;

#ifdef DEBUG
bool shell::dumpEntrainedVariables = false;
bool shell::OOM_printAllocationCount = false;
#endif

static bool SetTimeoutValue(JSContext* cx, double t);

static void KillWatchdog(JSContext* cx);

static bool ScheduleWatchdog(JSContext* cx, double t);

static void CancelExecution(JSContext* cx);

enum class ShellGlobalKind {
  GlobalObject,
  WindowProxy,
};

static JSObject* NewGlobalObject(JSContext* cx, JS::RealmOptions& options,
                                 JSPrincipals* principals,
                                 ShellGlobalKind kind);

/*
 * A toy WindowProxy class for the shell. This is intended for testing code
 * where global |this| is a WindowProxy. All requests are forwarded to the
 * underlying global and no navigation is supported.
 */
const JSClass ShellWindowProxyClass =
    PROXY_CLASS_DEF("ShellWindowProxy", JSCLASS_HAS_RESERVED_SLOTS(1));

JSObject* NewShellWindowProxy(JSContext* cx, JS::HandleObject global) {
  MOZ_ASSERT(global->is<GlobalObject>());

  js::WrapperOptions options;
  options.setClass(&ShellWindowProxyClass);
  options.setSingleton(true);

  JSAutoRealm ar(cx, global);
  JSObject* obj =
      js::Wrapper::New(cx, global, &js::Wrapper::singleton, options);
  MOZ_ASSERT_IF(obj, js::IsWindowProxy(obj));
  return obj;
}

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
    if (!p) {
      return 0xffff;
    }
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

  bool isSystemOrAddonPrincipal() override { return true; }

  static void destroy(JSPrincipals* principals) {
    MOZ_ASSERT(principals != &fullyTrusted);
    MOZ_ASSERT(principals->refcount == 0);
    js_delete(static_cast<const ShellPrincipals*>(principals));
  }

  static bool subsumes(JSPrincipals* first, JSPrincipals* second) {
    uint32_t firstBits = getBits(first);
    uint32_t secondBits = getBits(second);
    return (firstBits | secondBits) == firstBits;
  }

  static JSSecurityCallbacks securityCallbacks;

  // Fully-trusted principals singleton.
  static ShellPrincipals fullyTrusted;
};

JSSecurityCallbacks ShellPrincipals::securityCallbacks = {
    nullptr,  // contentSecurityPolicyAllows
    subsumes};

// The fully-trusted principal subsumes all other principals.
ShellPrincipals ShellPrincipals::fullyTrusted(-1, 1);

#ifdef EDITLINE
extern "C" {
extern MOZ_EXPORT char* readline(const char* prompt);
extern MOZ_EXPORT void add_history(char* line);
}  // extern "C"
#endif

ShellContext::ShellContext(JSContext* cx)
    : isWorker(false),
      lastWarningEnabled(false),
      trackUnhandledRejections(true),
      timeoutInterval(-1.0),
      startTime(PRMJ_Now()),
      serviceInterrupt(false),
      haveInterruptFunc(false),
      interruptFunc(cx, NullValue()),
      lastWarning(cx, NullValue()),
      promiseRejectionTrackerCallback(cx, NullValue()),
      unhandledRejectedPromises(cx),
      watchdogLock(mutexid::ShellContextWatchdog),
      exitCode(0),
      quitting(false),
      readLineBufPos(0),
      errFilePtr(nullptr),
      outFilePtr(nullptr),
      offThreadMonitor(mutexid::ShellOffThreadState),
      finalizationRegistriesToCleanUp(cx) {}

ShellContext::~ShellContext() { MOZ_ASSERT(offThreadJobs.empty()); }

ShellContext* js::shell::GetShellContext(JSContext* cx) {
  ShellContext* sc = static_cast<ShellContext*>(JS_GetContextPrivate(cx));
  MOZ_ASSERT(sc);
  return sc;
}

static void TraceGrayRoots(JSTracer* trc, void* data) {
  JSRuntime* rt = trc->runtime();
  for (ZonesIter zone(rt, SkipAtoms); !zone.done(); zone.next()) {
    for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next()) {
      auto priv = static_cast<ShellCompartmentPrivate*>(
          JS_GetCompartmentPrivate(comp.get()));
      if (priv) {
        TraceNullableEdge(trc, &priv->grayRoot, "test gray root");
      }
    }
  }
}

static mozilla::UniqueFreePtr<char[]> GetLine(FILE* file, const char* prompt) {
#ifdef EDITLINE
  /*
   * Use readline only if file is stdin, because there's no way to specify
   * another handle.  Are other filehandles interactive?
   */
  if (file == stdin) {
    mozilla::UniqueFreePtr<char[]> linep(readline(prompt));
    /*
     * We set it to zero to avoid complaining about inappropriate ioctl
     * for device in the case of EOF. Looks like errno == 251 if line is
     * finished with EOF and errno == 25 (EINVAL on Mac) if there is
     * nothing left to read.
     */
    if (errno == 251 || errno == 25 || errno == EINVAL) {
      errno = 0;
    }
    if (!linep) {
      return nullptr;
    }
    if (linep[0] != '\0') {
      add_history(linep.get());
    }
    return linep;
  }
#endif

  size_t len = 0;
  if (*prompt != '\0' && gOutFile->isOpen()) {
    fprintf(gOutFile->fp, "%s", prompt);
    fflush(gOutFile->fp);
  }

  size_t size = 80;
  mozilla::UniqueFreePtr<char[]> buffer(static_cast<char*>(malloc(size)));
  if (!buffer) {
    return nullptr;
  }

  char* current = buffer.get();
  do {
    while (true) {
      if (fgets(current, size - len, file)) {
        break;
      }
      if (errno != EINTR) {
        return nullptr;
      }
    }

    len += strlen(current);
    char* t = buffer.get() + len - 1;
    if (*t == '\n') {
      /* Line was read. We remove '\n' and exit. */
      *t = '\0';
      break;
    }

    if (len + 1 == size) {
      size = size * 2;
      char* raw = buffer.release();
      char* tmp = static_cast<char*>(realloc(raw, size));
      if (!tmp) {
        free(raw);
        return nullptr;
      }
      buffer.reset(tmp);
    }
    current = buffer.get() + len;
  } while (true);
  return buffer;
}

static bool ShellInterruptCallback(JSContext* cx) {
  ShellContext* sc = GetShellContext(cx);
  if (!sc->serviceInterrupt) {
    return true;
  }

  // Reset serviceInterrupt. CancelExecution or InterruptIf will set it to
  // true to distinguish watchdog or user triggered interrupts.
  // Do this first to prevent other interrupts that may occur while the
  // user-supplied callback is executing from re-entering the handler.
  sc->serviceInterrupt = false;

  bool result;
  if (sc->haveInterruptFunc) {
    bool wasAlreadyThrowing = cx->isExceptionPending();
    JS::AutoSaveExceptionState savedExc(cx);
    JSAutoRealm ar(cx, &sc->interruptFunc.toObject());
    RootedValue rval(cx);

    // Report any exceptions thrown by the JS interrupt callback, but do
    // *not* keep it on the cx. The interrupt handler is invoked at points
    // that are not expected to throw catchable exceptions, like at
    // JSOp::RetRval.
    //
    // If the interrupted JS code was already throwing, any exceptions
    // thrown by the interrupt handler are silently swallowed.
    {
      Maybe<AutoReportException> are;
      if (!wasAlreadyThrowing) {
        are.emplace(cx);
      }
      result = JS_CallFunctionValue(cx, nullptr, sc->interruptFunc,
                                    JS::HandleValueArray::empty(), &rval);
    }
    savedExc.restore();

    if (rval.isBoolean()) {
      result = rval.toBoolean();
    } else {
      result = false;
    }
  } else {
    result = false;
  }

  if (!result && sc->exitCode == 0) {
    static const char msg[] = "Script terminated by interrupt handler.\n";
    fputs(msg, stderr);

    sc->exitCode = EXITCODE_TIMEOUT;
  }

  return result;
}

/*
 * Some UTF-8 files, notably those written using Notepad, have a Unicode
 * Byte-Order-Mark (BOM) as their first character. This is useless (byte-order
 * is meaningless for UTF-8) but causes a syntax error unless we skip it.
 */
static void SkipUTF8BOM(FILE* file) {
  int ch1 = fgetc(file);
  int ch2 = fgetc(file);
  int ch3 = fgetc(file);

  // Skip the BOM
  if (ch1 == 0xEF && ch2 == 0xBB && ch3 == 0xBF) {
    return;
  }

  // No BOM - revert
  if (ch3 != EOF) {
    ungetc(ch3, file);
  }
  if (ch2 != EOF) {
    ungetc(ch2, file);
  }
  if (ch1 != EOF) {
    ungetc(ch1, file);
  }
}

void EnvironmentPreparer::invoke(HandleObject global, Closure& closure) {
  MOZ_ASSERT(JS_IsGlobalObject(global));

  JSContext* cx = TlsContext.get();
  MOZ_ASSERT(!JS_IsExceptionPending(cx));

  AutoRealm ar(cx, global);
  AutoReportException are(cx);
  if (!closure(cx)) {
    return;
  }
}

static bool RegisterScriptPathWithModuleLoader(JSContext* cx,
                                               HandleScript script,
                                               const char* filename) {
  // Set the private value associated with a script to a object containing the
  // script's filename so that the module loader can use it to resolve
  // relative imports.

  RootedString path(cx, JS_NewStringCopyZ(cx, filename));
  if (!path) {
    return false;
  }

  RootedObject infoObject(cx, JS_NewPlainObject(cx));
  if (!infoObject) {
    return false;
  }

  RootedValue pathValue(cx, StringValue(path));
  if (!JS_DefineProperty(cx, infoObject, "path", pathValue, 0)) {
    return false;
  }

  JS::SetScriptPrivate(script, ObjectValue(*infoObject));
  return true;
}

enum class CompileUtf8 {
  InflateToUtf16,
  DontInflate,
};

static MOZ_MUST_USE bool RunFile(JSContext* cx, const char* filename,
                                 FILE* file, CompileUtf8 compileMethod,
                                 bool compileOnly) {
  SkipUTF8BOM(file);

  int64_t t1 = PRMJ_Now();
  RootedScript script(cx);

  {
    CompileOptions options(cx);
    options.setIntroductionType("js shell file")
        .setFileAndLine(filename, 1)
        .setIsRunOnce(true)
        .setNoScriptRval(true);

    if (compileMethod == CompileUtf8::DontInflate) {
      script = JS::CompileUtf8File(cx, options, file);
    } else {
      fprintf(stderr, "(compiling '%s' after inflating to UTF-16)\n", filename);

      FileContents buffer(cx);
      if (!ReadCompleteFile(cx, file, buffer)) {
        return false;
      }

      size_t length = buffer.length();
      auto chars = UniqueTwoByteChars(
          UTF8CharsToNewTwoByteCharsZ(
              cx,
              UTF8Chars(reinterpret_cast<const char*>(buffer.begin()),
                        buffer.length()),
              &length, js::MallocArena)
              .get());
      if (!chars) {
        return false;
      }

      JS::SourceText<char16_t> source;
      if (!source.init(cx, std::move(chars), length)) {
        return false;
      }

      script = JS::Compile(cx, options, source);
    }

    if (!script) {
      return false;
    }
  }

  if (!RegisterScriptPathWithModuleLoader(cx, script, filename)) {
    return false;
  }

#ifdef DEBUG
  if (dumpEntrainedVariables) {
    AnalyzeEntrainedVariables(cx, script);
  }
#endif
  if (!compileOnly) {
    if (!JS_ExecuteScript(cx, script)) {
      return false;
    }
    int64_t t2 = PRMJ_Now() - t1;
    if (printTiming) {
      printf("runtime = %.3f ms\n", double(t2) / PRMJ_USEC_PER_MSEC);
    }
  }
  return true;
}

#if defined(JS_BUILD_BINAST)

static MOZ_MUST_USE bool RunBinAST(JSContext* cx, const char* filename,
                                   FILE* file, bool compileOnly,
                                   JS::BinASTFormat format) {
  RootedScript script(cx);

  {
    CompileOptions options(cx);
    options.setFileAndLine(filename, 0)
        .setIsRunOnce(true)
        .setNoScriptRval(true);

    script = JS::DecodeBinAST(cx, options, file, format);
    if (!script) {
      return false;
    }
  }

  if (!RegisterScriptPathWithModuleLoader(cx, script, filename)) {
    return false;
  }

  if (compileOnly) {
    return true;
  }

  return JS_ExecuteScript(cx, script);
}

#endif  // JS_BUILD_BINAST

static bool InitModuleLoader(JSContext* cx) {
  // Decompress and evaluate the embedded module loader source to initialize
  // the module loader for the current compartment.

  uint32_t srcLen = moduleloader::GetRawScriptsSize();
  auto src = cx->make_pod_array<char>(srcLen);
  if (!src ||
      !DecompressString(moduleloader::compressedSources,
                        moduleloader::GetCompressedSize(),
                        reinterpret_cast<unsigned char*>(src.get()), srcLen)) {
    return false;
  }

  CompileOptions options(cx);
  options.setIntroductionType("shell module loader");
  options.setFileAndLine("shell/ModuleLoader.js", 1);
  options.setSelfHostingMode(false);
  options.setForceFullParse();
  options.setForceStrictMode();

  JS::SourceText<Utf8Unit> srcBuf;
  if (!srcBuf.init(cx, std::move(src), srcLen)) {
    return false;
  }

  RootedValue rv(cx);
  return JS::Evaluate(cx, options, srcBuf, &rv);
}

static bool GetModuleImportHook(JSContext* cx,
                                MutableHandleFunction resultOut) {
  Handle<GlobalObject*> global = cx->global();
  RootedValue hookValue(cx,
                        global->getReservedSlot(GlobalAppSlotModuleLoadHook));
  if (hookValue.isUndefined()) {
    JS_ReportErrorASCII(cx, "Module load hook not set");
    return false;
  }

  if (!hookValue.isObject() || !hookValue.toObject().is<JSFunction>()) {
    JS_ReportErrorASCII(cx, "Module load hook is not a function");
    return false;
  }

  resultOut.set(&hookValue.toObject().as<JSFunction>());
  return true;
}

static MOZ_MUST_USE bool RunModule(JSContext* cx, const char* filename,
                                   FILE* file, bool compileOnly) {
  // Execute a module by calling the module loader's import hook on the
  // resolved filename.

  RootedFunction importFun(cx);
  if (!GetModuleImportHook(cx, &importFun)) {
    return false;
  }

  RootedString path(cx, JS_NewStringCopyZ(cx, filename));
  if (!path) {
    return false;
  }

  path = ResolvePath(cx, path, RootRelative);
  if (!path) {
    return false;
  }

  JS::AutoValueArray<1> args(cx);
  args[0].setString(path);

  RootedValue value(cx);
  return JS_CallFunction(cx, nullptr, importFun, args, &value);
}

static void ShellCleanupFinalizationRegistryCallback(JSObject* registry,
                                                     void* data) {
  // In the browser this queues a task. Shell jobs correspond to microtasks so
  // we arrange for cleanup to happen after all jobs/microtasks have run.
  auto sc = static_cast<ShellContext*>(data);
  AutoEnterOOMUnsafeRegion oomUnsafe;
  if (!sc->finalizationRegistriesToCleanUp.append(registry)) {
    oomUnsafe.crash("ShellCleanupFinalizationRegistryCallback");
  }
}

// Run any FinalizationRegistry cleanup tasks and return whether any ran.
static bool MaybeRunFinalizationRegistryCleanupTasks(JSContext* cx) {
  ShellContext* sc = GetShellContext(cx);
  MOZ_ASSERT(!sc->quitting);

  Rooted<ShellContext::ObjectVector> registries(cx);
  std::swap(registries.get(), sc->finalizationRegistriesToCleanUp.get());

  bool ranTasks = false;

  RootedObject registry(cx);
  for (const auto& r : registries) {
    registry = r;

    AutoRealm ar(cx, registry);

    {
      AutoReportException are(cx);
      mozilla::Unused << JS::CleanupQueuedFinalizationRegistry(cx, registry);
    }

    ranTasks = true;

    if (sc->quitting) {
      break;
    }
  }

  return ranTasks;
}

static bool EnqueueJob(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!IsFunctionObject(args.get(0))) {
    JS_ReportErrorASCII(cx, "EnqueueJob's first argument must be a function");
    return false;
  }

  args.rval().setUndefined();

  RootedObject job(cx, &args[0].toObject());
  return js::EnqueueJob(cx, job);
}

static void RunShellJobs(JSContext* cx) {
  ShellContext* sc = GetShellContext(cx);
  if (sc->quitting) {
    return;
  }

  while (true) {
    // Run microtasks.
    js::RunJobs(cx);
    if (sc->quitting) {
      return;
    }

    // Run tasks (only finalization registry clean tasks are possible).
    bool ranTasks = MaybeRunFinalizationRegistryCleanupTasks(cx);
    if (!ranTasks) {
      break;
    }
  }
}

static bool DrainJobQueue(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (GetShellContext(cx)->quitting) {
    JS_ReportErrorASCII(
        cx, "Mustn't drain the job queue when the shell is quitting");
    return false;
  }

  RunShellJobs(cx);

  if (GetShellContext(cx)->quitting) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

static bool GlobalOfFirstJobInQueue(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  RootedObject job(cx, cx->internalJobQueue->maybeFront());
  if (!job) {
    JS_ReportErrorASCII(cx, "Job queue is empty");
    return false;
  }

  RootedObject global(cx, &job->nonCCWGlobal());
  if (!cx->compartment()->wrap(cx, &global)) {
    return false;
  }

  args.rval().setObject(*global);
  return true;
}

static bool TrackUnhandledRejections(JSContext* cx, JS::HandleObject promise,
                                     JS::PromiseRejectionHandlingState state) {
  ShellContext* sc = GetShellContext(cx);
  if (!sc->trackUnhandledRejections) {
    return true;
  }

#if defined(DEBUG) || defined(JS_OOM_BREAKPOINT)
  if (cx->runningOOMTest) {
    // When OOM happens, we cannot reliably track the set of unhandled
    // promise rejections. Throw error only when simulated OOM is used
    // *and* promises are used in the test.
    JS_ReportErrorASCII(
        cx,
        "Can't track unhandled rejections while running simulated OOM "
        "test. Call ignoreUnhandledRejections before using oomTest etc.");
    return false;
  }
#endif

  if (!sc->unhandledRejectedPromises) {
    sc->unhandledRejectedPromises = SetObject::create(cx);
    if (!sc->unhandledRejectedPromises) {
      return false;
    }
  }

  RootedValue promiseVal(cx, ObjectValue(*promise));

  AutoRealm ar(cx, sc->unhandledRejectedPromises);
  if (!cx->compartment()->wrap(cx, &promiseVal)) {
    return false;
  }

  switch (state) {
    case JS::PromiseRejectionHandlingState::Unhandled:
      if (!SetObject::add(cx, sc->unhandledRejectedPromises, promiseVal)) {
        return false;
      }
      break;
    case JS::PromiseRejectionHandlingState::Handled:
      bool deleted = false;
      if (!SetObject::delete_(cx, sc->unhandledRejectedPromises, promiseVal,
                              &deleted)) {
        return false;
      }
      // We can't MOZ_ASSERT(deleted) here, because it's possible we failed to
      // add the promise in the first place, due to OOM.
      break;
  }

  return true;
}

static void ForwardingPromiseRejectionTrackerCallback(
    JSContext* cx, bool mutedErrors, JS::HandleObject promise,
    JS::PromiseRejectionHandlingState state, void* data) {
  AutoReportException are(cx);

  if (!TrackUnhandledRejections(cx, promise, state)) {
    return;
  }

  RootedValue callback(cx,
                       GetShellContext(cx)->promiseRejectionTrackerCallback);
  if (callback.isNull()) {
    return;
  }

  AutoRealm ar(cx, &callback.toObject());

  FixedInvokeArgs<2> args(cx);
  args[0].setObject(*promise);
  args[1].setInt32(static_cast<int32_t>(state));

  if (!JS_WrapValue(cx, args[0])) {
    return;
  }

  RootedValue rval(cx);
  (void)Call(cx, callback, UndefinedHandleValue, args, &rval);
}

static bool SetPromiseRejectionTrackerCallback(JSContext* cx, unsigned argc,
                                               Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!IsFunctionObject(args.get(0))) {
    JS_ReportErrorASCII(
        cx,
        "setPromiseRejectionTrackerCallback expects a function as its sole "
        "argument");
    return false;
  }

  GetShellContext(cx)->promiseRejectionTrackerCallback = args[0];

  args.rval().setUndefined();
  return true;
}

static bool BoundToAsyncStack(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  RootedValue function(cx, GetFunctionNativeReserved(&args.callee(), 0));
  RootedObject options(
      cx, &GetFunctionNativeReserved(&args.callee(), 1).toObject());

  RootedSavedFrame stack(cx, nullptr);
  bool isExplicit;

  RootedValue v(cx);

  if (!JS_GetProperty(cx, options, "stack", &v)) {
    return false;
  }
  if (!v.isObject() || !v.toObject().is<SavedFrame>()) {
    JS_ReportErrorASCII(cx,
                        "The 'stack' property must be a SavedFrame object.");
    return false;
  }
  stack = &v.toObject().as<SavedFrame>();

  if (!JS_GetProperty(cx, options, "cause", &v)) {
    return false;
  }
  RootedString causeString(cx, ToString(cx, v));
  if (!causeString) {
    MOZ_ASSERT(cx->isExceptionPending());
    return false;
  }

  UniqueChars cause = JS_EncodeStringToUTF8(cx, causeString);
  if (!cause) {
    MOZ_ASSERT(cx->isExceptionPending());
    return false;
  }

  if (!JS_GetProperty(cx, options, "explicit", &v)) {
    return false;
  }
  isExplicit = v.isUndefined() ? true : ToBoolean(v);

  auto kind =
      (isExplicit ? JS::AutoSetAsyncStackForNewCalls::AsyncCallKind::EXPLICIT
                  : JS::AutoSetAsyncStackForNewCalls::AsyncCallKind::IMPLICIT);

  JS::AutoSetAsyncStackForNewCalls asasfnckthxbye(cx, stack, cause.get(), kind);
  return Call(cx, UndefinedHandleValue, function, JS::HandleValueArray::empty(),
              args.rval());
}

static bool BindToAsyncStack(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() != 2) {
    JS_ReportErrorASCII(cx, "bindToAsyncStack takes exactly two arguments.");
    return false;
  }

  if (!args[0].isObject() || !IsCallable(args[0])) {
    JS_ReportErrorASCII(
        cx, "bindToAsyncStack's first argument should be a function.");
    return false;
  }

  if (!args[1].isObject()) {
    JS_ReportErrorASCII(
        cx, "bindToAsyncStack's second argument should be an object.");
    return false;
  }

  RootedFunction bound(cx, NewFunctionWithReserved(cx, BoundToAsyncStack, 0, 0,
                                                   "bindToAsyncStack thunk"));
  if (!bound) {
    return false;
  }
  SetFunctionNativeReserved(bound, 0, args[0]);
  SetFunctionNativeReserved(bound, 1, args[1]);

  args.rval().setObject(*bound);
  return true;
}

#ifdef JS_HAS_INTL_API
static bool AddIntlExtras(JSContext* cx, unsigned argc, Value* vp) {
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
      JS_FS_END};

  if (!JS_DefineFunctions(cx, intl, funcs)) {
    return false;
  }

  if (!js::AddMozDateTimeFormatConstructor(cx, intl)) {
    return false;
  }

  if (!js::AddListFormatConstructor(cx, intl)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}
#endif  // JS_HAS_INTL_API

static MOZ_MUST_USE bool EvalUtf8AndPrint(JSContext* cx, const char* bytes,
                                          size_t length, int lineno,
                                          bool compileOnly) {
  // Eval.
  JS::CompileOptions options(cx);
  options.setIntroductionType("js shell interactive")
      .setIsRunOnce(true)
      .setFileAndLine("typein", lineno);

  JS::SourceText<Utf8Unit> srcBuf;
  if (!srcBuf.init(cx, bytes, length, JS::SourceOwnership::Borrowed)) {
    return false;
  }

  RootedScript script(cx, JS::Compile(cx, options, srcBuf));
  if (!script) {
    return false;
  }
  if (compileOnly) {
    return true;
  }
  RootedValue result(cx);
  if (!JS_ExecuteScript(cx, script, &result)) {
    return false;
  }

  if (!result.isUndefined() && gOutFile->isOpen()) {
    // Print.
    RootedString str(cx, JS_ValueToSource(cx, result));
    if (!str) {
      return false;
    }

    UniqueChars utf8chars = JS_EncodeStringToUTF8(cx, str);
    if (!utf8chars) {
      return false;
    }
    fprintf(gOutFile->fp, "%s\n", utf8chars.get());
  }
  return true;
}

static MOZ_MUST_USE bool ReadEvalPrintLoop(JSContext* cx, FILE* in,
                                           bool compileOnly) {
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

      mozilla::UniqueFreePtr<char[]> line =
          GetLine(in, startline == lineno ? "js> " : "");
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

      if (!buffer.append(line.get(), strlen(line.get())) ||
          !buffer.append('\n')) {
        return false;
      }

      lineno++;
      if (!ScheduleWatchdog(cx, sc->timeoutInterval)) {
        hitEOF = true;
        break;
      }
    } while (!JS_Utf8BufferIsCompilableUnit(cx, cx->global(), buffer.begin(),
                                            buffer.length()));

    if (hitEOF && buffer.empty()) {
      break;
    }

    {
      // Report exceptions but keep going.
      AutoReportException are(cx);
      mozilla::Unused << EvalUtf8AndPrint(cx, buffer.begin(), buffer.length(),
                                          startline, compileOnly);
    }

    // If a let or const fail to initialize they will remain in an unusable
    // without further intervention. This call cleans up the global scope,
    // setting uninitialized lexicals to undefined so that they may still
    // be used. This behavior is _only_ acceptable in the context of the repl.
    if (JS::ForceLexicalInitialization(cx, globalLexical) &&
        gErrFile->isOpen()) {
      fputs(
          "Warning: According to the standard, after the above exception,\n"
          "Warning: the global bindings should be permanently uninitialized.\n"
          "Warning: We have non-standard-ly initialized them to `undefined`"
          "for you.\nWarning: This nicety only happens in the JS shell.\n",
          stderr);
    }

    RunShellJobs(cx);
  } while (!hitEOF && !sc->quitting);

  if (gOutFile->isOpen()) {
    fprintf(gOutFile->fp, "\n");
  }

  return true;
}

enum FileKind {
  FileScript,       // UTF-8, directly parsed as such
  FileScriptUtf16,  // FileScript, but inflate to UTF-16 before parsing
  FileModule,
  FileBinASTMultipart,
  FileBinASTContext,
};

static void ReportCantOpenErrorUnknownEncoding(JSContext* cx,
                                               const char* filename) {
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

static MOZ_MUST_USE bool Process(JSContext* cx, const char* filename,
                                 bool forceTTY, FileKind kind) {
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
    switch (kind) {
      case FileScript:
        if (!RunFile(cx, filename, file, CompileUtf8::DontInflate,
                     compileOnly)) {
          return false;
        }
        break;
      case FileScriptUtf16:
        if (!RunFile(cx, filename, file, CompileUtf8::InflateToUtf16,
                     compileOnly)) {
          return false;
        }
        break;
      case FileModule:
        if (!RunModule(cx, filename, file, compileOnly)) {
          return false;
        }
        break;
#if defined(JS_BUILD_BINAST)
      case FileBinASTMultipart:
        if (!RunBinAST(cx, filename, file, compileOnly,
                       JS::BinASTFormat::Multipart)) {
          return false;
        }
        break;
      case FileBinASTContext:
        if (!RunBinAST(cx, filename, file, compileOnly,
                       JS::BinASTFormat::Context)) {
          return false;
        }
        break;
#endif  // JS_BUILD_BINAST
      default:
        MOZ_CRASH("Impossible FileKind!");
    }
  } else {
    // It's an interactive filehandle; drop into read-eval-print loop.
    MOZ_ASSERT(kind == FileScript);
    if (!ReadEvalPrintLoop(cx, file, compileOnly)) {
      return false;
    }
  }
  return true;
}

#ifdef XP_WIN
#  define GET_FD_FROM_FILE(a) int(_get_osfhandle(fileno(a)))
#else
#  define GET_FD_FROM_FILE(a) fileno(a)
#endif

static bool CreateMappedArrayBuffer(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() < 1 || args.length() > 3) {
    JS_ReportErrorNumberASCII(
        cx, my_GetErrorMessage, nullptr,
        args.length() < 1 ? JSSMSG_NOT_ENOUGH_ARGS : JSSMSG_TOO_MANY_ARGS,
        "createMappedArrayBuffer");
    return false;
  }

  RootedString rawFilenameStr(cx, JS::ToString(cx, args[0]));
  if (!rawFilenameStr) {
    return false;
  }
  // It's a little bizarre to resolve relative to the script, but for testing
  // I need a file at a known location, and the only good way I know of to do
  // that right now is to include it in the repo alongside the test script.
  // Bug 944164 would introduce an alternative.
  JSString* filenameStr = ResolvePath(cx, rawFilenameStr, ScriptRelative);
  if (!filenameStr) {
    return false;
  }
  UniqueChars filename = JS_EncodeStringToLatin1(cx, filenameStr);
  if (!filename) {
    return false;
  }

  uint32_t offset = 0;
  if (args.length() >= 2) {
    if (!JS::ToUint32(cx, args[1], &offset)) {
      return false;
    }
  }

  bool sizeGiven = false;
  uint32_t size;
  if (args.length() >= 3) {
    if (!JS::ToUint32(cx, args[2], &size)) {
      return false;
    }
    sizeGiven = true;
    if (size == 0) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_BAD_ARRAY_LENGTH);
      return false;
    }
  }

  FILE* file = fopen(filename.get(), "rb");
  if (!file) {
    ReportCantOpenErrorUnknownEncoding(cx, filename.get());
    return false;
  }
  AutoCloseFile autoClose(file);

  struct stat st;
  if (fstat(fileno(file), &st) < 0) {
    JS_ReportErrorASCII(cx, "Unable to stat file");
    return false;
  }

  if ((st.st_mode & S_IFMT) != S_IFREG) {
    JS_ReportErrorASCII(cx, "Path is not a regular file");
    return false;
  }

  if (!sizeGiven) {
    if (off_t(offset) >= st.st_size) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_OFFSET_LARGER_THAN_FILESIZE);
      return false;
    }
    size = st.st_size - offset;
  }

  void* contents =
      JS::CreateMappedArrayBufferContents(GET_FD_FROM_FILE(file), offset, size);
  if (!contents) {
    JS_ReportErrorASCII(cx,
                        "failed to allocate mapped array buffer contents "
                        "(possibly due to bad alignment)");
    return false;
  }

  RootedObject obj(cx,
                   JS::NewMappedArrayBufferWithContents(cx, size, contents));
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

#undef GET_FD_FROM_FILE

static bool AddPromiseReactions(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() != 3) {
    JS_ReportErrorNumberASCII(
        cx, my_GetErrorMessage, nullptr,
        args.length() < 3 ? JSSMSG_NOT_ENOUGH_ARGS : JSSMSG_TOO_MANY_ARGS,
        "addPromiseReactions");
    return false;
  }

  RootedObject promise(cx);
  if (args[0].isObject()) {
    promise = &args[0].toObject();
  }

  if (!promise || !JS::IsPromiseObject(promise)) {
    JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                              JSSMSG_INVALID_ARGS, "addPromiseReactions");
    return false;
  }

  RootedObject onResolve(cx);
  if (args[1].isObject()) {
    onResolve = &args[1].toObject();
  }

  RootedObject onReject(cx);
  if (args[2].isObject()) {
    onReject = &args[2].toObject();
  }

  if (!onResolve || !onResolve->is<JSFunction>() || !onReject ||
      !onReject->is<JSFunction>()) {
    JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                              JSSMSG_INVALID_ARGS, "addPromiseReactions");
    return false;
  }

  return JS::AddPromiseReactions(cx, promise, onResolve, onReject);
}

static bool IgnoreUnhandledRejections(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  ShellContext* sc = GetShellContext(cx);
  sc->trackUnhandledRejections = false;

  args.rval().setUndefined();
  return true;
}

static bool Options(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  JS::ContextOptions oldContextOptions = JS::ContextOptionsRef(cx);
  for (unsigned i = 0; i < args.length(); i++) {
    RootedString str(cx, JS::ToString(cx, args[i]));
    if (!str) {
      return false;
    }

    RootedLinearString opt(cx, str->ensureLinear(cx));
    if (!opt) {
      return false;
    }

    if (StringEqualsLiteral(opt, "throw_on_asmjs_validation_failure")) {
      JS::ContextOptionsRef(cx).toggleThrowOnAsmJSValidationFailure();
    } else if (StringEqualsLiteral(opt, "strict_mode")) {
      JS::ContextOptionsRef(cx).toggleStrictMode();
    } else {
      UniqueChars optChars = QuoteString(cx, opt, '"');
      if (!optChars) {
        return false;
      }

      JS_ReportErrorASCII(cx,
                          "unknown option name %s."
                          " The valid names are "
                          "throw_on_asmjs_validation_failure and strict_mode.",
                          optChars.get());
      return false;
    }
  }

  UniqueChars names = DuplicateString("");
  bool found = false;
  if (names && oldContextOptions.throwOnAsmJSValidationFailure()) {
    names = JS_sprintf_append(std::move(names), "%s%s", found ? "," : "",
                              "throw_on_asmjs_validation_failure");
    found = true;
  }
  if (names && oldContextOptions.strictMode()) {
    names = JS_sprintf_append(std::move(names), "%s%s", found ? "," : "",
                              "strict_mode");
    found = true;
  }
  if (!names) {
    JS_ReportOutOfMemory(cx);
    return false;
  }

  JSString* str = JS_NewStringCopyZ(cx, names.get());
  if (!str) {
    return false;
  }
  args.rval().setString(str);
  return true;
}

static bool LoadScript(JSContext* cx, unsigned argc, Value* vp,
                       bool scriptRelative) {
  CallArgs args = CallArgsFromVp(argc, vp);

  RootedString str(cx);
  for (unsigned i = 0; i < args.length(); i++) {
    str = JS::ToString(cx, args[i]);
    if (!str) {
      JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                                JSSMSG_INVALID_ARGS, "load");
      return false;
    }

    str = ResolvePath(cx, str, scriptRelative ? ScriptRelative : RootRelative);
    if (!str) {
      JS_ReportErrorASCII(cx, "unable to resolve path");
      return false;
    }

    UniqueChars filename = JS_EncodeStringToLatin1(cx, str);
    if (!filename) {
      return false;
    }

    errno = 0;

    CompileOptions opts(cx);
    opts.setIntroductionType("js shell load")
        .setIsRunOnce(true)
        .setNoScriptRval(true);

    RootedValue unused(cx);
    if (!(compileOnly
              ? JS::CompileUtf8Path(cx, opts, filename.get()) != nullptr
              : JS::EvaluateUtf8Path(cx, opts, filename.get(), &unused))) {
      return false;
    }
  }

  args.rval().setUndefined();
  return true;
}

static bool Load(JSContext* cx, unsigned argc, Value* vp) {
  return LoadScript(cx, argc, vp, false);
}

static bool LoadScriptRelativeToScript(JSContext* cx, unsigned argc,
                                       Value* vp) {
  return LoadScript(cx, argc, vp, true);
}

// Populate |options| with the options given by |opts|'s properties. If we
// need to convert a filename to a C string, let fileNameBytes own the
// bytes.
static bool ParseCompileOptions(JSContext* cx, CompileOptions& options,
                                HandleObject opts, UniqueChars& fileNameBytes) {
  RootedValue v(cx);
  RootedString s(cx);

  if (!JS_GetProperty(cx, opts, "isRunOnce", &v)) {
    return false;
  }
  if (!v.isUndefined()) {
    options.setIsRunOnce(ToBoolean(v));
  }

  if (!JS_GetProperty(cx, opts, "noScriptRval", &v)) {
    return false;
  }
  if (!v.isUndefined()) {
    options.setNoScriptRval(ToBoolean(v));
  }

  if (!JS_GetProperty(cx, opts, "fileName", &v)) {
    return false;
  }
  if (v.isNull()) {
    options.setFile(nullptr);
  } else if (!v.isUndefined()) {
    s = ToString(cx, v);
    if (!s) {
      return false;
    }
    fileNameBytes = JS_EncodeStringToLatin1(cx, s);
    if (!fileNameBytes) {
      return false;
    }
    options.setFile(fileNameBytes.get());
  }

  if (!JS_GetProperty(cx, opts, "skipFileNameValidation", &v)) {
    return false;
  }
  if (!v.isUndefined()) {
    options.setSkipFilenameValidation(ToBoolean(v));
  }

  if (!JS_GetProperty(cx, opts, "element", &v)) {
    return false;
  }
  if (v.isObject()) {
    options.setElement(&v.toObject());
  }

  if (!JS_GetProperty(cx, opts, "elementAttributeName", &v)) {
    return false;
  }
  if (!v.isUndefined()) {
    s = ToString(cx, v);
    if (!s) {
      return false;
    }
    options.setElementAttributeName(s);
  }

  if (!JS_GetProperty(cx, opts, "lineNumber", &v)) {
    return false;
  }
  if (!v.isUndefined()) {
    uint32_t u;
    if (!ToUint32(cx, v, &u)) {
      return false;
    }
    options.setLine(u);
  }

  if (!JS_GetProperty(cx, opts, "columnNumber", &v)) {
    return false;
  }
  if (!v.isUndefined()) {
    int32_t c;
    if (!ToInt32(cx, v, &c)) {
      return false;
    }
    options.setColumn(c);
  }

  if (!JS_GetProperty(cx, opts, "sourceIsLazy", &v)) {
    return false;
  }
  if (v.isBoolean()) {
    options.setSourceIsLazy(v.toBoolean());
  }

  return true;
}

static void my_LargeAllocFailCallback() {
  JSContext* cx = TlsContext.get();
  if (!cx || cx->isHelperThreadContext()) {
    return;
  }

  MOZ_ASSERT(!JS::RuntimeHeapIsBusy());

  JS::PrepareForFullGC(cx);
  cx->runtime()->gc.gc(GC_NORMAL, JS::GCReason::SHARED_MEMORY_LIMIT);
}

static const uint32_t CacheEntry_SOURCE = 0;
static const uint32_t CacheEntry_BYTECODE = 1;

static const JSClass CacheEntry_class = {"CacheEntryObject",
                                         JSCLASS_HAS_RESERVED_SLOTS(2)};

static bool CacheEntry(JSContext* cx, unsigned argc, JS::Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() != 1 || !args[0].isString()) {
    JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                              JSSMSG_INVALID_ARGS, "CacheEntry");
    return false;
  }

  RootedObject obj(cx, JS_NewObject(cx, &CacheEntry_class));
  if (!obj) {
    return false;
  }

  SetReservedSlot(obj, CacheEntry_SOURCE, args[0]);
  SetReservedSlot(obj, CacheEntry_BYTECODE, UndefinedValue());
  args.rval().setObject(*obj);
  return true;
}

static bool CacheEntry_isCacheEntry(JSObject* cache) {
  return JS_GetClass(cache) == &CacheEntry_class;
}

static JSString* CacheEntry_getSource(JSContext* cx, HandleObject cache) {
  MOZ_ASSERT(CacheEntry_isCacheEntry(cache));
  Value v = JS_GetReservedSlot(cache, CacheEntry_SOURCE);
  if (!v.isString()) {
    JS_ReportErrorASCII(
        cx, "CacheEntry_getSource: Unexpected type of source reserved slot.");
    return nullptr;
  }

  return v.toString();
}

static uint8_t* CacheEntry_getBytecode(JSContext* cx, HandleObject cache,
                                       uint32_t* length) {
  MOZ_ASSERT(CacheEntry_isCacheEntry(cache));
  Value v = JS_GetReservedSlot(cache, CacheEntry_BYTECODE);
  if (!v.isObject() || !v.toObject().is<ArrayBufferObject>()) {
    JS_ReportErrorASCII(
        cx,
        "CacheEntry_getBytecode: Unexpected type of bytecode reserved slot.");
    return nullptr;
  }

  ArrayBufferObject* arrayBuffer = &v.toObject().as<ArrayBufferObject>();
  *length = arrayBuffer->byteLength();
  return arrayBuffer->dataPointer();
}

static bool CacheEntry_setBytecode(JSContext* cx, HandleObject cache,
                                   uint8_t* buffer, uint32_t length) {
  MOZ_ASSERT(CacheEntry_isCacheEntry(cache));

  using BufferContents = ArrayBufferObject::BufferContents;

  BufferContents contents = BufferContents::createMalloced(buffer);
  Rooted<ArrayBufferObject*> arrayBuffer(
      cx, ArrayBufferObject::createForContents(cx, length, contents));
  if (!arrayBuffer) {
    return false;
  }

  SetReservedSlot(cache, CacheEntry_BYTECODE, ObjectValue(*arrayBuffer));
  return true;
}

static bool ConvertTranscodeResultToJSException(JSContext* cx,
                                                JS::TranscodeResult rv) {
  switch (rv) {
    case JS::TranscodeResult_Ok:
      return true;

    default:
      [[fallthrough]];
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
    case JS::TranscodeResult_Failure_BadDecode:
      MOZ_ASSERT(!cx->isExceptionPending());
      JS_ReportErrorASCII(cx, "XDR data corruption");
      return false;
    case JS::TranscodeResult_Failure_WrongCompileOption:
      MOZ_ASSERT(!cx->isExceptionPending());
      JS_ReportErrorASCII(
          cx, "Compile options differs from Compile options of the encoding");
      return false;
    case JS::TranscodeResult_Failure_NotInterpretedFun:
      MOZ_ASSERT(!cx->isExceptionPending());
      JS_ReportErrorASCII(cx,
                          "Only interepreted functions are supported by XDR");
      return false;

    case JS::TranscodeResult_Throw:
      MOZ_ASSERT(cx->isExceptionPending());
      return false;
  }
}

static bool Evaluate(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() < 1 || args.length() > 2) {
    JS_ReportErrorNumberASCII(
        cx, my_GetErrorMessage, nullptr,
        args.length() < 1 ? JSSMSG_NOT_ENOUGH_ARGS : JSSMSG_TOO_MANY_ARGS,
        "evaluate");
    return false;
  }

  RootedString code(cx, nullptr);
  RootedObject cacheEntry(cx, nullptr);
  if (args[0].isString()) {
    code = args[0].toString();
  } else if (args[0].isObject() &&
             CacheEntry_isCacheEntry(&args[0].toObject())) {
    cacheEntry = &args[0].toObject();
    code = CacheEntry_getSource(cx, cacheEntry);
    if (!code) {
      return false;
    }
  }

  if (!code || (args.length() == 2 && args[1].isPrimitive())) {
    JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                              JSSMSG_INVALID_ARGS, "evaluate");
    return false;
  }

  CompileOptions options(cx);
  UniqueChars fileNameBytes;
  RootedString displayURL(cx);
  RootedString sourceMapURL(cx);
  RootedObject global(cx, nullptr);
  bool catchTermination = false;
  bool loadBytecode = false;
  bool saveBytecode = false;
  bool saveIncrementalBytecode = false;
  bool assertEqBytecode = false;
  JS::RootedObjectVector envChain(cx);
  RootedObject callerGlobal(cx, cx->global());

  options.setIntroductionType("js shell evaluate")
      .setFileAndLine("@evaluate", 1);

  global = JS::CurrentGlobalOrNull(cx);
  MOZ_ASSERT(global);

  if (args.length() == 2) {
    RootedObject opts(cx, &args[1].toObject());
    RootedValue v(cx);

    if (!ParseCompileOptions(cx, options, opts, fileNameBytes)) {
      return false;
    }

    if (!JS_GetProperty(cx, opts, "displayURL", &v)) {
      return false;
    }
    if (!v.isUndefined()) {
      displayURL = ToString(cx, v);
      if (!displayURL) {
        return false;
      }
    }

    if (!JS_GetProperty(cx, opts, "sourceMapURL", &v)) {
      return false;
    }
    if (!v.isUndefined()) {
      sourceMapURL = ToString(cx, v);
      if (!sourceMapURL) {
        return false;
      }
    }

    if (!JS_GetProperty(cx, opts, "global", &v)) {
      return false;
    }
    if (!v.isUndefined()) {
      if (v.isObject()) {
        global = js::CheckedUnwrapDynamic(&v.toObject(), cx,
                                          /* stopAtWindowProxy = */ false);
        if (!global) {
          return false;
        }
      }
      if (!global || !(JS_GetClass(global)->flags & JSCLASS_IS_GLOBAL)) {
        JS_ReportErrorNumberASCII(
            cx, GetErrorMessage, nullptr, JSMSG_UNEXPECTED_TYPE,
            "\"global\" passed to evaluate()", "not a global object");
        return false;
      }
    }

    if (!JS_GetProperty(cx, opts, "catchTermination", &v)) {
      return false;
    }
    if (!v.isUndefined()) {
      catchTermination = ToBoolean(v);
    }

    if (!JS_GetProperty(cx, opts, "loadBytecode", &v)) {
      return false;
    }
    if (!v.isUndefined()) {
      loadBytecode = ToBoolean(v);
    }

    if (!JS_GetProperty(cx, opts, "saveBytecode", &v)) {
      return false;
    }
    if (!v.isUndefined()) {
      saveBytecode = ToBoolean(v);
    }

    if (!JS_GetProperty(cx, opts, "saveIncrementalBytecode", &v)) {
      return false;
    }
    if (!v.isUndefined()) {
      saveIncrementalBytecode = ToBoolean(v);
    }

    if (!JS_GetProperty(cx, opts, "assertEqBytecode", &v)) {
      return false;
    }
    if (!v.isUndefined()) {
      assertEqBytecode = ToBoolean(v);
    }

    if (!JS_GetProperty(cx, opts, "envChainObject", &v)) {
      return false;
    }
    if (!v.isUndefined()) {
      if (loadBytecode) {
        JS_ReportErrorASCII(cx,
                            "Can't use both loadBytecode and envChainObject");
        return false;
      }

      if (!v.isObject()) {
        JS_ReportErrorNumberASCII(
            cx, GetErrorMessage, nullptr, JSMSG_UNEXPECTED_TYPE,
            "\"envChainObject\" passed to evaluate()", "not an object");
        return false;
      } else if (v.toObject().is<GlobalObject>()) {
        JS_ReportErrorASCII(
            cx,
            "\"envChainObject\" passed to evaluate() should not be a global");
        return false;
      } else if (!envChain.append(&v.toObject())) {
        JS_ReportOutOfMemory(cx);
        return false;
      }
    }

    // We cannot load or save the bytecode if we have no object where the
    // bytecode cache is stored.
    if (loadBytecode || saveBytecode || saveIncrementalBytecode) {
      if (!cacheEntry) {
        JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                                  JSSMSG_INVALID_ARGS, "evaluate");
        return false;
      }
      if (saveIncrementalBytecode && saveBytecode) {
        JS_ReportErrorASCII(
            cx,
            "saveIncrementalBytecode and saveBytecode cannot be used"
            " at the same time.");
        return false;
      }
    }
  }

  AutoStableStringChars codeChars(cx);
  if (!codeChars.initTwoByte(cx, code)) {
    return false;
  }

  JS::TranscodeBuffer loadBuffer;
  JS::TranscodeBuffer saveBuffer;

  if (loadBytecode) {
    uint32_t loadLength = 0;
    uint8_t* loadData = nullptr;
    loadData = CacheEntry_getBytecode(cx, cacheEntry, &loadLength);
    if (!loadData) {
      return false;
    }
    if (!loadBuffer.append(loadData, loadLength)) {
      JS_ReportOutOfMemory(cx);
      return false;
    }
  }

  {
    JSAutoRealm ar(cx, global);
    RootedScript script(cx);

    {
      if (saveBytecode) {
        if (!JS::RealmCreationOptionsRef(cx).cloneSingletons()) {
          JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                                    JSSMSG_CACHE_SINGLETON_FAILED);
          return false;
        }

        // cloneSingletons implies that singletons are used as template objects.
        MOZ_ASSERT(JS::RealmBehaviorsRef(cx).getSingletonsAsTemplates());
      }

      if (loadBytecode) {
        JS::TranscodeResult rv = JS::DecodeScript(cx, loadBuffer, &script);
        if (!ConvertTranscodeResultToJSException(cx, rv)) {
          return false;
        }
      } else {
        mozilla::Range<const char16_t> chars = codeChars.twoByteRange();
        JS::SourceText<char16_t> srcBuf;
        if (!srcBuf.init(cx, chars.begin().get(), chars.length(),
                         JS::SourceOwnership::Borrowed)) {
          return false;
        }

        if (envChain.length() == 0) {
          script = JS::Compile(cx, options, srcBuf);
        } else {
          script = JS::CompileForNonSyntacticScope(cx, options, srcBuf);
        }
      }

      if (!script) {
        return false;
      }
    }

    if (displayURL && !script->scriptSource()->hasDisplayURL()) {
      UniqueTwoByteChars chars = JS_CopyStringCharsZ(cx, displayURL);
      if (!chars) {
        return false;
      }
      if (!script->scriptSource()->setDisplayURL(cx, std::move(chars))) {
        return false;
      }
    }
    if (sourceMapURL && !script->scriptSource()->hasSourceMapURL()) {
      UniqueTwoByteChars chars = JS_CopyStringCharsZ(cx, sourceMapURL);
      if (!chars) {
        return false;
      }
      if (!script->scriptSource()->setSourceMapURL(cx, std::move(chars))) {
        return false;
      }
    }

    // If we want to save the bytecode incrementally, then we should
    // register ahead the fact that every JSFunction which is being
    // delazified should be encoded at the end of the delazification.
    if (saveIncrementalBytecode) {
      if (!StartIncrementalEncoding(cx, script)) {
        return false;
      }
    }

    if (!JS_ExecuteScript(cx, envChain, script, args.rval())) {
      if (catchTermination && !JS_IsExceptionPending(cx)) {
        JSAutoRealm ar1(cx, callerGlobal);
        JSString* str = JS_NewStringCopyZ(cx, "terminated");
        if (!str) {
          return false;
        }
        args.rval().setString(str);
        return true;
      }
      return false;
    }

    // Encode the bytecode after the execution of the script.
    if (saveBytecode) {
      JS::TranscodeResult rv = JS::EncodeScript(cx, saveBuffer, script);
      if (!ConvertTranscodeResultToJSException(cx, rv)) {
        return false;
      }
    }

    // Serialize the encoded bytecode, recorded before the execution, into a
    // buffer which can be deserialized linearly.
    if (saveIncrementalBytecode) {
      if (!FinishIncrementalEncoding(cx, script, saveBuffer)) {
        return false;
      }
    }
  }

  if (saveBytecode || saveIncrementalBytecode) {
    // If we are both loading and saving, we assert that we are going to
    // replace the current bytecode by the same stream of bytes.
    if (loadBytecode && assertEqBytecode) {
      if (saveBuffer.length() != loadBuffer.length()) {
        char loadLengthStr[16];
        SprintfLiteral(loadLengthStr, "%zu", loadBuffer.length());
        char saveLengthStr[16];
        SprintfLiteral(saveLengthStr, "%zu", saveBuffer.length());

        JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                                  JSSMSG_CACHE_EQ_SIZE_FAILED, loadLengthStr,
                                  saveLengthStr);
        return false;
      }

      if (!ArrayEqual(loadBuffer.begin(), saveBuffer.begin(),
                      loadBuffer.length())) {
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

JSString* js::shell::FileAsString(JSContext* cx, JS::HandleString pathnameStr) {
  UniqueChars pathname = JS_EncodeStringToLatin1(cx, pathnameStr);
  if (!pathname) {
    return nullptr;
  }

  FILE* file;

  file = fopen(pathname.get(), "rb");
  if (!file) {
    ReportCantOpenErrorUnknownEncoding(cx, pathname.get());
    return nullptr;
  }

  AutoCloseFile autoClose(file);

  struct stat st;
  if (fstat(fileno(file), &st) != 0) {
    JS_ReportErrorUTF8(cx, "can't stat %s", pathname.get());
    return nullptr;
  }

  if ((st.st_mode & S_IFMT) != S_IFREG) {
    JS_ReportErrorUTF8(cx, "can't read non-regular file %s", pathname.get());
    return nullptr;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    pathname = JS_EncodeStringToUTF8(cx, pathnameStr);
    if (!pathname) {
      return nullptr;
    }
    JS_ReportErrorUTF8(cx, "can't seek end of %s", pathname.get());
    return nullptr;
  }

  long endPos = ftell(file);
  if (endPos < 0) {
    JS_ReportErrorUTF8(cx, "can't read length of %s", pathname.get());
    return nullptr;
  }

  size_t len = endPos;
  if (fseek(file, 0, SEEK_SET) != 0) {
    pathname = JS_EncodeStringToUTF8(cx, pathnameStr);
    if (!pathname) {
      return nullptr;
    }
    JS_ReportErrorUTF8(cx, "can't seek start of %s", pathname.get());
    return nullptr;
  }

  UniqueChars buf(js_pod_malloc<char>(len + 1));
  if (!buf) {
    JS_ReportErrorUTF8(cx, "out of memory reading %s", pathname.get());
    return nullptr;
  }

  size_t cc = fread(buf.get(), 1, len, file);
  if (cc != len) {
    if (ptrdiff_t(cc) < 0) {
      ReportCantOpenErrorUnknownEncoding(cx, pathname.get());
    } else {
      pathname = JS_EncodeStringToUTF8(cx, pathnameStr);
      if (!pathname) {
        return nullptr;
      }
      JS_ReportErrorUTF8(cx, "can't read %s: short read", pathname.get());
    }
    return nullptr;
  }

  UniqueTwoByteChars ucbuf(
      JS::LossyUTF8CharsToNewTwoByteCharsZ(cx, JS::UTF8Chars(buf.get(), len),
                                           &len, js::MallocArena)
          .get());
  if (!ucbuf) {
    pathname = JS_EncodeStringToUTF8(cx, pathnameStr);
    if (!pathname) {
      return nullptr;
    }
    JS_ReportErrorUTF8(cx, "Invalid UTF-8 in file '%s'", pathname.get());
    return nullptr;
  }

  return JS_NewUCStringCopyN(cx, ucbuf.get(), len);
}

/*
 * Function to run scripts and return compilation + execution time. Semantics
 * are closely modelled after the equivalent function in WebKit, as this is used
 * to produce benchmark timings by SunSpider.
 */
static bool Run(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 1) {
    JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                              JSSMSG_INVALID_ARGS, "run");
    return false;
  }

  RootedString str(cx, JS::ToString(cx, args[0]));
  if (!str) {
    return false;
  }
  args[0].setString(str);

  str = FileAsString(cx, str);
  if (!str) {
    return false;
  }

  AutoStableStringChars chars(cx);
  if (!chars.initTwoByte(cx, str)) {
    return false;
  }

  JS::SourceText<char16_t> srcBuf;
  if (!srcBuf.init(cx, chars.twoByteRange().begin().get(), str->length(),
                   JS::SourceOwnership::Borrowed)) {
    return false;
  }

  RootedScript script(cx);
  int64_t startClock = PRMJ_Now();
  {
    /* FIXME: This should use UTF-8 (bug 987069). */
    UniqueChars filename = JS_EncodeStringToLatin1(cx, str);
    if (!filename) {
      return false;
    }

    JS::CompileOptions options(cx);
    options.setIntroductionType("js shell run")
        .setFileAndLine(filename.get(), 1)
        .setIsRunOnce(true)
        .setNoScriptRval(true);

    script = JS::Compile(cx, options, srcBuf);
    if (!script) {
      return false;
    }
  }

  if (!JS_ExecuteScript(cx, script)) {
    return false;
  }

  int64_t endClock = PRMJ_Now();

  args.rval().setDouble((endClock - startClock) / double(PRMJ_USEC_PER_MSEC));
  return true;
}

/*
 * function readline()
 * Provides a hook for scripts to read a line from stdin.
 */
static bool ReadLine(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

#define BUFSIZE 256
  FILE* from = stdin;
  size_t buflength = 0;
  size_t bufsize = BUFSIZE;
  char* buf = (char*)JS_malloc(cx, bufsize);
  if (!buf) {
    return false;
  }

  bool sawNewline = false;
  size_t gotlength;
  while ((gotlength = js_fgets(buf + buflength, bufsize - buflength, from)) >
         0) {
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
  JSString* str =
      JS_NewStringCopyN(cx, buf, sawNewline ? buflength - 1 : buflength);
  JS_free(cx, buf);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

/*
 * function readlineBuf()
 * Provides a hook for scripts to emulate readline() using a string object.
 */
static bool ReadLineBuf(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  ShellContext* sc = GetShellContext(cx);

  if (!args.length()) {
    if (!sc->readLineBuf) {
      JS_ReportErrorASCII(cx,
                          "No source buffer set. You must initially "
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
    while (len < buflen) {
      if (currentBuf[len] == '\n') {
        break;
      }
      len++;
    }

    JSString* str = JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(currentBuf, len));
    if (!str) {
      return false;
    }

    if (currentBuf[len] == '\0') {
      sc->readLineBufPos += len;
    } else {
      sc->readLineBufPos += len + 1;
    }

    args.rval().setString(str);
    return true;
  }

  if (args.length() == 1) {
    sc->readLineBuf = nullptr;
    sc->readLineBufPos = 0;

    RootedString str(cx, JS::ToString(cx, args[0]));
    if (!str) {
      return false;
    }
    sc->readLineBuf = JS_EncodeStringToUTF8(cx, str);
    if (!sc->readLineBuf) {
      return false;
    }

    args.rval().setUndefined();
    return true;
  }

  JS_ReportErrorASCII(cx, "Must specify at most one argument");
  return false;
}

static bool PutStr(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() != 0) {
    if (!gOutFile->isOpen()) {
      JS_ReportErrorASCII(cx, "output file is closed");
      return false;
    }

    RootedString str(cx, JS::ToString(cx, args[0]));
    if (!str) {
      return false;
    }
    UniqueChars bytes = JS_EncodeStringToUTF8(cx, str);
    if (!bytes) {
      return false;
    }
    fputs(bytes.get(), gOutFile->fp);
    fflush(gOutFile->fp);
  }

  args.rval().setUndefined();
  return true;
}

static bool Now(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  double now = PRMJ_Now() / double(PRMJ_USEC_PER_MSEC);
  args.rval().setDouble(now);
  return true;
}

static bool CpuNow(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  double now = double(std::clock()) / double(CLOCKS_PER_SEC);
  args.rval().setDouble(now);
  return true;
}

static bool PrintInternal(JSContext* cx, const CallArgs& args, RCFile* file) {
  if (!file->isOpen()) {
    JS_ReportErrorASCII(cx, "output file is closed");
    return false;
  }

  for (unsigned i = 0; i < args.length(); i++) {
    RootedString str(cx, JS::ToString(cx, args[i]));
    if (!str) {
      return false;
    }
    UniqueChars bytes = JS_EncodeStringToUTF8(cx, str);
    if (!bytes) {
      return false;
    }
    fprintf(file->fp, "%s%s", i ? " " : "", bytes.get());
  }

  fputc('\n', file->fp);
  fflush(file->fp);

  args.rval().setUndefined();
  return true;
}

static bool Print(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
#ifdef FUZZING_INTERFACES
  if (fuzzHaveModule && !fuzzDoDebug) {
    // When fuzzing and not debugging, suppress any print() output,
    // as it slows down fuzzing and makes libFuzzer's output hard
    // to read.
    args.rval().setUndefined();
    return true;
  }
#endif  // FUZZING_INTERFACES
  return PrintInternal(cx, args, gOutFile);
}

static bool PrintErr(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return PrintInternal(cx, args, gErrFile);
}

static bool Help(JSContext* cx, unsigned argc, Value* vp);

static bool Quit(JSContext* cx, unsigned argc, Value* vp) {
  ShellContext* sc = GetShellContext(cx);

#ifdef JS_MORE_DETERMINISTIC
  // Print a message to stderr in more-deterministic builds to help jsfunfuzz
  // find uncatchable-exception bugs.
  fprintf(stderr, "quit called\n");
#endif

  CallArgs args = CallArgsFromVp(argc, vp);
  int32_t code;
  if (!ToInt32(cx, args.get(0), &code)) {
    return false;
  }

  // The fuzzers check the shell's exit code and assume a value >= 128 means
  // the process crashed (for instance, SIGSEGV will result in code 139). On
  // POSIX platforms, the exit code is 8-bit and negative values can also
  // result in an exit code >= 128. We restrict the value to range [0, 127] to
  // avoid false positives.
  if (code < 0 || code >= 128) {
    JS_ReportErrorASCII(cx, "quit exit code should be in range 0-127");
    return false;
  }

  js::StopDrainingJobQueue(cx);
  sc->exitCode = code;
  sc->quitting = true;
  return false;
}

static bool StartTimingMutator(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() > 0) {
    JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                              JSSMSG_TOO_MANY_ARGS, "startTimingMutator");
    return false;
  }

  if (!cx->runtime()->gc.stats().startTimingMutator()) {
    JS_ReportErrorASCII(
        cx, "StartTimingMutator should only be called from outside of GC");
    return false;
  }

  args.rval().setUndefined();
  return true;
}

static bool StopTimingMutator(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() > 0) {
    JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                              JSSMSG_TOO_MANY_ARGS, "stopTimingMutator");
    return false;
  }

  double mutator_ms, gc_ms;
  if (!cx->runtime()->gc.stats().stopTimingMutator(mutator_ms, gc_ms)) {
    JS_ReportErrorASCII(cx,
                        "stopTimingMutator called when not timing the mutator");
    return false;
  }
  double total_ms = mutator_ms + gc_ms;
  if (total_ms > 0 && gOutFile->isOpen()) {
    fprintf(gOutFile->fp, "Mutator: %.3fms (%.1f%%), GC: %.3fms (%.1f%%)\n",
            mutator_ms, mutator_ms / total_ms * 100.0, gc_ms,
            gc_ms / total_ms * 100.0);
  }

  args.rval().setUndefined();
  return true;
}

static const char* ToSource(JSContext* cx, HandleValue vp, UniqueChars* bytes) {
  RootedString str(cx, JS_ValueToSource(cx, vp));
  if (str) {
    *bytes = JS_EncodeStringToUTF8(cx, str);
    if (*bytes) {
      return bytes->get();
    }
  }
  JS_ClearPendingException(cx);
  return "<<error converting value to string>>";
}

static bool AssertEq(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!(args.length() == 2 || (args.length() == 3 && args[2].isString()))) {
    JS_ReportErrorNumberASCII(
        cx, my_GetErrorMessage, nullptr,
        (args.length() < 2)
            ? JSSMSG_NOT_ENOUGH_ARGS
            : (args.length() == 3) ? JSSMSG_INVALID_ARGS : JSSMSG_TOO_MANY_ARGS,
        "assertEq");
    return false;
  }

  bool same;
  if (!JS::SameValue(cx, args[0], args[1], &same)) {
    return false;
  }
  if (!same) {
    UniqueChars bytes0, bytes1;
    const char* actual = ToSource(cx, args[0], &bytes0);
    const char* expected = ToSource(cx, args[1], &bytes1);
    if (args.length() == 2) {
      JS_ReportErrorNumberUTF8(cx, my_GetErrorMessage, nullptr,
                               JSSMSG_ASSERT_EQ_FAILED, actual, expected);
    } else {
      RootedString message(cx, args[2].toString());
      UniqueChars bytes2 = QuoteString(cx, message);
      if (!bytes2) {
        return false;
      }
      JS_ReportErrorNumberUTF8(cx, my_GetErrorMessage, nullptr,
                               JSSMSG_ASSERT_EQ_FAILED_MSG, actual, expected,
                               bytes2.get());
    }
    return false;
  }
  args.rval().setUndefined();
  return true;
}

static JSScript* GetTopScript(JSContext* cx) {
  NonBuiltinScriptFrameIter iter(cx);
  return iter.done() ? nullptr : iter.script();
}

static bool GetScriptAndPCArgs(JSContext* cx, CallArgs& args,
                               MutableHandleScript scriptp, int32_t* ip) {
  RootedScript script(cx, GetTopScript(cx));
  *ip = 0;
  if (!args.get(0).isUndefined()) {
    HandleValue v = args[0];
    unsigned intarg = 0;
    if (v.isObject() && JS_GetClass(&v.toObject()) == &JSFunction::class_) {
      script = TestingFunctionArgumentToScript(cx, v);
      if (!script) {
        return false;
      }
      intarg++;
    }
    if (!args.get(intarg).isUndefined()) {
      if (!JS::ToInt32(cx, args[intarg], ip)) {
        return false;
      }
      if ((uint32_t)*ip >= script->length()) {
        JS_ReportErrorASCII(cx, "Invalid PC");
        return false;
      }
    }
  }

  scriptp.set(script);

  return true;
}

static bool LineToPC(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() == 0) {
    JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                              JSSMSG_LINE2PC_USAGE);
    return false;
  }

  RootedScript script(cx, GetTopScript(cx));
  int32_t lineArg = 0;
  if (args[0].isObject() && args[0].toObject().is<JSFunction>()) {
    script = TestingFunctionArgumentToScript(cx, args[0]);
    if (!script) {
      return false;
    }
    lineArg++;
  }

  uint32_t lineno;
  if (!ToUint32(cx, args.get(lineArg), &lineno)) {
    return false;
  }

  jsbytecode* pc = LineNumberToPC(script, lineno);
  if (!pc) {
    return false;
  }
  args.rval().setInt32(script->pcToOffset(pc));
  return true;
}

static bool PCToLine(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedScript script(cx);
  int32_t i;
  unsigned lineno;

  if (!GetScriptAndPCArgs(cx, args, &script, &i)) {
    return false;
  }
  lineno = PCToLineNumber(script, script->offsetToPC(i));
  if (!lineno) {
    return false;
  }
  args.rval().setInt32(lineno);
  return true;
}

#if defined(DEBUG) || defined(JS_JITSPEW)

static MOZ_MUST_USE bool SrcNotes(JSContext* cx, HandleScript script,
                                  Sprinter* sp) {
  if (!sp->put("\nSource notes:\n") ||
      !sp->jsprintf("%4s %4s %5s %6s %-8s %s\n", "ofs", "line", "pc", "delta",
                    "desc", "args") ||
      !sp->put("---- ---- ----- ------ -------- ------\n")) {
    return false;
  }

  unsigned offset = 0;
  unsigned colspan = 0;
  unsigned lineno = script->lineno();
  SrcNote* notes = script->notes();
  for (SrcNoteIterator iter(notes); !iter.atEnd(); ++iter) {
    auto sn = *iter;

    unsigned delta = sn->delta();
    offset += delta;
    SrcNoteType type = sn->type();
    const char* name = sn->name();
    if (!sp->jsprintf("%3u: %4u %5u [%4u] %-8s", unsigned(sn - notes), lineno,
                      offset, delta, name)) {
      return false;
    }

    switch (type) {
      case SrcNoteType::Null:
      case SrcNoteType::AssignOp:
      case SrcNoteType::Breakpoint:
      case SrcNoteType::StepSep:
      case SrcNoteType::XDelta:
        break;

      case SrcNoteType::ColSpan:
        colspan = SrcNote::ColSpan::getSpan(sn);
        if (!sp->jsprintf("%d", colspan)) {
          return false;
        }
        break;

      case SrcNoteType::SetLine:
        lineno = SrcNote::SetLine::getLine(sn);
        if (!sp->jsprintf(" lineno %u", lineno)) {
          return false;
        }
        break;

      case SrcNoteType::NewLine:
        ++lineno;
        break;

      default:
        MOZ_ASSERT_UNREACHABLE("unrecognized srcnote");
    }
    if (!sp->put("\n")) {
      return false;
    }
  }

  return true;
}

static bool Notes(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  Sprinter sprinter(cx);
  if (!sprinter.init()) {
    return false;
  }

  for (unsigned i = 0; i < args.length(); i++) {
    RootedScript script(cx, TestingFunctionArgumentToScript(cx, args[i]));
    if (!script) {
      return false;
    }

    if (!SrcNotes(cx, script, &sprinter)) {
      return false;
    }
  }

  JSString* str = JS_NewStringCopyZ(cx, sprinter.string());
  if (!str) {
    return false;
  }
  args.rval().setString(str);
  return true;
}

static const char* TryNoteName(TryNoteKind kind) {
  switch (kind) {
    case TryNoteKind::Catch:
      return "catch";
    case TryNoteKind::Finally:
      return "finally";
    case TryNoteKind::ForIn:
      return "for-in";
    case TryNoteKind::ForOf:
      return "for-of";
    case TryNoteKind::Loop:
      return "loop";
    case TryNoteKind::ForOfIterClose:
      return "for-of-iterclose";
    case TryNoteKind::Destructuring:
      return "destructuring";
  }

  MOZ_CRASH("Bad TryNoteKind");
}

static MOZ_MUST_USE bool TryNotes(JSContext* cx, HandleScript script,
                                  Sprinter* sp) {
  if (!sp->put(
          "\nException table:\nkind               stack    start      end\n")) {
    return false;
  }

  for (const TryNote& tn : script->trynotes()) {
    if (!sp->jsprintf(" %-16s %6u %8u %8u\n", TryNoteName(tn.kind()),
                      tn.stackDepth, tn.start, tn.start + tn.length)) {
      return false;
    }
  }
  return true;
}

static MOZ_MUST_USE bool ScopeNotes(JSContext* cx, HandleScript script,
                                    Sprinter* sp) {
  if (!sp->put("\nScope notes:\n   index   parent    start      end\n")) {
    return false;
  }

  for (const ScopeNote& note : script->scopeNotes()) {
    if (note.index == ScopeNote::NoScopeIndex) {
      if (!sp->jsprintf("%8s ", "(none)")) {
        return false;
      }
    } else {
      if (!sp->jsprintf("%8u ", note.index)) {
        return false;
      }
    }
    if (note.parent == ScopeNote::NoScopeIndex) {
      if (!sp->jsprintf("%8s ", "(none)")) {
        return false;
      }
    } else {
      if (!sp->jsprintf("%8u ", note.parent)) {
        return false;
      }
    }
    if (!sp->jsprintf("%8u %8u\n", note.start, note.start + note.length)) {
      return false;
    }
  }
  return true;
}

static MOZ_MUST_USE bool GCThings(JSContext* cx, HandleScript script,
                                  Sprinter* sp) {
  if (!sp->put("\nGC things:\n   index   type       value\n")) {
    return false;
  }

  size_t i = 0;
  for (JS::GCCellPtr gcThing : script->gcthings()) {
    if (!sp->jsprintf("%8zu   ", i)) {
      return false;
    }
    if (gcThing.is<BigInt>()) {
      if (!sp->put("BigInt     ")) {
        return false;
      }
      gcThing.as<BigInt>().dump(*sp);
      if (!sp->put("\n")) {
        return false;
      }
    } else if (gcThing.is<Scope>()) {
      if (!sp->put("Scope      ")) {
        return false;
      }
      Rooted<Scope*> scope(cx, &gcThing.as<Scope>());
      if (!Scope::dumpForDisassemble(cx, scope, *sp,
                                     "                      ")) {
        return false;
      }
      if (!sp->put("\n")) {
        return false;
      }
    } else if (gcThing.is<JSObject>()) {
      JSObject* obj = &gcThing.as<JSObject>();
      if (obj->is<JSFunction>()) {
        if (!sp->put("Function   ")) {
          return false;
        }
        RootedFunction fun(cx, &obj->as<JSFunction>());
        if (fun->displayAtom()) {
          Rooted<JSAtom*> name(cx, fun->displayAtom());
          UniqueChars utf8chars = JS_EncodeStringToUTF8(cx, name);
          if (!utf8chars) {
            return false;
          }
          if (!sp->put(utf8chars.get())) {
            return false;
          }
        } else {
          if (!sp->put("(anonymous)")) {
            return false;
          }
        }

        if (fun->hasBaseScript()) {
          BaseScript* script = fun->baseScript();
          if (!sp->jsprintf(" @ %u:%u\n", script->lineno(), script->column())) {
            return false;
          }
        } else {
          if (!sp->put(" (no script)\n")) {
            return false;
          }
        }
      } else {
        if (obj->is<RegExpObject>()) {
          if (!sp->put("RegExp     ")) {
            return false;
          }
        } else {
          if (!sp->put("Object     ")) {
            return false;
          }
        }

        RootedValue objValue(cx, ObjectValue(*obj));
        RootedString str(cx, ValueToSource(cx, objValue));
        if (!str) {
          return false;
        }
        UniqueChars utf8chars = JS_EncodeStringToUTF8(cx, str);
        if (!utf8chars) {
          return false;
        }
        if (!sp->put(utf8chars.get())) {
          return false;
        }

        if (!sp->put("\n")) {
          return false;
        }
      }
    } else {
      if (!sp->put("Unknown\n")) {
        return false;
      }
    }
    i++;
  }

  return true;
}

static MOZ_MUST_USE bool DisassembleScript(JSContext* cx, HandleScript script,
                                           HandleFunction fun, bool lines,
                                           bool recursive, bool sourceNotes,
                                           bool gcThings, Sprinter* sp) {
  if (fun) {
    if (!sp->put("flags:")) {
      return false;
    }
    if (fun->isLambda()) {
      if (!sp->put(" LAMBDA")) {
        return false;
      }
    }
    if (fun->needsCallObject()) {
      if (!sp->put(" NEEDS_CALLOBJECT")) {
        return false;
      }
    }
    if (fun->needsExtraBodyVarEnvironment()) {
      if (!sp->put(" NEEDS_EXTRABODYVARENV")) {
        return false;
      }
    }
    if (fun->needsNamedLambdaEnvironment()) {
      if (!sp->put(" NEEDS_NAMEDLAMBDAENV")) {
        return false;
      }
    }
    if (fun->isConstructor()) {
      if (!sp->put(" CONSTRUCTOR")) {
        return false;
      }
    }
    if (fun->isSelfHostedBuiltin()) {
      if (!sp->put(" SELF_HOSTED")) {
        return false;
      }
    }
    if (fun->isArrow()) {
      if (!sp->put(" ARROW")) {
        return false;
      }
    }
    if (!sp->put("\n")) {
      return false;
    }
  }

  if (!Disassemble(cx, script, lines, sp)) {
    return false;
  }
  if (sourceNotes) {
    if (!SrcNotes(cx, script, sp)) {
      return false;
    }
  }
  if (!TryNotes(cx, script, sp)) {
    return false;
  }
  if (!ScopeNotes(cx, script, sp)) {
    return false;
  }
  if (gcThings) {
    if (!GCThings(cx, script, sp)) {
      return false;
    }
  }

  if (recursive) {
    for (JS::GCCellPtr gcThing : script->gcthings()) {
      if (!gcThing.is<JSObject>()) {
        continue;
      }

      JSObject* obj = &gcThing.as<JSObject>();
      if (obj->is<JSFunction>()) {
        if (!sp->put("\n")) {
          return false;
        }

        RootedFunction fun(cx, &obj->as<JSFunction>());
        if (fun->isInterpreted()) {
          RootedScript script(cx, JSFunction::getOrCreateScript(cx, fun));
          if (script) {
            if (!DisassembleScript(cx, script, fun, lines, recursive,
                                   sourceNotes, gcThings, sp)) {
              return false;
            }
          }
        } else {
          if (!sp->put("[native code]\n")) {
            return false;
          }
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
  bool gcThings;

  DisassembleOptionParser(unsigned argc, Value* argv)
      : argc(argc),
        argv(argv),
        lines(false),
        recursive(false),
        sourceNotes(true),
        gcThings(false) {}

  bool parse(JSContext* cx) {
    /* Read options off early arguments */
    while (argc > 0 && argv[0].isString()) {
      JSString* str = argv[0].toString();
      JSLinearString* linearStr = JS_EnsureLinearString(cx, str);
      if (!linearStr) {
        return false;
      }
      if (JS_LinearStringEqualsLiteral(linearStr, "-l")) {
        lines = true;
      } else if (JS_LinearStringEqualsLiteral(linearStr, "-r")) {
        recursive = true;
      } else if (JS_LinearStringEqualsLiteral(linearStr, "-S")) {
        sourceNotes = false;
      } else if (JS_LinearStringEqualsLiteral(linearStr, "-g")) {
        gcThings = true;
      } else {
        break;
      }
      argv++;
      argc--;
    }
    return true;
  }
};

} /* anonymous namespace */

static bool DisassembleToSprinter(JSContext* cx, unsigned argc, Value* vp,
                                  Sprinter* sprinter) {
  CallArgs args = CallArgsFromVp(argc, vp);
  DisassembleOptionParser p(args.length(), args.array());
  if (!p.parse(cx)) {
    return false;
  }

  if (p.argc == 0) {
    /* Without arguments, disassemble the current script. */
    RootedScript script(cx, GetTopScript(cx));
    if (script) {
      JSAutoRealm ar(cx, script);
      if (!Disassemble(cx, script, p.lines, sprinter)) {
        return false;
      }
      if (!SrcNotes(cx, script, sprinter)) {
        return false;
      }
      if (!TryNotes(cx, script, sprinter)) {
        return false;
      }
      if (!ScopeNotes(cx, script, sprinter)) {
        return false;
      }
      if (p.gcThings) {
        if (!GCThings(cx, script, sprinter)) {
          return false;
        }
      }
    }
  } else {
    for (unsigned i = 0; i < p.argc; i++) {
      RootedFunction fun(cx);
      RootedScript script(cx);
      RootedValue value(cx, p.argv[i]);
      if (value.isObject() && value.toObject().is<ModuleObject>()) {
        script = value.toObject().as<ModuleObject>().maybeScript();
      } else {
        script = TestingFunctionArgumentToScript(cx, value, fun.address());
      }
      if (!script) {
        return false;
      }
      if (!DisassembleScript(cx, script, fun, p.lines, p.recursive,
                             p.sourceNotes, p.gcThings, sprinter)) {
        return false;
      }
    }
  }

  return !sprinter->hadOutOfMemory();
}

static bool DisassembleToString(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  Sprinter sprinter(cx);
  if (!sprinter.init()) {
    return false;
  }
  if (!DisassembleToSprinter(cx, args.length(), vp, &sprinter)) {
    return false;
  }

  JS::ConstUTF8CharsZ utf8chars(sprinter.string(), strlen(sprinter.string()));
  JSString* str = JS_NewStringCopyUTF8Z(cx, utf8chars);
  if (!str) {
    return false;
  }
  args.rval().setString(str);
  return true;
}

static bool Disassemble(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!gOutFile->isOpen()) {
    JS_ReportErrorASCII(cx, "output file is closed");
    return false;
  }

  Sprinter sprinter(cx);
  if (!sprinter.init()) {
    return false;
  }
  if (!DisassembleToSprinter(cx, args.length(), vp, &sprinter)) {
    return false;
  }

  fprintf(gOutFile->fp, "%s\n", sprinter.string());
  args.rval().setUndefined();
  return true;
}

static bool DisassFile(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!gOutFile->isOpen()) {
    JS_ReportErrorASCII(cx, "output file is closed");
    return false;
  }

  /* Support extra options at the start, just like Disassemble. */
  DisassembleOptionParser p(args.length(), args.array());
  if (!p.parse(cx)) {
    return false;
  }

  if (!p.argc) {
    args.rval().setUndefined();
    return true;
  }

  // We should change DisassembleOptionParser to store CallArgs.
  JSString* str = JS::ToString(cx, HandleValue::fromMarkedLocation(&p.argv[0]));
  if (!str) {
    return false;
  }
  UniqueChars filename = JS_EncodeStringToLatin1(cx, str);
  if (!filename) {
    return false;
  }
  RootedScript script(cx);

  {
    CompileOptions options(cx);
    options.setIntroductionType("js shell disFile")
        .setFileAndLine(filename.get(), 1)
        .setIsRunOnce(true)
        .setNoScriptRval(true);

    script = JS::CompileUtf8Path(cx, options, filename.get());
    if (!script) {
      return false;
    }
  }

  Sprinter sprinter(cx);
  if (!sprinter.init()) {
    return false;
  }
  if (!DisassembleScript(cx, script, nullptr, p.lines, p.recursive,
                         p.sourceNotes, p.gcThings, &sprinter)) {
    return false;
  }

  fprintf(gOutFile->fp, "%s\n", sprinter.string());

  args.rval().setUndefined();
  return true;
}

static bool DisassWithSrc(JSContext* cx, unsigned argc, Value* vp) {
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
    script = TestingFunctionArgumentToScript(cx, args[i]);
    if (!script) {
      return false;
    }

    if (!script->filename()) {
      JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                                JSSMSG_FILE_SCRIPTS_ONLY);
      return false;
    }

    FILE* file = fopen(script->filename(), "rb");
    if (!file) {
      /* FIXME: script->filename() should become UTF-8 (bug 987069). */
      ReportCantOpenErrorUnknownEncoding(cx, script->filename());
      return false;
    }
    auto closeFile = MakeScopeExit([file] { fclose(file); });

    jsbytecode* pc = script->code();
    jsbytecode* end = script->codeEnd();

    Sprinter sprinter(cx);
    if (!sprinter.init()) {
      return false;
    }

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
          if (!sprinter.jsprintf("%s %3u: BACKUP\n", sep, line2)) {
            return false;
          }
        }
      } else {
        if (bupline && line1 == line2) {
          if (!sprinter.jsprintf("%s %3u: RESTORE\n", sep, line2)) {
            return false;
          }
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
          if (!sprinter.jsprintf("%s %3u: %s", sep, line1, linebuf)) {
            return false;
          }
        }
      }

      len =
          Disassemble1(cx, script, pc, script->pcToOffset(pc), true, &sprinter);
      if (!len) {
        return false;
      }

      pc += len;
    }

    fprintf(gOutFile->fp, "%s\n", sprinter.string());
  }

  args.rval().setUndefined();
  return true;
}

#endif /* defined(DEBUG) || defined(JS_JITSPEW) */

/* Pretend we can always preserve wrappers for dummy DOM objects. */
static bool DummyPreserveWrapperCallback(JSContext* cx, HandleObject obj) {
  return true;
}

static bool Intern(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  JSString* str = JS::ToString(cx, args.get(0));
  if (!str) {
    return false;
  }

  AutoStableStringChars strChars(cx);
  if (!strChars.initTwoByte(cx, str)) {
    return false;
  }

  mozilla::Range<const char16_t> chars = strChars.twoByteRange();

  if (!JS_AtomizeAndPinUCStringN(cx, chars.begin().get(), chars.length())) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

static bool Crash(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() == 0) {
    MOZ_CRASH("forced crash");
  }
  RootedString message(cx, JS::ToString(cx, args[0]));
  if (!message) {
    return false;
  }
  UniqueChars utf8chars = JS_EncodeStringToUTF8(cx, message);
  if (!utf8chars) {
    return false;
  }
  if (args.get(1).isObject()) {
    RootedValue v(cx);
    RootedObject opts(cx, &args[1].toObject());
    if (!JS_GetProperty(cx, opts, "suppress_minidump", &v)) {
      return false;
    }
    if (v.isBoolean() && v.toBoolean()) {
      js::NoteIntentionalCrash();
    }
  }
#ifndef DEBUG
  MOZ_ReportCrash(utf8chars.get(), __FILE__, __LINE__);
#endif
  MOZ_CRASH_UNSAFE(utf8chars.get());
}

static bool GetSLX(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedScript script(cx);

  script = TestingFunctionArgumentToScript(cx, args.get(0));
  if (!script) {
    return false;
  }
  args.rval().setInt32(GetScriptLineExtent(script));
  return true;
}

static bool ThrowError(JSContext* cx, unsigned argc, Value* vp) {
  JS_ReportErrorASCII(cx, "This is an error");
  return false;
}

static bool CopyErrorReportToObject(JSContext* cx, JSErrorReport* report,
                                    HandleObject obj) {
  RootedString nameStr(cx);
  if (report->exnType == JSEXN_WARN) {
    nameStr = JS_NewStringCopyZ(cx, "Warning");
    if (!nameStr) {
      return false;
    }
  } else {
    nameStr = GetErrorTypeName(cx, report->exnType);
    // GetErrorTypeName doesn't set an exception, but
    // can fail for InternalError or non-error objects.
    if (!nameStr) {
      nameStr = cx->runtime()->emptyString;
    }
  }
  RootedValue nameVal(cx, StringValue(nameStr));
  if (!DefineDataProperty(cx, obj, cx->names().name, nameVal)) {
    return false;
  }

  RootedString messageStr(cx, report->newMessageString(cx));
  if (!messageStr) {
    return false;
  }
  RootedValue messageVal(cx, StringValue(messageStr));
  if (!DefineDataProperty(cx, obj, cx->names().message, messageVal)) {
    return false;
  }

  RootedValue linenoVal(cx, Int32Value(report->lineno));
  if (!DefineDataProperty(cx, obj, cx->names().lineNumber, linenoVal)) {
    return false;
  }

  RootedValue columnVal(cx, Int32Value(report->column));
  if (!DefineDataProperty(cx, obj, cx->names().columnNumber, columnVal)) {
    return false;
  }

  RootedObject notesArray(cx, CreateErrorNotesArray(cx, report));
  if (!notesArray) {
    return false;
  }

  RootedValue notesArrayVal(cx, ObjectValue(*notesArray));
  return DefineDataProperty(cx, obj, cx->names().notes, notesArrayVal);
}

static bool CreateErrorReport(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  js::ErrorReport report(cx);
  if (!report.init(cx, args.get(0), js::ErrorReport::WithSideEffects)) {
    return false;
  }

  MOZ_ASSERT(!report.report()->isWarning());

  RootedObject obj(cx, JS_NewPlainObject(cx));
  if (!obj) {
    return false;
  }

  RootedString toString(cx,
                        NewStringCopyUTF8Z<CanGC>(cx, report.toStringResult()));
  if (!toString) {
    return false;
  }

  if (!JS_DefineProperty(cx, obj, "toStringResult", toString,
                         JSPROP_ENUMERATE)) {
    return false;
  }

  if (!CopyErrorReportToObject(cx, report.report(), obj)) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

#define LAZY_STANDARD_CLASSES

/* A class for easily testing the inner/outer object callbacks. */
typedef struct ComplexObject {
  bool isInner;
  bool frozen;
  JSObject* inner;
  JSObject* outer;
} ComplexObject;

static bool sandbox_enumerate(JSContext* cx, JS::HandleObject obj,
                              JS::MutableHandleIdVector properties,
                              bool enumerableOnly) {
  RootedValue v(cx);

  if (!JS_GetProperty(cx, obj, "lazy", &v)) {
    return false;
  }

  if (!ToBoolean(v)) {
    return true;
  }

  return JS_NewEnumerateStandardClasses(cx, obj, properties, enumerableOnly);
}

static bool sandbox_resolve(JSContext* cx, HandleObject obj, HandleId id,
                            bool* resolvedp) {
  RootedValue v(cx);
  if (!JS_GetProperty(cx, obj, "lazy", &v)) {
    return false;
  }

  if (ToBoolean(v)) {
    return JS_ResolveStandardClass(cx, obj, id, resolvedp);
  }
  return true;
}

static const JSClassOps sandbox_classOps = {
    nullptr,                   // addProperty
    nullptr,                   // delProperty
    nullptr,                   // enumerate
    sandbox_enumerate,         // newEnumerate
    sandbox_resolve,           // resolve
    nullptr,                   // mayResolve
    nullptr,                   // finalize
    nullptr,                   // call
    nullptr,                   // hasInstance
    nullptr,                   // construct
    JS_GlobalObjectTraceHook,  // trace
};

static const JSClass sandbox_class = {"sandbox", JSCLASS_GLOBAL_FLAGS,
                                      &sandbox_classOps};

static void SetStandardRealmOptions(JS::RealmOptions& options) {
  options.creationOptions()
      .setSharedMemoryAndAtomicsEnabled(enableSharedMemory)
      .setCoopAndCoepEnabled(false)
      .setStreamsEnabled(enableStreams)
      .setReadableByteStreamsEnabled(enableReadableByteStreams)
      .setBYOBStreamReadersEnabled(enableBYOBStreamReaders)
      .setWritableStreamsEnabled(enableWritableStreams)
      .setReadableStreamPipeToEnabled(enableReadableStreamPipeTo)
      .setWeakRefsEnabled(enableWeakRefs)
      .setToSourceEnabled(enableToSource)
      .setPropertyErrorMessageFixEnabled(enablePropertyErrorMessageFix);
}

static MOZ_MUST_USE bool CheckRealmOptions(JSContext* cx,
                                           JS::RealmOptions& options,
                                           JSPrincipals* principals) {
  JS::RealmCreationOptions& creationOptions = options.creationOptions();
  if (creationOptions.compartmentSpecifier() !=
      JS::CompartmentSpecifier::ExistingCompartment) {
    return true;
  }

  JS::Compartment* comp = creationOptions.compartment();

  // All realms in a compartment must be either system or non-system.
  bool isSystem =
      principals && principals == cx->runtime()->trustedPrincipals();
  if (isSystem != IsSystemCompartment(comp)) {
    JS_ReportErrorASCII(cx,
                        "Cannot create system and non-system realms in the "
                        "same compartment");
    return false;
  }

  // Debugger visibility is per-compartment, not per-realm, so make sure the
  // requested visibility matches the existing compartment's.
  if (creationOptions.invisibleToDebugger() != comp->invisibleToDebugger()) {
    JS_ReportErrorASCII(cx,
                        "All the realms in a compartment must have "
                        "the same debugger visibility");
    return false;
  }

  return true;
}

static JSObject* NewSandbox(JSContext* cx, bool lazy) {
  JS::RealmOptions options;
  SetStandardRealmOptions(options);

  if (defaultToSameCompartment) {
    options.creationOptions().setExistingCompartment(cx->global());
  } else {
    options.creationOptions().setNewCompartmentAndZone();
  }

  JSPrincipals* principals = nullptr;
  if (!CheckRealmOptions(cx, options, principals)) {
    return nullptr;
  }

  RootedObject obj(cx,
                   JS_NewGlobalObject(cx, &sandbox_class, principals,
                                      JS::DontFireOnNewGlobalHook, options));
  if (!obj) {
    return nullptr;
  }

  {
    JSAutoRealm ar(cx, obj);
    if (!lazy && !JS::InitRealmStandardClasses(cx)) {
      return nullptr;
    }

    RootedValue value(cx, BooleanValue(lazy));
    if (!JS_DefineProperty(cx, obj, "lazy", value,
                           JSPROP_PERMANENT | JSPROP_READONLY)) {
      return nullptr;
    }

    JS_FireOnNewGlobalObject(cx, obj);
  }

  if (!cx->compartment()->wrap(cx, &obj)) {
    return nullptr;
  }
  return obj;
}

static bool EvalInContext(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "evalcx", 1)) {
    return false;
  }

  RootedString str(cx, ToString(cx, args[0]));
  if (!str) {
    return false;
  }

  RootedObject sobj(cx);
  if (args.hasDefined(1)) {
    sobj = ToObject(cx, args[1]);
    if (!sobj) {
      return false;
    }
  }

  AutoStableStringChars strChars(cx);
  if (!strChars.initTwoByte(cx, str)) {
    return false;
  }

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
    if (!sobj) {
      return false;
    }
  }

  if (srclen == 0) {
    args.rval().setObject(*sobj);
    return true;
  }

  JS::AutoFilename filename;
  unsigned lineno;

  DescribeScriptedCaller(cx, &filename, &lineno);
  {
    sobj = UncheckedUnwrap(sobj, true);

    JSAutoRealm ar(cx, sobj);

    sobj = ToWindowIfWindowProxy(sobj);

    if (!JS_IsGlobalObject(sobj)) {
      JS_ReportErrorASCII(cx, "Invalid scope argument to evalcx");
      return false;
    }

    JS::CompileOptions opts(cx);
    opts.setFileAndLine(filename.get(), lineno);

    JS::SourceText<char16_t> srcBuf;
    if (!srcBuf.init(cx, src, srclen, JS::SourceOwnership::Borrowed) ||
        !JS::Evaluate(cx, opts, srcBuf, args.rval())) {
      return false;
    }
  }

  if (!cx->compartment()->wrap(cx, args.rval())) {
    return false;
  }

  return true;
}

static bool EnsureGeckoProfilingStackInstalled(JSContext* cx,
                                               ShellContext* sc) {
  if (cx->geckoProfiler().infraInstalled()) {
    MOZ_ASSERT(sc->geckoProfilingStack);
    return true;
  }

  MOZ_ASSERT(!sc->geckoProfilingStack);
  sc->geckoProfilingStack = MakeUnique<ProfilingStack>();
  if (!sc->geckoProfilingStack) {
    JS_ReportOutOfMemory(cx);
    return false;
  }

  SetContextProfilingStack(cx, sc->geckoProfilingStack.get());
  return true;
}

struct WorkerInput {
  JSRuntime* parentRuntime;
  UniqueTwoByteChars chars;
  size_t length;

  WorkerInput(JSRuntime* parentRuntime, UniqueTwoByteChars chars, size_t length)
      : parentRuntime(parentRuntime), chars(std::move(chars)), length(length) {}

  ~WorkerInput() = default;
};

static void DestroyShellCompartmentPrivate(JSFreeOp* fop,
                                           JS::Compartment* compartment) {
  auto priv = static_cast<ShellCompartmentPrivate*>(
      JS_GetCompartmentPrivate(compartment));
  js_delete(priv);
}

static void SetWorkerContextOptions(JSContext* cx);
static bool ShellBuildId(JS::BuildIdCharVector* buildId);

static void WorkerMain(WorkerInput* input) {
  MOZ_ASSERT(input->parentRuntime);

  JSContext* cx = JS_NewContext(8L * 1024L * 1024L, input->parentRuntime);
  if (!cx) {
    return;
  }

  ShellContext* sc = js_new<ShellContext>(cx);
  if (!sc) {
    return;
  }

  auto guard = mozilla::MakeScopeExit([&] {
    CancelOffThreadJobsForContext(cx);
    sc->markObservers.reset();
    JS_SetContextPrivate(cx, nullptr);
    js_delete(sc);
    JS_DestroyContext(cx);
    js_delete(input);
  });

  sc->isWorker = true;
  JS_SetContextPrivate(cx, sc);
  JS_SetGrayGCRootsTracer(cx, TraceGrayRoots, nullptr);
  SetWorkerContextOptions(cx);

  JS_SetFutexCanWait(cx);
  JS::SetWarningReporter(cx, WarningReporter);
  js::SetPreserveWrapperCallback(cx, DummyPreserveWrapperCallback);
  JS_InitDestroyPrincipalsCallback(cx, ShellPrincipals::destroy);
  JS_SetDestroyCompartmentCallback(cx, DestroyShellCompartmentPrivate);

  js::SetWindowProxyClass(cx, &ShellWindowProxyClass);

  js::UseInternalJobQueues(cx);

  JS::SetHostCleanupFinalizationRegistryCallback(
      cx, ShellCleanupFinalizationRegistryCallback, sc);

  if (!JS::InitSelfHostedCode(cx)) {
    return;
  }

  EnvironmentPreparer environmentPreparer(cx);

  do {
    JS::RealmOptions realmOptions;
    SetStandardRealmOptions(realmOptions);

    RootedObject global(cx, NewGlobalObject(cx, realmOptions, nullptr,
                                            ShellGlobalKind::WindowProxy));
    if (!global) {
      break;
    }

    JSAutoRealm ar(cx, global);

    JS::CompileOptions options(cx);
    options.setFileAndLine("<string>", 1).setIsRunOnce(true);

    AutoReportException are(cx);
    JS::SourceText<char16_t> srcBuf;
    if (!srcBuf.init(cx, input->chars.get(), input->length,
                     JS::SourceOwnership::Borrowed)) {
      break;
    }

    RootedScript script(cx, JS::Compile(cx, options, srcBuf));
    if (!script) {
      break;
    }
    RootedValue result(cx);
    JS_ExecuteScript(cx, script, &result);
  } while (0);

  KillWatchdog(cx);
  JS_SetGrayGCRootsTracer(cx, nullptr, nullptr);
}

// Workers can spawn other workers, so we need a lock to access workerThreads.
static Mutex* workerThreadsLock = nullptr;
static Vector<js::Thread*, 0, SystemAllocPolicy> workerThreads;

class MOZ_RAII AutoLockWorkerThreads : public LockGuard<Mutex> {
  using Base = LockGuard<Mutex>;

 public:
  AutoLockWorkerThreads() : Base(*workerThreadsLock) {
    MOZ_ASSERT(workerThreadsLock);
  }
};

static bool EvalInWorker(JSContext* cx, unsigned argc, Value* vp) {
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
    JS_ReportErrorASCII(
        cx, "Can't create threads while running simulated OOM test");
    return false;
  }
#endif

  if (!args[0].toString()->ensureLinear(cx)) {
    return false;
  }

  if (!workerThreadsLock) {
    workerThreadsLock = js_new<Mutex>(mutexid::ShellWorkerThreads);
    if (!workerThreadsLock) {
      ReportOutOfMemory(cx);
      return false;
    }
  }

  JSLinearString* str = &args[0].toString()->asLinear();

  UniqueTwoByteChars chars(js_pod_malloc<char16_t>(str->length()));
  if (!chars) {
    ReportOutOfMemory(cx);
    return false;
  }

  CopyChars(chars.get(), *str);

  WorkerInput* input = js_new<WorkerInput>(JS_GetParentRuntime(cx),
                                           std::move(chars), str->length());
  if (!input) {
    ReportOutOfMemory(cx);
    return false;
  }

  Thread* thread;
  {
    AutoEnterOOMUnsafeRegion oomUnsafe;
    thread = js_new<Thread>(
        Thread::Options().setStackSize(gMaxStackSize + 256 * 1024));
    if (!thread || !thread->init(WorkerMain, input)) {
      oomUnsafe.crash("EvalInWorker");
    }
  }

  AutoLockWorkerThreads alwt;
  if (!workerThreads.append(thread)) {
    ReportOutOfMemory(cx);
    thread->join();
    js_delete(thread);
    return false;
  }

  args.rval().setUndefined();
  return true;
}

static bool ShapeOf(JSContext* cx, unsigned argc, JS::Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.get(0).isObject()) {
    JS_ReportErrorASCII(cx, "shapeOf: object expected");
    return false;
  }
  JSObject* obj = &args[0].toObject();
  args.rval().set(JS_NumberValue(double(uintptr_t(obj->shape()) >> 3)));
  return true;
}

static bool GroupOf(JSContext* cx, unsigned argc, JS::Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.get(0).isObject()) {
    JS_ReportErrorASCII(cx, "groupOf: object expected");
    return false;
  }
  RootedObject obj(cx, &args[0].toObject());
  ObjectGroup* group = JSObject::getGroup(cx, obj);
  if (!group) {
    return false;
  }
  args.rval().set(JS_NumberValue(double(uintptr_t(group) >> 3)));
  return true;
}

static bool UnwrappedObjectsHaveSameShape(JSContext* cx, unsigned argc,
                                          JS::Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.get(0).isObject() || !args.get(1).isObject()) {
    JS_ReportErrorASCII(cx, "2 objects expected");
    return false;
  }

  RootedObject obj1(cx, UncheckedUnwrap(&args[0].toObject()));
  RootedObject obj2(cx, UncheckedUnwrap(&args[1].toObject()));

  args.rval().setBoolean(obj1->shape() == obj2->shape());
  return true;
}

static bool Sleep_fn(JSContext* cx, unsigned argc, Value* vp) {
  ShellContext* sc = GetShellContext(cx);
  CallArgs args = CallArgsFromVp(argc, vp);

  TimeDuration duration = TimeDuration::FromSeconds(0.0);
  if (args.length() > 0) {
    double t_secs;
    if (!ToNumber(cx, args[0], &t_secs)) {
      return false;
    }
    if (mozilla::IsNaN(t_secs)) {
      JS_ReportErrorASCII(cx, "sleep interval is not a number");
      return false;
    }

    duration = TimeDuration::FromSeconds(std::max(0.0, t_secs));
    const TimeDuration MAX_TIMEOUT_INTERVAL =
        TimeDuration::FromSeconds(MAX_TIMEOUT_SECONDS);
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
      if (sc->serviceInterrupt) {
        break;
      }
      auto now = TimeStamp::Now();
      if (now >= toWakeup) {
        break;
      }
      duration = toWakeup - now;
    }
  }
  args.rval().setUndefined();
  return !sc->serviceInterrupt;
}

static void KillWatchdog(JSContext* cx) {
  ShellContext* sc = GetShellContext(cx);
  Maybe<Thread> thread;

  {
    LockGuard<Mutex> guard(sc->watchdogLock);
    std::swap(sc->watchdogThread, thread);
    if (thread) {
      // The watchdog thread becoming Nothing is its signal to exit.
      sc->watchdogWakeup.notify_one();
    }
  }
  if (thread) {
    thread->join();
  }

  MOZ_ASSERT(!sc->watchdogThread);
}

static void WatchdogMain(JSContext* cx) {
  ThisThread::SetName("JS Watchdog");

  ShellContext* sc = GetShellContext(cx);

  {
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
}

static bool ScheduleWatchdog(JSContext* cx, double t) {
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
    if (!sc->watchdogThread->init(WatchdogMain, cx)) {
      oomUnsafe.crash("watchdogThread.init");
    }
  } else if (!sc->watchdogTimeout || timeout < sc->watchdogTimeout.value()) {
    sc->watchdogWakeup.notify_one();
  }
  sc->watchdogTimeout = Some(timeout);
  return true;
}

static void KillWorkerThreads(JSContext* cx) {
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
      if (workerThreads.empty()) {
        break;
      }
      thread = workerThreads.popCopy();
    }
    thread->join();
    js_delete(thread);
  }

  workerThreads.clearAndFree();

  js_delete(workerThreadsLock);
  workerThreadsLock = nullptr;
}

static void CancelExecution(JSContext* cx) {
  ShellContext* sc = GetShellContext(cx);
  sc->serviceInterrupt = true;
  JS_RequestInterruptCallback(cx);
}

static bool SetTimeoutValue(JSContext* cx, double t) {
  if (mozilla::IsNaN(t)) {
    JS_ReportErrorASCII(cx, "timeout is not a number");
    return false;
  }
  const TimeDuration MAX_TIMEOUT_INTERVAL =
      TimeDuration::FromSeconds(MAX_TIMEOUT_SECONDS);
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

static bool Timeout(JSContext* cx, unsigned argc, Value* vp) {
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
  if (!ToNumber(cx, args[0], &t)) {
    return false;
  }

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

static bool InterruptIf(JSContext* cx, unsigned argc, Value* vp) {
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

static bool InvokeInterruptCallbackWrapper(JSContext* cx, unsigned argc,
                                           Value* vp) {
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
  if (!interruptRv && !cx->isExceptionPending() &&
      !cx->isPropagatingForcedReturn()) {
    return false;
  }

  JS::AutoSaveExceptionState savedExc(cx);

  FixedInvokeArgs<1> iargs(cx);

  iargs[0].setBoolean(interruptRv);

  RootedValue rv(cx);
  if (!js::Call(cx, args[0], UndefinedHandleValue, iargs, &rv)) {
    return false;
  }

  args.rval().setUndefined();
  return interruptRv;
}

static bool SetInterruptCallback(JSContext* cx, unsigned argc, Value* vp) {
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

static bool SetJitCompilerOption(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject callee(cx, &args.callee());

  if (args.length() != 2) {
    ReportUsageErrorASCII(cx, callee, "Wrong number of arguments.");
    return false;
  }

  if (!args[0].isString()) {
    ReportUsageErrorASCII(cx, callee, "First argument must be a String.");
    return false;
  }

  if (!args[1].isInt32()) {
    ReportUsageErrorASCII(cx, callee, "Second argument must be an Int32.");
    return false;
  }

  // Disallow setting JIT options when there are worker threads, to avoid
  // races.
  if (workerThreadsLock) {
    ReportUsageErrorASCII(
        cx, callee, "Can't set JIT options when there are worker threads.");
    return false;
  }

  JSLinearString* strArg = JS_EnsureLinearString(cx, args[0].toString());
  if (!strArg) {
    return false;
  }

#define JIT_COMPILER_MATCH(key, string)                        \
  else if (JS_LinearStringEqualsLiteral(strArg, string)) opt = \
      JSJITCOMPILER_##key;

  JSJitCompilerOption opt = JSJITCOMPILER_NOT_AN_OPTION;
  if (false) {
  }
  JIT_COMPILER_OPTIONS(JIT_COMPILER_MATCH);
#undef JIT_COMPILER_MATCH

  if (opt == JSJITCOMPILER_NOT_AN_OPTION) {
    ReportUsageErrorASCII(
        cx, callee,
        "First argument does not name a valid option (see jsapi.h).");
    return false;
  }

  int32_t number = args[1].toInt32();
  if (number < 0) {
    number = -1;
  }

  // Disallow enabling or disabling the Baseline Interpreter at runtime.
  // Enabling is a problem because the Baseline Interpreter code is only
  // present if the interpreter was enabled when the JitRuntime was created.
  // To support disabling we would have to discard all JitScripts. Furthermore,
  // we really want JitOptions to be immutable after startup so it's better to
  // use shell flags.
  if (opt == JSJITCOMPILER_BASELINE_INTERPRETER_ENABLE &&
      bool(number) != jit::IsBaselineInterpreterEnabled()) {
    JS_ReportErrorASCII(cx,
                        "Enabling or disabling the Baseline Interpreter at "
                        "runtime is not supported.");
    return false;
  }

  // Throw if disabling the JITs and there's JIT code on the stack, to avoid
  // assertion failures.
  if ((opt == JSJITCOMPILER_BASELINE_ENABLE ||
       opt == JSJITCOMPILER_ION_ENABLE) &&
      number == 0) {
    js::jit::JitActivationIterator iter(cx);
    if (!iter.done()) {
      JS_ReportErrorASCII(cx,
                          "Can't turn off JITs with JIT code on the stack.");
      return false;
    }
  }

  // Throw if trying to disable all the Wasm compilers.  The logic here is that
  // if we're trying to disable a compiler that is currently enabled and that is
  // the last compiler enabled then we must throw.
  //
  // Note that this check does not prevent an error from being thrown later.
  // Actual compiler availability is dynamic and depends on other conditions,
  // such as other options set and whether a debugger is present.
  if ((opt == JSJITCOMPILER_WASM_JIT_BASELINE ||
       opt == JSJITCOMPILER_WASM_JIT_ION ||
       opt == JSJITCOMPILER_WASM_JIT_CRANELIFT) &&
      number == 0) {
    uint32_t baseline, ion, cranelift;
    MOZ_ALWAYS_TRUE(JS_GetGlobalJitCompilerOption(
        cx, JSJITCOMPILER_WASM_JIT_BASELINE, &baseline));
    MOZ_ALWAYS_TRUE(
        JS_GetGlobalJitCompilerOption(cx, JSJITCOMPILER_WASM_JIT_ION, &ion));
    MOZ_ALWAYS_TRUE(JS_GetGlobalJitCompilerOption(
        cx, JSJITCOMPILER_WASM_JIT_CRANELIFT, &cranelift));
    if (baseline + ion + cranelift == 1) {
      if ((opt == JSJITCOMPILER_WASM_JIT_BASELINE && baseline) ||
          (opt == JSJITCOMPILER_WASM_JIT_ION && ion) ||
          (opt == JSJITCOMPILER_WASM_JIT_CRANELIFT && cranelift)) {
        JS_ReportErrorASCII(
            cx,
            "Disabling all the Wasm compilers at runtime is not supported.");
        return false;
      }
    }
  }

  // JIT compiler options are process-wide, so we have to stop off-thread
  // compilations for all runtimes to avoid races.
  HelperThreadState().waitForAllThreads();

  // Only release JIT code for the current runtime because there's no good
  // way to discard code for other runtimes.
  ReleaseAllJITCode(cx->runtime()->defaultFreeOp());

  JS_SetGlobalJitCompilerOption(cx, opt, uint32_t(number));

  args.rval().setUndefined();
  return true;
}

static bool EnableLastWarning(JSContext* cx, unsigned argc, Value* vp) {
  ShellContext* sc = GetShellContext(cx);
  CallArgs args = CallArgsFromVp(argc, vp);

  sc->lastWarningEnabled = true;
  sc->lastWarning.setNull();

  args.rval().setUndefined();
  return true;
}

static bool DisableLastWarning(JSContext* cx, unsigned argc, Value* vp) {
  ShellContext* sc = GetShellContext(cx);
  CallArgs args = CallArgsFromVp(argc, vp);

  sc->lastWarningEnabled = false;
  sc->lastWarning.setNull();

  args.rval().setUndefined();
  return true;
}

static bool GetLastWarning(JSContext* cx, unsigned argc, Value* vp) {
  ShellContext* sc = GetShellContext(cx);
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!sc->lastWarningEnabled) {
    JS_ReportErrorASCII(cx, "Call enableLastWarning first.");
    return false;
  }

  if (!JS_WrapValue(cx, &sc->lastWarning)) {
    return false;
  }

  args.rval().set(sc->lastWarning);
  return true;
}

static bool ClearLastWarning(JSContext* cx, unsigned argc, Value* vp) {
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

#if defined(DEBUG) || defined(JS_JITSPEW)
static bool StackDump(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!gOutFile->isOpen()) {
    JS_ReportErrorASCII(cx, "output file is closed");
    return false;
  }

  bool showArgs = ToBoolean(args.get(0));
  bool showLocals = ToBoolean(args.get(1));
  bool showThisProps = ToBoolean(args.get(2));

  JS::UniqueChars buf =
      JS::FormatStackDump(cx, showArgs, showLocals, showThisProps);
  if (!buf) {
    fputs("Failed to format JavaScript stack for dump\n", gOutFile->fp);
    JS_ClearPendingException(cx);
  } else {
    fputs(buf.get(), gOutFile->fp);
  }

  args.rval().setUndefined();
  return true;
}
#endif

static bool StackPointerInfo(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Copy the truncated stack pointer to the result.  This value is not used
  // as a pointer but as a way to measure frame-size from JS.
  args.rval().setInt32(int32_t(reinterpret_cast<size_t>(&args) & 0xfffffff));
  return true;
}

static bool Elapsed(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() == 0) {
    double d = PRMJ_Now() - GetShellContext(cx)->startTime;
    args.rval().setDouble(d);
    return true;
  }
  JS_ReportErrorASCII(cx, "Wrong number of arguments");
  return false;
}

static bool Compile(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "compile", 1)) {
    return false;
  }
  if (!args[0].isString()) {
    const char* typeName = InformalValueTypeName(args[0]);
    JS_ReportErrorASCII(cx, "expected string to compile, got %s", typeName);
    return false;
  }

  RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  JSString* scriptContents = args[0].toString();

  AutoStableStringChars stableChars(cx);
  if (!stableChars.initTwoByte(cx, scriptContents)) {
    return false;
  }

  JS::CompileOptions options(cx);
  options.setIntroductionType("js shell compile")
      .setFileAndLine("<string>", 1)
      .setIsRunOnce(true)
      .setNoScriptRval(true);

  JS::SourceText<char16_t> srcBuf;
  if (!srcBuf.init(cx, stableChars.twoByteRange().begin().get(),
                   scriptContents->length(), JS::SourceOwnership::Borrowed)) {
    return false;
  }

  RootedScript script(cx, JS::Compile(cx, options, srcBuf));
  if (!script) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

static ShellCompartmentPrivate* EnsureShellCompartmentPrivate(JSContext* cx) {
  Compartment* comp = cx->compartment();
  auto priv =
      static_cast<ShellCompartmentPrivate*>(JS_GetCompartmentPrivate(comp));
  if (!priv) {
    priv = cx->new_<ShellCompartmentPrivate>();
    JS_SetCompartmentPrivate(cx->compartment(), priv);
  }
  return priv;
}

static bool ParseModule(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "parseModule", 1)) {
    return false;
  }

  if (!args[0].isString()) {
    const char* typeName = InformalValueTypeName(args[0]);
    JS_ReportErrorASCII(cx, "expected string to compile, got %s", typeName);
    return false;
  }

  JSString* scriptContents = args[0].toString();

  UniqueChars filename;
  CompileOptions options(cx);
  if (args.length() > 1) {
    if (!args[1].isString()) {
      const char* typeName = InformalValueTypeName(args[1]);
      JS_ReportErrorASCII(cx, "expected filename string, got %s", typeName);
      return false;
    }

    RootedString str(cx, args[1].toString());
    filename = JS_EncodeStringToLatin1(cx, str);
    if (!filename) {
      return false;
    }

    options.setFileAndLine(filename.get(), 1);
  } else {
    options.setFileAndLine("<string>", 1);
  }

  AutoStableStringChars stableChars(cx);
  if (!stableChars.initTwoByte(cx, scriptContents)) {
    return false;
  }

  const char16_t* chars = stableChars.twoByteRange().begin().get();
  JS::SourceText<char16_t> srcBuf;
  if (!srcBuf.init(cx, chars, scriptContents->length(),
                   JS::SourceOwnership::Borrowed)) {
    return false;
  }

  RootedObject module(cx, frontend::CompileModule(cx, options, srcBuf));
  if (!module) {
    return false;
  }

  args.rval().setObject(*module);
  return true;
}

// A JSObject that holds XDRBuffer.
class XDRBufferObject : public NativeObject {
  static const size_t VECTOR_SLOT = 0;
  static const unsigned RESERVED_SLOTS = 1;

 public:
  static const JSClassOps classOps_;
  static const JSClass class_;

  inline static MOZ_MUST_USE XDRBufferObject* create(JSContext* cx,
                                                     JS::TranscodeBuffer* buf);

  JS::TranscodeBuffer* data() const {
    Value value = getReservedSlot(VECTOR_SLOT);
    auto buf = static_cast<JS::TranscodeBuffer*>(value.toPrivate());
    MOZ_ASSERT(buf);
    return buf;
  }

  bool hasData() const {
    // Data may not be present if we hit OOM in initialization.
    return !getReservedSlot(VECTOR_SLOT).isUndefined();
  }

  static void finalize(JSFreeOp* fop, JSObject* obj);
};

/*static */ const JSClassOps XDRBufferObject::classOps_ = {
    nullptr,                    // addProperty
    nullptr,                    // delProperty
    nullptr,                    // enumerate
    nullptr,                    // newEnumerate
    nullptr,                    // resolve
    nullptr,                    // mayResolve
    XDRBufferObject::finalize,  // finalize
    nullptr,                    // call
    nullptr,                    // hasInstance
    nullptr,                    // construct
    nullptr,                    // trace
};

/*static */ const JSClass XDRBufferObject::class_ = {
    "XDRBufferObject",
    JSCLASS_HAS_RESERVED_SLOTS(XDRBufferObject::RESERVED_SLOTS) |
        JSCLASS_BACKGROUND_FINALIZE,
    &XDRBufferObject::classOps_};

XDRBufferObject* XDRBufferObject::create(JSContext* cx,
                                         JS::TranscodeBuffer* buf) {
  XDRBufferObject* bufObj = NewObjectWithNullTaggedProto<XDRBufferObject>(cx);
  if (!bufObj) {
    return nullptr;
  }

  auto heapBuf = cx->make_unique<JS::TranscodeBuffer>();
  if (!heapBuf) {
    return nullptr;
  }

  if (!heapBuf->appendAll(*buf)) {
    return nullptr;
  }

  size_t len = heapBuf->length();
  InitReservedSlot(bufObj, VECTOR_SLOT, heapBuf.release(), len,
                   MemoryUse::XDRBufferElements);

  return bufObj;
}

void XDRBufferObject::finalize(JSFreeOp* fop, JSObject* obj) {
  XDRBufferObject* buf = &obj->as<XDRBufferObject>();
  if (buf->hasData()) {
    fop->delete_(buf, buf->data(), buf->data()->length(),
                 MemoryUse::XDRBufferElements);
  }
}

static bool CodeModule(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "codeModule", 1)) {
    return false;
  }

  if (!args[0].isObject() || !args[0].toObject().is<ModuleObject>()) {
    const char* typeName = InformalValueTypeName(args[0]);
    JS_ReportErrorASCII(cx, "expected module object, got %s", typeName);
    return false;
  }

  RootedModuleObject modObject(cx, &args[0].toObject().as<ModuleObject>());
  if (modObject->status() >= MODULE_STATUS_INSTANTIATING) {
    JS_ReportErrorASCII(cx, "cannot encode module after instantiation.");
    return false;
  }

  JS::TranscodeBuffer buf;
  XDREncoder xdrEncoder_(cx, buf);
  XDRResult res = xdrEncoder_.codeModuleObject(&modObject);
  if (res.isErr()) {
    return false;
  }

  XDRBufferObject* xdrBuf = XDRBufferObject::create(cx, &buf);
  if (!xdrBuf) {
    return false;
  }
  args.rval().setObject(*xdrBuf);
  return true;
}

static bool DecodeModule(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "decodeModule", 1)) {
    return false;
  }

  if (!args[0].isObject() || !args[0].toObject().is<XDRBufferObject>()) {
    const char* typeName = InformalValueTypeName(args[0]);
    JS_ReportErrorASCII(cx, "expected XDRBufferObject to compile, got %s",
                        typeName);
    return false;
  }

  XDRDecoder xdrDecoder_(cx, *args[0].toObject().as<XDRBufferObject>().data());
  RootedModuleObject modObject(cx, nullptr);
  XDRResult res = xdrDecoder_.codeModuleObject(&modObject);
  if (res.isErr()) {
    return false;
  }

  if (!ModuleObject::Freeze(cx, modObject)) {
    return false;
  }

  args.rval().setObject(*modObject);
  return true;
}

static bool SetModuleLoadHook(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "setModuleLoadHook", 1)) {
    return false;
  }

  if (!args[0].isObject() || !args[0].toObject().is<JSFunction>()) {
    const char* typeName = InformalValueTypeName(args[0]);
    JS_ReportErrorASCII(cx, "expected hook function, got %s", typeName);
    return false;
  }

  Handle<GlobalObject*> global = cx->global();
  global->setReservedSlot(GlobalAppSlotModuleLoadHook, args[0]);

  args.rval().setUndefined();
  return true;
}

static bool SetModuleResolveHook(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "setModuleResolveHook", 1)) {
    return false;
  }

  if (!args[0].isObject() || !args[0].toObject().is<JSFunction>()) {
    const char* typeName = InformalValueTypeName(args[0]);
    JS_ReportErrorASCII(cx, "expected hook function, got %s", typeName);
    return false;
  }

  Handle<GlobalObject*> global = cx->global();
  global->setReservedSlot(GlobalAppSlotModuleResolveHook, args[0]);

  args.rval().setUndefined();
  return true;
}

static JSObject* ShellModuleResolveHook(JSContext* cx,
                                        HandleValue referencingPrivate,
                                        HandleString specifier) {
  Handle<GlobalObject*> global = cx->global();
  RootedValue hookValue(
      cx, global->getReservedSlot(GlobalAppSlotModuleResolveHook));
  if (hookValue.isUndefined()) {
    JS_ReportErrorASCII(cx, "Module resolve hook not set");
    return nullptr;
  }
  MOZ_ASSERT(hookValue.toObject().is<JSFunction>());

  JS::AutoValueArray<2> args(cx);
  args[0].set(referencingPrivate);
  args[1].setString(specifier);

  RootedValue result(cx);
  if (!JS_CallFunctionValue(cx, nullptr, hookValue, args, &result)) {
    return nullptr;
  }

  if (!result.isObject() || !result.toObject().is<ModuleObject>()) {
    JS_ReportErrorASCII(cx, "Module resolve hook did not return Module object");
    return nullptr;
  }

  return &result.toObject();
}

static bool SetModuleMetadataHook(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "setModuleMetadataHook", 1)) {
    return false;
  }

  if (!args[0].isObject() || !args[0].toObject().is<JSFunction>()) {
    const char* typeName = InformalValueTypeName(args[0]);
    JS_ReportErrorASCII(cx, "expected hook function, got %s", typeName);
    return false;
  }

  Handle<GlobalObject*> global = cx->global();
  global->setReservedSlot(GlobalAppSlotModuleMetadataHook, args[0]);

  args.rval().setUndefined();
  return true;
}

static bool CallModuleMetadataHook(JSContext* cx, HandleValue modulePrivate,
                                   HandleObject metaObject) {
  Handle<GlobalObject*> global = cx->global();
  RootedValue hookValue(
      cx, global->getReservedSlot(GlobalAppSlotModuleMetadataHook));
  if (hookValue.isUndefined()) {
    JS_ReportErrorASCII(cx, "Module metadata hook not set");
    return false;
  }
  MOZ_ASSERT(hookValue.toObject().is<JSFunction>());

  JS::AutoValueArray<2> args(cx);
  args[0].set(modulePrivate);
  args[1].setObject(*metaObject);

  RootedValue dummy(cx);
  return JS_CallFunctionValue(cx, nullptr, hookValue, args, &dummy);
}

static bool ReportArgumentTypeError(JSContext* cx, HandleValue value,
                                    const char* expected) {
  const char* typeName = InformalValueTypeName(value);
  JS_ReportErrorASCII(cx, "Expected %s, got %s", expected, typeName);
  return false;
}

static bool ShellSetModulePrivate(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!args.requireAtLeast(cx, "setModulePrivate", 2)) {
    return false;
  }

  if (!args[0].isObject() || !args[0].toObject().is<ModuleObject>()) {
    return ReportArgumentTypeError(cx, args[0], "module object");
  }

  JS::SetModulePrivate(&args[0].toObject(), args[1]);
  args.rval().setUndefined();
  return true;
}

static bool ShellGetModulePrivate(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!args.requireAtLeast(cx, "getModulePrivate", 1)) {
    return false;
  }

  if (!args[0].isObject() || !args[0].toObject().is<ModuleObject>()) {
    return ReportArgumentTypeError(cx, args[0], "module object");
  }

  args.rval().set(JS::GetModulePrivate(&args[0].toObject()));
  return true;
}

static bool SetModuleDynamicImportHook(JSContext* cx, unsigned argc,
                                       Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "setModuleDynamicImportHook", 1)) {
    return false;
  }

  if (!args[0].isObject() || !args[0].toObject().is<JSFunction>()) {
    const char* typeName = InformalValueTypeName(args[0]);
    JS_ReportErrorASCII(cx, "expected hook function, got %s", typeName);
    return false;
  }

  Handle<GlobalObject*> global = cx->global();
  global->setReservedSlot(GlobalAppSlotModuleDynamicImportHook, args[0]);

  args.rval().setUndefined();
  return true;
}

static bool FinishDynamicModuleImport(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "finishDynamicModuleImport", 3)) {
    return false;
  }

  if (!args[1].isString()) {
    return ReportArgumentTypeError(cx, args[1], "String");
  }

  if (!args[2].isObject() || !args[2].toObject().is<PromiseObject>()) {
    return ReportArgumentTypeError(cx, args[2], "PromiseObject");
  }

  RootedString specifier(cx, args[1].toString());
  Rooted<PromiseObject*> promise(cx, &args[2].toObject().as<PromiseObject>());

  return js::FinishDynamicModuleImport(cx, args[0], specifier, promise);
}

static bool AbortDynamicModuleImport(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "abortDynamicModuleImport", 4)) {
    return false;
  }

  if (!args[1].isString()) {
    return ReportArgumentTypeError(cx, args[1], "String");
  }

  if (!args[2].isObject() || !args[2].toObject().is<PromiseObject>()) {
    return ReportArgumentTypeError(cx, args[2], "PromiseObject");
  }

  RootedString specifier(cx, args[1].toString());
  Rooted<PromiseObject*> promise(cx, &args[2].toObject().as<PromiseObject>());

  cx->setPendingExceptionAndCaptureStack(args[3]);
  return js::FinishDynamicModuleImport(cx, args[0], specifier, promise);
}

static bool ShellModuleDynamicImportHook(JSContext* cx,
                                         HandleValue referencingPrivate,
                                         HandleString specifier,
                                         HandleObject promise) {
  Handle<GlobalObject*> global = cx->global();
  RootedValue hookValue(
      cx, global->getReservedSlot(GlobalAppSlotModuleDynamicImportHook));
  if (hookValue.isUndefined()) {
    JS_ReportErrorASCII(cx, "Module resolve hook not set");
    return false;
  }
  MOZ_ASSERT(hookValue.toObject().is<JSFunction>());

  JS::AutoValueArray<3> args(cx);
  args[0].set(referencingPrivate);
  args[1].setString(specifier);
  args[2].setObject(*promise);

  RootedValue result(cx);
  return JS_CallFunctionValue(cx, nullptr, hookValue, args, &result);
}

static bool GetModuleLoadPath(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  ShellContext* sc = GetShellContext(cx);
  if (sc->moduleLoadPath) {
    JSString* str = JS_NewStringCopyZ(cx, sc->moduleLoadPath.get());
    if (!str) {
      return false;
    }

    args.rval().setString(str);
  } else {
    args.rval().setNull();
  }
  return true;
}

#if defined(JS_BUILD_BINAST)

template <typename Tok>
static bool ParseBinASTData(JSContext* cx, uint8_t* buf_data,
                            uint32_t buf_length,
                            js::frontend::GlobalSharedContext* globalsc,
                            js::frontend::CompilationInfo& compilationInfo,
                            const JS::ReadOnlyCompileOptions& options,
                            HandleScriptSourceObject sourceObj) {
  MOZ_ASSERT(globalsc);

  // Note: We need to keep `reader` alive as long as we can use `parsed`.
  js::frontend::BinASTParser<Tok> reader(cx, compilationInfo, options,
                                         sourceObj);

  JS::Result<js::frontend::ParseNode*> parsed =
      reader.parse(globalsc, buf_data, buf_length);

  if (parsed.isErr()) {
    return false;
  }

#  ifdef DEBUG
  Fprinter out(stderr);
  DumpParseTree(parsed.unwrap(), out);
#  endif

  return true;
}

static bool BinParse(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!args.requireAtLeast(cx, "parseBin", 1)) {
    return false;
  }

  // Extract argument 1: ArrayBuffer.

  if (!args[0].isObject()) {
    const char* typeName = InformalValueTypeName(args[0]);
    JS_ReportErrorASCII(cx, "expected object (ArrayBuffer) to parse, got %s",
                        typeName);
    return false;
  }

  RootedObject objBuf(cx, &args[0].toObject());
  if (!JS::IsArrayBufferObject(objBuf)) {
    const char* typeName = InformalValueTypeName(args[0]);
    JS_ReportErrorASCII(cx, "expected ArrayBuffer to parse, got %s", typeName);
    return false;
  }

  uint32_t buf_length = 0;
  bool buf_isSharedMemory = false;
  uint8_t* buf_data = nullptr;
  JS::GetArrayBufferLengthAndData(objBuf, &buf_length, &buf_isSharedMemory,
                                  &buf_data);
  MOZ_ASSERT(buf_data);

  // Extract argument 2: Options.

  // BinAST currently supports 2 formats, multipart and context.
  enum {
    Multipart,
    Context,
  } mode = Multipart;

  if (args.length() >= 2) {
    if (!args[1].isObject()) {
      const char* typeName = InformalValueTypeName(args[1]);
      JS_ReportErrorASCII(cx, "expected object (options) to parse, got %s",
                          typeName);
      return false;
    }
    RootedObject objOptions(cx, &args[1].toObject());

    RootedValue optionFormat(cx);
    if (!JS_GetProperty(cx, objOptions, "format", &optionFormat)) {
      return false;
    }

    if (!optionFormat.isUndefined()) {
      RootedLinearString linearFormat(
          cx, optionFormat.toString()->ensureLinear(cx));
      if (!linearFormat) {
        return false;
      }
      // Currently not used, reserved for future.
      if (StringEqualsLiteral(linearFormat, "multipart")) {
        mode = Multipart;
      } else if (StringEqualsLiteral(linearFormat, "context")) {
        mode = Context;
      } else {
        UniqueChars printable = QuoteString(cx, linearFormat, '\'');
        if (!printable) {
          return false;
        }

        JS_ReportErrorASCII(
            cx,
            "Unknown value for option `format`, expected 'multipart', got %s",
            printable.get());
        return false;
      }
    } else {
      const char* typeName = InformalValueTypeName(optionFormat);
      JS_ReportErrorASCII(cx, "option `format` should be a string, got %s",
                          typeName);
      return false;
    }
  }

  CompileOptions options(cx);
  options.setIntroductionType("js shell bin parse")
      .setFileAndLine("<ArrayBuffer>", 1);

  LifoAllocScope allocScope(&cx->tempLifoAlloc());
  js::frontend::CompilationInfo compilationInfo(cx, allocScope, options);
  if (!compilationInfo.init(cx)) {
    return false;
  }

  js::frontend::Directives directives(false);
  js::frontend::GlobalSharedContext globalsc(cx, ScopeKind::Global,
                                             compilationInfo, directives);

  auto parseFunc = mode == Multipart
                       ? ParseBinASTData<frontend::BinASTTokenReaderMultipart>
                       : ParseBinASTData<frontend::BinASTTokenReaderContext>;
  if (!parseFunc(cx, buf_data, buf_length, &globalsc, compilationInfo, options,
                 compilationInfo.sourceObject)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

#endif  // defined(JS_BUILD_BINAST)

template <typename Unit>
static bool FullParseTest(JSContext* cx,
                          const JS::ReadOnlyCompileOptions& options,
                          const Unit* units, size_t length,
                          js::frontend::CompilationInfo& compilationInfo,
                          ScriptSourceObject* sourceObject,
                          js::frontend::ParseGoal goal) {
  using namespace js::frontend;
  Parser<FullParseHandler, Unit> parser(cx, options, units, length,
                                        /* foldConstants = */ false,
                                        compilationInfo, nullptr, nullptr,
                                        sourceObject);
  if (!parser.checkOptions()) {
    return false;
  }

  js::frontend::ParseNode* pn;  // Deallocated once `parser` goes out of scope.
  if (goal == frontend::ParseGoal::Script) {
    pn = parser.parse();
  } else {
    if (!GlobalObject::ensureModulePrototypesCreated(cx, cx->global())) {
      return false;
    }

    Rooted<ModuleObject*> module(cx, ModuleObject::create(cx));
    if (!module) {
      return false;
    }

    ModuleBuilder builder(cx, &parser);

    ModuleSharedContext modulesc(cx, module, compilationInfo, nullptr, builder);
    pn = parser.moduleBody(&modulesc);
  }
  if (!pn) {
    return false;
  }
#ifdef DEBUG
  js::Fprinter out(stderr);
  DumpParseTree(pn, out);
#endif
  return true;
}

static bool Parse(JSContext* cx, unsigned argc, Value* vp) {
  using namespace js::frontend;

  CallArgs args = CallArgsFromVp(argc, vp);

  if (!args.requireAtLeast(cx, "parse", 1)) {
    return false;
  }
  if (!args[0].isString()) {
    const char* typeName = InformalValueTypeName(args[0]);
    JS_ReportErrorASCII(cx, "expected string to parse, got %s", typeName);
    return false;
  }

  frontend::ParseGoal goal = frontend::ParseGoal::Script;
#ifdef JS_ENABLE_SMOOSH
  bool smoosh = false;
#endif

  if (args.length() >= 2) {
    if (!args[1].isObject()) {
      const char* typeName = InformalValueTypeName(args[1]);
      JS_ReportErrorASCII(cx, "expected object (options) to parse, got %s",
                          typeName);
      return false;
    }
    RootedObject objOptions(cx, &args[1].toObject());

    RootedValue optionModule(cx);
    if (!JS_GetProperty(cx, objOptions, "module", &optionModule)) {
      return false;
    }

    if (optionModule.isBoolean()) {
      if (optionModule.toBoolean()) {
        goal = frontend::ParseGoal::Module;
      }
    } else if (!optionModule.isUndefined()) {
      const char* typeName = InformalValueTypeName(optionModule);
      JS_ReportErrorASCII(cx, "option `module` should be a boolean, got %s",
                          typeName);
      return false;
    }

#ifdef JS_ENABLE_SMOOSH
    bool found = false;
    if (!JS_HasProperty(cx, objOptions, "rustFrontend", &found)) {
      return false;
    }
    if (found) {
      JS_ReportErrorASCII(cx, "'rustFrontend' option is renamed to 'smoosh'");
      return false;
    }

    RootedValue optionSmoosh(cx);
    if (!JS_GetProperty(cx, objOptions, "smoosh", &optionSmoosh)) {
      return false;
    }

    if (optionSmoosh.isBoolean()) {
      smoosh = optionSmoosh.toBoolean();
    } else if (!optionSmoosh.isUndefined()) {
      const char* typeName = InformalValueTypeName(optionSmoosh);
      JS_ReportErrorASCII(cx, "option `smoosh` should be a boolean, got %s",
                          typeName);
      return false;
    }
#endif  // JS_ENABLE_SMOOSH
  }

  JSString* scriptContents = args[0].toString();

  AutoStableStringChars stableChars(cx);
  if (!stableChars.init(cx, scriptContents)) {
    return false;
  }

  size_t length = scriptContents->length();
#ifdef JS_ENABLE_SMOOSH
  if (smoosh) {
    if (stableChars.isLatin1()) {
      const Latin1Char* chars = stableChars.latin1Range().begin().get();
      if (goal == frontend::ParseGoal::Script) {
        if (!SmooshParseScript(cx, chars, length)) {
          return false;
        }
      } else {
        if (!SmooshParseModule(cx, chars, length)) {
          return false;
        }
      }
      args.rval().setUndefined();
      return true;
    }
    JS_ReportErrorASCII(cx, "SmooshMonkey does not support two-byte chars yet");
    return false;
  }
#endif  // JS_ENABLE_SMOOSH

  CompileOptions options(cx);
  options.setIntroductionType("js shell parse").setFileAndLine("<string>", 1);
  if (goal == frontend::ParseGoal::Module) {
    // See frontend::CompileModule.
    options.setForceStrictMode();
    options.allowHTMLComments = false;
  }

  LifoAllocScope allocScope(&cx->tempLifoAlloc());
  js::frontend::CompilationInfo compilationInfo(cx, allocScope, options);
  if (!compilationInfo.init(cx)) {
    return false;
  }

  RootedScriptSourceObject sourceObject(
      cx, frontend::CreateScriptSourceObject(cx, options));
  if (!sourceObject) {
    return false;
  }

  if (stableChars.isLatin1()) {
    const Latin1Char* chars_ = stableChars.latin1Range().begin().get();
    auto chars = reinterpret_cast<const mozilla::Utf8Unit*>(chars_);
    if (!FullParseTest<mozilla::Utf8Unit>(
            cx, options, chars, length, compilationInfo, sourceObject, goal)) {
      return false;
    }
  } else {
    MOZ_ASSERT(stableChars.isTwoByte());
    const char16_t* chars = stableChars.twoByteRange().begin().get();
    if (!FullParseTest<char16_t>(cx, options, chars, length, compilationInfo,
                                 sourceObject, goal)) {
      return false;
    }
  }
  args.rval().setUndefined();
  return true;
}

static bool SyntaxParse(JSContext* cx, unsigned argc, Value* vp) {
  using namespace js::frontend;

  CallArgs args = CallArgsFromVp(argc, vp);

  if (!args.requireAtLeast(cx, "syntaxParse", 1)) {
    return false;
  }
  if (!args[0].isString()) {
    const char* typeName = InformalValueTypeName(args[0]);
    JS_ReportErrorASCII(cx, "expected string to parse, got %s", typeName);
    return false;
  }

  JSString* scriptContents = args[0].toString();

  CompileOptions options(cx);
  options.setIntroductionType("js shell syntaxParse")
      .setFileAndLine("<string>", 1);

  AutoStableStringChars stableChars(cx);
  if (!stableChars.initTwoByte(cx, scriptContents)) {
    return false;
  }

  const char16_t* chars = stableChars.twoByteRange().begin().get();
  size_t length = scriptContents->length();

  LifoAllocScope allocScope(&cx->tempLifoAlloc());
  js::frontend::CompilationInfo compilationInfo(cx, allocScope, options);
  if (!compilationInfo.init(cx)) {
    return false;
  }

  Parser<frontend::SyntaxParseHandler, char16_t> parser(
      cx, options, chars, length, false, compilationInfo, nullptr, nullptr,
      compilationInfo.sourceObject);
  if (!parser.checkOptions()) {
    return false;
  }

  bool succeeded = parser.parse();
  if (cx->isExceptionPending()) {
    return false;
  }

  if (!succeeded && !parser.hadAbortedSyntaxParse()) {
    // If no exception is posted, either there was an OOM or a language
    // feature unhandled by the syntax parser was encountered.
    MOZ_ASSERT(cx->runtime()->hadOutOfMemory);
    return false;
  }

  args.rval().setBoolean(succeeded);
  return true;
}

static void OffThreadCompileScriptCallback(JS::OffThreadToken* token,
                                           void* callbackData) {
  auto job = static_cast<OffThreadJob*>(callbackData);
  job->markDone(token);
}

static bool OffThreadCompileScript(JSContext* cx, unsigned argc, Value* vp) {
  if (!CanUseExtraThreads()) {
    JS_ReportErrorASCII(cx,
                        "Can't use offThreadCompileScript with --no-threads");
    return false;
  }

  CallArgs args = CallArgsFromVp(argc, vp);

  if (!args.requireAtLeast(cx, "offThreadCompileScript", 1)) {
    return false;
  }
  if (!args[0].isString()) {
    const char* typeName = InformalValueTypeName(args[0]);
    JS_ReportErrorASCII(cx, "expected string to parse, got %s", typeName);
    return false;
  }

  UniqueChars fileNameBytes;
  CompileOptions options(cx);
  options.setIntroductionType("js shell offThreadCompileScript")
      .setFileAndLine("<string>", 1);

  if (args.length() >= 2) {
    if (args[1].isPrimitive()) {
      JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                                JSSMSG_INVALID_ARGS, "evaluate");
      return false;
    }

    RootedObject opts(cx, &args[1].toObject());
    if (!ParseCompileOptions(cx, options, opts, fileNameBytes)) {
      return false;
    }
  }

  // These option settings must override whatever the caller requested.
  options.setIsRunOnce(true).setSourceIsLazy(false);

  // We assume the caller wants caching if at all possible, ignoring
  // heuristics that make sense for a real browser.
  options.forceAsync = true;

  JSString* scriptContents = args[0].toString();
  AutoStableStringChars stableChars(cx);
  if (!stableChars.initTwoByte(cx, scriptContents)) {
    return false;
  }

  size_t length = scriptContents->length();
  const char16_t* chars = stableChars.twoByteChars();

  // Make sure we own the string's chars, so that they are not freed before
  // the compilation is finished.
  UniqueTwoByteChars ownedChars;
  if (stableChars.maybeGiveOwnershipToCaller()) {
    ownedChars.reset(const_cast<char16_t*>(chars));
  } else {
    ownedChars.reset(cx->pod_malloc<char16_t>(length));
    if (!ownedChars) {
      return false;
    }

    mozilla::PodCopy(ownedChars.get(), chars, length);
  }

  if (!JS::CanCompileOffThread(cx, options, length)) {
    JS_ReportErrorASCII(cx, "cannot compile code on worker thread");
    return false;
  }

  OffThreadJob* job = NewOffThreadJob(
      cx, ScriptKind::Script, OffThreadJob::Source(std::move(ownedChars)));
  if (!job) {
    return false;
  }

  JS::SourceText<char16_t> srcBuf;
  if (!srcBuf.init(cx, job->sourceChars(), length,
                   JS::SourceOwnership::Borrowed) ||
      !JS::CompileOffThread(cx, options, srcBuf, OffThreadCompileScriptCallback,
                            job)) {
    job->cancel();
    DeleteOffThreadJob(cx, job);
    return false;
  }

  args.rval().setInt32(job->id);
  return true;
}

static bool runOffThreadScript(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (OffThreadParsingMustWaitForGC(cx->runtime())) {
    gc::FinishGC(cx);
  }

  OffThreadJob* job =
      LookupOffThreadJobForArgs(cx, ScriptKind::Script, args, 0);
  if (!job) {
    return false;
  }

  JS::OffThreadToken* token = job->waitUntilDone(cx);
  MOZ_ASSERT(token);

  DeleteOffThreadJob(cx, job);

  RootedScript script(cx, JS::FinishOffThreadScript(cx, token));
  if (!script) {
    return false;
  }

  return JS_ExecuteScript(cx, script, args.rval());
}

static bool OffThreadCompileModule(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() != 1 || !args[0].isString()) {
    JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                              JSSMSG_INVALID_ARGS, "offThreadCompileModule");
    return false;
  }

  UniqueChars fileNameBytes;
  CompileOptions options(cx);
  options.setIntroductionType("js shell offThreadCompileModule")
      .setFileAndLine("<string>", 1);
  options.setIsRunOnce(true).setSourceIsLazy(false);
  options.forceAsync = true;

  JSString* scriptContents = args[0].toString();
  AutoStableStringChars stableChars(cx);
  if (!stableChars.initTwoByte(cx, scriptContents)) {
    return false;
  }

  size_t length = scriptContents->length();
  const char16_t* chars = stableChars.twoByteChars();

  // Make sure we own the string's chars, so that they are not freed before
  // the compilation is finished.
  UniqueTwoByteChars ownedChars;
  if (stableChars.maybeGiveOwnershipToCaller()) {
    ownedChars.reset(const_cast<char16_t*>(chars));
  } else {
    ownedChars.reset(cx->pod_malloc<char16_t>(length));
    if (!ownedChars) {
      return false;
    }

    mozilla::PodCopy(ownedChars.get(), chars, length);
  }

  if (!JS::CanCompileOffThread(cx, options, length)) {
    JS_ReportErrorASCII(cx, "cannot compile code on worker thread");
    return false;
  }

  OffThreadJob* job = NewOffThreadJob(
      cx, ScriptKind::Module, OffThreadJob::Source(std::move(ownedChars)));
  if (!job) {
    return false;
  }

  JS::SourceText<char16_t> srcBuf;
  if (!srcBuf.init(cx, job->sourceChars(), length,
                   JS::SourceOwnership::Borrowed) ||
      !JS::CompileOffThreadModule(cx, options, srcBuf,
                                  OffThreadCompileScriptCallback, job)) {
    job->cancel();
    DeleteOffThreadJob(cx, job);
    return false;
  }

  args.rval().setInt32(job->id);
  return true;
}

static bool FinishOffThreadModule(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (OffThreadParsingMustWaitForGC(cx->runtime())) {
    gc::FinishGC(cx);
  }

  OffThreadJob* job =
      LookupOffThreadJobForArgs(cx, ScriptKind::Module, args, 0);
  if (!job) {
    return false;
  }

  JS::OffThreadToken* token = job->waitUntilDone(cx);
  MOZ_ASSERT(token);

  DeleteOffThreadJob(cx, job);

  RootedObject module(cx, JS::FinishOffThreadModule(cx, token));
  if (!module) {
    return false;
  }

  args.rval().setObject(*module);
  return true;
}

static bool OffThreadDecodeScript(JSContext* cx, unsigned argc, Value* vp) {
  if (!CanUseExtraThreads()) {
    JS_ReportErrorASCII(cx,
                        "Can't use offThreadDecodeScript with --no-threads");
    return false;
  }

  CallArgs args = CallArgsFromVp(argc, vp);

  if (!args.requireAtLeast(cx, "offThreadDecodeScript", 1)) {
    return false;
  }
  if (!args[0].isObject() || !CacheEntry_isCacheEntry(&args[0].toObject())) {
    const char* typeName = InformalValueTypeName(args[0]);
    JS_ReportErrorASCII(cx, "expected cache entry, got %s", typeName);
    return false;
  }
  RootedObject cacheEntry(cx, &args[0].toObject());

  UniqueChars fileNameBytes;
  CompileOptions options(cx);
  options.setIntroductionType("js shell offThreadDecodeScript")
      .setFileAndLine("<string>", 1);

  if (args.length() >= 2) {
    if (args[1].isPrimitive()) {
      JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                                JSSMSG_INVALID_ARGS, "evaluate");
      return false;
    }

    RootedObject opts(cx, &args[1].toObject());
    if (!ParseCompileOptions(cx, options, opts, fileNameBytes)) {
      return false;
    }
  }

  // These option settings must override whatever the caller requested.
  options.setIsRunOnce(true).setSourceIsLazy(false);

  // We assume the caller wants caching if at all possible, ignoring
  // heuristics that make sense for a real browser.
  options.forceAsync = true;

  JS::TranscodeBuffer loadBuffer;
  uint32_t loadLength = 0;
  uint8_t* loadData = nullptr;
  loadData = CacheEntry_getBytecode(cx, cacheEntry, &loadLength);
  if (!loadData) {
    return false;
  }
  if (!loadBuffer.append(loadData, loadLength)) {
    JS_ReportOutOfMemory(cx);
    return false;
  }

  if (!JS::CanDecodeOffThread(cx, options, loadLength)) {
    JS_ReportErrorASCII(cx, "cannot compile code on worker thread");
    return false;
  }

  OffThreadJob* job =
      NewOffThreadJob(cx, ScriptKind::DecodeScript,
                      OffThreadJob::Source(std::move(loadBuffer)));
  if (!job) {
    return false;
  }

  if (!JS::DecodeOffThreadScript(cx, options, job->xdrBuffer(), 0,
                                 OffThreadCompileScriptCallback, job)) {
    job->cancel();
    DeleteOffThreadJob(cx, job);
    return false;
  }

  args.rval().setInt32(job->id);
  return true;
}

static bool runOffThreadDecodedScript(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (OffThreadParsingMustWaitForGC(cx->runtime())) {
    gc::FinishGC(cx);
  }

  OffThreadJob* job =
      LookupOffThreadJobForArgs(cx, ScriptKind::DecodeScript, args, 0);
  if (!job) {
    return false;
  }

  JS::OffThreadToken* token = job->waitUntilDone(cx);
  MOZ_ASSERT(token);

  DeleteOffThreadJob(cx, job);

  RootedScript script(cx, JS::FinishOffThreadScriptDecoder(cx, token));
  if (!script) {
    return false;
  }

  return JS_ExecuteScript(cx, script, args.rval());
}

class AutoCStringVector {
  Vector<char*> argv_;

 public:
  explicit AutoCStringVector(JSContext* cx) : argv_(cx) {}
  ~AutoCStringVector() {
    for (size_t i = 0; i < argv_.length(); i++) {
      js_free(argv_[i]);
    }
  }
  bool append(UniqueChars&& arg) {
    if (!argv_.append(arg.get())) {
      return false;
    }

    // Now owned by this vector.
    mozilla::Unused << arg.release();
    return true;
  }
  char* const* get() const { return argv_.begin(); }
  size_t length() const { return argv_.length(); }
  char* operator[](size_t i) const { return argv_[i]; }
  void replace(size_t i, UniqueChars arg) {
    js_free(argv_[i]);
    argv_[i] = arg.release();
  }
};

#if defined(XP_WIN)
static bool EscapeForShell(JSContext* cx, AutoCStringVector& argv) {
  // Windows will break arguments in argv by various spaces, so we wrap each
  // argument in quotes and escape quotes within. Even with quotes, \ will be
  // treated like an escape character, so inflate each \ to \\.

  for (size_t i = 0; i < argv.length(); i++) {
    if (!argv[i]) {
      continue;
    }

    size_t newLen = 3;  // quotes before and after and null-terminator
    for (char* p = argv[i]; *p; p++) {
      newLen++;
      if (*p == '\"' || *p == '\\') {
        newLen++;
      }
    }

    auto escaped = cx->make_pod_array<char>(newLen);
    if (!escaped) {
      return false;
    }

    char* src = argv[i];
    char* dst = escaped.get();
    *dst++ = '\"';
    while (*src) {
      if (*src == '\"' || *src == '\\') {
        *dst++ = '\\';
      }
      *dst++ = *src++;
    }
    *dst++ = '\"';
    *dst++ = '\0';
    MOZ_ASSERT(escaped.get() + newLen == dst);

    argv.replace(i, std::move(escaped));
  }
  return true;
}
#endif

static bool ReadAll(int fd, wasm::Bytes* bytes) {
  size_t lastLength = bytes->length();
  while (true) {
    static const int ChunkSize = 64 * 1024;
    if (!bytes->growBy(ChunkSize)) {
      return false;
    }

    intptr_t readCount;
    while (true) {
      readCount = read(fd, bytes->begin() + lastLength, ChunkSize);
      if (readCount >= 0) {
        break;
      }
      if (errno != EINTR) {
        return false;
      }
    }

    if (readCount < ChunkSize) {
      bytes->shrinkTo(lastLength + readCount);
      if (readCount == 0) {
        return true;
      }
    }

    lastLength = bytes->length();
  }
}

static bool WriteAll(int fd, const uint8_t* bytes, size_t length) {
  while (length > 0) {
    int written = write(fd, bytes, length);
    if (written < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    MOZ_ASSERT(unsigned(written) <= length);
    length -= written;
    bytes += written;
  }

  return true;
}

class AutoPipe {
  int fds_[2];

 public:
  AutoPipe() {
    fds_[0] = -1;
    fds_[1] = -1;
  }

  ~AutoPipe() {
    if (fds_[0] != -1) {
      close(fds_[0]);
    }
    if (fds_[1] != -1) {
      close(fds_[1]);
    }
  }

  bool init() {
#ifdef XP_WIN
    return !_pipe(fds_, 4096, O_BINARY);
#else
    return !pipe(fds_);
#endif
  }

  int reader() const {
    MOZ_ASSERT(fds_[0] != -1);
    return fds_[0];
  }

  int writer() const {
    MOZ_ASSERT(fds_[1] != -1);
    return fds_[1];
  }

  void closeReader() {
    MOZ_ASSERT(fds_[0] != -1);
    close(fds_[0]);
    fds_[0] = -1;
  }

  void closeWriter() {
    MOZ_ASSERT(fds_[1] != -1);
    close(fds_[1]);
    fds_[1] = -1;
  }
};

int shell::sArgc;
char** shell::sArgv;

static const char sWasmCompileAndSerializeFlag[] =
    "--wasm-compile-and-serialize";
static Vector<const char*, 5, js::SystemAllocPolicy> sCompilerProcessFlags;

static bool CompileAndSerializeInSeparateProcess(JSContext* cx,
                                                 const uint8_t* bytecode,
                                                 size_t bytecodeLength,
                                                 wasm::Bytes* serialized) {
  AutoPipe stdIn, stdOut;
  if (!stdIn.init() || !stdOut.init()) {
    return false;
  }

  AutoCStringVector argv(cx);

  UniqueChars argv0 = DuplicateString(cx, sArgv[0]);
  if (!argv0 || !argv.append(std::move(argv0))) {
    return false;
  }

  // Put compiler flags first since they must precede the non-option
  // file-descriptor args (passed on Windows, below).
  for (unsigned i = 0; i < sCompilerProcessFlags.length(); i++) {
    UniqueChars flags = DuplicateString(cx, sCompilerProcessFlags[i]);
    if (!flags || !argv.append(std::move(flags))) {
      return false;
    }
  }

  UniqueChars arg;

  arg = DuplicateString(sWasmCompileAndSerializeFlag);
  if (!arg || !argv.append(std::move(arg))) {
    return false;
  }

#ifdef XP_WIN
  // The spawned process will have all the stdIn/stdOut file handles open, but
  // without the power of fork, we need some other way to communicate the
  // integer fd values so we encode them in argv and WasmCompileAndSerialize()
  // has a matching #ifdef XP_WIN to parse them out. Communicate both ends of
  // both pipes so the child process can closed the unused ends.

  arg = JS_smprintf("%d", stdIn.reader());
  if (!arg || !argv.append(std::move(arg))) {
    return false;
  }

  arg = JS_smprintf("%d", stdIn.writer());
  if (!arg || !argv.append(std::move(arg))) {
    return false;
  }

  arg = JS_smprintf("%d", stdOut.reader());
  if (!arg || !argv.append(std::move(arg))) {
    return false;
  }

  arg = JS_smprintf("%d", stdOut.writer());
  if (!arg || !argv.append(std::move(arg))) {
    return false;
  }
#endif

  // Required by both _spawnv and exec.
  if (!argv.append(nullptr)) {
    return false;
  }

#ifdef XP_WIN
  if (!EscapeForShell(cx, argv)) {
    return false;
  }

  int childPid = _spawnv(P_NOWAIT, sArgv[0], argv.get());
  if (childPid == -1) {
    return false;
  }
#else
  pid_t childPid = fork();
  switch (childPid) {
    case -1:
      return false;
    case 0:
      // In the child process. Redirect stdin/stdout to the respective ends of
      // the pipes. Closing stdIn.writer() is necessary for stdin to hit EOF.
      // This case statement must not return before exec() takes over. Rather,
      // exit(-1) is used to return failure to the parent process.
      if (dup2(stdIn.reader(), STDIN_FILENO) == -1) {
        exit(-1);
      }
      if (dup2(stdOut.writer(), STDOUT_FILENO) == -1) {
        exit(-1);
      }
      close(stdIn.reader());
      close(stdIn.writer());
      close(stdOut.reader());
      close(stdOut.writer());
      execv(sArgv[0], argv.get());
      exit(-1);
  }
#endif

  // In the parent process. Closing stdOut.writer() is necessary for
  // stdOut.reader() below to hit EOF.
  stdIn.closeReader();
  stdOut.closeWriter();

  if (!WriteAll(stdIn.writer(), bytecode, bytecodeLength)) {
    return false;
  }

  stdIn.closeWriter();

  if (!ReadAll(stdOut.reader(), serialized)) {
    return false;
  }

  stdOut.closeReader();

  int status;
#ifdef XP_WIN
  if (_cwait(&status, childPid, WAIT_CHILD) == -1) {
    return false;
  }
#else
  while (true) {
    if (waitpid(childPid, &status, 0) >= 0) {
      break;
    }
    if (errno != EINTR) {
      return false;
    }
  }
#endif

  return status == 0;
}

static bool WasmCompileAndSerialize(JSContext* cx) {
  MOZ_ASSERT(wasm::CodeCachingAvailable(cx));

#ifdef XP_WIN
  // See CompileAndSerializeInSeparateProcess for why we've had to smuggle
  // these fd values through argv. Closing the writing ends is necessary for
  // the reading ends to hit EOF.
  int flagIndex = 0;
  for (; flagIndex < sArgc; flagIndex++) {
    if (!strcmp(sArgv[flagIndex], sWasmCompileAndSerializeFlag)) {
      break;
    }
  }
  MOZ_RELEASE_ASSERT(flagIndex < sArgc);

  int fdsIndex = flagIndex + 1;
  MOZ_RELEASE_ASSERT(fdsIndex + 4 == sArgc);

  int stdInReader = atoi(sArgv[fdsIndex + 0]);
  int stdInWriter = atoi(sArgv[fdsIndex + 1]);
  int stdOutReader = atoi(sArgv[fdsIndex + 2]);
  int stdOutWriter = atoi(sArgv[fdsIndex + 3]);

  int stdIn = stdInReader;
  close(stdInWriter);
  close(stdOutReader);
  int stdOut = stdOutWriter;
#else
  int stdIn = STDIN_FILENO;
  int stdOut = STDOUT_FILENO;
#endif

  wasm::MutableBytes bytecode = js_new<wasm::ShareableBytes>();
  if (!ReadAll(stdIn, &bytecode->bytes)) {
    return false;
  }

  wasm::Bytes serialized;
  if (!wasm::CompileAndSerialize(*bytecode, &serialized)) {
    return false;
  }

  if (!WriteAll(stdOut, serialized.begin(), serialized.length())) {
    return false;
  }

  return true;
}

static bool WasmCompileInSeparateProcess(JSContext* cx, unsigned argc,
                                         Value* vp) {
  if (!wasm::CodeCachingAvailable(cx)) {
    JS_ReportErrorASCII(cx, "WebAssembly caching not supported");
    return false;
  }

  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "wasmCompileInSeparateProcess", 1)) {
    return false;
  }

  SharedMem<uint8_t*> bytecode;
  size_t numBytes;
  if (!args[0].isObject() ||
      !IsBufferSource(&args[0].toObject(), &bytecode, &numBytes)) {
    RootedObject callee(cx, &args.callee());
    ReportUsageErrorASCII(cx, callee, "Argument must be a buffer source");
    return false;
  }

  wasm::Bytes serialized;
  if (!CompileAndSerializeInSeparateProcess(cx, bytecode.unwrap(), numBytes,
                                            &serialized)) {
    if (!cx->isExceptionPending()) {
      JS_ReportErrorASCII(cx, "creating and executing child process");
    }
    return false;
  }

  RootedObject module(cx);
  if (!wasm::DeserializeModule(cx, serialized, &module)) {
    return false;
  }

  args.rval().setObject(*module);
  return true;
}

static bool DecompileFunction(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() < 1 || !args[0].isObject() ||
      !args[0].toObject().is<JSFunction>()) {
    args.rval().setUndefined();
    return true;
  }
  RootedFunction fun(cx, &args[0].toObject().as<JSFunction>());
  JSString* result = JS_DecompileFunction(cx, fun);
  if (!result) {
    return false;
  }
  args.rval().setString(result);
  return true;
}

static bool DecompileThisScript(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  NonBuiltinScriptFrameIter iter(cx);
  if (iter.done()) {
    args.rval().setString(cx->runtime()->emptyString);
    return true;
  }

  {
    JSAutoRealm ar(cx, iter.script());

    RootedScript script(cx, iter.script());
    JSString* result = JS_DecompileScript(cx, script);
    if (!result) {
      return false;
    }

    args.rval().setString(result);
  }

  return JS_WrapValue(cx, args.rval());
}

static bool ValueToSource(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  JSString* str = ValueToSource(cx, args.get(0));
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

static bool ThisFilename(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  JS::AutoFilename filename;
  if (!DescribeScriptedCaller(cx, &filename) || !filename.get()) {
    args.rval().setString(cx->runtime()->emptyString);
    return true;
  }

  JSString* str = JS_NewStringCopyZ(cx, filename.get());
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

static bool WrapWithProto(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  Value obj = args.get(0);
  Value proto = args.get(1);
  if (!obj.isObject() || !proto.isObjectOrNull()) {
    JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                              JSSMSG_INVALID_ARGS, "wrapWithProto");
    return false;
  }

  // Disallow constructing (deeply) nested wrapper chains, to avoid running
  // out of stack space in isCallable/isConstructor. See bug 1126105.
  if (IsWrapper(&obj.toObject())) {
    JS_ReportErrorASCII(cx, "wrapWithProto cannot wrap a wrapper");
    return false;
  }

  WrapperOptions options(cx);
  options.setProto(proto.toObjectOrNull());
  JSObject* wrapped = Wrapper::New(cx, &obj.toObject(),
                                   &Wrapper::singletonWithPrototype, options);
  if (!wrapped) {
    return false;
  }

  args.rval().setObject(*wrapped);
  return true;
}

static bool NewGlobal(JSContext* cx, unsigned argc, Value* vp) {
  JS::RealmOptions options;
  JS::RealmCreationOptions& creationOptions = options.creationOptions();
  JS::RealmBehaviors& behaviors = options.behaviors();
  ShellGlobalKind kind = ShellGlobalKind::WindowProxy;

  SetStandardRealmOptions(options);

  // Default to creating the global in the current compartment unless
  // --more-compartments is used.
  if (defaultToSameCompartment) {
    creationOptions.setExistingCompartment(cx->global());
  } else {
    creationOptions.setNewCompartmentAndZone();
  }

  JS::AutoHoldPrincipals principals(cx);

  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() == 1 && args[0].isObject()) {
    RootedObject opts(cx, &args[0].toObject());
    RootedValue v(cx);

    if (!JS_GetProperty(cx, opts, "invisibleToDebugger", &v)) {
      return false;
    }
    if (v.isBoolean()) {
      creationOptions.setInvisibleToDebugger(v.toBoolean());
    }

    if (!JS_GetProperty(cx, opts, "cloneSingletons", &v)) {
      return false;
    }
    if (v.isBoolean()) {
      creationOptions.setCloneSingletons(v.toBoolean());
    }

    if (!JS_GetProperty(cx, opts, "sameZoneAs", &v)) {
      return false;
    }
    if (v.isObject()) {
      creationOptions.setNewCompartmentInExistingZone(
          UncheckedUnwrap(&v.toObject()));
    }

    if (!JS_GetProperty(cx, opts, "sameCompartmentAs", &v)) {
      return false;
    }
    if (v.isObject()) {
      creationOptions.setExistingCompartment(UncheckedUnwrap(&v.toObject()));
    }

    if (!JS_GetProperty(cx, opts, "newCompartment", &v)) {
      return false;
    }
    if (v.isBoolean() && v.toBoolean()) {
      creationOptions.setNewCompartmentAndZone();
    }

    if (!JS_GetProperty(cx, opts, "disableLazyParsing", &v)) {
      return false;
    }
    if (v.isBoolean()) {
      behaviors.setDisableLazyParsing(v.toBoolean());
    }

    if (!JS_GetProperty(cx, opts, "useWindowProxy", &v)) {
      return false;
    }
    if (v.isBoolean()) {
      kind = v.toBoolean() ? ShellGlobalKind::WindowProxy
                           : ShellGlobalKind::GlobalObject;
    }

    if (!JS_GetProperty(cx, opts, "enableWritableStreams", &v)) {
      return false;
    }
    if (v.isBoolean()) {
      creationOptions.setWritableStreamsEnabled(v.toBoolean());
    }

    if (!JS_GetProperty(cx, opts, "enableReadableStreamPipeTo", &v)) {
      return false;
    }
    if (v.isBoolean()) {
      creationOptions.setReadableStreamPipeToEnabled(v.toBoolean());
    }

    if (!JS_GetProperty(cx, opts, "systemPrincipal", &v)) {
      return false;
    }
    if (v.isBoolean()) {
      principals.reset(&ShellPrincipals::fullyTrusted);
    }

    if (!JS_GetProperty(cx, opts, "principal", &v)) {
      return false;
    }
    if (!v.isUndefined()) {
      uint32_t bits;
      if (!ToUint32(cx, v, &bits)) {
        return false;
      }
      JSPrincipals* newPrincipals = cx->new_<ShellPrincipals>(bits);
      if (!newPrincipals) {
        return false;
      }
      principals.reset(newPrincipals);
    }

    if (!JS_GetProperty(cx, opts, "enableCoopAndCoep", &v)) {
      return false;
    }
    if (v.isBoolean()) {
      creationOptions.setCoopAndCoepEnabled(v.toBoolean());
    }
  }

  if (!CheckRealmOptions(cx, options, principals.get())) {
    return false;
  }

  RootedObject global(cx, NewGlobalObject(cx, options, principals.get(), kind));
  if (!global) {
    return false;
  }

  RootedObject wrapped(cx, ToWindowProxyIfWindow(global));
  if (!JS_WrapObject(cx, &wrapped)) {
    return false;
  }

  args.rval().setObject(*wrapped);
  return true;
}

static bool NukeCCW(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() != 1 || !args[0].isObject() ||
      !IsCrossCompartmentWrapper(&args[0].toObject())) {
    JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                              JSSMSG_INVALID_ARGS, "nukeCCW");
    return false;
  }

  NukeCrossCompartmentWrapper(cx, &args[0].toObject());
  args.rval().setUndefined();
  return true;
}

static bool NukeAllCCWs(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() != 0) {
    JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                              JSSMSG_INVALID_ARGS, "nukeAllCCWs");
    return false;
  }

  NukeCrossCompartmentWrappers(cx, AllCompartments(), cx->realm(),
                               NukeWindowReferences, NukeAllReferences);
  args.rval().setUndefined();
  return true;
}

static bool RecomputeWrappers(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() > 2) {
    JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                              JSSMSG_INVALID_ARGS, "recomputeWrappers");
    return false;
  }

  JS::Compartment* sourceComp = nullptr;
  if (args.get(0).isObject()) {
    sourceComp = GetObjectCompartment(UncheckedUnwrap(&args[0].toObject()));
  }

  JS::Compartment* targetComp = nullptr;
  if (args.get(1).isObject()) {
    targetComp = GetObjectCompartment(UncheckedUnwrap(&args[1].toObject()));
  }

  struct SingleOrAllCompartments final : public CompartmentFilter {
    JS::Compartment* comp;
    explicit SingleOrAllCompartments(JS::Compartment* c) : comp(c) {}
    virtual bool match(JS::Compartment* c) const override {
      return !comp || comp == c;
    }
  };

  if (!js::RecomputeWrappers(cx, SingleOrAllCompartments(sourceComp),
                             SingleOrAllCompartments(targetComp))) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

static bool DumpObjectWrappers(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  bool printedHeader = false;
  for (ZonesIter zone(cx->runtime(), WithAtoms); !zone.done(); zone.next()) {
    bool printedZoneInfo = false;
    for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next()) {
      bool printedCompartmentInfo = false;
      for (Compartment::ObjectWrapperEnum e(comp); !e.empty(); e.popFront()) {
        JSObject* wrapper = e.front().value().unbarrieredGet();
        JSObject* wrapped = e.front().key();
        if (!printedHeader) {
          fprintf(stderr, "Cross-compartment object wrappers:\n");
          printedHeader = true;
        }
        if (!printedZoneInfo) {
          fprintf(stderr, "  Zone %p:\n", zone.get());
          printedZoneInfo = true;
        }
        if (!printedCompartmentInfo) {
          fprintf(stderr, "    Compartment %p:\n", comp.get());
          printedCompartmentInfo = true;
        }
        fprintf(stderr,
                "      Object wrapper %p -> %p in zone %p compartment %p\n",
                wrapper, wrapped, wrapped->zone(), wrapped->compartment());
      }
    }
  }

  if (!printedHeader) {
    fprintf(stderr, "No cross-compartment object wrappers.\n");
  }

  args.rval().setUndefined();
  return true;
}

static bool GetMaxArgs(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setInt32(ARGS_LENGTH_MAX);
  return true;
}

static bool IsHTMLDDA_Call(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // These are the required conditions under which this object may be called
  // by test262 tests, and the required behavior under those conditions.
  if (args.length() == 0 || (args.length() == 1 && args[0].isString() &&
                             args[0].toString()->length() == 0)) {
    args.rval().setNull();
    return true;
  }

  JS_ReportErrorASCII(
      cx, "IsHTMLDDA object is being called in an impermissible manner");
  return false;
}

static bool CreateIsHTMLDDA(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  static const JSClassOps classOps = {
      nullptr,         // addProperty
      nullptr,         // delProperty
      nullptr,         // enumerate
      nullptr,         // newEnumerate
      nullptr,         // resolve
      nullptr,         // mayResolve
      nullptr,         // finalize
      IsHTMLDDA_Call,  // call
      nullptr,         // hasInstance
      nullptr,         // construct
      nullptr,         // trace
  };

  static const JSClass cls = {
      "IsHTMLDDA",
      JSCLASS_EMULATES_UNDEFINED,
      &classOps,
  };

  JSObject* obj = JS_NewObject(cx, &cls);
  if (!obj) {
    return false;
  }
  args.rval().setObject(*obj);
  return true;
}

static bool GetSelfHostedValue(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() != 1 || !args[0].isString()) {
    JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                              JSSMSG_INVALID_ARGS, "getSelfHostedValue");
    return false;
  }
  RootedAtom srcAtom(cx, ToAtom<CanGC>(cx, args[0]));
  if (!srcAtom) {
    return false;
  }
  RootedPropertyName srcName(cx, srcAtom->asPropertyName());
  return cx->runtime()->cloneSelfHostedValue(cx, srcName, args.rval());
}

class ShellSourceHook : public SourceHook {
  // The function we should call to lazily retrieve source code.
  PersistentRootedFunction fun;

 public:
  ShellSourceHook(JSContext* cx, JSFunction& fun) : fun(cx, &fun) {}

  bool load(JSContext* cx, const char* filename, char16_t** twoByteSource,
            char** utf8Source, size_t* length) override {
    MOZ_ASSERT((twoByteSource != nullptr) != (utf8Source != nullptr),
               "must be called requesting only one of UTF-8 or UTF-16 source");

    RootedString str(cx, JS_NewStringCopyZ(cx, filename));
    if (!str) {
      return false;
    }
    RootedValue filenameValue(cx, StringValue(str));

    RootedValue result(cx);
    if (!Call(cx, UndefinedHandleValue, fun, HandleValueArray(filenameValue),
              &result)) {
      return false;
    }

    str = JS::ToString(cx, result);
    if (!str) {
      return false;
    }

    Rooted<JSLinearString*> linear(cx, str->ensureLinear(cx));
    if (!linear) {
      return false;
    }

    if (twoByteSource) {
      *length = JS_GetStringLength(linear);

      *twoByteSource = cx->pod_malloc<char16_t>(*length);
      if (!*twoByteSource) {
        return false;
      }

      CopyChars(*twoByteSource, *linear);
    } else {
      MOZ_ASSERT(utf8Source != nullptr);

      *length = JS::GetDeflatedUTF8StringLength(linear);

      *utf8Source = cx->pod_malloc<char>(*length);
      if (!*utf8Source) {
        return false;
      }

      mozilla::DebugOnly<size_t> dstLen = JS::DeflateStringToUTF8Buffer(
          linear, mozilla::MakeSpan(*utf8Source, *length));
      MOZ_ASSERT(dstLen == *length);
    }

    return true;
  }
};

static bool WithSourceHook(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject callee(cx, &args.callee());

  if (args.length() != 2) {
    ReportUsageErrorASCII(cx, callee, "Wrong number of arguments.");
    return false;
  }

  if (!args[0].isObject() || !args[0].toObject().is<JSFunction>() ||
      !args[1].isObject() || !args[1].toObject().is<JSFunction>()) {
    ReportUsageErrorASCII(cx, callee,
                          "First and second arguments must be functions.");
    return false;
  }

  mozilla::UniquePtr<ShellSourceHook> hook =
      mozilla::MakeUnique<ShellSourceHook>(cx,
                                           args[0].toObject().as<JSFunction>());
  if (!hook) {
    return false;
  }

  mozilla::UniquePtr<SourceHook> savedHook = js::ForgetSourceHook(cx);
  js::SetSourceHook(cx, std::move(hook));

  RootedObject fun(cx, &args[1].toObject());
  bool result = Call(cx, UndefinedHandleValue, fun,
                     JS::HandleValueArray::empty(), args.rval());
  js::SetSourceHook(cx, std::move(savedHook));
  return result;
}

static void PrintProfilerEvents_Callback(const char* msg) {
  fprintf(stderr, "PROFILER EVENT: %s\n", msg);
}

static bool PrintProfilerEvents(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (cx->runtime()->geckoProfiler().enabled()) {
    js::RegisterContextProfilingEventMarker(cx, &PrintProfilerEvents_Callback);
  }
  args.rval().setUndefined();
  return true;
}

#ifdef SINGLESTEP_PROFILING
static void SingleStepCallback(void* arg, jit::Simulator* sim, void* pc) {
  JSContext* cx = reinterpret_cast<JSContext*>(arg);

  // If profiling is not enabled, don't do anything.
  if (!cx->runtime()->geckoProfiler().enabled()) {
    return;
  }

  JS::ProfilingFrameIterator::RegisterState state;
  state.pc = pc;
#  if defined(JS_SIMULATOR_ARM)
  state.sp = (void*)sim->get_register(jit::Simulator::sp);
  state.lr = (void*)sim->get_register(jit::Simulator::lr);
  state.fp = (void*)sim->get_register(jit::Simulator::fp);
#  elif defined(JS_SIMULATOR_MIPS64) || defined(JS_SIMULATOR_MIPS32)
  state.sp = (void*)sim->getRegister(jit::Simulator::sp);
  state.lr = (void*)sim->getRegister(jit::Simulator::ra);
  state.fp = (void*)sim->getRegister(jit::Simulator::fp);
#  else
#    error "NYI: Single-step profiling support"
#  endif

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
        if (!stack.append(",", 1)) {
          oomUnsafe.crash("stack.append");
        }
      }
      if (!stack.append(frames[i].label, strlen(frames[i].label))) {
        oomUnsafe.crash("stack.append");
      }
      frameNo++;
    }
  }

  ShellContext* sc = GetShellContext(cx);

  // Only append the stack if it differs from the last stack.
  if (sc->stacks.empty() || sc->stacks.back().length() != stack.length() ||
      !ArrayEqual(sc->stacks.back().begin(), stack.begin(), stack.length())) {
    if (!sc->stacks.append(std::move(stack))) {
      oomUnsafe.crash("stacks.append");
    }
  }
}
#endif

static bool EnableSingleStepProfiling(JSContext* cx, unsigned argc, Value* vp) {
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

static bool DisableSingleStepProfiling(JSContext* cx, unsigned argc,
                                       Value* vp) {
#ifdef SINGLESTEP_PROFILING
  CallArgs args = CallArgsFromVp(argc, vp);

  jit::Simulator* sim = cx->simulator();
  sim->disable_single_stepping();

  ShellContext* sc = GetShellContext(cx);

  RootedValueVector elems(cx);
  for (size_t i = 0; i < sc->stacks.length(); i++) {
    JSString* stack =
        JS_NewUCStringCopyN(cx, sc->stacks[i].begin(), sc->stacks[i].length());
    if (!stack) {
      return false;
    }
    if (!elems.append(StringValue(stack))) {
      return false;
    }
  }

  JSObject* array = JS::NewArrayObject(cx, elems);
  if (!array) {
    return false;
  }

  sc->stacks.clear();
  args.rval().setObject(*array);
  return true;
#else
  JS_ReportErrorASCII(cx, "single-step profiling not enabled on this platform");
  return false;
#endif
}

static bool IsLatin1(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  bool isLatin1 =
      args.get(0).isString() && args[0].toString()->hasLatin1Chars();
  args.rval().setBoolean(isLatin1);
  return true;
}

static bool HasCopyOnWriteElements(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setBoolean(
      args.get(0).isObject() && args[0].toObject().isNative() &&
      args[0].toObject().as<NativeObject>().denseElementsAreCopyOnWrite());
  return true;
}

static bool EnableGeckoProfiling(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!EnsureGeckoProfilingStackInstalled(cx, GetShellContext(cx))) {
    return false;
  }

  cx->runtime()->geckoProfiler().enableSlowAssertions(false);
  cx->runtime()->geckoProfiler().enable(true);

  args.rval().setUndefined();
  return true;
}

static bool EnableGeckoProfilingWithSlowAssertions(JSContext* cx, unsigned argc,
                                                   Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setUndefined();

  if (cx->runtime()->geckoProfiler().enabled()) {
    // If profiling already enabled with slow assertions disabled,
    // this is a no-op.
    if (cx->runtime()->geckoProfiler().slowAssertionsEnabled()) {
      return true;
    }

    // Slow assertions are off.  Disable profiling before re-enabling
    // with slow assertions on.
    cx->runtime()->geckoProfiler().enable(false);
  }

  if (!EnsureGeckoProfilingStackInstalled(cx, GetShellContext(cx))) {
    return false;
  }

  cx->runtime()->geckoProfiler().enableSlowAssertions(true);
  cx->runtime()->geckoProfiler().enable(true);

  return true;
}

static bool DisableGeckoProfiling(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setUndefined();

  if (!cx->runtime()->geckoProfiler().enabled()) {
    return true;
  }

  cx->runtime()->geckoProfiler().enable(false);
  return true;
}

// Global mailbox that is used to communicate a shareable object value from one
// worker to another.
//
// These object types are shareable:
//
//   - SharedArrayBuffer
//   - WasmMemoryObject (when constructed with shared:true)
//   - WasmModuleObject
//
// For the SharedArrayBuffer and WasmMemoryObject we transmit the underlying
// SharedArrayRawBuffer ("SARB"). For the WasmModuleObject we transmit the
// underlying JS::WasmModule.  The transmitted types are refcounted.  When they
// are in the mailbox their reference counts are at least 1, accounting for the
// reference from the mailbox.
//
// The lock guards the mailbox variable and prevents a race where two workers
// try to set the mailbox at the same time to replace an object that is only
// referenced from the mailbox: the workers will both decrement the reference
// count on the old object, and one of those decrements will be on a garbage
// object.  We could implement this with atomics and a CAS loop but it's not
// worth the bother.
//
// Note that if a thread reads the mailbox repeatedly it will get distinct
// objects on each read.  The alternatives are to cache created objects locally,
// but this retains storage we don't need to retain, or to somehow clear the
// mailbox locally, but this creates a coordination headache.  Buyer beware.

enum class MailboxTag {
  Empty,
  SharedArrayBuffer,
  WasmMemory,
  WasmModule,
  Number,
};

struct SharedObjectMailbox {
  union Value {
    struct {
      SharedArrayRawBuffer* buffer;
      uint32_t length;
    } sarb;
    JS::WasmModule* module;
    double number;
  };

  SharedObjectMailbox() : tag(MailboxTag::Empty) {}

  MailboxTag tag;
  Value val;
};

typedef ExclusiveData<SharedObjectMailbox> SOMailbox;

// Never null after successful initialization.
static SOMailbox* sharedObjectMailbox;

static bool InitSharedObjectMailbox() {
  sharedObjectMailbox = js_new<SOMailbox>(mutexid::ShellObjectMailbox);
  return sharedObjectMailbox != nullptr;
}

static void DestructSharedObjectMailbox() {
  // All workers need to have terminated at this point.

  {
    auto mbx = sharedObjectMailbox->lock();
    switch (mbx->tag) {
      case MailboxTag::Empty:
      case MailboxTag::Number:
        break;
      case MailboxTag::SharedArrayBuffer:
      case MailboxTag::WasmMemory:
        mbx->val.sarb.buffer->dropReference();
        break;
      case MailboxTag::WasmModule:
        mbx->val.module->Release();
        break;
      default:
        MOZ_CRASH();
    }
  }

  js_delete(sharedObjectMailbox);
  sharedObjectMailbox = nullptr;
}

static bool GetSharedObject(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject newObj(cx);

  {
    auto mbx = sharedObjectMailbox->lock();
    switch (mbx->tag) {
      case MailboxTag::Empty: {
        break;
      }
      case MailboxTag::Number: {
        args.rval().setNumber(mbx->val.number);
        return true;
      }
      case MailboxTag::SharedArrayBuffer:
      case MailboxTag::WasmMemory: {
        // Flag was set in the sender; ensure it is set in the receiver.
        MOZ_ASSERT(
            cx->realm()->creationOptions().getSharedMemoryAndAtomicsEnabled());

        // The protocol for creating a SAB requires the refcount to be
        // incremented prior to the SAB creation.

        SharedArrayRawBuffer* buf = mbx->val.sarb.buffer;
        uint32_t length = mbx->val.sarb.length;
        if (!buf->addReference()) {
          JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                    JSMSG_SC_SAB_REFCNT_OFLO);
          return false;
        }

        // If the allocation fails we must decrement the refcount before
        // returning.

        Rooted<ArrayBufferObjectMaybeShared*> maybesab(
            cx, SharedArrayBufferObject::New(cx, buf, length));
        if (!maybesab) {
          buf->dropReference();
          return false;
        }

        // At this point the SAB was created successfully and it owns the
        // refcount-increase on the buffer that we performed above.  So even
        // if we fail to allocate along any path below we must not decrement
        // the refcount; the garbage collector must be allowed to handle
        // that via finalization of the orphaned SAB object.

        if (mbx->tag == MailboxTag::SharedArrayBuffer) {
          newObj = maybesab;
        } else {
          if (!GlobalObject::ensureConstructor(cx, cx->global(),
                                               JSProto_WebAssembly)) {
            return false;
          }
          RootedObject proto(
              cx, &cx->global()->getPrototype(JSProto_WasmMemory).toObject());
          newObj = WasmMemoryObject::create(cx, maybesab, proto);
          MOZ_ASSERT_IF(newObj, newObj->as<WasmMemoryObject>().isShared());
          if (!newObj) {
            return false;
          }
        }

        break;
      }
      case MailboxTag::WasmModule: {
        // Flag was set in the sender; ensure it is set in the receiver.
        MOZ_ASSERT(
            cx->realm()->creationOptions().getSharedMemoryAndAtomicsEnabled());

        if (!GlobalObject::ensureConstructor(cx, cx->global(),
                                             JSProto_WebAssembly)) {
          return false;
        }

        // WasmModuleObject::create() increments the refcount on the module
        // and signals an error and returns null if that fails.
        newObj = mbx->val.module->createObject(cx);
        if (!newObj) {
          return false;
        }
        break;
      }
      default: {
        MOZ_CRASH();
      }
    }
  }

  args.rval().setObjectOrNull(newObj);
  return true;
}

static bool SetSharedObject(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  MailboxTag tag = MailboxTag::Empty;
  SharedObjectMailbox::Value value;

  // Increase refcounts when we obtain the value to avoid operating on dead
  // storage during self-assignment.

  if (args.get(0).isObject()) {
    RootedObject obj(cx, &args[0].toObject());
    if (obj->is<SharedArrayBufferObject>()) {
      Rooted<SharedArrayBufferObject*> sab(cx,
                                           &obj->as<SharedArrayBufferObject>());
      tag = MailboxTag::SharedArrayBuffer;
      value.sarb.buffer = sab->rawBufferObject();
      value.sarb.length = sab->byteLength();
      if (!value.sarb.buffer->addReference()) {
        JS_ReportErrorASCII(cx,
                            "Reference count overflow on SharedArrayBuffer");
        return false;
      }
    } else if (obj->is<WasmMemoryObject>()) {
      // Here we must transmit sab.byteLength() as the length; the SARB has its
      // own notion of the length which may be greater, and that's fine.
      if (obj->as<WasmMemoryObject>().isShared()) {
        Rooted<SharedArrayBufferObject*> sab(
            cx, &obj->as<WasmMemoryObject>()
                     .buffer()
                     .as<SharedArrayBufferObject>());
        tag = MailboxTag::WasmMemory;
        value.sarb.buffer = sab->rawBufferObject();
        value.sarb.length = sab->byteLength();
        if (!value.sarb.buffer->addReference()) {
          JS_ReportErrorASCII(cx,
                              "Reference count overflow on SharedArrayBuffer");
          return false;
        }
      } else {
        JS_ReportErrorASCII(cx, "Invalid argument to SetSharedObject");
        return false;
      }
    } else if (JS::IsWasmModuleObject(obj)) {
      tag = MailboxTag::WasmModule;
      value.module = JS::GetWasmModule(obj).forget().take();
    } else {
      JS_ReportErrorASCII(cx, "Invalid argument to SetSharedObject");
      return false;
    }
  } else if (args.get(0).isNumber()) {
    tag = MailboxTag::Number;
    value.number = args.get(0).toNumber();
    // Nothing
  } else if (args.get(0).isNullOrUndefined()) {
    // Nothing
  } else {
    JS_ReportErrorASCII(cx, "Invalid argument to SetSharedObject");
    return false;
  }

  {
    auto mbx = sharedObjectMailbox->lock();

    switch (mbx->tag) {
      case MailboxTag::Empty:
      case MailboxTag::Number:
        break;
      case MailboxTag::SharedArrayBuffer:
      case MailboxTag::WasmMemory:
        mbx->val.sarb.buffer->dropReference();
        break;
      case MailboxTag::WasmModule:
        mbx->val.module->Release();
        break;
      default:
        MOZ_CRASH();
    }

    mbx->tag = tag;
    mbx->val = value;
  }

  args.rval().setUndefined();
  return true;
}

typedef Vector<uint8_t, 0, SystemAllocPolicy> Uint8Vector;

class StreamCacheEntry : public AtomicRefCounted<StreamCacheEntry>,
                         public JS::OptimizedEncodingListener {
  typedef AtomicRefCounted<StreamCacheEntry> AtomicBase;

  Uint8Vector bytes_;
  ExclusiveData<Uint8Vector> optimized_;

 public:
  explicit StreamCacheEntry(Uint8Vector&& original)
      : bytes_(std::move(original)),
        optimized_(mutexid::ShellStreamCacheEntryState) {}

  // Implement JS::OptimizedEncodingListener:

  MozExternalRefCountType MOZ_XPCOM_ABI AddRef() override {
    AtomicBase::AddRef();
    return 1;  // unused
  }
  MozExternalRefCountType MOZ_XPCOM_ABI Release() override {
    AtomicBase::Release();
    return 0;  // unused
  }

  const Uint8Vector& bytes() const { return bytes_; }

  void storeOptimizedEncoding(JS::UniqueOptimizedEncodingBytes src) override {
    MOZ_ASSERT(src->length() > 0);

    // Tolerate races since a single StreamCacheEntry object can be used as
    // the source of multiple streaming compilations.
    auto dstBytes = optimized_.lock();
    if (dstBytes->length() > 0) {
      return;
    }

    if (!dstBytes->resize(src->length())) {
      return;
    }
    memcpy(dstBytes->begin(), src->begin(), src->length());
  }

  bool hasOptimizedEncoding() const { return !optimized_.lock()->empty(); }
  const Uint8Vector& optimizedEncoding() const {
    return optimized_.lock().get();
  }
};

typedef RefPtr<StreamCacheEntry> StreamCacheEntryPtr;

class StreamCacheEntryObject : public NativeObject {
  static const unsigned CACHE_ENTRY_SLOT = 0;
  static const JSClassOps classOps_;
  static const JSPropertySpec properties_;

  static void finalize(JSFreeOp*, JSObject* obj) {
    obj->as<StreamCacheEntryObject>().cache().Release();
  }

  static bool cachedGetter(JSContext* cx, unsigned argc, Value* vp) {
    CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.thisv().isObject() ||
        !args.thisv().toObject().is<StreamCacheEntryObject>()) {
      return false;
    }

    StreamCacheEntryObject& obj =
        args.thisv().toObject().as<StreamCacheEntryObject>();
    args.rval().setBoolean(obj.cache().hasOptimizedEncoding());
    return true;
  }
  static bool getBuffer(JSContext* cx, unsigned argc, Value* vp) {
    CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.thisv().isObject() ||
        !args.thisv().toObject().is<StreamCacheEntryObject>()) {
      return false;
    }

    auto& bytes =
        args.thisv().toObject().as<StreamCacheEntryObject>().cache().bytes();
    RootedArrayBufferObject buffer(
        cx, ArrayBufferObject::createZeroed(cx, bytes.length()));
    if (!buffer) {
      return false;
    }

    memcpy(buffer->dataPointer(), bytes.begin(), bytes.length());

    args.rval().setObject(*buffer);
    return true;
  }

 public:
  static const unsigned RESERVED_SLOTS = 1;
  static const JSClass class_;
  static const JSPropertySpec properties[];

  static bool construct(JSContext* cx, unsigned argc, Value* vp) {
    CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.requireAtLeast(cx, "streamCacheEntry", 1)) {
      return false;
    }

    SharedMem<uint8_t*> ptr;
    size_t numBytes;
    if (!args[0].isObject() ||
        !IsBufferSource(&args[0].toObject(), &ptr, &numBytes)) {
      RootedObject callee(cx, &args.callee());
      ReportUsageErrorASCII(cx, callee, "Argument must be an ArrayBuffer");
      return false;
    }

    Uint8Vector bytes;
    if (!bytes.resize(numBytes)) {
      return false;
    }

    memcpy(bytes.begin(), ptr.unwrap(), numBytes);

    RefPtr<StreamCacheEntry> cache =
        cx->new_<StreamCacheEntry>(std::move(bytes));
    if (!cache) {
      return false;
    }

    RootedNativeObject obj(
        cx, NewObjectWithGivenProto<StreamCacheEntryObject>(cx, nullptr));
    if (!obj) {
      return false;
    }
    obj->initReservedSlot(CACHE_ENTRY_SLOT,
                          PrivateValue(cache.forget().take()));

    if (!JS_DefineProperty(cx, obj, "cached", cachedGetter, nullptr, 0)) {
      return false;
    }
    if (!JS_DefineFunction(cx, obj, "getBuffer", getBuffer, 0, 0)) {
      return false;
    }

    args.rval().setObject(*obj);
    return true;
  }

  StreamCacheEntry& cache() const {
    return *(StreamCacheEntry*)getReservedSlot(CACHE_ENTRY_SLOT).toPrivate();
  }
};

const JSClassOps StreamCacheEntryObject::classOps_ = {
    nullptr,                           // addProperty
    nullptr,                           // delProperty
    nullptr,                           // enumerate
    nullptr,                           // newEnumerate
    nullptr,                           // resolve
    nullptr,                           // mayResolve
    StreamCacheEntryObject::finalize,  // finalize
    nullptr,                           // call
    nullptr,                           // hasInstance
    nullptr,                           // construct
    nullptr,                           // trace
};

const JSClass StreamCacheEntryObject::class_ = {
    "StreamCacheEntryObject",
    JSCLASS_HAS_RESERVED_SLOTS(StreamCacheEntryObject::RESERVED_SLOTS) |
        JSCLASS_BACKGROUND_FINALIZE,
    &StreamCacheEntryObject::classOps_};

struct BufferStreamJob {
  Variant<Uint8Vector, StreamCacheEntryPtr> source;
  Thread thread;
  JS::StreamConsumer* consumer;

  BufferStreamJob(Uint8Vector&& source, JS::StreamConsumer* consumer)
      : source(AsVariant<Uint8Vector>(std::move(source))), consumer(consumer) {}
  BufferStreamJob(StreamCacheEntry& source, JS::StreamConsumer* consumer)
      : source(AsVariant<StreamCacheEntryPtr>(&source)), consumer(consumer) {}
};

struct BufferStreamState {
  Vector<UniquePtr<BufferStreamJob>, 0, SystemAllocPolicy> jobs;
  size_t delayMillis;
  size_t chunkSize;
  bool shutdown;

  BufferStreamState() : delayMillis(1), chunkSize(10), shutdown(false) {}

  ~BufferStreamState() { MOZ_ASSERT(jobs.empty()); }
};

static ExclusiveWaitableData<BufferStreamState>* bufferStreamState;

static void BufferStreamMain(BufferStreamJob* job) {
  const uint8_t* bytes;
  size_t byteLength;
  JS::OptimizedEncodingListener* listener;
  if (job->source.is<StreamCacheEntryPtr>()) {
    StreamCacheEntry& cache = *job->source.as<StreamCacheEntryPtr>();
    if (cache.hasOptimizedEncoding()) {
      const Uint8Vector& optimized = cache.optimizedEncoding();
      job->consumer->consumeOptimizedEncoding(optimized.begin(),
                                              optimized.length());
      goto done;
    }

    bytes = cache.bytes().begin();
    byteLength = cache.bytes().length();
    listener = &cache;
  } else {
    bytes = job->source.as<Uint8Vector>().begin();
    byteLength = job->source.as<Uint8Vector>().length();
    listener = nullptr;
  }

  size_t byteOffset;
  byteOffset = 0;
  while (true) {
    if (byteOffset == byteLength) {
      job->consumer->streamEnd(listener);
      break;
    }

    bool shutdown;
    size_t delayMillis;
    size_t chunkSize;
    {
      auto state = bufferStreamState->lock();
      shutdown = state->shutdown;
      delayMillis = state->delayMillis;
      chunkSize = state->chunkSize;
    }

    if (shutdown) {
      job->consumer->streamError(JSMSG_STREAM_CONSUME_ERROR);
      break;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(delayMillis));

    chunkSize = std::min(chunkSize, byteLength - byteOffset);

    if (!job->consumer->consumeChunk(bytes + byteOffset, chunkSize)) {
      break;
    }

    byteOffset += chunkSize;
  }

done:
  auto state = bufferStreamState->lock();
  size_t jobIndex = 0;
  while (state->jobs[jobIndex].get() != job) {
    jobIndex++;
  }
  job->thread.detach();  // quiet assert in ~Thread() called by erase().
  state->jobs.erase(state->jobs.begin() + jobIndex);
  if (state->jobs.empty()) {
    state.notify_all(/* jobs empty */);
  }
}

static bool EnsureLatin1CharsLinearString(JSContext* cx, HandleValue value,
                                          UniqueChars* result) {
  if (!value.isString()) {
    result->reset(nullptr);
    return true;
  }
  RootedString str(cx, value.toString());
  if (!str->isLinear() || !str->hasLatin1Chars()) {
    JS_ReportErrorASCII(cx,
                        "only latin1 chars and linear strings are expected");
    return false;
  }

  // Use JS_EncodeStringToLatin1 to null-terminate.
  *result = JS_EncodeStringToLatin1(cx, str);
  return !!*result;
}

static bool ConsumeBufferSource(JSContext* cx, JS::HandleObject obj,
                                JS::MimeType, JS::StreamConsumer* consumer) {
  {
    RootedValue url(cx);
    if (!JS_GetProperty(cx, obj, "url", &url)) {
      return false;
    }
    UniqueChars urlChars;
    if (!EnsureLatin1CharsLinearString(cx, url, &urlChars)) {
      return false;
    }

    RootedValue mapUrl(cx);
    if (!JS_GetProperty(cx, obj, "sourceMappingURL", &mapUrl)) {
      return false;
    }
    UniqueChars mapUrlChars;
    if (!EnsureLatin1CharsLinearString(cx, mapUrl, &mapUrlChars)) {
      return false;
    }

    consumer->noteResponseURLs(urlChars.get(), mapUrlChars.get());
  }

  UniquePtr<BufferStreamJob> job;

  SharedMem<uint8_t*> dataPointer;
  size_t byteLength;
  if (IsBufferSource(obj, &dataPointer, &byteLength)) {
    Uint8Vector bytes;
    if (!bytes.resize(byteLength)) {
      JS_ReportOutOfMemory(cx);
      return false;
    }

    memcpy(bytes.begin(), dataPointer.unwrap(), byteLength);
    job = cx->make_unique<BufferStreamJob>(std::move(bytes), consumer);
  } else if (obj->is<StreamCacheEntryObject>()) {
    job = cx->make_unique<BufferStreamJob>(
        obj->as<StreamCacheEntryObject>().cache(), consumer);
  } else {
    JS_ReportErrorASCII(
        cx,
        "shell streaming consumes a buffer source (buffer or view) "
        "or StreamCacheEntryObject");
    return false;
  }
  if (!job) {
    return false;
  }

  BufferStreamJob* jobPtr = job.get();

  {
    auto state = bufferStreamState->lock();
    MOZ_ASSERT(!state->shutdown);
    if (!state->jobs.append(std::move(job))) {
      JS_ReportOutOfMemory(cx);
      return false;
    }
  }

  {
    AutoEnterOOMUnsafeRegion oomUnsafe;
    if (!jobPtr->thread.init(BufferStreamMain, jobPtr)) {
      oomUnsafe.crash("ConsumeBufferSource");
    }
  }

  return true;
}

static void ReportStreamError(JSContext* cx, size_t errorNumber) {
  JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, errorNumber);
}

static bool SetBufferStreamParams(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "setBufferStreamParams", 2)) {
    return false;
  }

  double delayMillis;
  if (!ToNumber(cx, args[0], &delayMillis)) {
    return false;
  }

  double chunkSize;
  if (!ToNumber(cx, args[1], &chunkSize)) {
    return false;
  }

  {
    auto state = bufferStreamState->lock();
    state->delayMillis = delayMillis;
    state->chunkSize = chunkSize;
  }

  args.rval().setUndefined();
  return true;
}

static void ShutdownBufferStreams() {
  auto state = bufferStreamState->lock();
  state->shutdown = true;
  while (!state->jobs.empty()) {
    state.wait(/* jobs empty */);
  }
  state->jobs.clearAndFree();
}

static bool DumpScopeChain(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject callee(cx, &args.callee());

  if (args.length() != 1) {
    ReportUsageErrorASCII(cx, callee, "Wrong number of arguments");
    return false;
  }

  if (!args[0].isObject() || !(args[0].toObject().is<JSFunction>() ||
                               args[0].toObject().is<ModuleObject>())) {
    ReportUsageErrorASCII(
        cx, callee, "Argument must be an interpreted function or a module");
    return false;
  }

  RootedObject obj(cx, &args[0].toObject());
  RootedScript script(cx);

  if (obj->is<JSFunction>()) {
    RootedFunction fun(cx, &obj->as<JSFunction>());
    if (!fun->isInterpreted()) {
      ReportUsageErrorASCII(cx, callee,
                            "Argument must be an interpreted function");
      return false;
    }
    script = JSFunction::getOrCreateScript(cx, fun);
    if (!script) {
      return false;
    }
  } else {
    script = obj->as<ModuleObject>().maybeScript();
    if (!script) {
      JS_ReportErrorASCII(cx, "module does not have an associated script");
      return false;
    }
  }

#ifndef JS_MORE_DETERMINISTIC
  // Don't dump anything in more-deterministic builds because the output
  // includes pointer values.
  script->bodyScope()->dump();
#endif

  args.rval().setUndefined();
  return true;
}

// For testing gray marking, grayRoot() will heap-allocate an address
// where we can store a JSObject*, and create a new object if one doesn't
// already exist.
//
// Note that EnsureGrayRoot() will blacken the returned object, so it will not
// actually end up marked gray until the following GC clears the black bit
// (assuming nothing is holding onto it.)
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

static bool EnsureGrayRoot(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  auto priv = EnsureShellCompartmentPrivate(cx);
  if (!priv) {
    return false;
  }

  if (!priv->grayRoot) {
    if (!(priv->grayRoot = NewDenseEmptyArray(cx, nullptr, TenuredObject))) {
      return false;
    }
  }

  // Barrier to enforce the invariant that JS does not touch gray objects.
  JSObject* obj = priv->grayRoot;
  JS::ExposeObjectToActiveJS(obj);

  args.rval().setObject(*obj);
  return true;
}

static MarkBitObservers* EnsureMarkBitObservers(JSContext* cx) {
  ShellContext* sc = GetShellContext(cx);
  if (!sc->markObservers) {
    auto* observers =
        cx->new_<MarkBitObservers>(cx->runtime(), NonshrinkingGCObjectVector());
    if (!observers) {
      return nullptr;
    }
    sc->markObservers.reset(observers);
  }
  return sc->markObservers.get();
}

static bool ClearMarkObservers(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  auto markObservers = EnsureMarkBitObservers(cx);
  if (!markObservers) {
    return false;
  }

  markObservers->get().clear();

  args.rval().setUndefined();
  return true;
}

static bool AddMarkObservers(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  auto markObservers = EnsureMarkBitObservers(cx);
  if (!markObservers) {
    return false;
  }

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
  if (!GetLengthProperty(cx, observersArg, &length)) {
    return false;
  }
  for (uint32_t i = 0; i < length; i++) {
    if (!JS_GetElement(cx, observersArg, i, &v)) {
      return false;
    }
    if (!v.isObject()) {
      JS_ReportErrorASCII(cx, "argument must be an Array of objects");
      return false;
    }
    if (!markObservers->get().append(&v.toObject())) {
      return false;
    }
  }

  args.rval().setInt32(length);
  return true;
}

static bool GetMarks(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  auto& observers = GetShellContext(cx)->markObservers;
  if (!observers) {
    args.rval().setUndefined();
    return true;
  }

  size_t length = observers->get().length();
  Rooted<ArrayObject*> ret(cx, js::NewDenseEmptyArray(cx));
  if (!ret) {
    return false;
  }

  for (uint32_t i = 0; i < length; i++) {
    const char* color;
    JSObject* obj = observers->get()[i];
    if (!obj) {
      color = "dead";
    } else {
      gc::TenuredCell* cell = &obj->asTenured();
      if (cell->isMarkedGray()) {
        color = "gray";
      } else if (cell->isMarkedBlack()) {
        color = "black";
      } else {
        color = "unmarked";
      }
    }
    JSString* s = JS_NewStringCopyZ(cx, color);
    if (!s) {
      return false;
    }
    if (!NewbornArrayPush(cx, ret, StringValue(s))) {
      return false;
    }
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
  explicit ShellAutoEntryMonitor(JSContext* cx)
      : AutoEntryMonitor(cx), oom(false), enteredWithoutExit(false) {}

  ~ShellAutoEntryMonitor() { MOZ_ASSERT(!enteredWithoutExit); }

  void Entry(JSContext* cx, JSFunction* function, JS::HandleValue asyncStack,
             const char* asyncCause) override {
    MOZ_ASSERT(!enteredWithoutExit);
    enteredWithoutExit = true;

    RootedString displayId(cx, JS_GetFunctionDisplayId(function));
    if (displayId) {
      UniqueChars displayIdStr = JS_EncodeStringToUTF8(cx, displayId);
      if (!displayIdStr) {
        // We report OOM in buildResult.
        cx->recoverFromOutOfMemory();
        oom = true;
        return;
      }
      oom = !log.append(std::move(displayIdStr));
      return;
    }

    oom = !log.append(DuplicateString("anonymous"));
  }

  void Entry(JSContext* cx, JSScript* script, JS::HandleValue asyncStack,
             const char* asyncCause) override {
    MOZ_ASSERT(!enteredWithoutExit);
    enteredWithoutExit = true;

    UniqueChars label(JS_smprintf("eval:%s", JS_GetScriptFilename(script)));
    oom = !label || !log.append(std::move(label));
  }

  void Exit(JSContext* cx) override {
    MOZ_ASSERT(enteredWithoutExit);
    enteredWithoutExit = false;
  }

  bool buildResult(JSContext* cx, MutableHandleValue resultValue) {
    if (oom) {
      JS_ReportOutOfMemory(cx);
      return false;
    }

    RootedObject result(cx, JS::NewArrayObject(cx, log.length()));
    if (!result) {
      return false;
    }

    for (size_t i = 0; i < log.length(); i++) {
      char* name = log[i].get();
      RootedString string(cx, Atomize(cx, name, strlen(name)));
      if (!string) {
        return false;
      }
      RootedValue value(cx, StringValue(string));
      if (!JS_SetElement(cx, result, i, value)) {
        return false;
      }
    }

    resultValue.setObject(*result.get());
    return true;
  }
};

}  // namespace shell
}  // namespace js

static bool EntryPoints(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() != 1) {
    JS_ReportErrorASCII(cx, "Wrong number of arguments");
    return false;
  }

  RootedObject opts(cx, ToObject(cx, args[0]));
  if (!opts) {
    return false;
  }

  // { function: f } --- Call f.
  {
    RootedValue fun(cx), dummy(cx);

    if (!JS_GetProperty(cx, opts, "function", &fun)) {
      return false;
    }
    if (!fun.isUndefined()) {
      js::shell::ShellAutoEntryMonitor sarep(cx);
      if (!Call(cx, UndefinedHandleValue, fun, JS::HandleValueArray::empty(),
                &dummy)) {
        return false;
      }
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
      if (!object) {
        return false;
      }

      RootedString string(cx, ToString(cx, propv));
      if (!string) {
        return false;
      }
      RootedId id(cx);
      if (!JS_StringToId(cx, string, &id)) {
        return false;
      }

      if (!JS_GetProperty(cx, opts, "value", &valuev)) {
        return false;
      }

      js::shell::ShellAutoEntryMonitor sarep(cx);

      if (!valuev.isUndefined()) {
        if (!JS_SetPropertyById(cx, object, id, valuev)) {
          return false;
        }
      } else {
        if (!JS_GetPropertyById(cx, object, id, &valuev)) {
          return false;
        }
      }

      return sarep.buildResult(cx, args.rval());
    }
  }

  // { ToString: v } --- Apply JS::ToString to v.
  {
    RootedValue v(cx);

    if (!JS_GetProperty(cx, opts, "ToString", &v)) {
      return false;
    }
    if (!v.isUndefined()) {
      js::shell::ShellAutoEntryMonitor sarep(cx);
      if (!JS::ToString(cx, v)) {
        return false;
      }
      return sarep.buildResult(cx, args.rval());
    }
  }

  // { ToNumber: v } --- Apply JS::ToNumber to v.
  {
    RootedValue v(cx);
    double dummy;

    if (!JS_GetProperty(cx, opts, "ToNumber", &v)) {
      return false;
    }
    if (!v.isUndefined()) {
      js::shell::ShellAutoEntryMonitor sarep(cx);
      if (!JS::ToNumber(cx, v, &dummy)) {
        return false;
      }
      return sarep.buildResult(cx, args.rval());
    }
  }

  // { eval: code } --- Apply ToString and then Evaluate to code.
  {
    RootedValue code(cx), dummy(cx);

    if (!JS_GetProperty(cx, opts, "eval", &code)) {
      return false;
    }
    if (!code.isUndefined()) {
      RootedString codeString(cx, ToString(cx, code));
      if (!codeString) {
        return false;
      }

      AutoStableStringChars stableChars(cx);
      if (!stableChars.initTwoByte(cx, codeString)) {
        return false;
      }
      JS::SourceText<char16_t> srcBuf;
      if (!srcBuf.init(cx, stableChars.twoByteRange().begin().get(),
                       codeString->length(), JS::SourceOwnership::Borrowed)) {
        return false;
      }

      CompileOptions options(cx);
      options.setIntroductionType("entryPoint eval")
          .setFileAndLine("entryPoint eval", 1);

      js::shell::ShellAutoEntryMonitor sarep(cx);
      if (!JS::Evaluate(cx, options, srcBuf, &dummy)) {
        return false;
      }
      return sarep.buildResult(cx, args.rval());
    }
  }

  JS_ReportErrorASCII(cx, "bad 'params' object");
  return false;
}

static bool SetARMHwCapFlags(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() != 1) {
    JS_ReportErrorASCII(cx, "Wrong number of arguments");
    return false;
  }

  RootedString flagsListString(cx, JS::ToString(cx, args.get(0)));
  if (!flagsListString) {
    return false;
  }

#if defined(JS_CODEGEN_ARM)
  UniqueChars flagsList = JS_EncodeStringToLatin1(cx, flagsListString);
  if (!flagsList) {
    return false;
  }

  jit::ParseARMHwCapFlags(flagsList.get());
#endif

  args.rval().setUndefined();
  return true;
}

#ifndef __AFL_HAVE_MANUAL_CONTROL
#  define __AFL_LOOP(x) true
#endif

static bool WasmLoop(JSContext* cx, unsigned argc, Value* vp) {
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
      ReportUsageErrorASCII(cx, callee,
                            "Second argument, if present, must be an Object");
      return false;
    }
    importObj = &args[1].toObject();
  }

  RootedString givenPath(cx, args[0].toString());
  RootedString filename(cx, ResolvePath(cx, givenPath, RootRelative));
  if (!filename) {
    return false;
  }

  while (__AFL_LOOP(1000)) {
    Rooted<JSObject*> ret(cx, FileAsTypedArray(cx, filename));
    if (!ret) {
      return false;
    }

    Rooted<TypedArrayObject*> typedArray(cx, &ret->as<TypedArrayObject>());
    RootedWasmInstanceObject instanceObj(cx);
    if (!wasm::Eval(cx, typedArray, importObj, &instanceObj)) {
      // Clear any pending exceptions, we don't care about them
      cx->clearPendingException();
    }
  }

#ifdef __AFL_HAVE_MANUAL_CONTROL  // to silence unreachable code warning
  return true;
#endif
}

static constexpr uint32_t DOM_OBJECT_SLOT = 0;

static const JSClass* GetDomClass();

static JSObject* GetDOMPrototype(JSContext* cx, JSObject* global);

static const JSClass TransplantableDOMObjectClass = {
    "TransplantableDOMObject",
    JSCLASS_IS_DOMJSCLASS | JSCLASS_HAS_RESERVED_SLOTS(1)};

static const JSClass TransplantableDOMProxyObjectClass =
    PROXY_CLASS_DEF("TransplantableDOMProxyObject",
                    JSCLASS_IS_DOMJSCLASS | JSCLASS_HAS_RESERVED_SLOTS(1));

class TransplantableDOMProxyHandler final : public ForwardingProxyHandler {
 public:
  static const TransplantableDOMProxyHandler singleton;
  static const char family;

  constexpr TransplantableDOMProxyHandler() : ForwardingProxyHandler(&family) {}

  // These two proxy traps are called in |js::DeadProxyTargetValue|, which in
  // turn is called when nuking proxies. Because this proxy can temporarily be
  // without an object in its private slot, see |EnsureExpandoObject|, the
  // default implementation inherited from ForwardingProxyHandler can't be used,
  // since it tries to derive the callable/constructible value from the target.
  bool isCallable(JSObject* obj) const override { return false; }
  bool isConstructor(JSObject* obj) const override { return false; }

  // Simplified implementation of |DOMProxyHandler::GetAndClearExpandoObject|.
  static JSObject* GetAndClearExpandoObject(JSObject* obj) {
    const Value& v = GetProxyPrivate(obj);
    if (v.isUndefined()) {
      return nullptr;
    }

    JSObject* expandoObject = &v.toObject();
    SetProxyPrivate(obj, UndefinedValue());
    return expandoObject;
  }

  // Simplified implementation of |DOMProxyHandler::EnsureExpandoObject|.
  static JSObject* EnsureExpandoObject(JSContext* cx, JS::HandleObject obj) {
    const Value& v = GetProxyPrivate(obj);
    if (v.isObject()) {
      return &v.toObject();
    }
    MOZ_ASSERT(v.isUndefined());

    JSObject* expando = JS_NewObjectWithGivenProto(cx, nullptr, nullptr);
    if (!expando) {
      return nullptr;
    }
    SetProxyPrivate(obj, ObjectValue(*expando));
    return expando;
  }
};

const TransplantableDOMProxyHandler TransplantableDOMProxyHandler::singleton;
const char TransplantableDOMProxyHandler::family = 0;

enum TransplantObjectSlots {
  TransplantSourceObject = 0,
};

static bool TransplantObject(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedFunction callee(cx, &args.callee().as<JSFunction>());

  if (args.length() != 1 || !args[0].isObject()) {
    JS_ReportErrorASCII(cx, "transplant() must be called with an object");
    return false;
  }

  // |newGlobal| needs to be a GlobalObject.
  RootedObject newGlobal(
      cx, js::CheckedUnwrapDynamic(&args[0].toObject(), cx,
                                   /* stopAtWindowProxy = */ false));
  if (!newGlobal) {
    ReportAccessDenied(cx);
    return false;
  }
  if (!JS_IsGlobalObject(newGlobal)) {
    JS_ReportErrorNumberASCII(
        cx, GetErrorMessage, nullptr, JSMSG_UNEXPECTED_TYPE,
        "\"global\" passed to transplant()", "not a global object");
    return false;
  }

  const Value& reserved =
      GetFunctionNativeReserved(callee, TransplantSourceObject);
  RootedObject source(cx, CheckedUnwrapStatic(&reserved.toObject()));
  if (!source) {
    ReportAccessDenied(cx);
    return false;
  }
  MOZ_ASSERT(source->getClass()->isDOMClass());

  // The following steps aim to replicate the behavior of UpdateReflectorGlobal
  // in dom/bindings/BindingUtils.cpp. In detail:
  // 1. Check the recursion depth using CheckRecursionLimitConservative.
  // 2. Enter the target compartment.
  // 3. Clone the source object using JS_CloneObject.
  // 4. Check if new wrappers can be created if source and target are in
  //    different compartments.
  // 5. Copy all properties from source to a temporary holder object.
  // 6. Actually transplant the object.
  // 7. And finally copy the properties back to the source object.
  //
  // As an extension to the algorithm in UpdateReflectorGlobal, we also allow
  // to transplant an object into the same compartment as the source object to
  // cover all operations supported by JS_TransplantObject.

  if (!CheckRecursionLimitConservative(cx)) {
    return false;
  }

  bool isProxy = IsProxy(source);
  RootedObject expandoObject(cx);
  if (isProxy) {
    expandoObject =
        TransplantableDOMProxyHandler::GetAndClearExpandoObject(source);
  }

  JSAutoRealm ar(cx, newGlobal);

  RootedObject proto(cx);
  if (JS_GetClass(source) == GetDomClass()) {
    proto = GetDOMPrototype(cx, newGlobal);
  } else {
    proto = JS::GetRealmObjectPrototype(cx);
  }
  if (!proto) {
    return false;
  }

  RootedObject target(cx, JS_CloneObject(cx, source, proto));
  if (!target) {
    return false;
  }

  if (GetObjectCompartment(source) != GetObjectCompartment(target) &&
      !AllowNewWrapper(GetObjectCompartment(source), target)) {
    JS_ReportErrorASCII(cx, "Cannot transplant into nuked compartment");
    return false;
  }

  RootedObject copyFrom(cx, isProxy ? expandoObject : source);
  RootedObject propertyHolder(cx,
                              JS_NewObjectWithGivenProto(cx, nullptr, nullptr));
  if (!propertyHolder) {
    return false;
  }

  if (!JS_CopyPropertiesFrom(cx, propertyHolder, copyFrom)) {
    return false;
  }

  SetReservedSlot(target, DOM_OBJECT_SLOT,
                  GetReservedSlot(source, DOM_OBJECT_SLOT));
  SetReservedSlot(source, DOM_OBJECT_SLOT, JS::PrivateValue(nullptr));

  source = JS_TransplantObject(cx, source, target);
  if (!source) {
    return false;
  }

  RootedObject copyTo(cx);
  if (isProxy) {
    copyTo = TransplantableDOMProxyHandler::EnsureExpandoObject(cx, source);
    if (!copyTo) {
      return false;
    }
  } else {
    copyTo = source;
  }
  if (!JS_CopyPropertiesFrom(cx, copyTo, propertyHolder)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

static bool TransplantableObject(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject callee(cx, &args.callee());

  if (args.length() > 1) {
    ReportUsageErrorASCII(cx, callee, "Wrong number of arguments");
    return false;
  }

  bool createProxy = false;
  RootedObject source(cx);
  if (args.length() == 1 && !args[0].isUndefined()) {
    if (!args[0].isObject()) {
      ReportUsageErrorASCII(cx, callee, "Argument must be an object");
      return false;
    }

    RootedObject options(cx, &args[0].toObject());
    RootedValue value(cx);

    if (!JS_GetProperty(cx, options, "proxy", &value)) {
      return false;
    }
    createProxy = JS::ToBoolean(value);

    if (!JS_GetProperty(cx, options, "object", &value)) {
      return false;
    }
    if (!value.isUndefined()) {
      if (!value.isObject()) {
        ReportUsageErrorASCII(cx, callee, "'object' option must be an object");
        return false;
      }

      source = &value.toObject();
      if (JS_GetClass(source) != GetDomClass()) {
        ReportUsageErrorASCII(cx, callee, "Object not a FakeDOMObject");
        return false;
      }

      // |source| must be a tenured object to be transplantable.
      if (gc::IsInsideNursery(source)) {
        JS_GC(cx);

        MOZ_ASSERT(!gc::IsInsideNursery(source),
                   "Live objects should be tenured after one GC, because "
                   "the nursery has only a single generation");
      }
    }
  }

  if (!source) {
    if (!createProxy) {
      source = NewBuiltinClassInstance(cx, &TransplantableDOMObjectClass,
                                       TenuredObject);
      if (!source) {
        return false;
      }

      SetReservedSlot(source, DOM_OBJECT_SLOT, JS::PrivateValue(nullptr));
    } else {
      JSObject* expando = JS_NewPlainObject(cx);
      if (!expando) {
        return false;
      }
      RootedValue expandoVal(cx, ObjectValue(*expando));

      ProxyOptions options;
      options.setClass(&TransplantableDOMProxyObjectClass);
      options.setLazyProto(true);

      source = NewProxyObject(cx, &TransplantableDOMProxyHandler::singleton,
                              expandoVal, nullptr, options);
      if (!source) {
        return false;
      }

      SetProxyReservedSlot(source, DOM_OBJECT_SLOT, JS::PrivateValue(nullptr));
    }
  }

  jsid emptyId = NameToId(cx->names().empty);
  RootedObject transplant(
      cx, NewFunctionByIdWithReserved(cx, TransplantObject, 0, 0, emptyId));
  if (!transplant) {
    return false;
  }

  SetFunctionNativeReserved(transplant, TransplantSourceObject,
                            ObjectValue(*source));

  RootedObject result(cx, JS_NewPlainObject(cx));
  if (!result) {
    return false;
  }

  RootedValue sourceVal(cx, ObjectValue(*source));
  RootedValue transplantVal(cx, ObjectValue(*transplant));
  if (!JS_DefineProperty(cx, result, "object", sourceVal, 0) ||
      !JS_DefineProperty(cx, result, "transplant", transplantVal, 0)) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

// clang-format off
static const JSFunctionSpecWithHelp shell_functions[] = {
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
"      skipFileNameValidation: skip the filename-validation callback\n"
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
"          script source should not be cached by the JS engine and should be \n"
"          lazily loaded from the embedding as-needed.\n"
"      loadBytecode: if true, and if the source is a CacheEntryObject,\n"
"         the bytecode would be loaded and decoded from the cache entry instead\n"
"         of being parsed, then it would be executed as usual.\n"
"      saveBytecode: if true, and if the source is a CacheEntryObject,\n"
"         the bytecode would be encoded and saved into the cache entry after\n"
"         the script execution.\n"
"      assertEqBytecode: if true, and if both loadBytecode and saveBytecode are \n"
"         true, then the loaded bytecode and the encoded bytecode are compared.\n"
"         and an assertion is raised if they differ.\n"
"      envChainObject: object to put on the scope chain, with its fields added\n"
"         as var bindings, akin to how elements are added to the environment in\n"
"         event handlers in Gecko.\n"
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
"help([function or interface object or /pattern/])",
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

    JS_FN_HELP("createErrorReport", CreateErrorReport, 1, 0,
"createErrorReport(value)",
"  Create an js::ErrorReport object from the given value and serialize\n"
"  to an object."),

#if defined(DEBUG) || defined(JS_JITSPEW)
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

    JS_FN_HELP("getSharedObject", GetSharedObject, 0, 0,
"getSharedObject()",
"  Retrieve the shared object from the cross-worker mailbox.\n"
"  The object retrieved may not be identical to the object that was\n"
"  installed, but it references the same shared memory.\n"
"  getSharedObject performs an ordering memory barrier.\n"),

    JS_FN_HELP("setSharedObject", SetSharedObject, 0, 0,
"setSharedObject(obj)",
"  Install the shared object in the cross-worker mailbox.  The object\n"
"  may be null.  setSharedObject performs an ordering memory barrier.\n"),

    JS_FN_HELP("getSharedArrayBuffer", GetSharedObject, 0, 0,
"getSharedArrayBuffer()",
"  Obsolete alias for getSharedObject().\n"),

    JS_FN_HELP("setSharedArrayBuffer", SetSharedObject, 0, 0,
"setSharedArrayBuffer(obj)",
"  Obsolete alias for setSharedObject(obj).\n"),

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

    JS_FN_HELP("codeModule", CodeModule, 1, 0,
"codeModule(module)",
"   Takes an uninstantiated ModuleObject and returns a XDR bytecode representation of that ModuleObject."),

    JS_FN_HELP("decodeModule", DecodeModule, 1, 0,
"decodeModule(code)",
"   Takes a XDR bytecode representation of an uninstantiated ModuleObject and returns a ModuleObject."),

    JS_FN_HELP("setModuleLoadHook", SetModuleLoadHook, 1, 0,
"setModuleLoadHook(function(path))",
"  Set the shell specific module load hook to |function|.\n"
"  This hook is used to load a module graph.  It should be implemented by the\n"
"  module loader."),

    JS_FN_HELP("setModuleResolveHook", SetModuleResolveHook, 1, 0,
"setModuleResolveHook(function(referrer, specifier))",
"  Set the HostResolveImportedModule hook to |function|.\n"
"  This hook is used to look up a previously loaded module object.  It should\n"
"  be implemented by the module loader."),

    JS_FN_HELP("setModuleMetadataHook", SetModuleMetadataHook, 1, 0,
"setModuleMetadataHook(function(module) {})",
"  Set the HostPopulateImportMeta hook to |function|.\n"
"  This hook is used to create the metadata object returned by import.meta for\n"
"  a module.  It should be implemented by the module loader."),

    JS_FN_HELP("setModuleDynamicImportHook", SetModuleDynamicImportHook, 1, 0,
"setModuleDynamicImportHook(function(referrer, specifier, promise))",
"  Set the HostImportModuleDynamically hook to |function|.\n"
"  This hook is used to dynamically import a module.  It should\n"
"  be implemented by the module loader."),

    JS_FN_HELP("finishDynamicModuleImport", FinishDynamicModuleImport, 3, 0,
"finishDynamicModuleImport(referrer, specifier, promise)",
"  The module loader's dynamic import hook should call this when the module has"
"  been loaded successfully."),

    JS_FN_HELP("abortDynamicModuleImport", AbortDynamicModuleImport, 4, 0,
"abortDynamicModuleImport(referrer, specifier, promise, error)",
"  The module loader's dynamic import hook should call this when the module "
"  import has failed."),

    JS_FN_HELP("setModulePrivate", ShellSetModulePrivate, 2, 0,
"setModulePrivate(scriptObject, privateValue)",
"  Associate a private value with a module object.\n"),

    JS_FN_HELP("getModulePrivate", ShellGetModulePrivate, 2, 0,
"getModulePrivate(scriptObject)",
"  Get the private value associated with a module object.\n"),

    JS_FN_HELP("getModuleLoadPath", GetModuleLoadPath, 0, 0,
"getModuleLoadPath()",
"  Return any --module-load-path argument passed to the shell.  Used by the\n"
"  module loader.\n"),

#if defined(JS_BUILD_BINAST)

JS_FN_HELP("parseBin", BinParse, 1, 0,
"parseBin(arraybuffer, [options])",
"  Parses a Binary AST, potentially throwing. If present, |options| may\n"
"  have properties saying how the passed |arraybuffer| should be handled:\n"
"      format: the format of the BinAST file\n"
"        (\"multipart\" or \"context\")"),

#endif // defined(JS_BUILD_BINAST)

    JS_FN_HELP("parse", Parse, 1, 0,
"parse(code)",
"  Parses a string, potentially throwing."),

    JS_FN_HELP("syntaxParse", SyntaxParse, 1, 0,
"syntaxParse(code)",
"  Check the syntax of a string, returning success value"),

    JS_FN_HELP("offThreadCompileScript", OffThreadCompileScript, 1, 0,
"offThreadCompileScript(code[, options])",
"  Compile |code| on a helper thread, returning a job ID.\n"
"  To wait for the compilation to finish and run the code, call\n"
"  |runOffThreadScript| passing the job ID. If present, |options| may\n"
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
"         Debugger.Source.prototype.elementAttributeName returns."),

    JS_FN_HELP("runOffThreadScript", runOffThreadScript, 0, 0,
"runOffThreadScript([jobID])",
"  Wait for an off-thread compilation job to complete. The job ID can be\n"
"  ommitted if there is only one job pending. If an error occurred,\n"
"  throw the appropriate exception; otherwise, run the script and return\n"
"  its value."),

    JS_FN_HELP("offThreadCompileModule", OffThreadCompileModule, 1, 0,
"offThreadCompileModule(code)",
"  Compile |code| on a helper thread, returning a job ID. To wait for the\n"
"  compilation to finish and and get the module record object call\n"
"  |finishOffThreadModule| passing the job ID."),

    JS_FN_HELP("finishOffThreadModule", FinishOffThreadModule, 0, 0,
"finishOffThreadModule([jobID])",
"  Wait for an off-thread compilation job to complete. The job ID can be\n"
"  ommitted if there is only one job pending. If an error occurred,\n"
"  throw the appropriate exception; otherwise, return the module record object."),

    JS_FN_HELP("offThreadDecodeScript", OffThreadDecodeScript, 1, 0,
"offThreadDecodeScript(cacheEntry[, options])",
"  Decode |code| on a helper thread, returning a job ID. To wait for the\n"
"  decoding to finish and run the code, call |runOffThreadDecodeScript| passing\n"
"  the job ID. If present, |options| may have properties saying how the code\n"
"  should be compiled (see also offThreadCompileScript)."),

    JS_FN_HELP("runOffThreadDecodedScript", runOffThreadDecodedScript, 0, 0,
"runOffThreadDecodedScript([jobID])",
"  Wait for off-thread decoding to complete. The job ID can be ommitted if there\n"
"  is only one job pending. If an error occurred, throw the appropriate\n"
"  exception; otherwise, run the script and return its value."),

    JS_FN_HELP("timeout", Timeout, 1, 0,
"timeout([seconds], [func])",
"  Get/Set the limit in seconds for the execution time for the current context.\n"
"  When the timeout expires the current interrupt callback is invoked.\n"
"  The timeout is used just once.  If the callback returns a falsy value, the\n"
"  script is aborted.  A negative value for seconds (this is the default) cancels\n"
"  any pending timeout.\n"
"  If a second argument is provided, it is installed as the interrupt handler,\n"
"  exactly as if by |setInterruptCallback|.\n"),

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
"  Calling this function will replace any callback set by |timeout|.\n"
"  If the callback returns a falsy value, the script is aborted.\n"),

    JS_FN_HELP("setJitCompilerOption", SetJitCompilerOption, 2, 0,
"setJitCompilerOption(<option>, <number>)",
"  Set a compiler option indexed in JSCompileOption enum to a number.\n"),

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

    JS_FN_HELP("valueToSource", ValueToSource, 1, 0,
"valueToSource(value)",
"  Format a value for inspection."),

    JS_FN_HELP("thisFilename", ThisFilename, 0, 0,
"thisFilename()",
"  Return the filename of the current script"),

    JS_FN_HELP("newGlobal", NewGlobal, 1, 0,
"newGlobal([options])",
"  Return a new global object/realm. The new global is created in the\n"
"  'newGlobal' function object's compartment and zone, unless the\n"
"  '--more-compartments' command-line flag was given, in which case new\n"
"  globals get a fresh compartment and zone. If options is given, it may\n"
"  have any of the following properties:\n"
"      sameCompartmentAs: If an object, the global will be in the same\n"
"         compartment and zone as the given object.\n"
"      sameZoneAs: The global will be in a new compartment in the same zone\n"
"         as the given object.\n"
"      newCompartment: If true, the global will always be created in a new\n"
"         compartment and zone.\n"
"      cloneSingletons: If true, always clone the objects baked into\n"
"         scripts, even if it's a top-level script that will only run once\n"
"         (defaults to using them directly in scripts that will only run\n"
"         once).\n"
"      invisibleToDebugger: If true, the global will be invisible to the\n"
"         debugger (default false)\n"
"      disableLazyParsing: If true, don't create lazy scripts for functions\n"
"         (default false).\n"
"      useWindowProxy: the global will be created with a WindowProxy attached. In this\n"
"          case, the WindowProxy will be returned.\n"
"      principal: if present, its value converted to a number must be an\n"
"         integer that fits in 32 bits; use that as the new realm's\n"
"         principal. Shell principals are toys, meant only for testing; one\n"
"         shell principal subsumes another if its set bits are a superset of\n"
"         the other's. Thus, a principal of 0 subsumes nothing, while a\n"
"         principals of ~0 subsumes all other principals. The absence of a\n"
"         principal is treated as if its bits were 0xffff, for subsumption\n"
"         purposes. If this property is omitted, supply no principal.\n"
"      systemPrincipal: If true, use the shell's trusted principals for the\n"
"         new realm. This creates a realm that's marked as a 'system' realm."),

    JS_FN_HELP("nukeCCW", NukeCCW, 1, 0,
"nukeCCW(wrapper)",
"  Nuke a CrossCompartmentWrapper, which turns it into a DeadProxyObject."),

    JS_FN_HELP("nukeAllCCWs", NukeAllCCWs, 0, 0,
"nukeAllCCWs()",
"  Like nukeCCW, but for all CrossCompartmentWrappers targeting the current realm."),

    JS_FN_HELP("recomputeWrappers", RecomputeWrappers, 2, 0,
"recomputeWrappers([src, [target]])",
"  Recompute all cross-compartment wrappers. src and target are both optional\n"
"  and can be used to filter source or target compartments: the unwrapped\n"
"  object's compartment is used as CompartmentFilter.\n"),

    JS_FN_HELP("dumpObjectWrappers", DumpObjectWrappers, 2, 0,
"dumpObjectWrappers()",
"  Print information about cross-compartment object wrappers.\n"),

    JS_FN_HELP("wrapWithProto", WrapWithProto, 2, 0,
"wrapWithProto(obj)",
"  Wrap an object into a noop wrapper with prototype semantics."),

    JS_FN_HELP("createMappedArrayBuffer", CreateMappedArrayBuffer, 1, 0,
"createMappedArrayBuffer(filename, [offset, [size]])",
"  Create an array buffer that mmaps the given file."),

    JS_FN_HELP("addPromiseReactions", AddPromiseReactions, 3, 0,
"addPromiseReactions(promise, onResolve, onReject)",
"  Calls the JS::AddPromiseReactions JSAPI function with the given arguments."),

    JS_FN_HELP("ignoreUnhandledRejections", IgnoreUnhandledRejections, 0, 0,
"ignoreUnhandledRejections()",
"  By default, js shell tracks unhandled promise rejections and reports\n"
"  them at the end of the exectuion.  If a testcase isn't interested\n"
"  in those rejections, call this to stop tracking and reporting."),

    JS_FN_HELP("getMaxArgs", GetMaxArgs, 0, 0,
"getMaxArgs()",
"  Return the maximum number of supported args for a call."),

    JS_FN_HELP("createIsHTMLDDA", CreateIsHTMLDDA, 0, 0,
"createIsHTMLDDA()",
"  Return an object |obj| that \"looks like\" the |document.all| object in\n"
"  browsers in certain ways: |typeof obj === \"undefined\"|, |obj == null|\n"
"  and |obj == undefined| (vice versa for !=), |ToBoolean(obj) === false|,\n"
"  and when called with no arguments or the single argument \"\" returns\n"
"  null.  (Calling |obj| any other way crashes or throws an exception.)\n"
"  This function implements the exact requirements of the $262.IsHTMLDDA\n"
"  property in test262."),

    JS_FN_HELP("cacheEntry", CacheEntry, 1, 0,
"cacheEntry(code)",
"  Return a new opaque object which emulates a cache entry of a script.  This\n"
"  object encapsulates the code and its cached content. The cache entry is filled\n"
"  and read by the \"evaluate\" function by using it in place of the source, and\n"
"  by setting \"saveBytecode\" and \"loadBytecode\" options."),

    JS_FN_HELP("streamCacheEntry", StreamCacheEntryObject::construct, 1, 0,
"streamCacheEntry(buffer)",
"  Create a shell-only object that holds wasm bytecode and can be streaming-\n"
"  compiled and cached by WebAssembly.{compile,instantiate}Streaming(). On a\n"
"  second compilation of the same cache entry, the cached code will be used."),

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

    JS_FN_HELP("hasCopyOnWriteElements", HasCopyOnWriteElements, 1, 0,
"hasCopyOnWriteElements(o)",
"  Return true iff the object has copy-on-write dense elements."),

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

    JS_FN_HELP("enqueueJob", EnqueueJob, 1, 0,
"enqueueJob(fn)",
"  Enqueue 'fn' on the shell's job queue."),

    JS_FN_HELP("globalOfFirstJobInQueue", GlobalOfFirstJobInQueue, 0, 0,
"globalOfFirstJobInQueue()",
"  Returns the global of the first item in the job queue. Throws an exception\n"
"  if the queue is empty.\n"),

    JS_FN_HELP("drainJobQueue", DrainJobQueue, 0, 0,
"drainJobQueue()",
"Take jobs from the shell's job queue in FIFO order and run them until the\n"
"queue is empty.\n"),

    JS_FN_HELP("setPromiseRejectionTrackerCallback", SetPromiseRejectionTrackerCallback, 1, 0,
"setPromiseRejectionTrackerCallback()",
"Sets the callback to be invoked whenever a Promise rejection is unhandled\n"
"or a previously-unhandled rejection becomes handled."),

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

    JS_FN_HELP("bindToAsyncStack", BindToAsyncStack, 2, 0,
"bindToAsyncStack(fn, { stack, cause, explicit })",
"  Returns a new function that calls 'fn' with no arguments, passing\n"
"  'undefined' as the 'this' value, and supplies an async stack for the\n"
"  call as described by the second argument, an object with the following\n"
"  properties (which are not optional, unless specified otherwise):\n"
"\n"
"  stack:    A SavedFrame object, like that returned by 'saveStack'. Stacks\n"
"            captured during calls to the returned function capture this as\n"
"            their async stack parent, accessible via a SavedFrame's\n"
"            'asyncParent' property.\n"
"\n"
"  cause:    A string, supplied as the async cause on the top frame of\n"
"            captured async stacks.\n"
"\n"
"  explicit: A boolean value, indicating whether the given 'stack' should\n"
"            always supplant the returned function's true callers (true),\n"
"            or only when there are no other JavaScript frames on the stack\n"
"            below it (false). If omitted, this is treated as 'true'."),

#ifdef JS_HAS_INTL_API
    JS_FN_HELP("addIntlExtras", AddIntlExtras, 1, 0,
"addIntlExtras(obj)",
"Adds various not-yet-standardized Intl functions as properties on the\n"
"provided object (this should generally be Intl itself).  The added\n"
"functions and their behavior are experimental: don't depend upon them\n"
"unless you're willing to update your code if these experimental APIs change\n"
"underneath you."),
#endif // JS_HAS_INTL_API

    JS_FN_HELP("wasmCompileInSeparateProcess", WasmCompileInSeparateProcess, 1, 0,
"wasmCompileInSeparateProcess(buffer)",
"  Compile the given buffer in a separate process, serialize the resulting\n"
"  wasm::Module into bytes, and deserialize those bytes in the current\n"
"  process, returning the resulting WebAssembly.Module."),

    JS_FN_HELP("transplantableObject", TransplantableObject, 0, 0,
"transplantableObject([options])",
"  Returns the pair {object, transplant}. |object| is an object which can be\n"
"  transplanted into a new object when the |transplant| function, which must\n"
"  be invoked with a global object, is called.\n"
"  |object| is swapped with a cross-compartment wrapper if the global object\n"
"  is in a different compartment.\n"
"\n"
"  If options is given, it may have any of the following properties:\n"
"    proxy: Create a DOM Proxy object instead of a plain DOM object.\n"
"    object: Don't create a new DOM object, but instead use the supplied\n"
"            FakeDOMObject."),

    JS_FN_HELP("cpuNow", CpuNow, /* nargs= */ 0, /* flags = */ 0,
"cpuNow()",
" Returns the approximate processor time used by the process since an arbitrary epoch, in seconds.\n"
" Only the difference between two calls to `cpuNow()` is meaningful."),

    JS_FS_HELP_END
};
// clang-format on

// clang-format off
static const JSFunctionSpecWithHelp fuzzing_unsafe_functions[] = {
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

    JS_FN_HELP("crash", Crash, 0, 0,
"crash([message, [{disable_minidump:true}]])",
"  Crashes the process with a MOZ_CRASH, optionally providing a message.\n"
"  An options object may be passed as the second argument. If the key\n"
"  'suppress_minidump' is set to true, then a minidump will not be\n"
"  generated by the crash (which only has an effect if the breakpad\n"
"  dumping library is loaded.)"),

    JS_FN_HELP("setARMHwCapFlags", SetARMHwCapFlags, 1, 0,
"setARMHwCapFlags(\"flag1,flag2 flag3\")",
"  On non-ARM, no-op. On ARM, set the hardware capabilities. The list of \n"
"  flags is available by calling this function with \"help\" as the flag's name"),

    JS_FN_HELP("wasmLoop", WasmLoop, 2, 0,
"wasmLoop(filename, imports)",
"  Performs an AFL-style persistent loop reading data from the given file and passing it\n"
"  to the 'wasmEval' function together with the specified imports object."),

    JS_FN_HELP("setBufferStreamParams", SetBufferStreamParams, 2, 0,
"setBufferStreamParams(delayMillis, chunkByteSize)",
"  Set the delay time (between calls to StreamConsumer::consumeChunk) and chunk\n"
"  size (in bytes)."),

    JS_FS_HELP_END
};
// clang-format on

// clang-format off
static const JSFunctionSpecWithHelp performance_functions[] = {
    JS_FN_HELP("now", Now, 0, 0,
"now()",
"  Return the current time with sub-ms precision.\n"
"  This function is an alias of the dateNow() function."),
    JS_FS_HELP_END
};
// clang-format on

// clang-format off
static const JSFunctionSpecWithHelp console_functions[] = {
    JS_FN_HELP("log", Print, 0, 0,
"log([exp ...])",
"  Evaluate and print expressions to stdout.\n"
"  This function is an alias of the print() function."),
    JS_FS_HELP_END
};
// clang-format on

bool DefineConsole(JSContext* cx, HandleObject global) {
  RootedObject obj(cx, JS_NewPlainObject(cx));
  return obj && JS_DefineFunctionsWithHelp(cx, obj, console_functions) &&
         JS_DefineProperty(cx, global, "console", obj, 0);
}

#ifdef MOZ_PROFILING
#  define PROFILING_FUNCTION_COUNT 5
#  ifdef MOZ_CALLGRIND
#    define CALLGRIND_FUNCTION_COUNT 3
#  else
#    define CALLGRIND_FUNCTION_COUNT 0
#  endif
#  ifdef MOZ_VTUNE
#    define VTUNE_FUNCTION_COUNT 4
#  else
#    define VTUNE_FUNCTION_COUNT 0
#  endif
#  define EXTERNAL_FUNCTION_COUNT \
    (PROFILING_FUNCTION_COUNT + CALLGRIND_FUNCTION_COUNT + VTUNE_FUNCTION_COUNT)
#else
#  define EXTERNAL_FUNCTION_COUNT 0
#endif

#undef PROFILING_FUNCTION_COUNT
#undef CALLGRIND_FUNCTION_COUNT
#undef VTUNE_FUNCTION_COUNT
#undef EXTERNAL_FUNCTION_COUNT

static bool PrintHelpString(JSContext* cx, HandleValue v) {
  RootedString str(cx, v.toString());
  MOZ_ASSERT(gOutFile->isOpen());

  UniqueChars bytes = JS_EncodeStringToUTF8(cx, str);
  if (!bytes) {
    return false;
  }

  fprintf(gOutFile->fp, "%s\n", bytes.get());
  return true;
}

static bool PrintHelp(JSContext* cx, HandleObject obj) {
  RootedValue usage(cx);
  if (!JS_GetProperty(cx, obj, "usage", &usage)) {
    return false;
  }
  RootedValue help(cx);
  if (!JS_GetProperty(cx, obj, "help", &help)) {
    return false;
  }

  if (!usage.isString() || !help.isString()) {
    return true;
  }

  return PrintHelpString(cx, usage) && PrintHelpString(cx, help);
}

static bool PrintEnumeratedHelp(JSContext* cx, HandleObject obj,
                                HandleObject pattern, bool brief) {
  RootedIdVector idv(cx);
  if (!GetPropertyKeys(cx, obj, JSITER_OWNONLY | JSITER_HIDDEN, &idv)) {
    return false;
  }

  Rooted<RegExpObject*> regex(cx);
  if (pattern) {
    regex = &UncheckedUnwrap(pattern)->as<RegExpObject>();
  }

  for (size_t i = 0; i < idv.length(); i++) {
    RootedValue v(cx);
    RootedId id(cx, idv[i]);
    if (!JS_GetPropertyById(cx, obj, id, &v)) {
      return false;
    }
    if (!v.isObject()) {
      continue;
    }

    RootedObject funcObj(cx, &v.toObject());
    if (regex) {
      // Only pay attention to objects with a 'help' property, which will
      // either be documented functions or interface objects.
      if (!JS_GetProperty(cx, funcObj, "help", &v)) {
        return false;
      }
      if (!v.isString()) {
        continue;
      }

      // For functions, match against the name. For interface objects,
      // match against the usage string.
      if (!JS_GetProperty(cx, funcObj, "name", &v)) {
        return false;
      }
      if (!v.isString()) {
        if (!JS_GetProperty(cx, funcObj, "usage", &v)) {
          return false;
        }
        if (!v.isString()) {
          continue;
        }
      }

      size_t ignored = 0;
      if (!JSString::ensureLinear(cx, v.toString())) {
        return false;
      }
      RootedLinearString input(cx, &v.toString()->asLinear());
      if (!ExecuteRegExpLegacy(cx, nullptr, regex, input, &ignored, true, &v)) {
        return false;
      }
      if (v.isNull()) {
        continue;
      }
    }

    if (!PrintHelp(cx, funcObj)) {
      return false;
    }
  }

  return true;
}

static bool Help(JSContext* cx, unsigned argc, Value* vp) {
  if (!gOutFile->isOpen()) {
    JS_ReportErrorASCII(cx, "output file is closed");
    return false;
  }

  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setUndefined();
  RootedObject global(cx, JS::CurrentGlobalOrNull(cx));

  // help() - display the version and dump out help for all functions on the
  // global.
  if (args.length() == 0) {
    fprintf(gOutFile->fp, "%s\n", JS_GetImplementationVersion());

    if (!PrintEnumeratedHelp(cx, global, nullptr, false)) {
      return false;
    }
    return true;
  }

  RootedValue v(cx);

  if (args[0].isPrimitive()) {
    // help("foo")
    JS_ReportErrorASCII(cx, "primitive arg");
    return false;
  }

  RootedObject obj(cx, &args[0].toObject());
  if (!obj) {
    return true;
  }
  bool isRegexp;
  if (!JS::ObjectIsRegExp(cx, obj, &isRegexp)) {
    return false;
  }

  if (isRegexp) {
    // help(/pattern/)
    return PrintEnumeratedHelp(cx, global, obj, false);
  }

  // help(function)
  // help(namespace_obj)
  return PrintHelp(cx, obj);
}

static const JSErrorFormatString jsShell_ErrorFormatString[JSShellErr_Limit] = {
#define MSG_DEF(name, count, exception, format) \
  {#name, format, count, JSEXN_ERR},
#include "jsshell.msg"
#undef MSG_DEF
};

const JSErrorFormatString* js::shell::my_GetErrorMessage(
    void* userRef, const unsigned errorNumber) {
  if (errorNumber == 0 || errorNumber >= JSShellErr_Limit) {
    return nullptr;
  }

  return &jsShell_ErrorFormatString[errorNumber];
}

static bool CreateLastWarningObject(JSContext* cx, JSErrorReport* report) {
  RootedObject warningObj(cx, JS_NewObject(cx, nullptr));
  if (!warningObj) {
    return false;
  }

  if (!CopyErrorReportToObject(cx, report, warningObj)) {
    return false;
  }

  GetShellContext(cx)->lastWarning.setObject(*warningObj);
  return true;
}

static FILE* ErrorFilePointer() {
  if (gErrFile->isOpen()) {
    return gErrFile->fp;
  }

  fprintf(stderr, "error file is closed; falling back to stderr\n");
  return stderr;
}

bool shell::PrintStackTrace(JSContext* cx, HandleObject stackObj) {
  if (!stackObj || !stackObj->is<SavedFrame>()) {
    return true;
  }

  JSPrincipals* principals = stackObj->nonCCWRealm()->principals();
  RootedString stackStr(cx);
  if (!BuildStackString(cx, principals, stackObj, &stackStr, 2)) {
    return false;
  }

  UniqueChars stack = JS_EncodeStringToUTF8(cx, stackStr);
  if (!stack) {
    return false;
  }

  FILE* fp = ErrorFilePointer();
  fputs("Stack:\n", fp);
  fputs(stack.get(), fp);

  return true;
}

js::shell::AutoReportException::~AutoReportException() {
  if (!JS_IsExceptionPending(cx)) {
    return;
  }

  // Get exception object and stack before printing and clearing exception.
  RootedValue exn(cx);
  (void)JS_GetPendingException(cx, &exn);
  RootedObject stack(cx, GetPendingExceptionStack(cx));

  JS_ClearPendingException(cx);

  ShellContext* sc = GetShellContext(cx);
  js::ErrorReport report(cx);
  if (!report.init(cx, exn, js::ErrorReport::WithSideEffects, stack)) {
    fprintf(stderr, "out of memory initializing ErrorReport\n");
    fflush(stderr);
    JS_ClearPendingException(cx);
    return;
  }

  MOZ_ASSERT(!report.report()->isWarning());

  FILE* fp = ErrorFilePointer();
  PrintError(cx, fp, report.toStringResult(), report.report(), reportWarnings);
  JS_ClearPendingException(cx);

  if (!PrintStackTrace(cx, stack)) {
    fputs("(Unable to print stack trace)\n", fp);
    JS_ClearPendingException(cx);
  }

#if defined(DEBUG) || defined(JS_OOM_BREAKPOINT)
  // Don't quit the shell if an unhandled exception is reported during OOM
  // testing.
  if (cx->runningOOMTest) {
    return;
  }
#endif

  if (report.report()->errorNumber == JSMSG_OUT_OF_MEMORY) {
    sc->exitCode = EXITCODE_OUT_OF_MEMORY;
  } else {
    sc->exitCode = EXITCODE_RUNTIME_ERROR;
  }
}

void js::shell::WarningReporter(JSContext* cx, JSErrorReport* report) {
  ShellContext* sc = GetShellContext(cx);
  FILE* fp = ErrorFilePointer();

  MOZ_ASSERT(report->isWarning());

  if (sc->lastWarningEnabled) {
    JS::AutoSaveExceptionState savedExc(cx);
    if (!CreateLastWarningObject(cx, report)) {
      fputs("Unhandled error happened while creating last warning object.\n",
            fp);
      fflush(fp);
    }
    savedExc.restore();
  }

  // Print the warning.
  PrintError(cx, fp, JS::ConstUTF8CharsZ(), report, reportWarnings);
}

static bool global_enumerate(JSContext* cx, JS::HandleObject obj,
                             JS::MutableHandleIdVector properties,
                             bool enumerableOnly) {
#ifdef LAZY_STANDARD_CLASSES
  return JS_NewEnumerateStandardClasses(cx, obj, properties, enumerableOnly);
#else
  return true;
#endif
}

static bool global_resolve(JSContext* cx, HandleObject obj, HandleId id,
                           bool* resolvedp) {
#ifdef LAZY_STANDARD_CLASSES
  if (!JS_ResolveStandardClass(cx, obj, id, resolvedp)) {
    return false;
  }
#endif
  return true;
}

static bool global_mayResolve(const JSAtomState& names, jsid id,
                              JSObject* maybeObj) {
  return JS_MayResolveStandardClass(names, id, maybeObj);
}

static const JSClassOps global_classOps = {
    nullptr,                   // addProperty
    nullptr,                   // delProperty
    nullptr,                   // enumerate
    global_enumerate,          // newEnumerate
    global_resolve,            // resolve
    global_mayResolve,         // mayResolve
    nullptr,                   // finalize
    nullptr,                   // call
    nullptr,                   // hasInstance
    nullptr,                   // construct
    JS_GlobalObjectTraceHook,  // trace
};

static constexpr uint32_t DOM_PROTOTYPE_SLOT = JSCLASS_GLOBAL_SLOT_COUNT;
static constexpr uint32_t DOM_GLOBAL_SLOTS = 1;

static const JSClass global_class = {
    "global",
    JSCLASS_GLOBAL_FLAGS | JSCLASS_GLOBAL_FLAGS_WITH_SLOTS(DOM_GLOBAL_SLOTS),
    &global_classOps};

/*
 * Define a FakeDOMObject constructor. It returns an object with a getter,
 * setter and method with attached JitInfo. This object can be used to test
 * IonMonkey DOM optimizations in the shell.
 */

/* Fow now just use to a constant we can check. */
static const void* DOM_PRIVATE_VALUE = (void*)0x1234;

static bool dom_genericGetter(JSContext* cx, unsigned argc, JS::Value* vp);

static bool dom_genericSetter(JSContext* cx, unsigned argc, JS::Value* vp);

static bool dom_genericMethod(JSContext* cx, unsigned argc, JS::Value* vp);

static bool dom_get_x(JSContext* cx, HandleObject obj, void* self,
                      JSJitGetterCallArgs args) {
  MOZ_ASSERT(JS_GetClass(obj) == GetDomClass());
  MOZ_ASSERT(self == DOM_PRIVATE_VALUE);
  args.rval().set(JS_NumberValue(double(3.14)));
  return true;
}

static bool dom_set_x(JSContext* cx, HandleObject obj, void* self,
                      JSJitSetterCallArgs args) {
  MOZ_ASSERT(JS_GetClass(obj) == GetDomClass());
  MOZ_ASSERT(self == DOM_PRIVATE_VALUE);
  return true;
}

static bool dom_get_global(JSContext* cx, HandleObject obj, void* self,
                           JSJitGetterCallArgs args) {
  MOZ_ASSERT(JS_GetClass(obj) == GetDomClass());
  MOZ_ASSERT(self == DOM_PRIVATE_VALUE);

  // Return the current global (instead of obj->global()) to test cx->realm
  // switching in the JIT.
  args.rval().setObject(*ToWindowProxyIfWindow(cx->global()));

  return true;
}

static bool dom_set_global(JSContext* cx, HandleObject obj, void* self,
                           JSJitSetterCallArgs args) {
  MOZ_ASSERT(JS_GetClass(obj) == GetDomClass());
  MOZ_ASSERT(self == DOM_PRIVATE_VALUE);

  // Throw an exception if our argument is not the current global. This lets
  // us test cx->realm switching.
  if (!args[0].isObject() ||
      ToWindowIfWindowProxy(&args[0].toObject()) != cx->global()) {
    JS_ReportErrorASCII(cx, "Setter not called with matching global argument");
    return false;
  }

  return true;
}

static bool dom_doFoo(JSContext* cx, HandleObject obj, void* self,
                      const JSJitMethodCallArgs& args) {
  MOZ_ASSERT(JS_GetClass(obj) == GetDomClass());
  MOZ_ASSERT(self == DOM_PRIVATE_VALUE);
  MOZ_ASSERT(cx->realm() == args.callee().as<JSFunction>().realm());

  /* Just return args.length(). */
  args.rval().setInt32(args.length());
  return true;
}

static const JSJitInfo dom_x_getterinfo = {
    {(JSJitGetterOp)dom_get_x},
    {0}, /* protoID */
    {0}, /* depth */
    JSJitInfo::Getter,
    JSJitInfo::AliasNone, /* aliasSet */
    JSVAL_TYPE_UNKNOWN,   /* returnType */
    true,                 /* isInfallible. False in setters. */
    true,                 /* isMovable */
    true,                 /* isEliminatable */
    false,                /* isAlwaysInSlot */
    false,                /* isLazilyCachedInSlot */
    false,                /* isTypedMethod */
    0                     /* slotIndex */
};

static const JSJitInfo dom_x_setterinfo = {
    {(JSJitGetterOp)dom_set_x},
    {0}, /* protoID */
    {0}, /* depth */
    JSJitInfo::Setter,
    JSJitInfo::AliasEverything, /* aliasSet */
    JSVAL_TYPE_UNKNOWN,         /* returnType */
    false,                      /* isInfallible. False in setters. */
    false,                      /* isMovable. */
    false,                      /* isEliminatable. */
    false,                      /* isAlwaysInSlot */
    false,                      /* isLazilyCachedInSlot */
    false,                      /* isTypedMethod */
    0                           /* slotIndex */
};

// Note: this getter uses AliasEverything and is marked as fallible and
// non-movable (1) to prevent Ion from getting too clever optimizing it and
// (2) it's nice to have a few different kinds of getters in the shell.
static const JSJitInfo dom_global_getterinfo = {
    {(JSJitGetterOp)dom_get_global},
    {0}, /* protoID */
    {0}, /* depth */
    JSJitInfo::Getter,
    JSJitInfo::AliasEverything, /* aliasSet */
    JSVAL_TYPE_OBJECT,          /* returnType */
    false,                      /* isInfallible. False in setters. */
    false,                      /* isMovable */
    false,                      /* isEliminatable */
    false,                      /* isAlwaysInSlot */
    false,                      /* isLazilyCachedInSlot */
    false,                      /* isTypedMethod */
    0                           /* slotIndex */
};

static const JSJitInfo dom_global_setterinfo = {
    {(JSJitGetterOp)dom_set_global},
    {0}, /* protoID */
    {0}, /* depth */
    JSJitInfo::Setter,
    JSJitInfo::AliasEverything, /* aliasSet */
    JSVAL_TYPE_UNKNOWN,         /* returnType */
    false,                      /* isInfallible. False in setters. */
    false,                      /* isMovable. */
    false,                      /* isEliminatable. */
    false,                      /* isAlwaysInSlot */
    false,                      /* isLazilyCachedInSlot */
    false,                      /* isTypedMethod */
    0                           /* slotIndex */
};

static const JSJitInfo doFoo_methodinfo = {
    {(JSJitGetterOp)dom_doFoo},
    {0}, /* protoID */
    {0}, /* depth */
    JSJitInfo::Method,
    JSJitInfo::AliasEverything, /* aliasSet */
    JSVAL_TYPE_UNKNOWN,         /* returnType */
    false,                      /* isInfallible. False in setters. */
    false,                      /* isMovable */
    false,                      /* isEliminatable */
    false,                      /* isAlwaysInSlot */
    false,                      /* isLazilyCachedInSlot */
    false,                      /* isTypedMethod */
    0                           /* slotIndex */
};

static const JSPropertySpec dom_props[] = {
    JSPropertySpec::nativeAccessors("x", JSPROP_ENUMERATE, dom_genericGetter,
                                    &dom_x_getterinfo, dom_genericSetter,
                                    &dom_x_setterinfo),
    JSPropertySpec::nativeAccessors("global", JSPROP_ENUMERATE,
                                    dom_genericGetter, &dom_global_getterinfo,
                                    dom_genericSetter, &dom_global_setterinfo),
    JS_PS_END};

static const JSFunctionSpec dom_methods[] = {
    JS_FNINFO("doFoo", dom_genericMethod, &doFoo_methodinfo, 3,
              JSPROP_ENUMERATE),
    JS_FS_END};

static const JSClass dom_class = {
    "FakeDOMObject", JSCLASS_IS_DOMJSCLASS | JSCLASS_HAS_RESERVED_SLOTS(2)};

static const JSClass* GetDomClass() { return &dom_class; }

static bool dom_genericGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!args.thisv().isObject()) {
    args.rval().setUndefined();
    return true;
  }

  RootedObject obj(cx, &args.thisv().toObject());
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

static bool dom_genericSetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() < 1 || !args.thisv().isObject()) {
    args.rval().setUndefined();
    return true;
  }

  RootedObject obj(cx, &args.thisv().toObject());
  if (JS_GetClass(obj) != &dom_class) {
    args.rval().set(UndefinedValue());
    return true;
  }

  JS::Value val = js::GetReservedSlot(obj, DOM_OBJECT_SLOT);

  const JSJitInfo* info = FUNCTION_VALUE_TO_JITINFO(args.calleev());
  MOZ_ASSERT(info->type() == JSJitInfo::Setter);
  JSJitSetterOp setter = info->setter;
  if (!setter(cx, obj, val.toPrivate(), JSJitSetterCallArgs(args))) {
    return false;
  }
  args.rval().set(UndefinedValue());
  return true;
}

static bool dom_genericMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!args.thisv().isObject()) {
    args.rval().setUndefined();
    return true;
  }

  RootedObject obj(cx, &args.thisv().toObject());
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

static void InitDOMObject(HandleObject obj) {
  SetReservedSlot(obj, DOM_OBJECT_SLOT,
                  PrivateValue(const_cast<void*>(DOM_PRIVATE_VALUE)));
}

static JSObject* GetDOMPrototype(JSContext* cx, JSObject* global) {
  MOZ_ASSERT(JS_IsGlobalObject(global));
  if (GetObjectClass(global) != &global_class) {
    JS_ReportErrorASCII(cx, "Can't get FakeDOMObject prototype in sandbox");
    return nullptr;
  }
  MOZ_ASSERT(GetReservedSlot(global, DOM_PROTOTYPE_SLOT).isObject());
  return &GetReservedSlot(global, DOM_PROTOTYPE_SLOT).toObject();
}

static bool dom_constructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  RootedObject callee(cx, &args.callee());
  RootedValue protov(cx);
  if (!GetProperty(cx, callee, callee, cx->names().prototype, &protov)) {
    return false;
  }

  if (!protov.isObject()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_BAD_PROTOTYPE,
                              "FakeDOMObject");
    return false;
  }

  RootedObject proto(cx, &protov.toObject());
  RootedObject domObj(cx, JS_NewObjectWithGivenProto(cx, &dom_class, proto));
  if (!domObj) {
    return false;
  }

  InitDOMObject(domObj);

  args.rval().setObject(*domObj);
  return true;
}

static bool InstanceClassHasProtoAtDepth(const JSClass* clasp, uint32_t protoID,
                                         uint32_t depth) {
  // Only the (fake) DOM object supports any JIT optimizations.
  return clasp == GetDomClass();
}

static bool ShellBuildId(JS::BuildIdCharVector* buildId) {
  // The browser embeds the date into the buildid and the buildid is embedded
  // in the binary, so every 'make' necessarily builds a new firefox binary.
  // Fortunately, the actual firefox executable is tiny -- all the code is in
  // libxul.so and other shared modules -- so this isn't a big deal. Not so
  // for the statically-linked JS shell. To avoid recompiling js.cpp and
  // re-linking 'js' on every 'make', we use a constant buildid and rely on
  // the shell user to manually clear any caches between cache-breaking updates.
  const char buildid[] = "JS-shell";
  return buildId->append(buildid, sizeof(buildid));
}

static bool TimesAccessed(JSContext* cx, unsigned argc, Value* vp) {
  static int32_t accessed = 0;
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setInt32(++accessed);
  return true;
}

static const JSPropertySpec TestingProperties[] = {
    JS_PSG("timesAccessed", TimesAccessed, 0), JS_PS_END};

static JSObject* NewGlobalObject(JSContext* cx, JS::RealmOptions& options,
                                 JSPrincipals* principals,
                                 ShellGlobalKind kind) {
  RootedObject glob(cx,
                    JS_NewGlobalObject(cx, &global_class, principals,
                                       JS::DontFireOnNewGlobalHook, options));
  if (!glob) {
    return nullptr;
  }

  {
    JSAutoRealm ar(cx, glob);

    if (kind == ShellGlobalKind::WindowProxy) {
      RootedObject proxy(cx, NewShellWindowProxy(cx, glob));
      if (!proxy) {
        return nullptr;
      }
      js::SetWindowProxy(cx, glob, proxy);
    }

#ifndef LAZY_STANDARD_CLASSES
    if (!JS::InitRealmStandardClasses(cx)) {
      return nullptr;
    }
#endif

    bool succeeded;
    if (!JS_SetImmutablePrototype(cx, glob, &succeeded)) {
      return nullptr;
    }
    MOZ_ASSERT(succeeded,
               "a fresh, unexposed global object is always capable of "
               "having its [[Prototype]] be immutable");

#ifdef JS_HAS_CTYPES
    if (!fuzzingSafe && !JS_InitCTypesClass(cx, glob)) {
      return nullptr;
    }
#endif
    if (!JS_InitReflectParse(cx, glob)) {
      return nullptr;
    }
    if (!JS_DefineDebuggerObject(cx, glob)) {
      return nullptr;
    }
    if (!JS_DefineFunctionsWithHelp(cx, glob, shell_functions) ||
        !JS_DefineProfilingFunctions(cx, glob)) {
      return nullptr;
    }
    if (!js::DefineTestingFunctions(cx, glob, fuzzingSafe,
                                    disableOOMFunctions)) {
      return nullptr;
    }
    if (!JS_DefineProperties(cx, glob, TestingProperties)) {
      return nullptr;
    }

    if (!fuzzingSafe) {
      if (!JS_DefineFunctionsWithHelp(cx, glob, fuzzing_unsafe_functions)) {
        return nullptr;
      }
      if (!DefineConsole(cx, glob)) {
        return nullptr;
      }
    }

    if (!DefineOS(cx, glob, fuzzingSafe, &gOutFile, &gErrFile)) {
      return nullptr;
    }

    RootedObject performanceObj(cx, JS_NewObject(cx, nullptr));
    if (!performanceObj) {
      return nullptr;
    }
    if (!JS_DefineFunctionsWithHelp(cx, performanceObj,
                                    performance_functions)) {
      return nullptr;
    }
    RootedObject mozMemoryObj(cx, JS_NewObject(cx, nullptr));
    if (!mozMemoryObj) {
      return nullptr;
    }
    RootedObject gcObj(cx, gc::NewMemoryInfoObject(cx));
    if (!gcObj) {
      return nullptr;
    }
    if (!JS_DefineProperty(cx, glob, "performance", performanceObj,
                           JSPROP_ENUMERATE)) {
      return nullptr;
    }
    if (!JS_DefineProperty(cx, performanceObj, "mozMemory", mozMemoryObj,
                           JSPROP_ENUMERATE)) {
      return nullptr;
    }
    if (!JS_DefineProperty(cx, mozMemoryObj, "gc", gcObj, JSPROP_ENUMERATE)) {
      return nullptr;
    }

    /* Initialize FakeDOMObject. */
    static const js::DOMCallbacks DOMcallbacks = {InstanceClassHasProtoAtDepth};
    SetDOMCallbacks(cx, &DOMcallbacks);

    RootedObject domProto(
        cx, JS_InitClass(cx, glob, nullptr, &dom_class, dom_constructor, 0,
                         dom_props, dom_methods, nullptr, nullptr));
    if (!domProto) {
      return nullptr;
    }

    // FakeDOMObject.prototype is the only DOM object which needs to retrieved
    // in the shell; store it directly instead of creating a separate layer
    // (ProtoAndIfaceCache) as done in the browser.
    SetReservedSlot(glob, DOM_PROTOTYPE_SLOT, ObjectValue(*domProto));

    /* Initialize FakeDOMObject.prototype */
    InitDOMObject(domProto);

    JS_FireOnNewGlobalObject(cx, glob);
  }

  return glob;
}

static bool BindScriptArgs(JSContext* cx, OptionParser* op) {
  AutoReportException are(cx);

  MultiStringRange msr = op->getMultiStringArg("scriptArgs");
  RootedObject scriptArgs(cx);
  scriptArgs = JS::NewArrayObject(cx, 0);
  if (!scriptArgs) {
    return false;
  }

  if (!JS_DefineProperty(cx, cx->global(), "scriptArgs", scriptArgs, 0)) {
    return false;
  }

  for (size_t i = 0; !msr.empty(); msr.popFront(), ++i) {
    const char* scriptArg = msr.front();
    JS::RootedString str(cx, JS_NewStringCopyZ(cx, scriptArg));
    if (!str || !JS_DefineElement(cx, scriptArgs, i, str, JSPROP_ENUMERATE)) {
      return false;
    }
  }

  const char* scriptPath = op->getStringArg("script");
  RootedValue scriptPathValue(cx);
  if (scriptPath) {
    RootedString scriptPathString(cx, JS_NewStringCopyZ(cx, scriptPath));
    if (!scriptPathString) {
      return false;
    }
    scriptPathValue = StringValue(scriptPathString);
  } else {
    scriptPathValue = UndefinedValue();
  }

  if (!JS_DefineProperty(cx, cx->global(), "scriptPath", scriptPathValue, 0)) {
    return false;
  }

  return true;
}

static bool OptionFailure(const char* option, const char* str) {
  fprintf(stderr, "Unrecognized option for %s: %s\n", option, str);
  return false;
}

static MOZ_MUST_USE bool ProcessArgs(JSContext* cx, OptionParser* op) {
  ShellContext* sc = GetShellContext(cx);

#ifdef JS_ENABLE_SMOOSH
  if (op->getBoolOption("smoosh")) {
    JS::ContextOptionsRef(cx).setTrySmoosh(true);
    js::frontend::InitSmoosh();
  }

  if (const char* filename = op->getStringOption("not-implemented-watchfile")) {
    FILE* out = fopen(filename, "a");
    MOZ_RELEASE_ASSERT(out);
    setbuf(out, nullptr);  // Make unbuffered
    cx->runtime()->parserWatcherFile.init(out);
    JS::ContextOptionsRef(cx).setTrackNotImplemented(true);
  }
#endif

  /* |scriptArgs| gets bound on the global before any code is run. */
  if (!BindScriptArgs(cx, op)) {
    return false;
  }

  MultiStringRange filePaths = op->getMultiStringOption('f');
  MultiStringRange utf16FilePaths = op->getMultiStringOption('u');
  MultiStringRange codeChunks = op->getMultiStringOption('e');
  MultiStringRange modulePaths = op->getMultiStringOption('m');
  MultiStringRange binASTPaths(nullptr, nullptr);
  FileKind binASTFileKind = FileBinASTMultipart;
#if defined(JS_BUILD_BINAST)
  binASTPaths = op->getMultiStringOption('B');
  if (const char* str = op->getStringOption("binast-format")) {
    if (strcmp(str, "multipart") == 0) {
      binASTFileKind = FileBinASTMultipart;
    } else if (strcmp(str, "context") == 0) {
      binASTFileKind = FileBinASTContext;
    } else {
      return OptionFailure("binast-format", str);
    }
  }
#endif  // JS_BUILD_BINAST

  if (filePaths.empty() && utf16FilePaths.empty() && codeChunks.empty() &&
      modulePaths.empty() && binASTPaths.empty() &&
      !op->getStringArg("script")) {
    // Always use the interactive shell when -i is used. Without -i we let
    // Process figure it out based on isatty.
    bool forceTTY = op->getBoolOption('i');
    return Process(cx, nullptr, forceTTY, FileScript);
  }

  if (const char* path = op->getStringOption("module-load-path")) {
    RootedString jspath(cx, JS_NewStringCopyZ(cx, path));
    if (!jspath) {
      return false;
    }

    JSString* absolutePath = js::shell::ResolvePath(cx, jspath, RootRelative);
    if (!absolutePath) {
      return false;
    }

    sc->moduleLoadPath = JS_EncodeStringToLatin1(cx, absolutePath);
  } else {
    sc->moduleLoadPath = js::shell::GetCWD();
  }

  if (!sc->moduleLoadPath) {
    return false;
  }

  if (!InitModuleLoader(cx)) {
    return false;
  }

  while (!filePaths.empty() || !utf16FilePaths.empty() || !codeChunks.empty() ||
         !modulePaths.empty() || !binASTPaths.empty()) {
    size_t fpArgno = filePaths.empty() ? SIZE_MAX : filePaths.argno();
    size_t ufpArgno =
        utf16FilePaths.empty() ? SIZE_MAX : utf16FilePaths.argno();
    size_t ccArgno = codeChunks.empty() ? SIZE_MAX : codeChunks.argno();
    size_t mpArgno = modulePaths.empty() ? SIZE_MAX : modulePaths.argno();
    size_t baArgno = binASTPaths.empty() ? SIZE_MAX : binASTPaths.argno();

    if (fpArgno < ufpArgno && fpArgno < ccArgno && fpArgno < mpArgno &&
        fpArgno < baArgno) {
      char* path = filePaths.front();
      if (!Process(cx, path, false, FileScript)) {
        return false;
      }

      filePaths.popFront();
      continue;
    }

    if (ufpArgno < fpArgno && ufpArgno < ccArgno && ufpArgno < mpArgno &&
        ufpArgno < baArgno) {
      char* path = utf16FilePaths.front();
      if (!Process(cx, path, false, FileScriptUtf16)) {
        return false;
      }

      utf16FilePaths.popFront();
      continue;
    }

    if (ccArgno < fpArgno && ccArgno < ufpArgno && ccArgno < mpArgno &&
        ccArgno < baArgno) {
      const char* code = codeChunks.front();

      JS::CompileOptions opts(cx);
      opts.setFileAndLine("-e", 1);

      JS::SourceText<Utf8Unit> srcBuf;
      if (!srcBuf.init(cx, code, strlen(code), JS::SourceOwnership::Borrowed)) {
        return false;
      }

      RootedValue rval(cx);
      if (!JS::Evaluate(cx, opts, srcBuf, &rval)) {
        return false;
      }

      codeChunks.popFront();
      if (sc->quitting) {
        break;
      }

      continue;
    }

    if (baArgno < fpArgno && baArgno < ufpArgno && baArgno < ccArgno &&
        baArgno < mpArgno) {
      char* path = binASTPaths.front();
      if (!Process(cx, path, false, binASTFileKind)) {
        return false;
      }

      binASTPaths.popFront();
      continue;
    }

    MOZ_ASSERT(mpArgno < fpArgno && mpArgno < ufpArgno && mpArgno < ccArgno &&
               mpArgno < baArgno);

    char* path = modulePaths.front();
    if (!Process(cx, path, false, FileModule)) {
      return false;
    }

    modulePaths.popFront();
  }

  if (sc->quitting) {
    return false;
  }

  /* The |script| argument is processed after all options. */
  if (const char* path = op->getStringArg("script")) {
    if (!Process(cx, path, false, FileScript)) {
      return false;
    }
  }

  if (op->getBoolOption('i')) {
    if (!Process(cx, nullptr, true, FileScript)) {
      return false;
    }
  }

  return true;
}

static bool SetContextOptions(JSContext* cx, const OptionParser& op) {
  enableAsmJS = !op.getBoolOption("no-asmjs");

  // Default values for wasm.
  enableWasm = true;
  enableWasmBaseline = true;
  enableWasmIon = true;
  if (const char* str = op.getStringOption("wasm-compiler")) {
    if (strcmp(str, "none") == 0) {
      enableWasm = false;
    } else if (strcmp(str, "baseline") == 0) {
      // Baseline is enabled by default.
      enableWasmIon = false;
    } else if (strcmp(str, "ion") == 0) {
      // Ion is enabled by default.
      enableWasmBaseline = false;
    } else if (strcmp(str, "cranelift") == 0) {
      enableWasmBaseline = false;
      enableWasmIon = false;
      enableWasmCranelift = true;
    } else if (strcmp(str, "baseline+ion") == 0) {
      // Default.
    } else if (strcmp(str, "baseline+cranelift") == 0) {
      // Baseline is enabled by default.
      enableWasmIon = false;
      enableWasmCranelift = true;
    } else {
      return OptionFailure("wasm-compiler", str);
    }
  }

#ifdef ENABLE_WASM_GC
  enableWasmGc = op.getBoolOption("wasm-gc");
#endif
#ifdef ENABLE_WASM_MULTI_VALUE
  enableWasmMultiValue = !op.getBoolOption("no-wasm-multi-value");
#endif
  enableWasmVerbose = op.getBoolOption("wasm-verbose");
  enableTestWasmAwaitTier2 = op.getBoolOption("test-wasm-await-tier2");
  enableAsyncStacks = !op.getBoolOption("no-async-stacks");
  enableStreams = !op.getBoolOption("no-streams");
  enableReadableByteStreams = op.getBoolOption("enable-readable-byte-streams");
  enableBYOBStreamReaders = op.getBoolOption("enable-byob-stream-readers");
  enableWritableStreams = op.getBoolOption("enable-writable-streams");
  enableReadableStreamPipeTo = op.getBoolOption("enable-readablestream-pipeto");
#ifdef ENABLE_WASM_BIGINT
  enableWasmBigInt = !op.getBoolOption("no-wasm-bigint");
#endif
  enableWeakRefs = op.getBoolOption("enable-weak-refs");
  enableToSource = !op.getBoolOption("disable-tosource");
  enablePropertyErrorMessageFix =
      !op.getBoolOption("disable-property-error-message-fix");

  JS::ContextOptionsRef(cx)
      .setAsmJS(enableAsmJS)
      .setWasm(enableWasm)
      .setWasmForTrustedPrinciples(enableWasm)
      .setWasmBaseline(enableWasmBaseline)
      .setWasmIon(enableWasmIon)
#ifdef ENABLE_WASM_CRANELIFT
      .setWasmCranelift(enableWasmCranelift)
#endif
#ifdef ENABLE_WASM_GC
      .setWasmGc(enableWasmGc)
#endif
#ifdef ENABLE_WASM_MULTI_VALUE
      .setWasmMultiValue(enableWasmMultiValue)
#endif
      .setWasmVerbose(enableWasmVerbose)
      .setTestWasmAwaitTier2(enableTestWasmAwaitTier2)
#ifdef ENABLE_WASM_BIGINT
      .setWasmBigIntEnabled(enableWasmBigInt)
#endif
      .setAsyncStack(enableAsyncStacks);

  if (op.getBoolOption("no-ion-for-main-context")) {
    JS::ContextOptionsRef(cx).setDisableIon();
  }

  if (const char* str = op.getStringOption("cache-ir-stubs")) {
    if (strcmp(str, "on") == 0) {
      jit::JitOptions.disableCacheIR = false;
    } else if (strcmp(str, "off") == 0) {
      jit::JitOptions.disableCacheIR = true;
    } else {
      return OptionFailure("cache-ir-stubs", str);
    }
  }

  if (const char* str = op.getStringOption("spectre-mitigations")) {
    if (strcmp(str, "on") == 0) {
      jit::JitOptions.spectreIndexMasking = true;
      jit::JitOptions.spectreObjectMitigationsBarriers = true;
      jit::JitOptions.spectreObjectMitigationsMisc = true;
      jit::JitOptions.spectreStringMitigations = true;
      jit::JitOptions.spectreValueMasking = true;
      jit::JitOptions.spectreJitToCxxCalls = true;
    } else if (strcmp(str, "off") == 0) {
      jit::JitOptions.spectreIndexMasking = false;
      jit::JitOptions.spectreObjectMitigationsBarriers = false;
      jit::JitOptions.spectreObjectMitigationsMisc = false;
      jit::JitOptions.spectreStringMitigations = false;
      jit::JitOptions.spectreValueMasking = false;
      jit::JitOptions.spectreJitToCxxCalls = false;
    } else {
      return OptionFailure("spectre-mitigations", str);
    }
  }

  if (const char* str = op.getStringOption("ion-scalar-replacement")) {
    if (strcmp(str, "on") == 0) {
      jit::JitOptions.disableScalarReplacement = false;
    } else if (strcmp(str, "off") == 0) {
      jit::JitOptions.disableScalarReplacement = true;
    } else {
      return OptionFailure("ion-scalar-replacement", str);
    }
  }

  if (op.getStringOption("ion-shared-stubs")) {
    // Dead option, preserved for now for potential fuzzer interaction.
  }

  if (const char* str = op.getStringOption("ion-gvn")) {
    if (strcmp(str, "off") == 0) {
      jit::JitOptions.disableGvn = true;
    } else if (strcmp(str, "on") != 0 && strcmp(str, "optimistic") != 0 &&
               strcmp(str, "pessimistic") != 0) {
      // We accept "pessimistic" and "optimistic" as synonyms for "on"
      // for backwards compatibility.
      return OptionFailure("ion-gvn", str);
    }
  }

  if (const char* str = op.getStringOption("ion-licm")) {
    if (strcmp(str, "on") == 0) {
      jit::JitOptions.disableLicm = false;
    } else if (strcmp(str, "off") == 0) {
      jit::JitOptions.disableLicm = true;
    } else {
      return OptionFailure("ion-licm", str);
    }
  }

  if (const char* str = op.getStringOption("ion-edgecase-analysis")) {
    if (strcmp(str, "on") == 0) {
      jit::JitOptions.disableEdgeCaseAnalysis = false;
    } else if (strcmp(str, "off") == 0) {
      jit::JitOptions.disableEdgeCaseAnalysis = true;
    } else {
      return OptionFailure("ion-edgecase-analysis", str);
    }
  }

  if (const char* str = op.getStringOption("ion-pgo")) {
    if (strcmp(str, "on") == 0) {
      jit::JitOptions.disablePgo = false;
    } else if (strcmp(str, "off") == 0) {
      jit::JitOptions.disablePgo = true;
    } else {
      return OptionFailure("ion-pgo", str);
    }
  }

  if (const char* str = op.getStringOption("ion-range-analysis")) {
    if (strcmp(str, "on") == 0) {
      jit::JitOptions.disableRangeAnalysis = false;
    } else if (strcmp(str, "off") == 0) {
      jit::JitOptions.disableRangeAnalysis = true;
    } else {
      return OptionFailure("ion-range-analysis", str);
    }
  }

  if (const char* str = op.getStringOption("ion-sink")) {
    if (strcmp(str, "on") == 0) {
      jit::JitOptions.disableSink = false;
    } else if (strcmp(str, "off") == 0) {
      jit::JitOptions.disableSink = true;
    } else {
      return OptionFailure("ion-sink", str);
    }
  }

  if (const char* str = op.getStringOption("ion-optimization-levels")) {
    if (strcmp(str, "on") == 0) {
      jit::JitOptions.disableOptimizationLevels = false;
    } else if (strcmp(str, "off") == 0) {
      jit::JitOptions.disableOptimizationLevels = true;
    } else {
      return OptionFailure("ion-optimization-levels", str);
    }
  }

  if (const char* str = op.getStringOption("ion-instruction-reordering")) {
    if (strcmp(str, "on") == 0) {
      jit::JitOptions.disableInstructionReordering = false;
    } else if (strcmp(str, "off") == 0) {
      jit::JitOptions.disableInstructionReordering = true;
    } else {
      return OptionFailure("ion-instruction-reordering", str);
    }
  }

  if (op.getBoolOption("ion-check-range-analysis")) {
    jit::JitOptions.checkRangeAnalysis = true;
  }

  if (op.getBoolOption("ion-extra-checks")) {
    jit::JitOptions.runExtraChecks = true;
  }

  if (const char* str = op.getStringOption("ion-inlining")) {
    if (strcmp(str, "on") == 0) {
      jit::JitOptions.disableInlining = false;
    } else if (strcmp(str, "off") == 0) {
      jit::JitOptions.disableInlining = true;
    } else {
      return OptionFailure("ion-inlining", str);
    }
  }

  if (const char* str = op.getStringOption("ion-osr")) {
    if (strcmp(str, "on") == 0) {
      jit::JitOptions.osr = true;
    } else if (strcmp(str, "off") == 0) {
      jit::JitOptions.osr = false;
    } else {
      return OptionFailure("ion-osr", str);
    }
  }

  if (const char* str = op.getStringOption("ion-limit-script-size")) {
    if (strcmp(str, "on") == 0) {
      jit::JitOptions.limitScriptSize = true;
    } else if (strcmp(str, "off") == 0) {
      jit::JitOptions.limitScriptSize = false;
    } else {
      return OptionFailure("ion-limit-script-size", str);
    }
  }

  int32_t warmUpThreshold = op.getIntOption("ion-warmup-threshold");
  if (warmUpThreshold >= 0) {
    jit::JitOptions.setNormalIonWarmUpThreshold(warmUpThreshold);
  }

  warmUpThreshold = op.getIntOption("ion-full-warmup-threshold");
  if (warmUpThreshold >= 0) {
    jit::JitOptions.setFullIonWarmUpThreshold(warmUpThreshold);
  }

  warmUpThreshold = op.getIntOption("baseline-warmup-threshold");
  if (warmUpThreshold >= 0) {
    jit::JitOptions.baselineJitWarmUpThreshold = warmUpThreshold;
  }

  if (op.getBoolOption("baseline-eager")) {
    jit::JitOptions.setEagerBaselineCompilation();
  }

  if (op.getBoolOption("blinterp")) {
    jit::JitOptions.baselineInterpreter = true;
  }

  if (op.getBoolOption("no-blinterp")) {
    jit::JitOptions.baselineInterpreter = false;
  }

  warmUpThreshold = op.getIntOption("blinterp-warmup-threshold");
  if (warmUpThreshold >= 0) {
    jit::JitOptions.baselineInterpreterWarmUpThreshold = warmUpThreshold;
  }

  if (op.getBoolOption("blinterp-eager")) {
    jit::JitOptions.baselineInterpreterWarmUpThreshold = 0;
  }

  if (op.getBoolOption("no-baseline")) {
    jit::JitOptions.baselineJit = false;
  }

  if (op.getBoolOption("no-ion")) {
    jit::JitOptions.ion = false;
  }

  if (op.getBoolOption("no-native-regexp")) {
    jit::JitOptions.nativeRegExp = false;
  }

#ifdef NIGHTLY_BUILD
  if (op.getBoolOption("no-ti")) {
    jit::JitOptions.typeInference = false;
  }
  if (op.getBoolOption("no-warp")) {
    MOZ_ASSERT(!jit::JitOptions.warpBuilder,
               "WarpBuilder is disabled by default");
  } else if (op.getBoolOption("warp")) {
    // WarpBuilder requires TI to be disabled.
    jit::JitOptions.typeInference = false;
    jit::JitOptions.warpBuilder = true;
  }
#endif

  if (const char* str = op.getStringOption("ion-regalloc")) {
    jit::JitOptions.forcedRegisterAllocator = jit::LookupRegisterAllocator(str);
    if (!jit::JitOptions.forcedRegisterAllocator.isSome()) {
      return OptionFailure("ion-regalloc", str);
    }
  }

  if (op.getBoolOption("ion-eager")) {
    jit::JitOptions.setEagerIonCompilation();
  }

  offthreadCompilation = true;
  if (const char* str = op.getStringOption("ion-offthread-compile")) {
    if (strcmp(str, "off") == 0) {
      offthreadCompilation = false;
    } else if (strcmp(str, "on") != 0) {
      return OptionFailure("ion-offthread-compile", str);
    }
  }
  cx->runtime()->setOffthreadIonCompilationEnabled(offthreadCompilation);

  if (op.getStringOption("ion-parallel-compile")) {
    fprintf(stderr,
            "--ion-parallel-compile is deprecated. Please use "
            "--ion-offthread-compile instead.\n");
    return false;
  }

  if (const char* str = op.getStringOption("shared-memory")) {
    if (strcmp(str, "off") == 0) {
      enableSharedMemory = false;
    } else if (strcmp(str, "on") == 0) {
      enableSharedMemory = true;
    } else {
      return OptionFailure("shared-memory", str);
    }
  }

#if defined(JS_CODEGEN_ARM)
  if (const char* str = op.getStringOption("arm-hwcap")) {
    jit::ParseARMHwCapFlags(str);
  }

  int32_t fill = op.getIntOption("arm-asm-nop-fill");
  if (fill >= 0) {
    jit::Assembler::NopFill = fill;
  }

  int32_t poolMaxOffset = op.getIntOption("asm-pool-max-offset");
  if (poolMaxOffset >= 5 && poolMaxOffset <= 1024) {
    jit::Assembler::AsmPoolMaxOffset = poolMaxOffset;
  }
#endif

#if defined(JS_SIMULATOR_ARM)
  if (op.getBoolOption("arm-sim-icache-checks")) {
    jit::SimulatorProcess::ICacheCheckingDisableCount = 0;
  }

  int32_t stopAt = op.getIntOption("arm-sim-stop-at");
  if (stopAt >= 0) {
    jit::Simulator::StopSimAt = stopAt;
  }
#elif defined(JS_SIMULATOR_MIPS32) || defined(JS_SIMULATOR_MIPS64)
  if (op.getBoolOption("mips-sim-icache-checks")) {
    jit::SimulatorProcess::ICacheCheckingDisableCount = 0;
  }

  int32_t stopAt = op.getIntOption("mips-sim-stop-at");
  if (stopAt >= 0) {
    jit::Simulator::StopSimAt = stopAt;
  }
#endif

  reportWarnings = op.getBoolOption('w');
  compileOnly = op.getBoolOption('c');
  printTiming = op.getBoolOption('b');
  enableDisassemblyDumps = op.getBoolOption('D');
  cx->runtime()->profilingScripts =
      enableCodeCoverage || enableDisassemblyDumps;

#ifdef DEBUG
  dumpEntrainedVariables = op.getBoolOption("dump-entrained-variables");
#endif

#ifdef JS_GC_ZEAL
  const char* zealStr = op.getStringOption("gc-zeal");
  if (zealStr) {
    if (!cx->runtime()->gc.parseAndSetZeal(zealStr)) {
      return false;
    }
    uint32_t nextScheduled;
    cx->runtime()->gc.getZealBits(&gZealBits, &gZealFrequency, &nextScheduled);
  }
#endif

  return true;
}

static void SetWorkerContextOptions(JSContext* cx) {
  // Copy option values from the main thread.
  JS::ContextOptionsRef(cx)
      .setAsmJS(enableAsmJS)
      .setWasm(enableWasm)
      .setWasmBaseline(enableWasmBaseline)
      .setWasmIon(enableWasmIon)
#ifdef ENABLE_WASM_CRANELIFT
      .setWasmCranelift(enableWasmCranelift)
#endif
#ifdef ENABLE_WASM_GC
      .setWasmGc(enableWasmGc)
#endif
#ifdef ENABLE_WASM_MULTI_VALUE
      .setWasmMultiValue(enableWasmMultiValue)
#endif
#ifdef ENABLE_WASM_BIGINT
      .setWasmBigIntEnabled(enableWasmBigInt)
#endif
      .setWasmVerbose(enableWasmVerbose)
      .setTestWasmAwaitTier2(enableTestWasmAwaitTier2);

  cx->runtime()->setOffthreadIonCompilationEnabled(offthreadCompilation);
  cx->runtime()->profilingScripts =
      enableCodeCoverage || enableDisassemblyDumps;

#ifdef JS_GC_ZEAL
  if (gZealBits && gZealFrequency) {
    for (size_t i = 0; i < size_t(gc::ZealMode::Count); i++) {
      if (gZealBits & (1 << i)) {
        cx->runtime()->gc.setZeal(i, gZealFrequency);
      }
    }
  }
#endif

  JS_SetNativeStackQuota(cx, gMaxStackSize);
}

static MOZ_MUST_USE bool PrintUnhandledRejection(
    JSContext* cx, Handle<PromiseObject*> promise) {
  RootedValue reason(cx, promise->reason());
  RootedObject site(cx, promise->resolutionSite());

  RootedString str(cx, JS_ValueToSource(cx, reason));
  if (!str) {
    return false;
  }

  UniqueChars utf8chars = JS_EncodeStringToUTF8(cx, str);
  if (!utf8chars) {
    return false;
  }

  FILE* fp = ErrorFilePointer();
  fprintf(fp, "Unhandled rejection: %s\n", utf8chars.get());

  if (!site) {
    fputs("(no stack trace available)\n", stderr);
    return true;
  }

  JSPrincipals* principals = cx->realm()->principals();
  RootedString stackStr(cx);
  if (!BuildStackString(cx, principals, site, &stackStr, 2)) {
    return false;
  }

  UniqueChars stack = JS_EncodeStringToUTF8(cx, stackStr);
  if (!stack) {
    return false;
  }

  fputs("Stack:\n", fp);
  fputs(stack.get(), fp);

  return true;
}

static MOZ_MUST_USE bool ReportUnhandledRejections(JSContext* cx) {
  ShellContext* sc = GetShellContext(cx);
  if (!sc->trackUnhandledRejections) {
    return true;
  }

  if (!sc->unhandledRejectedPromises) {
    return true;
  }

  AutoRealm ar(cx, sc->unhandledRejectedPromises);

  if (!SetObject::size(cx, sc->unhandledRejectedPromises)) {
    return true;
  }

  sc->exitCode = EXITCODE_RUNTIME_ERROR;

  RootedValue iter(cx);
  if (!SetObject::iterator(cx, SetObject::IteratorKind::Values,
                           sc->unhandledRejectedPromises, &iter)) {
    return false;
  }

  Rooted<SetIteratorObject*> iterObj(cx,
                                     &iter.toObject().as<SetIteratorObject>());
  JSObject* obj = SetIteratorObject::createResult(cx);
  if (!obj) {
    return false;
  }

  RootedArrayObject resultObj(cx, &obj->as<ArrayObject>());
  while (true) {
    bool done = SetIteratorObject::next(iterObj, resultObj, cx);
    if (done) {
      break;
    }

    RootedObject obj(cx, &resultObj->getDenseElement(0).toObject());
    Rooted<PromiseObject*> promise(cx, obj->maybeUnwrapIf<PromiseObject>());
    if (!promise) {
      FILE* fp = ErrorFilePointer();
      fputs(
          "Unhandled rejection: dead proxy found in unhandled "
          "rejections set\n",
          fp);
      continue;
    }

    AutoRealm ar2(cx, promise);

    if (!PrintUnhandledRejection(cx, promise)) {
      return false;
    }
  }

  sc->unhandledRejectedPromises = nullptr;

  return true;
}

static int Shell(JSContext* cx, OptionParser* op, char** envp) {
  if (JS::TraceLoggerSupported()) {
    JS::StartTraceLogger(cx);
  }
#ifdef JS_STRUCTURED_SPEW
  cx->spewer().enableSpewing();
#endif

  auto exitShell = MakeScopeExit([&] {
    if (JS::TraceLoggerSupported()) {
      JS::SpewTraceLoggerForCurrentProcess();
      JS::StopTraceLogger(cx);
    }
#ifdef JS_STRUCTURED_SPEW
    cx->spewer().disableSpewing();
#endif
  });

  if (op->getBoolOption("wasm-compile-and-serialize")) {
    if (!WasmCompileAndSerialize(cx)) {
      // Errors have been printed directly to stderr.
      MOZ_ASSERT(!cx->isExceptionPending());
      return -1;
    }
    return EXIT_SUCCESS;
  }

#ifdef MOZ_CODE_COVERAGE
  InstallCoverageSignalHandlers();
#endif

  Maybe<JS::AutoDisableGenerationalGC> noggc;
  if (op->getBoolOption("no-ggc")) {
    noggc.emplace(cx);
  }

  Maybe<AutoDisableCompactingGC> nocgc;
  if (op->getBoolOption("no-cgc")) {
    nocgc.emplace(cx);
  }

  if (op->getBoolOption("fuzzing-safe")) {
    fuzzingSafe = true;
  } else {
    fuzzingSafe =
        (getenv("MOZ_FUZZING_SAFE") && getenv("MOZ_FUZZING_SAFE")[0] != '0');
  }

  if (op->getBoolOption("disable-oom-functions")) {
    disableOOMFunctions = true;
  }

  if (op->getBoolOption("more-compartments")) {
    defaultToSameCompartment = false;
  }

  JS::RealmOptions options;
  SetStandardRealmOptions(options);
  RootedObject glob(
      cx, NewGlobalObject(cx, options, nullptr, ShellGlobalKind::WindowProxy));
  if (!glob) {
    return 1;
  }

  JSAutoRealm ar(cx, glob);

#ifdef FUZZING_INTERFACES
  if (fuzzHaveModule) {
    return FuzzJSRuntimeStart(cx, &sArgc, &sArgv);
  }
#endif

  ShellContext* sc = GetShellContext(cx);
  int result = EXIT_SUCCESS;
  {
    AutoReportException are(cx);
    if (!ProcessArgs(cx, op) && !sc->quitting) {
      result = EXITCODE_RUNTIME_ERROR;
    }
  }

  /*
   * The job queue must be drained even on error to finish outstanding async
   * tasks before the main thread JSRuntime is torn down. Drain after
   * uncaught exceptions have been reported since draining runs callbacks.
   */
  RunShellJobs(cx);

  // Only if there's no other error, report unhandled rejections.
  if (!result && !sc->exitCode) {
    AutoReportException are(cx);
    if (!ReportUnhandledRejections(cx)) {
      FILE* fp = ErrorFilePointer();
      fputs("Error while printing unhandled rejection\n", fp);
    }
  }

  if (sc->exitCode) {
    result = sc->exitCode;
  }

  if (enableDisassemblyDumps) {
    AutoReportException are(cx);
    if (!js::DumpRealmPCCounts(cx)) {
      result = EXITCODE_OUT_OF_MEMORY;
    }
  }

  /*
   * Dump remaining type inference results while we still have a context.
   * This printing depends on atoms still existing.
   */
  for (CompartmentsIter c(cx->runtime()); !c.done(); c.next()) {
    PrintTypes(cx, c, false);
  }

  return result;
}

// Used to allocate memory when jemalloc isn't yet initialized.
JS_DECLARE_NEW_METHODS(SystemAlloc_New, malloc, static)

static void SetOutputFile(const char* const envVar, RCFile* defaultOut,
                          RCFile** outFileP) {
  RCFile* outFile;

  const char* outPath = getenv(envVar);
  FILE* newfp;
  if (outPath && *outPath && (newfp = fopen(outPath, "w"))) {
    outFile = SystemAlloc_New<RCFile>(newfp);
  } else {
    outFile = defaultOut;
  }

  if (!outFile) {
    MOZ_CRASH("Failed to allocate output file");
  }

  outFile->acquire();
  *outFileP = outFile;
}

static void PreInit() {
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

#ifndef JS_WITHOUT_NSPR
class AutoLibraryLoader {
  Vector<PRLibrary*, 4, SystemAllocPolicy> libraries;

 public:
  ~AutoLibraryLoader() {
    for (auto dll : libraries) {
      PR_UnloadLibrary(dll);
    }
  }

  PRLibrary* load(const char* path) {
    PRLibSpec libSpec;
    libSpec.type = PR_LibSpec_Pathname;
    libSpec.value.pathname = path;
    PRLibrary* dll = PR_LoadLibraryWithFlags(libSpec, PR_LD_NOW | PR_LD_GLOBAL);
    if (!dll) {
      fprintf(stderr, "LoadLibrary '%s' failed with code %d\n", path,
              PR_GetError());
      MOZ_CRASH("Failed to load library");
    }

    MOZ_ALWAYS_TRUE(libraries.append(dll));
    return dll;
  }
};
#endif

int main(int argc, char** argv, char** envp) {
  PreInit();

  sArgc = argc;
  sArgv = argv;

  int result;

  setlocale(LC_ALL, "");

  // Special-case stdout and stderr. We bump their refcounts to prevent them
  // from getting closed and then having some printf fail somewhere.
  RCFile rcStdout(stdout);
  rcStdout.acquire();
  RCFile rcStderr(stderr);
  rcStderr.acquire();

  SetOutputFile("JS_STDOUT", &rcStdout, &gOutFile);
  SetOutputFile("JS_STDERR", &rcStderr, &gErrFile);

  // Start the engine.
  if (const char* message = JS_InitWithFailureDiagnostic()) {
    fprintf(gErrFile->fp, "JS_Init failed: %s\n", message);
    return 1;
  }

  auto shutdownEngine = MakeScopeExit([] { JS_ShutDown(); });

  OptionParser op("Usage: {progname} [options] [[script] scriptArgs*]");

  op.setDescription(
      "The SpiderMonkey shell provides a command line interface to the "
      "JavaScript engine. Code and file options provided via the command line "
      "are "
      "run left to right. If provided, the optional script argument is run "
      "after "
      "all options have been processed. Just-In-Time compilation modes may be "
      "enabled via "
      "command line options.");
  op.setDescriptionWidth(72);
  op.setHelpWidth(80);
  op.setVersion(JS_GetImplementationVersion());

  if (!op.addMultiStringOption(
          'f', "file", "PATH",
          "File path to run, parsing file contents as UTF-8") ||
      !op.addMultiStringOption(
          'u', "utf16-file", "PATH",
          "File path to run, inflating the file's UTF-8 contents to UTF-16 and "
          "then parsing that") ||
      !op.addMultiStringOption('m', "module", "PATH", "Module path to run") ||
#if defined(JS_BUILD_BINAST)
      !op.addMultiStringOption('B', "binast", "PATH", "BinAST path to run") ||
      !op.addStringOption('\0', "binast-format", "[format]",
                          "Format of BinAST file (multipart/context)") ||
#else
      !op.addMultiStringOption('B', "binast", "", "No-op") ||
      !op.addStringOption('\0', "binast-format", "[format]", "No-op") ||
#endif  // JS_BUILD_BINAST
      !op.addMultiStringOption('e', "execute", "CODE", "Inline code to run") ||
      !op.addBoolOption('i', "shell", "Enter prompt after running code") ||
      !op.addBoolOption('c', "compileonly",
                        "Only compile, don't run (syntax checking mode)") ||
      !op.addBoolOption('w', "warnings", "Emit warnings") ||
      !op.addBoolOption('W', "nowarnings", "Don't emit warnings") ||
      !op.addBoolOption('D', "dump-bytecode",
                        "Dump bytecode with exec count for all scripts") ||
      !op.addBoolOption('b', "print-timing",
                        "Print sub-ms runtime for each file that's run") ||
      !op.addBoolOption('\0', "code-coverage",
                        "Enable code coverage instrumentation.") ||
      !op.addBoolOption(
          '\0', "disable-parser-deferred-alloc",
          "Disable deferred allocation of GC objects until after parser") ||
#ifdef DEBUG
      !op.addBoolOption('O', "print-alloc",
                        "Print the number of allocations at exit") ||
#endif
      !op.addOptionalStringArg("script",
                               "A script to execute (after all options)") ||
      !op.addOptionalMultiStringArg(
          "scriptArgs",
          "String arguments to bind as |scriptArgs| in the "
          "shell's global") ||
      !op.addIntOption(
          '\0', "cpu-count", "COUNT",
          "Set the number of CPUs (hardware threads) to COUNT, the "
          "default is the actual number of CPUs. The total number of "
          "background helper threads is the CPU count plus some constant.",
          -1) ||
      !op.addIntOption('\0', "thread-count", "COUNT", "Alias for --cpu-count.",
                       -1) ||
      !op.addBoolOption('\0', "ion", "Enable IonMonkey (default)") ||
      !op.addBoolOption('\0', "no-ion", "Disable IonMonkey") ||
      !op.addBoolOption('\0', "no-ion-for-main-context",
                        "Disable IonMonkey for the main context only") ||
#ifdef NIGHTLY_BUILD
      !op.addBoolOption('\0', "no-ti", "Disable Type Inference") ||
      !op.addBoolOption('\0', "warp", "Use WarpBuilder as MIR builder") ||
#endif
      !op.addBoolOption('\0', "no-warp", "Disable WarpBuilder (default)") ||
      !op.addBoolOption('\0', "no-asmjs", "Disable asm.js compilation") ||
      !op.addStringOption(
          '\0', "wasm-compiler", "[option]",
          "Choose to enable a subset of the wasm compilers (valid options are "
          "none/baseline/ion/cranelift/baseline+ion/baseline+cranelift)") ||
      !op.addBoolOption('\0', "wasm-verbose",
                        "Enable WebAssembly verbose logging") ||
      !op.addBoolOption('\0', "disable-wasm-huge-memory",
                        "Disable WebAssembly huge memory") ||
#ifdef ENABLE_WASM_BIGINT
      !op.addBoolOption('\0', "no-wasm-bigint",
                        "Disable WebAssembly BigInt conversions") ||
#else
      !op.addBoolOption('\0', "no-wasm-bigint", "No-op") ||
#endif
      !op.addBoolOption('\0', "test-wasm-await-tier2",
                        "Forcibly activate tiering and block "
                        "instantiation on completion of tier2") ||
#ifdef ENABLE_WASM_GC
      !op.addBoolOption('\0', "wasm-gc",
                        "Enable experimental wasm GC features") ||
#else
      !op.addBoolOption('\0', "wasm-gc", "No-op") ||
#endif
#ifdef ENABLE_WASM_MULTI_VALUE
      !op.addBoolOption('\0', "no-wasm-multi-value",
                        "Disable wasm multi-value features") ||
#else
      !op.addBoolOption('\0', "no-wasm-multi-value", "No-op") ||
#endif

      !op.addBoolOption('\0', "no-native-regexp",
                        "Disable native regexp compilation") ||
      !op.addBoolOption('\0', "no-unboxed-objects",
                        "Disable creating unboxed plain objects") ||
      !op.addBoolOption('\0', "enable-streams",
                        "Enable WHATWG Streams (default)") ||
      !op.addBoolOption('\0', "no-streams", "Disable WHATWG Streams") ||
      !op.addBoolOption('\0', "enable-readable-byte-streams",
                        "Enable support for WHATWG ReadableStreams of type "
                        "'bytes'") ||
      !op.addBoolOption('\0', "enable-byob-stream-readers",
                        "Enable support for getting BYOB readers for WHATWG "
                        "ReadableStreams of type \"bytes\"") ||
      !op.addBoolOption('\0', "enable-writable-streams",
                        "Enable support for WHATWG WritableStreams") ||
      !op.addBoolOption('\0', "enable-readablestream-pipeto",
                        "Enable support for "
                        "WHATWG ReadableStream.prototype.pipeTo") ||
      !op.addBoolOption('\0', "enable-weak-refs", "Enable weak references") ||
      !op.addBoolOption('\0', "disable-tosource", "Disable toSource/uneval") ||
      !op.addBoolOption('\0', "disable-property-error-message-fix",
                        "Disable fix for the error message when accessing "
                        "property of null or undefined") ||
      !op.addStringOption('\0', "shared-memory", "on/off",
                          "SharedArrayBuffer and Atomics "
#if SHARED_MEMORY_DEFAULT
                          "(default: on, off to disable)"
#else
                          "(default: off, on to enable)"
#endif
                          ) ||
      !op.addStringOption('\0', "spectre-mitigations", "on/off",
                          "Whether Spectre mitigations are enabled (default: "
                          "off, on to enable)") ||
      !op.addStringOption('\0', "cache-ir-stubs", "on/off/call",
                          "Use CacheIR stubs (default: on, off to disable, "
                          "call to enable work-in-progress call ICs)") ||
      !op.addStringOption('\0', "ion-shared-stubs", "on/off",
                          "Use shared stubs (default: on, off to disable)") ||
      !op.addStringOption('\0', "ion-scalar-replacement", "on/off",
                          "Scalar Replacement (default: on, off to disable)") ||
      !op.addStringOption('\0', "ion-gvn", "[mode]",
                          "Specify Ion global value numbering:\n"
                          "  off: disable GVN\n"
                          "  on:  enable GVN (default)\n") ||
      !op.addStringOption(
          '\0', "ion-licm", "on/off",
          "Loop invariant code motion (default: on, off to disable)") ||
      !op.addStringOption('\0', "ion-edgecase-analysis", "on/off",
                          "Find edge cases where Ion can avoid bailouts "
                          "(default: on, off to disable)") ||
      !op.addStringOption(
          '\0', "ion-pgo", "on/off",
          "Profile guided optimization (default: on, off to disable)") ||
      !op.addStringOption('\0', "ion-range-analysis", "on/off",
                          "Range analysis (default: on, off to disable)") ||
      !op.addStringOption('\0', "ion-sink", "on/off",
                          "Sink code motion (default: off, on to enable)") ||
      !op.addStringOption('\0', "ion-optimization-levels", "on/off",
                          "Use multiple Ion optimization levels (default: on, "
                          "off to disable)") ||
      !op.addStringOption('\0', "ion-loop-unrolling", "on/off",
                          "(NOP for fuzzers)") ||
      !op.addStringOption(
          '\0', "ion-instruction-reordering", "on/off",
          "Instruction reordering (default: off, on to enable)") ||
      !op.addBoolOption('\0', "ion-check-range-analysis",
                        "Range analysis checking") ||
      !op.addBoolOption('\0', "ion-extra-checks",
                        "Perform extra dynamic validation checks") ||
      !op.addStringOption(
          '\0', "ion-inlining", "on/off",
          "Inline methods where possible (default: on, off to disable)") ||
      !op.addStringOption(
          '\0', "ion-osr", "on/off",
          "On-Stack Replacement (default: on, off to disable)") ||
      !op.addStringOption(
          '\0', "ion-limit-script-size", "on/off",
          "Don't compile very large scripts (default: on, off to disable)") ||
      !op.addIntOption('\0', "ion-warmup-threshold", "COUNT",
                       "Wait for COUNT calls or iterations before compiling "
                       "at the normal optimization level (default: 1000)",
                       -1) ||
      !op.addIntOption('\0', "ion-full-warmup-threshold", "COUNT",
                       "Wait for COUNT calls or iterations before compiling "
                       "at the 'full' optimization level (default: 100,000)",
                       -1) ||
      !op.addStringOption(
          '\0', "ion-regalloc", "[mode]",
          "Specify Ion register allocation:\n"
          "  backtracking: Priority based backtracking register allocation "
          "(default)\n"
          "  testbed: Backtracking allocator with experimental features\n"
          "  stupid: Simple block local register allocation") ||
      !op.addBoolOption(
          '\0', "ion-eager",
          "Always ion-compile methods (implies --baseline-eager)") ||
      !op.addStringOption('\0', "ion-offthread-compile", "on/off",
                          "Compile scripts off thread (default: on)") ||
      !op.addStringOption('\0', "ion-parallel-compile", "on/off",
                          "--ion-parallel compile is deprecated. Use "
                          "--ion-offthread-compile.") ||
      !op.addBoolOption('\0', "baseline",
                        "Enable baseline compiler (default)") ||
      !op.addBoolOption('\0', "no-baseline", "Disable baseline compiler") ||
      !op.addBoolOption('\0', "baseline-eager",
                        "Always baseline-compile methods") ||
      !op.addIntOption(
          '\0', "baseline-warmup-threshold", "COUNT",
          "Wait for COUNT calls or iterations before baseline-compiling "
          "(default: 10)",
          -1) ||
      !op.addBoolOption('\0', "blinterp",
                        "Enable Baseline Interpreter (default)") ||
      !op.addBoolOption('\0', "no-blinterp", "Disable Baseline Interpreter") ||
      !op.addBoolOption('\0', "blinterp-eager",
                        "Always Baseline-interpret scripts") ||
      !op.addIntOption(
          '\0', "blinterp-warmup-threshold", "COUNT",
          "Wait for COUNT calls or iterations before Baseline-interpreting "
          "(default: 10)",
          -1) ||
      !op.addBoolOption(
          '\0', "non-writable-jitcode",
          "(NOP for fuzzers) Allocate JIT code as non-writable memory.") ||
      !op.addBoolOption(
          '\0', "no-sse3",
          "Pretend CPU does not support SSE3 instructions and above "
          "to test JIT codegen (no-op on platforms other than x86 and x64).") ||
      !op.addBoolOption(
          '\0', "no-sse4",
          "Pretend CPU does not support SSE4 instructions "
          "to test JIT codegen (no-op on platforms other than x86 and x64).") ||
      !op.addBoolOption('\0', "enable-avx",
                        "AVX is disabled by default. Enable AVX. "
                        "(no-op on platforms other than x86 and x64).") ||
      !op.addBoolOption('\0', "no-avx",
                        "No-op. AVX is currently disabled by default.") ||
      !op.addBoolOption('\0', "more-compartments",
                        "Make newGlobal default to creating a new "
                        "compartment.") ||
      !op.addBoolOption('\0', "fuzzing-safe",
                        "Don't expose functions that aren't safe for "
                        "fuzzers to call") ||
      !op.addBoolOption('\0', "disable-oom-functions",
                        "Disable functions that cause "
                        "artificial OOMs") ||
      !op.addBoolOption('\0', "no-threads", "Disable helper threads") ||
#ifdef DEBUG
      !op.addBoolOption('\0', "dump-entrained-variables",
                        "Print variables which are "
                        "unnecessarily entrained by inner functions") ||
#endif
      !op.addBoolOption('\0', "no-ggc", "Disable Generational GC") ||
      !op.addBoolOption('\0', "no-cgc", "Disable Compacting GC") ||
      !op.addBoolOption('\0', "no-incremental-gc", "Disable Incremental GC") ||
      !op.addStringOption('\0', "nursery-strings", "on/off",
                          "Allocate strings in the nursery") ||
      !op.addStringOption('\0', "nursery-bigints", "on/off",
                          "Allocate BigInts in the nursery") ||
      !op.addIntOption('\0', "available-memory", "SIZE",
                       "Select GC settings based on available memory (MB)",
                       0) ||
      !op.addStringOption('\0', "arm-hwcap", "[features]",
                          "Specify ARM code generation features, or 'help' to "
                          "list all features.") ||
      !op.addIntOption('\0', "arm-asm-nop-fill", "SIZE",
                       "Insert the given number of NOP instructions at all "
                       "possible pool locations.",
                       0) ||
      !op.addIntOption('\0', "asm-pool-max-offset", "OFFSET",
                       "The maximum pc relative OFFSET permitted in pool "
                       "reference instructions.",
                       1024) ||
      !op.addBoolOption('\0', "arm-sim-icache-checks",
                        "Enable icache flush checks in the ARM "
                        "simulator.") ||
      !op.addIntOption('\0', "arm-sim-stop-at", "NUMBER",
                       "Stop the ARM simulator after the given "
                       "NUMBER of instructions.",
                       -1) ||
      !op.addBoolOption('\0', "mips-sim-icache-checks",
                        "Enable icache flush checks in the MIPS "
                        "simulator.") ||
      !op.addIntOption('\0', "mips-sim-stop-at", "NUMBER",
                       "Stop the MIPS simulator after the given "
                       "NUMBER of instructions.",
                       -1) ||
      !op.addIntOption('\0', "nursery-size", "SIZE-MB",
                       "Set the maximum nursery size in MB",
                       JS::DefaultNurseryMaxBytes / 1024 / 1024) ||
#ifdef JS_GC_ZEAL
      !op.addStringOption('z', "gc-zeal", "LEVEL(;LEVEL)*[,N]",
                          gc::ZealModeHelpText) ||
#else
      !op.addStringOption('z', "gc-zeal", "LEVEL(;LEVEL)*[,N]",
                          "option ignored in non-gc-zeal builds") ||
#endif
      !op.addStringOption('\0', "module-load-path", "DIR",
                          "Set directory to load modules from") ||
      !op.addBoolOption('\0', "no-async-stacks", "Disable async stacks") ||
      !op.addMultiStringOption('\0', "dll", "LIBRARY",
                               "Dynamically load LIBRARY") ||
      !op.addBoolOption('\0', "suppress-minidump",
                        "Suppress crash minidumps") ||
#ifdef JS_ENABLE_SMOOSH
      !op.addBoolOption('\0', "smoosh", "Use SmooshMonkey") ||
      !op.addStringOption('\0', "not-implemented-watchfile", "[filename]",
                          "Track NotImplemented errors in the new frontend") ||
#else
      !op.addBoolOption('\0', "smoosh", "No-op") ||
#endif
      !op.addBoolOption('\0', "wasm-compile-and-serialize",
                        "Compile the wasm bytecode from stdin and serialize "
                        "the results to stdout")) {
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

  if (op.getHelpOption()) {
    return EXIT_SUCCESS;
  }

  /*
   * Allow dumping on Linux with the fuzzing flag set, even when running with
   * the suid/sgid flag set on the shell.
   */
#ifdef XP_LINUX
  if (op.getBoolOption("fuzzing-safe")) {
    prctl(PR_SET_DUMPABLE, 1);
  }
#endif

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
    if (!sCompilerProcessFlags.append("--no-sse3")) {
      return EXIT_FAILURE;
    }
  }
  if (op.getBoolOption("no-sse4")) {
    js::jit::CPUInfo::SetSSE4Disabled();
    if (!sCompilerProcessFlags.append("--no-sse4")) {
      return EXIT_FAILURE;
    }
  }
  if (op.getBoolOption("enable-avx")) {
    js::jit::CPUInfo::SetAVXEnabled();
    if (!sCompilerProcessFlags.append("--enable-avx")) {
      return EXIT_FAILURE;
    }
  }
#endif

  if (op.getBoolOption("no-threads")) {
    js::DisableExtraThreads();
  }

  enableCodeCoverage = op.getBoolOption("code-coverage");
  if (enableCodeCoverage) {
    coverage::EnableLCov();
  }

  enableDeferredMode = !op.getBoolOption("disable-parser-deferred-alloc") &&
                       getenv("DISABLE_PARSER_DEFERRED_ALLOC") == nullptr;

#ifdef JS_WITHOUT_NSPR
  if (!op.getMultiStringOption("dll").empty()) {
    fprintf(stderr, "Error: --dll requires NSPR support!\n");
    return EXIT_FAILURE;
  }
#else
  AutoLibraryLoader loader;
  MultiStringRange dllPaths = op.getMultiStringOption("dll");
  while (!dllPaths.empty()) {
    char* path = dllPaths.front();
    loader.load(path);
    dllPaths.popFront();
  }
#endif

  if (op.getBoolOption("suppress-minidump")) {
    js::NoteIntentionalCrash();
  }

  if (!InitSharedObjectMailbox()) {
    return 1;
  }

  JS::SetProcessBuildIdOp(ShellBuildId);

  // The fake CPU count must be set before initializing the Runtime,
  // which spins up the thread pool.
  int32_t cpuCount = op.getIntOption("cpu-count");  // What we're really setting
  if (cpuCount < 0) {
    cpuCount = op.getIntOption("thread-count");  // Legacy name
  }
  if (cpuCount >= 0 && !SetFakeCPUCount(cpuCount)) {
    return 1;
  }

  /* Use the same parameters as the browser in xpcjsruntime.cpp. */
  JSContext* const cx = JS_NewContext(JS::DefaultHeapMaxBytes);
  if (!cx) {
    return 1;
  }

  size_t nurseryBytes = op.getIntOption("nursery-size") * 1024L * 1024L;
  JS_SetGCParameter(cx, JSGC_MAX_NURSERY_BYTES, nurseryBytes);

  auto destroyCx = MakeScopeExit([cx] { JS_DestroyContext(cx); });

  UniquePtr<ShellContext> sc = MakeUnique<ShellContext>(cx);
  if (!sc) {
    return 1;
  }
  auto destroyShellContext = MakeScopeExit([cx, &sc] {
    // Must clear out some of sc's pointer containers before JS_DestroyContext.
    sc->markObservers.reset();

    JS_SetContextPrivate(cx, nullptr);
    sc.reset();
  });

  JS_SetContextPrivate(cx, sc.get());
  JS_SetGrayGCRootsTracer(cx, TraceGrayRoots, nullptr);
  auto resetGrayGCRootsTracer =
      MakeScopeExit([cx] { JS_SetGrayGCRootsTracer(cx, nullptr, nullptr); });

  // Waiting is allowed on the shell's main thread, for now.
  JS_SetFutexCanWait(cx);
  JS::SetWarningReporter(cx, WarningReporter);
  if (!SetContextOptions(cx, op)) {
    return 1;
  }

  JS_SetGCParameter(cx, JSGC_MAX_BYTES, 0xffffffff);

  size_t availMem = op.getIntOption("available-memory");
  if (availMem > 0) {
    JS_SetGCParametersBasedOnAvailableMemory(cx, availMem);
  }

  JS_SetTrustedPrincipals(cx, &ShellPrincipals::fullyTrusted);
  JS_SetSecurityCallbacks(cx, &ShellPrincipals::securityCallbacks);
  JS_InitDestroyPrincipalsCallback(cx, ShellPrincipals::destroy);
  JS_SetDestroyCompartmentCallback(cx, DestroyShellCompartmentPrivate);

  js::SetWindowProxyClass(cx, &ShellWindowProxyClass);

  JS_AddInterruptCallback(cx, ShellInterruptCallback);

  bufferStreamState = js_new<ExclusiveWaitableData<BufferStreamState>>(
      mutexid::BufferStreamState);
  if (!bufferStreamState) {
    return 1;
  }
  auto shutdownBufferStreams = MakeScopeExit([] {
    ShutdownBufferStreams();
    js_delete(bufferStreamState);
  });
  JS::InitConsumeStreamCallback(cx, ConsumeBufferSource, ReportStreamError);

  JS::SetPromiseRejectionTrackerCallback(
      cx, ForwardingPromiseRejectionTrackerCallback);

  JS_SetNativeStackQuota(cx, gMaxStackSize);

  JS::dbg::SetDebuggerMallocSizeOf(cx, moz_malloc_size_of);

  js::UseInternalJobQueues(cx);

  JS::SetHostCleanupFinalizationRegistryCallback(
      cx, ShellCleanupFinalizationRegistryCallback, sc.get());

  auto shutdownShellThreads = MakeScopeExit([cx] {
    KillWatchdog(cx);
    KillWorkerThreads(cx);
    DestructSharedObjectMailbox();
    CancelOffThreadJobsForRuntime(cx);
  });

  if (const char* opt = op.getStringOption("nursery-strings")) {
    if (strcmp(opt, "on") == 0) {
      cx->runtime()->gc.nursery().enableStrings();
    } else if (strcmp(opt, "off") == 0) {
      cx->runtime()->gc.nursery().disableStrings();
    } else {
      MOZ_CRASH("invalid option value for --nursery-strings, must be on/off");
    }
  }

  if (const char* opt = op.getStringOption("nursery-bigints")) {
    if (strcmp(opt, "on") == 0) {
      cx->runtime()->gc.nursery().enableBigInts();
    } else if (strcmp(opt, "off") == 0) {
      cx->runtime()->gc.nursery().disableBigInts();
    } else {
      MOZ_CRASH("invalid option value for --nursery-bigints, must be on/off");
    }
  }

  if (!JS::InitSelfHostedCode(cx)) {
    return 1;
  }

  EnvironmentPreparer environmentPreparer(cx);

  JS_SetGCParameter(cx, JSGC_MODE, JSGC_MODE_ZONE_INCREMENTAL);

  JS::SetProcessLargeAllocationFailureCallback(my_LargeAllocFailCallback);

  // Set some parameters to allow incremental GC in low memory conditions,
  // as is done for the browser, except in more-deterministic builds or when
  // disabled by command line options.
#ifndef JS_MORE_DETERMINISTIC
  if (!op.getBoolOption("no-incremental-gc")) {
    JS_SetGCParameter(cx, JSGC_DYNAMIC_HEAP_GROWTH, 1);
    JS_SetGCParameter(cx, JSGC_DYNAMIC_MARK_SLICE, 1);
    JS_SetGCParameter(cx, JSGC_SLICE_TIME_BUDGET_MS, 10);
  }
#endif

  js::SetPreserveWrapperCallback(cx, DummyPreserveWrapperCallback);

  JS::SetModuleResolveHook(cx->runtime(), ShellModuleResolveHook);
  JS::SetModuleDynamicImportHook(cx->runtime(), ShellModuleDynamicImportHook);
  JS::SetModuleMetadataHook(cx->runtime(), CallModuleMetadataHook);

  if (op.getBoolOption("disable-wasm-huge-memory")) {
    if (!sCompilerProcessFlags.append("--disable-wasm-huge-memory")) {
      return EXIT_FAILURE;
    }
    bool disabledHugeMemory = JS::DisableWasmHugeMemory();
    MOZ_RELEASE_ASSERT(disabledHugeMemory);
  }

  result = Shell(cx, &op, envp);

#ifdef DEBUG
  if (OOM_printAllocationCount) {
    printf("OOM max count: %" PRIu64 "\n", js::oom::simulator.counter());
  }
#endif

  return result;
}
