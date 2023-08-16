/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "gtest/MozGTestBench.h"
#include "nsString.h"
#include "ExampleStylesheet.h"
#include "ServoBindings.h"
#include "mozilla/dom/DOMString.h"
#include "mozilla/Encoding.h"
#include "mozilla/Utf8.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/css/SheetParsingMode.h"
#include "ReferrerInfo.h"
#include "nsCSSValue.h"
#include "ReferrerInfo.h"

using namespace mozilla;
using namespace mozilla::css;
using namespace mozilla::dom;
using namespace mozilla::net;

// Bug 1436018 - Disable Stylo microbenchmark on Windows
#if !defined(_WIN32) && !defined(_WIN64)

#  define PARSING_REPETITIONS 20
#  define SETPROPERTY_REPETITIONS (1000 * 1000)
#  define GETPROPERTY_REPETITIONS (1000 * 1000)

static void ServoParsingBench(const StyleUseCounters* aCounters) {
  auto css = AsBytes(MakeStringSpan(EXAMPLE_STYLESHEET));
  nsCString cssStr;
  cssStr.Append(css);
  ASSERT_EQ(Encoding::UTF8ValidUpTo(css), css.Length());

  RefPtr<nsIURI> uri = NullPrincipal::CreateURI();
  nsCOMPtr<nsIReferrerInfo> referrerInfo = new ReferrerInfo(nullptr);
  RefPtr<URLExtraData> data =
      new URLExtraData(uri.forget(), referrerInfo.forget(),
                       NullPrincipal::CreateWithoutOriginAttributes());
  for (int i = 0; i < PARSING_REPETITIONS; i++) {
    RefPtr<StyleStylesheetContents> stylesheet =
        Servo_StyleSheet_FromUTF8Bytes(
            nullptr, nullptr, nullptr, &cssStr, eAuthorSheetFeatures, data,
            eCompatibility_FullStandards, nullptr, aCounters,
            StyleAllowImportRules::Yes, StyleSanitizationKind::None, nullptr)
            .Consume();
  }
}

static constexpr auto STYLE_RULE = StyleCssRuleType::Style;

static void ServoSetPropertyByIdBench(const nsACString& css) {
  RefPtr<StyleLockedDeclarationBlock> block =
      Servo_DeclarationBlock_CreateEmpty().Consume();
  RefPtr<nsIURI> uri = NullPrincipal::CreateURI();
  nsCOMPtr<nsIReferrerInfo> referrerInfo = new ReferrerInfo(nullptr);
  RefPtr<URLExtraData> data =
      new URLExtraData(uri.forget(), referrerInfo.forget(),
                       NullPrincipal::CreateWithoutOriginAttributes());
  ASSERT_TRUE(IsUtf8(css));

  for (int i = 0; i < SETPROPERTY_REPETITIONS; i++) {
    Servo_DeclarationBlock_SetPropertyById(
        block, eCSSProperty_width, &css,
        /* is_important = */ false, data, StyleParsingMode::DEFAULT,
        eCompatibility_FullStandards, nullptr, STYLE_RULE, {});
  }
}

static void ServoGetPropertyValueById() {
  RefPtr<StyleLockedDeclarationBlock> block =
      Servo_DeclarationBlock_CreateEmpty().Consume();

  RefPtr<nsIURI> uri = NullPrincipal::CreateURI();
  nsCOMPtr<nsIReferrerInfo> referrerInfo = new ReferrerInfo(nullptr);
  RefPtr<URLExtraData> data =
      new URLExtraData(uri.forget(), referrerInfo.forget(),
                       NullPrincipal::CreateWithoutOriginAttributes());
  constexpr auto css_ = "10px"_ns;
  const nsACString& css = css_;
  Servo_DeclarationBlock_SetPropertyById(
      block, eCSSProperty_width, &css,
      /* is_important = */ false, data, StyleParsingMode::DEFAULT,
      eCompatibility_FullStandards, nullptr, STYLE_RULE, {});

  for (int i = 0; i < GETPROPERTY_REPETITIONS; i++) {
    nsAutoCString value;
    Servo_DeclarationBlock_GetPropertyValueById(block, eCSSProperty_width,
                                                &value);
    ASSERT_TRUE(value.EqualsLiteral("10px"));
  }
}

MOZ_GTEST_BENCH(Stylo, Servo_StyleSheet_FromUTF8Bytes_Bench,
                [] { ServoParsingBench(nullptr); });

MOZ_GTEST_BENCH(Stylo, Servo_StyleSheet_FromUTF8Bytes_Bench_UseCounters, [] {
  UniquePtr<StyleUseCounters> counters(Servo_UseCounters_Create());
  ServoParsingBench(counters.get());
});

MOZ_GTEST_BENCH(Stylo, Servo_DeclarationBlock_SetPropertyById_Bench,
                [] { ServoSetPropertyByIdBench("10px"_ns); });

MOZ_GTEST_BENCH(Stylo,
                Servo_DeclarationBlock_SetPropertyById_WithInitialSpace_Bench,
                [] { ServoSetPropertyByIdBench(" 10px"_ns); });

MOZ_GTEST_BENCH(Stylo, Servo_DeclarationBlock_GetPropertyById_Bench,
                ServoGetPropertyValueById);

#endif
