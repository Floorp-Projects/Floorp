/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <limits>

#include "jsstr.h"

#include "jsapi-tests/tests.h"

using namespace js;

class AutoInflatedString {
    JSContext * const cx;
    jschar *chars_;
    size_t length_;

  public:
    AutoInflatedString(JSContext *cx) : cx(cx), chars_(nullptr), length_(0) { }
    ~AutoInflatedString() {
        JS_free(cx, chars_);
    }

    template<size_t N> void operator=(const char (&str)[N]) {
        length_ = N - 1;
        chars_ = InflateString(cx, str, &length_);
        if (!chars_)
            abort();
    }

    const jschar *chars() const { return chars_; }
    size_t length() const { return length_; }
};

template<size_t N> JSFlatString *
NewString(JSContext *cx, const jschar (&chars)[N])
{
    return js_NewStringCopyN<CanGC>(cx, chars, N);
}

BEGIN_TEST(testParseJSON_success)
{
    // Primitives
    JS::RootedValue expected(cx);
    expected = JSVAL_TRUE;
    CHECK(TryParse(cx, "true", expected));

    expected = JSVAL_FALSE;
    CHECK(TryParse(cx, "false", expected));

    expected = JSVAL_NULL;
    CHECK(TryParse(cx, "null", expected));

    expected = INT_TO_JSVAL(0);
    CHECK(TryParse(cx, "0", expected));

    expected = INT_TO_JSVAL(1);
    CHECK(TryParse(cx, "1", expected));

    expected = INT_TO_JSVAL(-1);
    CHECK(TryParse(cx, "-1", expected));

    expected = DOUBLE_TO_JSVAL(1);
    CHECK(TryParse(cx, "1", expected));

    expected = DOUBLE_TO_JSVAL(1.75);
    CHECK(TryParse(cx, "1.75", expected));

    expected = DOUBLE_TO_JSVAL(9e9);
    CHECK(TryParse(cx, "9e9", expected));

    expected = DOUBLE_TO_JSVAL(std::numeric_limits<double>::infinity());
    CHECK(TryParse(cx, "9e99999", expected));

    JS::Rooted<JSFlatString*> str(cx);

    const jschar emptystr[] = { '\0' };
    str = js_NewStringCopyN<CanGC>(cx, emptystr, 0);
    CHECK(str);
    expected = STRING_TO_JSVAL(str);
    CHECK(TryParse(cx, "\"\"", expected));

    const jschar nullstr[] = { '\0' };
    str = NewString(cx, nullstr);
    CHECK(str);
    expected = STRING_TO_JSVAL(str);
    CHECK(TryParse(cx, "\"\\u0000\"", expected));

    const jschar backstr[] = { '\b' };
    str = NewString(cx, backstr);
    CHECK(str);
    expected = STRING_TO_JSVAL(str);
    CHECK(TryParse(cx, "\"\\b\"", expected));
    CHECK(TryParse(cx, "\"\\u0008\"", expected));

    const jschar newlinestr[] = { '\n', };
    str = NewString(cx, newlinestr);
    CHECK(str);
    expected = STRING_TO_JSVAL(str);
    CHECK(TryParse(cx, "\"\\n\"", expected));
    CHECK(TryParse(cx, "\"\\u000A\"", expected));


    // Arrays
    JS::RootedValue v(cx), v2(cx);
    JS::RootedObject obj(cx);

    CHECK(Parse(cx, "[]", &v));
    CHECK(!JSVAL_IS_PRIMITIVE(v));
    obj = JSVAL_TO_OBJECT(v);
    CHECK(JS_IsArrayObject(cx, obj));
    CHECK(JS_GetProperty(cx, obj, "length", &v2));
    CHECK_SAME(v2, JSVAL_ZERO);

    CHECK(Parse(cx, "[1]", &v));
    CHECK(!JSVAL_IS_PRIMITIVE(v));
    obj = JSVAL_TO_OBJECT(v);
    CHECK(JS_IsArrayObject(cx, obj));
    CHECK(JS_GetProperty(cx, obj, "0", &v2));
    CHECK_SAME(v2, JSVAL_ONE);
    CHECK(JS_GetProperty(cx, obj, "length", &v2));
    CHECK_SAME(v2, JSVAL_ONE);


    // Objects
    CHECK(Parse(cx, "{}", &v));
    CHECK(!JSVAL_IS_PRIMITIVE(v));
    obj = JSVAL_TO_OBJECT(v);
    CHECK(!JS_IsArrayObject(cx, obj));

    CHECK(Parse(cx, "{ \"f\": 17 }", &v));
    CHECK(!JSVAL_IS_PRIMITIVE(v));
    obj = JSVAL_TO_OBJECT(v);
    CHECK(!JS_IsArrayObject(cx, obj));
    CHECK(JS_GetProperty(cx, obj, "f", &v2));
    CHECK_SAME(v2, INT_TO_JSVAL(17));

    return true;
}

template<size_t N> inline bool
Parse(JSContext *cx, const char (&input)[N], JS::MutableHandleValue vp)
{
    AutoInflatedString str(cx);
    str = input;
    CHECK(JS_ParseJSON(cx, str.chars(), str.length(), vp));
    return true;
}

template<size_t N> inline bool
TryParse(JSContext *cx, const char (&input)[N], JS::HandleValue expected)
{
    AutoInflatedString str(cx);
    RootedValue v(cx);
    str = input;
    CHECK(JS_ParseJSON(cx, str.chars(), str.length(), &v));
    CHECK_SAME(v, expected);
    return true;
}
END_TEST(testParseJSON_success)

BEGIN_TEST(testParseJSON_error)
{
    CHECK(Error(cx, "["));
    CHECK(Error(cx, "[,]"));
    CHECK(Error(cx, "[1,]"));
    CHECK(Error(cx, "{a:2}"));
    CHECK(Error(cx, "{\"a\":2,}"));
    CHECK(Error(cx, "]"));
    CHECK(Error(cx, "'bad string'"));
    CHECK(Error(cx, "\""));
    CHECK(Error(cx, "{]"));
    CHECK(Error(cx, "[}"));
    return true;
}

template<size_t N> inline bool
Error(JSContext *cx, const char (&input)[N])
{
    AutoInflatedString str(cx);
    RootedValue dummy(cx);
    str = input;

    ContextPrivate p = {0, 0};
    CHECK(!JS_GetContextPrivate(cx));
    JS_SetContextPrivate(cx, &p);
    JSErrorReporter old = JS_SetErrorReporter(cx, reportJSONEror);
    bool ok = JS_ParseJSON(cx, str.chars(), str.length(), &dummy);
    JS_SetErrorReporter(cx, old);
    JS_SetContextPrivate(cx, nullptr);

    CHECK(!ok);
    CHECK(!p.unexpectedErrorCount);
    CHECK(p.expectedErrorCount == 1);

    /* We do not execute JS, so there should be no exception thrown. */
    CHECK(!JS_IsExceptionPending(cx));

    return true;
}

struct ContextPrivate {
    unsigned unexpectedErrorCount;
    unsigned expectedErrorCount;
};

static void
reportJSONEror(JSContext *cx, const char *message, JSErrorReport *report)
{
    ContextPrivate *p = static_cast<ContextPrivate *>(JS_GetContextPrivate(cx));
    if (report->errorNumber == JSMSG_JSON_BAD_PARSE)
        p->expectedErrorCount++;
    else
        p->unexpectedErrorCount++;
}

END_TEST(testParseJSON_error)

static bool
Censor(JSContext *cx, unsigned argc, jsval *vp)
{
    JS_ASSERT(argc == 2);
#ifdef DEBUG
    jsval *argv = JS_ARGV(cx, vp);
    JS_ASSERT(JSVAL_IS_STRING(argv[0]));
#endif
    JS_SET_RVAL(cx, vp, JSVAL_NULL);
    return true;
}

BEGIN_TEST(testParseJSON_reviver)
{
    JSFunction *fun = JS_NewFunction(cx, Censor, 0, 0, global, "censor");
    CHECK(fun);

    JS::RootedValue filter(cx, OBJECT_TO_JSVAL(JS_GetFunctionObject(fun)));

    CHECK(TryParse(cx, "true", filter));
    CHECK(TryParse(cx, "false", filter));
    CHECK(TryParse(cx, "null", filter));
    CHECK(TryParse(cx, "1", filter));
    CHECK(TryParse(cx, "1.75", filter));
    CHECK(TryParse(cx, "[]", filter));
    CHECK(TryParse(cx, "[1]", filter));
    CHECK(TryParse(cx, "{}", filter));
    return true;
}

template<size_t N> inline bool
TryParse(JSContext *cx, const char (&input)[N], JS::HandleValue filter)
{
    AutoInflatedString str(cx);
    JS::RootedValue v(cx);
    str = input;
    CHECK(JS_ParseJSONWithReviver(cx, str.chars(), str.length(), filter, &v));
    CHECK_SAME(v, JSVAL_NULL);
    return true;
}
END_TEST(testParseJSON_reviver)
