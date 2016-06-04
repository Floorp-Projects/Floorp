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
    char16_t* chars_;
    size_t length_;

  public:
    explicit AutoInflatedString(JSContext* cx) : cx(cx), chars_(nullptr), length_(0) { }
    ~AutoInflatedString() {
        JS_free(cx, chars_);
    }

    template<size_t N> void operator=(const char (&str)[N]) {
        length_ = N - 1;
        chars_ = InflateString(cx, str, &length_);
        if (!chars_)
            abort();
    }

    const char16_t* chars() const { return chars_; }
    size_t length() const { return length_; }
};

BEGIN_TEST(testParseJSON_success)
{
    // Primitives
    JS::RootedValue expected(cx);
    expected = JS::TrueValue();
    CHECK(TryParse(cx, "true", expected));

    expected = JS::FalseValue();
    CHECK(TryParse(cx, "false", expected));

    expected = JS::NullValue();
    CHECK(TryParse(cx, "null", expected));

    expected.setInt32(0);
    CHECK(TryParse(cx, "0", expected));

    expected.setInt32(1);
    CHECK(TryParse(cx, "1", expected));

    expected.setInt32(-1);
    CHECK(TryParse(cx, "-1", expected));

    expected.setDouble(1);
    CHECK(TryParse(cx, "1", expected));

    expected.setDouble(1.75);
    CHECK(TryParse(cx, "1.75", expected));

    expected.setDouble(9e9);
    CHECK(TryParse(cx, "9e9", expected));

    expected.setDouble(std::numeric_limits<double>::infinity());
    CHECK(TryParse(cx, "9e99999", expected));

    JS::Rooted<JSFlatString*> str(cx);

    const char16_t emptystr[] = { '\0' };
    str = js::NewStringCopyN<CanGC>(cx, emptystr, 0);
    CHECK(str);
    expected = JS::StringValue(str);
    CHECK(TryParse(cx, "\"\"", expected));

    const char16_t nullstr[] = { '\0' };
    str = NewString(cx, nullstr);
    CHECK(str);
    expected = JS::StringValue(str);
    CHECK(TryParse(cx, "\"\\u0000\"", expected));

    const char16_t backstr[] = { '\b' };
    str = NewString(cx, backstr);
    CHECK(str);
    expected = JS::StringValue(str);
    CHECK(TryParse(cx, "\"\\b\"", expected));
    CHECK(TryParse(cx, "\"\\u0008\"", expected));

    const char16_t newlinestr[] = { '\n', };
    str = NewString(cx, newlinestr);
    CHECK(str);
    expected = JS::StringValue(str);
    CHECK(TryParse(cx, "\"\\n\"", expected));
    CHECK(TryParse(cx, "\"\\u000A\"", expected));


    // Arrays
    JS::RootedValue v(cx), v2(cx);
    JS::RootedObject obj(cx);

    bool isArray;

    CHECK(Parse(cx, "[]", &v));
    CHECK(v.isObject());
    obj = &v.toObject();
    CHECK(JS_IsArrayObject(cx, obj, &isArray));
    CHECK(isArray);
    CHECK(JS_GetProperty(cx, obj, "length", &v2));
    CHECK(v2.isInt32(0));

    CHECK(Parse(cx, "[1]", &v));
    CHECK(v.isObject());
    obj = &v.toObject();
    CHECK(JS_IsArrayObject(cx, obj, &isArray));
    CHECK(isArray);
    CHECK(JS_GetProperty(cx, obj, "0", &v2));
    CHECK(v2.isInt32(1));
    CHECK(JS_GetProperty(cx, obj, "length", &v2));
    CHECK(v2.isInt32(1));


    // Objects
    CHECK(Parse(cx, "{}", &v));
    CHECK(v.isObject());
    obj = &v.toObject();
    CHECK(JS_IsArrayObject(cx, obj, &isArray));
    CHECK(!isArray);

    CHECK(Parse(cx, "{ \"f\": 17 }", &v));
    CHECK(v.isObject());
    obj = &v.toObject();
    CHECK(JS_IsArrayObject(cx, obj, &isArray));
    CHECK(!isArray);
    CHECK(JS_GetProperty(cx, obj, "f", &v2));
    CHECK(v2.isInt32(17));

    return true;
}

template<size_t N> static JSFlatString*
NewString(JSContext* cx, const char16_t (&chars)[N])
{
    return js::NewStringCopyN<CanGC>(cx, chars, N);
}

template<size_t N> inline bool
Parse(JSContext* cx, const char (&input)[N], JS::MutableHandleValue vp)
{
    AutoInflatedString str(cx);
    str = input;
    CHECK(JS_ParseJSON(cx, str.chars(), str.length(), vp));
    return true;
}

template<size_t N> inline bool
TryParse(JSContext* cx, const char (&input)[N], JS::HandleValue expected)
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
    CHECK(Error(cx, ""                                  , "1", "1"));
    CHECK(Error(cx, "\n"                                , "2", "1"));
    CHECK(Error(cx, "\r"                                , "2", "1"));
    CHECK(Error(cx, "\r\n"                              , "2", "1"));

    CHECK(Error(cx, "["                                 , "1", "2"));
    CHECK(Error(cx, "[,]"                               , "1", "2"));
    CHECK(Error(cx, "[1,]"                              , "1", "4"));
    CHECK(Error(cx, "{a:2}"                             , "1", "2"));
    CHECK(Error(cx, "{\"a\":2,}"                        , "1", "8"));
    CHECK(Error(cx, "]"                                 , "1", "1"));
    CHECK(Error(cx, "\""                                , "1", "2"));
    CHECK(Error(cx, "{]"                                , "1", "2"));
    CHECK(Error(cx, "[}"                                , "1", "2"));
    CHECK(Error(cx, "'wrongly-quoted string'"           , "1", "1"));

    CHECK(Error(cx, "{\"a\":2 \n b:3}"                  , "2", "2"));
    CHECK(Error(cx, "\n["                               , "2", "2"));
    CHECK(Error(cx, "\n[,]"                             , "2", "2"));
    CHECK(Error(cx, "\n[1,]"                            , "2", "4"));
    CHECK(Error(cx, "\n{a:2}"                           , "2", "2"));
    CHECK(Error(cx, "\n{\"a\":2,}"                      , "2", "8"));
    CHECK(Error(cx, "\n]"                               , "2", "1"));
    CHECK(Error(cx, "\"bad string\n\""                  , "1", "12"));
    CHECK(Error(cx, "\r'wrongly-quoted string'"         , "2", "1"));
    CHECK(Error(cx, "\n\""                              , "2", "2"));
    CHECK(Error(cx, "\n{]"                              , "2", "2"));
    CHECK(Error(cx, "\n[}"                              , "2", "2"));
    CHECK(Error(cx, "{\"a\":[2,3],\n\"b\":,5,6}"        , "2", "5"));

    CHECK(Error(cx, "{\"a\":2 \r b:3}"                  , "2", "2"));
    CHECK(Error(cx, "\r["                               , "2", "2"));
    CHECK(Error(cx, "\r[,]"                             , "2", "2"));
    CHECK(Error(cx, "\r[1,]"                            , "2", "4"));
    CHECK(Error(cx, "\r{a:2}"                           , "2", "2"));
    CHECK(Error(cx, "\r{\"a\":2,}"                      , "2", "8"));
    CHECK(Error(cx, "\r]"                               , "2", "1"));
    CHECK(Error(cx, "\"bad string\r\""                  , "1", "12"));
    CHECK(Error(cx, "\r'wrongly-quoted string'"       , "2", "1"));
    CHECK(Error(cx, "\r\""                              , "2", "2"));
    CHECK(Error(cx, "\r{]"                              , "2", "2"));
    CHECK(Error(cx, "\r[}"                              , "2", "2"));
    CHECK(Error(cx, "{\"a\":[2,3],\r\"b\":,5,6}"        , "2", "5"));

    CHECK(Error(cx, "{\"a\":2 \r\n b:3}"                , "2", "2"));
    CHECK(Error(cx, "\r\n["                             , "2", "2"));
    CHECK(Error(cx, "\r\n[,]"                           , "2", "2"));
    CHECK(Error(cx, "\r\n[1,]"                          , "2", "4"));
    CHECK(Error(cx, "\r\n{a:2}"                         , "2", "2"));
    CHECK(Error(cx, "\r\n{\"a\":2,}"                    , "2", "8"));
    CHECK(Error(cx, "\r\n]"                             , "2", "1"));
    CHECK(Error(cx, "\"bad string\r\n\""                , "1", "12"));
    CHECK(Error(cx, "\r\n'wrongly-quoted string'"       , "2", "1"));
    CHECK(Error(cx, "\r\n\""                            , "2", "2"));
    CHECK(Error(cx, "\r\n{]"                            , "2", "2"));
    CHECK(Error(cx, "\r\n[}"                            , "2", "2"));
    CHECK(Error(cx, "{\"a\":[2,3],\r\n\"b\":,5,6}"      , "2", "5"));

    CHECK(Error(cx, "\n\"bad string\n\""                , "2", "12"));
    CHECK(Error(cx, "\r\"bad string\r\""                , "2", "12"));
    CHECK(Error(cx, "\r\n\"bad string\r\n\""            , "2", "12"));

    CHECK(Error(cx, "{\n\"a\":[2,3],\r\"b\":,5,6}"      , "3", "5"));
    CHECK(Error(cx, "{\r\"a\":[2,3],\n\"b\":,5,6}"      , "3", "5"));
    CHECK(Error(cx, "[\"\\t\\q"                         , "1", "6"));
    CHECK(Error(cx, "[\"\\t\x00"                        , "1", "5"));
    CHECK(Error(cx, "[\"\\t\x01"                        , "1", "5"));
    CHECK(Error(cx, "[\"\\t\\\x00"                      , "1", "6"));
    CHECK(Error(cx, "[\"\\t\\\x01"                      , "1", "6"));

    // Unicode escape errors are messy.  The first bad character could be
    // non-hexadecimal, or it could be absent entirely.  Include tests where
    // there's a bad character, followed by zero to as many characters as are
    // needed to form a complete Unicode escape sequence, plus one.  (The extra
    // characters beyond are valuable because our implementation checks for
    // too-few subsequent characters first, before checking for subsequent
    // non-hexadecimal characters.  So \u<END>, \u0<END>, \u00<END>, and
    // \u000<END> are all *detected* as invalid by the same code path, but the
    // process of computing the first invalid character follows a different
    // code path for each.  And \uQQQQ, \u0QQQ, \u00QQ, and \u000Q are detected
    // as invalid by the same code path [ignoring which precise subexpression
    // triggers failure of a single condition], but the computation of the
    // first invalid character follows a different code path for each.)
    CHECK(Error(cx, "[\"\\t\\u"                         , "1", "7"));
    CHECK(Error(cx, "[\"\\t\\uZ"                        , "1", "7"));
    CHECK(Error(cx, "[\"\\t\\uZZ"                       , "1", "7"));
    CHECK(Error(cx, "[\"\\t\\uZZZ"                      , "1", "7"));
    CHECK(Error(cx, "[\"\\t\\uZZZZ"                     , "1", "7"));
    CHECK(Error(cx, "[\"\\t\\uZZZZZ"                    , "1", "7"));

    CHECK(Error(cx, "[\"\\t\\u0"                        , "1", "8"));
    CHECK(Error(cx, "[\"\\t\\u0Z"                       , "1", "8"));
    CHECK(Error(cx, "[\"\\t\\u0ZZ"                      , "1", "8"));
    CHECK(Error(cx, "[\"\\t\\u0ZZZ"                     , "1", "8"));
    CHECK(Error(cx, "[\"\\t\\u0ZZZZ"                    , "1", "8"));

    CHECK(Error(cx, "[\"\\t\\u00"                       , "1", "9"));
    CHECK(Error(cx, "[\"\\t\\u00Z"                      , "1", "9"));
    CHECK(Error(cx, "[\"\\t\\u00ZZ"                     , "1", "9"));
    CHECK(Error(cx, "[\"\\t\\u00ZZZ"                    , "1", "9"));

    CHECK(Error(cx, "[\"\\t\\u000"                      , "1", "10"));
    CHECK(Error(cx, "[\"\\t\\u000Z"                     , "1", "10"));
    CHECK(Error(cx, "[\"\\t\\u000ZZ"                    , "1", "10"));

    return true;
}

template<size_t N, size_t M, size_t L> inline bool
Error(JSContext* cx, const char (&input)[N], const char (&expectedLine)[M],
      const char (&expectedColumn)[L])
{
    AutoInflatedString str(cx), line(cx), column(cx);
    RootedValue dummy(cx);
    str = input;

    bool ok = JS_ParseJSON(cx, str.chars(), str.length(), &dummy);
    CHECK(!ok);

    RootedValue exn(cx);
    CHECK(JS_GetPendingException(cx, &exn));
    JS_ClearPendingException(cx);

    js::ErrorReport report(cx);
    CHECK(report.init(cx, exn, js::ErrorReport::WithSideEffects));
    CHECK(report.report()->errorNumber == JSMSG_JSON_BAD_PARSE);

    column = expectedColumn;
    CHECK(js_strcmp(column.chars(), report.report()->messageArgs[2]) == 0);
    line = expectedLine;
    CHECK(js_strcmp(line.chars(), report.report()->messageArgs[1]) == 0);

    /* We do not execute JS, so there should be no exception thrown. */
    CHECK(!JS_IsExceptionPending(cx));

    return true;
}
END_TEST(testParseJSON_error)

static bool
Censor(JSContext* cx, unsigned argc, JS::Value* vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    MOZ_RELEASE_ASSERT(args.length() == 2);
    MOZ_RELEASE_ASSERT(args[0].isString());
    args.rval().setNull();
    return true;
}

BEGIN_TEST(testParseJSON_reviver)
{
    JSFunction* fun = JS_NewFunction(cx, Censor, 0, 0, "censor");
    CHECK(fun);

    JS::RootedValue filter(cx, JS::ObjectValue(*JS_GetFunctionObject(fun)));

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
TryParse(JSContext* cx, const char (&input)[N], JS::HandleValue filter)
{
    AutoInflatedString str(cx);
    JS::RootedValue v(cx);
    str = input;
    CHECK(JS_ParseJSONWithReviver(cx, str.chars(), str.length(), filter, &v));
    CHECK(v.isNull());
    return true;
}
END_TEST(testParseJSON_reviver)
