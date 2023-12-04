// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

import "./delete_fetch.mjs";
import { ICU4XLocale } from "./lib/ICU4XLocale.js";
import { ICU4XDataProvider } from "./lib/ICU4XDataProvider.js";
import { ICU4XFixedDecimalFormatter } from "./lib/ICU4XFixedDecimalFormatter.js";
import { ICU4XFixedDecimal } from "./lib/ICU4XFixedDecimal.js";

async function main() {
    const locale = ICU4XLocale.create_from_string("bn");
    const provider = ICU4XDataProvider.create_compiled();
  
    const format = ICU4XFixedDecimalFormatter.create_with_grouping_strategy(provider, locale, "Auto");
  
    const decimal = ICU4XFixedDecimal.create_from_i32(1000007);
    decimal.multiply_pow10(-2);
  
    const result = format.format(decimal);
    console.log("Output is", result);
}

main();
