/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/FloatingPoint.h"  // mozilla::NumberIsInt32
#include "mozilla/Range.h"          // mozilla::Range
#include "mozilla/Span.h"           // mozilla::MakeStringSpan

#include <stdint.h>

#include "js/BigInt.h"  // JS::{,Number,String,SimpleString}ToBigInt, JS::ToBig{I,Ui}nt64
#include "js/CharacterEncoding.h"  // JS::Const{Latin1,TwoByte}Chars
#include "js/Conversions.h"        // JS::ToString
#include "js/ErrorReport.h"        // JS::ErrorReportBuilder, JSEXN_SYNTAXERR
#include "js/Exception.h"  // JS::StealPendingExceptionStack, JS_IsExceptionPending
#include "js/friend/ErrorMessages.h"  // JSMSG_*
#include "js/RootingAPI.h"            // JS::Rooted
#include "js/String.h"                // JS_StringEqualsLiteral
#include "js/Value.h"                 // JS::FalseValue, JS::Value

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

BEGIN_TEST(testBigIntToNumber) {
  JS::BigInt* bi = JS::NumberToBigInt(cx, 0);
  CHECK(bi);
  int32_t result;
  CHECK(mozilla::NumberIsInt32(JS::BigIntToNumber(bi), &result));
  CHECK_EQUAL(result, 0);

  bi = JS::NumberToBigInt(cx, 100);
  CHECK(bi);
  CHECK(JS::BigIntToNumber(bi) == 100);

  bi = JS::NumberToBigInt(cx, -100);
  CHECK(bi);
  CHECK(JS::BigIntToNumber(bi) == -100);

  JS::Rooted<JS::Value> v(cx);

  EVAL("18446744073709551615n", &v);
  CHECK(v.isBigInt());
  double numberValue = JS::BigIntToNumber(v.toBigInt());
  EVAL("Number(18446744073709551615n)", &v);
  CHECK(v.isNumber());
  CHECK(numberValue == v.toNumber());

  EVAL((std::string(500, '9') + "n").c_str(), &v);
  CHECK(v.isBigInt());
  CHECK(JS::BigIntToNumber(v.toBigInt()) == INFINITY);

  return true;
}
END_TEST(testBigIntToNumber)

BEGIN_TEST(testBigIntIsNegative) {
  JS::BigInt* bi = JS::NumberToBigInt(cx, 0);
  CHECK(bi);
  CHECK(!JS::BigIntIsNegative(bi));

  bi = JS::NumberToBigInt(cx, 100);
  CHECK(bi);
  CHECK(!JS::BigIntIsNegative(bi));

  bi = JS::NumberToBigInt(cx, -100);
  CHECK(bi);
  CHECK(JS::BigIntIsNegative(bi));

  return true;
}
END_TEST(testBigIntIsNegative)

#define GENERATE_INTTYPE_TEST(bits)              \
  BEGIN_TEST(testBigIntFits_Int##bits) {         \
    int64_t in = INT##bits##_MIN;                \
    JS::BigInt* bi = JS::NumberToBigInt(cx, in); \
    CHECK(bi);                                   \
    int##bits##_t i;                             \
    CHECK(JS::BigIntFits(bi, &i));               \
    CHECK_EQUAL(i, in);                          \
                                                 \
    in = int64_t(INT##bits##_MIN) - 1;           \
    bi = JS::NumberToBigInt(cx, in);             \
    CHECK(bi);                                   \
    CHECK(!JS::BigIntFits(bi, &i));              \
                                                 \
    in = INT64_MIN;                              \
    bi = JS::NumberToBigInt(cx, in);             \
    CHECK(bi);                                   \
    CHECK(!JS::BigIntFits(bi, &i));              \
                                                 \
    in = INT##bits##_MAX;                        \
    bi = JS::NumberToBigInt(cx, in);             \
    CHECK(bi);                                   \
    CHECK(JS::BigIntFits(bi, &i));               \
    CHECK_EQUAL(i, in);                          \
                                                 \
    in = int64_t(INT##bits##_MAX) + 1;           \
    bi = JS::NumberToBigInt(cx, in);             \
    CHECK(bi);                                   \
    CHECK(!JS::BigIntFits(bi, &i));              \
                                                 \
    in = INT64_MAX;                              \
    bi = JS::NumberToBigInt(cx, in);             \
    CHECK(bi);                                   \
    CHECK(!JS::BigIntFits(bi, &i));              \
                                                 \
    uint64_t uin = 0;                            \
    bi = JS::NumberToBigInt(cx, uin);            \
    CHECK(bi);                                   \
    uint##bits##_t u;                            \
    CHECK(JS::BigIntFits(bi, &u));               \
    CHECK_EQUAL(u, uin);                         \
                                                 \
    uin = UINT##bits##_MAX;                      \
    bi = JS::NumberToBigInt(cx, uin);            \
    CHECK(bi);                                   \
    CHECK(JS::BigIntFits(bi, &u));               \
    CHECK_EQUAL(u, uin);                         \
                                                 \
    uin = uint64_t(UINT##bits##_MAX) + 1;        \
    bi = JS::NumberToBigInt(cx, uin);            \
    CHECK(bi);                                   \
    CHECK(!JS::BigIntFits(bi, &u));              \
                                                 \
    uin = UINT64_MAX;                            \
    bi = JS::NumberToBigInt(cx, uin);            \
    CHECK(bi);                                   \
    CHECK(!JS::BigIntFits(bi, &u));              \
                                                 \
    return true;                                 \
  }                                              \
  END_TEST(testBigIntFits_Int##bits)

GENERATE_INTTYPE_TEST(8);
GENERATE_INTTYPE_TEST(16);
GENERATE_INTTYPE_TEST(32);

#undef GENERATE_INTTYPE_TEST

BEGIN_TEST(testBigIntFits_Int64) {
  int64_t in = INT64_MIN;
  JS::BigInt* bi = JS::NumberToBigInt(cx, in);
  CHECK(bi);
  int64_t i;
  CHECK(JS::BigIntFits(bi, &i));
  CHECK_EQUAL(i, in);

  in = INT64_MAX;
  bi = JS::NumberToBigInt(cx, in);
  CHECK(bi);
  CHECK(JS::BigIntFits(bi, &i));
  CHECK_EQUAL(i, in);

  JS::RootedValue v(cx);

  EVAL((std::string(500, '9') + "n").c_str(), &v);
  CHECK(v.isBigInt());
  CHECK(!JS::BigIntFits(v.toBigInt(), &i));

  EVAL(("-" + std::string(500, '9') + "n").c_str(), &v);
  CHECK(v.isBigInt());
  CHECK(!JS::BigIntFits(v.toBigInt(), &i));

  return true;
}
END_TEST(testBigIntFits_Int64)

BEGIN_TEST(testBigIntFits_Uint64) {
  uint64_t uin = 0;
  JS::BigInt* bi = JS::NumberToBigInt(cx, uin);
  CHECK(bi);
  uint64_t u;
  CHECK(JS::BigIntFits(bi, &u));
  CHECK_EQUAL(u, uin);

  uin = UINT64_MAX;
  bi = JS::NumberToBigInt(cx, uin);
  CHECK(bi);
  CHECK(JS::BigIntFits(bi, &u));
  CHECK_EQUAL(u, uin);

  JS::RootedValue v(cx);

  EVAL((std::string(500, '9') + "n").c_str(), &v);
  CHECK(v.isBigInt());
  CHECK(!JS::BigIntFits(v.toBigInt(), &u));

  return true;
}
END_TEST(testBigIntFits_Uint64)

#define GENERATE_SIGNED_VALUE_TEST(type, tag, val) \
  BEGIN_TEST(testBigIntFits_##type##_##tag) {      \
    int64_t v = val;                               \
    JS::BigInt* bi = JS::NumberToBigInt(cx, v);    \
    CHECK(bi);                                     \
    type result;                                   \
    CHECK(JS::BigIntFits(bi, &result));            \
    CHECK_EQUAL(v, result);                        \
    return true;                                   \
  }                                                \
  END_TEST(testBigIntFits_##type##_##tag)

#define GENERATE_UNSIGNED_VALUE_TEST(type, tag, val) \
  BEGIN_TEST(testBigIntFits_##type##_##tag) {        \
    uint64_t v = val;                                \
    JS::BigInt* bi = JS::NumberToBigInt(cx, v);      \
    CHECK(bi);                                       \
    type result;                                     \
    CHECK(JS::BigIntFits(bi, &result));              \
    CHECK_EQUAL(v, result);                          \
    return true;                                     \
  }                                                  \
  END_TEST(testBigIntFits_##type##_##tag)

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

#undef GENERATE_SIGNED_VALUE_TEST
#undef GENERATE_UNSIGNED_VALUE_TEST

BEGIN_TEST(testBigIntFitsNumber) {
  JS::BigInt* bi = JS::NumberToBigInt(cx, 0);
  CHECK(bi);
  double num;
  CHECK(JS::BigIntFitsNumber(bi, &num));
  int32_t result;
  CHECK(mozilla::NumberIsInt32(num, &result));
  CHECK_EQUAL(result, 0);

  bi = JS::NumberToBigInt(cx, 100);
  CHECK(bi);
  CHECK(JS::BigIntFitsNumber(bi, &num));
  CHECK(num == 100);

  bi = JS::NumberToBigInt(cx, -100);
  CHECK(bi);
  CHECK(JS::BigIntFitsNumber(bi, &num));
  CHECK(num == -100);

  JS::Rooted<JS::Value> v(cx);

  EVAL("BigInt(Number.MAX_SAFE_INTEGER)", &v);
  CHECK(v.isBigInt());
  CHECK(JS::BigIntFitsNumber(v.toBigInt(), &num));

  EVAL("BigInt(Number.MIN_SAFE_INTEGER)", &v);
  CHECK(v.isBigInt());
  CHECK(JS::BigIntFitsNumber(v.toBigInt(), &num));

  EVAL("BigInt(Number.MAX_SAFE_INTEGER) + 1n", &v);
  CHECK(v.isBigInt());
  CHECK(!JS::BigIntFitsNumber(v.toBigInt(), &num));

  EVAL("BigInt(Number.MIN_SAFE_INTEGER) - 1n", &v);
  CHECK(v.isBigInt());
  CHECK(!JS::BigIntFitsNumber(v.toBigInt(), &num));

  EVAL((std::string(500, '9') + "n").c_str(), &v);
  CHECK(v.isBigInt());
  CHECK(!JS::BigIntFitsNumber(v.toBigInt(), &num));

  EVAL(("-" + std::string(500, '9') + "n").c_str(), &v);
  CHECK(v.isBigInt());
  CHECK(!JS::BigIntFitsNumber(v.toBigInt(), &num));

  return true;
}
END_TEST(testBigIntFitsNumber)

BEGIN_TEST(testBigIntToString) {
  CHECK(Convert(12345, 10, "12345"));
  CHECK(Convert(-12345, 10, "-12345"));
  CHECK(Convert(0775, 8, "775"));
  CHECK(Convert(-0775, 8, "-775"));
  CHECK(Convert(0xCAFE, 16, "cafe"));
  CHECK(Convert(-0xCAFE, 16, "-cafe"));

  return true;
}

template <size_t N>
inline bool Convert(int64_t input, uint8_t radix, const char (&expected)[N]) {
  JS::Rooted<JS::BigInt*> bi(cx, JS::NumberToBigInt(cx, input));
  CHECK(bi);
  JS::Rooted<JSString*> str(cx, JS::BigIntToString(cx, bi, radix));
  CHECK(str);

  bool match;
  CHECK(JS_StringEqualsLiteral(cx, str, expected, &match));
  CHECK(match);

  return true;
}
END_TEST(testBigIntToString)

BEGIN_TEST(testBigIntToString_AllPossibleDigits) {
  const char allPossible[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
  JS::Rooted<JS::BigInt*> bi(
      cx,
      JS::SimpleStringToBigInt(cx, mozilla::MakeStringSpan(allPossible), 36));
  CHECK(bi);
  JS::Rooted<JSString*> str(cx, JS::BigIntToString(cx, bi, 36));
  CHECK(str);

  bool match;
  CHECK(JS_StringEqualsLiteral(cx, str, "abcdefghijklmnopqrstuvwxyz1234567890",
                               &match));
  CHECK(match);
  return true;
}
END_TEST(testBigIntToString_AllPossibleDigits)

BEGIN_TEST(testBigIntToString_RadixOutOfRange) {
  CHECK(RadixOutOfRange(1));
  CHECK(RadixOutOfRange(37));
  return true;
}

inline bool RadixOutOfRange(uint8_t radix) {
  JS::Rooted<JS::BigInt*> bi(cx, JS::NumberToBigInt(cx, 1));
  CHECK(bi);
  JSString* s = JS::BigIntToString(cx, bi, radix);
  CHECK_NULL(s);
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
END_TEST(testBigIntToString_RadixOutOfRange)
