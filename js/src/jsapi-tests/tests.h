/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsapi_tests_tests_h
#define jsapi_tests_tests_h

#include "mozilla/ArrayUtils.h"
#include "mozilla/TypeTraits.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jsalloc.h"
#include "jscntxt.h"
#include "jsgc.h"

#include "js/Vector.h"

/* Note: Aborts on OOM. */
class JSAPITestString {
    js::Vector<char, 0, js::SystemAllocPolicy> chars;
  public:
    JSAPITestString() {}
    explicit JSAPITestString(const char* s) { *this += s; }
    JSAPITestString(const JSAPITestString& s) { *this += s; }

    const char* begin() const { return chars.begin(); }
    const char* end() const { return chars.end(); }
    size_t length() const { return chars.length(); }

    JSAPITestString & operator +=(const char* s) {
        if (!chars.append(s, strlen(s)))
            abort();
        return *this;
    }

    JSAPITestString & operator +=(const JSAPITestString& s) {
        if (!chars.append(s.begin(), s.length()))
            abort();
        return *this;
    }
};

inline JSAPITestString operator+(JSAPITestString a, const char* b) { return a += b; }
inline JSAPITestString operator+(JSAPITestString a, const JSAPITestString& b) { return a += b; }

class JSAPITest
{
  public:
    static JSAPITest* list;
    JSAPITest* next;

    JSRuntime* rt;
    JSContext* cx;
    JS::PersistentRootedObject global;
    bool knownFail;
    JSAPITestString msgs;
    JSCompartment* oldCompartment;

    JSAPITest() : rt(nullptr), cx(nullptr), knownFail(false), oldCompartment(nullptr) {
        next = list;
        list = this;
    }

    virtual ~JSAPITest() {
        MOZ_RELEASE_ASSERT(!rt);
        MOZ_RELEASE_ASSERT(!cx);
        MOZ_RELEASE_ASSERT(!global);
    }

    virtual bool init();
    virtual void uninit();

    virtual const char * name() = 0;
    virtual bool run(JS::HandleObject global) = 0;

#define EXEC(s) do { if (!exec(s, __FILE__, __LINE__)) return false; } while (false)

    bool exec(const char* bytes, const char* filename, int lineno);

    // Like exec(), but doesn't call fail() if JS::Evaluate returns false.
    bool execDontReport(const char* bytes, const char* filename, int lineno);

#define EVAL(s, vp) do { if (!evaluate(s, __FILE__, __LINE__, vp)) return false; } while (false)

    bool evaluate(const char* bytes, const char* filename, int lineno, JS::MutableHandleValue vp);

    JSAPITestString jsvalToSource(JS::HandleValue v) {
        JSString* str = JS_ValueToSource(cx, v);
        if (str) {
            JSAutoByteString bytes(cx, str);
            if (!!bytes)
                return JSAPITestString(bytes.ptr());
        }
        JS_ClearPendingException(cx);
        return JSAPITestString("<<error converting value to string>>");
    }

    JSAPITestString toSource(long v) {
        char buf[40];
        sprintf(buf, "%ld", v);
        return JSAPITestString(buf);
    }

    JSAPITestString toSource(unsigned long v) {
        char buf[40];
        sprintf(buf, "%lu", v);
        return JSAPITestString(buf);
    }

    JSAPITestString toSource(long long v) {
        char buf[40];
        sprintf(buf, "%lld", v);
        return JSAPITestString(buf);
    }

    JSAPITestString toSource(unsigned long long v) {
        char buf[40];
        sprintf(buf, "%llu", v);
        return JSAPITestString(buf);
    }

    JSAPITestString toSource(unsigned int v) {
        return toSource((unsigned long)v);
    }

    JSAPITestString toSource(int v) {
        return toSource((long)v);
    }

    JSAPITestString toSource(bool v) {
        return JSAPITestString(v ? "true" : "false");
    }

    JSAPITestString toSource(JSAtom* v) {
        JS::RootedValue val(cx, JS::StringValue((JSString*)v));
        return jsvalToSource(val);
    }

    JSAPITestString toSource(JSVersion v) {
        return JSAPITestString(JS_VersionToString(v));
    }

    // Note that in some still-supported GCC versions (we think anything before
    // GCC 4.6), this template does not work when the second argument is
    // nullptr. It infers type U = long int. Use CHECK_NULL instead.
    template <typename T, typename U>
    bool checkEqual(const T& actual, const U& expected,
                    const char* actualExpr, const char* expectedExpr,
                    const char* filename, int lineno)
    {
        static_assert(mozilla::IsSigned<T>::value == mozilla::IsSigned<U>::value,
                      "using CHECK_EQUAL with different-signed inputs triggers compiler warnings");
        static_assert(mozilla::IsUnsigned<T>::value == mozilla::IsUnsigned<U>::value,
                      "using CHECK_EQUAL with different-signed inputs triggers compiler warnings");
        return (actual == expected) ||
            fail(JSAPITestString("CHECK_EQUAL failed: expected (") +
                 expectedExpr + ") = " + toSource(expected) +
                 ", got (" + actualExpr + ") = " + toSource(actual), filename, lineno);
    }

#define CHECK_EQUAL(actual, expected) \
    do { \
        if (!checkEqual(actual, expected, #actual, #expected, __FILE__, __LINE__)) \
            return false; \
    } while (false)

    template <typename T>
    bool checkNull(const T* actual, const char* actualExpr,
                   const char* filename, int lineno) {
        return (actual == nullptr) ||
            fail(JSAPITestString("CHECK_NULL failed: expected nullptr, got (") +
                 actualExpr + ") = " + toSource(actual),
                 filename, lineno);
    }

#define CHECK_NULL(actual) \
    do { \
        if (!checkNull(actual, #actual, __FILE__, __LINE__)) \
            return false; \
    } while (false)

    bool checkSame(JS::Value actualArg, JS::Value expectedArg,
                   const char* actualExpr, const char* expectedExpr,
                   const char* filename, int lineno) {
        bool same;
        JS::RootedValue actual(cx, actualArg), expected(cx, expectedArg);
        return (JS_SameValue(cx, actual, expected, &same) && same) ||
               fail(JSAPITestString("CHECK_SAME failed: expected JS_SameValue(cx, ") +
                    actualExpr + ", " + expectedExpr + "), got !JS_SameValue(cx, " +
                    jsvalToSource(actual) + ", " + jsvalToSource(expected) + ")", filename, lineno);
    }

#define CHECK_SAME(actual, expected) \
    do { \
        if (!checkSame(actual, expected, #actual, #expected, __FILE__, __LINE__)) \
            return false; \
    } while (false)

#define CHECK(expr) \
    do { \
        if (!(expr)) \
            return fail(JSAPITestString("CHECK failed: " #expr), __FILE__, __LINE__); \
    } while (false)

    bool fail(JSAPITestString msg = JSAPITestString(), const char* filename = "-", int lineno = 0) {
        if (JS_IsExceptionPending(cx)) {
            js::gc::AutoSuppressGC gcoff(cx);
            JS::RootedValue v(cx);
            JS_GetPendingException(cx, &v);
            JS_ClearPendingException(cx);
            JSString* s = JS::ToString(cx, v);
            if (s) {
                JSAutoByteString bytes(cx, s);
                if (!!bytes)
                    msg += bytes.ptr();
            }
        }
        fprintf(stderr, "%s:%d:%.*s\n", filename, lineno, (int) msg.length(), msg.begin());
        msgs += msg;
        return false;
    }

    JSAPITestString messages() const { return msgs; }

    static const JSClass * basicGlobalClass() {
        static const JSClassOps cOps = {
            nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr,
            JS_GlobalObjectTraceHook
        };
        static const JSClass c = {
            "global", JSCLASS_GLOBAL_FLAGS,
            &cOps
        };
        return &c;
    }

  protected:
    static bool
    print(JSContext* cx, unsigned argc, JS::Value* vp)
    {
        JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

        for (unsigned i = 0; i < args.length(); i++) {
            JSString* str = JS::ToString(cx, args[i]);
            if (!str)
                return false;
            char* bytes = JS_EncodeString(cx, str);
            if (!bytes)
                return false;
            printf("%s%s", i ? " " : "", bytes);
            JS_free(cx, bytes);
        }

        putchar('\n');
        fflush(stdout);
        args.rval().setUndefined();
        return true;
    }

    bool definePrint();

    static void setNativeStackQuota(JSRuntime* rt)
    {
        const size_t MAX_STACK_SIZE =
/* Assume we can't use more than 5e5 bytes of C stack by default. */
#if (defined(DEBUG) && defined(__SUNPRO_CC))  || defined(JS_CPU_SPARC)
            /*
             * Sun compiler uses a larger stack space for js::Interpret() with
             * debug.  Use a bigger gMaxStackSize to make "make check" happy.
             */
            5000000
#else
            500000
#endif
        ;

        JS_SetNativeStackQuota(rt, MAX_STACK_SIZE);
    }

    virtual JSRuntime * createRuntime() {
        JSRuntime* rt = JS_NewRuntime(8L * 1024 * 1024);
        if (!rt)
            return nullptr;
        JS_SetErrorReporter(rt, &reportWarning);
        setNativeStackQuota(rt);
        return rt;
    }

    virtual void destroyRuntime() {
        MOZ_RELEASE_ASSERT(!cx);
        MOZ_RELEASE_ASSERT(rt);
        JS_DestroyRuntime(rt);
        rt = nullptr;
    }

    static void reportWarning(JSContext* cx, const char* message, JSErrorReport* report) {
        MOZ_RELEASE_ASSERT(report);
        MOZ_RELEASE_ASSERT(JSREPORT_IS_WARNING(report->flags));

        fprintf(stderr, "%s:%u:%s\n",
                report->filename ? report->filename : "<no filename>",
                (unsigned int) report->lineno,
                message);
    }

    virtual JSContext* createContext() {
        return JS_NewContext(rt, 8192);
    }

    virtual const JSClass * getGlobalClass() {
        return basicGlobalClass();
    }

    virtual JSObject * createGlobal(JSPrincipals* principals = nullptr);
};

#define BEGIN_TEST(testname)                                            \
    class cls_##testname : public JSAPITest {                           \
      public:                                                           \
        virtual const char * name() override { return #testname; }      \
        virtual bool run(JS::HandleObject global) override

#define END_TEST(testname)                                              \
    };                                                                  \
    static cls_##testname cls_##testname##_instance;

/*
 * A "fixture" is a subclass of JSAPITest that holds common definitions for a
 * set of tests. Each test that wants to use the fixture should use
 * BEGIN_FIXTURE_TEST and END_FIXTURE_TEST, just as one would use BEGIN_TEST and
 * END_TEST, but include the fixture class as the first argument. The fixture
 * class's declarations are then in scope for the test bodies.
 */

#define BEGIN_FIXTURE_TEST(fixture, testname)                           \
    class cls_##testname : public fixture {                             \
      public:                                                           \
        virtual const char * name() override { return #testname; }      \
        virtual bool run(JS::HandleObject global) override

#define END_FIXTURE_TEST(fixture, testname)                             \
    };                                                                  \
    static cls_##testname cls_##testname##_instance;

/*
 * A class for creating and managing one temporary file.
 *
 * We could use the ISO C temporary file functions here, but those try to
 * create files in the root directory on Windows, which fails for users
 * without Administrator privileges.
 */
class TempFile {
    const char* name;
    FILE* stream;

  public:
    TempFile() : name(), stream() { }
    ~TempFile() {
        if (stream)
            close();
        if (name)
            remove();
    }

    /*
     * Return a stream for a temporary file named |fileName|. Infallible.
     * Use only once per TempFile instance. If the file is not explicitly
     * closed and deleted via the member functions below, this object's
     * destructor will clean them up.
     */
    FILE* open(const char* fileName)
    {
        stream = fopen(fileName, "wb+");
        if (!stream) {
            fprintf(stderr, "error opening temporary file '%s': %s\n",
                    fileName, strerror(errno));
            exit(1);
        }
        name = fileName;
        return stream;
    }

    /* Close the temporary file's stream. */
    void close() {
        if (fclose(stream) == EOF) {
            fprintf(stderr, "error closing temporary file '%s': %s\n",
                    name, strerror(errno));
            exit(1);
        }
        stream = nullptr;
    }

    /* Delete the temporary file. */
    void remove() {
        if (::remove(name) != 0) {
            fprintf(stderr, "error deleting temporary file '%s': %s\n",
                    name, strerror(errno));
            exit(1);
        }
        name = nullptr;
    }
};

// Just a wrapper around JSPrincipals that allows static construction.
class TestJSPrincipals : public JSPrincipals
{
  public:
    explicit TestJSPrincipals(int rc = 0)
      : JSPrincipals()
    {
        refcount = rc;
    }

    bool write(JSContext* cx, JSStructuredCloneWriter* writer) override {
        MOZ_ASSERT(false, "not implemented");
        return false;
    }
};

#ifdef JS_GC_ZEAL
/*
 * Temporarily disable the GC zeal setting. This is only useful in tests that
 * need very explicit GC behavior and should not be used elsewhere.
 */
class AutoLeaveZeal
{
    JSContext* cx_;
    uint32_t zealBits_;
    uint32_t frequency_;

  public:
    explicit AutoLeaveZeal(JSContext* cx) : cx_(cx) {
        uint32_t dummy;
        JS_GetGCZealBits(cx_, &zealBits_, &frequency_, &dummy);
        JS_SetGCZeal(JS_GetRuntime(cx_), 0, 0);
        JS::PrepareForFullGC(JS_GetRuntime(cx_));
        JS::GCForReason(JS_GetRuntime(cx_), GC_SHRINK, JS::gcreason::DEBUG_GC);
    }
    ~AutoLeaveZeal() {
        for (size_t i = 0; i < sizeof(zealBits_) * 8; i++) {
            if (zealBits_ & (1 << i))
                JS_SetGCZeal(JS_GetRuntime(cx_), i, frequency_);
        }

#ifdef DEBUG
        uint32_t zealBitsAfter, frequencyAfter, dummy;
        JS_GetGCZealBits(cx_, &zealBitsAfter, &frequencyAfter, &dummy);
        MOZ_ASSERT(zealBitsAfter == zealBits_);
        MOZ_ASSERT(frequencyAfter == frequency_);
#endif
    }
};
#endif /* JS_GC_ZEAL */

#endif /* jsapi_tests_tests_h */
