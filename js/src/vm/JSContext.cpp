/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS execution context.
 */

#include "vm/JSContext-inl.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Sprintf.h"
#include "mozilla/TextUtils.h"
#include "mozilla/Unused.h"

#include <stdarg.h>
#include <string.h>
#ifdef ANDROID
#  include <android/log.h>
#  include <fstream>
#  include <string>
#endif  // ANDROID
#ifdef XP_WIN
#  include <processthreadsapi.h>
#endif  // XP_WIN

#include "jsexn.h"
#include "jspubtd.h"
#include "jstypes.h"

#include "gc/FreeOp.h"
#include "gc/Marking.h"
#include "jit/Ion.h"
#include "jit/PcScriptCache.h"
#include "js/CharacterEncoding.h"
#include "js/ContextOptions.h"  // JS::ContextOptions
#include "js/Printf.h"
#ifdef JS_SIMULATOR_ARM64
#  include "jit/arm64/vixl/Simulator-vixl.h"
#endif
#ifdef JS_SIMULATOR_ARM
#  include "jit/arm/Simulator-arm.h"
#endif
#include "util/DoubleToString.h"
#include "util/NativeStack.h"
#include "util/Windows.h"
#include "vm/BytecodeUtil.h"
#include "vm/ErrorReporting.h"
#include "vm/HelperThreads.h"
#include "vm/Iteration.h"
#include "vm/JSAtom.h"
#include "vm/JSFunction.h"
#include "vm/JSObject.h"
#include "vm/JSScript.h"
#include "vm/Realm.h"
#include "vm/Shape.h"

#include "vm/Compartment-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/Stack-inl.h"

using namespace js;

using mozilla::DebugOnly;
using mozilla::PodArrayZero;

bool js::AutoCycleDetector::init() {
  MOZ_ASSERT(cyclic);

  AutoCycleDetector::Vector& vector = cx->cycleDetectorVector();

  for (JSObject* obj2 : vector) {
    if (MOZ_UNLIKELY(obj == obj2)) {
      return true;
    }
  }

  if (!vector.append(obj)) {
    return false;
  }

  cyclic = false;
  return true;
}

js::AutoCycleDetector::~AutoCycleDetector() {
  if (MOZ_LIKELY(!cyclic)) {
    AutoCycleDetector::Vector& vec = cx->cycleDetectorVector();
    MOZ_ASSERT(vec.back() == obj);
    if (vec.length() > 1) {
      vec.popBack();
    } else {
      // Avoid holding on to unused heap allocations.
      vec.clearAndFree();
    }
  }
}

bool JSContext::init(ContextKind kind) {
  // Skip most of the initialization if this thread will not be running JS.
  if (kind == ContextKind::MainThread) {
    TlsContext.set(this);
    currentThread_ = ThisThread::GetId();
    if (!regexpStack.ref().init()) {
      return false;
    }

    if (!fx.initInstance()) {
      return false;
    }

#ifdef JS_SIMULATOR
    simulator_ = jit::Simulator::Create();
    if (!simulator_) {
      return false;
    }
#endif

  } else {
    atomsZoneFreeLists_ = js_new<gc::FreeLists>();
    if (!atomsZoneFreeLists_) {
      return false;
    }
  }

  // Set the ContextKind last, so that ProtectedData checks will allow us to
  // initialize this context before it becomes the runtime's active context.
  kind_ = kind;

  return true;
}

JSContext* js::NewContext(uint32_t maxBytes, uint32_t maxNurseryBytes,
                          JSRuntime* parentRuntime) {
  AutoNoteSingleThreadedRegion anstr;

  MOZ_RELEASE_ASSERT(!TlsContext.get());

#if defined(DEBUG) || defined(JS_OOM_BREAKPOINT)
  js::oom::SetThreadType(!parentRuntime ? js::THREAD_TYPE_MAIN
                                        : js::THREAD_TYPE_WORKER);
#endif

  JSRuntime* runtime = js_new<JSRuntime>(parentRuntime);
  if (!runtime) {
    return nullptr;
  }

  JSContext* cx = js_new<JSContext>(runtime, JS::ContextOptions());
  if (!cx) {
    js_delete(runtime);
    return nullptr;
  }

  if (!cx->init(ContextKind::MainThread)) {
    js_delete(cx);
    js_delete(runtime);
    return nullptr;
  }

  if (!runtime->init(cx, maxBytes, maxNurseryBytes)) {
    runtime->destroyRuntime();
    js_delete(cx);
    js_delete(runtime);
    return nullptr;
  }

  return cx;
}

void js::DestroyContext(JSContext* cx) {
  JS_AbortIfWrongThread(cx);

  cx->checkNoGCRooters();

  // Cancel all off thread Ion compiles. Completed Ion compiles may try to
  // interrupt this context. See HelperThread::handleIonWorkload.
  CancelOffThreadIonCompile(cx->runtime());

  cx->jobQueue = nullptr;
  cx->internalJobQueue = nullptr;
  SetContextProfilingStack(cx, nullptr);

  JSRuntime* rt = cx->runtime();

  // Flush promise tasks executing in helper threads early, before any parts
  // of the JSRuntime that might be visible to helper threads are torn down.
  rt->offThreadPromiseState.ref().shutdown(cx);

  // Destroy the runtime along with its last context.
  js::AutoNoteSingleThreadedRegion nochecks;
  rt->destroyRuntime();
  js_delete_poison(cx);
  js_delete_poison(rt);
}

void JS::RootingContext::checkNoGCRooters() {
#ifdef DEBUG
  for (auto const& stackRootPtr : stackRoots_) {
    MOZ_ASSERT(stackRootPtr == nullptr);
  }
#endif
}

bool AutoResolving::alreadyStartedSlow() const {
  MOZ_ASSERT(link);
  AutoResolving* cursor = link;
  do {
    MOZ_ASSERT(this != cursor);
    if (object.get() == cursor->object && id.get() == cursor->id &&
        kind == cursor->kind) {
      return true;
    }
  } while (!!(cursor = cursor->link));
  return false;
}

static void ReportError(JSContext* cx, JSErrorReport* reportp,
                        JSErrorCallback callback, void* userRef) {
  /*
   * Check the error report, and set a JavaScript-catchable exception
   * if the error is defined to have an associated exception.  If an
   * exception is thrown, then the JSREPORT_EXCEPTION flag will be set
   * on the error report, and exception-aware hosts should ignore it.
   */
  MOZ_ASSERT(reportp);
  if ((!callback || callback == GetErrorMessage) &&
      reportp->errorNumber == JSMSG_UNCAUGHT_EXCEPTION) {
    reportp->flags |= JSREPORT_EXCEPTION;
  }

  if (JSREPORT_IS_WARNING(reportp->flags)) {
    CallWarningReporter(cx, reportp);
    return;
  }

  ErrorToException(cx, reportp, callback, userRef);
}

/*
 * The given JSErrorReport object have been zeroed and must not outlive
 * cx->fp() (otherwise owned fields may become invalid).
 */
static void PopulateReportBlame(JSContext* cx, JSErrorReport* report) {
  JS::Realm* realm = cx->realm();
  if (!realm) {
    return;
  }

  /*
   * Walk stack until we find a frame that is associated with a non-builtin
   * rather than a builtin frame and which we're allowed to know about.
   */
  NonBuiltinFrameIter iter(cx, realm->principals());
  if (iter.done()) {
    return;
  }

  report->filename = iter.filename();
  if (iter.hasScript()) {
    report->sourceId = iter.script()->scriptSource()->id();
  }
  uint32_t column;
  report->lineno = iter.computeLine(&column);
  report->column = FixupColumnForDisplay(column);
  report->isMuted = iter.mutedErrors();
}

/*
 * Since memory has been exhausted, avoid the normal error-handling path which
 * allocates an error object, report and callstack. If code is running, simply
 * throw the static atom "out of memory". If code is not running, call the
 * error reporter directly.
 *
 * Furthermore, callers of ReportOutOfMemory (viz., malloc) assume a GC does
 * not occur, so GC must be avoided or suppressed.
 */
JS_FRIEND_API void js::ReportOutOfMemory(JSContext* cx) {
#ifdef JS_MORE_DETERMINISTIC
  /*
   * OOMs are non-deterministic, especially across different execution modes
   * (e.g. interpreter vs JIT). In more-deterministic builds, print to stderr
   * so that the fuzzers can detect this.
   */
  fprintf(stderr, "ReportOutOfMemory called\n");
#endif
  mozilla::recordreplay::InvalidateRecording("OutOfMemory exception thrown");

  if (cx->isHelperThreadContext()) {
    return cx->addPendingOutOfMemory();
  }

  cx->runtime()->hadOutOfMemory = true;
  gc::AutoSuppressGC suppressGC(cx);

  /* Report the oom. */
  if (JS::OutOfMemoryCallback oomCallback = cx->runtime()->oomCallback) {
    oomCallback(cx, cx->runtime()->oomCallbackData);
  }

  RootedValue oomMessage(cx, StringValue(cx->names().outOfMemory));
  cx->setPendingException(oomMessage, nullptr);
}

mozilla::GenericErrorResult<OOM&> js::ReportOutOfMemoryResult(JSContext* cx) {
  ReportOutOfMemory(cx);
  return cx->alreadyReportedOOM();
}

void js::ReportOverRecursed(JSContext* maybecx, unsigned errorNumber) {
#ifdef JS_MORE_DETERMINISTIC
  /*
   * We cannot make stack depth deterministic across different
   * implementations (e.g. JIT vs. interpreter will differ in
   * their maximum stack depth).
   * However, we can detect externally when we hit the maximum
   * stack depth which is useful for external testing programs
   * like fuzzers.
   */
  fprintf(stderr, "ReportOverRecursed called\n");
#endif
  mozilla::recordreplay::InvalidateRecording("OverRecursed exception thrown");
  if (maybecx) {
    if (!maybecx->isHelperThreadContext()) {
      JS_ReportErrorNumberASCII(maybecx, GetErrorMessage, nullptr, errorNumber);
      maybecx->overRecursed_ = true;
    } else {
      maybecx->addPendingOverRecursed();
    }
  }
}

JS_FRIEND_API void js::ReportOverRecursed(JSContext* maybecx) {
  ReportOverRecursed(maybecx, JSMSG_OVER_RECURSED);
}

void js::ReportAllocationOverflow(JSContext* cx) {
  if (!cx) {
    return;
  }

  if (cx->isHelperThreadContext()) {
    return;
  }

  gc::AutoSuppressGC suppressGC(cx);
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_ALLOC_OVERFLOW);
}

/*
 * Given flags and the state of cx, decide whether we should report an
 * error, a warning, or just continue execution normally.  Return
 * true if we should continue normally, without reporting anything;
 * otherwise, adjust *flags as appropriate and return false.
 */
static bool checkReportFlags(JSContext* cx, unsigned* flags) {
  if (JSREPORT_IS_STRICT(*flags)) {
    /* Warning/error only when JSOPTION_STRICT is set. */
    if (!cx->realm()->behaviors().extraWarnings(cx)) {
      return true;
    }
  }

  /* Warnings become errors when JSOPTION_WERROR is set. */
  if (JSREPORT_IS_WARNING(*flags) && cx->options().werror()) {
    *flags &= ~JSREPORT_WARNING;
  }

  return false;
}

bool js::ReportErrorVA(JSContext* cx, unsigned flags, const char* format,
                       ErrorArgumentsType argumentsType, va_list ap) {
  JSErrorReport report;

  if (checkReportFlags(cx, &flags)) {
    return true;
  }

  UniqueChars message(JS_vsmprintf(format, ap));
  if (!message) {
    ReportOutOfMemory(cx);
    return false;
  }

  MOZ_ASSERT_IF(argumentsType == ArgumentsAreASCII,
                JS::StringIsASCII(message.get()));

  report.flags = flags;
  report.errorNumber = JSMSG_USER_DEFINED_ERROR;
  if (argumentsType == ArgumentsAreASCII || argumentsType == ArgumentsAreUTF8) {
    report.initOwnedMessage(message.release());
  } else {
    MOZ_ASSERT(argumentsType == ArgumentsAreLatin1);
    Latin1Chars latin1(message.get(), strlen(message.get()));
    UTF8CharsZ utf8(JS::CharsToNewUTF8CharsZ(cx, latin1));
    if (!utf8) {
      return false;
    }
    report.initOwnedMessage(reinterpret_cast<const char*>(utf8.get()));
  }
  PopulateReportBlame(cx, &report);

  bool warning = JSREPORT_IS_WARNING(report.flags);

  ReportError(cx, &report, nullptr, nullptr);
  return warning;
}

/* |callee| requires a usage string provided by JS_DefineFunctionsWithHelp. */
void js::ReportUsageErrorASCII(JSContext* cx, HandleObject callee,
                               const char* msg) {
  RootedValue usage(cx);
  if (!JS_GetProperty(cx, callee, "usage", &usage)) {
    return;
  }

  if (!usage.isString()) {
    JS_ReportErrorASCII(cx, "%s", msg);
  } else {
    RootedString usageStr(cx, usage.toString());
    UniqueChars str = JS_EncodeStringToUTF8(cx, usageStr);
    if (!str) {
      return;
    }
    JS_ReportErrorUTF8(cx, "%s. Usage: %s", msg, str.get());
  }
}

enum class PrintErrorKind { Error, Warning, StrictWarning, Note };

static void PrintErrorLine(FILE* file, const char* prefix,
                           JSErrorReport* report) {
  if (const char16_t* linebuf = report->linebuf()) {
    size_t n = report->linebufLength();

    fputs(":\n", file);
    if (prefix) {
      fputs(prefix, file);
    }

    for (size_t i = 0; i < n; i++) {
      fputc(static_cast<char>(linebuf[i]), file);
    }

    // linebuf usually ends with a newline. If not, add one here.
    if (n == 0 || linebuf[n - 1] != '\n') {
      fputc('\n', file);
    }

    if (prefix) {
      fputs(prefix, file);
    }

    n = report->tokenOffset();
    for (size_t i = 0, j = 0; i < n; i++) {
      if (linebuf[i] == '\t') {
        for (size_t k = (j + 8) & ~7; j < k; j++) {
          fputc('.', file);
        }
        continue;
      }
      fputc('.', file);
      j++;
    }
    fputc('^', file);
  }
}

static void PrintErrorLine(FILE* file, const char* prefix,
                           JSErrorNotes::Note* note) {}

template <typename T>
static bool PrintSingleError(JSContext* cx, FILE* file,
                             JS::ConstUTF8CharsZ toStringResult, T* report,
                             PrintErrorKind kind) {
  UniqueChars prefix;
  if (report->filename) {
    prefix = JS_smprintf("%s:", report->filename);
  }

  if (report->lineno) {
    prefix = JS_smprintf("%s%u:%u ", prefix ? prefix.get() : "", report->lineno,
                         report->column);
  }

  if (kind != PrintErrorKind::Error) {
    const char* kindPrefix = nullptr;
    switch (kind) {
      case PrintErrorKind::Error:
        MOZ_CRASH("unreachable");
      case PrintErrorKind::Warning:
        kindPrefix = "warning";
        break;
      case PrintErrorKind::StrictWarning:
        kindPrefix = "strict warning";
        break;
      case PrintErrorKind::Note:
        kindPrefix = "note";
        break;
    }

    prefix = JS_smprintf("%s%s: ", prefix ? prefix.get() : "", kindPrefix);
  }

  const char* message =
      toStringResult ? toStringResult.c_str() : report->message().c_str();

  /* embedded newlines -- argh! */
  const char* ctmp;
  while ((ctmp = strchr(message, '\n')) != 0) {
    ctmp++;
    if (prefix) {
      fputs(prefix.get(), file);
    }
    mozilla::Unused << fwrite(message, 1, ctmp - message, file);
    message = ctmp;
  }

  /* If there were no filename or lineno, the prefix might be empty */
  if (prefix) {
    fputs(prefix.get(), file);
  }
  fputs(message, file);

  PrintErrorLine(file, prefix.get(), report);
  fputc('\n', file);

  fflush(file);
  return true;
}

bool js::PrintError(JSContext* cx, FILE* file,
                    JS::ConstUTF8CharsZ toStringResult, JSErrorReport* report,
                    bool reportWarnings) {
  MOZ_ASSERT(report);

  /* Conditionally ignore reported warnings. */
  if (JSREPORT_IS_WARNING(report->flags) && !reportWarnings) {
    return false;
  }

  PrintErrorKind kind = PrintErrorKind::Error;
  if (JSREPORT_IS_WARNING(report->flags)) {
    if (JSREPORT_IS_STRICT(report->flags)) {
      kind = PrintErrorKind::StrictWarning;
    } else {
      kind = PrintErrorKind::Warning;
    }
  }
  PrintSingleError(cx, file, toStringResult, report, kind);

  if (report->notes) {
    for (auto&& note : *report->notes) {
      PrintSingleError(cx, file, JS::ConstUTF8CharsZ(), note.get(),
                       PrintErrorKind::Note);
    }
  }

  return true;
}

class MOZ_RAII AutoMessageArgs {
  size_t totalLength_;
  /* only {0} thru {9} supported */
  mozilla::Array<const char*, JS::MaxNumErrorArguments> args_;
  mozilla::Array<size_t, JS::MaxNumErrorArguments> lengths_;
  uint16_t count_;
  bool allocatedElements_ : 1;

 public:
  AutoMessageArgs() : totalLength_(0), count_(0), allocatedElements_(false) {
    PodArrayZero(args_);
  }

  ~AutoMessageArgs() {
    /* free the arguments only if we allocated them */
    if (allocatedElements_) {
      uint16_t i = 0;
      while (i < count_) {
        if (args_[i]) {
          js_free((void*)args_[i]);
        }
        i++;
      }
    }
  }

  const char* args(size_t i) const {
    MOZ_ASSERT(i < count_);
    return args_[i];
  }

  size_t totalLength() const { return totalLength_; }

  size_t lengths(size_t i) const {
    MOZ_ASSERT(i < count_);
    return lengths_[i];
  }

  uint16_t count() const { return count_; }

  /* Gather the arguments into an array, and accumulate their sizes. */
  bool init(JSContext* cx, const char16_t** argsArg, uint16_t countArg,
            ErrorArgumentsType typeArg, va_list ap) {
    MOZ_ASSERT(countArg > 0);

    count_ = countArg;

    for (uint16_t i = 0; i < count_; i++) {
      switch (typeArg) {
        case ArgumentsAreASCII:
        case ArgumentsAreUTF8: {
          MOZ_ASSERT(!argsArg);
          args_[i] = va_arg(ap, char*);
          MOZ_ASSERT_IF(typeArg == ArgumentsAreASCII,
                        JS::StringIsASCII(args_[i]));
          lengths_[i] = strlen(args_[i]);
          break;
        }
        case ArgumentsAreLatin1: {
          MOZ_ASSERT(!argsArg);
          const Latin1Char* latin1 = va_arg(ap, Latin1Char*);
          size_t len = strlen(reinterpret_cast<const char*>(latin1));
          mozilla::Range<const Latin1Char> range(latin1, len);
          char* utf8 = JS::CharsToNewUTF8CharsZ(cx, range).c_str();
          if (!utf8) {
            return false;
          }

          args_[i] = utf8;
          lengths_[i] = strlen(utf8);
          allocatedElements_ = true;
          break;
        }
        case ArgumentsAreUnicode: {
          const char16_t* uc = argsArg ? argsArg[i] : va_arg(ap, char16_t*);
          size_t len = js_strlen(uc);
          mozilla::Range<const char16_t> range(uc, len);
          char* utf8 = JS::CharsToNewUTF8CharsZ(cx, range).c_str();
          if (!utf8) {
            return false;
          }

          args_[i] = utf8;
          lengths_[i] = strlen(utf8);
          allocatedElements_ = true;
          break;
        }
      }
      totalLength_ += lengths_[i];
    }
    return true;
  }
};

static void SetExnType(JSErrorReport* reportp, int16_t exnType) {
  reportp->exnType = exnType;
}

static void SetExnType(JSErrorNotes::Note* notep, int16_t exnType) {
  // Do nothing for JSErrorNotes::Note.
}

/*
 * The arguments from ap need to be packaged up into an array and stored
 * into the report struct.
 *
 * The format string addressed by the error number may contain operands
 * identified by the format {N}, where N is a decimal digit. Each of these
 * is to be replaced by the Nth argument from the va_list. The complete
 * message is placed into reportp->message_.
 *
 * Returns true if the expansion succeeds (can fail if out of memory).
 */
template <typename T>
bool ExpandErrorArgumentsHelper(JSContext* cx, JSErrorCallback callback,
                                void* userRef, const unsigned errorNumber,
                                const char16_t** messageArgs,
                                ErrorArgumentsType argumentsType, T* reportp,
                                va_list ap) {
  const JSErrorFormatString* efs;

  if (!callback) {
    callback = GetErrorMessage;
  }

  {
    gc::AutoSuppressGC suppressGC(cx);
    efs = callback(userRef, errorNumber);
  }

  if (efs) {
    SetExnType(reportp, efs->exnType);

    MOZ_ASSERT_IF(argumentsType == ArgumentsAreASCII,
                  JS::StringIsASCII(efs->format));

    uint16_t argCount = efs->argCount;
    MOZ_RELEASE_ASSERT(argCount <= JS::MaxNumErrorArguments);
    if (argCount > 0) {
      /*
       * Parse the error format, substituting the argument X
       * for {X} in the format.
       */
      if (efs->format) {
        const char* fmt;
        char* out;
#ifdef DEBUG
        int expandedArgs = 0;
#endif
        size_t expandedLength;
        size_t len = strlen(efs->format);

        AutoMessageArgs args;
        if (!args.init(cx, messageArgs, argCount, argumentsType, ap)) {
          return false;
        }

        expandedLength = len - (3 * args.count()) /* exclude the {n} */
                         + args.totalLength();

        /*
         * Note - the above calculation assumes that each argument
         * is used once and only once in the expansion !!!
         */
        char* utf8 = out = cx->pod_malloc<char>(expandedLength + 1);
        if (!out) {
          return false;
        }

        fmt = efs->format;
        while (*fmt) {
          if (*fmt == '{') {
            if (mozilla::IsAsciiDigit(fmt[1])) {
              int d = AsciiDigitToNumber(fmt[1]);
              MOZ_RELEASE_ASSERT(d < args.count());
              strncpy(out, args.args(d), args.lengths(d));
              out += args.lengths(d);
              fmt += 3;
#ifdef DEBUG
              expandedArgs++;
#endif
              continue;
            }
          }
          *out++ = *fmt++;
        }
        MOZ_ASSERT(expandedArgs == args.count());
        *out = 0;

        reportp->initOwnedMessage(utf8);
      }
    } else {
      /* Non-null messageArgs should have at least one non-null arg. */
      MOZ_ASSERT(!messageArgs);
      /*
       * Zero arguments: the format string (if it exists) is the
       * entire message.
       */
      if (efs->format) {
        reportp->initBorrowedMessage(efs->format);
      }
    }
  }
  if (!reportp->message()) {
    /* where's the right place for this ??? */
    const char* defaultErrorMessage =
        "No error message available for error number %d";
    size_t nbytes = strlen(defaultErrorMessage) + 16;
    char* message = cx->pod_malloc<char>(nbytes);
    if (!message) {
      return false;
    }
    snprintf(message, nbytes, defaultErrorMessage, errorNumber);
    reportp->initOwnedMessage(message);
  }
  return true;
}

bool js::ExpandErrorArgumentsVA(JSContext* cx, JSErrorCallback callback,
                                void* userRef, const unsigned errorNumber,
                                const char16_t** messageArgs,
                                ErrorArgumentsType argumentsType,
                                JSErrorReport* reportp, va_list ap) {
  return ExpandErrorArgumentsHelper(cx, callback, userRef, errorNumber,
                                    messageArgs, argumentsType, reportp, ap);
}

bool js::ExpandErrorArgumentsVA(JSContext* cx, JSErrorCallback callback,
                                void* userRef, const unsigned errorNumber,
                                const char16_t** messageArgs,
                                ErrorArgumentsType argumentsType,
                                JSErrorNotes::Note* notep, va_list ap) {
  return ExpandErrorArgumentsHelper(cx, callback, userRef, errorNumber,
                                    messageArgs, argumentsType, notep, ap);
}

bool js::ReportErrorNumberVA(JSContext* cx, unsigned flags,
                             JSErrorCallback callback, void* userRef,
                             const unsigned errorNumber,
                             ErrorArgumentsType argumentsType, va_list ap) {
  JSErrorReport report;
  bool warning;

  if (checkReportFlags(cx, &flags)) {
    return true;
  }
  warning = JSREPORT_IS_WARNING(flags);

  report.flags = flags;
  report.errorNumber = errorNumber;
  PopulateReportBlame(cx, &report);

  if (!ExpandErrorArgumentsVA(cx, callback, userRef, errorNumber, nullptr,
                              argumentsType, &report, ap)) {
    return false;
  }

  ReportError(cx, &report, callback, userRef);

  return warning;
}

static bool ExpandErrorArguments(JSContext* cx, JSErrorCallback callback,
                                 void* userRef, const unsigned errorNumber,
                                 const char16_t** messageArgs,
                                 ErrorArgumentsType argumentsType,
                                 JSErrorReport* reportp, ...) {
  va_list ap;
  va_start(ap, reportp);
  bool expanded =
      js::ExpandErrorArgumentsVA(cx, callback, userRef, errorNumber,
                                 messageArgs, argumentsType, reportp, ap);
  va_end(ap);
  return expanded;
}

bool js::ReportErrorNumberUCArray(JSContext* cx, unsigned flags,
                                  JSErrorCallback callback, void* userRef,
                                  const unsigned errorNumber,
                                  const char16_t** args) {
  if (checkReportFlags(cx, &flags)) {
    return true;
  }
  bool warning = JSREPORT_IS_WARNING(flags);

  JSErrorReport report;
  report.flags = flags;
  report.errorNumber = errorNumber;
  PopulateReportBlame(cx, &report);

  if (!ExpandErrorArguments(cx, callback, userRef, errorNumber, args,
                            ArgumentsAreUnicode, &report)) {
    return false;
  }

  ReportError(cx, &report, callback, userRef);

  return warning;
}

void js::ReportIsNotDefined(JSContext* cx, HandleId id) {
  if (UniqueChars printable =
          IdToPrintableUTF8(cx, id, IdToPrintableBehavior::IdIsIdentifier)) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_NOT_DEFINED,
                             printable.get());
  }
}

void js::ReportIsNotDefined(JSContext* cx, HandlePropertyName name) {
  RootedId id(cx, NameToId(name));
  ReportIsNotDefined(cx, id);
}

void js::ReportIsNullOrUndefined(JSContext* cx, int spindex, HandleValue v) {
  MOZ_ASSERT(v.isNullOrUndefined());

  UniqueChars bytes = DecompileValueGenerator(cx, spindex, v, nullptr);
  if (!bytes) {
    return;
  }

  if (strcmp(bytes.get(), js_undefined_str) == 0 ||
      strcmp(bytes.get(), js_null_str) == 0) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_NO_PROPERTIES,
                             bytes.get());
  } else if (v.isUndefined()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_UNEXPECTED_TYPE, bytes.get(),
                             js_undefined_str);
  } else {
    MOZ_ASSERT(v.isNull());
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_UNEXPECTED_TYPE, bytes.get(), js_null_str);
  }
}

void js::ReportMissingArg(JSContext* cx, HandleValue v, unsigned arg) {
  char argbuf[11];
  UniqueChars bytes;

  SprintfLiteral(argbuf, "%u", arg);
  if (IsFunctionObject(v)) {
    RootedAtom name(cx, v.toObject().as<JSFunction>().explicitName());
    bytes = DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, v, name);
    if (!bytes) {
      return;
    }
  }
  JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_MISSING_FUN_ARG,
                           argbuf, bytes ? bytes.get() : "");
}

bool js::ReportValueErrorFlags(JSContext* cx, unsigned flags,
                               const unsigned errorNumber, int spindex,
                               HandleValue v, HandleString fallback,
                               const char* arg1, const char* arg2) {
  MOZ_ASSERT(js_ErrorFormatString[errorNumber].argCount >= 1);
  MOZ_ASSERT(js_ErrorFormatString[errorNumber].argCount <= 3);
  UniqueChars bytes = DecompileValueGenerator(cx, spindex, v, fallback);
  if (!bytes) {
    return false;
  }

  return JS_ReportErrorFlagsAndNumberUTF8(cx, flags, GetErrorMessage, nullptr,
                                          errorNumber, bytes.get(), arg1, arg2);
}

JSObject* js::CreateErrorNotesArray(JSContext* cx, JSErrorReport* report) {
  RootedArrayObject notesArray(cx, NewDenseEmptyArray(cx));
  if (!notesArray) {
    return nullptr;
  }

  if (!report->notes) {
    return notesArray;
  }

  for (auto&& note : *report->notes) {
    RootedPlainObject noteObj(cx, NewBuiltinClassInstance<PlainObject>(cx));
    if (!noteObj) {
      return nullptr;
    }

    RootedString messageStr(cx, note->newMessageString(cx));
    if (!messageStr) {
      return nullptr;
    }
    RootedValue messageVal(cx, StringValue(messageStr));
    if (!DefineDataProperty(cx, noteObj, cx->names().message, messageVal)) {
      return nullptr;
    }

    RootedValue filenameVal(cx);
    if (note->filename) {
      RootedString filenameStr(cx, NewStringCopyZ<CanGC>(cx, note->filename));
      if (!filenameStr) {
        return nullptr;
      }
      filenameVal = StringValue(filenameStr);
    }
    if (!DefineDataProperty(cx, noteObj, cx->names().fileName, filenameVal)) {
      return nullptr;
    }

    RootedValue linenoVal(cx, Int32Value(note->lineno));
    if (!DefineDataProperty(cx, noteObj, cx->names().lineNumber, linenoVal)) {
      return nullptr;
    }
    RootedValue columnVal(cx, Int32Value(note->column));
    if (!DefineDataProperty(cx, noteObj, cx->names().columnNumber, columnVal)) {
      return nullptr;
    }

    if (!NewbornArrayPush(cx, notesArray, ObjectValue(*noteObj))) {
      return nullptr;
    }
  }

  return notesArray;
}

const JSErrorFormatString js_ErrorFormatString[JSErr_Limit] = {
#define MSG_DEF(name, count, exception, format) \
  {#name, format, count, exception},
#include "js.msg"
#undef MSG_DEF
};

JS_FRIEND_API const JSErrorFormatString* js::GetErrorMessage(
    void* userRef, const unsigned errorNumber) {
  if (errorNumber > 0 && errorNumber < JSErr_Limit) {
    return &js_ErrorFormatString[errorNumber];
  }
  return nullptr;
}

void JSContext::recoverFromOutOfMemory() {
  if (isHelperThreadContext()) {
    // Keep in sync with addPendingOutOfMemory.
    if (ParseTask* task = parseTask()) {
      task->outOfMemory = false;
    }
  } else {
    if (isExceptionPending()) {
      MOZ_ASSERT(isThrowingOutOfMemory());
      clearPendingException();
    }
  }
}

JS_FRIEND_API bool js::UseInternalJobQueues(JSContext* cx) {
  // Internal job queue handling must be set up very early. Self-hosting
  // initialization is as good a marker for that as any.
  MOZ_RELEASE_ASSERT(
      !cx->runtime()->hasInitializedSelfHosting(),
      "js::UseInternalJobQueues must be called early during runtime startup.");
  MOZ_ASSERT(!cx->jobQueue);
  auto queue = MakeUnique<InternalJobQueue>(cx);
  if (!queue) {
    return false;
  }

  cx->internalJobQueue = std::move(queue);
  cx->jobQueue = cx->internalJobQueue.ref().get();

  cx->runtime()->offThreadPromiseState.ref().initInternalDispatchQueue();
  MOZ_ASSERT(cx->runtime()->offThreadPromiseState.ref().initialized());

  return true;
}

JS_FRIEND_API bool js::EnqueueJob(JSContext* cx, JS::HandleObject job) {
  MOZ_ASSERT(cx->jobQueue);
  return cx->jobQueue->enqueuePromiseJob(cx, nullptr, job, nullptr, nullptr);
}

JS_FRIEND_API void js::StopDrainingJobQueue(JSContext* cx) {
  MOZ_ASSERT(cx->internalJobQueue.ref());
  cx->internalJobQueue->interrupt();
}

JS_FRIEND_API void js::RunJobs(JSContext* cx) {
  MOZ_ASSERT(cx->jobQueue);
  cx->jobQueue->runJobs(cx);
}

JSObject* InternalJobQueue::getIncumbentGlobal(JSContext* cx) {
  if (!cx->compartment()) {
    return nullptr;
  }
  return cx->global();
}

bool InternalJobQueue::enqueuePromiseJob(JSContext* cx,
                                         JS::HandleObject promise,
                                         JS::HandleObject job,
                                         JS::HandleObject allocationSite,
                                         JS::HandleObject incumbentGlobal) {
  MOZ_ASSERT(job);
  if (!queue.pushBack(job)) {
    ReportOutOfMemory(cx);
    return false;
  }

  JS::JobQueueMayNotBeEmpty(cx);
  return true;
}

void InternalJobQueue::runJobs(JSContext* cx) {
  if (draining_ || interrupted_) {
    return;
  }

  while (true) {
    cx->runtime()->offThreadPromiseState.ref().internalDrain(cx);

    // It doesn't make sense for job queue draining to be reentrant. At the
    // same time we don't want to assert against it, because that'd make
    // drainJobQueue unsafe for fuzzers. We do want fuzzers to test this,
    // so we simply ignore nested calls of drainJobQueue.
    draining_ = true;

    RootedObject job(cx);
    JS::HandleValueArray args(JS::HandleValueArray::empty());
    RootedValue rval(cx);

    // Execute jobs in a loop until we've reached the end of the queue.
    while (!queue.empty()) {
      // A previous job might have set this flag. E.g., the js shell
      // sets it if the `quit` builtin function is called.
      if (interrupted_) {
        break;
      }

      job = queue.front();
      queue.popFront();

      // If the next job is the last job in the job queue, allow
      // skipping the standard job queuing behavior.
      if (queue.empty()) {
        JS::JobQueueIsEmpty(cx);
      }

      AutoRealm ar(cx, &job->as<JSFunction>());
      {
        if (!JS::Call(cx, UndefinedHandleValue, job, args, &rval)) {
          // Nothing we can do about uncatchable exceptions.
          if (!cx->isExceptionPending()) {
            continue;
          }
          RootedValue exn(cx);
          if (cx->getPendingException(&exn)) {
            /*
             * Clear the exception, because
             * PrepareScriptEnvironmentAndInvoke will assert that we don't
             * have one.
             */
            cx->clearPendingException();
            js::ReportExceptionClosure reportExn(exn);
            PrepareScriptEnvironmentAndInvoke(cx, cx->global(), reportExn);
          }
        }
      }
    }

    draining_ = false;

    if (interrupted_) {
      interrupted_ = false;
      break;
    }

    queue.clear();

    // It's possible a job added a new off-thread promise task.
    if (!cx->runtime()->offThreadPromiseState.ref().internalHasPending()) {
      break;
    }
  }
}

bool InternalJobQueue::empty() const { return queue.empty(); }

JSObject* InternalJobQueue::maybeFront() const {
  if (queue.empty()) {
    return nullptr;
  }

  return queue.get().front();
}

class js::InternalJobQueue::SavedQueue : public JobQueue::SavedJobQueue {
 public:
  SavedQueue(JSContext* cx, Queue&& saved, bool draining)
      : cx(cx), saved(cx, std::move(saved)), draining_(draining) {
    MOZ_ASSERT(cx->internalJobQueue.ref());
  }

  ~SavedQueue() {
    MOZ_ASSERT(cx->internalJobQueue.ref());
    cx->internalJobQueue->queue = std::move(saved.get());
    cx->internalJobQueue->draining_ = draining_;
  }

 private:
  JSContext* cx;
  PersistentRooted<Queue> saved;
  bool draining_;
};

js::UniquePtr<JS::JobQueue::SavedJobQueue> InternalJobQueue::saveJobQueue(
    JSContext* cx) {
  auto saved =
      js::MakeUnique<SavedQueue>(cx, std::move(queue.get()), draining_);
  if (!saved) {
    // When MakeUnique's allocation fails, the SavedQueue constructor is never
    // called, so this->queue is still initialized. (The move doesn't occur
    // until the constructor gets called.)
    ReportOutOfMemory(cx);
    return nullptr;
  }

  queue = Queue(SystemAllocPolicy());
  draining_ = false;
  return saved;
}

JS::Error JSContext::reportedError;
JS::OOM JSContext::reportedOOM;

mozilla::GenericErrorResult<OOM&> JSContext::alreadyReportedOOM() {
#ifdef DEBUG
  if (isHelperThreadContext()) {
    // Keep in sync with addPendingOutOfMemory.
    if (ParseTask* task = parseTask()) {
      MOZ_ASSERT(task->outOfMemory);
    }
  } else {
    MOZ_ASSERT(isThrowingOutOfMemory());
  }
#endif
  return mozilla::Err(reportedOOM);
}

mozilla::GenericErrorResult<JS::Error&> JSContext::alreadyReportedError() {
#ifdef DEBUG
  if (!isHelperThreadContext()) {
    MOZ_ASSERT(isExceptionPending());
  }
#endif
  return mozilla::Err(reportedError);
}

JSContext::JSContext(JSRuntime* runtime, const JS::ContextOptions& options)
    : runtime_(runtime),
      kind_(ContextKind::HelperThread),
      nurserySuppressions_(this),
      options_(this, options),
      freeLists_(this, nullptr),
      atomsZoneFreeLists_(this),
      defaultFreeOp_(this, runtime, true),
      freeUnusedMemory(false),
      jitActivation(this, nullptr),
      regexpStack(this),
      activation_(this, nullptr),
      profilingActivation_(nullptr),
      nativeStackBase(GetNativeStackBase()),
      entryMonitor(this, nullptr),
      noExecuteDebuggerTop(this, nullptr),
#ifdef DEBUG
      inUnsafeCallWithABI(this, false),
      hasAutoUnsafeCallWithABI(this, false),
#endif
#ifdef JS_SIMULATOR
      simulator_(this, nullptr),
#endif
#ifdef JS_TRACE_LOGGING
      traceLogger(nullptr),
#endif
      autoFlushICache_(this, nullptr),
      dtoaState(this, nullptr),
      suppressGC(this, 0),
      gcSweeping(this, false),
#ifdef DEBUG
      isTouchingGrayThings(this, false),
      noNurseryAllocationCheck(this, 0),
      disableStrictProxyCheckingCount(this, 0),
#endif
#if defined(DEBUG) || defined(JS_OOM_BREAKPOINT)
      runningOOMTest(this, false),
#endif
      enableAccessValidation(this, false),
      inUnsafeRegion(this, 0),
      generationalDisabled(this, 0),
      compactingDisabledCount(this, 0),
      frontendCollectionPool_(this),
      suppressProfilerSampling(false),
      wasmTriedToInstallSignalHandlers(false),
      wasmHaveSignalHandlers(false),
      tempLifoAlloc_(this, (size_t)TEMP_LIFO_ALLOC_PRIMARY_CHUNK_SIZE),
      debuggerMutations(this, 0),
      ionPcScriptCache(this, nullptr),
      throwing(this, false),
      unwrappedException_(this),
      unwrappedExceptionStack_(this),
      overRecursed_(this, false),
      propagatingForcedReturn_(this, false),
      reportGranularity(this, JS_DEFAULT_JITREPORT_GRANULARITY),
      resolvingList(this, nullptr),
#ifdef DEBUG
      enteredPolicy(this, nullptr),
#endif
      generatingError(this, false),
      cycleDetectorVector_(this, this),
      data(nullptr),
      asyncStackForNewActivations_(this),
      asyncCauseForNewActivations(this, nullptr),
      asyncCallIsExplicit(this, false),
      interruptCallbacks_(this),
      interruptCallbackDisabled(this, false),
      interruptBits_(0),
      osrTempData_(this, nullptr),
      ionReturnOverride_(this, MagicValue(JS_ARG_POISON)),
      jitStackLimit(UINTPTR_MAX),
      jitStackLimitNoInterrupt(this, UINTPTR_MAX),
      jobQueue(this, nullptr),
      internalJobQueue(this),
      canSkipEnqueuingJobs(this, false),
      promiseRejectionTrackerCallback(this, nullptr),
      promiseRejectionTrackerCallbackData(this, nullptr)
#ifdef JS_STRUCTURED_SPEW
      ,
      structuredSpewer_()
#endif
{
  MOZ_ASSERT(static_cast<JS::RootingContext*>(this) ==
             JS::RootingContext::get(this));
}

JSContext::~JSContext() {
  // Clear the ContextKind first, so that ProtectedData checks will allow us to
  // destroy this context even if the runtime is already gone.
  kind_ = ContextKind::HelperThread;

  /* Free the stuff hanging off of cx. */
  MOZ_ASSERT(!resolvingList);

  if (dtoaState) {
    DestroyDtoaState(dtoaState);
  }

  fx.destroyInstance();
  freeOsrTempData();

#ifdef JS_SIMULATOR
  js::jit::Simulator::Destroy(simulator_);
#endif

#ifdef JS_TRACE_LOGGING
  if (traceLogger) {
    DestroyTraceLogger(traceLogger);
  }
#endif

  js_delete(atomsZoneFreeLists_.ref());

  TlsContext.set(nullptr);
}

void JSContext::setHelperThread(AutoLockHelperThreadState& locked) {
  MOZ_ASSERT_IF(!JSRuntime::hasLiveRuntimes(), !TlsContext.get());
  TlsContext.set(this);
  currentThread_ = ThisThread::GetId();
}

void JSContext::clearHelperThread(AutoLockHelperThreadState& locked) {
  currentThread_ = Thread::Id();
  TlsContext.set(nullptr);
}

void JSContext::setRuntime(JSRuntime* rt) {
  MOZ_ASSERT(!resolvingList);
  MOZ_ASSERT(!compartment());
  MOZ_ASSERT(!activation());
  MOZ_ASSERT(!unwrappedException_.ref().initialized());
  MOZ_ASSERT(!unwrappedExceptionStack_.ref().initialized());
  MOZ_ASSERT(!asyncStackForNewActivations_.ref().initialized());

  runtime_ = rt;
}

static const size_t MAX_REPORTED_STACK_DEPTH = 1u << 7;

void JSContext::setPendingExceptionAndCaptureStack(HandleValue value) {
  RootedObject stack(this);
  if (!CaptureCurrentStack(
          this, &stack,
          JS::StackCapture(JS::MaxFrames(MAX_REPORTED_STACK_DEPTH)))) {
    clearPendingException();
  }

  RootedSavedFrame nstack(this);
  if (stack) {
    nstack = &stack->as<SavedFrame>();
  }
  setPendingException(value, nstack);
}

bool JSContext::getPendingException(MutableHandleValue rval) {
  MOZ_ASSERT(throwing);
  rval.set(unwrappedException());
  if (zone()->isAtomsZone()) {
    return true;
  }
  RootedSavedFrame stack(this, unwrappedExceptionStack());
  bool wasOverRecursed = overRecursed_;
  clearPendingException();
  if (!compartment()->wrap(this, rval)) {
    return false;
  }
  this->check(rval);
  setPendingException(rval, stack);
  overRecursed_ = wasOverRecursed;
  return true;
}

SavedFrame* JSContext::getPendingExceptionStack() {
  return unwrappedExceptionStack();
}

bool JSContext::isThrowingOutOfMemory() {
  return throwing && unwrappedException() == StringValue(names().outOfMemory);
}

bool JSContext::isClosingGenerator() {
  return throwing && unwrappedException().isMagic(JS_GENERATOR_CLOSING);
}

bool JSContext::isThrowingDebuggeeWouldRun() {
  return throwing && unwrappedException().isObject() &&
         unwrappedException().toObject().is<ErrorObject>() &&
         unwrappedException().toObject().as<ErrorObject>().type() ==
             JSEXN_DEBUGGEEWOULDRUN;
}

size_t JSContext::sizeOfExcludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
  /*
   * There are other JSContext members that could be measured; the following
   * ones have been found by DMD to be worth measuring.  More stuff may be
   * added later.
   */
  return cycleDetectorVector().sizeOfExcludingThis(mallocSizeOf);
}

#ifdef DEBUG
bool JSContext::inAtomsZone() const { return zone_->isAtomsZone(); }
#endif

void JSContext::trace(JSTracer* trc) {
  cycleDetectorVector().trace(trc);
  geckoProfiler().trace(trc);
}

void* JSContext::stackLimitAddressForJitCode(JS::StackKind kind) {
#ifdef JS_SIMULATOR
  return addressOfSimulatorStackLimit();
#else
  return stackLimitAddress(kind);
#endif
}

uintptr_t JSContext::stackLimitForJitCode(JS::StackKind kind) {
#ifdef JS_SIMULATOR
  return simulator()->stackLimit();
#else
  return stackLimit(kind);
#endif
}

void JSContext::resetJitStackLimit() {
  // Note that, for now, we use the untrusted limit for ion. This is fine,
  // because it's the most conservative limit, and if we hit it, we'll bail
  // out of ion into the interpreter, which will do a proper recursion check.
#ifdef JS_SIMULATOR
  jitStackLimit = jit::Simulator::StackLimit();
#else
  jitStackLimit = nativeStackLimit[JS::StackForUntrustedScript];
#endif
  jitStackLimitNoInterrupt = jitStackLimit;
}

void JSContext::initJitStackLimit() { resetJitStackLimit(); }

#ifdef JS_CRASH_DIAGNOSTICS
void ContextChecks::check(AbstractFramePtr frame, int argIndex) {
  if (frame) {
    check(frame.realm(), argIndex);
  }
}
#endif

void AutoEnterOOMUnsafeRegion::crash(const char* reason) {
  char msgbuf[1024];
  js::NoteIntentionalCrash();
  SprintfLiteral(msgbuf, "[unhandlable oom] %s", reason);
  MOZ_ReportAssertionFailure(msgbuf, __FILE__, __LINE__);
  MOZ_CRASH();
}

AutoEnterOOMUnsafeRegion::AnnotateOOMAllocationSizeCallback
    AutoEnterOOMUnsafeRegion::annotateOOMSizeCallback = nullptr;

void AutoEnterOOMUnsafeRegion::crash(size_t size, const char* reason) {
  {
    JS::AutoSuppressGCAnalysis suppress;
    if (annotateOOMSizeCallback) {
      annotateOOMSizeCallback(size);
    }
  }
  crash(reason);
}

#ifdef DEBUG
AutoUnsafeCallWithABI::AutoUnsafeCallWithABI(UnsafeABIStrictness strictness)
    : cx_(TlsContext.get()), nested_(cx_->hasAutoUnsafeCallWithABI), nogc(cx_) {
  switch (strictness) {
    case UnsafeABIStrictness::NoExceptions:
      MOZ_ASSERT(!JS_IsExceptionPending(cx_));
      checkForPendingException_ = true;
      break;
    case UnsafeABIStrictness::AllowPendingExceptions:
      checkForPendingException_ = !JS_IsExceptionPending(cx_);
      break;
    case UnsafeABIStrictness::AllowThrownExceptions:
      checkForPendingException_ = false;
      break;
  }

  cx_->hasAutoUnsafeCallWithABI = true;
}

AutoUnsafeCallWithABI::~AutoUnsafeCallWithABI() {
  MOZ_ASSERT(cx_->hasAutoUnsafeCallWithABI);
  if (!nested_) {
    cx_->hasAutoUnsafeCallWithABI = false;
    cx_->inUnsafeCallWithABI = false;
  }
  MOZ_ASSERT_IF(checkForPendingException_, !JS_IsExceptionPending(cx_));
}
#endif
