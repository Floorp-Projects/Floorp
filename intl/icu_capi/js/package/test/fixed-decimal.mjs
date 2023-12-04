// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

import test from 'ava';

import { ICU4XFixedDecimal } from "../lib/index.js"

test("convert a simple decimal to a string", t => {
  const decimal = ICU4XFixedDecimal.create_from_i64(1234n);

  t.is(decimal.to_string(), "1234");
});

test("multiply a decimal by a power of 10", t => {
  const decimal = ICU4XFixedDecimal.create_from_i32(1234);
  decimal.multiply_pow10(-2);

  t.is(decimal.to_string(), "12.34");
});

test("negate a decimal", t => {
  const decimal = ICU4XFixedDecimal.create_from_i32(1234);
  decimal.set_sign("Negative");

  t.is(decimal.to_string(), "-1234");
});
