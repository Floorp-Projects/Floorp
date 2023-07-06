/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsapi_tests_tests_h
#define jsapi_tests_tests_h

#include "mozilla/Sprintf.h"

#include <errno.h>
#include <iterator>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <type_traits>

#include "jsapi.h"

#include "gc/GC.h"
#include "js/AllocPolicy.h"
#include "js/ArrayBuffer.h"
#include "js/CharacterEncoding.h"
#include "js/Conversions.h"
#include "js/Equality.h"      // JS::SameValue
#include "js/GlobalObject.h"  // JS::DefaultGlobalClassOps
#include "js/RegExpFlags.h"   // JS::RegExpFlags
#include "js/Vector.h"
#include "js/Warnings.h"  // JS::SetWarningReporter
#include "vm/JSContext.h"

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
  void clear() { chars.clearAndFree(); }

  JSAPITestString& operator+=(const char* s) {
    if (!chars.append(s, strlen(s))) {
      abort();
    }
    return *this;
  }

  JSAPITestString& operator+=(const JSAPITestString& s) {
    if (!chars.append(s.begin(), s.length())) {
      abort();
    }
    return *this;
  }
};

inline JSAPITestString operator+(const JSAPITestString& a, const char* b) {
  JSAPITestString result = a;
  result += b;
  return result;
}

inline JSAPITestString operator+(const JSAPITestString& a,
                                 const JSAPITestString& b) {
  JSAPITestString result = a;
  result += b;
  return result;
}

class JSAPIRuntimeTest;

class JSAPITest {
 public:
  bool knownFail;
  JSAPITestString msgs;

  JSAPITest() : knownFail(false) {}

  virtual ~JSAPITest() {}

  virtual const char* name() = 0;

  virtual void maybeAppendException(JSAPITestString& message) {}

  bool fail(const JSAPITestString& msg = JSAPITestString(),
            const char* filename = "-", int lineno = 0) {
    char location[256];
    SprintfLiteral(location, "%s:%d:", filename, lineno);

    JSAPITestString message(location);
    message += msg;

    maybeAppendException(message);

    fprintf(stderr, "%.*s\n", int(message.length()), message.begin());

    if (msgs.length() != 0) {
      msgs += " | ";
    }
    msgs += message;
    return false;
  }

  JSAPITestString messages() const { return msgs; }
};

class JSAPIRuntimeTest : public JSAPITest {
 public:
  static JSAPIRuntimeTest* list;
  JSAPIRuntimeTest* next;

  JSContext* cx;
  JS::PersistentRootedObject global;

  // Whether this test is willing to skip its init() and reuse a global (and
  // JSContext etc.) from a previous test that also has reuseGlobal=true. It
  // also means this test is willing to skip its uninit() if it is followed by
  // another reuseGlobal test.
  bool reuseGlobal;

  JSAPIRuntimeTest() : JSAPITest(), cx(nullptr), reuseGlobal(false) {
    next = list;
    list = this;
  }

  virtual ~JSAPIRuntimeTest() {
    MOZ_RELEASE_ASSERT(!cx);
    MOZ_RELEASE_ASSERT(!global);
  }

  // Initialize this test, possibly with the cx from a previously run test.
  bool init(JSContext* maybeReusedContext);

  // If this test is ok with its cx and global being reused, release this
  // test's cx to be reused by another test.
  JSContext* maybeForgetContext();

  static void MaybeFreeContext(JSContext* maybeCx);

  // The real initialization happens in init(JSContext*), above, but this
  // method may be overridden to perform additional initialization after the
  // JSContext and global have been created.
  virtual bool init() { return true; }
  virtual void uninit();

  virtual bool run(JS::HandleObject global) = 0;

#define EXEC(s)                                     \
  do {                                              \
    if (!exec(s, __FILE__, __LINE__)) return false; \
  } while (false)

  bool exec(const char* utf8, const char* filename, int lineno);

  // Like exec(), but doesn't call fail() if JS::Evaluate returns false.
  bool execDontReport(const char* utf8, const char* filename, int lineno);

#define EVAL(s, vp)                                         \
  do {                                                      \
    if (!evaluate(s, __FILE__, __LINE__, vp)) return false; \
  } while (false)

  bool evaluate(const char* utf8, const char* filename, int lineno,
                JS::MutableHandleValue vp);

  JSAPITestString jsvalToSource(JS::HandleValue v) {
    JS::Rooted<JSString*> str(cx, JS_ValueToSource(cx, v));
    if (str) {
      if (JS::UniqueChars bytes = JS_EncodeStringToUTF8(cx, str)) {
        return JSAPITestString(bytes.get());
      }
    }
    JS_ClearPendingException(cx);
    return JSAPITestString("<<error converting value to string>>");
  }

  JSAPITestString toSource(char c) {
    char buf[2] = {c, '\0'};
    return JSAPITestString(buf);
  }

  JSAPITestString toSource(long v) {
    char buf[40];
    SprintfLiteral(buf, "%ld", v);
    return JSAPITestString(buf);
  }

  JSAPITestString toSource(unsigned long v) {
    char buf[40];
    SprintfLiteral(buf, "%lu", v);
    return JSAPITestString(buf);
  }

  JSAPITestString toSource(long long v) {
    char buf[40];
    SprintfLiteral(buf, "%lld", v);
    return JSAPITestString(buf);
  }

  JSAPITestString toSource(unsigned long long v) {
    char buf[40];
    SprintfLiteral(buf, "%llu", v);
    return JSAPITestString(buf);
  }

  JSAPITestString toSource(double d) {
    char buf[40];
    SprintfLiteral(buf, "%17lg", d);
    return JSAPITestString(buf);
  }

  JSAPITestString toSource(unsigned int v) {
    return toSource((unsigned long)v);
  }

  JSAPITestString toSource(int v) { return toSource((long)v); }

  JSAPITestString toSource(bool v) {
    return JSAPITestString(v ? "true" : "false");
  }

  JSAPITestString toSource(JS::RegExpFlags flags) {
    JSAPITestString str;
    if (flags.hasIndices()) {
      str += "d";
    }
    if (flags.global()) {
      str += "g";
    }
    if (flags.ignoreCase()) {
      str += "i";
    }
    if (flags.multiline()) {
      str += "m";
    }
    if (flags.dotAll()) {
      str += "s";
    }
    if (flags.unicode()) {
      str += "u";
    }
    if (flags.unicodeSets()) {
      str += "v";
    }
    if (flags.sticky()) {
      str += "y";
    }
    return str;
  }

  JSAPITestString toSource(JSAtom* v) {
    JS::RootedValue val(cx, JS::StringValue((JSString*)v));
    return jsvalToSource(val);
  }

  // Note that in some still-supported GCC versions (we think anything before
  // GCC 4.6), this template does not work when the second argument is
  // nullptr. It infers type U = long int. Use CHECK_NULL instead.
  template <typename T, typename U>
  bool checkEqual(const T& actual, const U& expected, const char* actualExpr,
                  const char* expectedExpr, const char* filename, int lineno) {
    static_assert(std::is_signed_v<T> == std::is_signed_v<U>,
                  "using CHECK_EQUAL with different-signed inputs triggers "
                  "compiler warnings");
    static_assert(
        std::is_unsigned_v<T> == std::is_unsigned_v<U>,
        "using CHECK_EQUAL with different-signed inputs triggers compiler "
        "warnings");
    return (actual == expected) ||
           fail(JSAPITestString("CHECK_EQUAL failed: expected (") +
                    expectedExpr + ") = " + toSource(expected) + ", got (" +
                    actualExpr + ") = " + toSource(actual),
                filename, lineno);
  }

#define CHECK_EQUAL(actual, expected)                                          \
  do {                                                                         \
    if (!checkEqual(actual, expected, #actual, #expected, __FILE__, __LINE__)) \
      return false;                                                            \
  } while (false)

  template <typename T>
  bool checkNull(const T* actual, const char* actualExpr, const char* filename,
                 int lineno) {
    return (actual == nullptr) ||
           fail(JSAPITestString("CHECK_NULL failed: expected nullptr, got (") +
                    actualExpr + ") = " + toSource(actual),
                filename, lineno);
  }

#define CHECK_NULL(actual)                                             \
  do {                                                                 \
    if (!checkNull(actual, #actual, __FILE__, __LINE__)) return false; \
  } while (false)

  bool checkSame(const JS::Value& actualArg, const JS::Value& expectedArg,
                 const char* actualExpr, const char* expectedExpr,
                 const char* filename, int lineno) {
    bool same;
    JS::RootedValue actual(cx, actualArg), expected(cx, expectedArg);
    return (JS::SameValue(cx, actual, expected, &same) && same) ||
           fail(JSAPITestString(
                    "CHECK_SAME failed: expected JS::SameValue(cx, ") +
                    actualExpr + ", " + expectedExpr +
                    "), got !JS::SameValue(cx, " + jsvalToSource(actual) +
                    ", " + jsvalToSource(expected) + ")",
                filename, lineno);
  }

#define CHECK_SAME(actual, expected)                                          \
  do {                                                                        \
    if (!checkSame(actual, expected, #actual, #expected, __FILE__, __LINE__)) \
      return false;                                                           \
  } while (false)

#define CHECK(expr)                                                  \
  do {                                                               \
    if (!(expr))                                                     \
      return fail(JSAPITestString("CHECK failed: " #expr), __FILE__, \
                  __LINE__);                                         \
  } while (false)

  void maybeAppendException(JSAPITestString& message) override {
    if (JS_IsExceptionPending(cx)) {
      message += " -- ";

      js::gc::AutoSuppressGC gcoff(cx);
      JS::RootedValue v(cx);
      JS_GetPendingException(cx, &v);
      JS_ClearPendingException(cx);
      JS::Rooted<JSString*> s(cx, JS::ToString(cx, v));
      if (s) {
        if (JS::UniqueChars bytes = JS_EncodeStringToLatin1(cx, s)) {
          message += bytes.get();
        }
      }
    }
  }

  static const JSClass* basicGlobalClass() {
    static const JSClass c = {"global", JSCLASS_GLOBAL_FLAGS,
                              &JS::DefaultGlobalClassOps};
    return &c;
  }

 protected:
  static bool print(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

    JS::Rooted<JSString*> str(cx);
    for (unsigned i = 0; i < args.length(); i++) {
      str = JS::ToString(cx, args[i]);
      if (!str) {
        return false;
      }
      JS::UniqueChars bytes = JS_EncodeStringToUTF8(cx, str);
      if (!bytes) {
        return false;
      }
      printf("%s%s", i ? " " : "", bytes.get());
    }

    putchar('\n');
    fflush(stdout);
    args.rval().setUndefined();
    return true;
  }

  bool definePrint();

  virtual JSContext* createContext() {
    JSContext* cx = JS_NewContext(8L * 1024 * 1024);
    if (!cx) {
      return nullptr;
    }
    JS::SetWarningReporter(cx, &reportWarning);
    return cx;
  }

  static void reportWarning(JSContext* cx, JSErrorReport* report) {
    MOZ_RELEASE_ASSERT(report->isWarning());

    fprintf(stderr, "%s:%u:%s\n",
            report->filename ? report->filename.c_str() : "<no filename>",
            (unsigned int)report->lineno, report->message().c_str());
  }

  virtual const JSClass* getGlobalClass() { return basicGlobalClass(); }

  virtual JSObject* createGlobal(JSPrincipals* principals = nullptr);
};

class JSAPIFrontendTest : public JSAPITest {
 public:
  static JSAPIFrontendTest* list;
  JSAPIFrontendTest* next;

  JSAPIFrontendTest() : JSAPITest() {
    next = list;
    list = this;
  }

  virtual ~JSAPIFrontendTest() {}

  virtual bool init() { return true; }
  virtual void uninit() {}

  virtual bool run() = 0;
};

#define BEGIN_TEST_WITH_ATTRIBUTES_AND_EXTRA(testname, attrs, extra) \
  class cls_##testname : public JSAPIRuntimeTest {                   \
   public:                                                           \
    virtual const char* name() override { return #testname; }        \
    extra virtual bool run(JS::HandleObject global) override attrs

#define BEGIN_TEST_WITH_ATTRIBUTES(testname, attrs) \
  BEGIN_TEST_WITH_ATTRIBUTES_AND_EXTRA(testname, attrs, )

#define BEGIN_TEST(testname) BEGIN_TEST_WITH_ATTRIBUTES(testname, )

#define BEGIN_FRONTEND_TEST_WITH_ATTRIBUTES_AND_EXTRA(testname, attrs, extra) \
  class cls_##testname : public JSAPIFrontendTest {                           \
   public:                                                                    \
    virtual const char* name() override { return #testname; }                 \
    extra virtual bool run() override attrs

#define BEGIN_FRONTEND_TEST_WITH_ATTRIBUTES(testname, attrs) \
  BEGIN_FRONTEND_TEST_WITH_ATTRIBUTES_AND_EXTRA(testname, attrs, )

#define BEGIN_FRONTEND_TEST(testname) \
  BEGIN_FRONTEND_TEST_WITH_ATTRIBUTES(testname, )

#define BEGIN_REUSABLE_TEST(testname)   \
  BEGIN_TEST_WITH_ATTRIBUTES_AND_EXTRA( \
      testname, , cls_##testname()      \
      : JSAPIRuntimeTest() { reuseGlobal = true; })

#define END_TEST(testname) \
  }                        \
  ;                        \
  static cls_##testname cls_##testname##_instance;

/*
 * A "fixture" is a subclass of JSAPIRuntimeTest that holds common definitions
 * for a set of tests. Each test that wants to use the fixture should use
 * BEGIN_FIXTURE_TEST and END_FIXTURE_TEST, just as one would use BEGIN_TEST and
 * END_TEST, but include the fixture class as the first argument. The fixture
 * class's declarations are then in scope for the test bodies.
 */

#define BEGIN_FIXTURE_TEST(fixture, testname)                 \
  class cls_##testname : public fixture {                     \
   public:                                                    \
    virtual const char* name() override { return #testname; } \
    virtual bool run(JS::HandleObject global) override

#define END_FIXTURE_TEST(fixture, testname) \
  }                                         \
  ;                                         \
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
  TempFile() : name(), stream() {}
  ~TempFile() {
    if (stream) {
      close();
    }
    if (name) {
      remove();
    }
  }

  /*
   * Return a stream for a temporary file named |fileName|. Infallible.
   * Use only once per TempFile instance. If the file is not explicitly
   * closed and deleted via the member functions below, this object's
   * destructor will clean them up.
   */
  FILE* open(const char* fileName) {
    stream = fopen(fileName, "wb+");
    if (!stream) {
      fprintf(stderr, "error opening temporary file '%s': %s\n", fileName,
              strerror(errno));
      exit(1);
    }
    name = fileName;
    return stream;
  }

  /* Close the temporary file's stream. */
  void close() {
    if (fclose(stream) == EOF) {
      fprintf(stderr, "error closing temporary file '%s': %s\n", name,
              strerror(errno));
      exit(1);
    }
    stream = nullptr;
  }

  /* Delete the temporary file. */
  void remove() {
    if (::remove(name) != 0) {
      fprintf(stderr, "error deleting temporary file '%s': %s\n", name,
              strerror(errno));
      exit(1);
    }
    name = nullptr;
  }
};

// Just a wrapper around JSPrincipals that allows static construction.
class TestJSPrincipals : public JSPrincipals {
 public:
  explicit TestJSPrincipals(int rc = 0) : JSPrincipals() { refcount = rc; }

  bool write(JSContext* cx, JSStructuredCloneWriter* writer) override {
    MOZ_ASSERT(false, "not implemented");
    return false;
  }

  bool isSystemOrAddonPrincipal() override { return true; }
};

// A class that simulates externally memory-managed data, for testing with
// array buffers.
class ExternalData {
  char* contents_;
  size_t len_;
  bool uniquePointerCreated_ = false;

 public:
  explicit ExternalData(const char* str)
      : contents_(strdup(str)), len_(strlen(str) + 1) {}

  size_t len() const { return len_; }
  void* contents() const { return contents_; }
  char* asString() const { return contents_; }
  bool wasFreed() const { return !contents_; }

  void free() {
    MOZ_ASSERT(!wasFreed());
    ::free(contents_);
    contents_ = nullptr;
  }

  mozilla::UniquePtr<void, JS::BufferContentsDeleter> pointer() {
    MOZ_ASSERT(!uniquePointerCreated_,
               "Not allowed to create multiple unique pointers to contents");
    uniquePointerCreated_ = true;
    return {contents_, {ExternalData::freeCallback, this}};
  }

  static void freeCallback(void* contents, void* userData) {
    auto self = static_cast<ExternalData*>(userData);
    MOZ_ASSERT(self->contents() == contents);
    self->free();
  }
};

class AutoGCParameter {
  JSContext* cx_;
  JSGCParamKey key_;
  uint32_t value_;

 public:
  explicit AutoGCParameter(JSContext* cx, JSGCParamKey key, uint32_t value)
      : cx_(cx), key_(key), value_() {
    value_ = JS_GetGCParameter(cx, key);
    JS_SetGCParameter(cx, key, value);
  }
  ~AutoGCParameter() { JS_SetGCParameter(cx_, key_, value_); }
};

#ifdef JS_GC_ZEAL
/*
 * Temporarily disable the GC zeal setting. This is only useful in tests that
 * need very explicit GC behavior and should not be used elsewhere.
 */
class AutoLeaveZeal {
  JSContext* cx_;
  uint32_t zealBits_;
  uint32_t frequency_;

 public:
  explicit AutoLeaveZeal(JSContext* cx) : cx_(cx), zealBits_(0), frequency_(0) {
    uint32_t dummy;
    JS_GetGCZealBits(cx_, &zealBits_, &frequency_, &dummy);
    JS_SetGCZeal(cx_, 0, 0);
    JS::PrepareForFullGC(cx_);
    JS::NonIncrementalGC(cx_, JS::GCOptions::Normal, JS::GCReason::DEBUG_GC);
  }
  ~AutoLeaveZeal() {
    JS_SetGCZeal(cx_, 0, 0);
    for (size_t i = 0; i < sizeof(zealBits_) * 8; i++) {
      if (zealBits_ & (1 << i)) {
        JS_SetGCZeal(cx_, i, frequency_);
      }
    }

#  ifdef DEBUG
    uint32_t zealBitsAfter, frequencyAfter, dummy;
    JS_GetGCZealBits(cx_, &zealBitsAfter, &frequencyAfter, &dummy);
    MOZ_ASSERT(zealBitsAfter == zealBits_);
    MOZ_ASSERT(frequencyAfter == frequency_);
#  endif
  }
};

#else
class AutoLeaveZeal {
 public:
  explicit AutoLeaveZeal(JSContext* cx) {}
};
#endif

#endif /* jsapi_tests_tests_h */
