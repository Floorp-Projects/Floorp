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
#include "mozilla/EnumSet.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/mozalloc.h"
#include "mozilla/PodOperations.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Sprintf.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtrExtensions.h"  // UniqueFreePtr
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
#ifndef __wasi__
#  include <signal.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <utility>
#ifdef XP_UNIX
#  ifndef __wasi__
#    include <sys/mman.h>
#    include <sys/wait.h>
#  endif
#  include <sys/stat.h>
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

#include "builtin/Array.h"
#include "builtin/MapObject.h"
#include "builtin/ModuleObject.h"
#include "builtin/RegExp.h"
#include "builtin/TestingFunctions.h"
#include "builtin/TestingUtility.h"  // js::ParseCompileOptions, js::ParseDebugMetadata, js::CreateScriptPrivate
#include "debugger/DebugAPI.h"
#include "frontend/BytecodeCompilation.h"
#include "frontend/BytecodeCompiler.h"
#include "frontend/CompilationStencil.h"
#ifdef JS_ENABLE_SMOOSH
#  include "frontend/Frontend2.h"
#endif
#include "frontend/ModuleSharedContext.h"
#include "frontend/Parser.h"
#include "frontend/SourceNotes.h"  // SrcNote, SrcNoteType, SrcNoteIterator
#include "gc/PublicIterators.h"
#ifdef DEBUG
#  include "irregexp/RegExpAPI.h"
#endif
#include "gc/GC-inl.h"  // ZoneCellIter

#ifdef JS_SIMULATOR_ARM
#  include "jit/arm/Simulator-arm.h"
#endif
#ifdef JS_SIMULATOR_MIPS32
#  include "jit/mips32/Simulator-mips32.h"
#endif
#ifdef JS_SIMULATOR_MIPS64
#  include "jit/mips64/Simulator-mips64.h"
#endif
#ifdef JS_SIMULATOR_LOONG64
#  include "jit/loong64/Simulator-loong64.h"
#endif
#include "jit/CacheIRHealth.h"
#include "jit/InlinableNatives.h"
#include "jit/Ion.h"
#include "jit/JitcodeMap.h"
#include "jit/shared/CodeGenerator-shared.h"
#include "js/Array.h"        // JS::NewArrayObject
#include "js/ArrayBuffer.h"  // JS::{CreateMappedArrayBufferContents,NewMappedArrayBufferWithContents,IsArrayBufferObject,GetArrayBufferLengthAndData}
#include "js/BuildId.h"      // JS::BuildIdCharVector, JS::SetProcessBuildIdOp
#include "js/CallAndConstruct.h"  // JS::Call, JS::IsCallable, JS_CallFunction, JS_CallFunctionValue
#include "js/CharacterEncoding.h"  // JS::StringIsASCII
#include "js/CompilationAndEvaluation.h"
#include "js/CompileOptions.h"
#include "js/ContextOptions.h"  // JS::ContextOptions{,Ref}
#include "js/Debug.h"
#include "js/Equality.h"                   // JS::SameValue
#include "js/ErrorReport.h"                // JS::PrintError
#include "js/Exception.h"                  // JS::StealPendingExceptionStack
#include "js/experimental/CodeCoverage.h"  // js::EnableCodeCoverage
#include "js/experimental/CTypes.h"        // JS::InitCTypesClass
#include "js/experimental/Intl.h"  // JS::AddMoz{DateTimeFormat,DisplayNames}Constructor
#include "js/experimental/JitInfo.h"  // JSJit{Getter,Setter,Method}CallArgs, JSJitGetterInfo, JSJit{Getter,Setter}Op, JSJitInfo
#include "js/experimental/JSStencil.h"  // JS::Stencil, JS::CompileToStencilOffThread, JS::FinishCompileToStencilOffThread
#include "js/experimental/SourceHook.h"  // js::{Set,Forget,}SourceHook
#include "js/experimental/TypedData.h"   // JS_NewUint8Array
#include "js/friend/DumpFunctions.h"     // JS::FormatStackDump
#include "js/friend/ErrorMessages.h"     // js::GetErrorMessage, JSMSG_*
#include "js/friend/StackLimits.h"       // js::AutoCheckRecursionLimit
#include "js/friend/WindowProxy.h"  // js::IsWindowProxy, js::SetWindowProxyClass, js::ToWindowProxyIfWindow, js::ToWindowIfWindowProxy
#include "js/GCAPI.h"               // JS::AutoCheckCannotGC
#include "js/GCVector.h"
#include "js/GlobalObject.h"
#include "js/Initialization.h"
#include "js/Interrupt.h"
#include "js/JSON.h"
#include "js/MemoryCallbacks.h"
#include "js/MemoryFunctions.h"
#include "js/Modules.h"  // JS::GetModulePrivate, JS::SetModule{DynamicImport,Metadata,Resolve}Hook, JS::SetModulePrivate
#include "js/Object.h"  // JS::GetClass, JS::GetCompartment, JS::GetReservedSlot, JS::SetReservedSlot
#include "js/Printf.h"
#include "js/PropertyAndElement.h"  // JS_DefineElement, JS_DefineFunction, JS_DefineFunctions, JS_DefineProperties, JS_DefineProperty, JS_GetElement, JS_GetProperty, JS_GetPropertyById, JS_HasProperty, JS_SetElement, JS_SetProperty, JS_SetPropertyById
#include "js/PropertySpec.h"
#include "js/Realm.h"
#include "js/RegExp.h"  // JS::ObjectIsRegExp
#include "js/ScriptPrivate.h"
#include "js/SourceText.h"
#include "js/StableStringChars.h"
#include "js/Stack.h"
#include "js/StreamConsumer.h"
#include "js/StructuredClone.h"
#include "js/SweepingAPI.h"
#include "js/Transcoding.h"  // JS::TranscodeBuffer, JS::TranscodeRange
#include "js/Warnings.h"     // JS::SetWarningReporter
#include "js/WasmModule.h"   // JS::WasmModule
#include "js/Wrapper.h"
#include "proxy/DeadObjectProxy.h"  // js::IsDeadProxyObject
#include "shell/jsoptparse.h"
#include "shell/jsshell.h"
#include "shell/OSObject.h"
#include "shell/ShellModuleObjectWrapper.h"
#include "shell/WasmTesting.h"
#include "threading/ConditionVariable.h"
#include "threading/ExclusiveData.h"
#include "threading/LockGuard.h"
#include "threading/Thread.h"
#include "util/CompleteFile.h"  // js::FileContents, js::ReadCompleteFile
#include "util/DifferentialTesting.h"
#include "util/StringBuffer.h"
#include "util/Text.h"
#include "util/WindowsWrapper.h"
#include "vm/ArgumentsObject.h"
#include "vm/Compression.h"
#include "vm/ErrorObject.h"
#include "vm/HelperThreads.h"
#include "vm/JSAtom.h"
#include "vm/JSContext.h"
#include "vm/JSFunction.h"
#include "vm/JSObject.h"
#include "vm/JSScript.h"
#include "vm/ModuleBuilder.h"  // js::ModuleBuilder
#include "vm/Monitor.h"
#include "vm/MutexIDs.h"
#include "vm/Printer.h"        // QuoteString
#include "vm/PromiseObject.h"  // js::PromiseObject
#include "vm/Shape.h"
#include "vm/SharedArrayObject.h"
#include "vm/StencilObject.h"  // js::StencilObject
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

#ifdef FUZZING_JS_FUZZILLI
#  define REPRL_CRFD 100
#  define REPRL_CWFD 101
#  define REPRL_DRFD 102
#  define REPRL_DWFD 103

#  define SHM_SIZE 0x100000
#  define MAX_EDGES ((SHM_SIZE - 4) * 8)

struct shmem_data {
  uint32_t num_edges;
  unsigned char edges[];
};

struct shmem_data* __shmem;

uint32_t *__edges_start, *__edges_stop;
void __sanitizer_cov_reset_edgeguards() {
  uint64_t N = 0;
  for (uint32_t* x = __edges_start; x < __edges_stop && N < MAX_EDGES; x++)
    *x = ++N;
}

extern "C" void __sanitizer_cov_trace_pc_guard_init(uint32_t* start,
                                                    uint32_t* stop) {
  // Avoid duplicate initialization
  if (start == stop || *start) return;

  if (__edges_start != NULL || __edges_stop != NULL) {
    fprintf(stderr,
            "Coverage instrumentation is only supported for a single module\n");
    _exit(-1);
  }

  __edges_start = start;
  __edges_stop = stop;

  // Map the shared memory region
  const char* shm_key = getenv("SHM_ID");
  if (!shm_key) {
    puts("[COV] no shared memory bitmap available, skipping");
    __shmem = (struct shmem_data*)malloc(SHM_SIZE);
  } else {
    int fd = shm_open(shm_key, O_RDWR, S_IREAD | S_IWRITE);
    if (fd <= -1) {
      fprintf(stderr, "Failed to open shared memory region: %s\n",
              strerror(errno));
      _exit(-1);
    }

    __shmem = (struct shmem_data*)mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE,
                                       MAP_SHARED, fd, 0);
    if (__shmem == MAP_FAILED) {
      fprintf(stderr, "Failed to mmap shared memory region\n");
      _exit(-1);
    }
  }

  __sanitizer_cov_reset_edgeguards();

  __shmem->num_edges = stop - start;
  printf("[COV] edge counters initialized. Shared memory: %s with %u edges\n",
         shm_key, __shmem->num_edges);
}

extern "C" void __sanitizer_cov_trace_pc_guard(uint32_t* guard) {
  // There's a small race condition here: if this function executes in two
  // threads for the same edge at the same time, the first thread might disable
  // the edge (by setting the guard to zero) before the second thread fetches
  // the guard value (and thus the index). However, our instrumentation ignores
  // the first edge (see libcoverage.c) and so the race is unproblematic.
  uint32_t index = *guard;
  // If this function is called before coverage instrumentation is properly
  // initialized we want to return early.
  if (!index) return;
  __shmem->edges[index / 8] |= 1 << (index % 8);
  *guard = 0;
}
#endif /* FUZZING_JS_FUZZILLI */

enum JSShellExitCode {
  EXITCODE_RUNTIME_ERROR = 3,
  EXITCODE_FILE_NOT_FOUND = 4,
  EXITCODE_OUT_OF_MEMORY = 5,
  EXITCODE_TIMEOUT = 6
};

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

  OffThreadJob(ShellContext* sc, OffThreadJobKind kind, Source&& source);
  ~OffThreadJob();

  void cancel();
  void markDone(JS::OffThreadToken* newToken);
  JS::OffThreadToken* waitUntilDone(JSContext* cx);

  char16_t* sourceChars() { return source.as<UniqueTwoByteChars>().get(); }
  JS::TranscodeBuffer& xdrBuffer() { return source.as<JS::TranscodeBuffer>(); }

 public:
  const int32_t id;
  const OffThreadJobKind kind;

 private:
  js::Monitor& monitor;
  State state;
  JS::OffThreadToken* token;
  Source source;
};

static OffThreadJob* NewOffThreadJob(JSContext* cx, OffThreadJobKind kind,
                                     CompileOptions& options,
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

static OffThreadJob* GetSingleOffThreadJob(JSContext* cx,
                                           OffThreadJobKind kind) {
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

static OffThreadJob* LookupOffThreadJobByID(JSContext* cx,
                                            OffThreadJobKind kind, int32_t id) {
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

static OffThreadJob* LookupOffThreadJobForArgs(JSContext* cx,
                                               OffThreadJobKind kind,
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

OffThreadJob::OffThreadJob(ShellContext* sc, OffThreadJobKind kind,
                           Source&& source)
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
  GCPtr<JSObject*> grayRoot;
};

struct MOZ_STACK_CLASS EnvironmentPreparer
    : public js::ScriptEnvironmentPreparer {
  explicit EnvironmentPreparer(JSContext* cx) {
    js::SetScriptEnvironmentPreparer(cx, this);
  }
  void invoke(JS::HandleObject global, Closure& closure) override;
};

const char* shell::selfHostedXDRPath = nullptr;
bool shell::encodeSelfHostedCode = false;
bool shell::enableCodeCoverage = false;
bool shell::enableDisassemblyDumps = false;
bool shell::offthreadCompilation = false;
JS::DelazificationOption shell::defaultDelazificationMode =
    JS::DelazificationOption::OnDemandOnly;
bool shell::enableAsmJS = false;
bool shell::enableWasm = false;
bool shell::enableSharedMemory = SHARED_MEMORY_DEFAULT;
bool shell::enableWasmBaseline = false;
bool shell::enableWasmOptimizing = false;

#define WASM_DEFAULT_FEATURE(NAME, ...) bool shell::enableWasm##NAME = true;
#define WASM_EXPERIMENTAL_FEATURE(NAME, ...) \
  bool shell::enableWasm##NAME = false;
JS_FOR_WASM_FEATURES(WASM_DEFAULT_FEATURE, WASM_DEFAULT_FEATURE,
                     WASM_EXPERIMENTAL_FEATURE);
#undef WASM_DEFAULT_FEATURE
#undef WASM_EXPERIMENTAL_FEATURE

#ifdef ENABLE_WASM_SIMD_WORMHOLE
bool shell::enableWasmSimdWormhole = false;
#endif
bool shell::enableWasmVerbose = false;
bool shell::enableTestWasmAwaitTier2 = false;
bool shell::enableSourcePragmas = true;
bool shell::enableAsyncStacks = false;
bool shell::enableAsyncStackCaptureDebuggeeOnly = false;
bool shell::enableStreams = false;
bool shell::enableWeakRefs = false;
bool shell::enableToSource = false;
bool shell::enablePropertyErrorMessageFix = false;
bool shell::enableIteratorHelpers = false;
#ifdef NIGHTLY_BUILD
bool shell::enableArrayGrouping = true;
#endif
#ifdef ENABLE_CHANGE_ARRAY_BY_COPY
bool shell::enableChangeArrayByCopy = false;
#endif
#ifdef ENABLE_NEW_SET_METHODS
bool shell::enableNewSetMethods = true;
#endif
bool shell::enableImportAssertions = false;
#ifdef JS_GC_ZEAL
uint32_t shell::gZealBits = 0;
uint32_t shell::gZealFrequency = 0;
#endif
bool shell::printTiming = false;
RCFile* shell::gErrFile = nullptr;
RCFile* shell::gOutFile = nullptr;
bool shell::reportWarnings = true;
bool shell::compileOnly = false;
bool shell::disableOOMFunctions = false;
bool shell::defaultToSameCompartment = true;

bool shell::useFdlibmForSinCosTan = false;

#ifdef DEBUG
bool shell::dumpEntrainedVariables = false;
bool shell::OOM_printAllocationCount = false;
#endif

UniqueChars shell::processWideModuleLoadPath;

static bool SetTimeoutValue(JSContext* cx, double t);

static void KillWatchdog(JSContext* cx);

static bool ScheduleWatchdog(JSContext* cx, double t);

static void CancelExecution(JSContext* cx);

enum class ShellGlobalKind {
  GlobalObject,
  WindowProxy,
};

static JSObject* NewGlobalObject(JSContext* cx, JS::RealmOptions& options,
                                 JSPrincipals* principals, ShellGlobalKind kind,
                                 bool immutablePrototype);

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
      finalizationRegistryCleanupCallbacks(cx) {}

ShellContext::~ShellContext() { MOZ_ASSERT(offThreadJobs.empty()); }

ShellContext* js::shell::GetShellContext(JSContext* cx) {
  ShellContext* sc = static_cast<ShellContext*>(JS_GetContextPrivate(cx));
  MOZ_ASSERT(sc);
  return sc;
}

static bool TraceGrayRoots(JSTracer* trc, SliceBudget& budget, void* data) {
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

  return true;
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

  MOZ_ASSERT(JS::GetScriptPrivate(script).isUndefined());
  RootedObject infoObject(cx, js::CreateScriptPrivate(cx, path));
  if (!infoObject) {
    return false;
  }

  JS::SetScriptPrivate(script, ObjectValue(*infoObject));
  return true;
}

enum class CompileUtf8 {
  InflateToUtf16,
  DontInflate,
};

[[nodiscard]] static bool RunFile(JSContext* cx, const char* filename,
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
        .setNoScriptRval(true)
        .setEagerDelazificationStrategy(defaultDelazificationMode);

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
              JS::UTF8Chars(reinterpret_cast<const char*>(buffer.begin()),
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

[[nodiscard]] static bool RunModule(JSContext* cx, const char* filename,
                                    bool compileOnly) {
  ShellContext* sc = GetShellContext(cx);

  RootedString path(cx, JS_NewStringCopyZ(cx, filename));
  if (!path) {
    return false;
  }

  path = ResolvePath(cx, path, RootRelative);
  if (!path) {
    return false;
  }

  return sc->moduleLoader->loadRootModule(cx, path);
}

static void ShellCleanupFinalizationRegistryCallback(JSFunction* doCleanup,
                                                     JSObject* incumbentGlobal,
                                                     void* data) {
  // In the browser this queues a task. Shell jobs correspond to microtasks so
  // we arrange for cleanup to happen after all jobs/microtasks have run. The
  // incumbent global is ignored in the shell.

  auto sc = static_cast<ShellContext*>(data);
  AutoEnterOOMUnsafeRegion oomUnsafe;
  if (!sc->finalizationRegistryCleanupCallbacks.append(doCleanup)) {
    oomUnsafe.crash("ShellCleanupFinalizationRegistryCallback");
  }
}

// Run any FinalizationRegistry cleanup tasks and return whether any ran.
static bool MaybeRunFinalizationRegistryCleanupTasks(JSContext* cx) {
  ShellContext* sc = GetShellContext(cx);
  MOZ_ASSERT(!sc->quitting);

  Rooted<ShellContext::FunctionVector> callbacks(cx);
  std::swap(callbacks.get(), sc->finalizationRegistryCleanupCallbacks.get());

  bool ranTasks = false;

  RootedFunction callback(cx);
  for (JSFunction* f : callbacks) {
    callback = f;

    AutoRealm ar(cx, f);

    {
      AutoReportException are(cx);
      RootedValue unused(cx);
      (void)JS_CallFunction(cx, nullptr, callback, HandleValueArray::empty(),
                            &unused);
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

// clang-format off
#define LIT(NAME) #NAME,
static const char* telemetryNames[JS_TELEMETRY_END + 1] = {
  MAP_JS_TELEMETRY(LIT)
  "JS_TELEMETRY_END"
};
#undef LIT
// clang-format on

// Telemetry can be executed from multiple threads, and the callback is
// responsible to avoid contention on the recorded telemetry data.
static Mutex* telemetryLock = nullptr;
class MOZ_RAII AutoLockTelemetry : public LockGuard<Mutex> {
  using Base = LockGuard<Mutex>;

 public:
  AutoLockTelemetry() : Base(*telemetryLock) { MOZ_ASSERT(telemetryLock); }
};

using TelemetryData = uint32_t;
using TelemetryVec = Vector<TelemetryData, 0, SystemAllocPolicy>;
static mozilla::Array<TelemetryVec, JS_TELEMETRY_END> telemetryResults;
static void AccumulateTelemetryDataCallback(int id, uint32_t sample,
                                            const char* key) {
  AutoLockTelemetry alt;
  // We ignore OOMs while writting teleemtry data.
  if (telemetryResults[id].append(sample)) {
    return;
  }
}

static void WriteTelemetryDataToDisk(const char* dir) {
  const int pathLen = 260;
  char fileName[pathLen];
  Fprinter output;
  auto initOutput = [&](const char* name) -> bool {
    if (SprintfLiteral(fileName, "%s%s.csv", dir, name) >= pathLen) {
      return false;
    }
    FILE* file = fopen(fileName, "a");
    if (!file) {
      return false;
    }
    output.init(file);
    return true;
  };

  for (size_t id = 0; id < JS_TELEMETRY_END; id++) {
    auto clear = MakeScopeExit([&] { telemetryResults[id].clearAndFree(); });
    if (!initOutput(telemetryNames[id])) {
      continue;
    }
    for (uint32_t data : telemetryResults[id]) {
      output.printf("%u\n", data);
    }
    output.finish();
  }
}

#undef MAP_TELEMETRY

static bool BoundToAsyncStack(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  RootedValue function(cx, GetFunctionNativeReserved(&args.callee(), 0));
  RootedObject options(
      cx, &GetFunctionNativeReserved(&args.callee(), 1).toObject());

  Rooted<SavedFrame*> stack(cx, nullptr);
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
      JS_FS_END};

  if (!JS_DefineFunctions(cx, intl, funcs)) {
    return false;
  }

  if (!JS::AddMozDateTimeFormatConstructor(cx, intl)) {
    return false;
  }

  if (!JS::AddMozDisplayNamesConstructor(cx, intl)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}
#endif  // JS_HAS_INTL_API

[[nodiscard]] static bool EvalUtf8AndPrint(JSContext* cx, const char* bytes,
                                           size_t length, int lineno,
                                           bool compileOnly) {
  // Eval.
  JS::CompileOptions options(cx);
  options.setIntroductionType("js shell interactive")
      .setIsRunOnce(true)
      .setFileAndLine("typein", lineno)
      .setEagerDelazificationStrategy(defaultDelazificationMode);

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

[[nodiscard]] static bool ReadEvalPrintLoop(JSContext* cx, FILE* in,
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
      (void)EvalUtf8AndPrint(cx, buffer.begin(), buffer.length(), startline,
                             compileOnly);
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

[[nodiscard]] static bool Process(JSContext* cx, const char* filename,
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
        if (!RunModule(cx, filename, compileOnly)) {
          return false;
        }
        break;
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

static void freeExternalCallback(void* contents, void* userData) {
  MOZ_ASSERT(!userData);
  js_free(contents);
}

static bool CreateExternalArrayBuffer(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 1) {
    JS_ReportErrorNumberASCII(
        cx, my_GetErrorMessage, nullptr,
        args.length() < 1 ? JSSMSG_NOT_ENOUGH_ARGS : JSSMSG_TOO_MANY_ARGS,
        "createExternalArrayBuffer");
    return false;
  }

  int32_t bytes = 0;
  if (!ToInt32(cx, args[0], &bytes)) {
    return false;
  }

  if (bytes <= 0) {
    JS_ReportErrorASCII(cx, "Size must be positive");
    return false;
  }

  void* buffer = js_malloc(bytes);
  if (!buffer) {
    JS_ReportOutOfMemory(cx);
    return false;
  }

  RootedObject arrayBuffer(
      cx, JS::NewExternalArrayBuffer(cx, bytes, buffer, &freeExternalCallback));
  if (!arrayBuffer) {
    return false;
  }

  args.rval().setObject(*arrayBuffer);
  return true;
}

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

    Rooted<JSLinearString*> opt(cx, str->ensureLinear(cx));
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
        .setNoScriptRval(true)
        .setEagerDelazificationStrategy(defaultDelazificationMode);

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

static void my_LargeAllocFailCallback() {
  JSContext* cx = TlsContext.get();
  if (!cx || cx->isHelperThreadContext()) {
    return;
  }

  MOZ_ASSERT(!JS::RuntimeHeapIsBusy());

  JS::PrepareForFullGC(cx);
  cx->runtime()->gc.gc(JS::GCOptions::Shrink,
                       JS::GCReason::SHARED_MEMORY_LIMIT);
}

static const uint32_t CacheEntry_SOURCE = 0;
static const uint32_t CacheEntry_BYTECODE = 1;
static const uint32_t CacheEntry_OPTIONS = 2;

// Some compile options can't be combined differently between save and load.
//
// CacheEntries store a CacheOption set, and on load an exception is thrown
// if the entries are incompatible.

enum CacheOptions : uint32_t {
  IsRunOnce,
  NoScriptRval,
  Global,
  NonSyntactic,
  SourceIsLazy,
  ForceFullParse,
};

struct CacheOptionSet : public mozilla::EnumSet<CacheOptions> {
  using mozilla::EnumSet<CacheOptions>::EnumSet;

  explicit CacheOptionSet(const CompileOptions& options) : EnumSet() {
    initFromOptions(options);
  }

  void initFromOptions(const CompileOptions& options) {
    if (options.noScriptRval) {
      *this += CacheOptions::NoScriptRval;
    }
    if (options.isRunOnce) {
      *this += CacheOptions::IsRunOnce;
    }
    if (options.sourceIsLazy) {
      *this += CacheOptions::SourceIsLazy;
    }
    if (options.forceFullParse()) {
      *this += CacheOptions::ForceFullParse;
    }
    if (options.nonSyntacticScope) {
      *this += CacheOptions::NonSyntactic;
    }
  }
};

static bool CacheOptionsCompatible(const CacheOptionSet& a,
                                   const CacheOptionSet& b) {
  // If the options are identical, they are trivially compatible.
  return a == b;
}

static const JSClass CacheEntry_class = {"CacheEntryObject",
                                         JSCLASS_HAS_RESERVED_SLOTS(3)};

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

  JS::SetReservedSlot(obj, CacheEntry_SOURCE, args[0]);
  JS::SetReservedSlot(obj, CacheEntry_BYTECODE, UndefinedValue());

  // Fill in empty option set.
  CacheOptionSet defaultOptions;
  JS::SetReservedSlot(obj, CacheEntry_OPTIONS,
                      Int32Value(defaultOptions.serialize()));

  args.rval().setObject(*obj);
  return true;
}

static bool CacheEntry_isCacheEntry(JSObject* cache) {
  return cache->hasClass(&CacheEntry_class);
}

static JSString* CacheEntry_getSource(JSContext* cx, HandleObject cache) {
  MOZ_ASSERT(CacheEntry_isCacheEntry(cache));
  Value v = JS::GetReservedSlot(cache, CacheEntry_SOURCE);
  if (!v.isString()) {
    JS_ReportErrorASCII(
        cx, "CacheEntry_getSource: Unexpected type of source reserved slot.");
    return nullptr;
  }

  return v.toString();
}

static bool CacheEntry_compatible(JSContext* cx, HandleObject cache,
                                  const CacheOptionSet& currentOptionSet) {
  CacheOptionSet cacheEntryOptions;
  MOZ_ASSERT(CacheEntry_isCacheEntry(cache));
  Value v = JS::GetReservedSlot(cache, CacheEntry_OPTIONS);
  cacheEntryOptions.deserialize(v.toInt32());
  if (!CacheOptionsCompatible(cacheEntryOptions, currentOptionSet)) {
    JS_ReportErrorASCII(cx,
                        "CacheEntry_compatible: Incompatible cache contents");
    return false;
  }
  return true;
}

static uint8_t* CacheEntry_getBytecode(JSContext* cx, HandleObject cache,
                                       size_t* length) {
  MOZ_ASSERT(CacheEntry_isCacheEntry(cache));
  Value v = JS::GetReservedSlot(cache, CacheEntry_BYTECODE);
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
                                   const CacheOptionSet& cacheOptions,
                                   uint8_t* buffer, uint32_t length) {
  MOZ_ASSERT(CacheEntry_isCacheEntry(cache));

  using BufferContents = ArrayBufferObject::BufferContents;

  BufferContents contents = BufferContents::createMalloced(buffer);
  Rooted<ArrayBufferObject*> arrayBuffer(
      cx, ArrayBufferObject::createForContents(cx, length, contents));
  if (!arrayBuffer) {
    return false;
  }

  JS::SetReservedSlot(cache, CacheEntry_BYTECODE, ObjectValue(*arrayBuffer));
  JS::SetReservedSlot(cache, CacheEntry_OPTIONS,
                      Int32Value(cacheOptions.serialize()));
  return true;
}

static bool ConvertTranscodeResultToJSException(JSContext* cx,
                                                JS::TranscodeResult rv) {
  switch (rv) {
    case JS::TranscodeResult::Ok:
      return true;

    default:
      [[fallthrough]];
    case JS::TranscodeResult::Failure:
      MOZ_ASSERT(!cx->isExceptionPending());
      JS_ReportErrorASCII(cx, "generic warning");
      return false;
    case JS::TranscodeResult::Failure_BadBuildId:
      MOZ_ASSERT(!cx->isExceptionPending());
      JS_ReportErrorASCII(cx, "the build-id does not match");
      return false;
    case JS::TranscodeResult::Failure_AsmJSNotSupported:
      MOZ_ASSERT(!cx->isExceptionPending());
      JS_ReportErrorASCII(cx, "Asm.js is not supported by XDR");
      return false;
    case JS::TranscodeResult::Failure_BadDecode:
      MOZ_ASSERT(!cx->isExceptionPending());
      JS_ReportErrorASCII(cx, "XDR data corruption");
      return false;

    case JS::TranscodeResult::Throw:
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

  RootedObject opts(cx);
  if (args.length() == 2) {
    if (!args[1].isObject()) {
      JS_ReportErrorASCII(cx, "evaluate: The 2nd argument must be an object");
      return false;
    }

    opts = &args[1].toObject();
  }

  RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  MOZ_ASSERT(global);

  // Check "global" property before everything to use the given global's
  // option as the default value.
  Maybe<CompileOptions> maybeOptions;
  if (opts) {
    RootedValue v(cx);
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
      if (!global || !(JS::GetClass(global)->flags & JSCLASS_IS_GLOBAL)) {
        JS_ReportErrorNumberASCII(
            cx, GetErrorMessage, nullptr, JSMSG_UNEXPECTED_TYPE,
            "\"global\" passed to evaluate()", "not a global object");
        return false;
      }

      JSAutoRealm ar(cx, global);
      maybeOptions.emplace(cx);
    }
  }
  if (!maybeOptions) {
    // If "global" property is not given, use the current global's option as
    // the default value.
    maybeOptions.emplace(cx);
  }

  CompileOptions& options = maybeOptions.ref();
  UniqueChars fileNameBytes;
  RootedString displayURL(cx);
  RootedString sourceMapURL(cx);
  bool catchTermination = false;
  bool loadBytecode = false;
  bool saveIncrementalBytecode = false;
  bool transcodeOnly = false;
  bool assertEqBytecode = false;
  JS::RootedObjectVector envChain(cx);
  RootedObject callerGlobal(cx, cx->global());
  CacheOptionSet optionSet;

  options.setIntroductionType("js shell evaluate")
      .setFileAndLine("@evaluate", 1)
      .setDeferDebugMetadata();

  options.borrowBuffer = true;

  RootedValue privateValue(cx);
  RootedString elementAttributeName(cx);

  if (opts) {
    if (!js::ParseCompileOptions(cx, options, opts, &fileNameBytes)) {
      return false;
    }
    if (!ParseDebugMetadata(cx, opts, &privateValue, &elementAttributeName)) {
      return false;
    }
    if (!ParseSourceOptions(cx, opts, &displayURL, &sourceMapURL)) {
      return false;
    }

    RootedValue v(cx);
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

    if (!JS_GetProperty(cx, opts, "saveIncrementalBytecode", &v)) {
      return false;
    }
    if (!v.isUndefined()) {
      saveIncrementalBytecode = ToBoolean(v);
    }

    if (!JS_GetProperty(cx, opts, "transcodeOnly", &v)) {
      return false;
    }
    if (!v.isUndefined()) {
      transcodeOnly = ToBoolean(v);
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
    if (loadBytecode || saveIncrementalBytecode) {
      if (!cacheEntry) {
        JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                                  JSSMSG_INVALID_ARGS, "evaluate");
        return false;
      }
    }
  }

  if (envChain.length() != 0) {
    {
      // Wrap the envChainObject list into target realm.
      JSAutoRealm ar(cx, global);
      for (size_t i = 0; i < envChain.length(); ++i) {
        if (!JS_WrapObject(cx, envChain[i])) {
          return false;
        }
      }
    }

    options.setNonSyntacticScope(true);
  }

  optionSet.initFromOptions(options);

  AutoStableStringChars codeChars(cx);
  if (!codeChars.initTwoByte(cx, code)) {
    return false;
  }

  JS::TranscodeBuffer loadBuffer;
  JS::TranscodeBuffer saveBuffer;

  if (loadBytecode) {
    size_t loadLength = 0;
    uint8_t* loadData = nullptr;

    if (!CacheEntry_compatible(cx, cacheEntry, optionSet)) {
      return false;
    }

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
      if (loadBytecode) {
        JS::TranscodeRange range(loadBuffer.begin(), loadBuffer.length());

        RefPtr<JS::Stencil> stencil;

        JS::DecodeOptions decodeOptions(options);

        JS::TranscodeResult rv = JS::DecodeStencil(cx, decodeOptions, range,
                                                   getter_AddRefs(stencil));
        if (!ConvertTranscodeResultToJSException(cx, rv)) {
          return false;
        }

        JS::InstantiateOptions instantiateOptions(options);
        script = JS::InstantiateGlobalStencil(cx, instantiateOptions, stencil);
        if (!script) {
          return false;
        }

        if (saveIncrementalBytecode) {
          if (!JS::StartIncrementalEncoding(cx, std::move(stencil))) {
            return false;
          }
        }
      } else {
        mozilla::Range<const char16_t> chars = codeChars.twoByteRange();
        JS::SourceText<char16_t> srcBuf;
        if (!srcBuf.init(cx, chars.begin().get(), chars.length(),
                         JS::SourceOwnership::Borrowed)) {
          return false;
        }

        if (saveIncrementalBytecode) {
          script = JS::CompileAndStartIncrementalEncoding(cx, options, srcBuf);
          if (!script) {
            return false;
          }
        } else {
          script = JS::Compile(cx, options, srcBuf);
          if (!script) {
            return false;
          }
        }
      }
    }

    if (!SetSourceOptions(cx, script->scriptSource(), displayURL,
                          sourceMapURL)) {
      return false;
    }

    JS::InstantiateOptions instantiateOptions(options);
    if (!JS::UpdateDebugMetadata(cx, script, instantiateOptions, privateValue,
                                 elementAttributeName, nullptr, nullptr)) {
      return false;
    }

    if (!transcodeOnly) {
      if (!(envChain.empty()
                ? JS_ExecuteScript(cx, script, args.rval())
                : JS_ExecuteScript(cx, envChain, script, args.rval()))) {
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
    }

    // Serialize the encoded bytecode, recorded before the execution, into a
    // buffer which can be deserialized linearly.
    if (saveIncrementalBytecode) {
      if (!FinishIncrementalEncoding(cx, script, saveBuffer)) {
        return false;
      }
    }
  }

  if (saveIncrementalBytecode) {
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
    if (!CacheEntry_setBytecode(cx, cacheEntry, optionSet, saveData,
                                saveLength)) {
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
        .setNoScriptRval(true)
        .setEagerDelazificationStrategy(defaultDelazificationMode);

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

static int js_fgets(char* buf, int size, FILE* file) {
  int n, i, c;
  bool crflag;

  n = size - 1;
  if (n < 0) {
    return -1;
  }

  // Use the fastest available getc.
  auto fast_getc =
#if defined(HAVE_GETC_UNLOCKED)
      getc_unlocked
#elif defined(HAVE__GETC_NOLOCK)
      _getc_nolock
#else
      getc
#endif
      ;

  crflag = false;
  for (i = 0; i < n && (c = fast_getc(file)) != EOF; i++) {
    buf[i] = c;
    if (c == '\n') {  // any \n ends a line
      i++;            // keep the \n; we know there is room for \0
      break;
    }
    if (crflag) {  // \r not followed by \n ends line at the \r
      ungetc(c, file);
      break;  // and overwrite c in buf with \0
    }
    crflag = (c == '\r');
  }

  buf[i] = '\0';
  return i;
}

/*
 * function readline()
 * Provides a hook for scripts to read a line from stdin.
 */
static bool ReadLine(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  static constexpr size_t BUFSIZE = 256;
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

  // Print a message to stderr in differential testing to help jsfunfuzz
  // find uncatchable-exception bugs.
  if (js::SupportDifferentialTesting()) {
    fprintf(stderr, "quit called\n");
  }

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
    JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                              (args.length() < 2)    ? JSSMSG_NOT_ENOUGH_ARGS
                              : (args.length() == 3) ? JSSMSG_INVALID_ARGS
                                                     : JSSMSG_TOO_MANY_ARGS,
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
    if (v.isObject() && JS::GetClass(&v.toObject())->isJSFunction()) {
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

[[nodiscard]] static bool SrcNotes(JSContext* cx, HandleScript script,
                                   Sprinter* sp) {
  if (!sp->put("\nSource notes:\n") ||
      !sp->jsprintf("%4s %4s %6s %5s %6s %-10s %s\n", "ofs", "line", "column",
                    "pc", "delta", "desc", "args") ||
      !sp->put("---- ---- ------ ----- ------ ---------- ------\n")) {
    return false;
  }

  unsigned offset = 0;
  unsigned lineno = script->lineno();
  unsigned column = script->column();
  SrcNote* notes = script->notes();
  for (SrcNoteIterator iter(notes); !iter.atEnd(); ++iter) {
    auto sn = *iter;

    unsigned delta = sn->delta();
    offset += delta;
    SrcNoteType type = sn->type();
    const char* name = sn->name();
    if (!sp->jsprintf("%3u: %4u %6u %5u [%4u] %-10s", unsigned(sn - notes),
                      lineno, column, offset, delta, name)) {
      return false;
    }

    switch (type) {
      case SrcNoteType::Null:
      case SrcNoteType::AssignOp:
      case SrcNoteType::Breakpoint:
      case SrcNoteType::StepSep:
      case SrcNoteType::XDelta:
        break;

      case SrcNoteType::ColSpan: {
        uint32_t colspan = SrcNote::ColSpan::getSpan(sn);
        if (!sp->jsprintf(" colspan %u", colspan)) {
          return false;
        }
        column += colspan;
        break;
      }

      case SrcNoteType::SetLine:
        lineno = SrcNote::SetLine::getLine(sn, script->lineno());
        if (!sp->jsprintf(" lineno %u", lineno)) {
          return false;
        }
        column = 0;
        break;

      case SrcNoteType::NewLine:
        ++lineno;
        column = 0;
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

[[nodiscard]] static bool TryNotes(JSContext* cx, HandleScript script,
                                   Sprinter* sp) {
  if (!sp->put(
          "\nException table:\nkind               stack    start      end\n")) {
    return false;
  }

  for (const js::TryNote& tn : script->trynotes()) {
    if (!sp->jsprintf(" %-16s %6u %8u %8u\n", TryNoteName(tn.kind()),
                      tn.stackDepth, tn.start, tn.start + tn.length)) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] static bool ScopeNotes(JSContext* cx, HandleScript script,
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
      if (!sp->jsprintf("%8u ", note.index.index)) {
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

[[nodiscard]] static bool GCThings(JSContext* cx, HandleScript script,
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
    } else if (gcThing.is<JSString>()) {
      RootedString str(cx, &gcThing.as<JSString>());
      if (str->isAtom()) {
        if (!sp->put("Atom       ")) {
          return false;
        }
      } else {
        if (!sp->put("String     ")) {
          return false;
        }
      }
      UniqueChars chars = QuoteString(cx, str, '"');
      if (!chars) {
        return false;
      }
      if (!sp->put(chars.get())) {
        return false;
      }
      if (!sp->put("\n")) {
        return false;
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

[[nodiscard]] static bool DisassembleScript(JSContext* cx, HandleScript script,
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
          if (!script || !DisassembleScript(cx, script, fun, lines, recursive,
                                            sourceNotes, gcThings, sp)) {
            return false;
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
      if (value.isObject() && value.toObject().is<ShellModuleObjectWrapper>()) {
        script = value.toObject()
                     .as<ShellModuleObjectWrapper>()
                     .get()
                     ->maybeScript();
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
        .setNoScriptRval(true)
        .setEagerDelazificationStrategy(defaultDelazificationMode);

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

#ifdef JS_CACHEIR_SPEW
static bool CacheIRHealthReport(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  js::jit::CacheIRHealth cih;
  RootedScript script(cx);

  // In the case that we are calling this function from the shell and
  // the environment variable is not set, AutoSpewChannel automatically
  // sets and unsets the proper channel for the duration of spewing
  // a health report.
  AutoSpewChannel channel(cx, SpewChannel::CacheIRHealthReport, script);
  if (!argc) {
    // Calling CacheIRHealthReport without any arguments will create health
    // reports for all scripts in the zone.
    for (auto base = cx->zone()->cellIter<BaseScript>(); !base.done();
         base.next()) {
      if (!base->hasJitScript() || base->selfHosted()) {
        continue;
      }

      script = base->asJSScript();
      cih.healthReportForScript(cx, script, js::jit::SpewContext::Shell);
    }
  } else {
    RootedValue value(cx, args.get(0));

    if (value.isObject() && value.toObject().is<ShellModuleObjectWrapper>()) {
      script =
          value.toObject().as<ShellModuleObjectWrapper>().get()->maybeScript();
    } else {
      script = TestingFunctionArgumentToScript(cx, args.get(0));
    }

    if (!script) {
      return false;
    }

    cih.healthReportForScript(cx, script, js::jit::SpewContext::Shell);
  }

  args.rval().setUndefined();
  return true;
}
#endif /* JS_CACHEIR_SPEW */

/* Pretend we can always preserve wrappers for dummy DOM objects. */
static bool DummyPreserveWrapperCallback(JSContext* cx, HandleObject obj) {
  return true;
}

static bool DummyHasReleasedWrapperCallback(HandleObject obj) { return true; }

#ifdef FUZZING_JS_FUZZILLI
// We have to assume that the fuzzer will be able to call this function e.g. by
// enumerating the properties of the global object and eval'ing them. As such
// this function is implemented in a way that requires passing some magic value
// as first argument (with the idea being that the fuzzer won't be able to
// generate this value) which then also acts as a selector for the operation
// to perform.
static bool Fuzzilli(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  RootedString arg(cx, JS::ToString(cx, args.get(0)));
  if (!arg) {
    return false;
  }
  Rooted<JSLinearString*> operation(cx, StringToLinearString(cx, arg));
  if (!operation) {
    return false;
  }

  if (StringEqualsAscii(operation, "FUZZILLI_CRASH")) {
    int type;
    if (!ToInt32(cx, args.get(1), &type)) {
      return false;
    }

    // With this, we can test the various ways the JS shell can crash and make
    // sure that Fuzzilli is able to detect all of these failures properly.
    switch (type) {
      case 0:
        *((int*)0x41414141) = 0x1337;
        break;
      case 1:
        MOZ_RELEASE_ASSERT(false);
        break;
      case 2:
        MOZ_ASSERT(false);
        break;
      case 3:
        __asm__("int3");
        break;
      default:
        exit(1);
    }
  } else if (StringEqualsAscii(operation, "FUZZILLI_PRINT")) {
    static FILE* fzliout = fdopen(REPRL_DWFD, "w");
    if (!fzliout) {
      fprintf(
          stderr,
          "Fuzzer output channel not available, printing to stdout instead\n");
      fzliout = stdout;
    }

    RootedString str(cx, JS::ToString(cx, args.get(1)));
    if (!str) {
      return false;
    }
    UniqueChars bytes = JS_EncodeStringToUTF8(cx, str);
    if (!bytes) {
      return false;
    }
    fprintf(fzliout, "%s\n", bytes.get());
    fflush(fzliout);
  }

  args.rval().setUndefined();
  return true;
}

static bool FuzzilliReprlGetAndRun(JSContext* cx) {
  size_t scriptSize = 0;

  unsigned action;
  MOZ_RELEASE_ASSERT(read(REPRL_CRFD, &action, 4) == 4);
  if (action == 'cexe') {
    MOZ_RELEASE_ASSERT(read(REPRL_CRFD, &scriptSize, 8) == 8);
  } else {
    fprintf(stderr, "Unknown action: %u\n", action);
    _exit(-1);
  }

  CompileOptions options(cx);
  options.setIntroductionType("reprl")
      .setFileAndLine("reprl", 1)
      .setIsRunOnce(true)
      .setNoScriptRval(true)
      .setEagerDelazificationStrategy(defaultDelazificationMode);

  char* scriptSrc = static_cast<char*>(js_malloc(scriptSize));

  char* ptr = scriptSrc;
  size_t remaining = scriptSize;
  while (remaining > 0) {
    ssize_t rv = read(REPRL_DRFD, ptr, remaining);
    if (rv <= 0) {
      fprintf(stderr, "Failed to load script\n");
      _exit(-1);
    }
    remaining -= rv;
    ptr += rv;
  }

  JS::SourceText<Utf8Unit> srcBuf;
  if (!srcBuf.init(cx, scriptSrc, scriptSize,
                   JS::SourceOwnership::TakeOwnership)) {
    return false;
  }

  RootedScript script(cx, JS::Compile(cx, options, srcBuf));
  if (!script) {
    return false;
  }

  if (!JS_ExecuteScript(cx, script)) {
    return false;
  }

  return true;
}

#endif /* FUZZING_JS_FUZZILLI */

static bool FuzzilliUseReprlMode(OptionParser* op) {
#ifdef FUZZING_JS_FUZZILLI
  // Check if we should use REPRL mode
  bool reprl_mode = op->getBoolOption("reprl");
  if (reprl_mode) {
    // Check in with parent
    char helo[] = "HELO";
    if (write(REPRL_CWFD, helo, 4) != 4 || read(REPRL_CRFD, helo, 4) != 4) {
      reprl_mode = false;
    }

    if (memcmp(helo, "HELO", 4) != 0) {
      fprintf(stderr, "Invalid response from parent\n");
      _exit(-1);
    }
  }
  return reprl_mode;
#else
  return false;
#endif /* FUZZING_JS_FUZZILLI */
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

  // We don't have a stack here, so just initialize with null.
  JS::ExceptionStack exnStack(cx, args.get(0), nullptr);
  JS::ErrorReportBuilder report(cx);
  if (!report.init(cx, exnStack, JS::ErrorReportBuilder::WithSideEffects)) {
    return false;
  }

  MOZ_ASSERT(!report.report()->isWarning());

  RootedObject obj(cx, JS_NewPlainObject(cx));
  if (!obj) {
    return false;
  }

  RootedString toString(cx, NewStringCopyUTF8Z(cx, report.toStringResult()));
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
      .setWeakRefsEnabled(enableWeakRefs
                              ? JS::WeakRefSpecifier::EnabledWithCleanupSome
                              : JS::WeakRefSpecifier::Disabled)
      .setToSourceEnabled(enableToSource)
      .setPropertyErrorMessageFixEnabled(enablePropertyErrorMessageFix)
      .setIteratorHelpersEnabled(enableIteratorHelpers)
#ifdef NIGHTLY_BUILD
      .setArrayGroupingEnabled(enableArrayGrouping)
#endif
#ifdef ENABLE_NEW_SET_METHODS
      .setNewSetMethodsEnabled(enableNewSetMethods)
#endif
      ;
}

[[nodiscard]] static bool CheckRealmOptions(JSContext* cx,
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
    opts.setFileAndLine(filename.get(), lineno)
        .setEagerDelazificationStrategy(defaultDelazificationMode);

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
};

static void DestroyShellCompartmentPrivate(JS::GCContext* gcx,
                                           JS::Compartment* compartment) {
  auto priv = static_cast<ShellCompartmentPrivate*>(
      JS_GetCompartmentPrivate(compartment));
  js_delete(priv);
}

static void SetWorkerContextOptions(JSContext* cx);
static bool ShellBuildId(JS::BuildIdCharVector* buildId);

static constexpr size_t gWorkerStackSize = 2 * 128 * sizeof(size_t) * 1024;

static void WorkerMain(UniquePtr<WorkerInput> input) {
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
  });

  sc->isWorker = true;

  JS_SetContextPrivate(cx, sc);
  JS_SetGrayGCRootsTracer(cx, TraceGrayRoots, nullptr);
  SetWorkerContextOptions(cx);

  JS_SetFutexCanWait(cx);
  JS::SetWarningReporter(cx, WarningReporter);
  js::SetPreserveWrapperCallbacks(cx, DummyPreserveWrapperCallback,
                                  DummyHasReleasedWrapperCallback);
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
                                            ShellGlobalKind::WindowProxy,
                                            /* immutablePrototype = */ true));
    if (!global) {
      break;
    }

    JSAutoRealm ar(cx, global);

    JS::ConstUTF8CharsZ path(processWideModuleLoadPath.get(),
                             strlen(processWideModuleLoadPath.get()));
    RootedString moduleLoadPath(cx, JS_NewStringCopyUTF8Z(cx, path));
    if (!moduleLoadPath) {
      return;
    }
    sc->moduleLoader = js::MakeUnique<ModuleLoader>();
    if (!sc->moduleLoader || !sc->moduleLoader->init(cx, moduleLoadPath)) {
      return;
    }

    JS::CompileOptions options(cx);
    options.setFileAndLine("<string>", 1)
        .setIsRunOnce(true)
        .setEagerDelazificationStrategy(defaultDelazificationMode);

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
static Vector<UniquePtr<js::Thread>, 0, SystemAllocPolicy> workerThreads;

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

  auto input = js::MakeUnique<WorkerInput>(JS_GetParentRuntime(cx),
                                           std::move(chars), str->length());
  if (!input) {
    ReportOutOfMemory(cx);
    return false;
  }

  UniquePtr<Thread> thread;
  {
    AutoEnterOOMUnsafeRegion oomUnsafe;
    thread = js::MakeUnique<Thread>(
        Thread::Options().setStackSize(gWorkerStackSize + 512 * 1024));
    if (!thread || !thread->init(WorkerMain, std::move(input))) {
      oomUnsafe.crash("EvalInWorker");
    }
  }

  AutoLockWorkerThreads alwt;
  if (!workerThreads.append(std::move(thread))) {
    ReportOutOfMemory(cx);
    thread->join();
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

#ifdef __wasi__
  return false;
#endif

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
    UniquePtr<Thread> thread;
    {
      AutoLockWorkerThreads alwt;
      if (workerThreads.empty()) {
        break;
      }
      thread = std::move(workerThreads.back());
      workerThreads.popBack();
    }
    thread->join();
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

#ifdef DEBUG
// var s0 = "A".repeat(10*1024);
// interruptRegexp(/a(bc|bd)/, s0);
// first arg is regexp
// second arg is string
static bool InterruptRegexp(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  ShellContext* sc = GetShellContext(cx);
  RootedObject callee(cx, &args.callee());

  if (args.length() != 2) {
    ReportUsageErrorASCII(cx, callee, "Wrong number of arguments.");
    return false;
  }
  if (!(args[0].isObject() && args[0].toObject().is<RegExpObject>())) {
    ReportUsageErrorASCII(cx, callee,
                          "First argument must be a regular expression.");
    return false;
  }
  if (!args[1].isString()) {
    ReportUsageErrorASCII(cx, callee, "Second argument must be a String.");
    return false;
  }
  // Set interrupt flags
  sc->serviceInterrupt = true;
  js::irregexp::IsolateSetShouldSimulateInterrupt(cx->isolate);

  RootedObject regexp(cx, &args[0].toObject());
  RootedString string(cx, args[1].toString());
  int32_t lastIndex = 0;

  return js::RegExpMatcherRaw(cx, regexp, string, lastIndex, nullptr,
                              args.rval());
}
#endif

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
       opt == JSJITCOMPILER_WASM_JIT_OPTIMIZING) &&
      number == 0) {
    uint32_t baseline, optimizing;
    MOZ_ALWAYS_TRUE(JS_GetGlobalJitCompilerOption(
        cx, JSJITCOMPILER_WASM_JIT_BASELINE, &baseline));
    MOZ_ALWAYS_TRUE(JS_GetGlobalJitCompilerOption(
        cx, JSJITCOMPILER_WASM_JIT_OPTIMIZING, &optimizing));
    if (baseline + optimizing == 1) {
      if ((opt == JSJITCOMPILER_WASM_JIT_BASELINE && baseline) ||
          (opt == JSJITCOMPILER_WASM_JIT_OPTIMIZING && optimizing)) {
        JS_ReportErrorASCII(
            cx,
            "Disabling all the Wasm compilers at runtime is not supported.");
        return false;
      }
    }
  }

  // JIT compiler options are process-wide, so we have to stop off-thread
  // compilations for all runtimes to avoid races.
  WaitForAllHelperThreads();

  // Only release JIT code for the current runtime because there's no good
  // way to discard code for other runtimes.
  ReleaseAllJITCode(cx->gcContext());

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
  RootedString scriptContents(cx, args[0].toString());

  AutoStableStringChars stableChars(cx);
  if (!stableChars.initTwoByte(cx, scriptContents)) {
    return false;
  }

  JS::CompileOptions options(cx);
  options.setIntroductionType("js shell compile")
      .setFileAndLine("<string>", 1)
      .setIsRunOnce(true)
      .setNoScriptRval(true)
      .setDeferDebugMetadata();
  RootedValue privateValue(cx);
  RootedString elementAttributeName(cx);

  if (args.length() >= 2) {
    if (!args[1].isObject()) {
      JS_ReportErrorASCII(cx, "compile: The 2nd argument must be an object");
      return false;
    }

    RootedObject opts(cx, &args[1].toObject());
    if (!js::ParseCompileOptions(cx, options, opts, nullptr)) {
      return false;
    }
    if (!ParseDebugMetadata(cx, opts, &privateValue, &elementAttributeName)) {
      return false;
    }
  }

  JS::SourceText<char16_t> srcBuf;
  if (!srcBuf.init(cx, stableChars.twoByteRange().begin().get(),
                   scriptContents->length(), JS::SourceOwnership::Borrowed)) {
    return false;
  }

  RootedScript script(cx, JS::Compile(cx, options, srcBuf));
  if (!script) {
    return false;
  }

  JS::InstantiateOptions instantiateOptions(options);
  if (!JS::UpdateDebugMetadata(cx, script, instantiateOptions, privateValue,
                               elementAttributeName, nullptr, nullptr)) {
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
  options.setModule();

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

  Rooted<ShellModuleObjectWrapper*> wrapper(
      cx, ShellModuleObjectWrapper::create(cx, module.as<ModuleObject>()));
  if (!wrapper) {
    return false;
  }
  args.rval().setObject(*wrapper);
  return true;
}

// A JSObject that holds XDRBuffer.
class XDRBufferObject : public NativeObject {
  static const size_t VECTOR_SLOT = 0;
  static const unsigned RESERVED_SLOTS = 1;

 public:
  static const JSClassOps classOps_;
  static const JSClass class_;

  [[nodiscard]] inline static XDRBufferObject* create(
      JSContext* cx, JS::TranscodeBuffer&& buf);

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

  static void finalize(JS::GCContext* gcx, JSObject* obj);
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
    nullptr,                    // construct
    nullptr,                    // trace
};

/*static */ const JSClass XDRBufferObject::class_ = {
    "XDRBufferObject",
    JSCLASS_HAS_RESERVED_SLOTS(XDRBufferObject::RESERVED_SLOTS) |
        JSCLASS_BACKGROUND_FINALIZE,
    &XDRBufferObject::classOps_};

XDRBufferObject* XDRBufferObject::create(JSContext* cx,
                                         JS::TranscodeBuffer&& buf) {
  XDRBufferObject* bufObj =
      NewObjectWithGivenProto<XDRBufferObject>(cx, nullptr);
  if (!bufObj) {
    return nullptr;
  }

  auto heapBuf = cx->make_unique<JS::TranscodeBuffer>(std::move(buf));
  if (!heapBuf) {
    return nullptr;
  }

  size_t len = heapBuf->length();
  InitReservedSlot(bufObj, VECTOR_SLOT, heapBuf.release(), len,
                   MemoryUse::XDRBufferElements);

  return bufObj;
}

void XDRBufferObject::finalize(JS::GCContext* gcx, JSObject* obj) {
  XDRBufferObject* buf = &obj->as<XDRBufferObject>();
  if (buf->hasData()) {
    gcx->delete_(buf, buf->data(), buf->data()->length(),
                 MemoryUse::XDRBufferElements);
  }
}

static bool InstantiateModuleStencil(JSContext* cx, uint32_t argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!args.requireAtLeast(cx, "instantiateModuleStencil", 1)) {
    return false;
  }

  /* Prepare the input byte array. */
  if (!args[0].isObject() || !args[0].toObject().is<js::StencilObject>()) {
    JS_ReportErrorASCII(cx,
                        "instantiateModuleStencil: Stencil object expected");
    return false;
  }
  Rooted<js::StencilObject*> stencilObj(
      cx, &args[0].toObject().as<js::StencilObject>());

  if (!stencilObj->stencil()->isModule()) {
    JS_ReportErrorASCII(cx,
                        "instantiateModuleStencil: Module stencil expected");
    return false;
  }

  CompileOptions options(cx);
  UniqueChars fileNameBytes;
  if (args.length() == 2) {
    if (!args[1].isObject()) {
      JS_ReportErrorASCII(
          cx, "instantiateModuleStencil: The 2nd argument must be an object");
      return false;
    }

    RootedObject opts(cx, &args[1].toObject());
    if (!js::ParseCompileOptions(cx, options, opts, &fileNameBytes)) {
      return false;
    }
  }

  /* Prepare the CompilationStencil for decoding. */
  Rooted<frontend::CompilationInput> input(cx,
                                           frontend::CompilationInput(options));
  if (!input.get().initForModule(cx)) {
    return false;
  }

  /* Instantiate the stencil. */
  Rooted<frontend::CompilationGCOutput> output(cx);
  if (!frontend::CompilationStencil::instantiateStencils(
          cx, input.get(), *stencilObj->stencil(), output.get())) {
    return false;
  }

  Rooted<ModuleObject*> modObject(cx, output.get().module);
  Rooted<ShellModuleObjectWrapper*> wrapper(
      cx, ShellModuleObjectWrapper::create(cx, modObject));
  if (!wrapper) {
    return false;
  }
  args.rval().setObject(*wrapper);
  return true;
}

static bool InstantiateModuleStencilXDR(JSContext* cx, uint32_t argc,
                                        Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!args.requireAtLeast(cx, "instantiateModuleStencilXDR", 1)) {
    return false;
  }

  /* Prepare the input byte array. */
  if (!args[0].isObject() || !args[0].toObject().is<StencilXDRBufferObject>()) {
    JS_ReportErrorASCII(
        cx, "instantiateModuleStencilXDR: stencil XDR object expected");
    return false;
  }
  Rooted<StencilXDRBufferObject*> xdrObj(
      cx, &args[0].toObject().as<StencilXDRBufferObject>());
  MOZ_ASSERT(xdrObj->hasBuffer());

  CompileOptions options(cx);
  UniqueChars fileNameBytes;
  if (args.length() == 2) {
    if (!args[1].isObject()) {
      JS_ReportErrorASCII(
          cx,
          "instantiateModuleStencilXDR: The 2nd argument must be an object");
      return false;
    }

    RootedObject opts(cx, &args[1].toObject());
    if (!js::ParseCompileOptions(cx, options, opts, &fileNameBytes)) {
      return false;
    }
  }

  /* Prepare the CompilationStencil for decoding. */
  Rooted<frontend::CompilationInput> input(cx,
                                           frontend::CompilationInput(options));
  if (!input.get().initForModule(cx)) {
    return false;
  }
  frontend::CompilationStencil stencil(nullptr);

  /* Deserialize the stencil from XDR. */
  JS::TranscodeRange xdrRange(xdrObj->buffer(), xdrObj->bufferLength());
  bool succeeded = false;
  if (!stencil.deserializeStencils(cx, input.get(), xdrRange, &succeeded)) {
    return false;
  }
  if (!succeeded) {
    JS_ReportErrorASCII(cx, "Decoding failure");
    return false;
  }

  if (!stencil.isModule()) {
    JS_ReportErrorASCII(cx,
                        "instantiateModuleStencilXDR: Module stencil expected");
    return false;
  }

  /* Instantiate the stencil. */
  Rooted<frontend::CompilationGCOutput> output(cx);
  if (!frontend::CompilationStencil::instantiateStencils(
          cx, input.get(), stencil, output.get())) {
    return false;
  }

  Rooted<ModuleObject*> modObject(cx, output.get().module);
  Rooted<ShellModuleObjectWrapper*> wrapper(
      cx, ShellModuleObjectWrapper::create(cx, modObject));
  if (!wrapper) {
    return false;
  }
  args.rval().setObject(*wrapper);
  return true;
}

static bool RegisterModule(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "registerModule", 2)) {
    return false;
  }

  if (!args[0].isString()) {
    const char* typeName = InformalValueTypeName(args[0]);
    JS_ReportErrorASCII(cx, "expected string, got %s", typeName);
    return false;
  }

  if (!args[1].isObject() ||
      !args[1].toObject().is<ShellModuleObjectWrapper>()) {
    const char* typeName = InformalValueTypeName(args[1]);
    JS_ReportErrorASCII(cx, "expected module, got %s", typeName);
    return false;
  }

  ShellContext* sc = GetShellContext(cx);
  Rooted<ModuleObject*> module(
      cx, args[1].toObject().as<ShellModuleObjectWrapper>().get());

  Rooted<JSAtom*> specifier(cx, AtomizeString(cx, args[0].toString()));
  if (!specifier) {
    return false;
  }

  RootedObject moduleRequest(
      cx, ModuleRequestObject::create(cx, specifier, nullptr));
  if (!moduleRequest) {
    return false;
  }

  if (!sc->moduleLoader->registerTestModule(cx, moduleRequest, module)) {
    return false;
  }

  Rooted<ShellModuleObjectWrapper*> wrapper(
      cx, ShellModuleObjectWrapper::create(cx, module));
  if (!wrapper) {
    return false;
  }
  args.rval().setObject(*wrapper);
  return true;
}

static ModuleEnvironmentObject* GetModuleInitialEnvironment(
    JSContext* cx, Handle<ModuleObject*> module) {
  // Use the initial environment so that tests can check bindings exists
  // before they have been instantiated.
  Rooted<ModuleEnvironmentObject*> env(cx, &module->initialEnvironment());
  MOZ_ASSERT(env);
  return env;
}

static bool GetModuleEnvironmentNames(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 1) {
    JS_ReportErrorASCII(cx, "Wrong number of arguments");
    return false;
  }

  if (!args[0].isObject() ||
      !args[0].toObject().is<ShellModuleObjectWrapper>()) {
    JS_ReportErrorASCII(cx,
                        "First argument should be a ShellModuleObjectWrapper");
    return false;
  }

  Rooted<ModuleObject*> module(
      cx, args[0].toObject().as<ShellModuleObjectWrapper>().get());
  if (module->hadEvaluationError()) {
    JS_ReportErrorASCII(cx, "Module environment unavailable");
    return false;
  }

  Rooted<ModuleEnvironmentObject*> env(cx,
                                       GetModuleInitialEnvironment(cx, module));
  Rooted<IdVector> ids(cx, IdVector(cx));
  if (!JS_Enumerate(cx, env, &ids)) {
    return false;
  }

  // The "*namespace*" binding is a detail of current implementation so hide
  // it to give stable results in tests.
  ids.eraseIfEqual(NameToId(cx->names().starNamespaceStar));

  uint32_t length = ids.length();
  Rooted<ArrayObject*> array(cx, NewDenseFullyAllocatedArray(cx, length));
  if (!array) {
    return false;
  }

  array->setDenseInitializedLength(length);
  for (uint32_t i = 0; i < length; i++) {
    array->initDenseElement(i, StringValue(ids[i].toString()));
  }

  args.rval().setObject(*array);
  return true;
}

static bool GetModuleEnvironmentValue(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 2) {
    JS_ReportErrorASCII(cx, "Wrong number of arguments");
    return false;
  }

  if (!args[0].isObject() ||
      !args[0].toObject().is<ShellModuleObjectWrapper>()) {
    JS_ReportErrorASCII(cx,
                        "First argument should be a ShellModuleObjectWrapper");
    return false;
  }

  if (!args[1].isString()) {
    JS_ReportErrorASCII(cx, "Second argument should be a string");
    return false;
  }

  Rooted<ModuleObject*> module(
      cx, args[0].toObject().as<ShellModuleObjectWrapper>().get());
  if (module->hadEvaluationError()) {
    JS_ReportErrorASCII(cx, "Module environment unavailable");
    return false;
  }

  Rooted<ModuleEnvironmentObject*> env(cx,
                                       GetModuleInitialEnvironment(cx, module));
  RootedString name(cx, args[1].toString());
  RootedId id(cx);
  if (!JS_StringToId(cx, name, &id)) {
    return false;
  }

  if (!GetProperty(cx, env, env, id, args.rval())) {
    return false;
  }

  if (args.rval().isMagic(JS_UNINITIALIZED_LEXICAL)) {
    ReportRuntimeLexicalError(cx, JSMSG_UNINITIALIZED_LEXICAL, id);
    return false;
  }

  return true;
}

enum class DumpType {
  ParseNode,
  Stencil,
};

template <typename Unit>
static bool DumpAST(JSContext* cx, const JS::ReadOnlyCompileOptions& options,
                    const Unit* units, size_t length,
                    js::frontend::CompilationState& compilationState,
                    js::frontend::ParseGoal goal) {
  using namespace js::frontend;

  Parser<FullParseHandler, Unit> parser(cx, options, units, length,
                                        /* foldConstants = */ false,
                                        compilationState,
                                        /* syntaxParser = */ nullptr);
  if (!parser.checkOptions()) {
    return false;
  }

  // Emplace the top-level stencil.
  MOZ_ASSERT(compilationState.scriptData.length() ==
             CompilationStencil::TopLevelIndex);
  if (!compilationState.appendScriptStencilAndData(cx)) {
    return false;
  }

  js::frontend::ParseNode* pn;
  if (goal == frontend::ParseGoal::Script) {
    pn = parser.parse();
  } else {
    if (!GlobalObject::ensureModulePrototypesCreated(cx, cx->global())) {
      return false;
    }

    ModuleBuilder builder(cx, &parser);

    SourceExtent extent = SourceExtent::makeGlobalExtent(length);
    ModuleSharedContext modulesc(cx, options, builder, extent);
    pn = parser.moduleBody(&modulesc);
  }

  if (!pn) {
    return false;
  }

#if defined(DEBUG)
  js::Fprinter out(stderr);
  DumpParseTree(&parser, pn, out);
#endif

  return true;
}

template <typename Unit>
[[nodiscard]] static bool DumpStencil(JSContext* cx,
                                      const JS::ReadOnlyCompileOptions& options,
                                      const Unit* units, size_t length,
                                      js::frontend::ParseGoal goal) {
  Rooted<frontend::CompilationInput> input(cx,
                                           frontend::CompilationInput(options));

  JS::SourceText<Unit> srcBuf;
  if (!srcBuf.init(cx, units, length, JS::SourceOwnership::Borrowed)) {
    return false;
  }

  UniquePtr<frontend::ExtensibleCompilationStencil> stencil;
  if (goal == frontend::ParseGoal::Script) {
    stencil = frontend::CompileGlobalScriptToExtensibleStencil(
        cx, input.get(), srcBuf, ScopeKind::Global);
  } else {
    stencil = frontend::ParseModuleToExtensibleStencil(cx, input.get(), srcBuf);
  }

  if (!stencil) {
    return false;
  }

#if defined(DEBUG) || defined(JS_JITSPEW)
  stencil->dump();
#endif

  return true;
}

static bool FrontendTest(JSContext* cx, unsigned argc, Value* vp,
                         const char* funcName, DumpType dumpType) {
  using namespace js::frontend;

  CallArgs args = CallArgsFromVp(argc, vp);

  if (!args.requireAtLeast(cx, funcName, 1)) {
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

  CompileOptions options(cx);
  options.setIntroductionType("js shell parse")
      .setFileAndLine("<string>", 1)
      .setIsRunOnce(true)
      .setNoScriptRval(true);

  if (args.length() >= 2) {
    if (!args[1].isObject()) {
      JS_ReportErrorASCII(cx, "The 2nd argument must be an object");
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
    if (!js::ParseCompileOptions(cx, options, objOptions, nullptr)) {
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
  Rooted<JSLinearString*> linearString(cx, scriptContents->ensureLinear(cx));
  if (!linearString) {
    return false;
  }

  bool isAscii = false;
  if (linearString->hasLatin1Chars()) {
    JS::AutoCheckCannotGC nogc;
    isAscii = JS::StringIsASCII(mozilla::Span(
        reinterpret_cast<const char*>(linearString->latin1Chars(nogc)),
        linearString->length()));
  }

  AutoStableStringChars stableChars(cx);
  if (isAscii) {
    if (!stableChars.init(cx, scriptContents)) {
      return false;
    }
    MOZ_ASSERT(stableChars.isLatin1());
  } else {
    if (!stableChars.initTwoByte(cx, scriptContents)) {
      return false;
    }
  }

  size_t length = scriptContents->length();
#ifdef JS_ENABLE_SMOOSH
  if (dumpType == DumpType::ParseNode) {
    if (smoosh) {
      if (isAscii) {
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
      JS_ReportErrorASCII(cx,
                          "SmooshMonkey does not support non-ASCII chars yet");
      return false;
    }
  }
#endif  // JS_ENABLE_SMOOSH

  if (goal == frontend::ParseGoal::Module) {
    // See frontend::CompileModule.
    options.setForceStrictMode();
    options.allowHTMLComments = false;
  }

  if (dumpType == DumpType::Stencil) {
#ifdef JS_ENABLE_SMOOSH
    if (smoosh) {
      if (isAscii) {
        if (goal == frontend::ParseGoal::Script) {
          const Latin1Char* latin1 = stableChars.latin1Range().begin().get();
          auto utf8 = reinterpret_cast<const mozilla::Utf8Unit*>(latin1);
          JS::SourceText<Utf8Unit> srcBuf;
          if (!srcBuf.init(cx, utf8, length, JS::SourceOwnership::Borrowed)) {
            return false;
          }

          Rooted<frontend::CompilationInput> input(
              cx, frontend::CompilationInput(options));
          UniquePtr<frontend::ExtensibleCompilationStencil> stencil;
          if (!Smoosh::tryCompileGlobalScriptToExtensibleStencil(
                  cx, input.get(), srcBuf, stencil)) {
            return false;
          }
          if (!stencil) {
            JS_ReportErrorASCII(cx, "SmooshMonkey failed to parse");
            return false;
          }

#  ifdef DEBUG
          {
            frontend::BorrowingCompilationStencil borrowingStencil(*stencil);
            borrowingStencil.dump();
          }
#  endif
        } else {
          JS_ReportErrorASCII(cx,
                              "SmooshMonkey does not support module stencil");
          return false;
        }
        args.rval().setUndefined();
        return true;
      }
      JS_ReportErrorASCII(cx,
                          "SmooshMonkey does not support non-ASCII chars yet");
      return false;
    }
#endif  // JS_ENABLE_SMOOSH

    if (isAscii) {
      const Latin1Char* latin1 = stableChars.latin1Range().begin().get();
      auto utf8 = reinterpret_cast<const mozilla::Utf8Unit*>(latin1);
      if (!DumpStencil<mozilla::Utf8Unit>(cx, options, utf8, length, goal)) {
        return false;
      }
    } else {
      MOZ_ASSERT(stableChars.isTwoByte());
      const char16_t* chars = stableChars.twoByteRange().begin().get();
      if (!DumpStencil<char16_t>(cx, options, chars, length, goal)) {
        return false;
      }
    }

    args.rval().setUndefined();
    return true;
  }

  Rooted<frontend::CompilationInput> input(cx,
                                           frontend::CompilationInput(options));
  if (goal == frontend::ParseGoal::Script) {
    if (!input.get().initForGlobal(cx)) {
      return false;
    }
  } else {
    if (!input.get().initForModule(cx)) {
      return false;
    }
  }

  LifoAllocScope allocScope(&cx->tempLifoAlloc());
  frontend::CompilationState compilationState(cx, allocScope, input.get());
  if (!compilationState.init(cx)) {
    return false;
  }

  if (isAscii) {
    const Latin1Char* latin1 = stableChars.latin1Range().begin().get();
    auto utf8 = reinterpret_cast<const mozilla::Utf8Unit*>(latin1);
    if (!DumpAST<mozilla::Utf8Unit>(cx, options, utf8, length, compilationState,
                                    goal)) {
      return false;
    }
  } else {
    MOZ_ASSERT(stableChars.isTwoByte());
    const char16_t* chars = stableChars.twoByteRange().begin().get();
    if (!DumpAST<char16_t>(cx, options, chars, length, compilationState,
                           goal)) {
      return false;
    }
  }
  args.rval().setUndefined();
  return true;
}

static bool DumpStencil(JSContext* cx, unsigned argc, Value* vp) {
  return FrontendTest(cx, argc, vp, "dumpStencil", DumpType::Stencil);
}

static bool Parse(JSContext* cx, unsigned argc, Value* vp) {
  // Parse returns local scope information with variables ordered
  // differently, depending on the underlying JIT implementation.
  if (js::SupportDifferentialTesting()) {
    JS_ReportErrorASCII(cx,
                        "Function not available in differential testing mode.");
    return false;
  }

  return FrontendTest(cx, argc, vp, "parse", DumpType::ParseNode);
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

  Rooted<frontend::CompilationInput> input(cx,
                                           frontend::CompilationInput(options));
  if (!input.get().initForGlobal(cx)) {
    return false;
  }

  LifoAllocScope allocScope(&cx->tempLifoAlloc());
  frontend::CompilationState compilationState(cx, allocScope, input.get());
  if (!compilationState.init(cx)) {
    return false;
  }

  Parser<frontend::SyntaxParseHandler, char16_t> parser(
      cx, options, chars, length,
      /* foldConstants = */ false, compilationState,
      /* syntaxParser = */ nullptr);
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

static bool OffThreadCompileToStencil(JSContext* cx, unsigned argc, Value* vp) {
  if (!CanUseExtraThreads()) {
    JS_ReportErrorASCII(
        cx, "Can't use offThreadCompileToStencil with --no-threads");
    return false;
  }

  CallArgs args = CallArgsFromVp(argc, vp);

  if (!args.requireAtLeast(cx, "offThreadCompileToStencil", 1)) {
    return false;
  }
  if (!args[0].isString()) {
    const char* typeName = InformalValueTypeName(args[0]);
    JS_ReportErrorASCII(cx, "expected string to parse, got %s", typeName);
    return false;
  }

  UniqueChars fileNameBytes;
  CompileOptions options(cx);
  options.setIntroductionType("js shell offThreadCompileToStencil")
      .setFileAndLine("<string>", 1)
      .setDeferDebugMetadata();

  if (args.length() >= 2) {
    if (!args[1].isObject()) {
      JS_ReportErrorASCII(
          cx, "offThreadCompileToStencil: The 2nd argument must be an object");
      return false;
    }

    // Offthread compilation requires that the debug metadata be set when the
    // script is collected from offthread, rather than when compiled.
    RootedObject opts(cx, &args[1].toObject());
    if (!js::ParseCompileOptions(cx, options, opts, &fileNameBytes)) {
      return false;
    }
  }

  // This option setting must override whatever the caller requested.
  options.setIsRunOnce(true);

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

  OffThreadJob* job =
      NewOffThreadJob(cx, OffThreadJobKind::CompileScript, options,
                      OffThreadJob::Source(std::move(ownedChars)));
  if (!job) {
    return false;
  }

  JS::SourceText<char16_t> srcBuf;
  if (!srcBuf.init(cx, job->sourceChars(), length,
                   JS::SourceOwnership::Borrowed) ||
      !JS::CompileToStencilOffThread(cx, options, srcBuf,
                                     OffThreadCompileScriptCallback, job)) {
    job->cancel();
    DeleteOffThreadJob(cx, job);
    return false;
  }

  args.rval().setInt32(job->id);
  return true;
}

static bool FinishOffThreadCompileToStencil(JSContext* cx, unsigned argc,
                                            Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  OffThreadJob* job =
      LookupOffThreadJobForArgs(cx, OffThreadJobKind::CompileScript, args, 0);
  if (!job) {
    return false;
  }

  JS::OffThreadToken* token = job->waitUntilDone(cx);
  MOZ_ASSERT(token);

  RefPtr<JS::Stencil> stencil = JS::FinishCompileToStencilOffThread(cx, token);
  DeleteOffThreadJob(cx, job);
  if (!stencil) {
    return false;
  }
  RootedObject stencilObj(cx,
                          js::StencilObject::create(cx, std::move(stencil)));
  if (!stencilObj) {
    return false;
  }

  args.rval().setObject(*stencilObj);
  return true;
}

static bool OffThreadCompileModuleToStencil(JSContext* cx, unsigned argc,
                                            Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() != 1 || !args[0].isString()) {
    JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                              JSSMSG_INVALID_ARGS,
                              "offThreadCompileModuleToStencil");
    return false;
  }

  UniqueChars fileNameBytes;
  CompileOptions options(cx);
  options.setIntroductionType("js shell offThreadCompileModuleToStencil")
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

  OffThreadJob* job =
      NewOffThreadJob(cx, OffThreadJobKind::CompileModule, options,
                      OffThreadJob::Source(std::move(ownedChars)));
  if (!job) {
    return false;
  }

  JS::SourceText<char16_t> srcBuf;
  if (!srcBuf.init(cx, job->sourceChars(), length,
                   JS::SourceOwnership::Borrowed) ||
      !JS::CompileModuleToStencilOffThread(
          cx, options, srcBuf, OffThreadCompileScriptCallback, job)) {
    job->cancel();
    DeleteOffThreadJob(cx, job);
    return false;
  }

  args.rval().setInt32(job->id);
  return true;
}

static bool FinishOffThreadCompileModuleToStencil(JSContext* cx, unsigned argc,
                                                  Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  OffThreadJob* job =
      LookupOffThreadJobForArgs(cx, OffThreadJobKind::CompileModule, args, 0);
  if (!job) {
    return false;
  }

  JS::OffThreadToken* token = job->waitUntilDone(cx);
  MOZ_ASSERT(token);

  RefPtr<JS::Stencil> stencil =
      JS::FinishCompileModuleToStencilOffThread(cx, token);
  DeleteOffThreadJob(cx, job);
  if (!stencil) {
    return false;
  }

  RootedObject stencilObj(cx,
                          js::StencilObject::create(cx, std::move(stencil)));
  if (!stencilObj) {
    return false;
  }

  args.rval().setObject(*stencilObj);
  return true;
}

static bool OffThreadDecodeStencil(JSContext* cx, unsigned argc, Value* vp) {
  if (!CanUseExtraThreads()) {
    JS_ReportErrorASCII(cx,
                        "Can't use offThreadDecodeStencil with --no-threads");
    return false;
  }

  CallArgs args = CallArgsFromVp(argc, vp);

  if (!args.requireAtLeast(cx, "offThreadDecodeStencil", 1)) {
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
  options.setIntroductionType("js shell offThreadDecodeStencil")
      .setFileAndLine("<string>", 1);

  if (args.length() >= 2) {
    if (!args[1].isObject()) {
      JS_ReportErrorASCII(
          cx, "offThreadDecodeStencil: The 2nd argument must be an object");
      return false;
    }

    RootedObject opts(cx, &args[1].toObject());
    if (!js::ParseCompileOptions(cx, options, opts, &fileNameBytes)) {
      return false;
    }
  }

  // This option setting must override whatever the caller requested, and
  // this should match `Evaluate` that encodes the script.
  options.setIsRunOnce(false);

  // We assume the caller wants caching if at all possible, ignoring
  // heuristics that make sense for a real browser.
  options.forceAsync = true;

  JS::TranscodeBuffer loadBuffer;
  size_t loadLength = 0;
  uint8_t* loadData = nullptr;
  loadData = CacheEntry_getBytecode(cx, cacheEntry, &loadLength);
  if (!loadData) {
    return false;
  }
  if (!loadBuffer.append(loadData, loadLength)) {
    JS_ReportOutOfMemory(cx);
    return false;
  }

  JS::DecodeOptions decodeOptions(options);
  if (!JS::CanDecodeOffThread(cx, decodeOptions, loadLength)) {
    JS_ReportErrorASCII(cx, "cannot compile code on worker thread");
    return false;
  }

  OffThreadJob* job =
      NewOffThreadJob(cx, OffThreadJobKind::Decode, options,
                      OffThreadJob::Source(std::move(loadBuffer)));
  if (!job) {
    return false;
  }

  if (!JS::DecodeStencilOffThread(cx, decodeOptions, job->xdrBuffer(), 0,
                                  OffThreadCompileScriptCallback, job)) {
    job->cancel();
    DeleteOffThreadJob(cx, job);
    return false;
  }

  args.rval().setInt32(job->id);
  return true;
}

static bool FinishOffThreadDecodeStencil(JSContext* cx, unsigned argc,
                                         Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  OffThreadJob* job =
      LookupOffThreadJobForArgs(cx, OffThreadJobKind::Decode, args, 0);
  if (!job) {
    return false;
  }

  JS::OffThreadToken* token = job->waitUntilDone(cx);
  MOZ_ASSERT(token);

  RefPtr<JS::Stencil> stencil = JS::FinishDecodeStencilOffThread(cx, token);
  DeleteOffThreadJob(cx, job);
  if (!stencil) {
    return false;
  }

  RootedObject stencilObj(cx,
                          js::StencilObject::create(cx, std::move(stencil)));
  if (!stencilObj) {
    return false;
  }

  args.rval().setObject(*stencilObj);
  return true;
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
    (void)arg.release();
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

#ifndef __wasi__
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
#  ifdef XP_WIN
    return !_pipe(fds_, 4096, O_BINARY);
#  else
    return !pipe(fds_);
#  endif
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
#endif  // __wasi__

int shell::sArgc;
char** shell::sArgv;

#ifndef __wasi__
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

#  ifdef XP_WIN
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
#  endif

  // Required by both _spawnv and exec.
  if (!argv.append(nullptr)) {
    return false;
  }

#  ifdef XP_WIN
  if (!EscapeForShell(cx, argv)) {
    return false;
  }

  int childPid = _spawnv(P_NOWAIT, sArgv[0], argv.get());
  if (childPid == -1) {
    return false;
  }
#  else
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
#  endif

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
#  ifdef XP_WIN
  if (_cwait(&status, childPid, WAIT_CHILD) == -1) {
    return false;
  }
#  else
  while (true) {
    if (waitpid(childPid, &status, 0) >= 0) {
      break;
    }
    if (errno != EINTR) {
      return false;
    }
  }
#  endif

  return status == 0;
}

static bool WasmCompileAndSerialize(JSContext* cx) {
  MOZ_ASSERT(wasm::CodeCachingAvailable(cx));

#  ifdef XP_WIN
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
#  else
  int stdIn = STDIN_FILENO;
  int stdOut = STDOUT_FILENO;
#  endif

  wasm::MutableBytes bytecode = js_new<wasm::ShareableBytes>();
  if (!ReadAll(stdIn, &bytecode->bytes)) {
    return false;
  }

  wasm::Bytes serialized;
  if (!wasm::CompileAndSerialize(cx, *bytecode, &serialized)) {
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
#endif  // __wasi__

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
  bool immutablePrototype = true;

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

    if (!JS_GetProperty(cx, opts, "discardSource", &v)) {
      return false;
    }
    if (v.isBoolean()) {
      behaviors.setDiscardSource(v.toBoolean());
    }

    if (!JS_GetProperty(cx, opts, "useWindowProxy", &v)) {
      return false;
    }
    if (v.isBoolean()) {
      kind = v.toBoolean() ? ShellGlobalKind::WindowProxy
                           : ShellGlobalKind::GlobalObject;
    }

    if (!JS_GetProperty(cx, opts, "immutablePrototype", &v)) {
      return false;
    }
    if (v.isBoolean()) {
      immutablePrototype = v.toBoolean();
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

    if (!JS_GetProperty(cx, opts, "freezeBuiltins", &v)) {
      return false;
    }
    if (v.isBoolean()) {
      creationOptions.setFreezeBuiltins(v.toBoolean());
    }

    // On the web, the SharedArrayBuffer constructor is not installed as a
    // global property in pages that aren't isolated in a separate process (and
    // thus can't allow the structured cloning of shared memory).  Specify false
    // for this option to reproduce this behavior.
    if (!JS_GetProperty(cx, opts, "defineSharedArrayBufferConstructor", &v)) {
      return false;
    }
    if (v.isBoolean()) {
      creationOptions.setDefineSharedArrayBufferConstructor(v.toBoolean());
    }
  }

  if (!CheckRealmOptions(cx, options, principals.get())) {
    return false;
  }

  RootedObject global(cx, NewGlobalObject(cx, options, principals.get(), kind,
                                          immutablePrototype));
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
    sourceComp = JS::GetCompartment(UncheckedUnwrap(&args[0].toObject()));
  }

  JS::Compartment* targetComp = nullptr;
  if (args.get(1).isObject()) {
    targetComp = JS::GetCompartment(UncheckedUnwrap(&args[1].toObject()));
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
  if (args.length() == 0 ||
      (args[0].isString() && args[0].toString()->length() == 0)) {
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
  Rooted<JSAtom*> srcAtom(cx, ToAtom<CanGC>(cx, args[0]));
  if (!srcAtom) {
    return false;
  }
  Rooted<PropertyName*> srcName(cx, srcAtom->asPropertyName());
  return GlobalObject::getIntrinsicValue(cx, cx->global(), srcName,
                                         args.rval());
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
          linear, mozilla::Span(*utf8Source, *length));
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

static void PrintProfilerEvents_Callback(const char* msg, const char* details) {
  fprintf(stderr, "PROFILER EVENT: %s %s\n", msg, details);
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
#  elif defined(JS_SIMULATOR_LOONG64)
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
      size_t length;
      bool isHugeMemory;  // For a WasmMemory tag, otherwise false
    } sarb;
    JS::WasmModule* module;
    double number;

    Value() : number(0.0) {}
  };

  MailboxTag tag = MailboxTag::Empty;
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
        size_t length = mbx->val.sarb.length;
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
          RootedObject proto(cx,
                             &cx->global()->getPrototype(JSProto_WasmMemory));
          newObj = WasmMemoryObject::create(cx, maybesab,
                                            mbx->val.sarb.isHugeMemory, proto);
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
      value.sarb.isHugeMemory = false;
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
        value.sarb.isHugeMemory = obj->as<WasmMemoryObject>().isHuge();
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

  void storeOptimizedEncoding(const uint8_t* srcBytes,
                              size_t srcLength) override {
    MOZ_ASSERT(srcLength > 0);

    // Tolerate races since a single StreamCacheEntry object can be used as
    // the source of multiple streaming compilations.
    auto dstBytes = optimized_.lock();
    if (dstBytes->length() > 0) {
      return;
    }

    if (!dstBytes->resize(srcLength)) {
      return;
    }
    memcpy(dstBytes->begin(), srcBytes, srcLength);
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

  static void finalize(JS::GCContext* gcx, JSObject* obj) {
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

    Rooted<NativeObject*> obj(
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

    ThisThread::SleepMilliseconds(delayMillis);

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

  if (js::SupportDifferentialTesting()) {
    ReportUsageErrorASCII(
        cx, callee, "Function not available in differential testing mode.");
    return false;
  }

  if (args.length() != 1) {
    ReportUsageErrorASCII(cx, callee, "Wrong number of arguments");
    return false;
  }

  if (!args[0].isObject() ||
      !(args[0].toObject().is<JSFunction>() ||
        args[0].toObject().is<ShellModuleObjectWrapper>())) {
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
    script = obj->as<ShellModuleObjectWrapper>().get()->maybeScript();
    if (!script) {
      JS_ReportErrorASCII(cx, "module does not have an associated script");
      return false;
    }
  }

  script->bodyScope()->dump();

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
    if (!(priv->grayRoot = NewTenuredDenseEmptyArray(cx))) {
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

  RootedObject observersArg(cx, &args[0].toObject());
  uint64_t length;
  if (!GetLengthProperty(cx, observersArg, &length)) {
    return false;
  }

  if (length > UINT32_MAX) {
    JS_ReportErrorASCII(cx, "Invalid length for observers array");
    return false;
  }

  RootedValue value(cx);
  RootedObject object(cx);
  for (uint32_t i = 0; i < length; i++) {
    if (!JS_GetElement(cx, observersArg, i, &value)) {
      return false;
    }

    if (!value.isObject()) {
      JS_ReportErrorASCII(cx, "argument must be an Array of objects");
      return false;
    }

    object = &value.toObject();
    if (gc::IsInsideNursery(object)) {
      // WeakCaches are not swept during a minor GC. To prevent
      // nursery-allocated contents from having the mark bits be deceptively
      // black until the second GC, they would need to be marked weakly (cf
      // NurseryAwareHashMap). It is simpler to evict the nursery to prevent
      // nursery objects from being observed.
      cx->runtime()->gc.evictNursery();
    }

    if (!markObservers->get().append(object)) {
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

#ifndef __wasi__
static bool WasmTextToBinary(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject callee(cx, &args.callee());

  if (!args.requireAtLeast(cx, "wasmTextToBinary", 1)) {
    return false;
  }

  if (!args[0].isString()) {
    ReportUsageErrorASCII(cx, callee, "First argument must be a String");
    return false;
  }

  size_t textLen = args[0].toString()->length();

  AutoStableStringChars twoByteChars(cx);
  if (!twoByteChars.initTwoByte(cx, args[0].toString())) {
    return false;
  }

  wasm::Bytes bytes;
  UniqueChars error;
  if (!wasm::TextToBinary(twoByteChars.twoByteChars(), textLen, &bytes,
                          &error)) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_WASM_TEXT_FAIL,
                             error.get() ? error.get() : "out of memory");
    return false;
  }

  RootedObject binary(cx, JS_NewUint8Array(cx, bytes.length()));
  if (!binary) {
    return false;
  }

  memcpy(binary->as<TypedArrayObject>().dataPointerUnshared(), bytes.begin(),
         bytes.length());

  args.rval().setObject(*binary);
  return true;
}

static bool WasmCodeOffsets(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject callee(cx, &args.callee());

  if (!args.requireAtLeast(cx, "wasmCodeOffsets", 1)) {
    return false;
  }

  if (!args.get(0).isObject()) {
    JS_ReportErrorASCII(cx, "argument is not an object");
    return false;
  }

  SharedMem<uint8_t*> bytes;
  size_t byteLength;

  JSObject* bufferObject = &args[0].toObject();
  JSObject* unwrappedBufferObject = CheckedUnwrapStatic(bufferObject);
  if (!unwrappedBufferObject ||
      !IsBufferSource(unwrappedBufferObject, &bytes, &byteLength)) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_BUF_ARG);
    return false;
  }

  wasm::Uint32Vector offsets;
  wasm::CodeOffsets(bytes.unwrap(), byteLength, &offsets);

  RootedObject jsOffsets(cx, JS::NewArrayObject(cx, offsets.length()));
  if (!jsOffsets) {
    return false;
  }
  for (size_t i = 0; i < offsets.length(); i++) {
    uint32_t offset = offsets[i];
    RootedValue offsetVal(cx, NumberValue(offset));
    if (!JS_SetElement(cx, jsOffsets, i, offsetVal)) {
      return false;
    }
  }
  args.rval().setObject(*jsOffsets);
  return true;
}

#  ifndef __AFL_HAVE_MANUAL_CONTROL
#    define __AFL_LOOP(x) true
#  endif

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
    Rooted<WasmInstanceObject*> instanceObj(cx);
    // No additional compile options here, we don't need them for this use case.
    RootedValue maybeOptions(cx);
    if (!wasm::Eval(cx, typedArray, importObj, maybeOptions, &instanceObj)) {
      // Clear any pending exceptions, we don't care about them
      cx->clearPendingException();
    }
  }

#  ifdef __AFL_HAVE_MANUAL_CONTROL  // to silence unreachable code warning
  return true;
#  endif
}
#endif  // __wasi__

static constexpr uint32_t DOM_OBJECT_SLOT = 0;
static constexpr uint32_t DOM_OBJECT_SLOT2 = 1;

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
  // 1. Check the recursion depth using checkConservative.
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

  AutoCheckRecursionLimit recursion(cx);
  if (!recursion.checkConservative(cx)) {
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
  if (JS::GetClass(source) == GetDomClass()) {
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

  if (JS::GetCompartment(source) != JS::GetCompartment(target) &&
      !AllowNewWrapper(JS::GetCompartment(source), target)) {
    JS_ReportErrorASCII(cx, "Cannot transplant into nuked compartment");
    return false;
  }

  RootedObject copyFrom(cx, isProxy ? expandoObject : source);
  RootedObject propertyHolder(cx,
                              JS_NewObjectWithGivenProto(cx, nullptr, nullptr));
  if (!propertyHolder) {
    return false;
  }

  if (!JS_CopyOwnPropertiesAndPrivateFields(cx, propertyHolder, copyFrom)) {
    return false;
  }

  JS::SetReservedSlot(target, DOM_OBJECT_SLOT,
                      JS::GetReservedSlot(source, DOM_OBJECT_SLOT));
  JS::SetReservedSlot(source, DOM_OBJECT_SLOT, JS::PrivateValue(nullptr));
  if (JS::GetClass(source) == GetDomClass()) {
    JS::SetReservedSlot(target, DOM_OBJECT_SLOT2,
                        JS::GetReservedSlot(source, DOM_OBJECT_SLOT2));
    JS::SetReservedSlot(source, DOM_OBJECT_SLOT2, UndefinedValue());
  }

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
  if (!JS_CopyOwnPropertiesAndPrivateFields(cx, copyTo, propertyHolder)) {
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
      if (JS::GetClass(source) != GetDomClass()) {
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

      JS::SetReservedSlot(source, DOM_OBJECT_SLOT, JS::PrivateValue(nullptr));
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

#ifdef DEBUG
static bool DebugGetQueuedJobs(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  JSObject* jobs = js::GetJobsInInternalJobQueue(cx);
  if (!jobs) {
    return false;
  }

  args.rval().setObject(*jobs);
  return true;
}
#endif

#ifdef FUZZING_INTERFACES
extern "C" {
size_t gluesmith(uint8_t* data, size_t size, uint8_t* out, size_t maxsize);
}

static bool GetWasmSmithModule(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject callee(cx, &args.callee());

  if (args.length() != 1) {
    ReportUsageErrorASCII(cx, callee, "Wrong number of arguments");
    return false;
  }

  if (!args[0].isObject() || !args[0].toObject().is<ArrayBufferObject>()) {
    ReportUsageErrorASCII(cx, callee, "Argument must be ArrayBuffer.");
    return false;
  }

  ArrayBufferObject* arrayBuffer = &args[0].toObject().as<ArrayBufferObject>();
  size_t length = arrayBuffer->byteLength();
  uint8_t* data = arrayBuffer->dataPointer();

  const size_t maxModuleSize = 4096;
  uint8_t tmp[maxModuleSize];

  size_t outSize = gluesmith(data, length, tmp, maxModuleSize);
  if (!outSize) {
    JS_ReportErrorASCII(cx, "Generated module is too large.");
    return false;
  }

  JS::Rooted<JSObject*> outArr(cx, JS_NewUint8ClampedArray(cx, outSize));
  if (!outArr) {
    return false;
  }

  {
    JS::AutoCheckCannotGC nogc;
    bool isShared;
    uint8_t* data = JS_GetUint8ClampedArrayData(outArr, &isShared, nogc);
    MOZ_RELEASE_ASSERT(!isShared);
    memcpy(data, tmp, outSize);
  }

  args.rval().setObject(*outArr);
  return true;
}

#endif

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
"      forceFullParse: if present and true, disable syntax-parse.\n"
"      loadBytecode: if true, and if the source is a CacheEntryObject,\n"
"         the bytecode would be loaded and decoded from the cache entry instead\n"
"         of being parsed, then it would be executed as usual.\n"
"      saveIncrementalBytecode: if true, and if the source is a\n"
"         CacheEntryObject, the bytecode would be incrementally encoded and\n"
"         saved into the cache entry.\n"
"      transcodeOnly: if true, do not execute the script.\n"
"      assertEqBytecode: if true, and if both loadBytecode and either\n"
"         saveIncrementalBytecode is true, then the loaded\n"
"         bytecode and the encoded bytecode are compared.\n"
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
"  Create an JS::ErrorReportBuilder object from the given value and serialize\n"
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

#ifdef DEBUG
    JS_FN_HELP("arrayInfo", ArrayInfo, 1, 0,
"arrayInfo(a1, a2, ...)",
"  Report statistics about arrays."),
#endif

    JS_FN_HELP("sleep", Sleep_fn, 1, 0,
"sleep(dt)",
"  Sleep for dt seconds."),

    JS_FN_HELP("compile", Compile, 1, 0,
"compile(code, [options])",
"  Compiles a string to bytecode, potentially throwing.\n"
"  If present, |options| may have CompileOptions-related properties of\n"
"  evaluate function"),

    JS_FN_HELP("parseModule", ParseModule, 1, 0,
"parseModule(code)",
"  Parses source text as a module and returns a ModuleObject wrapper object."),

    JS_FN_HELP("instantiateModuleStencil", InstantiateModuleStencil, 1, 0,
"instantiateModuleStencil(stencil, [options])",
"  Instantiates the given stencil as module, and return the module object."),

    JS_FN_HELP("instantiateModuleStencilXDR", InstantiateModuleStencilXDR, 1, 0,
"instantiateModuleStencilXDR(stencil, [options])",
"  Reads the given stencil XDR object, instantiates the stencil as module, and"
"  return the module object."),

    JS_FN_HELP("registerModule", RegisterModule, 2, 0,
"registerModule(specifier, module)",
"  Register a module with the module loader, so that subsequent import from\n"
"  |specifier| will resolve to |module|.  Returns |module|."),

    JS_FN_HELP("getModuleEnvironmentNames", GetModuleEnvironmentNames, 1, 0,
"getModuleEnvironmentNames(module)",
"  Get the list of a module environment's bound names for a specified module.\n"),

    JS_FN_HELP("getModuleEnvironmentValue", GetModuleEnvironmentValue, 2, 0,
"getModuleEnvironmentValue(module, name)",
"  Get the value of a bound name in a module environment.\n"),

    JS_FN_HELP("dumpStencil", DumpStencil, 1, 0,
"dumpStencil(code, [options])",
"  Parses a string and returns string that represents stencil.\n"
"  If present, |options| may have properties saying how the code should be\n"
"  compiled:\n"
"      module: if present and true, compile the source as module.\n"
"      smoosh: if present and true, use SmooshMonkey.\n"
"  CompileOptions-related properties of evaluate function's option can also\n"
"  be used."),

    JS_FN_HELP("parse", Parse, 1, 0,
"parse(code, [options])",
"  Parses a string, potentially throwing. If present, |options| may\n"
"  have properties saying how the code should be compiled:\n"
"      module: if present and true, compile the source as module.\n"
"      smoosh: if present and true, use SmooshMonkey.\n"
"  CompileOptions-related properties of evaluate function's option can also\n"
"  be used. except forceFullParse. This function always use full parse."),

    JS_FN_HELP("syntaxParse", SyntaxParse, 1, 0,
"syntaxParse(code)",
"  Check the syntax of a string, returning success value"),

    JS_FN_HELP("offThreadCompileModuleToStencil", OffThreadCompileModuleToStencil, 1, 0,
"offThreadCompileModuleToStencil(code)",
"  Compile |code| on a helper thread, returning a job ID. To wait for the\n"
"  compilation to finish and and get the module stencil object call\n"
"  |finishOffThreadCompileModuleToStencil| passing the job ID."),

    JS_FN_HELP("finishOffThreadCompileModuleToStencil", FinishOffThreadCompileModuleToStencil, 0, 0,
"finishOffThreadCompileModuleToStencil([jobID])",
"  Wait for an off-thread compilation job to complete. The job ID can be\n"
"  ommitted if there is only one job pending. If an error occurred,\n"
"  throw the appropriate exception; otherwise, return the module stencil\n"
"  object."),

    JS_FN_HELP("offThreadDecodeStencil", OffThreadDecodeStencil, 1, 0,
"offThreadDecodeStencil(cacheEntry[, options])",
"  Decode |code| on a helper thread, returning a job ID. To wait for the\n"
"  decoding to finish and run the code, call |finishOffThreadDecodeStencil| passing\n"
"  the job ID. If present, |options| may have properties saying how the code\n"
"  should be compiled (see also offThreadCompileToStencil)."),

    JS_FN_HELP("finishOffThreadDecodeStencil", FinishOffThreadDecodeStencil, 0, 0,
"finishOffThreadDecodeStencil([jobID])",
"  Wait for an off-thread decode job to complete. The job ID can be\n"
"  ommitted if there is only one job pending. If an error occurred,\n"
"  throw the appropriate exception; otherwise, return the decoded stencil\n"
"  object."),

    JS_FN_HELP("offThreadCompileToStencil", OffThreadCompileToStencil, 1, 0,
"offThreadCompileToStencil(code[, options])",
"  Compile |code| on a helper thread, returning a job ID. To wait for the\n"
"  compilation to finish and get the stencil object, call\n"
"  |finishOffThreadCompileToStencil| passing the job ID.  If present, \n"
"  |options| may have properties saying how the code should be compiled:\n"
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

    JS_FN_HELP("finishOffThreadCompileToStencil", FinishOffThreadCompileToStencil, 0, 0,
"finishOffThreadCompileToStencil([jobID])",
"  Wait for an off-thread compilation job to complete. The job ID can be\n"
"  ommitted if there is only one job pending. If an error occurred,\n"
"  throw the appropriate exception; otherwise, return the stencil object,"
"  that can be passed to |evalStencil|."),

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
#ifdef DEBUG
    JS_FN_HELP("interruptRegexp", InterruptRegexp, 2, 0,
"interruptRegexp(<regexp>, <string>)",
"  Interrrupt the execution of regular expression.\n"),
#endif
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
"      invisibleToDebugger: If true, the global will be invisible to the\n"
"         debugger (default false)\n"
"      discardSource: If true, discard source after compiling a script\n"
"         (default false).\n"
"      useWindowProxy: the global will be created with a WindowProxy attached. In this\n"
"          case, the WindowProxy will be returned.\n"
"      freezeBuiltins: certain builtin constructors will be frozen when created and\n"
"          their prototypes will be sealed. These constructors will be defined on the\n"
"          global as non-configurable and non-writable.\n"
"      immutablePrototype: whether the global's prototype is immutable.\n"
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

    JS_FN_HELP("createExternalArrayBuffer", CreateExternalArrayBuffer, 1, 0,
"createExternalArrayBuffer(size)",
"  Create an array buffer that has external data of size."),

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
"  by setting \"saveIncrementalBytecode\" and \"loadBytecode\" options."),

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

#ifndef __wasi__
    JS_FN_HELP("wasmCompileInSeparateProcess", WasmCompileInSeparateProcess, 1, 0,
"wasmCompileInSeparateProcess(buffer)",
"  Compile the given buffer in a separate process, serialize the resulting\n"
"  wasm::Module into bytes, and deserialize those bytes in the current\n"
"  process, returning the resulting WebAssembly.Module."),

    JS_FN_HELP("wasmTextToBinary", WasmTextToBinary, 1, 0,
"wasmTextToBinary(str)",
"  Translates the given text wasm module into its binary encoding."),

    JS_FN_HELP("wasmCodeOffsets", WasmCodeOffsets, 1, 0,
"wasmCodeOffsets(binary)",
"  Decodes the given wasm binary to find the offsets of every instruction in the"
"  code section."),
#endif // __wasi__

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

#ifdef FUZZING_JS_FUZZILLI
    JS_FN_HELP("fuzzilli", Fuzzilli, 0, 0,
"fuzzilli(operation, arg)",
"  Exposes functionality used by the Fuzzilli JavaScript fuzzer."),
#endif

#ifdef FUZZING_INTERFACES
    JS_FN_HELP("getWasmSmithModule", GetWasmSmithModule, 1, 0,
"getWasmSmithModule(arrayBuffer)",
"  Call wasm-smith to generate a random wasm module from the provided data."),
#endif

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

#ifndef __wasi__
    JS_FN_HELP("wasmLoop", WasmLoop, 2, 0,
"wasmLoop(filename, imports)",
"  Performs an AFL-style persistent loop reading data from the given file and passing it\n"
"  to the 'wasmEval' function together with the specified imports object."),
#endif // __wasi__

    JS_FN_HELP("setBufferStreamParams", SetBufferStreamParams, 2, 0,
"setBufferStreamParams(delayMillis, chunkByteSize)",
"  Set the delay time (between calls to StreamConsumer::consumeChunk) and chunk\n"
"  size (in bytes)."),

#ifdef JS_CACHEIR_SPEW
  JS_FN_HELP("cacheIRHealthReport", CacheIRHealthReport, 0, 0,
"cacheIRHealthReport()",
"  Show health rating of CacheIR stubs."),
#endif

#ifdef DEBUG
  JS_FN_HELP("debugGetQueuedJobs", DebugGetQueuedJobs, 0, 0,
"debugGetQueuedJobs()",
"  Returns an array of queued jobs."),
#endif

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
      Rooted<JSLinearString*> input(cx, &v.toString()->asLinear());
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

  auto printError = [](JSContext* cx, auto& report, const auto& exnStack,
                       const char* prefix = nullptr) {
    if (!report.init(cx, exnStack, JS::ErrorReportBuilder::WithSideEffects)) {
      fprintf(stderr, "out of memory initializing JS::ErrorReportBuilder\n");
      fflush(stderr);
      JS_ClearPendingException(cx);
      return false;
    }

    MOZ_ASSERT(!report.report()->isWarning());

    FILE* fp = ErrorFilePointer();
    if (prefix) {
      fputs(prefix, fp);
    }
    JS::PrintError(fp, report, reportWarnings);
    JS_ClearPendingException(cx);

    // If possible, use the original error stack as the source of truth, because
    // finally block handlers may have overwritten the exception stack.
    RootedObject stack(cx, exnStack.stack());
    if (exnStack.exception().isObject()) {
      RootedObject exception(cx, &exnStack.exception().toObject());
      if (JSObject* exceptionStack = JS::ExceptionStackOrNull(exception)) {
        stack.set(exceptionStack);
      }
    }

    if (!PrintStackTrace(cx, stack)) {
      fputs("(Unable to print stack trace)\n", fp);
      JS_ClearPendingException(cx);
    }

    return true;
  };

  // Get exception object and stack before printing and clearing exception.
  JS::ExceptionStack exnStack(cx);
  if (!JS::StealPendingExceptionStack(cx, &exnStack)) {
    fprintf(stderr, "out of memory while stealing exception\n");
    fflush(stderr);
    JS_ClearPendingException(cx);
    return;
  }

  ShellContext* sc = GetShellContext(cx);
  JS::ErrorReportBuilder report(cx);
  if (!printError(cx, report, exnStack)) {
    // Return if we couldn't initialize the error report.
    return;
  }

  // Print the error's cause, if available.
  if (exnStack.exception().isObject()) {
    JSObject* exception = &exnStack.exception().toObject();
    if (exception->is<ErrorObject>()) {
      auto* error = &exception->as<ErrorObject>();
      if (auto maybeCause = error->getCause()) {
        RootedValue cause(cx, maybeCause.value());

        RootedObject causeStack(cx);
        if (cause.isObject()) {
          RootedObject causeObj(cx, &cause.toObject());
          causeStack = JS::ExceptionStackOrNull(causeObj);
        }

        JS::ExceptionStack exnStack(cx, cause, causeStack);
        JS::ErrorReportBuilder report(cx);
        printError(cx, report, exnStack, "Caused by: ");
      }
    }
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
  JS::PrintError(fp, report, reportWarnings);
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
  MOZ_ASSERT(JS::GetClass(obj) == GetDomClass());
  MOZ_ASSERT(self == DOM_PRIVATE_VALUE);
  args.rval().set(JS_NumberValue(double(3.14)));
  return true;
}

static bool dom_set_x(JSContext* cx, HandleObject obj, void* self,
                      JSJitSetterCallArgs args) {
  MOZ_ASSERT(JS::GetClass(obj) == GetDomClass());
  MOZ_ASSERT(self == DOM_PRIVATE_VALUE);
  return true;
}

static bool dom_get_slot(JSContext* cx, HandleObject obj, void* self,
                         JSJitGetterCallArgs args) {
  MOZ_ASSERT(JS::GetClass(obj) == GetDomClass());
  MOZ_ASSERT(self == DOM_PRIVATE_VALUE);

  Value v = JS::GetReservedSlot(obj, DOM_OBJECT_SLOT2);
  MOZ_ASSERT(v.toInt32() == 42);
  args.rval().set(v);
  return true;
}

static bool dom_get_global(JSContext* cx, HandleObject obj, void* self,
                           JSJitGetterCallArgs args) {
  MOZ_ASSERT(JS::GetClass(obj) == GetDomClass());
  MOZ_ASSERT(self == DOM_PRIVATE_VALUE);

  // Return the current global (instead of obj->global()) to test cx->realm
  // switching in the JIT.
  args.rval().setObject(*ToWindowProxyIfWindow(cx->global()));

  return true;
}

static bool dom_set_global(JSContext* cx, HandleObject obj, void* self,
                           JSJitSetterCallArgs args) {
  MOZ_ASSERT(JS::GetClass(obj) == GetDomClass());
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
  MOZ_ASSERT(JS::GetClass(obj) == GetDomClass());
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

static const JSJitInfo dom_slot_getterinfo = {
    {(JSJitGetterOp)dom_get_slot},
    {0}, /* protoID */
    {0}, /* depth */
    JSJitInfo::Getter,
    JSJitInfo::AliasNone, /* aliasSet */
    JSVAL_TYPE_INT32,     /* returnType */
    false,                /* isInfallible. False in setters. */
    true,                 /* isMovable */
    true,                 /* isEliminatable */
    true,                 /* isAlwaysInSlot */
    false,                /* isLazilyCachedInSlot */
    false,                /* isTypedMethod */
    DOM_OBJECT_SLOT2      /* slotIndex */
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
    JSPropertySpec::nativeAccessors("slot", JSPROP_ENUMERATE, dom_genericGetter,
                                    &dom_slot_getterinfo),
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
  if (JS::GetClass(obj) != &dom_class) {
    args.rval().set(UndefinedValue());
    return true;
  }

  JS::Value val = JS::GetReservedSlot(obj, DOM_OBJECT_SLOT);

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
  if (JS::GetClass(obj) != &dom_class) {
    args.rval().set(UndefinedValue());
    return true;
  }

  JS::Value val = JS::GetReservedSlot(obj, DOM_OBJECT_SLOT);

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
  if (JS::GetClass(obj) != &dom_class) {
    args.rval().set(UndefinedValue());
    return true;
  }

  JS::Value val = JS::GetReservedSlot(obj, DOM_OBJECT_SLOT);

  const JSJitInfo* info = FUNCTION_VALUE_TO_JITINFO(args.calleev());
  MOZ_ASSERT(info->type() == JSJitInfo::Method);
  JSJitMethodOp method = info->method;
  return method(cx, obj, val.toPrivate(), JSJitMethodCallArgs(args));
}

static void InitDOMObject(HandleObject obj) {
  JS::SetReservedSlot(obj, DOM_OBJECT_SLOT,
                      PrivateValue(const_cast<void*>(DOM_PRIVATE_VALUE)));
  JS::SetReservedSlot(obj, DOM_OBJECT_SLOT2, Int32Value(42));
}

static JSObject* GetDOMPrototype(JSContext* cx, JSObject* global) {
  MOZ_ASSERT(JS_IsGlobalObject(global));
  if (JS::GetClass(global) != &global_class) {
    JS_ReportErrorASCII(cx, "Can't get FakeDOMObject prototype in sandbox");
    return nullptr;
  }

  const JS::Value& slot = JS::GetReservedSlot(global, DOM_PROTOTYPE_SLOT);
  MOZ_ASSERT(slot.isObject());
  return &slot.toObject();
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
                                 JSPrincipals* principals, ShellGlobalKind kind,
                                 bool immutablePrototype) {
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

    if (immutablePrototype) {
      bool succeeded;
      if (!JS_SetImmutablePrototype(cx, glob, &succeeded)) {
        return nullptr;
      }
      MOZ_ASSERT(succeeded,
                 "a fresh, unexposed global object is always capable of "
                 "having its [[Prototype]] be immutable");
    }

#ifdef JS_HAS_CTYPES
    if (!fuzzingSafe && !JS::InitCTypesClass(cx, glob)) {
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
    JS::SetReservedSlot(glob, DOM_PROTOTYPE_SLOT, ObjectValue(*domProto));

    /* Initialize FakeDOMObject.prototype */
    InitDOMObject(domProto);

    if (!DefineToStringTag(cx, glob, cx->names().global)) {
      return nullptr;
    }

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

[[nodiscard]] static bool ProcessArgs(JSContext* cx, OptionParser* op) {
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

  if (const char* mode = op->getStringOption("delazification-mode")) {
    if (strcmp(mode, "on-demand") == 0) {
      defaultDelazificationMode = JS::DelazificationOption::OnDemandOnly;
    } else if (strcmp(mode, "concurrent-df") == 0) {
      defaultDelazificationMode =
          JS::DelazificationOption::ConcurrentDepthFirst;
    } else if (strcmp(mode, "eager") == 0) {
      defaultDelazificationMode =
          JS::DelazificationOption::ParseEverythingEagerly;
    } else if (strcmp(mode, "concurrent-df+on-demand") == 0 ||
               strcmp(mode, "on-demand+concurrent-df") == 0) {
      defaultDelazificationMode =
          JS::DelazificationOption::CheckConcurrentWithOnDemand;
    } else {
      return OptionFailure("delazification-mode", mode);
    }
  }

  /* |scriptArgs| gets bound on the global before any code is run. */
  if (!BindScriptArgs(cx, op)) {
    return false;
  }

  RootedString moduleLoadPath(cx);
  if (const char* option = op->getStringOption("module-load-path")) {
    RootedString jspath(cx, JS_NewStringCopyZ(cx, option));
    if (!jspath) {
      return false;
    }

    moduleLoadPath = js::shell::ResolvePath(cx, jspath, RootRelative);
  } else {
    UniqueChars cwd = js::shell::GetCWD();
    moduleLoadPath = JS_NewStringCopyZ(cx, cwd.get());
  }

  if (!moduleLoadPath) {
    return false;
  }

  processWideModuleLoadPath = JS_EncodeStringToUTF8(cx, moduleLoadPath);
  if (!processWideModuleLoadPath) {
    MOZ_ASSERT(cx->isExceptionPending());
    return false;
  }

  sc->moduleLoader = js::MakeUnique<ModuleLoader>();
  if (!sc->moduleLoader || !sc->moduleLoader->init(cx, moduleLoadPath)) {
    return false;
  }

  MultiStringRange filePaths = op->getMultiStringOption('f');
  MultiStringRange utf16FilePaths = op->getMultiStringOption('u');
  MultiStringRange codeChunks = op->getMultiStringOption('e');
  MultiStringRange modulePaths = op->getMultiStringOption('m');

#ifdef FUZZING_JS_FUZZILLI
  // Check for REPRL file source
  if (op->getBoolOption("reprl")) {
    return FuzzilliReprlGetAndRun(cx);
  }
#endif /* FUZZING_JS_FUZZILLI */

  if (filePaths.empty() && utf16FilePaths.empty() && codeChunks.empty() &&
      modulePaths.empty() && !op->getStringArg("script")) {
    // Always use the interactive shell when -i is used. Without -i we let
    // Process figure it out based on isatty.
    bool forceTTY = op->getBoolOption('i');
    return Process(cx, nullptr, forceTTY, FileScript);
  }

  while (!filePaths.empty() || !utf16FilePaths.empty() || !codeChunks.empty() ||
         !modulePaths.empty()) {
    size_t fpArgno = filePaths.empty() ? SIZE_MAX : filePaths.argno();
    size_t ufpArgno =
        utf16FilePaths.empty() ? SIZE_MAX : utf16FilePaths.argno();
    size_t ccArgno = codeChunks.empty() ? SIZE_MAX : codeChunks.argno();
    size_t mpArgno = modulePaths.empty() ? SIZE_MAX : modulePaths.argno();

    if (fpArgno < ufpArgno && fpArgno < ccArgno && fpArgno < mpArgno) {
      char* path = filePaths.front();
      if (!Process(cx, path, false, FileScript)) {
        return false;
      }

      filePaths.popFront();
      continue;
    }

    if (ufpArgno < fpArgno && ufpArgno < ccArgno && ufpArgno < mpArgno) {
      char* path = utf16FilePaths.front();
      if (!Process(cx, path, false, FileScriptUtf16)) {
        return false;
      }

      utf16FilePaths.popFront();
      continue;
    }

    if (ccArgno < fpArgno && ccArgno < ufpArgno && ccArgno < mpArgno) {
      const char* code = codeChunks.front();

      JS::CompileOptions opts(cx);
      opts.setFileAndLine("-e", 1).setEagerDelazificationStrategy(
          defaultDelazificationMode);

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

    MOZ_ASSERT(mpArgno < fpArgno && mpArgno < ufpArgno && mpArgno < ccArgno);

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

  enableWasm = true;
  enableWasmBaseline = true;
  enableWasmOptimizing = true;

  if (const char* str = op.getStringOption("wasm-compiler")) {
    if (strcmp(str, "none") == 0) {
      enableWasm = false;
    } else if (strcmp(str, "baseline") == 0) {
      MOZ_ASSERT(enableWasmBaseline);
      enableWasmOptimizing = false;
    } else if (strcmp(str, "optimizing") == 0 ||
               strcmp(str, "optimized") == 0) {
      enableWasmBaseline = false;
      MOZ_ASSERT(enableWasmOptimizing);
    } else if (strcmp(str, "baseline+optimizing") == 0 ||
               strcmp(str, "baseline+optimized") == 0) {
      MOZ_ASSERT(enableWasmBaseline);
      MOZ_ASSERT(enableWasmOptimizing);
#ifdef ENABLE_WASM_CRANELIFT
    } else if (strcmp(str, "cranelift") == 0) {
      enableWasmBaseline = false;
      enableWasmOptimizing = true;
    } else if (strcmp(str, "baseline+cranelift") == 0) {
      MOZ_ASSERT(enableWasmBaseline);
      enableWasmOptimizing = true;
#else
    } else if (strcmp(str, "ion") == 0) {
      enableWasmBaseline = false;
      enableWasmOptimizing = true;
    } else if (strcmp(str, "baseline+ion") == 0) {
      MOZ_ASSERT(enableWasmBaseline);
      enableWasmOptimizing = true;
#endif
    } else {
      return OptionFailure("wasm-compiler", str);
    }
  }

#define WASM_DEFAULT_FEATURE(NAME, LOWER_NAME, COMPILE_PRED, COMPILER_PRED, \
                             FLAG_PRED, SHELL, ...)                         \
  enableWasm##NAME = !op.getBoolOption("no-wasm-" SHELL);
#define WASM_EXPERIMENTAL_FEATURE(NAME, LOWER_NAME, COMPILE_PRED,       \
                                  COMPILER_PRED, FLAG_PRED, SHELL, ...) \
  enableWasm##NAME = op.getBoolOption("wasm-" SHELL);
  JS_FOR_WASM_FEATURES(WASM_DEFAULT_FEATURE, WASM_DEFAULT_FEATURE,
                       WASM_EXPERIMENTAL_FEATURE);
#undef WASM_DEFAULT_FEATURE
#undef WASM_EXPERIMENTAL_FEATURE

#ifdef ENABLE_WASM_SIMD_WORMHOLE
  enableWasmSimdWormhole = op.getBoolOption("wasm-simd-wormhole");
#endif
  enableWasmVerbose = op.getBoolOption("wasm-verbose");
  enableTestWasmAwaitTier2 = op.getBoolOption("test-wasm-await-tier2");
  enableSourcePragmas = !op.getBoolOption("no-source-pragmas");
  enableAsyncStacks = !op.getBoolOption("no-async-stacks");
  enableAsyncStackCaptureDebuggeeOnly =
      op.getBoolOption("async-stacks-capture-debuggee-only");
  enableStreams = !op.getBoolOption("no-streams");
  enableWeakRefs = !op.getBoolOption("disable-weak-refs");
  enableToSource = !op.getBoolOption("disable-tosource");
  enablePropertyErrorMessageFix =
      !op.getBoolOption("disable-property-error-message-fix");
  enableIteratorHelpers = op.getBoolOption("enable-iterator-helpers");
#ifdef NIGHTLY_BUILD
  enableArrayGrouping = op.getBoolOption("enable-array-grouping");
#endif
#ifdef ENABLE_CHANGE_ARRAY_BY_COPY
  enableChangeArrayByCopy = op.getBoolOption("enable-change-array-by-copy");
#endif
#ifdef ENABLE_NEW_SET_METHODS
  enableNewSetMethods = op.getBoolOption("enable-new-set-methods");
#endif
  enableImportAssertions = op.getBoolOption("enable-import-assertions");
  useFdlibmForSinCosTan = op.getBoolOption("use-fdlibm-for-sin-cos-tan");

  JS::ContextOptionsRef(cx)
      .setAsmJS(enableAsmJS)
      .setWasm(enableWasm)
      .setWasmForTrustedPrinciples(enableWasm)
      .setWasmBaseline(enableWasmBaseline)
#if defined(ENABLE_WASM_CRANELIFT)
      .setWasmCranelift(enableWasmOptimizing)
      .setWasmIon(false)
#else
      .setWasmCranelift(false)
      .setWasmIon(enableWasmOptimizing)
#endif

#define WASM_FEATURE(NAME, ...) .setWasm##NAME(enableWasm##NAME)
          JS_FOR_WASM_FEATURES(WASM_FEATURE, WASM_FEATURE, WASM_FEATURE)
#undef WASM_FEATURE

#ifdef ENABLE_WASM_SIMD_WORMHOLE
      .setWasmSimdWormhole(enableWasmSimdWormhole)
#endif
      .setWasmVerbose(enableWasmVerbose)
      .setTestWasmAwaitTier2(enableTestWasmAwaitTier2)
      .setSourcePragmas(enableSourcePragmas)
      .setAsyncStack(enableAsyncStacks)
      .setAsyncStackCaptureDebuggeeOnly(enableAsyncStackCaptureDebuggeeOnly)
#ifdef NIGHTLY_BUILD
      .setArrayGrouping(enableArrayGrouping)
#endif
#ifdef ENABLE_CHANGE_ARRAY_BY_COPY
      .setChangeArrayByCopy(enableChangeArrayByCopy)
#endif
      .setImportAssertions(enableImportAssertions);

  JS::SetUseFdlibmForSinCosTan(useFdlibmForSinCosTan);

  // Check --fast-warmup first because it sets default warm-up thresholds. These
  // thresholds can then be overridden below by --ion-eager and other flags.
  if (op.getBoolOption("fast-warmup")) {
    jit::JitOptions.setFastWarmUp();
  }

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
      jit::JitOptions.spectreObjectMitigations = true;
      jit::JitOptions.spectreStringMitigations = true;
      jit::JitOptions.spectreValueMasking = true;
      jit::JitOptions.spectreJitToCxxCalls = true;
    } else if (strcmp(str, "off") == 0) {
      jit::JitOptions.spectreIndexMasking = false;
      jit::JitOptions.spectreObjectMitigations = false;
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

  if (const char* str = op.getStringOption("ion-pruning")) {
    if (strcmp(str, "on") == 0) {
      jit::JitOptions.disablePruning = false;
    } else if (strcmp(str, "off") == 0) {
      jit::JitOptions.disablePruning = true;
    } else {
      return OptionFailure("ion-pruning", str);
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

  if (const char* str = op.getStringOption("ion-optimize-shapeguards")) {
    if (strcmp(str, "on") == 0) {
      jit::JitOptions.disableRedundantShapeGuards = false;
    } else if (strcmp(str, "off") == 0) {
      jit::JitOptions.disableRedundantShapeGuards = true;
    } else {
      return OptionFailure("ion-optimize-shapeguards", str);
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

  warmUpThreshold = op.getIntOption("baseline-warmup-threshold");
  if (warmUpThreshold >= 0) {
    jit::JitOptions.baselineJitWarmUpThreshold = warmUpThreshold;
  }

  warmUpThreshold = op.getIntOption("trial-inlining-warmup-threshold");
  if (warmUpThreshold >= 0) {
    jit::JitOptions.trialInliningWarmUpThreshold = warmUpThreshold;
  }

  warmUpThreshold = op.getIntOption("regexp-warmup-threshold");
  if (warmUpThreshold >= 0) {
    jit::JitOptions.regexpWarmUpThreshold = warmUpThreshold;
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

  if (op.getBoolOption("no-warp-async")) {
    jit::JitOptions.warpAsync = false;
  }

  if (op.getBoolOption("no-warp-generator")) {
    jit::JitOptions.warpGenerator = false;
  }

  if (op.getBoolOption("warp-async")) {
    jit::JitOptions.warpAsync = true;
  }

  if (op.getBoolOption("warp-generator")) {
    jit::JitOptions.warpGenerator = true;
  }

  if (op.getBoolOption("no-ion")) {
    jit::JitOptions.ion = false;
  }

  if (op.getBoolOption("no-native-regexp")) {
    jit::JitOptions.nativeRegExp = false;
  }

  if (op.getBoolOption("trace-regexp-parser")) {
    jit::JitOptions.traceRegExpParser = true;
  }
  if (op.getBoolOption("trace-regexp-assembler")) {
    jit::JitOptions.traceRegExpAssembler = true;
  }
  if (op.getBoolOption("trace-regexp-interpreter")) {
    jit::JitOptions.traceRegExpInterpreter = true;
  }
  if (op.getBoolOption("trace-regexp-peephole")) {
    jit::JitOptions.traceRegExpPeephole = true;
  }

  if (op.getBoolOption("less-debug-code")) {
    jit::JitOptions.lessDebugCode = true;
  }

  int32_t inliningEntryThreshold = op.getIntOption("inlining-entry-threshold");
  if (inliningEntryThreshold > 0) {
    jit::JitOptions.inliningEntryThreshold = inliningEntryThreshold;
  }

  int32_t smallFunctionLength = op.getIntOption("small-function-length");
  if (smallFunctionLength > 0) {
    jit::JitOptions.smallFunctionMaxBytecodeLength = smallFunctionLength;
  }

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

  if (op.getBoolOption("no-large-arraybuffers")) {
    JS::SetLargeArrayBuffersEnabled(false);
  }

  if (op.getBoolOption("disable-bailout-loop-check")) {
    jit::JitOptions.disableBailoutLoopCheck = true;
  }

  if (op.getBoolOption("enable-watchtower")) {
    jit::JitOptions.enableWatchtowerMegamorphic = true;
  }
  if (op.getBoolOption("disable-watchtower")) {
    jit::JitOptions.enableWatchtowerMegamorphic = false;
  }

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
#elif defined(JS_SIMULATOR_LOONG64)
  if (op.getBoolOption("loong64-sim-icache-checks")) {
    jit::SimulatorProcess::ICacheCheckingDisableCount = 0;
  }

  int32_t stopAt = op.getIntOption("loong64-sim-stop-at");
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
#if defined(ENABLE_WASM_CRANELIFT)
      .setWasmCranelift(enableWasmOptimizing)
      .setWasmIon(false)
#else
      .setWasmCranelift(false)
      .setWasmIon(enableWasmOptimizing)
#endif
#define WASM_FEATURE(NAME, ...) .setWasm##NAME(enableWasm##NAME)
          JS_FOR_WASM_FEATURES(WASM_FEATURE, WASM_FEATURE, WASM_FEATURE)
#undef WASM_FEATURE

#ifdef ENABLE_WASM_SIMD_WORMHOLE
      .setWasmSimdWormhole(enableWasmSimdWormhole)
#endif
      .setWasmVerbose(enableWasmVerbose)
      .setTestWasmAwaitTier2(enableTestWasmAwaitTier2)
      .setSourcePragmas(enableSourcePragmas);

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

  JS_SetNativeStackQuota(cx, gWorkerStackSize);
}

[[nodiscard]] static bool PrintUnhandledRejection(
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

[[nodiscard]] static bool ReportUnhandledRejections(JSContext* cx) {
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

  Rooted<ArrayObject*> resultObj(cx, &obj->as<ArrayObject>());
  while (true) {
    bool done = SetIteratorObject::next(iterObj, resultObj);
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

static int Shell(JSContext* cx, OptionParser* op) {
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
#ifdef __wasi__
    MOZ_CRASH("WASI doesn't support wasm");
#else
    if (!WasmCompileAndSerialize(cx)) {
      // Errors have been printed directly to stderr.
      MOZ_ASSERT(!cx->isExceptionPending());
      return -1;
    }
#endif
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

#ifdef DEBUG
  if (op->getBoolOption("differential-testing")) {
    JS::SetSupportDifferentialTesting(true);
  }
#endif

  if (op->getBoolOption("disable-oom-functions")) {
    disableOOMFunctions = true;
  }

  if (op->getBoolOption("more-compartments")) {
    defaultToSameCompartment = false;
  }

  bool reprl_mode = FuzzilliUseReprlMode(op);

  // Begin REPRL Loop
  int result = EXIT_SUCCESS;
  do {
    JS::RealmOptions options;
    SetStandardRealmOptions(options);
    RootedObject glob(
        cx, NewGlobalObject(cx, options, nullptr, ShellGlobalKind::WindowProxy,
                            /* immutablePrototype = */ true));
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
    sc->exitCode = 0;
    result = EXIT_SUCCESS;
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

#ifdef FUZZING_JS_FUZZILLI
    if (reprl_mode) {
      fflush(stdout);
      fflush(stderr);
      // Send return code to parent and reset edge counters.
      int status = (result & 0xff) << 8;
      MOZ_RELEASE_ASSERT(write(REPRL_CWFD, &status, 4) == 4);
      __sanitizer_cov_reset_edgeguards();
    }
#endif

    if (enableDisassemblyDumps) {
      AutoReportException are(cx);
      if (!js::DumpRealmPCCounts(cx)) {
        result = EXITCODE_OUT_OF_MEMORY;
      }
    }

    // End REPRL loop
  } while (reprl_mode);

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

static bool ReadSelfHostedXDRFile(JSContext* cx, FileContents& buf) {
  FILE* file = fopen(selfHostedXDRPath, "rb");
  if (!file) {
    fprintf(stderr, "Can't open self-hosted stencil XDR file.\n");
    return false;
  }
  AutoCloseFile autoClose(file);

  struct stat st;
  if (fstat(fileno(file), &st) < 0) {
    fprintf(stderr, "Unable to stat self-hosted stencil XDR file.\n");
    return false;
  }

  if (st.st_size >= INT32_MAX) {
    fprintf(stderr, "self-hosted stencil XDR file too large.\n");
    return false;
  }
  uint32_t filesize = uint32_t(st.st_size);

  if (!buf.growBy(filesize)) {
    return false;
  }
  size_t cc = fread(buf.begin(), 1, filesize, file);
  if (cc != filesize) {
    fprintf(stderr, "Short read on self-hosted stencil XDR file.\n");
    return false;
  }

  return true;
}

static bool WriteSelfHostedXDRFile(JSContext* cx, JS::SelfHostedCache buffer) {
  FILE* file = fopen(selfHostedXDRPath, "wb");
  if (!file) {
    JS_ReportErrorUTF8(cx, "Can't open self-hosted stencil XDR file.");
    return false;
  }
  AutoCloseFile autoClose(file);

  size_t cc = fwrite(buffer.Elements(), 1, buffer.LengthBytes(), file);
  if (cc != buffer.LengthBytes()) {
    JS_ReportErrorUTF8(cx, "Short write on self-hosted stencil XDR file.");
    return false;
  }

  return true;
}

static bool SetGCParameterFromArg(JSContext* cx, char* arg) {
  char* c = strchr(arg, '=');
  if (!c) {
    fprintf(stderr,
            "Error: --gc-param argument '%s' must be of the form "
            "name=decimalValue\n",
            arg);
    return false;
  }

  *c = '\0';
  const char* name = arg;
  const char* valueStr = c + 1;

  JSGCParamKey key;
  bool writable;
  if (!GetGCParameterInfo(name, &key, &writable)) {
    fprintf(stderr, "Error: Unknown GC parameter name '%s'\n", name);
    fprintf(stderr, "Writable GC parameter names are:\n");
#define PRINT_WRITABLE_PARAM_NAME(name, _, writable) \
  if (writable) {                                    \
    fprintf(stderr, "  %s\n", name);                 \
  }
    FOR_EACH_GC_PARAM(PRINT_WRITABLE_PARAM_NAME)
#undef PRINT_WRITABLE_PARAM_NAME
    return false;
  }

  if (!writable) {
    fprintf(stderr, "Error: GC parameter '%s' is not writable\n", name);
    return false;
  }

  char* end = nullptr;
  unsigned long int value = strtoul(valueStr, &end, 10);
  if (end == valueStr || *end) {
    fprintf(stderr,
            "Error: Could not parse '%s' as decimal for GC parameter '%s'\n",
            valueStr, name);
    return false;
  }

  uint32_t paramValue = uint32_t(value);
  if (value == ULONG_MAX || value != paramValue ||
      !cx->runtime()->gc.setParameter(key, paramValue)) {
    fprintf(stderr, "Error: Value %s is out of range for GC parameter '%s'\n",
            valueStr, name);
    return false;
  }

  return true;
}

int main(int argc, char** argv) {
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
      !op.addMultiStringOption('e', "execute", "CODE", "Inline code to run") ||
      !op.addStringOption('\0', "selfhosted-xdr-path", "[filename]",
                          "Read/Write selfhosted script data from/to the given "
                          "XDR file") ||
      !op.addStringOption('\0', "selfhosted-xdr-mode", "(encode,decode,off)",
                          "Whether to encode/decode data of the file provided"
                          "with --selfhosted-xdr-path.") ||
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
      !op.addIntOption('\0', "inlining-entry-threshold", "COUNT",
                       "The minimum stub entry count before trial-inlining a"
                       " call",
                       -1) ||
      !op.addIntOption('\0', "small-function-length", "COUNT",
                       "The maximum bytecode length of a 'small function' for "
                       "the purpose of inlining.",
                       -1) ||
      !op.addBoolOption('\0', "no-asmjs", "Disable asm.js compilation") ||
      !op.addStringOption(
          '\0', "wasm-compiler", "[option]",
          "Choose to enable a subset of the wasm compilers, valid options are "
          "'none', 'baseline', 'ion', 'cranelift', 'optimizing', "
          "'baseline+ion', "
          "'baseline+cranelift', 'baseline+optimizing'. Choosing 'ion' when "
          "Ion is "
          "not available or 'cranelift' when Cranelift is not available will "
          "fail; "
          "use 'optimizing' for cross-compiler compatibility.") ||
      !op.addBoolOption('\0', "wasm-verbose",
                        "Enable WebAssembly verbose logging") ||
      !op.addBoolOption('\0', "disable-wasm-huge-memory",
                        "Disable WebAssembly huge memory") ||
      !op.addBoolOption('\0', "test-wasm-await-tier2",
                        "Forcibly activate tiering and block "
                        "instantiation on completion of tier2") ||
#define WASM_DEFAULT_FEATURE(NAME, LOWER_NAME, COMPILE_PRED, COMPILER_PRED, \
                             FLAG_PRED, SHELL, ...)                         \
  !op.addBoolOption('\0', "no-wasm-" SHELL, "Disable wasm " SHELL "feature.") ||
#define WASM_TENTATIVE_FEATURE(NAME, LOWER_NAME, COMPILE_PRED, COMPILER_PRED, \
                               FLAG_PRED, SHELL, ...)                         \
  !op.addBoolOption('\0', "no-wasm-" SHELL,                                   \
                    "Disable wasm " SHELL "feature.") ||                      \
      !op.addBoolOption('\0', "wasm-" SHELL, "No-op.") ||
#define WASM_EXPERIMENTAL_FEATURE(NAME, LOWER_NAME, COMPILE_PRED,       \
                                  COMPILER_PRED, FLAG_PRED, SHELL, ...) \
  !op.addBoolOption('\0', "wasm-" SHELL,                                \
                    "Enable experimental wasm " SHELL "feature.") ||    \
      !op.addBoolOption('\0', "no-wasm-" SHELL, "No-op.") ||
      JS_FOR_WASM_FEATURES(WASM_DEFAULT_FEATURE, WASM_TENTATIVE_FEATURE,
                           WASM_EXPERIMENTAL_FEATURE)
#undef WASM_DEFAULT_FEATURE
#undef WASM_TENTATIVE_FEATURE
#undef WASM_EXPERIMENTAL_FEATURE
#ifdef ENABLE_WASM_SIMD_WORMHOLE
          !op.addBoolOption('\0', "wasm-simd-wormhole",
                            "Enable wasm SIMD wormhole (UTSL)") ||
#else
          !op.addBoolOption('\0', "wasm-simd-wormhole", "No-op") ||
#endif
      !op.addBoolOption('\0', "no-native-regexp",
                        "Disable native regexp compilation") ||
      !op.addIntOption(
          '\0', "regexp-warmup-threshold", "COUNT",
          "Wait for COUNT invocations before compiling regexps to native code "
          "(default 10)",
          -1) ||
      !op.addBoolOption('\0', "trace-regexp-parser", "Trace regexp parsing") ||
      !op.addBoolOption('\0', "trace-regexp-assembler",
                        "Trace regexp assembler") ||
      !op.addBoolOption('\0', "trace-regexp-interpreter",
                        "Trace regexp interpreter") ||
      !op.addBoolOption('\0', "trace-regexp-peephole",
                        "Trace regexp peephole optimization") ||
      !op.addBoolOption('\0', "less-debug-code",
                        "Emit less machine code for "
                        "checking assertions under DEBUG.") ||
      !op.addBoolOption('\0', "enable-streams",
                        "Enable WHATWG Streams (default)") ||
      !op.addBoolOption('\0', "no-streams", "Disable WHATWG Streams") ||
      !op.addBoolOption('\0', "disable-weak-refs", "Disable weak references") ||
      !op.addBoolOption('\0', "disable-tosource", "Disable toSource/uneval") ||
      !op.addBoolOption('\0', "disable-property-error-message-fix",
                        "Disable fix for the error message when accessing "
                        "property of null or undefined") ||
      !op.addBoolOption('\0', "enable-iterator-helpers",
                        "Enable iterator helpers") ||
      !op.addBoolOption('\0', "enable-array-grouping",
                        "Enable Array Grouping") ||
#ifdef ENABLE_CHANGE_ARRAY_BY_COPY
      !op.addBoolOption('\0', "enable-change-array-by-copy",
                        "Enable change-array-by-copy methods") ||
      !op.addBoolOption('\0', "disable-change-array-by-copy",
                        "Disable change-array-by-copy methods") ||
#else
      !op.addBoolOption('\0', "enable-change-array-by-copy", "no-op") ||
      !op.addBoolOption('\0', "disable-change-array-by-copy", "no-op") ||
#endif
#ifdef ENABLE_NEW_SET_METHODS
      !op.addBoolOption('\0', "enable-new-set-methods",
                        "Enable New Set methods") ||
      !op.addBoolOption('\0', "disable-new-set-methods",
                        "Disable New Set methods") ||
#else
      !op.addBoolOption('\0', "enable-new-set-methods", "no-op") ||
      !op.addBoolOption('\0', "disable-new-set-methods", "no-op") ||
#endif
      !op.addBoolOption('\0', "enable-top-level-await",
                        "Enable top-level await") ||
      !op.addBoolOption('\0', "enable-class-static-blocks",
                        "(no-op) Enable class static blocks") ||
      !op.addBoolOption('\0', "enable-import-assertions",
                        "Enable import assertions") ||
      !op.addBoolOption('\0', "no-large-arraybuffers",
                        "Disallow creating ArrayBuffers larger than 2 GB on "
                        "64-bit platforms") ||
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
      !op.addStringOption('\0', "ion-pruning", "on/off",
                          "Branch pruning (default: on, off to disable)") ||
      !op.addStringOption('\0', "ion-range-analysis", "on/off",
                          "Range analysis (default: on, off to disable)") ||
      !op.addStringOption('\0', "ion-sink", "on/off",
                          "Sink code motion (default: off, on to enable)") ||
      !op.addStringOption('\0', "ion-optimization-levels", "on/off",
                          "No-op for fuzzing") ||
      !op.addStringOption('\0', "ion-loop-unrolling", "on/off",
                          "(NOP for fuzzers)") ||
      !op.addStringOption(
          '\0', "ion-instruction-reordering", "on/off",
          "Instruction reordering (default: off, on to enable)") ||
      !op.addStringOption(
          '\0', "ion-optimize-shapeguards", "on/off",
          "Eliminate redundant shape guards (default: on, off to disable)") ||
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
      !op.addBoolOption('\0', "disable-bailout-loop-check",
                        "Turn off bailout loop check") ||
      !op.addBoolOption('\0', "enable-watchtower",
                        "Enable Watchtower optimizations") ||
      !op.addBoolOption('\0', "disable-watchtower",
                        "Disable Watchtower optimizations") ||
      !op.addBoolOption('\0', "scalar-replace-arguments",
                        "Use scalar replacement to optimize ArgumentsObject") ||
      !op.addStringOption(
          '\0', "ion-limit-script-size", "on/off",
          "Don't compile very large scripts (default: on, off to disable)") ||
      !op.addIntOption('\0', "ion-warmup-threshold", "COUNT",
                       "Wait for COUNT calls or iterations before compiling "
                       "at the normal optimization level (default: 1000)",
                       -1) ||
      !op.addIntOption('\0', "ion-full-warmup-threshold", "COUNT",
                       "No-op for fuzzing", -1) ||
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
      !op.addBoolOption('\0', "fast-warmup",
                        "Reduce warmup thresholds for each tier.") ||
      !op.addStringOption('\0', "ion-offthread-compile", "on/off",
                          "Compile scripts off thread (default: on)") ||
      !op.addStringOption('\0', "ion-parallel-compile", "on/off",
                          "--ion-parallel compile is deprecated. Use "
                          "--ion-offthread-compile.") ||
      !op.addBoolOption('\0', "baseline",
                        "Enable baseline compiler (default)") ||
      !op.addBoolOption('\0', "no-baseline", "Disable baseline compiler") ||
      !op.addBoolOption('\0', "no-warp-async",
                        "Don't Warp compile Async Functions") ||
      !op.addBoolOption('\0', "no-warp-generator",
                        "Warp compile Generator Functions") ||
      !op.addBoolOption('\0', "warp-async", "Warp compile Async Functions") ||
      !op.addBoolOption('\0', "warp-generator",
                        "Don't Warp compile Generator Functions") ||
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
      !op.addIntOption(
          '\0', "trial-inlining-warmup-threshold", "COUNT",
          "Wait for COUNT calls or iterations before trial-inlining "
          "(default: 500)",
          -1) ||
      !op.addBoolOption(
          '\0', "non-writable-jitcode",
          "(NOP for fuzzers) Allocate JIT code as non-writable memory.") ||
      !op.addBoolOption(
          '\0', "no-sse3",
          "Pretend CPU does not support SSE3 instructions and above "
          "to test JIT codegen (no-op on platforms other than x86 and x64).") ||
      !op.addBoolOption(
          '\0', "no-ssse3",
          "Pretend CPU does not support SSSE3 [sic] instructions and above "
          "to test JIT codegen (no-op on platforms other than x86 and x64).") ||
      !op.addBoolOption(
          '\0', "no-sse41",
          "Pretend CPU does not support SSE4.1 instructions "
          "to test JIT codegen (no-op on platforms other than x86 and x64).") ||
      !op.addBoolOption('\0', "no-sse4", "Alias for --no-sse41") ||
      !op.addBoolOption(
          '\0', "no-sse42",
          "Pretend CPU does not support SSE4.2 instructions "
          "to test JIT codegen (no-op on platforms other than x86 and x64).") ||
#ifdef ENABLE_WASM_AVX
      !op.addBoolOption('\0', "enable-avx",
                        "No-op. AVX is enabled by default, if available.") ||
      !op.addBoolOption(
          '\0', "no-avx",
          "Pretend CPU does not support AVX or AVX2 instructions "
          "to test JIT codegen (no-op on platforms other than x86 and x64).") ||
#else
      !op.addBoolOption('\0', "enable-avx",
                        "AVX is disabled by default. Enable AVX. "
                        "(no-op on platforms other than x86 and x64).") ||
      !op.addBoolOption('\0', "no-avx",
                        "No-op. AVX is currently disabled by default.") ||
#endif
      !op.addBoolOption('\0', "more-compartments",
                        "Make newGlobal default to creating a new "
                        "compartment.") ||
      !op.addBoolOption('\0', "fuzzing-safe",
                        "Don't expose functions that aren't safe for "
                        "fuzzers to call") ||
#ifdef DEBUG
      !op.addBoolOption('\0', "differential-testing",
                        "Avoid random/undefined behavior that disturbs "
                        "differential testing (correctness fuzzing)") ||
#endif
      !op.addBoolOption('\0', "disable-oom-functions",
                        "Disable functions that cause "
                        "artificial OOMs") ||
      !op.addBoolOption('\0', "no-threads", "Disable helper threads") ||
      !op.addBoolOption(
          '\0', "no-jit-backend",
          "Disable the JIT backend completely for this process") ||
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
      !op.addBoolOption('\0', "loong64-sim-icache-checks",
                        "Enable icache flush checks in the LoongArch64 "
                        "simulator.") ||
      !op.addIntOption('\0', "loong64-sim-stop-at", "NUMBER",
                       "Stop the LoongArch64 simulator after the given "
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
      !op.addMultiStringOption('\0', "gc-param", "NAME=VALUE",
                               "Set a named GC parameter") ||
      !op.addStringOption('\0', "module-load-path", "DIR",
                          "Set directory to load modules from") ||
      !op.addBoolOption('\0', "no-source-pragmas",
                        "Disable source(Mapping)URL pragma parsing") ||
      !op.addBoolOption('\0', "no-async-stacks", "Disable async stacks") ||
      !op.addBoolOption('\0', "async-stacks-capture-debuggee-only",
                        "Limit async stack capture to only debuggees") ||
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
      !op.addStringOption(
          '\0', "delazification-mode", "[option]",
          "Select one of the delazification mode for scripts given on the "
          "command line, valid options are: "
          "'on-demand', 'concurrent-df', 'eager', 'concurrent-df+on-demand'. "
          "Choosing 'concurrent-df+on-demand' will run both concurrent-df and "
          "on-demand delazification mode, and compare compilation outcome. ") ||
      !op.addBoolOption('\0', "wasm-compile-and-serialize",
                        "Compile the wasm bytecode from stdin and serialize "
                        "the results to stdout") ||
#ifdef FUZZING_JS_FUZZILLI
      !op.addBoolOption('\0', "reprl", "Enable REPRL mode for fuzzing") ||
#endif
      !op.addStringOption('\0', "telemetry-dir", "[directory]",
                          "Output telemetry results in a directory") ||
      !op.addBoolOption('\0', "use-fdlibm-for-sin-cos-tan",
                        "Use fdlibm for Math.sin, Math.cos, and Math.tan")) {
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

  // Note: DisableJitBackend must be called before JS_InitWithFailureDiagnostic.
  if (op.getBoolOption("no-jit-backend")) {
    JS::DisableJitBackend();
  }

#if defined(JS_CODEGEN_ARM)
  if (const char* str = op.getStringOption("arm-hwcap")) {
    jit::SetARMHwCapFlagsString(str);
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

  // Fish around in `op` for various important compiler-configuration flags
  // and make sure they get handed on to any child processes we might create.
  // See bug 1700900.  Semantically speaking, this is all rather dubious:
  //
  // * What set of flags need to be propagated in order to guarantee that the
  //   child produces code that is "compatible" (in whatever sense) with that
  //   produced by the parent?  This isn't always easy to determine.
  //
  // * There's nothing that ensures that flags given to the child are
  //   presented in the same order that they exist in the parent's `argv[]`.
  //   That could be a problem in the case where two flags with contradictory
  //   meanings are given, and they are presented to the child in the opposite
  //   order.  For example: --wasm-compiler=optimizing --wasm-compiler=baseline.

#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
  MOZ_ASSERT(!js::jit::CPUFlagsHaveBeenComputed());

  if (op.getBoolOption("no-sse3")) {
    js::jit::CPUInfo::SetSSE3Disabled();
    if (!sCompilerProcessFlags.append("--no-sse3")) {
      return EXIT_FAILURE;
    }
  }
  if (op.getBoolOption("no-ssse3")) {
    js::jit::CPUInfo::SetSSSE3Disabled();
    if (!sCompilerProcessFlags.append("--no-ssse3")) {
      return EXIT_FAILURE;
    }
  }
  if (op.getBoolOption("no-sse4") || op.getBoolOption("no-sse41")) {
    js::jit::CPUInfo::SetSSE41Disabled();
    if (!sCompilerProcessFlags.append("--no-sse41")) {
      return EXIT_FAILURE;
    }
  }
  if (op.getBoolOption("no-sse42")) {
    js::jit::CPUInfo::SetSSE42Disabled();
    if (!sCompilerProcessFlags.append("--no-sse42")) {
      return EXIT_FAILURE;
    }
  }
  if (op.getBoolOption("no-avx")) {
    js::jit::CPUInfo::SetAVXDisabled();
    if (!sCompilerProcessFlags.append("--no-avx")) {
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

  // Start the engine.
  if (const char* message = JS_InitWithFailureDiagnostic()) {
    fprintf(gErrFile->fp, "JS_Init failed: %s\n", message);
    return 1;
  }

  // `selfHostedXDRBuffer` contains XDR buffer of the self-hosted JS.
  // A part of it is borrowed by ImmutableScriptData of the self-hosted scripts.
  //
  // This buffer's should outlive JS_Shutdown.
  Maybe<FileContents> selfHostedXDRBuffer;

  auto shutdownEngine = MakeScopeExit([] { JS_ShutDown(); });

  // Record aggregated telemetry data on disk. Do this as early as possible such
  // that the telemetry is recording both before starting the context and after
  // closing it.
  if (op.getStringOption("telemetry-dir")) {
    if (!telemetryLock) {
      telemetryLock = js_new<Mutex>(mutexid::ShellTelemetry);
      if (!telemetryLock) {
        return EXIT_FAILURE;
      }
    }
  }
  auto writeTelemetryResults = MakeScopeExit([&op] {
    if (const char* dir = op.getStringOption("telemetry-dir")) {
      WriteTelemetryDataToDisk(dir);
      js_free(telemetryLock);
      telemetryLock = nullptr;
    }
  });

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

  if (op.getBoolOption("no-threads")) {
    js::DisableExtraThreads();
  }

  enableCodeCoverage = op.getBoolOption("code-coverage");
  if (enableCodeCoverage) {
    js::EnableCodeCoverage();
  }

  // If LCov is enabled, then the default delazification mode should be changed
  // to parse everything eagerly, such that we know the location of every
  // instruction, to report them in the LCov summary, even if there is no uses
  // of these instructions.
  //
  // Note: code coverage can be enabled either using the --code-coverage command
  // line, or the JS_CODE_COVERAGE_OUTPUT_DIR environment variable, which is
  // processed by JS_InitWithFailureDiagnostic.
  if (coverage::IsLCovEnabled()) {
    defaultDelazificationMode =
        JS::DelazificationOption::ParseEverythingEagerly;
  }

  if (const char* xdr = op.getStringOption("selfhosted-xdr-path")) {
    shell::selfHostedXDRPath = xdr;
  }
  if (const char* opt = op.getStringOption("selfhosted-xdr-mode")) {
    if (strcmp(opt, "encode") == 0) {
      shell::encodeSelfHostedCode = true;
    } else if (strcmp(opt, "decode") == 0) {
      shell::encodeSelfHostedCode = false;
    } else if (strcmp(opt, "off") == 0) {
      shell::selfHostedXDRPath = nullptr;
    } else {
      MOZ_CRASH(
          "invalid option value for --selfhosted-xdr-mode, must be "
          "encode/decode");
    }
  }

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

  // Register telemetry callbacks, if needed.
  if (telemetryLock) {
    JS_SetAccumulateTelemetryCallback(cx, AccumulateTelemetryDataCallback);
  }

  size_t nurseryBytes = op.getIntOption("nursery-size") * 1024L * 1024L;
  if (nurseryBytes == 0) {
    fprintf(stderr, "Error: --nursery-size parameter must be non-zero.\n");
    fprintf(stderr,
            "The nursery can be disabled by passing the --no-ggc option.\n");
    return EXIT_FAILURE;
  }
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

  size_t availMemMB = op.getIntOption("available-memory");
  if (availMemMB > 0) {
    JS_SetGCParametersBasedOnAvailableMemory(cx, availMemMB);
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

  // The file content should stay alive as long as Worker thread can be
  // initialized.
  JS::SelfHostedCache xdrSpan = nullptr;
  JS::SelfHostedWriter xdrWriter = nullptr;
  if (selfHostedXDRPath) {
    if (encodeSelfHostedCode) {
      xdrWriter = WriteSelfHostedXDRFile;
    } else {
      selfHostedXDRBuffer.emplace(cx);
      if (ReadSelfHostedXDRFile(cx, *selfHostedXDRBuffer)) {
        MOZ_ASSERT(selfHostedXDRBuffer->length() > 0);
        JS::SelfHostedCache span(selfHostedXDRBuffer->begin(),
                                 selfHostedXDRBuffer->end());
        xdrSpan = span;
      } else {
        fprintf(stderr, "Falling back on parsing source.\n");
        selfHostedXDRPath = nullptr;
      }
    }
  }

#ifndef __wasi__
  // This must be set before self-hosted code is initialized, as self-hosted
  // code reads the property and the property may not be changed later.
  bool disabledHugeMemory = false;
  if (op.getBoolOption("disable-wasm-huge-memory")) {
    disabledHugeMemory = JS::DisableWasmHugeMemory();
    MOZ_RELEASE_ASSERT(disabledHugeMemory);
  }
#endif

  if (!JS::InitSelfHostedCode(cx, xdrSpan, xdrWriter)) {
    return 1;
  }

  EnvironmentPreparer environmentPreparer(cx);

  bool incrementalGC = !op.getBoolOption("no-incremental-gc");
  JS_SetGCParameter(cx, JSGC_INCREMENTAL_GC_ENABLED, incrementalGC);
  JS_SetGCParameter(cx, JSGC_SLICE_TIME_BUDGET_MS, 5);

  JS_SetGCParameter(cx, JSGC_PER_ZONE_GC_ENABLED, true);

  for (MultiStringRange args = op.getMultiStringOption("gc-param");
       !args.empty(); args.popFront()) {
    if (!SetGCParameterFromArg(cx, args.front())) {
      return EXIT_FAILURE;
    }
  }

  JS::SetProcessLargeAllocationFailureCallback(my_LargeAllocFailCallback);

  js::SetPreserveWrapperCallbacks(cx, DummyPreserveWrapperCallback,
                                  DummyHasReleasedWrapperCallback);

#ifndef __wasi__
  // --disable-wasm-huge-memory needs to be propagated.  See bug 1518210.
  if (disabledHugeMemory &&
      !sCompilerProcessFlags.append("--disable-wasm-huge-memory")) {
    return EXIT_FAILURE;
  }

  // Also the following are to be propagated.
  const char* to_propagate[] = {
#  define WASM_DEFAULT_FEATURE(NAME, LOWER_NAME, COMPILE_PRED, COMPILER_PRED, \
                               FLAG_PRED, SHELL, ...)                         \
    "--no-wasm-" SHELL,
#  define WASM_EXPERIMENTAL_FEATURE(NAME, LOWER_NAME, COMPILE_PRED,       \
                                    COMPILER_PRED, FLAG_PRED, SHELL, ...) \
    "--wasm-" SHELL,
      JS_FOR_WASM_FEATURES(WASM_DEFAULT_FEATURE, WASM_DEFAULT_FEATURE,
                           WASM_EXPERIMENTAL_FEATURE)
#  undef WASM_DEFAULT_FEATURE
#  undef WASM_EXPERIMENTAL_FEATURE
      // Feature selection options
      "--wasm-simd-wormhole",
      // Compiler selection options
      "--test-wasm-await-tier2", NULL};
  for (const char** p = &to_propagate[0]; *p; p++) {
    if (op.getBoolOption(&(*p)[2] /* 2 => skip the leading '--' */)) {
      if (!sCompilerProcessFlags.append(*p)) {
        return EXIT_FAILURE;
      }
    }
  }

  // Also --wasm-compiler= is to be propagated.  This is tricky because it is
  // necessary to reconstitute the --wasm-compiler=<whatever> string from its
  // pieces, without causing a leak.  Hence it is copied into a static buffer.
  // This is thread-unsafe, but we're in `main()` and on the process' root
  // thread.  Also, we do this only once -- it wouldn't work properly if we
  // handled multiple --wasm-compiler= flags in a loop.
  const char* wasm_compiler = op.getStringOption("wasm-compiler");
  if (wasm_compiler) {
    size_t n_needed =
        2 + strlen("wasm-compiler") + 1 + strlen(wasm_compiler) + 1;
    const size_t n_avail = 128;
    static char buf[n_avail];
    // `n_needed` depends on the compiler name specified.  However, it can't
    // be arbitrarily long, since previous flag-checking should have limited
    // it to a set of known possibilities: "baseline", "ion", "cranelift",
    // "baseline+ion", "baseline+cranelift", etc.  Still, assert this for
    // safety.
    MOZ_RELEASE_ASSERT(n_needed < n_avail);
    memset(buf, 0, sizeof(buf));
    SprintfBuf(buf, n_avail, "--%s=%s", "wasm-compiler", wasm_compiler);
    if (!sCompilerProcessFlags.append(buf)) {
      return EXIT_FAILURE;
    }
  }
#endif  // __wasi__

  result = Shell(cx, &op);

#ifdef DEBUG
  if (OOM_printAllocationCount) {
    printf("OOM max count: %" PRIu64 "\n", js::oom::simulator.counter());
  }
#endif

  return result;
}
