/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"

#include "mozilla/intl/Currency.h"
#include "mozilla/Span.h"

namespace mozilla::intl {

TEST(IntlCurrency, GetISOCurrencies)
{
  bool hasUSDollar = false;
  bool hasEuro = false;
  constexpr auto usdollar = MakeStringSpan("USD");
  constexpr auto euro = MakeStringSpan("EUR");

  auto currencies = Currency::GetISOCurrencies().unwrap();
  for (auto currency : currencies) {
    // Check a few currencies, as the list may not be stable between ICU
    // updates.
    if (currency.unwrap() == usdollar) {
      hasUSDollar = true;
    }
    if (currency.unwrap() == euro) {
      hasEuro = true;
    }
  }

  ASSERT_TRUE(hasUSDollar);
  ASSERT_TRUE(hasEuro);
}

}  // namespace mozilla::intl
