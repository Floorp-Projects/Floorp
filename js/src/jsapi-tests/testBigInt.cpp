/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Range.h"  // mozilla::Range
#include "mozilla/Span.h"   // mozilla::MakeStringSpan

#include <stdint.h>

#include "jsapi.h"        // JS_IsExceptionPending, JS_StringEqualsLiteral

#include "js/BigInt.h"  // JS::{,Number,String,SimpleString}ToBigInt, JS::ToBig{I,Ui}nt64
#include "js/CharacterEncoding.h"  // JS::Const{Latin1,TwoByte}Chars
#include "js/Conversions.h"        // JS::ToString
#include "js/ErrorReport.h"        // JS::ErrorReportBuilder, JSEXN_SYNTAXERR
#include "js/Exception.h"          // JS::StealPendingExceptionStack
#include "js/friend/ErrorMessages.h"  // JSMSG_*
#include "js/RootingAPI.h"         // JS::Rooted
#include "js/Value.h"              // JS::FalseValue, JS::Value

#include "jsapi-tests/tests.h"
#include "util/Text.h"  // js::InflateString

struct JS_PUBLIC_API JSContext;
class JS_PUBLIC_API JSString;

namespace JS {

class JS_PUBLIC_API BigInt;

}  // namespace JS

BEGIN_TEST(testToBigInt64) {
  JS::Rooted<JS::Value> v(cx);

  EVAL("0n", &v);
  CHECK(v.isBigInt());
  CHECK(JS::ToBigInt64(v.toBigInt()) == 0);

  EVAL("9223372036854775807n", &v);
  CHECK(v.isBigInt());
  CHECK(JS::ToBigInt64(v.toBigInt()) == 9223372036854775807L);

  EVAL("-9223372036854775808n", &v);
  CHECK(v.isBigInt());
  CHECK(JS::ToBigInt64(v.toBigInt()) == -9223372036854775807L - 1L);

  return true;
}
END_TEST(testToBigInt64)

BEGIN_TEST(testToBigUint64) {
  JS::Rooted<JS::Value> v(cx);

  EVAL("0n", &v);
  CHECK(v.isBigInt());
  CHECK(JS::ToBigUint64(v.toBigInt()) == 0);

  EVAL("18446744073709551615n", &v);
  CHECK(v.isBigInt());
  CHECK(JS::ToBigUint64(v.toBigInt()) == 18446744073709551615UL);

  return true;
}
END_TEST(testToBigUint64)

#define GENERATE_INTTYPE_TEST(bits)             \
  BEGIN_TEST(testNumberToBigInt_Int##bits) {    \
    int##bits##_t i = INT##bits##_MIN;          \
    JS::BigInt* bi = JS::NumberToBigInt(cx, i); \
    CHECK(bi);                                  \
    CHECK(JS::ToBigInt64(bi) == i);             \
                                                \
    i = INT##bits##_MAX;                        \
    bi = JS::NumberToBigInt(cx, i);             \
    CHECK(bi);                                  \
    CHECK(JS::ToBigInt64(bi) == i);             \
                                                \
    uint##bits##_t u = 0;                       \
    bi = JS::NumberToBigInt(cx, u);             \
    CHECK(bi);                                  \
    CHECK(JS::ToBigUint64(bi) == 0);            \
                                                \
    u = UINT##bits##_MAX;                       \
    bi = JS::NumberToBigInt(cx, u);             \
    CHECK(bi);                                  \
    CHECK(JS::ToBigUint64(bi) == u);            \
                                                \
    return true;                                \
  }                                             \
  END_TEST(testNumberToBigInt_Int##bits)

GENERATE_INTTYPE_TEST(8);
GENERATE_INTTYPE_TEST(16);
GENERATE_INTTYPE_TEST(32);
GENERATE_INTTYPE_TEST(64);

#undef GENERATE_INTTYPE_TEST

#define GENERATE_SIGNED_VALUE_TEST(type, tag, val) \
  BEGIN_TEST(testNumberToBigInt_##type##_##tag) {  \
    type v = val;                                  \
    JS::BigInt* bi = JS::NumberToBigInt(cx, v);    \
    CHECK(bi);                                     \
    CHECK(JS::ToBigInt64(bi) == (val));            \
    return true;                                   \
  }                                                \
  END_TEST(testNumberToBigInt_##type##_##tag)

#define GENERATE_UNSIGNED_VALUE_TEST(type, tag, val) \
  BEGIN_TEST(testNumberToBigInt_##type##_##tag) {    \
    type v = val;                                    \
    JS::BigInt* bi = JS::NumberToBigInt(cx, v);      \
    CHECK(bi);                                       \
    CHECK(JS::ToBigUint64(bi) == (val));             \
    return true;                                     \
  }                                                  \
  END_TEST(testNumberToBigInt_##type##_##tag)

GENERATE_SIGNED_VALUE_TEST(int, zero, 0);
GENERATE_SIGNED_VALUE_TEST(int, aValue, -42);
GENERATE_UNSIGNED_VALUE_TEST(unsigned, zero, 0);
GENERATE_UNSIGNED_VALUE_TEST(unsigned, aValue, 42);
GENERATE_SIGNED_VALUE_TEST(long, zero, 0);
GENERATE_SIGNED_VALUE_TEST(long, aValue, -42);
GENERATE_UNSIGNED_VALUE_TEST(uintptr_t, zero, 0);
GENERATE_UNSIGNED_VALUE_TEST(uintptr_t, aValue, 42);
GENERATE_UNSIGNED_VALUE_TEST(size_t, zero, 0);
GENERATE_UNSIGNED_VALUE_TEST(size_t, aValue, 42);
GENERATE_SIGNED_VALUE_TEST(double, zero, 0);
GENERATE_SIGNED_VALUE_TEST(double, aValue, -42);

#undef GENERATE_SIGNED_VALUE_TEST
#undef GENERATE_UNSIGNED_VALUE_TEST

BEGIN_TEST(testNumberToBigInt_bool) {
  JS::BigInt* bi = JS::NumberToBigInt(cx, true);
  CHECK(bi);
  CHECK(JS::ToBigUint64(bi) == 1);

  bi = JS::NumberToBigInt(cx, false);
  CHECK(bi);
  CHECK(JS::ToBigUint64(bi) == 0);

  return true;
}
END_TEST(testNumberToBigInt_bool)

BEGIN_TEST(testNumberToBigInt_NonIntegerValueFails) {
  JS::BigInt* bi = JS::NumberToBigInt(cx, 3.1416);
  CHECK_NULL(bi);
  CHECK(JS_IsExceptionPending(cx));
  JS_ClearPendingException(cx);
  return true;
}
END_TEST(testNumberToBigInt_NonIntegerValueFails)

BEGIN_TEST(testStringToBigInt_FromTwoByteStringSpan) {
  mozilla::Range<const char16_t> input{
      mozilla::MakeStringSpan(u"18446744073709551616")};
  JS::BigInt* bi = JS::StringToBigInt(cx, input);
  CHECK(bi);
  JS::Rooted<JS::Value> val(cx, JS::BigIntValue(bi));
  JS::Rooted<JSString*> str(cx, JS::ToString(cx, val));
  CHECK(str);
  bool match;
  CHECK(JS_StringEqualsLiteral(cx, str, "18446744073709551616", &match));
  CHECK(match);
  return true;
}
END_TEST(testStringToBigInt_FromTwoByteStringSpan)

BEGIN_TEST(testStringToBigInt_FromLatin1Range) {
  const JS::Latin1Char string[] = "12345 and some junk at the end";
  JS::ConstLatin1Chars range(string, 5);
  JS::BigInt* bi = JS::StringToBigInt(cx, range);
  CHECK(bi);
  CHECK(JS::ToBigInt64(bi) == 12345);
  return true;
}
END_TEST(testStringToBigInt_FromLatin1Range)

BEGIN_TEST(testStringToBigInt_FromTwoByteRange) {
  const char16_t string[] = u"12345 and some junk at the end";
  JS::ConstTwoByteChars range(string, 5);
  JS::BigInt* bi = JS::StringToBigInt(cx, range);
  CHECK(bi);
  CHECK(JS::ToBigInt64(bi) == 12345);
  return true;
}
END_TEST(testStringToBigInt_FromTwoByteRange)

BEGIN_TEST(testStringToBigInt_AcceptedInput) {
  CHECK(Allowed(u"", 0));
  CHECK(Allowed(u"\n", 0));
  CHECK(Allowed(u" ", 0));
  CHECK(Allowed(u"0\n", 0));
  CHECK(Allowed(u"0 ", 0));
  CHECK(Allowed(u"\n1", 1));
  CHECK(Allowed(u" 1", 1));
  CHECK(Allowed(u"\n2 ", 2));
  CHECK(Allowed(u" 2\n", 2));
  CHECK(Allowed(u"0b11", 3));
  CHECK(Allowed(u"0x17", 23));
  CHECK(Allowed(u"-5", -5));
  CHECK(Allowed(u"+5", 5));
  CHECK(Allowed(u"-0", 0));

  CHECK(Fails(u"!!!!!!111one1111one1!1!1!!"));
  CHECK(Fails(u"3.1416"));
  CHECK(Fails(u"6.022e23"));
  CHECK(Fails(u"1e3"));
  CHECK(Fails(u".25"));
  CHECK(Fails(u".25e2"));
  CHECK(Fails(u"1_000_000"));
  CHECK(Fails(u"3n"));
  CHECK(Fails(u"-0x3"));
  CHECK(Fails(u"Infinity"));

  return true;
}

template <size_t N>
inline bool Allowed(const char16_t (&str)[N], int64_t expected) {
  JS::BigInt* bi = JS::StringToBigInt(cx, mozilla::MakeStringSpan(str));
  CHECK(bi);
  CHECK(JS::ToBigInt64(bi) == expected);
  return true;
}

template <size_t N>
inline bool Fails(const char16_t (&str)[N]) {
  JS::BigInt* bi = JS::StringToBigInt(cx, mozilla::MakeStringSpan(str));
  CHECK_NULL(bi);
  CHECK(JS_IsExceptionPending(cx));

  JS::ExceptionStack exnStack(cx);
  CHECK(JS::StealPendingExceptionStack(cx, &exnStack));

  JS::ErrorReportBuilder report(cx);
  CHECK(report.init(cx, exnStack, JS::ErrorReportBuilder::NoSideEffects));
  CHECK(report.report()->exnType == JSEXN_SYNTAXERR);
  CHECK(report.report()->errorNumber == JSMSG_BIGINT_INVALID_SYNTAX);

  CHECK(!JS_IsExceptionPending(cx));

  return true;
}
END_TEST(testStringToBigInt_AcceptedInput)

BEGIN_TEST(testSimpleStringToBigInt_AcceptedInput) {
  CHECK(Allowed("12345", 10, 12345));
  CHECK(Allowed("+12345", 10, 12345));
  CHECK(Allowed("-12345", 10, -12345));
  CHECK(Allowed("775", 8, 0775));
  CHECK(Allowed("+775", 8, 0775));
  CHECK(Allowed("-775", 8, -0775));
  CHECK(Allowed("cAfE", 16, 0xCAFE));
  CHECK(Allowed("+cAfE", 16, +0xCAFE));
  CHECK(Allowed("-cAfE", 16, -0xCAFE));
  CHECK(Allowed("-0", 10, 0));

  CHECK(Fails("", 10));
  CHECK(Fails("\n", 10));
  CHECK(Fails(" ", 10));
  CHECK(Fails("0\n", 10));
  CHECK(Fails("0 ", 10));
  CHECK(Fails("\n1", 10));
  CHECK(Fails(" 1", 10));
  CHECK(Fails("\n2 ", 10));
  CHECK(Fails(" 2\n", 10));
  CHECK(Fails("0b11", 2));
  CHECK(Fails("0x17", 16));
  CHECK(Fails("!!!!!!111one1111one1!1!1!!", 10));
  CHECK(Fails("3.1416", 10));
  CHECK(Fails("6.022e23", 10));
  CHECK(Fails("1e3", 10));
  CHECK(Fails(".25", 10));
  CHECK(Fails(".25e2", 10));
  CHECK(Fails("1_000_000", 10));
  CHECK(Fails("3n", 10));
  CHECK(Fails("-0x3", 10));
  CHECK(Fails("Infinity", 10));
  CHECK(Fails("555", 4));
  CHECK(Fails("fff", 15));

  return true;
}

template <size_t N>
inline bool Allowed(const char (&str)[N], unsigned radix, int64_t expected) {
  JS::BigInt* bi = JS::SimpleStringToBigInt(cx, {str, N - 1}, radix);
  CHECK(bi);
  CHECK(JS::ToBigInt64(bi) == expected);
  return true;
}

template <size_t N>
inline bool Fails(const char (&str)[N], unsigned radix) {
  JS::BigInt* bi = JS::SimpleStringToBigInt(cx, {str, N - 1}, radix);
  CHECK_NULL(bi);
  CHECK(JS_IsExceptionPending(cx));

  JS::ExceptionStack exnStack(cx);
  CHECK(JS::StealPendingExceptionStack(cx, &exnStack));

  JS::ErrorReportBuilder report(cx);
  CHECK(report.init(cx, exnStack, JS::ErrorReportBuilder::NoSideEffects));
  CHECK(report.report()->exnType == JSEXN_SYNTAXERR);
  CHECK(report.report()->errorNumber == JSMSG_BIGINT_INVALID_SYNTAX);

  CHECK(!JS_IsExceptionPending(cx));

  return true;
}
END_TEST(testSimpleStringToBigInt_AcceptedInput)

BEGIN_TEST(testSimpleStringToBigInt_AllPossibleDigits) {
  const char allPossible[] =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
  JS::BigInt* bi =
      JS::SimpleStringToBigInt(cx, mozilla::MakeStringSpan(allPossible), 36);
  CHECK(bi);
  JS::Rooted<JS::Value> val(cx, JS::BigIntValue(bi));
  JS::Rooted<JSString*> str(cx, JS::ToString(cx, val));
  CHECK(str);

  // Answer calculated using Python:
  // int('abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890', 36)
  // Do not trust online base-36 calculators for values > UINT32_MAX!
  bool match;
  CHECK(
      JS_StringEqualsLiteral(cx, str,
                             "8870050151210747660007771095260505028056221996735"
                             "67534007158336222790086855213834764150805438340",
                             &match));
  CHECK(match);
  return true;
}
END_TEST(testSimpleStringToBigInt_AllPossibleDigits)

BEGIN_TEST(testSimpleStringToBigInt_RadixOutOfRange) {
  CHECK(RadixOutOfRange(1));
  CHECK(RadixOutOfRange(37));
  return true;
}

inline bool RadixOutOfRange(unsigned radix) {
  JS::BigInt* bi =
      JS::SimpleStringToBigInt(cx, mozilla::MakeStringSpan("1"), radix);
  CHECK_NULL(bi);
  CHECK(JS_IsExceptionPending(cx));

  JS::ExceptionStack exnStack(cx);
  CHECK(JS::StealPendingExceptionStack(cx, &exnStack));

  JS::ErrorReportBuilder report(cx);
  CHECK(report.init(cx, exnStack, JS::ErrorReportBuilder::NoSideEffects));
  CHECK(report.report()->exnType == JSEXN_RANGEERR);
  CHECK(report.report()->errorNumber == JSMSG_BAD_RADIX);

  CHECK(!JS_IsExceptionPending(cx));

  return true;
}
END_TEST(testSimpleStringToBigInt_RadixOutOfRange)

BEGIN_TEST(testToBigInt_Undefined) {
  JS::Rooted<JS::Value> v(cx, JS::UndefinedValue());
  JS::BigInt* bi = JS::ToBigInt(cx, v);
  CHECK_NULL(bi);
  CHECK(JS_IsExceptionPending(cx));
  JS_ClearPendingException(cx);
  return true;
}
END_TEST(testToBigInt_Undefined)

BEGIN_TEST(testToBigInt_Null) {
  JS::Rooted<JS::Value> v(cx, JS::NullValue());
  JS::BigInt* bi = JS::ToBigInt(cx, v);
  CHECK_NULL(bi);
  CHECK(JS_IsExceptionPending(cx));
  JS_ClearPendingException(cx);
  return true;
}
END_TEST(testToBigInt_Null)

BEGIN_TEST(testToBigInt_Boolean) {
  JS::Rooted<JS::Value> v(cx, JS::TrueValue());
  JS::BigInt* bi = JS::ToBigInt(cx, v);
  CHECK(bi);
  CHECK(JS::ToBigInt64(bi) == 1);

  v = JS::FalseValue();
  bi = JS::ToBigInt(cx, v);
  CHECK(bi);
  CHECK(JS::ToBigInt64(bi) == 0);

  return true;
}
END_TEST(testToBigInt_Boolean)

BEGIN_TEST(testToBigInt_BigInt) {
  JS::Rooted<JS::Value> v(cx);
  EVAL("42n", &v);
  JS::BigInt* bi = JS::ToBigInt(cx, v);
  CHECK(bi);
  CHECK(JS::ToBigInt64(bi) == 42);
  return true;
}
END_TEST(testToBigInt_BigInt)

BEGIN_TEST(testToBigInt_Number) {
  JS::Rooted<JS::Value> v(cx, JS::Int32Value(42));
  JS::BigInt* bi = JS::ToBigInt(cx, v);
  CHECK_NULL(bi);
  CHECK(JS_IsExceptionPending(cx));
  JS_ClearPendingException(cx);
  return true;
}
END_TEST(testToBigInt_Number)

BEGIN_TEST(testToBigInt_String) {
  JS::Rooted<JS::Value> v(cx);
  EVAL("'42'", &v);
  JS::BigInt* bi = JS::ToBigInt(cx, v);
  CHECK(bi);
  CHECK(JS::ToBigInt64(bi) == 42);
  return true;
}
END_TEST(testToBigInt_String)

BEGIN_TEST(testToBigInt_Symbol) {
  JS::Rooted<JS::Value> v(cx);
  EVAL("Symbol.toStringTag", &v);
  JS::BigInt* bi = JS::ToBigInt(cx, v);
  CHECK_NULL(bi);
  CHECK(JS_IsExceptionPending(cx));
  JS_ClearPendingException(cx);
  return true;
}
END_TEST(testToBigInt_Symbol)
