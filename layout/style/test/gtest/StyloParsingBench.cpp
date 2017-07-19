/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "gtest/MozGTestBench.h"
#include "nsString.h"
#include "ExampleStylesheet.h"
#include "ServoBindings.h"
#include "NullPrincipalURI.h"
#include "nsCSSParser.h"

using namespace mozilla;
using namespace mozilla::css;
using namespace mozilla::net;

#define PARSING_REPETITIONS 20
#define SETPROPERTY_REPETITIONS (1000 * 1000)

#ifdef MOZ_STYLO

static void ServoParsingBench() {
  NS_NAMED_LITERAL_CSTRING(css_, EXAMPLE_STYLESHEET);
  const nsACString& css = css_;
  ASSERT_TRUE(IsUTF8(css));

  RefPtr<URLExtraData> data = new URLExtraData(
    NullPrincipalURI::Create(), nullptr, NullPrincipal::Create());
  for (int i = 0; i < PARSING_REPETITIONS; i++) {
    RefPtr<RawServoStyleSheetContents> stylesheet =
      Servo_StyleSheet_FromUTF8Bytes(
        nullptr, nullptr, &css, eAuthorSheetFeatures,
        data, 0, eCompatibility_FullStandards
      ).Consume();
  }
}

MOZ_GTEST_BENCH(Stylo, Servo_StyleSheet_FromUTF8Bytes_Bench, ServoParsingBench);

#endif


static void GeckoParsingBench() {
  // Don’t use NS_LITERAL_STRING to work around
  // "fatal error C1091: compiler limit: string exceeds 65535 bytes in length"
  // https://msdn.microsoft.com/en-us/library/f27ch0t1.aspx
  NS_ConvertUTF8toUTF16 css(NS_LITERAL_CSTRING(EXAMPLE_STYLESHEET));

  RefPtr<nsIURI> uri = NullPrincipalURI::Create();
  for (int i = 0; i < PARSING_REPETITIONS; i++) {
    RefPtr<CSSStyleSheet> stylesheet = new CSSStyleSheet(
      eAuthorSheetFeatures, CORS_NONE, RP_No_Referrer);
    stylesheet->SetURIs(uri, uri, uri);
    stylesheet->SetComplete();
    ASSERT_EQ(stylesheet->ReparseSheet(css), NS_OK);
  }
}

MOZ_GTEST_BENCH(Stylo, Gecko_nsCSSParser_ParseSheet_Bench, GeckoParsingBench);


#ifdef MOZ_STYLO

static void ServoSetPropertyByIdBench(const nsACString& css) {
  RefPtr<RawServoDeclarationBlock> block = Servo_DeclarationBlock_CreateEmpty().Consume();
  RefPtr<URLExtraData> data = new URLExtraData(
    NullPrincipalURI::Create(), nullptr, NullPrincipal::Create());

  ASSERT_TRUE(IsUTF8(css));

  for (int i = 0; i < SETPROPERTY_REPETITIONS; i++) {
    Servo_DeclarationBlock_SetPropertyById(
      block,
      eCSSProperty_width,
      &css,
      /* is_important = */ false,
      data,
      ParsingMode::Default,
      eCompatibility_FullStandards,
      nullptr
    );
  }
}

MOZ_GTEST_BENCH(Stylo, Servo_DeclarationBlock_SetPropertyById_Bench, [] {
  ServoSetPropertyByIdBench(NS_LITERAL_CSTRING("10px"));
});

MOZ_GTEST_BENCH(Stylo, Servo_DeclarationBlock_SetPropertyById_WithInitialSpace_Bench, [] {
  ServoSetPropertyByIdBench(NS_LITERAL_CSTRING(" 10px"));
});

#endif
