/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS execution context.
 */

#include "jscntxtinlines.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Sprintf.h"

#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#ifdef ANDROID
# include <android/log.h>
# include <fstream>
# include <string>
#endif  // ANDROID

#include "jsatom.h"
#include "jscompartment.h"
#include "jsexn.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsiter.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsprf.h"
#include "jspubtd.h"
#include "jsscript.h"
#include "jsstr.h"
#include "jstypes.h"
#include "jswatchpoint.h"

#include "gc/Marking.h"
#include "jit/Ion.h"
#include "js/CharacterEncoding.h"
#include "vm/HelperThreads.h"
#include "vm/Shape.h"

#include "jsobjinlines.h"
#include "jsscriptinlines.h"

#include "vm/Stack-inl.h"

using namespace js;
using namespace js::gc;

using mozilla::DebugOnly;
using mozilla::PodArrayZero;
using mozilla::PointerRangeSize;

bool
js::AutoCycleDetector::init()
{
    AutoCycleDetector::Set& set = cx->cycleDetectorSet;
    hashsetAddPointer = set.lookupForAdd(obj);
    if (!hashsetAddPointer) {
        if (!set.add(hashsetAddPointer, obj)) {
            ReportOutOfMemory(cx);
            return false;
        }
        cyclic = false;
        hashsetGenerationAtInit = set.generation();
    }
    return true;
}

js::AutoCycleDetector::~AutoCycleDetector()
{
    if (!cyclic) {
        if (hashsetGenerationAtInit == cx->cycleDetectorSet.generation())
            cx->cycleDetectorSet.remove(hashsetAddPointer);
        else
            cx->cycleDetectorSet.remove(obj);
    }
}

void
js::TraceCycleDetectionSet(JSTracer* trc, AutoCycleDetector::Set& set)
{
    for (AutoCycleDetector::Set::Enum e(set); !e.empty(); e.popFront())
        TraceRoot(trc, &e.mutableFront(), "cycle detector table entry");
}

bool
JSContext::init(uint32_t maxBytes, uint32_t maxNurseryBytes)
{
    if (!JSRuntime::init(maxBytes, maxNurseryBytes))
        return false;

    if (!caches.init())
        return false;

    return true;
}

JSContext*
js::NewContext(uint32_t maxBytes, uint32_t maxNurseryBytes, JSRuntime* parentRuntime)
{
    JSContext* cx = js_new<JSContext>(parentRuntime);
    if (!cx)
        return nullptr;

    if (!cx->init(maxBytes, maxNurseryBytes)) {
        js_delete(cx);
        return nullptr;
    }

    return cx;
}

void
js::DestroyContext(JSContext* cx)
{
    JS_AbortIfWrongThread(cx);

    if (cx->outstandingRequests != 0)
        MOZ_CRASH("Attempted to destroy a context while it is in a request.");

    cx->roots.checkNoGCRooters();

    /*
     * Dump remaining type inference results while we still have a context.
     * This printing depends on atoms still existing.
     */
    for (CompartmentsIter c(cx, SkipAtoms); !c.done(); c.next())
        PrintTypes(cx, c, false);

    js_delete_poison(cx);
}

void
RootLists::checkNoGCRooters() {
#ifdef DEBUG
    for (auto const& stackRootPtr : stackRoots_)
        MOZ_ASSERT(stackRootPtr == nullptr);
#endif
}

bool
AutoResolving::alreadyStartedSlow() const
{
    MOZ_ASSERT(link);
    AutoResolving* cursor = link;
    do {
        MOZ_ASSERT(this != cursor);
        if (object.get() == cursor->object && id.get() == cursor->id && kind == cursor->kind)
            return true;
    } while (!!(cursor = cursor->link));
    return false;
}

static void
ReportError(JSContext* cx, JSErrorReport* reportp, JSErrorCallback callback,
            void* userRef)
{
    /*
     * Check the error report, and set a JavaScript-catchable exception
     * if the error is defined to have an associated exception.  If an
     * exception is thrown, then the JSREPORT_EXCEPTION flag will be set
     * on the error report, and exception-aware hosts should ignore it.
     */
    MOZ_ASSERT(reportp);
    if ((!callback || callback == GetErrorMessage) &&
        reportp->errorNumber == JSMSG_UNCAUGHT_EXCEPTION)
    {
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
static void
PopulateReportBlame(JSContext* cx, JSErrorReport* report)
{
    JSCompartment* compartment = cx->compartment();
    if (!compartment)
        return;

    /*
     * Walk stack until we find a frame that is associated with a non-builtin
     * rather than a builtin frame and which we're allowed to know about.
     */
    NonBuiltinFrameIter iter(cx, compartment->principals());
    if (iter.done())
        return;

    report->filename = iter.filename();
    report->lineno = iter.computeLine(&report->column);
    // XXX: Make the column 1-based as in other browsers, instead of 0-based
    // which is how SpiderMonkey stores it internally. This will be
    // unnecessary once bug 1144340 is fixed.
    report->column++;
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
void
js::ReportOutOfMemory(ExclusiveContext* cxArg)
{
#ifdef JS_MORE_DETERMINISTIC
    /*
     * OOMs are non-deterministic, especially across different execution modes
     * (e.g. interpreter vs JIT). In more-deterministic builds, print to stderr
     * so that the fuzzers can detect this.
     */
    fprintf(stderr, "ReportOutOfMemory called\n");
#endif

    if (!cxArg->isJSContext())
        return cxArg->addPendingOutOfMemory();

    JSContext* cx = cxArg->asJSContext();
    cx->runtime()->hadOutOfMemory = true;
    AutoSuppressGC suppressGC(cx);

    /* Report the oom. */
    if (JS::OutOfMemoryCallback oomCallback = cx->runtime()->oomCallback)
        oomCallback(cx, cx->runtime()->oomCallbackData);

    cx->setPendingException(StringValue(cx->names().outOfMemory));
}

mozilla::GenericErrorResult<OOM&>
js::ReportOutOfMemoryResult(ExclusiveContext* cx)
{
    ReportOutOfMemory(cx);
    return cx->alreadyReportedOOM();
}

void
js::ReportOverRecursed(JSContext* maybecx, unsigned errorNumber)
{
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
    if (maybecx) {
        JS_ReportErrorNumberASCII(maybecx, GetErrorMessage, nullptr, errorNumber);
        maybecx->overRecursed_ = true;
    }
}

JS_FRIEND_API(void)
js::ReportOverRecursed(JSContext* maybecx)
{
    ReportOverRecursed(maybecx, JSMSG_OVER_RECURSED);
}

void
js::ReportOverRecursed(ExclusiveContext* cx)
{
    if (cx->isJSContext())
        ReportOverRecursed(cx->asJSContext());
    else
        cx->addPendingOverRecursed();
}

void
js::ReportAllocationOverflow(ExclusiveContext* cxArg)
{
    if (!cxArg)
        return;

    if (!cxArg->isJSContext())
        return;
    JSContext* cx = cxArg->asJSContext();

    AutoSuppressGC suppressGC(cx);
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_ALLOC_OVERFLOW);
}

/*
 * Given flags and the state of cx, decide whether we should report an
 * error, a warning, or just continue execution normally.  Return
 * true if we should continue normally, without reporting anything;
 * otherwise, adjust *flags as appropriate and return false.
 */
static bool
checkReportFlags(JSContext* cx, unsigned* flags)
{
    if (JSREPORT_IS_STRICT(*flags)) {
        /* Warning/error only when JSOPTION_STRICT is set. */
        if (!cx->compartment()->behaviors().extraWarnings(cx))
            return true;
    }

    /* Warnings become errors when JSOPTION_WERROR is set. */
    if (JSREPORT_IS_WARNING(*flags) && cx->options().werror())
        *flags &= ~JSREPORT_WARNING;

    return false;
}

bool
js::ReportErrorVA(JSContext* cx, unsigned flags, const char* format,
                  ErrorArgumentsType argumentsType, va_list ap)
{
    JSErrorReport report;

    if (checkReportFlags(cx, &flags))
        return true;

    UniqueChars message(JS_vsmprintf(format, ap));
    if (!message) {
        ReportOutOfMemory(cx);
        return false;
    }

    MOZ_ASSERT_IF(argumentsType == ArgumentsAreASCII, JS::StringIsASCII(message.get()));

    report.flags = flags;
    report.errorNumber = JSMSG_USER_DEFINED_ERROR;
    if (argumentsType == ArgumentsAreASCII || argumentsType == ArgumentsAreUTF8) {
        report.initOwnedMessage(message.release());
    } else {
        MOZ_ASSERT(argumentsType == ArgumentsAreLatin1);
        Latin1Chars latin1(message.get(), strlen(message.get()));
        UTF8CharsZ utf8(JS::CharsToNewUTF8CharsZ(cx, latin1));
        if (!utf8)
            return false;
        report.initOwnedMessage(reinterpret_cast<const char*>(utf8.get()));
    }
    PopulateReportBlame(cx, &report);

    bool warning = JSREPORT_IS_WARNING(report.flags);

    ReportError(cx, &report, nullptr, nullptr);
    return warning;
}

/* |callee| requires a usage string provided by JS_DefineFunctionsWithHelp. */
void
js::ReportUsageErrorASCII(JSContext* cx, HandleObject callee, const char* msg)
{
    const char* usageStr = "usage";
    PropertyName* usageAtom = Atomize(cx, usageStr, strlen(usageStr))->asPropertyName();
    RootedId id(cx, NameToId(usageAtom));
    DebugOnly<Shape*> shape = static_cast<Shape*>(callee->as<JSFunction>().lookup(cx, id));
    MOZ_ASSERT(!shape->configurable());
    MOZ_ASSERT(!shape->writable());
    MOZ_ASSERT(shape->hasDefaultGetter());

    RootedValue usage(cx);
    if (!JS_GetProperty(cx, callee, "usage", &usage))
        return;

    if (!usage.isString()) {
        JS_ReportErrorASCII(cx, "%s", msg);
    } else {
        RootedString usageStr(cx, usage.toString());
        JSAutoByteString str;
        if (!str.encodeUtf8(cx, usageStr))
            return;
        JS_ReportErrorUTF8(cx, "%s. Usage: %s", msg, str.ptr());
    }
}

bool
js::PrintError(JSContext* cx, FILE* file, JS::ConstUTF8CharsZ toStringResult,
               JSErrorReport* report, bool reportWarnings)
{
    MOZ_ASSERT(report);

    /* Conditionally ignore reported warnings. */
    if (JSREPORT_IS_WARNING(report->flags) && !reportWarnings)
        return false;

    char* prefix = nullptr;
    if (report->filename)
        prefix = JS_smprintf("%s:", report->filename);
    if (report->lineno) {
        char* tmp = prefix;
        prefix = JS_smprintf("%s%u:%u ", tmp ? tmp : "", report->lineno, report->column);
        JS_free(cx, tmp);
    }
    if (JSREPORT_IS_WARNING(report->flags)) {
        char* tmp = prefix;
        prefix = JS_smprintf("%s%swarning: ",
                             tmp ? tmp : "",
                             JSREPORT_IS_STRICT(report->flags) ? "strict " : "");
        JS_free(cx, tmp);
    }

    const char* message = toStringResult ? toStringResult.c_str() : report->message().c_str();

    /* embedded newlines -- argh! */
    const char* ctmp;
    while ((ctmp = strchr(message, '\n')) != 0) {
        ctmp++;
        if (prefix)
            fputs(prefix, file);
        fwrite(message, 1, ctmp - message, file);
        message = ctmp;
    }

    /* If there were no filename or lineno, the prefix might be empty */
    if (prefix)
        fputs(prefix, file);
    fputs(message, file);

    if (const char16_t* linebuf = report->linebuf()) {
        size_t n = report->linebufLength();

        fputs(":\n", file);
        if (prefix)
            fputs(prefix, file);

        for (size_t i = 0; i < n; i++)
            fputc(static_cast<char>(linebuf[i]), file);

        // linebuf usually ends with a newline. If not, add one here.
        if (n == 0 || linebuf[n-1] != '\n')
            fputc('\n', file);

        if (prefix)
            fputs(prefix, file);

        n = report->tokenOffset();
        for (size_t i = 0, j = 0; i < n; i++) {
            if (linebuf[i] == '\t') {
                for (size_t k = (j + 8) & ~7; j < k; j++)
                    fputc('.', file);
                continue;
            }
            fputc('.', file);
            j++;
        }
        fputc('^', file);
    }
    fputc('\n', file);
    fflush(file);
    JS_free(cx, prefix);
    return true;
}

class MOZ_RAII AutoMessageArgs
{
    size_t totalLength_;
    /* only {0} thru {9} supported */
    mozilla::Array<const char*, JS::MaxNumErrorArguments> args_;
    mozilla::Array<size_t, JS::MaxNumErrorArguments> lengths_;
    uint16_t count_;
    bool allocatedElements_ : 1;

  public:
    AutoMessageArgs()
      : totalLength_(0), count_(0), allocatedElements_(false)
    {
        PodArrayZero(args_);
    }

    ~AutoMessageArgs()
    {
        /* free the arguments only if we allocated them */
        if (allocatedElements_) {
            uint16_t i = 0;
            while (i < count_) {
                if (args_[i])
                    js_free((void*)args_[i]);
                i++;
            }
        }
    }

    const char* args(size_t i) const {
        MOZ_ASSERT(i < count_);
        return args_[i];
    }

    size_t totalLength() const {
        return totalLength_;
    }

    size_t lengths(size_t i) const {
        MOZ_ASSERT(i < count_);
        return lengths_[i];
    }

    uint16_t count() const {
        return count_;
    }

    /* Gather the arguments into an array, and accumulate their sizes. */
    bool init(ExclusiveContext* cx, const char16_t** argsArg, uint16_t countArg,
              ErrorArgumentsType typeArg, va_list ap) {
        MOZ_ASSERT(countArg > 0);

        count_ = countArg;

        for (uint16_t i = 0; i < count_; i++) {
            switch (typeArg) {
              case ArgumentsAreASCII:
              case ArgumentsAreUTF8: {
                MOZ_ASSERT(!argsArg);
                args_[i] = va_arg(ap, char*);
                MOZ_ASSERT_IF(typeArg == ArgumentsAreASCII, JS::StringIsASCII(args_[i]));
                lengths_[i] = strlen(args_[i]);
                break;
              }
              case ArgumentsAreLatin1: {
                MOZ_ASSERT(!argsArg);
                const Latin1Char* latin1 = va_arg(ap, Latin1Char*);
                size_t len = strlen(reinterpret_cast<const char*>(latin1));
                mozilla::Range<const Latin1Char> range(latin1, len);
                char* utf8 = JS::CharsToNewUTF8CharsZ(cx, range).c_str();
                if (!utf8)
                    return false;

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
                if (!utf8)
                    return false;

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
bool
js::ExpandErrorArgumentsVA(ExclusiveContext* cx, JSErrorCallback callback,
                           void* userRef, const unsigned errorNumber,
                           const char16_t** messageArgs,
                           ErrorArgumentsType argumentsType,
                           JSErrorReport* reportp, va_list ap)
{
    const JSErrorFormatString* efs;

    if (!callback)
        callback = GetErrorMessage;

    {
        AutoSuppressGC suppressGC(cx);
        efs = callback(userRef, errorNumber);
    }

    if (efs) {
        reportp->exnType = efs->exnType;

        MOZ_ASSERT_IF(argumentsType == ArgumentsAreASCII, JS::StringIsASCII(efs->format));

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
                if (!args.init(cx, messageArgs, argCount, argumentsType, ap))
                    return false;

                expandedLength = len
                                 - (3 * args.count()) /* exclude the {n} */
                                 + args.totalLength();

                /*
                * Note - the above calculation assumes that each argument
                * is used once and only once in the expansion !!!
                */
                char* utf8 = out = cx->pod_malloc<char>(expandedLength + 1);
                if (!out)
                    return false;

                fmt = efs->format;
                while (*fmt) {
                    if (*fmt == '{') {
                        if (isdigit(fmt[1])) {
                            int d = JS7_UNDEC(fmt[1]);
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
            if (efs->format)
                reportp->initBorrowedMessage(efs->format);
        }
    }
    if (!reportp->message()) {
        /* where's the right place for this ??? */
        const char* defaultErrorMessage
            = "No error message available for error number %d";
        size_t nbytes = strlen(defaultErrorMessage) + 16;
        char* message = cx->pod_malloc<char>(nbytes);
        if (!message)
            return false;
        snprintf(message, nbytes, defaultErrorMessage, errorNumber);
        reportp->initOwnedMessage(message);
    }
    return true;
}

bool
js::ReportErrorNumberVA(JSContext* cx, unsigned flags, JSErrorCallback callback,
                        void* userRef, const unsigned errorNumber,
                        ErrorArgumentsType argumentsType, va_list ap)
{
    JSErrorReport report;
    bool warning;

    if (checkReportFlags(cx, &flags))
        return true;
    warning = JSREPORT_IS_WARNING(flags);

    report.flags = flags;
    report.errorNumber = errorNumber;
    PopulateReportBlame(cx, &report);

    if (!ExpandErrorArgumentsVA(cx, callback, userRef, errorNumber,
                                nullptr, argumentsType, &report, ap)) {
        return false;
    }

    ReportError(cx, &report, callback, userRef);

    return warning;
}

static bool
ExpandErrorArguments(ExclusiveContext* cx, JSErrorCallback callback,
                     void* userRef, const unsigned errorNumber,
                     const char16_t** messageArgs,
                     ErrorArgumentsType argumentsType,
                     JSErrorReport* reportp, ...)
{
    va_list ap;
    va_start(ap, reportp);
    bool expanded = js::ExpandErrorArgumentsVA(cx, callback, userRef, errorNumber,
                                               messageArgs, argumentsType, reportp, ap);
    va_end(ap);
    return expanded;
}

bool
js::ReportErrorNumberUCArray(JSContext* cx, unsigned flags, JSErrorCallback callback,
                             void* userRef, const unsigned errorNumber,
                             const char16_t** args)
{
    if (checkReportFlags(cx, &flags))
        return true;
    bool warning = JSREPORT_IS_WARNING(flags);

    JSErrorReport report;
    report.flags = flags;
    report.errorNumber = errorNumber;
    PopulateReportBlame(cx, &report);

    if (!ExpandErrorArguments(cx, callback, userRef, errorNumber,
                              args, ArgumentsAreUnicode, &report))
    {
        return false;
    }

    ReportError(cx, &report, callback, userRef);

    return warning;
}

void
js::CallWarningReporter(JSContext* cx, JSErrorReport* reportp)
{
    MOZ_ASSERT(reportp);
    MOZ_ASSERT(JSREPORT_IS_WARNING(reportp->flags));

    if (JS::WarningReporter warningReporter = cx->runtime()->warningReporter)
        warningReporter(cx, reportp);
}

bool
js::ReportIsNotDefined(JSContext* cx, HandleId id)
{
    JSAutoByteString printable;
    if (ValueToPrintable(cx, IdToValue(id), &printable)) {
        JS_ReportErrorNumberLatin1(cx, GetErrorMessage, nullptr, JSMSG_NOT_DEFINED,
                                   printable.ptr());
    }
    return false;
}

bool
js::ReportIsNotDefined(JSContext* cx, HandlePropertyName name)
{
    RootedId id(cx, NameToId(name));
    return ReportIsNotDefined(cx, id);
}

bool
js::ReportIsNullOrUndefined(JSContext* cx, int spindex, HandleValue v,
                            HandleString fallback)
{
    bool ok;

    UniqueChars bytes = DecompileValueGenerator(cx, spindex, v, fallback);
    if (!bytes)
        return false;

    if (strcmp(bytes.get(), js_undefined_str) == 0 ||
        strcmp(bytes.get(), js_null_str) == 0) {
        ok = JS_ReportErrorFlagsAndNumberLatin1(cx, JSREPORT_ERROR,
                                                GetErrorMessage, nullptr,
                                                JSMSG_NO_PROPERTIES,
                                                bytes.get());
    } else if (v.isUndefined()) {
        ok = JS_ReportErrorFlagsAndNumberLatin1(cx, JSREPORT_ERROR,
                                                GetErrorMessage, nullptr,
                                                JSMSG_UNEXPECTED_TYPE,
                                                bytes.get(), js_undefined_str);
    } else {
        MOZ_ASSERT(v.isNull());
        ok = JS_ReportErrorFlagsAndNumberLatin1(cx, JSREPORT_ERROR,
                                                GetErrorMessage, nullptr,
                                                JSMSG_UNEXPECTED_TYPE,
                                                bytes.get(), js_null_str);
    }

    return ok;
}

void
js::ReportMissingArg(JSContext* cx, HandleValue v, unsigned arg)
{
    char argbuf[11];
    UniqueChars bytes;

    SprintfLiteral(argbuf, "%u", arg);
    if (IsFunctionObject(v)) {
        RootedAtom name(cx, v.toObject().as<JSFunction>().explicitName());
        bytes = DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, v, name);
        if (!bytes)
            return;
    }
    JS_ReportErrorNumberLatin1(cx, GetErrorMessage, nullptr,
                               JSMSG_MISSING_FUN_ARG,
                               argbuf, bytes ? bytes.get() : "");
}

bool
js::ReportValueErrorFlags(JSContext* cx, unsigned flags, const unsigned errorNumber,
                          int spindex, HandleValue v, HandleString fallback,
                          const char* arg1, const char* arg2)
{
    UniqueChars bytes;
    bool ok;

    MOZ_ASSERT(js_ErrorFormatString[errorNumber].argCount >= 1);
    MOZ_ASSERT(js_ErrorFormatString[errorNumber].argCount <= 3);
    bytes = DecompileValueGenerator(cx, spindex, v, fallback);
    if (!bytes)
        return false;

    ok = JS_ReportErrorFlagsAndNumberLatin1(cx, flags, GetErrorMessage, nullptr, errorNumber,
                                            bytes.get(), arg1, arg2);
    return ok;
}

const JSErrorFormatString js_ErrorFormatString[JSErr_Limit] = {
#define MSG_DEF(name, count, exception, format) \
    { #name, format, count, exception } ,
#include "js.msg"
#undef MSG_DEF
};

JS_FRIEND_API(const JSErrorFormatString*)
js::GetErrorMessage(void* userRef, const unsigned errorNumber)
{
    if (errorNumber > 0 && errorNumber < JSErr_Limit)
        return &js_ErrorFormatString[errorNumber];
    return nullptr;
}

ExclusiveContext::ExclusiveContext(JSRuntime* rt, PerThreadData* pt, ContextKind kind,
                                   const JS::ContextOptions& options)
  : ContextFriendFields(kind == Context_JS),
    runtime_(rt),
    helperThread_(nullptr),
    contextKind_(kind),
    options_(options),
    perThreadData(pt),
    arenas_(nullptr),
    enterCompartmentDepth_(0)
{
}

void
ExclusiveContext::recoverFromOutOfMemory()
{
    if (JSContext* maybecx = maybeJSContext()) {
        if (maybecx->isExceptionPending()) {
            MOZ_ASSERT(maybecx->isThrowingOutOfMemory());
            maybecx->clearPendingException();
        }
        return;
    }
    // Keep in sync with addPendingOutOfMemory.
    if (ParseTask* task = helperThread()->parseTask())
        task->outOfMemory = false;
}

JS::Error ExclusiveContext::reportedError;
JS::OOM ExclusiveContext::reportedOOM;

mozilla::GenericErrorResult<OOM&>
ExclusiveContext::alreadyReportedOOM()
{
#ifdef DEBUG
    if (JSContext* maybecx = maybeJSContext()) {
        MOZ_ASSERT(maybecx->isThrowingOutOfMemory());
    } else {
        // Keep in sync with addPendingOutOfMemory.
        if (ParseTask* task = helperThread()->parseTask())
            MOZ_ASSERT(task->outOfMemory);
    }
#endif
    return mozilla::MakeGenericErrorResult(reportedOOM);
}

mozilla::GenericErrorResult<JS::Error&>
ExclusiveContext::alreadyReportedError()
{
#ifdef DEBUG
    if (JSContext* maybecx = maybeJSContext())
        MOZ_ASSERT(maybecx->isExceptionPending());
#endif
    return mozilla::MakeGenericErrorResult(reportedError);
}

JSContext::JSContext(JSRuntime* parentRuntime)
  : ExclusiveContext(this, &this->JSRuntime::mainThread, Context_JS, JS::ContextOptions()),
    JSRuntime(parentRuntime),
    throwing(false),
    unwrappedException_(this),
    overRecursed_(false),
    propagatingForcedReturn_(false),
    liveVolatileJitFrameIterators_(nullptr),
    reportGranularity(JS_DEFAULT_JITREPORT_GRANULARITY),
    resolvingList(nullptr),
    generatingError(false),
    data(nullptr),
    outstandingRequests(0),
    jitIsBroken(false),
    asyncStackForNewActivations(this),
    asyncCauseForNewActivations(nullptr),
    asyncCallIsExplicit(false)
{
    MOZ_ASSERT(static_cast<ContextFriendFields*>(this) ==
               ContextFriendFields::get(this));
}

JSContext::~JSContext()
{
    destroyRuntime();

    /* Free the stuff hanging off of cx. */
    MOZ_ASSERT(!resolvingList);
}

bool
JSContext::getPendingException(MutableHandleValue rval)
{
    MOZ_ASSERT(throwing);
    rval.set(unwrappedException_);
    if (IsAtomsCompartment(compartment()))
        return true;
    bool wasOverRecursed = overRecursed_;
    clearPendingException();
    if (!compartment()->wrap(this, rval))
        return false;
    assertSameCompartment(this, rval);
    setPendingException(rval);
    overRecursed_ = wasOverRecursed;
    return true;
}

bool
JSContext::isThrowingOutOfMemory()
{
    return throwing && unwrappedException_ == StringValue(names().outOfMemory);
}

bool
JSContext::isClosingGenerator()
{
    return throwing && unwrappedException_.isMagic(JS_GENERATOR_CLOSING);
}

bool
JSContext::isThrowingDebuggeeWouldRun()
{
    return throwing &&
           unwrappedException_.isObject() &&
           unwrappedException_.toObject().is<ErrorObject>() &&
           unwrappedException_.toObject().as<ErrorObject>().type() == JSEXN_DEBUGGEEWOULDRUN;
}

static bool
ComputeIsJITBroken()
{
#if !defined(ANDROID)
    return false;
#else  // ANDROID
    if (getenv("JS_IGNORE_JIT_BROKENNESS")) {
        return false;
    }

    std::string line;

    // Check for the known-bad kernel version (2.6.29).
    std::ifstream osrelease("/proc/sys/kernel/osrelease");
    std::getline(osrelease, line);
    __android_log_print(ANDROID_LOG_INFO, "Gecko", "Detected osrelease `%s'",
                        line.c_str());

    if (line.npos == line.find("2.6.29")) {
        // We're using something other than 2.6.29, so the JITs should work.
        __android_log_print(ANDROID_LOG_INFO, "Gecko", "JITs are not broken");
        return false;
    }

    // We're using 2.6.29, and this causes trouble with the JITs on i9000.
    line = "";
    bool broken = false;
    std::ifstream cpuinfo("/proc/cpuinfo");
    do {
        if (0 == line.find("Hardware")) {
            static const char* const blacklist[] = {
                "SCH-I400",     // Samsung Continuum
                "SGH-T959",     // Samsung i9000, Vibrant device
                "SGH-I897",     // Samsung i9000, Captivate device
                "SCH-I500",     // Samsung i9000, Fascinate device
                "SPH-D700",     // Samsung i9000, Epic device
                "GT-I9000",     // Samsung i9000, UK/Europe device
                nullptr
            };
            for (const char* const* hw = &blacklist[0]; *hw; ++hw) {
                if (line.npos != line.find(*hw)) {
                    __android_log_print(ANDROID_LOG_INFO, "Gecko",
                                        "Blacklisted device `%s'", *hw);
                    broken = true;
                    break;
                }
            }
            break;
        }
        std::getline(cpuinfo, line);
    } while(!cpuinfo.fail() && !cpuinfo.eof());

    __android_log_print(ANDROID_LOG_INFO, "Gecko", "JITs are %sbroken",
                        broken ? "" : "not ");

    return broken;
#endif  // ifndef ANDROID
}

static bool
IsJITBrokenHere()
{
    static bool computedIsBroken = false;
    static bool isBroken = false;
    if (!computedIsBroken) {
        isBroken = ComputeIsJITBroken();
        computedIsBroken = true;
    }
    return isBroken;
}

void
JSContext::updateJITEnabled()
{
    jitIsBroken = IsJITBrokenHere();
}

size_t
JSContext::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
    /*
     * There are other JSContext members that could be measured; the following
     * ones have been found by DMD to be worth measuring.  More stuff may be
     * added later.
     */
    return cycleDetectorSet.sizeOfExcludingThis(mallocSizeOf);
}

void
JSContext::trace(JSTracer* trc)
{
    if (cycleDetectorSet.initialized())
        TraceCycleDetectionSet(trc, cycleDetectorSet);

    if (trc->isMarkingTracer() && compartment_)
        compartment_->mark();
}

void*
ExclusiveContext::stackLimitAddressForJitCode(StackKind kind)
{
#ifdef JS_SIMULATOR
    return runtime_->addressOfSimulatorStackLimit();
#else
    return stackLimitAddress(kind);
#endif
}

uintptr_t
ExclusiveContext::stackLimitForJitCode(StackKind kind)
{
#ifdef JS_SIMULATOR
    return runtime_->simulator()->stackLimit();
#else
    return stackLimit(kind);
#endif
}

void
JSContext::resetJitStackLimit()
{
    // Note that, for now, we use the untrusted limit for ion. This is fine,
    // because it's the most conservative limit, and if we hit it, we'll bail
    // out of ion into the interpreter, which will do a proper recursion check.
#ifdef JS_SIMULATOR
    jitStackLimit_ = jit::Simulator::StackLimit();
#else
    jitStackLimit_ = nativeStackLimit[StackForUntrustedScript];
#endif
    jitStackLimitNoInterrupt_ = jitStackLimit_;
}

void
JSContext::initJitStackLimit()
{
    resetJitStackLimit();
}

JSVersion
JSContext::findVersion() const
{
    if (JSScript* script = currentScript(nullptr, ALLOW_CROSS_COMPARTMENT))
        return script->getVersion();

    if (compartment() && compartment()->behaviors().version() != JSVERSION_UNKNOWN)
        return compartment()->behaviors().version();

    return defaultVersion();
}

#ifdef DEBUG

JS::AutoCheckRequestDepth::AutoCheckRequestDepth(JSContext* cx)
    : cx(cx)
{
    MOZ_ASSERT(cx->runtime()->requestDepth || cx->runtime()->isHeapBusy());
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(cx->runtime()));
    cx->runtime()->checkRequestDepth++;
}

JS::AutoCheckRequestDepth::AutoCheckRequestDepth(ContextFriendFields* cxArg)
    : cx(static_cast<ExclusiveContext*>(cxArg)->maybeJSContext())
{
    if (cx) {
        MOZ_ASSERT(cx->runtime()->requestDepth || cx->runtime()->isHeapBusy());
        MOZ_ASSERT(CurrentThreadCanAccessRuntime(cx->runtime()));
        cx->runtime()->checkRequestDepth++;
    }
}

JS::AutoCheckRequestDepth::~AutoCheckRequestDepth()
{
    if (cx) {
        MOZ_ASSERT(cx->runtime()->checkRequestDepth != 0);
        cx->runtime()->checkRequestDepth--;
    }
}

#endif

#ifdef JS_CRASH_DIAGNOSTICS
void
CompartmentChecker::check(InterpreterFrame* fp)
{
    if (fp)
        check(fp->environmentChain());
}

void
CompartmentChecker::check(AbstractFramePtr frame)
{
    if (frame)
        check(frame.environmentChain());
}
#endif

void
AutoEnterOOMUnsafeRegion::crash(const char* reason)
{
    char msgbuf[1024];
    SprintfLiteral(msgbuf, "[unhandlable oom] %s", reason);
    MOZ_ReportAssertionFailure(msgbuf, __FILE__, __LINE__);
    MOZ_CRASH();
}

AutoEnterOOMUnsafeRegion::AnnotateOOMAllocationSizeCallback
AutoEnterOOMUnsafeRegion::annotateOOMSizeCallback = nullptr;

void
AutoEnterOOMUnsafeRegion::crash(size_t size, const char* reason)
{
    {
        JS::AutoSuppressGCAnalysis suppress;
        if (annotateOOMSizeCallback)
            annotateOOMSizeCallback(size);
    }
    crash(reason);
}
