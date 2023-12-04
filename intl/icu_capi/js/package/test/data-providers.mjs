// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

import test from 'ava';

import { ICU4XFixedDecimal, ICU4XLocale, ICU4XDataProvider, ICU4XFixedDecimalFormatter } from "../index.js"

test("use create_compiled to format a simple decimal", async t => {
  const locale = ICU4XLocale.create_from_string("bn");
  const provider = ICU4XDataProvider.create_compiled();

  const format = ICU4XFixedDecimalFormatter.create_with_grouping_strategy(provider, locale, "Auto");

  const decimal = ICU4XFixedDecimal.create_from_i32(1234);
  decimal.multiply_pow10(-2);

  t.is(format.format(decimal), "১২.৩৪");
});
