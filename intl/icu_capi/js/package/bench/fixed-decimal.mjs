import Benchmark from 'benchmark';

import { ICU4XFixedDecimal } from "../index.js"

let suite = new Benchmark.Suite();

suite = suite.add("ICU4XFixedDecimal.create_from_i64", () => {
  (ICU4XFixedDecimal.create_from_i64(BigInt(1234))).underlying > 0;
});

const decimal = ICU4XFixedDecimal.create_from_i64(BigInt(1234));
decimal.multiply_pow10(-2);

suite = suite.add("ICU4XFixedDecimal.to_string", () => {
  decimal.to_string();
});

suite = suite.add("ICU4XFixedDecimal.multiply_pow10", () => {
  decimal.multiply_pow10(2);
  decimal.multiply_pow10(-2);
});

suite = suite.add("ICU4XFixedDecimal.set_sign", () => {
  decimal.set_sign("Negative");
  decimal.set_sign("None");
});

suite
  .on('cycle', (event) => {
    console.log(String(event.target));
    console.log('μs/it:', event.target.stats.mean * 1000 * 1000);
    console.log();
  })
  .run({ "async": false });
