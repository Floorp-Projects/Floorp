/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "NativeNt.h"

#include <stdio.h>

const wchar_t kNormal[] = L"Foo.dll";
const wchar_t kHex12[] = L"Foo.ABCDEF012345.dll";
const wchar_t kHex15[] = L"ABCDEF012345678.dll";
const wchar_t kHex16[] = L"ABCDEF0123456789.dll";
const wchar_t kHex17[] = L"ABCDEF0123456789a.dll";
const wchar_t kHex24[] = L"ABCDEF0123456789cdabef98.dll";
const wchar_t kHex8[] = L"01234567.dll";
const wchar_t kNonHex12[] = L"Foo.ABCDEFG12345.dll";
const wchar_t kHex13[] = L"Foo.ABCDEF0123456.dll";
const wchar_t kHex11[] = L"Foo.ABCDEF01234.dll";
const wchar_t kPrefixedHex16[] = L"Pabcdef0123456789.dll";

const char kFailFmt[] = "TEST-FAILED | NativeNt | %s(%s) should have returned %s but did not\n";

#define RUN_TEST(fn, varName, expected) \
  if (fn(varName) == !expected) { \
    printf(kFailFmt, #fn, #varName, #expected); \
    return 1; \
  }

#define EXPECT_FAIL(fn, varName) \
  RUN_TEST(fn, varName, false) \

#define EXPECT_SUCCESS(fn, varName) \
  RUN_TEST(fn, varName, true)

using namespace mozilla::nt;

int main(int argc, char* argv[])
{
  UNICODE_STRING normal;
  ::RtlInitUnicodeString(&normal, kNormal);

  UNICODE_STRING hex12;
  ::RtlInitUnicodeString(&hex12, kHex12);

  UNICODE_STRING hex16;
  ::RtlInitUnicodeString(&hex16, kHex16);

  UNICODE_STRING hex24;
  ::RtlInitUnicodeString(&hex24, kHex24);

  UNICODE_STRING hex8;
  ::RtlInitUnicodeString(&hex8, kHex8);

  UNICODE_STRING nonHex12;
  ::RtlInitUnicodeString(&nonHex12, kNonHex12);

  UNICODE_STRING hex13;
  ::RtlInitUnicodeString(&hex13, kHex13);

  UNICODE_STRING hex11;
  ::RtlInitUnicodeString(&hex11, kHex11);

  UNICODE_STRING hex15;
  ::RtlInitUnicodeString(&hex15, kHex15);

  UNICODE_STRING hex17;
  ::RtlInitUnicodeString(&hex17, kHex17);

  UNICODE_STRING prefixedHex16;
  ::RtlInitUnicodeString(&prefixedHex16, kPrefixedHex16);

  EXPECT_FAIL(Contains12DigitHexString, normal);
  EXPECT_SUCCESS(Contains12DigitHexString, hex12);
  EXPECT_FAIL(Contains12DigitHexString, hex13);
  EXPECT_FAIL(Contains12DigitHexString, hex11);
  EXPECT_FAIL(Contains12DigitHexString, hex16);
  EXPECT_FAIL(Contains12DigitHexString, nonHex12);

  EXPECT_FAIL(IsFileNameAtLeast16HexDigits, normal);
  EXPECT_FAIL(IsFileNameAtLeast16HexDigits, hex12);
  EXPECT_SUCCESS(IsFileNameAtLeast16HexDigits, hex24);
  EXPECT_SUCCESS(IsFileNameAtLeast16HexDigits, hex16);
  EXPECT_SUCCESS(IsFileNameAtLeast16HexDigits, hex17);
  EXPECT_FAIL(IsFileNameAtLeast16HexDigits, hex8);
  EXPECT_FAIL(IsFileNameAtLeast16HexDigits, hex15);
  EXPECT_FAIL(IsFileNameAtLeast16HexDigits, prefixedHex16);

  if (RtlGetProcessHeap() != ::GetProcessHeap()) {
    printf("TEST-FAILED | NativeNt | RtlGetProcessHeap() is broken\n");
    return 1;
  }

  return 0;
}

