/* Copyright 2021 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// ./test/core/float_literals.wast

// ./test/core/float_literals.wast:3
let $0 = instantiate(`(module
  ;; f32 special values
  (func (export "f32.nan") (result i32) (i32.reinterpret_f32 (f32.const nan)))
  (func (export "f32.positive_nan") (result i32) (i32.reinterpret_f32 (f32.const +nan)))
  (func (export "f32.negative_nan") (result i32) (i32.reinterpret_f32 (f32.const -nan)))
  (func (export "f32.plain_nan") (result i32) (i32.reinterpret_f32 (f32.const nan:0x400000)))
  (func (export "f32.informally_known_as_plain_snan") (result i32) (i32.reinterpret_f32 (f32.const nan:0x200000)))
  (func (export "f32.all_ones_nan") (result i32) (i32.reinterpret_f32 (f32.const -nan:0x7fffff)))
  (func (export "f32.misc_nan") (result i32) (i32.reinterpret_f32 (f32.const nan:0x012345)))
  (func (export "f32.misc_positive_nan") (result i32) (i32.reinterpret_f32 (f32.const +nan:0x304050)))
  (func (export "f32.misc_negative_nan") (result i32) (i32.reinterpret_f32 (f32.const -nan:0x2abcde)))
  (func (export "f32.infinity") (result i32) (i32.reinterpret_f32 (f32.const inf)))
  (func (export "f32.positive_infinity") (result i32) (i32.reinterpret_f32 (f32.const +inf)))
  (func (export "f32.negative_infinity") (result i32) (i32.reinterpret_f32 (f32.const -inf)))

  ;; f32 numbers
  (func (export "f32.zero") (result i32) (i32.reinterpret_f32 (f32.const 0x0.0p0)))
  (func (export "f32.positive_zero") (result i32) (i32.reinterpret_f32 (f32.const +0x0.0p0)))
  (func (export "f32.negative_zero") (result i32) (i32.reinterpret_f32 (f32.const -0x0.0p0)))
  (func (export "f32.misc") (result i32) (i32.reinterpret_f32 (f32.const 0x1.921fb6p+2)))
  (func (export "f32.min_positive") (result i32) (i32.reinterpret_f32 (f32.const 0x1p-149)))
  (func (export "f32.min_normal") (result i32) (i32.reinterpret_f32 (f32.const 0x1p-126)))
  (func (export "f32.max_finite") (result i32) (i32.reinterpret_f32 (f32.const 0x1.fffffep+127)))
  (func (export "f32.max_subnormal") (result i32) (i32.reinterpret_f32 (f32.const 0x1.fffffcp-127)))
  (func (export "f32.trailing_dot") (result i32) (i32.reinterpret_f32 (f32.const 0x1.p10)))

  ;; f32 in decimal format
  (func (export "f32_dec.zero") (result i32) (i32.reinterpret_f32 (f32.const 0.0e0)))
  (func (export "f32_dec.positive_zero") (result i32) (i32.reinterpret_f32 (f32.const +0.0e0)))
  (func (export "f32_dec.negative_zero") (result i32) (i32.reinterpret_f32 (f32.const -0.0e0)))
  (func (export "f32_dec.misc") (result i32) (i32.reinterpret_f32 (f32.const 6.28318548202514648)))
  (func (export "f32_dec.min_positive") (result i32) (i32.reinterpret_f32 (f32.const 1.4013e-45)))
  (func (export "f32_dec.min_normal") (result i32) (i32.reinterpret_f32 (f32.const 1.1754944e-38)))
  (func (export "f32_dec.max_subnormal") (result i32) (i32.reinterpret_f32 (f32.const 1.1754942e-38)))
  (func (export "f32_dec.max_finite") (result i32) (i32.reinterpret_f32 (f32.const 3.4028234e+38)))
  (func (export "f32_dec.trailing_dot") (result i32) (i32.reinterpret_f32 (f32.const 1.e10)))

  ;; https://twitter.com/Archivd/status/994637336506912768
  (func (export "f32_dec.root_beer_float") (result i32) (i32.reinterpret_f32 (f32.const 1.000000119)))

  ;; f64 special values
  (func (export "f64.nan") (result i64) (i64.reinterpret_f64 (f64.const nan)))
  (func (export "f64.positive_nan") (result i64) (i64.reinterpret_f64 (f64.const +nan)))
  (func (export "f64.negative_nan") (result i64) (i64.reinterpret_f64 (f64.const -nan)))
  (func (export "f64.plain_nan") (result i64) (i64.reinterpret_f64 (f64.const nan:0x8000000000000)))
  (func (export "f64.informally_known_as_plain_snan") (result i64) (i64.reinterpret_f64 (f64.const nan:0x4000000000000)))
  (func (export "f64.all_ones_nan") (result i64) (i64.reinterpret_f64 (f64.const -nan:0xfffffffffffff)))
  (func (export "f64.misc_nan") (result i64) (i64.reinterpret_f64 (f64.const nan:0x0123456789abc)))
  (func (export "f64.misc_positive_nan") (result i64) (i64.reinterpret_f64 (f64.const +nan:0x3040506070809)))
  (func (export "f64.misc_negative_nan") (result i64) (i64.reinterpret_f64 (f64.const -nan:0x2abcdef012345)))
  (func (export "f64.infinity") (result i64) (i64.reinterpret_f64 (f64.const inf)))
  (func (export "f64.positive_infinity") (result i64) (i64.reinterpret_f64 (f64.const +inf)))
  (func (export "f64.negative_infinity") (result i64) (i64.reinterpret_f64 (f64.const -inf)))

  ;; f64 numbers
  (func (export "f64.zero") (result i64) (i64.reinterpret_f64 (f64.const 0x0.0p0)))
  (func (export "f64.positive_zero") (result i64) (i64.reinterpret_f64 (f64.const +0x0.0p0)))
  (func (export "f64.negative_zero") (result i64) (i64.reinterpret_f64 (f64.const -0x0.0p0)))
  (func (export "f64.misc") (result i64) (i64.reinterpret_f64 (f64.const 0x1.921fb54442d18p+2)))
  (func (export "f64.min_positive") (result i64) (i64.reinterpret_f64 (f64.const 0x0.0000000000001p-1022)))
  (func (export "f64.min_normal") (result i64) (i64.reinterpret_f64 (f64.const 0x1p-1022)))
  (func (export "f64.max_subnormal") (result i64) (i64.reinterpret_f64 (f64.const 0x0.fffffffffffffp-1022)))
  (func (export "f64.max_finite") (result i64) (i64.reinterpret_f64 (f64.const 0x1.fffffffffffffp+1023)))
  (func (export "f64.trailing_dot") (result i64) (i64.reinterpret_f64 (f64.const 0x1.p100)))

  ;; f64 numbers in decimal format
  (func (export "f64_dec.zero") (result i64) (i64.reinterpret_f64 (f64.const 0.0e0)))
  (func (export "f64_dec.positive_zero") (result i64) (i64.reinterpret_f64 (f64.const +0.0e0)))
  (func (export "f64_dec.negative_zero") (result i64) (i64.reinterpret_f64 (f64.const -0.0e0)))
  (func (export "f64_dec.misc") (result i64) (i64.reinterpret_f64 (f64.const 6.28318530717958623)))
  (func (export "f64_dec.min_positive") (result i64) (i64.reinterpret_f64 (f64.const 4.94066e-324)))
  (func (export "f64_dec.min_normal") (result i64) (i64.reinterpret_f64 (f64.const 2.2250738585072012e-308)))
  (func (export "f64_dec.max_subnormal") (result i64) (i64.reinterpret_f64 (f64.const 2.2250738585072011e-308)))
  (func (export "f64_dec.max_finite") (result i64) (i64.reinterpret_f64 (f64.const 1.7976931348623157e+308)))
  (func (export "f64_dec.trailing_dot") (result i64) (i64.reinterpret_f64 (f64.const 1.e100)))

  ;; https://twitter.com/Archivd/status/994637336506912768
  (func (export "f64_dec.root_beer_float") (result i64) (i64.reinterpret_f64 (f64.const 1.000000119)))

  (func (export "f32-dec-sep1") (result f32) (f32.const 1_000_000))
  (func (export "f32-dec-sep2") (result f32) (f32.const 1_0_0_0))
  (func (export "f32-dec-sep3") (result f32) (f32.const 100_3.141_592))
  (func (export "f32-dec-sep4") (result f32) (f32.const 99e+1_3))
  (func (export "f32-dec-sep5") (result f32) (f32.const 122_000.11_3_54E0_2_3))
  (func (export "f32-hex-sep1") (result f32) (f32.const 0xa_0f_00_99))
  (func (export "f32-hex-sep2") (result f32) (f32.const 0x1_a_A_0_f))
  (func (export "f32-hex-sep3") (result f32) (f32.const 0xa0_ff.f141_a59a))
  (func (export "f32-hex-sep4") (result f32) (f32.const 0xf0P+1_3))
  (func (export "f32-hex-sep5") (result f32) (f32.const 0x2a_f00a.1f_3_eep2_3))

  (func (export "f64-dec-sep1") (result f64) (f64.const 1_000_000))
  (func (export "f64-dec-sep2") (result f64) (f64.const 1_0_0_0))
  (func (export "f64-dec-sep3") (result f64) (f64.const 100_3.141_592))
  (func (export "f64-dec-sep4") (result f64) (f64.const 99e-1_23))
  (func (export "f64-dec-sep5") (result f64) (f64.const 122_000.11_3_54e0_2_3))
  (func (export "f64-hex-sep1") (result f64) (f64.const 0xa_f00f_0000_9999))
  (func (export "f64-hex-sep2") (result f64) (f64.const 0x1_a_A_0_f))
  (func (export "f64-hex-sep3") (result f64) (f64.const 0xa0_ff.f141_a59a))
  (func (export "f64-hex-sep4") (result f64) (f64.const 0xf0P+1_3))
  (func (export "f64-hex-sep5") (result f64) (f64.const 0x2a_f00a.1f_3_eep2_3))
)`);

// ./test/core/float_literals.wast:105
assert_return(() => invoke($0, `f32.nan`, []), [value("i32", 2143289344)]);

// ./test/core/float_literals.wast:106
assert_return(() => invoke($0, `f32.positive_nan`, []), [value("i32", 2143289344)]);

// ./test/core/float_literals.wast:107
assert_return(() => invoke($0, `f32.negative_nan`, []), [value("i32", -4194304)]);

// ./test/core/float_literals.wast:108
assert_return(() => invoke($0, `f32.plain_nan`, []), [value("i32", 2143289344)]);

// ./test/core/float_literals.wast:109
assert_return(() => invoke($0, `f32.informally_known_as_plain_snan`, []), [value("i32", 2141192192)]);

// ./test/core/float_literals.wast:110
assert_return(() => invoke($0, `f32.all_ones_nan`, []), [value("i32", -1)]);

// ./test/core/float_literals.wast:111
assert_return(() => invoke($0, `f32.misc_nan`, []), [value("i32", 2139169605)]);

// ./test/core/float_literals.wast:112
assert_return(() => invoke($0, `f32.misc_positive_nan`, []), [value("i32", 2142257232)]);

// ./test/core/float_literals.wast:113
assert_return(() => invoke($0, `f32.misc_negative_nan`, []), [value("i32", -5587746)]);

// ./test/core/float_literals.wast:114
assert_return(() => invoke($0, `f32.infinity`, []), [value("i32", 2139095040)]);

// ./test/core/float_literals.wast:115
assert_return(() => invoke($0, `f32.positive_infinity`, []), [value("i32", 2139095040)]);

// ./test/core/float_literals.wast:116
assert_return(() => invoke($0, `f32.negative_infinity`, []), [value("i32", -8388608)]);

// ./test/core/float_literals.wast:117
assert_return(() => invoke($0, `f32.zero`, []), [value("i32", 0)]);

// ./test/core/float_literals.wast:118
assert_return(() => invoke($0, `f32.positive_zero`, []), [value("i32", 0)]);

// ./test/core/float_literals.wast:119
assert_return(() => invoke($0, `f32.negative_zero`, []), [value("i32", -2147483648)]);

// ./test/core/float_literals.wast:120
assert_return(() => invoke($0, `f32.misc`, []), [value("i32", 1086918619)]);

// ./test/core/float_literals.wast:121
assert_return(() => invoke($0, `f32.min_positive`, []), [value("i32", 1)]);

// ./test/core/float_literals.wast:122
assert_return(() => invoke($0, `f32.min_normal`, []), [value("i32", 8388608)]);

// ./test/core/float_literals.wast:123
assert_return(() => invoke($0, `f32.max_subnormal`, []), [value("i32", 8388607)]);

// ./test/core/float_literals.wast:124
assert_return(() => invoke($0, `f32.max_finite`, []), [value("i32", 2139095039)]);

// ./test/core/float_literals.wast:125
assert_return(() => invoke($0, `f32.trailing_dot`, []), [value("i32", 1149239296)]);

// ./test/core/float_literals.wast:126
assert_return(() => invoke($0, `f32_dec.zero`, []), [value("i32", 0)]);

// ./test/core/float_literals.wast:127
assert_return(() => invoke($0, `f32_dec.positive_zero`, []), [value("i32", 0)]);

// ./test/core/float_literals.wast:128
assert_return(() => invoke($0, `f32_dec.negative_zero`, []), [value("i32", -2147483648)]);

// ./test/core/float_literals.wast:129
assert_return(() => invoke($0, `f32_dec.misc`, []), [value("i32", 1086918619)]);

// ./test/core/float_literals.wast:130
assert_return(() => invoke($0, `f32_dec.min_positive`, []), [value("i32", 1)]);

// ./test/core/float_literals.wast:131
assert_return(() => invoke($0, `f32_dec.min_normal`, []), [value("i32", 8388608)]);

// ./test/core/float_literals.wast:132
assert_return(() => invoke($0, `f32_dec.max_subnormal`, []), [value("i32", 8388607)]);

// ./test/core/float_literals.wast:133
assert_return(() => invoke($0, `f32_dec.max_finite`, []), [value("i32", 2139095039)]);

// ./test/core/float_literals.wast:134
assert_return(() => invoke($0, `f32_dec.trailing_dot`, []), [value("i32", 1343554297)]);

// ./test/core/float_literals.wast:135
assert_return(() => invoke($0, `f32_dec.root_beer_float`, []), [value("i32", 1065353217)]);

// ./test/core/float_literals.wast:137
assert_return(() => invoke($0, `f64.nan`, []), [value("i64", 9221120237041090560n)]);

// ./test/core/float_literals.wast:138
assert_return(() => invoke($0, `f64.positive_nan`, []), [value("i64", 9221120237041090560n)]);

// ./test/core/float_literals.wast:139
assert_return(() => invoke($0, `f64.negative_nan`, []), [value("i64", -2251799813685248n)]);

// ./test/core/float_literals.wast:140
assert_return(() => invoke($0, `f64.plain_nan`, []), [value("i64", 9221120237041090560n)]);

// ./test/core/float_literals.wast:141
assert_return(
  () => invoke($0, `f64.informally_known_as_plain_snan`, []),
  [value("i64", 9219994337134247936n)],
);

// ./test/core/float_literals.wast:142
assert_return(() => invoke($0, `f64.all_ones_nan`, []), [value("i64", -1n)]);

// ./test/core/float_literals.wast:143
assert_return(() => invoke($0, `f64.misc_nan`, []), [value("i64", 9218888453225749180n)]);

// ./test/core/float_literals.wast:144
assert_return(() => invoke($0, `f64.misc_positive_nan`, []), [value("i64", 9219717281780008969n)]);

// ./test/core/float_literals.wast:145
assert_return(() => invoke($0, `f64.misc_negative_nan`, []), [value("i64", -3751748707474619n)]);

// ./test/core/float_literals.wast:146
assert_return(() => invoke($0, `f64.infinity`, []), [value("i64", 9218868437227405312n)]);

// ./test/core/float_literals.wast:147
assert_return(() => invoke($0, `f64.positive_infinity`, []), [value("i64", 9218868437227405312n)]);

// ./test/core/float_literals.wast:148
assert_return(() => invoke($0, `f64.negative_infinity`, []), [value("i64", -4503599627370496n)]);

// ./test/core/float_literals.wast:149
assert_return(() => invoke($0, `f64.zero`, []), [value("i64", 0n)]);

// ./test/core/float_literals.wast:150
assert_return(() => invoke($0, `f64.positive_zero`, []), [value("i64", 0n)]);

// ./test/core/float_literals.wast:151
assert_return(() => invoke($0, `f64.negative_zero`, []), [value("i64", -9223372036854775808n)]);

// ./test/core/float_literals.wast:152
assert_return(() => invoke($0, `f64.misc`, []), [value("i64", 4618760256179416344n)]);

// ./test/core/float_literals.wast:153
assert_return(() => invoke($0, `f64.min_positive`, []), [value("i64", 1n)]);

// ./test/core/float_literals.wast:154
assert_return(() => invoke($0, `f64.min_normal`, []), [value("i64", 4503599627370496n)]);

// ./test/core/float_literals.wast:155
assert_return(() => invoke($0, `f64.max_subnormal`, []), [value("i64", 4503599627370495n)]);

// ./test/core/float_literals.wast:156
assert_return(() => invoke($0, `f64.max_finite`, []), [value("i64", 9218868437227405311n)]);

// ./test/core/float_literals.wast:157
assert_return(() => invoke($0, `f64.trailing_dot`, []), [value("i64", 5057542381537067008n)]);

// ./test/core/float_literals.wast:158
assert_return(() => invoke($0, `f64_dec.zero`, []), [value("i64", 0n)]);

// ./test/core/float_literals.wast:159
assert_return(() => invoke($0, `f64_dec.positive_zero`, []), [value("i64", 0n)]);

// ./test/core/float_literals.wast:160
assert_return(() => invoke($0, `f64_dec.negative_zero`, []), [value("i64", -9223372036854775808n)]);

// ./test/core/float_literals.wast:161
assert_return(() => invoke($0, `f64_dec.misc`, []), [value("i64", 4618760256179416344n)]);

// ./test/core/float_literals.wast:162
assert_return(() => invoke($0, `f64_dec.min_positive`, []), [value("i64", 1n)]);

// ./test/core/float_literals.wast:163
assert_return(() => invoke($0, `f64_dec.min_normal`, []), [value("i64", 4503599627370496n)]);

// ./test/core/float_literals.wast:164
assert_return(() => invoke($0, `f64_dec.max_subnormal`, []), [value("i64", 4503599627370495n)]);

// ./test/core/float_literals.wast:165
assert_return(() => invoke($0, `f64_dec.max_finite`, []), [value("i64", 9218868437227405311n)]);

// ./test/core/float_literals.wast:166
assert_return(() => invoke($0, `f64_dec.trailing_dot`, []), [value("i64", 6103021453049119613n)]);

// ./test/core/float_literals.wast:167
assert_return(() => invoke($0, `f64_dec.root_beer_float`, []), [value("i64", 4607182419335945764n)]);

// ./test/core/float_literals.wast:169
assert_return(() => invoke($0, `f32-dec-sep1`, []), [value("f32", 1000000)]);

// ./test/core/float_literals.wast:170
assert_return(() => invoke($0, `f32-dec-sep2`, []), [value("f32", 1000)]);

// ./test/core/float_literals.wast:171
assert_return(() => invoke($0, `f32-dec-sep3`, []), [value("f32", 1003.1416)]);

// ./test/core/float_literals.wast:172
assert_return(() => invoke($0, `f32-dec-sep4`, []), [value("f32", 990000000000000)]);

// ./test/core/float_literals.wast:173
assert_return(() => invoke($0, `f32-dec-sep5`, []), [value("f32", 12200012000000000000000000000)]);

// ./test/core/float_literals.wast:174
assert_return(() => invoke($0, `f32-hex-sep1`, []), [value("f32", 168755360)]);

// ./test/core/float_literals.wast:175
assert_return(() => invoke($0, `f32-hex-sep2`, []), [value("f32", 109071)]);

// ./test/core/float_literals.wast:176
assert_return(() => invoke($0, `f32-hex-sep3`, []), [value("f32", 41215.94)]);

// ./test/core/float_literals.wast:177
assert_return(() => invoke($0, `f32-hex-sep4`, []), [value("f32", 1966080)]);

// ./test/core/float_literals.wast:178
assert_return(() => invoke($0, `f32-hex-sep5`, []), [value("f32", 23605224000000)]);

// ./test/core/float_literals.wast:180
assert_return(() => invoke($0, `f64-dec-sep1`, []), [value("f64", 1000000)]);

// ./test/core/float_literals.wast:181
assert_return(() => invoke($0, `f64-dec-sep2`, []), [value("f64", 1000)]);

// ./test/core/float_literals.wast:182
assert_return(() => invoke($0, `f64-dec-sep3`, []), [value("f64", 1003.141592)]);

// ./test/core/float_literals.wast:183
assert_return(
  () => invoke($0, `f64-dec-sep4`, []),
  [
    value("f64", 0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000099),
  ],
);

// ./test/core/float_literals.wast:184
assert_return(() => invoke($0, `f64-dec-sep5`, []), [value("f64", 12200011354000000000000000000)]);

// ./test/core/float_literals.wast:185
assert_return(() => invoke($0, `f64-hex-sep1`, []), [value("f64", 3078696982321561)]);

// ./test/core/float_literals.wast:186
assert_return(() => invoke($0, `f64-hex-sep2`, []), [value("f64", 109071)]);

// ./test/core/float_literals.wast:187
assert_return(() => invoke($0, `f64-hex-sep3`, []), [value("f64", 41215.94240794191)]);

// ./test/core/float_literals.wast:188
assert_return(() => invoke($0, `f64-hex-sep4`, []), [value("f64", 1966080)]);

// ./test/core/float_literals.wast:189
assert_return(() => invoke($0, `f64-hex-sep5`, []), [value("f64", 23605225168752)]);

// ./test/core/float_literals.wast:192
let $1 = instantiate(`(module binary
  ;; (func (export "4294967249") (result f64) (f64.const 4294967249))
  "\\00\\61\\73\\6d\\01\\00\\00\\00\\01\\85\\80\\80\\80\\00\\01\\60"
  "\\00\\01\\7c\\03\\82\\80\\80\\80\\00\\01\\00\\07\\8e\\80\\80\\80"
  "\\00\\01\\0a\\34\\32\\39\\34\\39\\36\\37\\32\\34\\39\\00\\00\\0a"
  "\\91\\80\\80\\80\\00\\01\\8b\\80\\80\\80\\00\\00\\44\\00\\00\\20"
  "\\fa\\ff\\ff\\ef\\41\\0b"
)`);

// ./test/core/float_literals.wast:201
assert_return(() => invoke($1, `4294967249`, []), [value("f64", 4294967249)]);

// ./test/core/float_literals.wast:203
assert_malformed(() => instantiate(`(global f32 (f32.const _100)) `), `unknown operator`);

// ./test/core/float_literals.wast:207
assert_malformed(() => instantiate(`(global f32 (f32.const +_100)) `), `unknown operator`);

// ./test/core/float_literals.wast:211
assert_malformed(() => instantiate(`(global f32 (f32.const -_100)) `), `unknown operator`);

// ./test/core/float_literals.wast:215
assert_malformed(() => instantiate(`(global f32 (f32.const 99_)) `), `unknown operator`);

// ./test/core/float_literals.wast:219
assert_malformed(
  () => instantiate(`(global f32 (f32.const 1__000)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:223
assert_malformed(() => instantiate(`(global f32 (f32.const _1.0)) `), `unknown operator`);

// ./test/core/float_literals.wast:227
assert_malformed(() => instantiate(`(global f32 (f32.const 1.0_)) `), `unknown operator`);

// ./test/core/float_literals.wast:231
assert_malformed(() => instantiate(`(global f32 (f32.const 1_.0)) `), `unknown operator`);

// ./test/core/float_literals.wast:235
assert_malformed(() => instantiate(`(global f32 (f32.const 1._0)) `), `unknown operator`);

// ./test/core/float_literals.wast:239
assert_malformed(() => instantiate(`(global f32 (f32.const _1e1)) `), `unknown operator`);

// ./test/core/float_literals.wast:243
assert_malformed(() => instantiate(`(global f32 (f32.const 1e1_)) `), `unknown operator`);

// ./test/core/float_literals.wast:247
assert_malformed(() => instantiate(`(global f32 (f32.const 1_e1)) `), `unknown operator`);

// ./test/core/float_literals.wast:251
assert_malformed(() => instantiate(`(global f32 (f32.const 1e_1)) `), `unknown operator`);

// ./test/core/float_literals.wast:255
assert_malformed(
  () => instantiate(`(global f32 (f32.const _1.0e1)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:259
assert_malformed(
  () => instantiate(`(global f32 (f32.const 1.0e1_)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:263
assert_malformed(
  () => instantiate(`(global f32 (f32.const 1.0_e1)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:267
assert_malformed(
  () => instantiate(`(global f32 (f32.const 1.0e_1)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:271
assert_malformed(
  () => instantiate(`(global f32 (f32.const 1.0e+_1)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:275
assert_malformed(
  () => instantiate(`(global f32 (f32.const 1.0e_+1)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:279
assert_malformed(
  () => instantiate(`(global f32 (f32.const _0x100)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:283
assert_malformed(
  () => instantiate(`(global f32 (f32.const 0_x100)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:287
assert_malformed(
  () => instantiate(`(global f32 (f32.const 0x_100)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:291
assert_malformed(() => instantiate(`(global f32 (f32.const 0x00_)) `), `unknown operator`);

// ./test/core/float_literals.wast:295
assert_malformed(
  () => instantiate(`(global f32 (f32.const 0xff__ffff)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:299
assert_malformed(
  () => instantiate(`(global f32 (f32.const 0x_1.0)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:303
assert_malformed(
  () => instantiate(`(global f32 (f32.const 0x1.0_)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:307
assert_malformed(
  () => instantiate(`(global f32 (f32.const 0x1_.0)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:311
assert_malformed(
  () => instantiate(`(global f32 (f32.const 0x1._0)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:315
assert_malformed(
  () => instantiate(`(global f32 (f32.const 0x_1p1)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:319
assert_malformed(
  () => instantiate(`(global f32 (f32.const 0x1p1_)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:323
assert_malformed(
  () => instantiate(`(global f32 (f32.const 0x1_p1)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:327
assert_malformed(
  () => instantiate(`(global f32 (f32.const 0x1p_1)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:331
assert_malformed(
  () => instantiate(`(global f32 (f32.const 0x_1.0p1)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:335
assert_malformed(
  () => instantiate(`(global f32 (f32.const 0x1.0p1_)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:339
assert_malformed(
  () => instantiate(`(global f32 (f32.const 0x1.0_p1)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:343
assert_malformed(
  () => instantiate(`(global f32 (f32.const 0x1.0p_1)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:347
assert_malformed(
  () => instantiate(`(global f32 (f32.const 0x1.0p+_1)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:351
assert_malformed(
  () => instantiate(`(global f32 (f32.const 0x1.0p_+1)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:356
assert_malformed(() => instantiate(`(global f64 (f64.const _100)) `), `unknown operator`);

// ./test/core/float_literals.wast:360
assert_malformed(() => instantiate(`(global f64 (f64.const +_100)) `), `unknown operator`);

// ./test/core/float_literals.wast:364
assert_malformed(() => instantiate(`(global f64 (f64.const -_100)) `), `unknown operator`);

// ./test/core/float_literals.wast:368
assert_malformed(() => instantiate(`(global f64 (f64.const 99_)) `), `unknown operator`);

// ./test/core/float_literals.wast:372
assert_malformed(
  () => instantiate(`(global f64 (f64.const 1__000)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:376
assert_malformed(() => instantiate(`(global f64 (f64.const _1.0)) `), `unknown operator`);

// ./test/core/float_literals.wast:380
assert_malformed(() => instantiate(`(global f64 (f64.const 1.0_)) `), `unknown operator`);

// ./test/core/float_literals.wast:384
assert_malformed(() => instantiate(`(global f64 (f64.const 1_.0)) `), `unknown operator`);

// ./test/core/float_literals.wast:388
assert_malformed(() => instantiate(`(global f64 (f64.const 1._0)) `), `unknown operator`);

// ./test/core/float_literals.wast:392
assert_malformed(() => instantiate(`(global f64 (f64.const _1e1)) `), `unknown operator`);

// ./test/core/float_literals.wast:396
assert_malformed(() => instantiate(`(global f64 (f64.const 1e1_)) `), `unknown operator`);

// ./test/core/float_literals.wast:400
assert_malformed(() => instantiate(`(global f64 (f64.const 1_e1)) `), `unknown operator`);

// ./test/core/float_literals.wast:404
assert_malformed(() => instantiate(`(global f64 (f64.const 1e_1)) `), `unknown operator`);

// ./test/core/float_literals.wast:408
assert_malformed(
  () => instantiate(`(global f64 (f64.const _1.0e1)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:412
assert_malformed(
  () => instantiate(`(global f64 (f64.const 1.0e1_)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:416
assert_malformed(
  () => instantiate(`(global f64 (f64.const 1.0_e1)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:420
assert_malformed(
  () => instantiate(`(global f64 (f64.const 1.0e_1)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:424
assert_malformed(
  () => instantiate(`(global f64 (f64.const 1.0e+_1)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:428
assert_malformed(
  () => instantiate(`(global f64 (f64.const 1.0e_+1)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:432
assert_malformed(
  () => instantiate(`(global f64 (f64.const _0x100)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:436
assert_malformed(
  () => instantiate(`(global f64 (f64.const 0_x100)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:440
assert_malformed(
  () => instantiate(`(global f64 (f64.const 0x_100)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:444
assert_malformed(() => instantiate(`(global f64 (f64.const 0x00_)) `), `unknown operator`);

// ./test/core/float_literals.wast:448
assert_malformed(
  () => instantiate(`(global f64 (f64.const 0xff__ffff)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:452
assert_malformed(
  () => instantiate(`(global f64 (f64.const 0x_1.0)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:456
assert_malformed(
  () => instantiate(`(global f64 (f64.const 0x1.0_)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:460
assert_malformed(
  () => instantiate(`(global f64 (f64.const 0x1_.0)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:464
assert_malformed(
  () => instantiate(`(global f64 (f64.const 0x1._0)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:468
assert_malformed(
  () => instantiate(`(global f64 (f64.const 0x_1p1)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:472
assert_malformed(
  () => instantiate(`(global f64 (f64.const 0x1p1_)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:476
assert_malformed(
  () => instantiate(`(global f64 (f64.const 0x1_p1)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:480
assert_malformed(
  () => instantiate(`(global f64 (f64.const 0x1p_1)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:484
assert_malformed(
  () => instantiate(`(global f64 (f64.const 0x_1.0p1)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:488
assert_malformed(
  () => instantiate(`(global f64 (f64.const 0x1.0p1_)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:492
assert_malformed(
  () => instantiate(`(global f64 (f64.const 0x1.0_p1)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:496
assert_malformed(
  () => instantiate(`(global f64 (f64.const 0x1.0p_1)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:500
assert_malformed(
  () => instantiate(`(global f64 (f64.const 0x1.0p+_1)) `),
  `unknown operator`,
);

// ./test/core/float_literals.wast:504
assert_malformed(
  () => instantiate(`(global f64 (f64.const 0x1.0p_+1)) `),
  `unknown operator`,
);
