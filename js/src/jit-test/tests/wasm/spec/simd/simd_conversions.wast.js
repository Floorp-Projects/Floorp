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

// ./test/core/simd/simd_conversions.wast

// ./test/core/simd/simd_conversions.wast:3
let $0 = instantiate(`(module
  ;; Integer to floating point
  (func (export "f32x4.convert_i32x4_s") (param v128) (result v128)
    (f32x4.convert_i32x4_s (local.get 0)))
  (func (export "f32x4.convert_i32x4_u") (param v128) (result v128)
    (f32x4.convert_i32x4_u (local.get 0)))

  (func (export "f64x2.convert_low_i32x4_s") (param v128) (result v128)
    (f64x2.convert_low_i32x4_s (local.get 0)))
  (func (export "f64x2.convert_low_i32x4_u") (param v128) (result v128)
    (f64x2.convert_low_i32x4_u (local.get 0)))

  ;; Integer to integer narrowing
  (func (export "i8x16.narrow_i16x8_s") (param v128 v128) (result v128)
    (i8x16.narrow_i16x8_s (local.get 0) (local.get 1)))
  (func (export "i8x16.narrow_i16x8_u") (param v128 v128) (result v128)
    (i8x16.narrow_i16x8_u (local.get 0) (local.get 1)))
  (func (export "i16x8.narrow_i32x4_s") (param v128 v128) (result v128)
    (i16x8.narrow_i32x4_s (local.get 0) (local.get 1)))
  (func (export "i16x8.narrow_i32x4_u") (param v128 v128) (result v128)
    (i16x8.narrow_i32x4_u (local.get 0)(local.get 1)))

  ;; Float to float promote/demote
  (func (export "f64x2.promote_low_f32x4") (param v128) (result v128)
    (f64x2.promote_low_f32x4 (local.get 0)))
  (func (export "f32x4.demote_f64x2_zero") (param v128) (result v128)
    (f32x4.demote_f64x2_zero (local.get 0)))
)`);

// ./test/core/simd/simd_conversions.wast:35
assert_return(() => invoke($0, `f64x2.promote_low_f32x4`, [f32x4([0, 0, 0, 0])]), [new F64x2Pattern(value('f64', 0), value('f64', 0))]);

// ./test/core/simd/simd_conversions.wast:37
assert_return(() => invoke($0, `f64x2.promote_low_f32x4`, [bytes('v128', [0x0, 0x0, 0x0, 0x80, 0x0, 0x0, 0x0, 0x80, 0x0, 0x0, 0x0, 0x80, 0x0, 0x0, 0x0, 0x80])]), [new F64x2Pattern(value('f64', -0), value('f64', -0))]);

// ./test/core/simd/simd_conversions.wast:39
assert_return(() => invoke($0, `f64x2.promote_low_f32x4`, [f32x4([0.000000000000000000000000000000000000000000001, 0.000000000000000000000000000000000000000000001, 0.000000000000000000000000000000000000000000001, 0.000000000000000000000000000000000000000000001])]), [new F64x2Pattern(value('f64', 0.000000000000000000000000000000000000000000001401298464324817), value('f64', 0.000000000000000000000000000000000000000000001401298464324817))]);

// ./test/core/simd/simd_conversions.wast:41
assert_return(() => invoke($0, `f64x2.promote_low_f32x4`, [f32x4([-0.000000000000000000000000000000000000000000001, -0.000000000000000000000000000000000000000000001, -0.000000000000000000000000000000000000000000001, -0.000000000000000000000000000000000000000000001])]), [new F64x2Pattern(value('f64', -0.000000000000000000000000000000000000000000001401298464324817), value('f64', -0.000000000000000000000000000000000000000000001401298464324817))]);

// ./test/core/simd/simd_conversions.wast:43
assert_return(() => invoke($0, `f64x2.promote_low_f32x4`, [f32x4([1, 1, 1, 1])]), [new F64x2Pattern(value('f64', 1), value('f64', 1))]);

// ./test/core/simd/simd_conversions.wast:45
assert_return(() => invoke($0, `f64x2.promote_low_f32x4`, [f32x4([-1, -1, -1, -1])]), [new F64x2Pattern(value('f64', -1), value('f64', -1))]);

// ./test/core/simd/simd_conversions.wast:47
assert_return(() => invoke($0, `f64x2.promote_low_f32x4`, [f32x4([-340282350000000000000000000000000000000, -340282350000000000000000000000000000000, -340282350000000000000000000000000000000, -340282350000000000000000000000000000000])]), [new F64x2Pattern(value('f64', -340282346638528860000000000000000000000), value('f64', -340282346638528860000000000000000000000))]);

// ./test/core/simd/simd_conversions.wast:49
assert_return(() => invoke($0, `f64x2.promote_low_f32x4`, [f32x4([340282350000000000000000000000000000000, 340282350000000000000000000000000000000, 340282350000000000000000000000000000000, 340282350000000000000000000000000000000])]), [new F64x2Pattern(value('f64', 340282346638528860000000000000000000000), value('f64', 340282346638528860000000000000000000000))]);

// ./test/core/simd/simd_conversions.wast:52
assert_return(() => invoke($0, `f64x2.promote_low_f32x4`, [f32x4([0.0000000000000000000000000000000000015046328, 0.0000000000000000000000000000000000015046328, 0.0000000000000000000000000000000000015046328, 0.0000000000000000000000000000000000015046328])]), [new F64x2Pattern(value('f64', 0.000000000000000000000000000000000001504632769052528), value('f64', 0.000000000000000000000000000000000001504632769052528))]);

// ./test/core/simd/simd_conversions.wast:55
assert_return(() => invoke($0, `f64x2.promote_low_f32x4`, [f32x4([66382537000000000000000000000000000000, 66382537000000000000000000000000000000, 66382537000000000000000000000000000000, 66382537000000000000000000000000000000])]), [new F64x2Pattern(value('f64', 66382536710104395000000000000000000000), value('f64', 66382536710104395000000000000000000000))]);

// ./test/core/simd/simd_conversions.wast:57
assert_return(() => invoke($0, `f64x2.promote_low_f32x4`, [f32x4([Infinity, Infinity, Infinity, Infinity])]), [new F64x2Pattern(value('f64', Infinity), value('f64', Infinity))]);

// ./test/core/simd/simd_conversions.wast:59
assert_return(() => invoke($0, `f64x2.promote_low_f32x4`, [f32x4([-Infinity, -Infinity, -Infinity, -Infinity])]), [new F64x2Pattern(value('f64', -Infinity), value('f64', -Infinity))]);

// ./test/core/simd/simd_conversions.wast:61
assert_return(() => invoke($0, `f64x2.promote_low_f32x4`, [bytes('v128', [0x0, 0x0, 0xc0, 0x7f, 0x0, 0x0, 0xc0, 0x7f, 0x0, 0x0, 0xc0, 0x7f, 0x0, 0x0, 0xc0, 0x7f])]), [new F64x2Pattern(`canonical_nan`, `canonical_nan`)]);

// ./test/core/simd/simd_conversions.wast:63
assert_return(() => invoke($0, `f64x2.promote_low_f32x4`, [bytes('v128', [0x0, 0x0, 0xa0, 0x7f, 0x0, 0x0, 0xa0, 0x7f, 0x0, 0x0, 0xa0, 0x7f, 0x0, 0x0, 0xa0, 0x7f])]), [new F64x2Pattern(`arithmetic_nan`, `arithmetic_nan`)]);

// ./test/core/simd/simd_conversions.wast:65
assert_return(() => invoke($0, `f64x2.promote_low_f32x4`, [bytes('v128', [0x0, 0x0, 0xc0, 0xff, 0x0, 0x0, 0xc0, 0xff, 0x0, 0x0, 0xc0, 0xff, 0x0, 0x0, 0xc0, 0xff])]), [new F64x2Pattern(`canonical_nan`, `canonical_nan`)]);

// ./test/core/simd/simd_conversions.wast:67
assert_return(() => invoke($0, `f64x2.promote_low_f32x4`, [bytes('v128', [0x0, 0x0, 0xa0, 0xff, 0x0, 0x0, 0xa0, 0xff, 0x0, 0x0, 0xa0, 0xff, 0x0, 0x0, 0xa0, 0xff])]), [new F64x2Pattern(`arithmetic_nan`, `arithmetic_nan`)]);

// ./test/core/simd/simd_conversions.wast:73
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([0, 0])]), [new F32x4Pattern(value('f32', 0), value('f32', 0), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:75
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([-0, -0])]), [new F32x4Pattern(bytes('f32', [0x0, 0x0, 0x0, 0x80]), bytes('f32', [0x0, 0x0, 0x0, 0x80]), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:77
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005, 0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005])]), [new F32x4Pattern(value('f32', 0), value('f32', 0), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:79
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([-0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005, -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005])]), [new F32x4Pattern(bytes('f32', [0x0, 0x0, 0x0, 0x80]), bytes('f32', [0x0, 0x0, 0x0, 0x80]), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:81
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([1, 1])]), [new F32x4Pattern(value('f32', 1), value('f32', 1), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:83
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([-1, -1])]), [new F32x4Pattern(value('f32', -1), value('f32', -1), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:85
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([0.000000000000000000000000000000000000011754942807573643, 0.000000000000000000000000000000000000011754942807573643])]), [new F32x4Pattern(value('f32', 0.000000000000000000000000000000000000011754944), value('f32', 0.000000000000000000000000000000000000011754944), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:87
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([-0.000000000000000000000000000000000000011754942807573643, -0.000000000000000000000000000000000000011754942807573643])]), [new F32x4Pattern(value('f32', -0.000000000000000000000000000000000000011754944), value('f32', -0.000000000000000000000000000000000000011754944), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:89
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([0.000000000000000000000000000000000000011754942807573642, 0.000000000000000000000000000000000000011754942807573642])]), [new F32x4Pattern(value('f32', 0.000000000000000000000000000000000000011754942), value('f32', 0.000000000000000000000000000000000000011754942), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:91
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([-0.000000000000000000000000000000000000011754942807573642, -0.000000000000000000000000000000000000011754942807573642])]), [new F32x4Pattern(value('f32', -0.000000000000000000000000000000000000011754942), value('f32', -0.000000000000000000000000000000000000011754942), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:93
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([0.000000000000000000000000000000000000000000001401298464324817, 0.000000000000000000000000000000000000000000001401298464324817])]), [new F32x4Pattern(value('f32', 0.000000000000000000000000000000000000000000001), value('f32', 0.000000000000000000000000000000000000000000001), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:95
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([-0.000000000000000000000000000000000000000000001401298464324817, -0.000000000000000000000000000000000000000000001401298464324817])]), [new F32x4Pattern(value('f32', -0.000000000000000000000000000000000000000000001), value('f32', -0.000000000000000000000000000000000000000000001), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:97
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([340282336497324060000000000000000000000, 340282336497324060000000000000000000000])]), [new F32x4Pattern(value('f32', 340282330000000000000000000000000000000), value('f32', 340282330000000000000000000000000000000), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:99
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([-340282336497324060000000000000000000000, -340282336497324060000000000000000000000])]), [new F32x4Pattern(value('f32', -340282330000000000000000000000000000000), value('f32', -340282330000000000000000000000000000000), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:101
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([340282336497324100000000000000000000000, 340282336497324100000000000000000000000])]), [new F32x4Pattern(value('f32', 340282350000000000000000000000000000000), value('f32', 340282350000000000000000000000000000000), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:103
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([-340282336497324100000000000000000000000, -340282336497324100000000000000000000000])]), [new F32x4Pattern(value('f32', -340282350000000000000000000000000000000), value('f32', -340282350000000000000000000000000000000), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:105
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([340282346638528860000000000000000000000, 340282346638528860000000000000000000000])]), [new F32x4Pattern(value('f32', 340282350000000000000000000000000000000), value('f32', 340282350000000000000000000000000000000), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:107
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([-340282346638528860000000000000000000000, -340282346638528860000000000000000000000])]), [new F32x4Pattern(value('f32', -340282350000000000000000000000000000000), value('f32', -340282350000000000000000000000000000000), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:109
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([340282356779733620000000000000000000000, 340282356779733620000000000000000000000])]), [new F32x4Pattern(value('f32', 340282350000000000000000000000000000000), value('f32', 340282350000000000000000000000000000000), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:111
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([-340282356779733620000000000000000000000, -340282356779733620000000000000000000000])]), [new F32x4Pattern(value('f32', -340282350000000000000000000000000000000), value('f32', -340282350000000000000000000000000000000), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:113
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([340282356779733660000000000000000000000, 340282356779733660000000000000000000000])]), [new F32x4Pattern(value('f32', Infinity), value('f32', Infinity), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:115
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([-340282356779733660000000000000000000000, -340282356779733660000000000000000000000])]), [new F32x4Pattern(value('f32', -Infinity), value('f32', -Infinity), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:117
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([0.000000000000000000000000000000000001504632769052528, 0.000000000000000000000000000000000001504632769052528])]), [new F32x4Pattern(value('f32', 0.0000000000000000000000000000000000015046328), value('f32', 0.0000000000000000000000000000000000015046328), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:119
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([66382536710104395000000000000000000000, 66382536710104395000000000000000000000])]), [new F32x4Pattern(value('f32', 66382537000000000000000000000000000000), value('f32', 66382537000000000000000000000000000000), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:121
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([Infinity, Infinity])]), [new F32x4Pattern(value('f32', Infinity), value('f32', Infinity), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:123
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([-Infinity, -Infinity])]), [new F32x4Pattern(value('f32', -Infinity), value('f32', -Infinity), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:125
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([1.0000000000000002, 1.0000000000000002])]), [new F32x4Pattern(value('f32', 1), value('f32', 1), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:127
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([0.9999999999999999, 0.9999999999999999])]), [new F32x4Pattern(value('f32', 1), value('f32', 1), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:129
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([1.0000000596046448, 1.0000000596046448])]), [new F32x4Pattern(value('f32', 1), value('f32', 1), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:131
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([1.000000059604645, 1.000000059604645])]), [new F32x4Pattern(value('f32', 1.0000001), value('f32', 1.0000001), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:133
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([1.000000178813934, 1.000000178813934])]), [new F32x4Pattern(value('f32', 1.0000001), value('f32', 1.0000001), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:135
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([1.0000001788139343, 1.0000001788139343])]), [new F32x4Pattern(value('f32', 1.0000002), value('f32', 1.0000002), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:137
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([1.0000002980232239, 1.0000002980232239])]), [new F32x4Pattern(value('f32', 1.0000002), value('f32', 1.0000002), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:139
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([16777217, 16777217])]), [new F32x4Pattern(value('f32', 16777216), value('f32', 16777216), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:141
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([16777217.000000004, 16777217.000000004])]), [new F32x4Pattern(value('f32', 16777218), value('f32', 16777218), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:143
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([16777218.999999996, 16777218.999999996])]), [new F32x4Pattern(value('f32', 16777218), value('f32', 16777218), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:145
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([16777219, 16777219])]), [new F32x4Pattern(value('f32', 16777220), value('f32', 16777220), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:147
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([424258443299142700000000000000000, 424258443299142700000000000000000])]), [new F32x4Pattern(value('f32', 424258450000000000000000000000000), value('f32', 424258450000000000000000000000000), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:149
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([0.0000000000000000000000000000000001569262107843488, 0.0000000000000000000000000000000001569262107843488])]), [new F32x4Pattern(value('f32', 0.00000000000000000000000000000000015692621), value('f32', 0.00000000000000000000000000000000015692621), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:151
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([0.000000000000000000000000000000000000010551773688605172, 0.000000000000000000000000000000000000010551773688605172])]), [new F32x4Pattern(value('f32', 0.000000000000000000000000000000000000010551773), value('f32', 0.000000000000000000000000000000000000010551773), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:153
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([-2.8238128484141933, -2.8238128484141933])]), [new F32x4Pattern(value('f32', -2.823813), value('f32', -2.823813), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:155
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([-9063376370095757000000000000000000, -9063376370095757000000000000000000])]), [new F32x4Pattern(value('f32', -9063376000000000000000000000000000), value('f32', -9063376000000000000000000000000000), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:157
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [bytes('v128', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])]), [new F32x4Pattern(`canonical_nan`, `canonical_nan`, value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:159
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [bytes('v128', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f])]), [new F32x4Pattern(`arithmetic_nan`, `arithmetic_nan`, value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:161
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [bytes('v128', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff])]), [new F32x4Pattern(`canonical_nan`, `canonical_nan`, value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:163
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [bytes('v128', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0xff])]), [new F32x4Pattern(`arithmetic_nan`, `arithmetic_nan`, value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:165
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000022250738585072014, 0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000022250738585072014])]), [new F32x4Pattern(value('f32', 0), value('f32', 0), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:167
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([-0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000022250738585072014, -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000022250738585072014])]), [new F32x4Pattern(bytes('f32', [0x0, 0x0, 0x0, 0x80]), bytes('f32', [0x0, 0x0, 0x0, 0x80]), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:169
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([0.0000000000000000000000000000000000000000000007006492321624085, 0.0000000000000000000000000000000000000000000007006492321624085])]), [new F32x4Pattern(value('f32', 0), value('f32', 0), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:171
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([-0.0000000000000000000000000000000000000000000007006492321624085, -0.0000000000000000000000000000000000000000000007006492321624085])]), [new F32x4Pattern(bytes('f32', [0x0, 0x0, 0x0, 0x80]), bytes('f32', [0x0, 0x0, 0x0, 0x80]), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:173
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([0.0000000000000000000000000000000000000000000007006492321624087, 0.0000000000000000000000000000000000000000000007006492321624087])]), [new F32x4Pattern(value('f32', 0.000000000000000000000000000000000000000000001), value('f32', 0.000000000000000000000000000000000000000000001), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:175
assert_return(() => invoke($0, `f32x4.demote_f64x2_zero`, [f64x2([-0.0000000000000000000000000000000000000000000007006492321624087, -0.0000000000000000000000000000000000000000000007006492321624087])]), [new F32x4Pattern(value('f32', -0.000000000000000000000000000000000000000000001), value('f32', -0.000000000000000000000000000000000000000000001), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:182
assert_return(() => invoke($0, `f32x4.convert_i32x4_s`, [i32x4([0x0, 0x0, 0x0, 0x0])]), [new F32x4Pattern(value('f32', 0), value('f32', 0), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:184
assert_return(() => invoke($0, `f32x4.convert_i32x4_s`, [i32x4([0x1, 0x1, 0x1, 0x1])]), [new F32x4Pattern(value('f32', 1), value('f32', 1), value('f32', 1), value('f32', 1))]);

// ./test/core/simd/simd_conversions.wast:186
assert_return(() => invoke($0, `f32x4.convert_i32x4_s`, [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])]), [new F32x4Pattern(value('f32', -1), value('f32', -1), value('f32', -1), value('f32', -1))]);

// ./test/core/simd/simd_conversions.wast:188
assert_return(() => invoke($0, `f32x4.convert_i32x4_s`, [i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff])]), [new F32x4Pattern(value('f32', 2147483600), value('f32', 2147483600), value('f32', 2147483600), value('f32', 2147483600))]);

// ./test/core/simd/simd_conversions.wast:190
assert_return(() => invoke($0, `f32x4.convert_i32x4_s`, [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])]), [new F32x4Pattern(value('f32', -2147483600), value('f32', -2147483600), value('f32', -2147483600), value('f32', -2147483600))]);

// ./test/core/simd/simd_conversions.wast:192
assert_return(() => invoke($0, `f32x4.convert_i32x4_s`, [i32x4([0x499602d2, 0x499602d2, 0x499602d2, 0x499602d2])]), [new F32x4Pattern(value('f32', 1234568000), value('f32', 1234568000), value('f32', 1234568000), value('f32', 1234568000))]);

// ./test/core/simd/simd_conversions.wast:194
assert_return(() => invoke($0, `f32x4.convert_i32x4_s`, [i32x4([0x75bcd18, 0x75bcd18, 0x75bcd18, 0x75bcd18])]), [new F32x4Pattern(value('f32', 123456790), value('f32', 123456790), value('f32', 123456790), value('f32', 123456790))]);

// ./test/core/simd/simd_conversions.wast:196
assert_return(() => invoke($0, `f32x4.convert_i32x4_s`, [i32x4([0x12345680, 0x12345680, 0x12345680, 0x12345680])]), [new F32x4Pattern(value('f32', 305419900), value('f32', 305419900), value('f32', 305419900), value('f32', 305419900))]);

// ./test/core/simd/simd_conversions.wast:200
assert_return(() => invoke($0, `f32x4.convert_i32x4_s`, [i32x4([0x1000001, 0x1000001, 0x1000001, 0x1000001])]), [new F32x4Pattern(value('f32', 16777216), value('f32', 16777216), value('f32', 16777216), value('f32', 16777216))]);

// ./test/core/simd/simd_conversions.wast:202
assert_return(() => invoke($0, `f32x4.convert_i32x4_s`, [i32x4([0xfeffffff, 0xfeffffff, 0xfeffffff, 0xfeffffff])]), [new F32x4Pattern(value('f32', -16777216), value('f32', -16777216), value('f32', -16777216), value('f32', -16777216))]);

// ./test/core/simd/simd_conversions.wast:204
assert_return(() => invoke($0, `f32x4.convert_i32x4_s`, [i32x4([0x1000003, 0x1000003, 0x1000003, 0x1000003])]), [new F32x4Pattern(value('f32', 16777220), value('f32', 16777220), value('f32', 16777220), value('f32', 16777220))]);

// ./test/core/simd/simd_conversions.wast:206
assert_return(() => invoke($0, `f32x4.convert_i32x4_s`, [i32x4([0xfefffffd, 0xfefffffd, 0xfefffffd, 0xfefffffd])]), [new F32x4Pattern(value('f32', -16777220), value('f32', -16777220), value('f32', -16777220), value('f32', -16777220))]);

// ./test/core/simd/simd_conversions.wast:208
assert_return(() => invoke($0, `f32x4.convert_i32x4_s`, [i32x4([0x0, 0xffffffff, 0x7fffffff, 0x80000000])]), [new F32x4Pattern(value('f32', 0), value('f32', -1), value('f32', 2147483600), value('f32', -2147483600))]);

// ./test/core/simd/simd_conversions.wast:213
assert_return(() => invoke($0, `f32x4.convert_i32x4_u`, [i32x4([0x0, 0x0, 0x0, 0x0])]), [new F32x4Pattern(value('f32', 0), value('f32', 0), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_conversions.wast:215
assert_return(() => invoke($0, `f32x4.convert_i32x4_u`, [i32x4([0x1, 0x1, 0x1, 0x1])]), [new F32x4Pattern(value('f32', 1), value('f32', 1), value('f32', 1), value('f32', 1))]);

// ./test/core/simd/simd_conversions.wast:217
assert_return(() => invoke($0, `f32x4.convert_i32x4_u`, [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])]), [new F32x4Pattern(value('f32', 4294967300), value('f32', 4294967300), value('f32', 4294967300), value('f32', 4294967300))]);

// ./test/core/simd/simd_conversions.wast:219
assert_return(() => invoke($0, `f32x4.convert_i32x4_u`, [i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff])]), [new F32x4Pattern(value('f32', 2147483600), value('f32', 2147483600), value('f32', 2147483600), value('f32', 2147483600))]);

// ./test/core/simd/simd_conversions.wast:221
assert_return(() => invoke($0, `f32x4.convert_i32x4_u`, [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])]), [new F32x4Pattern(value('f32', 2147483600), value('f32', 2147483600), value('f32', 2147483600), value('f32', 2147483600))]);

// ./test/core/simd/simd_conversions.wast:223
assert_return(() => invoke($0, `f32x4.convert_i32x4_u`, [i32x4([0x12345678, 0x12345678, 0x12345678, 0x12345678])]), [new F32x4Pattern(value('f32', 305419900), value('f32', 305419900), value('f32', 305419900), value('f32', 305419900))]);

// ./test/core/simd/simd_conversions.wast:225
assert_return(() => invoke($0, `f32x4.convert_i32x4_u`, [i32x4([0x80000080, 0x80000080, 0x80000080, 0x80000080])]), [new F32x4Pattern(value('f32', 2147483600), value('f32', 2147483600), value('f32', 2147483600), value('f32', 2147483600))]);

// ./test/core/simd/simd_conversions.wast:227
assert_return(() => invoke($0, `f32x4.convert_i32x4_u`, [i32x4([0x80000081, 0x80000081, 0x80000081, 0x80000081])]), [new F32x4Pattern(value('f32', 2147484000), value('f32', 2147484000), value('f32', 2147484000), value('f32', 2147484000))]);

// ./test/core/simd/simd_conversions.wast:229
assert_return(() => invoke($0, `f32x4.convert_i32x4_u`, [i32x4([0x80000082, 0x80000082, 0x80000082, 0x80000082])]), [new F32x4Pattern(value('f32', 2147484000), value('f32', 2147484000), value('f32', 2147484000), value('f32', 2147484000))]);

// ./test/core/simd/simd_conversions.wast:231
assert_return(() => invoke($0, `f32x4.convert_i32x4_u`, [i32x4([0xfffffe80, 0xfffffe80, 0xfffffe80, 0xfffffe80])]), [new F32x4Pattern(value('f32', 4294966800), value('f32', 4294966800), value('f32', 4294966800), value('f32', 4294966800))]);

// ./test/core/simd/simd_conversions.wast:233
assert_return(() => invoke($0, `f32x4.convert_i32x4_u`, [i32x4([0xfffffe81, 0xfffffe81, 0xfffffe81, 0xfffffe81])]), [new F32x4Pattern(value('f32', 4294967000), value('f32', 4294967000), value('f32', 4294967000), value('f32', 4294967000))]);

// ./test/core/simd/simd_conversions.wast:235
assert_return(() => invoke($0, `f32x4.convert_i32x4_u`, [i32x4([0xfffffe82, 0xfffffe82, 0xfffffe82, 0xfffffe82])]), [new F32x4Pattern(value('f32', 4294967000), value('f32', 4294967000), value('f32', 4294967000), value('f32', 4294967000))]);

// ./test/core/simd/simd_conversions.wast:237
assert_return(() => invoke($0, `f32x4.convert_i32x4_u`, [i32x4([0x75bcd18, 0x75bcd18, 0x75bcd18, 0x75bcd18])]), [new F32x4Pattern(value('f32', 123456790), value('f32', 123456790), value('f32', 123456790), value('f32', 123456790))]);

// ./test/core/simd/simd_conversions.wast:239
assert_return(() => invoke($0, `f32x4.convert_i32x4_u`, [i32x4([0x90abcdef, 0x90abcdef, 0x90abcdef, 0x90abcdef])]), [new F32x4Pattern(value('f32', 2427178500), value('f32', 2427178500), value('f32', 2427178500), value('f32', 2427178500))]);

// ./test/core/simd/simd_conversions.wast:243
assert_return(() => invoke($0, `f32x4.convert_i32x4_u`, [i32x4([0x1000001, 0x1000001, 0x1000001, 0x1000001])]), [new F32x4Pattern(value('f32', 16777216), value('f32', 16777216), value('f32', 16777216), value('f32', 16777216))]);

// ./test/core/simd/simd_conversions.wast:245
assert_return(() => invoke($0, `f32x4.convert_i32x4_u`, [i32x4([0x1000003, 0x1000003, 0x1000003, 0x1000003])]), [new F32x4Pattern(value('f32', 16777220), value('f32', 16777220), value('f32', 16777220), value('f32', 16777220))]);

// ./test/core/simd/simd_conversions.wast:247
assert_return(() => invoke($0, `f32x4.convert_i32x4_u`, [i32x4([0x0, 0xffffffff, 0x7fffffff, 0x80000000])]), [new F32x4Pattern(value('f32', 0), value('f32', 4294967300), value('f32', 2147483600), value('f32', 2147483600))]);

// ./test/core/simd/simd_conversions.wast:253
assert_return(() => invoke($0, `f64x2.convert_low_i32x4_s`, [i32x4([0x1, 0x1, 0x0, 0x0])]), [new F64x2Pattern(value('f64', 1), value('f64', 1))]);

// ./test/core/simd/simd_conversions.wast:255
assert_return(() => invoke($0, `f64x2.convert_low_i32x4_s`, [i32x4([0xffffffff, 0xffffffff, 0x0, 0x0])]), [new F64x2Pattern(value('f64', -1), value('f64', -1))]);

// ./test/core/simd/simd_conversions.wast:257
assert_return(() => invoke($0, `f64x2.convert_low_i32x4_s`, [i32x4([0x0, 0x0, 0x0, 0x0])]), [new F64x2Pattern(value('f64', 0), value('f64', 0))]);

// ./test/core/simd/simd_conversions.wast:259
assert_return(() => invoke($0, `f64x2.convert_low_i32x4_s`, [i32x4([0x7fffffff, 0x7fffffff, 0x0, 0x0])]), [new F64x2Pattern(value('f64', 2147483647), value('f64', 2147483647))]);

// ./test/core/simd/simd_conversions.wast:261
assert_return(() => invoke($0, `f64x2.convert_low_i32x4_s`, [i32x4([0x80000000, 0x80000000, 0x0, 0x0])]), [new F64x2Pattern(value('f64', -2147483648), value('f64', -2147483648))]);

// ./test/core/simd/simd_conversions.wast:263
assert_return(() => invoke($0, `f64x2.convert_low_i32x4_s`, [i32x4([0x3ade68b1, 0x3ade68b1, 0x0, 0x0])]), [new F64x2Pattern(value('f64', 987654321), value('f64', 987654321))]);

// ./test/core/simd/simd_conversions.wast:269
assert_return(() => invoke($0, `f64x2.convert_low_i32x4_u`, [i32x4([0x1, 0x1, 0x0, 0x0])]), [new F64x2Pattern(value('f64', 1), value('f64', 1))]);

// ./test/core/simd/simd_conversions.wast:271
assert_return(() => invoke($0, `f64x2.convert_low_i32x4_u`, [i32x4([0x0, 0x0, 0x0, 0x0])]), [new F64x2Pattern(value('f64', 0), value('f64', 0))]);

// ./test/core/simd/simd_conversions.wast:273
assert_return(() => invoke($0, `f64x2.convert_low_i32x4_u`, [i32x4([0x7fffffff, 0x7fffffff, 0x0, 0x0])]), [new F64x2Pattern(value('f64', 2147483647), value('f64', 2147483647))]);

// ./test/core/simd/simd_conversions.wast:275
assert_return(() => invoke($0, `f64x2.convert_low_i32x4_u`, [i32x4([0x80000000, 0x80000000, 0x0, 0x0])]), [new F64x2Pattern(value('f64', 2147483648), value('f64', 2147483648))]);

// ./test/core/simd/simd_conversions.wast:277
assert_return(() => invoke($0, `f64x2.convert_low_i32x4_u`, [i32x4([0xffffffff, 0xffffffff, 0x0, 0x0])]), [new F64x2Pattern(value('f64', 4294967295), value('f64', 4294967295))]);

// ./test/core/simd/simd_conversions.wast:283
assert_return(() => invoke($0, `i8x16.narrow_i16x8_s`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]), [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_conversions.wast:286
assert_return(() => invoke($0, `i8x16.narrow_i16x8_s`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1])]), [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1])]);

// ./test/core/simd/simd_conversions.wast:289
assert_return(() => invoke($0, `i8x16.narrow_i16x8_s`, [i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1]), i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]), [i8x16([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_conversions.wast:292
assert_return(() => invoke($0, `i8x16.narrow_i16x8_s`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff])]), [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])]);

// ./test/core/simd/simd_conversions.wast:295
assert_return(() => invoke($0, `i8x16.narrow_i16x8_s`, [i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]), i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]), [i8x16([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_conversions.wast:298
assert_return(() => invoke($0, `i8x16.narrow_i16x8_s`, [i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1]), i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff])]), [i8x16([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])]);

// ./test/core/simd/simd_conversions.wast:301
assert_return(() => invoke($0, `i8x16.narrow_i16x8_s`, [i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]), i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1])]), [i8x16([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1])]);

// ./test/core/simd/simd_conversions.wast:304
assert_return(() => invoke($0, `i8x16.narrow_i16x8_s`, [i16x8([0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e]), i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])]), [i8x16([0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])]);

// ./test/core/simd/simd_conversions.wast:307
assert_return(() => invoke($0, `i8x16.narrow_i16x8_s`, [i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f]), i16x8([0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e])]), [i8x16([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e])]);

// ./test/core/simd/simd_conversions.wast:310
assert_return(() => invoke($0, `i8x16.narrow_i16x8_s`, [i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f]), i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])]), [i8x16([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])]);

// ./test/core/simd/simd_conversions.wast:313
assert_return(() => invoke($0, `i8x16.narrow_i16x8_s`, [i16x8([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80]), i16x8([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80])]), [i8x16([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])]);

// ./test/core/simd/simd_conversions.wast:316
assert_return(() => invoke($0, `i8x16.narrow_i16x8_s`, [i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f]), i16x8([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80])]), [i8x16([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])]);

// ./test/core/simd/simd_conversions.wast:319
assert_return(() => invoke($0, `i8x16.narrow_i16x8_s`, [i16x8([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80]), i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])]), [i8x16([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])]);

// ./test/core/simd/simd_conversions.wast:322
assert_return(() => invoke($0, `i8x16.narrow_i16x8_s`, [i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f]), i16x8([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])]), [i8x16([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])]);

// ./test/core/simd/simd_conversions.wast:325
assert_return(() => invoke($0, `i8x16.narrow_i16x8_s`, [i16x8([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff]), i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])]), [i8x16([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])]);

// ./test/core/simd/simd_conversions.wast:328
assert_return(() => invoke($0, `i8x16.narrow_i16x8_s`, [i16x8([0xff81, 0xff81, 0xff81, 0xff81, 0xff81, 0xff81, 0xff81, 0xff81]), i16x8([0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80])]), [i8x16([0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80])]);

// ./test/core/simd/simd_conversions.wast:331
assert_return(() => invoke($0, `i8x16.narrow_i16x8_s`, [i16x8([0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80]), i16x8([0xff81, 0xff81, 0xff81, 0xff81, 0xff81, 0xff81, 0xff81, 0xff81])]), [i8x16([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81])]);

// ./test/core/simd/simd_conversions.wast:334
assert_return(() => invoke($0, `i8x16.narrow_i16x8_s`, [i16x8([0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80]), i16x8([0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80])]), [i8x16([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80])]);

// ./test/core/simd/simd_conversions.wast:337
assert_return(() => invoke($0, `i8x16.narrow_i16x8_s`, [i16x8([0xff7f, 0xff7f, 0xff7f, 0xff7f, 0xff7f, 0xff7f, 0xff7f, 0xff7f]), i16x8([0xff7f, 0xff7f, 0xff7f, 0xff7f, 0xff7f, 0xff7f, 0xff7f, 0xff7f])]), [i8x16([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80])]);

// ./test/core/simd/simd_conversions.wast:340
assert_return(() => invoke($0, `i8x16.narrow_i16x8_s`, [i16x8([0xff7f, 0xff7f, 0xff7f, 0xff7f, 0xff7f, 0xff7f, 0xff7f, 0xff7f]), i16x8([0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80])]), [i8x16([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80])]);

// ./test/core/simd/simd_conversions.wast:343
assert_return(() => invoke($0, `i8x16.narrow_i16x8_s`, [i16x8([0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80]), i16x8([0xff7f, 0xff7f, 0xff7f, 0xff7f, 0xff7f, 0xff7f, 0xff7f, 0xff7f])]), [i8x16([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80])]);

// ./test/core/simd/simd_conversions.wast:346
assert_return(() => invoke($0, `i8x16.narrow_i16x8_s`, [i16x8([0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80]), i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])]), [i8x16([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])]);

// ./test/core/simd/simd_conversions.wast:349
assert_return(() => invoke($0, `i8x16.narrow_i16x8_s`, [i16x8([0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80]), i16x8([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80])]), [i8x16([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])]);

// ./test/core/simd/simd_conversions.wast:352
assert_return(() => invoke($0, `i8x16.narrow_i16x8_s`, [i16x8([0xff7f, 0xff7f, 0xff7f, 0xff7f, 0xff7f, 0xff7f, 0xff7f, 0xff7f]), i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])]), [i8x16([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])]);

// ./test/core/simd/simd_conversions.wast:355
assert_return(() => invoke($0, `i8x16.narrow_i16x8_s`, [i16x8([0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80]), i16x8([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])]), [i8x16([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])]);

// ./test/core/simd/simd_conversions.wast:358
assert_return(() => invoke($0, `i8x16.narrow_i16x8_s`, [i16x8([0xff7f, 0xff7f, 0xff7f, 0xff7f, 0xff7f, 0xff7f, 0xff7f, 0xff7f]), i16x8([0x100, 0x100, 0x100, 0x100, 0x100, 0x100, 0x100, 0x100])]), [i8x16([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])]);

// ./test/core/simd/simd_conversions.wast:361
assert_return(() => invoke($0, `i8x16.narrow_i16x8_s`, [i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000]), i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff])]), [i8x16([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])]);

// ./test/core/simd/simd_conversions.wast:364
assert_return(() => invoke($0, `i8x16.narrow_i16x8_s`, [i16x8([0x3039, 0x3039, 0x3039, 0x3039, 0x3039, 0x3039, 0x3039, 0x3039]), i16x8([0xddd5, 0xddd5, 0xddd5, 0xddd5, 0xddd5, 0xddd5, 0xddd5, 0xddd5])]), [i8x16([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80])]);

// ./test/core/simd/simd_conversions.wast:367
assert_return(() => invoke($0, `i8x16.narrow_i16x8_s`, [i16x8([0x1234, 0x1234, 0x1234, 0x1234, 0x1234, 0x1234, 0x1234, 0x1234]), i16x8([0x5678, 0x5678, 0x5678, 0x5678, 0x5678, 0x5678, 0x5678, 0x5678])]), [i8x16([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])]);

// ./test/core/simd/simd_conversions.wast:372
assert_return(() => invoke($0, `i8x16.narrow_i16x8_u`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]), [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_conversions.wast:375
assert_return(() => invoke($0, `i8x16.narrow_i16x8_u`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1])]), [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1])]);

// ./test/core/simd/simd_conversions.wast:378
assert_return(() => invoke($0, `i8x16.narrow_i16x8_u`, [i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1]), i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]), [i8x16([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_conversions.wast:381
assert_return(() => invoke($0, `i8x16.narrow_i16x8_u`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff])]), [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_conversions.wast:384
assert_return(() => invoke($0, `i8x16.narrow_i16x8_u`, [i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]), i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]), [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_conversions.wast:387
assert_return(() => invoke($0, `i8x16.narrow_i16x8_u`, [i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1]), i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff])]), [i8x16([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_conversions.wast:390
assert_return(() => invoke($0, `i8x16.narrow_i16x8_u`, [i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]), i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1])]), [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1])]);

// ./test/core/simd/simd_conversions.wast:393
assert_return(() => invoke($0, `i8x16.narrow_i16x8_u`, [i16x8([0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e]), i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])]), [i8x16([0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])]);

// ./test/core/simd/simd_conversions.wast:396
assert_return(() => invoke($0, `i8x16.narrow_i16x8_u`, [i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f]), i16x8([0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e])]), [i8x16([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e])]);

// ./test/core/simd/simd_conversions.wast:399
assert_return(() => invoke($0, `i8x16.narrow_i16x8_u`, [i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f]), i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])]), [i8x16([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])]);

// ./test/core/simd/simd_conversions.wast:402
assert_return(() => invoke($0, `i8x16.narrow_i16x8_u`, [i16x8([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80]), i16x8([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80])]), [i8x16([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80])]);

// ./test/core/simd/simd_conversions.wast:405
assert_return(() => invoke($0, `i8x16.narrow_i16x8_u`, [i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f]), i16x8([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80])]), [i8x16([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80])]);

// ./test/core/simd/simd_conversions.wast:408
assert_return(() => invoke($0, `i8x16.narrow_i16x8_u`, [i16x8([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80]), i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])]), [i8x16([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])]);

// ./test/core/simd/simd_conversions.wast:411
assert_return(() => invoke($0, `i8x16.narrow_i16x8_u`, [i16x8([0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe]), i16x8([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])]), [i8x16([0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])]);

// ./test/core/simd/simd_conversions.wast:414
assert_return(() => invoke($0, `i8x16.narrow_i16x8_u`, [i16x8([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff]), i16x8([0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe])]), [i8x16([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe])]);

// ./test/core/simd/simd_conversions.wast:417
assert_return(() => invoke($0, `i8x16.narrow_i16x8_u`, [i16x8([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff]), i16x8([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])]), [i8x16([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])]);

// ./test/core/simd/simd_conversions.wast:420
assert_return(() => invoke($0, `i8x16.narrow_i16x8_u`, [i16x8([0x100, 0x100, 0x100, 0x100, 0x100, 0x100, 0x100, 0x100]), i16x8([0x100, 0x100, 0x100, 0x100, 0x100, 0x100, 0x100, 0x100])]), [i8x16([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])]);

// ./test/core/simd/simd_conversions.wast:423
assert_return(() => invoke($0, `i8x16.narrow_i16x8_u`, [i16x8([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff]), i16x8([0x100, 0x100, 0x100, 0x100, 0x100, 0x100, 0x100, 0x100])]), [i8x16([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])]);

// ./test/core/simd/simd_conversions.wast:426
assert_return(() => invoke($0, `i8x16.narrow_i16x8_u`, [i16x8([0x100, 0x100, 0x100, 0x100, 0x100, 0x100, 0x100, 0x100]), i16x8([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])]), [i8x16([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])]);

// ./test/core/simd/simd_conversions.wast:429
assert_return(() => invoke($0, `i8x16.narrow_i16x8_u`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), i16x8([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])]), [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])]);

// ./test/core/simd/simd_conversions.wast:432
assert_return(() => invoke($0, `i8x16.narrow_i16x8_u`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), i16x8([0x100, 0x100, 0x100, 0x100, 0x100, 0x100, 0x100, 0x100])]), [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])]);

// ./test/core/simd/simd_conversions.wast:435
assert_return(() => invoke($0, `i8x16.narrow_i16x8_u`, [i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]), i16x8([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])]), [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])]);

// ./test/core/simd/simd_conversions.wast:438
assert_return(() => invoke($0, `i8x16.narrow_i16x8_u`, [i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]), i16x8([0x100, 0x100, 0x100, 0x100, 0x100, 0x100, 0x100, 0x100])]), [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])]);

// ./test/core/simd/simd_conversions.wast:441
assert_return(() => invoke($0, `i8x16.narrow_i16x8_u`, [i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000]), i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff])]), [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_conversions.wast:444
assert_return(() => invoke($0, `i8x16.narrow_i16x8_u`, [i16x8([0xddd5, 0xddd5, 0xddd5, 0xddd5, 0xddd5, 0xddd5, 0xddd5, 0xddd5]), i16x8([0x3039, 0x3039, 0x3039, 0x3039, 0x3039, 0x3039, 0x3039, 0x3039])]), [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])]);

// ./test/core/simd/simd_conversions.wast:447
assert_return(() => invoke($0, `i8x16.narrow_i16x8_u`, [i16x8([0x90ab, 0x90ab, 0x90ab, 0x90ab, 0x90ab, 0x90ab, 0x90ab, 0x90ab]), i16x8([0x1234, 0x1234, 0x1234, 0x1234, 0x1234, 0x1234, 0x1234, 0x1234])]), [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])]);

// ./test/core/simd/simd_conversions.wast:452
assert_return(() => invoke($0, `i16x8.narrow_i32x4_s`, [i32x4([0x0, 0x0, 0x0, 0x0]), i32x4([0x0, 0x0, 0x0, 0x0])]), [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_conversions.wast:455
assert_return(() => invoke($0, `i16x8.narrow_i32x4_s`, [i32x4([0x0, 0x0, 0x0, 0x0]), i32x4([0x1, 0x1, 0x1, 0x1])]), [i16x8([0x0, 0x0, 0x0, 0x0, 0x1, 0x1, 0x1, 0x1])]);

// ./test/core/simd/simd_conversions.wast:458
assert_return(() => invoke($0, `i16x8.narrow_i32x4_s`, [i32x4([0x1, 0x1, 0x1, 0x1]), i32x4([0x0, 0x0, 0x0, 0x0])]), [i16x8([0x1, 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_conversions.wast:461
assert_return(() => invoke($0, `i16x8.narrow_i32x4_s`, [i32x4([0x0, 0x0, 0x0, 0x0]), i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])]), [i16x8([0x0, 0x0, 0x0, 0x0, 0xffff, 0xffff, 0xffff, 0xffff])]);

// ./test/core/simd/simd_conversions.wast:464
assert_return(() => invoke($0, `i16x8.narrow_i32x4_s`, [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]), i32x4([0x0, 0x0, 0x0, 0x0])]), [i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_conversions.wast:467
assert_return(() => invoke($0, `i16x8.narrow_i32x4_s`, [i32x4([0x1, 0x1, 0x1, 0x1]), i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])]), [i16x8([0x1, 0x1, 0x1, 0x1, 0xffff, 0xffff, 0xffff, 0xffff])]);

// ./test/core/simd/simd_conversions.wast:470
assert_return(() => invoke($0, `i16x8.narrow_i32x4_s`, [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]), i32x4([0x1, 0x1, 0x1, 0x1])]), [i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0x1, 0x1, 0x1, 0x1])]);

// ./test/core/simd/simd_conversions.wast:473
assert_return(() => invoke($0, `i16x8.narrow_i32x4_s`, [i32x4([0x7ffe, 0x7ffe, 0x7ffe, 0x7ffe]), i32x4([0x7fff, 0x7fff, 0x7fff, 0x7fff])]), [i16x8([0x7ffe, 0x7ffe, 0x7ffe, 0x7ffe, 0x7fff, 0x7fff, 0x7fff, 0x7fff])]);

// ./test/core/simd/simd_conversions.wast:476
assert_return(() => invoke($0, `i16x8.narrow_i32x4_s`, [i32x4([0x7fff, 0x7fff, 0x7fff, 0x7fff]), i32x4([0x7ffe, 0x7ffe, 0x7ffe, 0x7ffe])]), [i16x8([0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7ffe, 0x7ffe, 0x7ffe, 0x7ffe])]);

// ./test/core/simd/simd_conversions.wast:479
assert_return(() => invoke($0, `i16x8.narrow_i32x4_s`, [i32x4([0x7fff, 0x7fff, 0x7fff, 0x7fff]), i32x4([0x7fff, 0x7fff, 0x7fff, 0x7fff])]), [i16x8([0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff])]);

// ./test/core/simd/simd_conversions.wast:482
assert_return(() => invoke($0, `i16x8.narrow_i32x4_s`, [i32x4([0x8000, 0x8000, 0x8000, 0x8000]), i32x4([0x8000, 0x8000, 0x8000, 0x8000])]), [i16x8([0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff])]);

// ./test/core/simd/simd_conversions.wast:485
assert_return(() => invoke($0, `i16x8.narrow_i32x4_s`, [i32x4([0x7fff, 0x7fff, 0x7fff, 0x7fff]), i32x4([0x8000, 0x8000, 0x8000, 0x8000])]), [i16x8([0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff])]);

// ./test/core/simd/simd_conversions.wast:488
assert_return(() => invoke($0, `i16x8.narrow_i32x4_s`, [i32x4([0x8000, 0x8000, 0x8000, 0x8000]), i32x4([0x7fff, 0x7fff, 0x7fff, 0x7fff])]), [i16x8([0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff])]);

// ./test/core/simd/simd_conversions.wast:491
assert_return(() => invoke($0, `i16x8.narrow_i32x4_s`, [i32x4([0x7fff, 0x7fff, 0x7fff, 0x7fff]), i32x4([0xffff, 0xffff, 0xffff, 0xffff])]), [i16x8([0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff])]);

// ./test/core/simd/simd_conversions.wast:494
assert_return(() => invoke($0, `i16x8.narrow_i32x4_s`, [i32x4([0xffff, 0xffff, 0xffff, 0xffff]), i32x4([0x7fff, 0x7fff, 0x7fff, 0x7fff])]), [i16x8([0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff])]);

// ./test/core/simd/simd_conversions.wast:497
assert_return(() => invoke($0, `i16x8.narrow_i32x4_s`, [i32x4([0xffff8001, 0xffff8001, 0xffff8001, 0xffff8001]), i32x4([0xffff8000, 0xffff8000, 0xffff8000, 0xffff8000])]), [i16x8([0x8001, 0x8001, 0x8001, 0x8001, 0x8000, 0x8000, 0x8000, 0x8000])]);

// ./test/core/simd/simd_conversions.wast:500
assert_return(() => invoke($0, `i16x8.narrow_i32x4_s`, [i32x4([0xffff8000, 0xffff8000, 0xffff8000, 0xffff8000]), i32x4([0xffff8001, 0xffff8001, 0xffff8001, 0xffff8001])]), [i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0x8001, 0x8001, 0x8001, 0x8001])]);

// ./test/core/simd/simd_conversions.wast:503
assert_return(() => invoke($0, `i16x8.narrow_i32x4_s`, [i32x4([0xffff8000, 0xffff8000, 0xffff8000, 0xffff8000]), i32x4([0xffff8000, 0xffff8000, 0xffff8000, 0xffff8000])]), [i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000])]);

// ./test/core/simd/simd_conversions.wast:506
assert_return(() => invoke($0, `i16x8.narrow_i32x4_s`, [i32x4([0xffff7fff, 0xffff7fff, 0xffff7fff, 0xffff7fff]), i32x4([0xffff7fff, 0xffff7fff, 0xffff7fff, 0xffff7fff])]), [i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000])]);

// ./test/core/simd/simd_conversions.wast:509
assert_return(() => invoke($0, `i16x8.narrow_i32x4_s`, [i32x4([0xffff7fff, 0xffff7fff, 0xffff7fff, 0xffff7fff]), i32x4([0xffff8000, 0xffff8000, 0xffff8000, 0xffff8000])]), [i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000])]);

// ./test/core/simd/simd_conversions.wast:512
assert_return(() => invoke($0, `i16x8.narrow_i32x4_s`, [i32x4([0xffff8000, 0xffff8000, 0xffff8000, 0xffff8000]), i32x4([0xffff7fff, 0xffff7fff, 0xffff7fff, 0xffff7fff])]), [i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000])]);

// ./test/core/simd/simd_conversions.wast:515
assert_return(() => invoke($0, `i16x8.narrow_i32x4_s`, [i32x4([0xffff8000, 0xffff8000, 0xffff8000, 0xffff8000]), i32x4([0x7fff, 0x7fff, 0x7fff, 0x7fff])]), [i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0x7fff, 0x7fff, 0x7fff, 0x7fff])]);

// ./test/core/simd/simd_conversions.wast:518
assert_return(() => invoke($0, `i16x8.narrow_i32x4_s`, [i32x4([0xffff8000, 0xffff8000, 0xffff8000, 0xffff8000]), i32x4([0x8000, 0x8000, 0x8000, 0x8000])]), [i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0x7fff, 0x7fff, 0x7fff, 0x7fff])]);

// ./test/core/simd/simd_conversions.wast:521
assert_return(() => invoke($0, `i16x8.narrow_i32x4_s`, [i32x4([0xffff7fff, 0xffff7fff, 0xffff7fff, 0xffff7fff]), i32x4([0x7fff, 0x7fff, 0x7fff, 0x7fff])]), [i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0x7fff, 0x7fff, 0x7fff, 0x7fff])]);

// ./test/core/simd/simd_conversions.wast:524
assert_return(() => invoke($0, `i16x8.narrow_i32x4_s`, [i32x4([0xffff8000, 0xffff8000, 0xffff8000, 0xffff8000]), i32x4([0xffff, 0xffff, 0xffff, 0xffff])]), [i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0x7fff, 0x7fff, 0x7fff, 0x7fff])]);

// ./test/core/simd/simd_conversions.wast:527
assert_return(() => invoke($0, `i16x8.narrow_i32x4_s`, [i32x4([0xffff7fff, 0xffff7fff, 0xffff7fff, 0xffff7fff]), i32x4([0x10000, 0x10000, 0x10000, 0x10000])]), [i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0x7fff, 0x7fff, 0x7fff, 0x7fff])]);

// ./test/core/simd/simd_conversions.wast:530
assert_return(() => invoke($0, `i16x8.narrow_i32x4_s`, [i32x4([0xf8000000, 0xf8000000, 0xf8000000, 0xf8000000]), i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])]), [i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0xffff, 0xffff, 0xffff, 0xffff])]);

// ./test/core/simd/simd_conversions.wast:533
assert_return(() => invoke($0, `i16x8.narrow_i32x4_s`, [i32x4([0x75bcd15, 0x75bcd15, 0x75bcd15, 0x75bcd15]), i32x4([0x499602d2, 0x499602d2, 0x499602d2, 0x499602d2])]), [i16x8([0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff])]);

// ./test/core/simd/simd_conversions.wast:536
assert_return(() => invoke($0, `i16x8.narrow_i32x4_s`, [i32x4([0x90abcdef, 0x90abcdef, 0x90abcdef, 0x90abcdef]), i32x4([0x12345678, 0x12345678, 0x12345678, 0x12345678])]), [i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0x7fff, 0x7fff, 0x7fff, 0x7fff])]);

// ./test/core/simd/simd_conversions.wast:541
assert_return(() => invoke($0, `i16x8.narrow_i32x4_u`, [i32x4([0x0, 0x0, 0x0, 0x0]), i32x4([0x0, 0x0, 0x0, 0x0])]), [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_conversions.wast:544
assert_return(() => invoke($0, `i16x8.narrow_i32x4_u`, [i32x4([0x0, 0x0, 0x0, 0x0]), i32x4([0x1, 0x1, 0x1, 0x1])]), [i16x8([0x0, 0x0, 0x0, 0x0, 0x1, 0x1, 0x1, 0x1])]);

// ./test/core/simd/simd_conversions.wast:547
assert_return(() => invoke($0, `i16x8.narrow_i32x4_u`, [i32x4([0x1, 0x1, 0x1, 0x1]), i32x4([0x0, 0x0, 0x0, 0x0])]), [i16x8([0x1, 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_conversions.wast:550
assert_return(() => invoke($0, `i16x8.narrow_i32x4_u`, [i32x4([0x0, 0x0, 0x0, 0x0]), i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])]), [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_conversions.wast:553
assert_return(() => invoke($0, `i16x8.narrow_i32x4_u`, [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]), i32x4([0x0, 0x0, 0x0, 0x0])]), [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_conversions.wast:556
assert_return(() => invoke($0, `i16x8.narrow_i32x4_u`, [i32x4([0x1, 0x1, 0x1, 0x1]), i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])]), [i16x8([0x1, 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_conversions.wast:559
assert_return(() => invoke($0, `i16x8.narrow_i32x4_u`, [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]), i32x4([0x1, 0x1, 0x1, 0x1])]), [i16x8([0x0, 0x0, 0x0, 0x0, 0x1, 0x1, 0x1, 0x1])]);

// ./test/core/simd/simd_conversions.wast:562
assert_return(() => invoke($0, `i16x8.narrow_i32x4_u`, [i32x4([0xfffe, 0xfffe, 0xfffe, 0xfffe]), i32x4([0xffff, 0xffff, 0xffff, 0xffff])]), [i16x8([0xfffe, 0xfffe, 0xfffe, 0xfffe, 0xffff, 0xffff, 0xffff, 0xffff])]);

// ./test/core/simd/simd_conversions.wast:565
assert_return(() => invoke($0, `i16x8.narrow_i32x4_u`, [i32x4([0xffff, 0xffff, 0xffff, 0xffff]), i32x4([0xfffe, 0xfffe, 0xfffe, 0xfffe])]), [i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xfffe, 0xfffe, 0xfffe, 0xfffe])]);

// ./test/core/simd/simd_conversions.wast:568
assert_return(() => invoke($0, `i16x8.narrow_i32x4_u`, [i32x4([0xffff, 0xffff, 0xffff, 0xffff]), i32x4([0xffff, 0xffff, 0xffff, 0xffff])]), [i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff])]);

// ./test/core/simd/simd_conversions.wast:571
assert_return(() => invoke($0, `i16x8.narrow_i32x4_u`, [i32x4([0x10000, 0x10000, 0x10000, 0x10000]), i32x4([0x10000, 0x10000, 0x10000, 0x10000])]), [i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff])]);

// ./test/core/simd/simd_conversions.wast:574
assert_return(() => invoke($0, `i16x8.narrow_i32x4_u`, [i32x4([0xffff, 0xffff, 0xffff, 0xffff]), i32x4([0x10000, 0x10000, 0x10000, 0x10000])]), [i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff])]);

// ./test/core/simd/simd_conversions.wast:577
assert_return(() => invoke($0, `i16x8.narrow_i32x4_u`, [i32x4([0x10000, 0x10000, 0x10000, 0x10000]), i32x4([0xffff, 0xffff, 0xffff, 0xffff])]), [i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff])]);

// ./test/core/simd/simd_conversions.wast:580
assert_return(() => invoke($0, `i16x8.narrow_i32x4_u`, [i32x4([0x0, 0x0, 0x0, 0x0]), i32x4([0xffff, 0xffff, 0xffff, 0xffff])]), [i16x8([0x0, 0x0, 0x0, 0x0, 0xffff, 0xffff, 0xffff, 0xffff])]);

// ./test/core/simd/simd_conversions.wast:583
assert_return(() => invoke($0, `i16x8.narrow_i32x4_u`, [i32x4([0x0, 0x0, 0x0, 0x0]), i32x4([0x10000, 0x10000, 0x10000, 0x10000])]), [i16x8([0x0, 0x0, 0x0, 0x0, 0xffff, 0xffff, 0xffff, 0xffff])]);

// ./test/core/simd/simd_conversions.wast:586
assert_return(() => invoke($0, `i16x8.narrow_i32x4_u`, [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]), i32x4([0xffff, 0xffff, 0xffff, 0xffff])]), [i16x8([0x0, 0x0, 0x0, 0x0, 0xffff, 0xffff, 0xffff, 0xffff])]);

// ./test/core/simd/simd_conversions.wast:589
assert_return(() => invoke($0, `i16x8.narrow_i32x4_u`, [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]), i32x4([0x10000, 0x10000, 0x10000, 0x10000])]), [i16x8([0x0, 0x0, 0x0, 0x0, 0xffff, 0xffff, 0xffff, 0xffff])]);

// ./test/core/simd/simd_conversions.wast:592
assert_return(() => invoke($0, `i16x8.narrow_i32x4_u`, [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]), i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])]), [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_conversions.wast:595
assert_return(() => invoke($0, `i16x8.narrow_i32x4_u`, [i32x4([0x75bcd15, 0x75bcd15, 0x75bcd15, 0x75bcd15]), i32x4([0x499602d2, 0x499602d2, 0x499602d2, 0x499602d2])]), [i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff])]);

// ./test/core/simd/simd_conversions.wast:598
assert_return(() => invoke($0, `i16x8.narrow_i32x4_u`, [i32x4([0x90abcdef, 0x90abcdef, 0x90abcdef, 0x90abcdef]), i32x4([0x12345678, 0x12345678, 0x12345678, 0x12345678])]), [i16x8([0x0, 0x0, 0x0, 0x0, 0xffff, 0xffff, 0xffff, 0xffff])]);

// ./test/core/simd/simd_conversions.wast:605
assert_malformed(() => instantiate(`(func (result v128) (i32x4.trunc_sat_f32x4 (v128.const f32x4 0.0 0.0 0.0 0.0))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:608
assert_malformed(() => instantiate(`(func (result v128) (i32x4.trunc_s_sat_f32x4 (v128.const f32x4 -2.0 -1.0 1.0 2.0))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:611
assert_malformed(() => instantiate(`(func (result v128) (i32x4.trunc_u_sat_f32x4 (v128.const f32x4 -2.0 -1.0 1.0 2.0))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:614
assert_malformed(() => instantiate(`(func (result v128) (i32x4.convert_f32x4 (v128.const f32x4 -1 0 1 2))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:617
assert_malformed(() => instantiate(`(func (result v128) (i32x4.convert_s_f32x4 (v128.const f32x4 -1 0 1 2))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:620
assert_malformed(() => instantiate(`(func (result v128) (i32x4.convert_u_f32x4 (v128.const f32x4 -1 0 1 2))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:624
assert_malformed(() => instantiate(`(func (result v128) (i64x2.trunc_sat_f64x2_s (v128.const f64x2 0.0 0.0))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:627
assert_malformed(() => instantiate(`(func (result v128) (i64x2.trunc_sat_f64x2_u (v128.const f64x2 -2.0 -1.0))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:630
assert_malformed(() => instantiate(`(func (result v128) (f64x2.convert_i64x2_s (v128.const i64x2 1 2))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:633
assert_malformed(() => instantiate(`(func (result v128) (f64x2.convert_i64x2_u (v128.const i64x2 1 2))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:637
assert_malformed(() => instantiate(`(func (result v128) (i8x16.narrow_i16x8 (v128.const i16x8 0 0 0 0 0 0 0 0) (v128.const i16x8 0 0 0 0 0 0 0 0))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:640
assert_malformed(() => instantiate(`(func (result v128) (i16x8.narrow_i8x16 (v128.const i16x8 0 0 0 0 0 0 0 0) (v128.const i16x8 0 0 0 0 0 0 0 0))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:643
assert_malformed(() => instantiate(`(func (result v128) (i16x8.narrow_i8x16_s (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:646
assert_malformed(() => instantiate(`(func (result v128) (i16x8.narrow_i8x16_u (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:649
assert_malformed(() => instantiate(`(func (result v128) (i16x8.narrow_i32x4 (v128.const i16x8 0 0 0 0 0 0 0 0) (v128.const i16x8 0 0 0 0 0 0 0 0))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:652
assert_malformed(() => instantiate(`(func (result v128) (i32x4.narrow_i16x8 (v128.const i16x8 0 0 0 0 0 0 0 0) (v128.const i16x8 0 0 0 0 0 0 0 0))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:655
assert_malformed(() => instantiate(`(func (result v128) (i32x4.narrow_i16x8_s (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:658
assert_malformed(() => instantiate(`(func (result v128) (i32x4.narrow_i16x8_u (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:662
assert_malformed(() => instantiate(`(func (result v128) (i16x8.extend_low_i8x16 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:665
assert_malformed(() => instantiate(`(func (result v128) (i8x16.extend_low_i16x8_s (v128.const i16x8 0 0 0 0 0 0 0 0))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:668
assert_malformed(() => instantiate(`(func (result v128) (i8x16.extend_low_i16x8_u (v128.const i16x8 0 0 0 0 0 0 0 0))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:671
assert_malformed(() => instantiate(`(func (result v128) (i16x8.extend_high_i8x16 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:674
assert_malformed(() => instantiate(`(func (result v128) (i8x16.extend_high_i16x8_s (v128.const i16x8 0 0 0 0 0 0 0 0))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:677
assert_malformed(() => instantiate(`(func (result v128) (i8x16.extend_high_i16x8_u (v128.const i16x8 0 0 0 0 0 0 0 0))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:680
assert_malformed(() => instantiate(`(func (result v128) (i32x4.extend_low_i16x8 (v128.const i16x8 0 0 0 0 0 0 0 0))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:683
assert_malformed(() => instantiate(`(func (result v128) (i16x8.extend_low_i32x4_s (v128.const i32x4 0 0 0 0))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:686
assert_malformed(() => instantiate(`(func (result v128) (i16x8.extend_low_i32x4_u (v128.const i32x4 0 0 0 0))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:689
assert_malformed(() => instantiate(`(func (result v128) (i32x4.extend_high_i16x8 (v128.const i16x8 0 0 0 0 0 0 0 0))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:692
assert_malformed(() => instantiate(`(func (result v128) (i16x8.extend_high_i32x4_s (v128.const i32x4 0 0 0 0))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:695
assert_malformed(() => instantiate(`(func (result v128) (i16x8.extend_high_i32x4_u (v128.const i32x4 0 0 0 0))) `), `unknown operator`);

// ./test/core/simd/simd_conversions.wast:702
assert_invalid(() => instantiate(`(module (func (result v128) (f32x4.convert_i32x4_s (i32.const 0))))`), `type mismatch`);

// ./test/core/simd/simd_conversions.wast:703
assert_invalid(() => instantiate(`(module (func (result v128) (f32x4.convert_i32x4_s (i64.const 0))))`), `type mismatch`);

// ./test/core/simd/simd_conversions.wast:704
assert_invalid(() => instantiate(`(module (func (result v128) (f32x4.convert_i32x4_u (i32.const 0))))`), `type mismatch`);

// ./test/core/simd/simd_conversions.wast:705
assert_invalid(() => instantiate(`(module (func (result v128) (f32x4.convert_i32x4_u (i64.const 0))))`), `type mismatch`);

// ./test/core/simd/simd_conversions.wast:707
assert_invalid(() => instantiate(`(module (func (result v128) (i8x16.narrow_i16x8_s (i32.const 0) (i64.const 0))))`), `type mismatch`);

// ./test/core/simd/simd_conversions.wast:708
assert_invalid(() => instantiate(`(module (func (result v128) (i8x16.narrow_i16x8_u (i32.const 0) (i64.const 0))))`), `type mismatch`);

// ./test/core/simd/simd_conversions.wast:709
assert_invalid(() => instantiate(`(module (func (result v128) (i16x8.narrow_i32x4_s (f32.const 0.0) (f64.const 0.0))))`), `type mismatch`);

// ./test/core/simd/simd_conversions.wast:710
assert_invalid(() => instantiate(`(module (func (result v128) (i16x8.narrow_i32x4_s (f32.const 0.0) (f64.const 0.0))))`), `type mismatch`);

// ./test/core/simd/simd_conversions.wast:715
let $1 = instantiate(`(module
  (func (export "f32x4_convert_i32x4_s_add") (param v128 v128) (result v128)
    (f32x4.convert_i32x4_s (i32x4.add (local.get 0) (local.get 1))))
  (func (export "f32x4_convert_i32x4_s_sub") (param  v128 v128) (result v128)
    (f32x4.convert_i32x4_s (i32x4.sub (local.get 0) (local.get 1))))
  (func (export "f32x4_convert_i32x4_u_mul") (param  v128 v128) (result v128)
    (f32x4.convert_i32x4_u (i32x4.mul (local.get 0) (local.get 1))))

  (func (export "i16x8_low_extend_narrow_ss") (param v128 v128) (result v128)
    (i16x8.extend_low_i8x16_s (i8x16.narrow_i16x8_s (local.get 0) (local.get 1))))
  (func (export "i16x8_low_extend_narrow_su") (param v128 v128) (result v128)
    (i16x8.extend_low_i8x16_s (i8x16.narrow_i16x8_u (local.get 0) (local.get 1))))
  (func (export "i16x8_high_extend_narrow_ss") (param v128 v128) (result v128)
    (i16x8.extend_low_i8x16_s (i8x16.narrow_i16x8_s (local.get 0) (local.get 1))))
  (func (export "i16x8_high_extend_narrow_su") (param v128 v128) (result v128)
    (i16x8.extend_low_i8x16_s (i8x16.narrow_i16x8_u (local.get 0) (local.get 1))))
  (func (export "i16x8_low_extend_narrow_uu") (param v128 v128) (result v128)
    (i16x8.extend_low_i8x16_u (i8x16.narrow_i16x8_u (local.get 0) (local.get 1))))
  (func (export "i16x8_low_extend_narrow_us") (param v128 v128) (result v128)
    (i16x8.extend_low_i8x16_u (i8x16.narrow_i16x8_s (local.get 0) (local.get 1))))
  (func (export "i16x8_high_extend_narrow_uu") (param v128 v128) (result v128)
    (i16x8.extend_low_i8x16_u (i8x16.narrow_i16x8_u (local.get 0) (local.get 1))))
  (func (export "i16x8_high_extend_narrow_us") (param v128 v128) (result v128)
    (i16x8.extend_low_i8x16_u (i8x16.narrow_i16x8_s (local.get 0) (local.get 1))))

  (func (export "i32x4_low_extend_narrow_ss") (param v128 v128) (result v128)
    (i32x4.extend_low_i16x8_s (i16x8.narrow_i32x4_s (local.get 0) (local.get 1))))
  (func (export "i32x4_low_extend_narrow_su") (param v128 v128) (result v128)
    (i32x4.extend_low_i16x8_s (i16x8.narrow_i32x4_u (local.get 0) (local.get 1))))
  (func (export "i32x4_high_extend_narrow_ss") (param v128 v128) (result v128)
    (i32x4.extend_low_i16x8_s (i16x8.narrow_i32x4_s (local.get 0) (local.get 1))))
  (func (export "i32x4_high_extend_narrow_su") (param v128 v128) (result v128)
    (i32x4.extend_low_i16x8_s (i16x8.narrow_i32x4_u (local.get 0) (local.get 1))))
  (func (export "i32x4_low_extend_narrow_uu") (param v128 v128) (result v128)
    (i32x4.extend_low_i16x8_u (i16x8.narrow_i32x4_u (local.get 0) (local.get 1))))
  (func (export "i32x4_low_extend_narrow_us") (param v128 v128) (result v128)
    (i32x4.extend_low_i16x8_u (i16x8.narrow_i32x4_s (local.get 0) (local.get 1))))
  (func (export "i32x4_high_extend_narrow_uu") (param v128 v128) (result v128)
    (i32x4.extend_low_i16x8_u (i16x8.narrow_i32x4_u (local.get 0) (local.get 1))))
  (func (export "i32x4_high_extend_narrow_us") (param v128 v128) (result v128)
    (i32x4.extend_low_i16x8_u (i16x8.narrow_i32x4_s (local.get 0) (local.get 1))))
)`);

// ./test/core/simd/simd_conversions.wast:758
assert_return(() => invoke($1, `f32x4_convert_i32x4_s_add`, [i32x4([0x1, 0x2, 0x3, 0x4]), i32x4([0x2, 0x3, 0x4, 0x5])]), [new F32x4Pattern(value('f32', 3), value('f32', 5), value('f32', 7), value('f32', 9))]);

// ./test/core/simd/simd_conversions.wast:761
assert_return(() => invoke($1, `f32x4_convert_i32x4_s_sub`, [i32x4([0x0, 0x1, 0x2, 0x3]), i32x4([0x1, 0x1, 0x1, 0x1])]), [new F32x4Pattern(value('f32', -1), value('f32', 0), value('f32', 1), value('f32', 2))]);

// ./test/core/simd/simd_conversions.wast:764
assert_return(() => invoke($1, `f32x4_convert_i32x4_u_mul`, [i32x4([0x1, 0x2, 0x3, 0x4]), i32x4([0x1, 0x2, 0x3, 0x4])]), [new F32x4Pattern(value('f32', 1), value('f32', 4), value('f32', 9), value('f32', 16))]);

// ./test/core/simd/simd_conversions.wast:768
assert_return(() => invoke($1, `i16x8_low_extend_narrow_ss`, [i16x8([0x8000, 0x8001, 0x7fff, 0x8000, 0x8000, 0x8001, 0x7fff, 0x8000]), i16x8([0x8000, 0x8001, 0x7fff, 0x8000, 0x8000, 0x8001, 0x7fff, 0x8000])]), [i16x8([0xff80, 0xff80, 0x7f, 0xff80, 0xff80, 0xff80, 0x7f, 0xff80])]);

// ./test/core/simd/simd_conversions.wast:771
assert_return(() => invoke($1, `i16x8_low_extend_narrow_su`, [i16x8([0x8000, 0x8001, 0x7fff, 0xffff, 0x8000, 0x8001, 0x7fff, 0xffff]), i16x8([0x8000, 0x8001, 0x7fff, 0xffff, 0x8000, 0x8001, 0x7fff, 0xffff])]), [i16x8([0x0, 0x0, 0xffff, 0x0, 0x0, 0x0, 0xffff, 0x0])]);

// ./test/core/simd/simd_conversions.wast:774
assert_return(() => invoke($1, `i16x8_high_extend_narrow_ss`, [i16x8([0x8000, 0x8001, 0x7fff, 0x8000, 0x8000, 0x8001, 0x7fff, 0x8000]), i16x8([0x8000, 0x8001, 0x7fff, 0x8000, 0x8000, 0x8001, 0x7fff, 0x8000])]), [i16x8([0xff80, 0xff80, 0x7f, 0xff80, 0xff80, 0xff80, 0x7f, 0xff80])]);

// ./test/core/simd/simd_conversions.wast:777
assert_return(() => invoke($1, `i16x8_high_extend_narrow_su`, [i16x8([0x8000, 0x8001, 0x7fff, 0xffff, 0x8000, 0x8001, 0x7fff, 0xffff]), i16x8([0x8000, 0x8001, 0x7fff, 0xffff, 0x8000, 0x8001, 0x7fff, 0xffff])]), [i16x8([0x0, 0x0, 0xffff, 0x0, 0x0, 0x0, 0xffff, 0x0])]);

// ./test/core/simd/simd_conversions.wast:780
assert_return(() => invoke($1, `i16x8_low_extend_narrow_uu`, [i16x8([0x8000, 0x8001, 0x8000, 0xffff, 0x8000, 0x8001, 0x8000, 0xffff]), i16x8([0x8000, 0x8001, 0x8000, 0xffff, 0x8000, 0x8001, 0x8000, 0xffff])]), [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_conversions.wast:783
assert_return(() => invoke($1, `i16x8_low_extend_narrow_us`, [i16x8([0x8000, 0x8001, 0x7fff, 0x8000, 0x8000, 0x8001, 0x7fff, 0x8000]), i16x8([0x8000, 0x8001, 0x7fff, 0x8000, 0x8000, 0x8001, 0x7fff, 0x8000])]), [i16x8([0x80, 0x80, 0x7f, 0x80, 0x80, 0x80, 0x7f, 0x80])]);

// ./test/core/simd/simd_conversions.wast:786
assert_return(() => invoke($1, `i16x8_high_extend_narrow_uu`, [i16x8([0x8000, 0x8001, 0x7fff, 0xffff, 0x8000, 0x8001, 0x7fff, 0xffff]), i16x8([0x8000, 0x8001, 0x7fff, 0xffff, 0x8000, 0x8001, 0x7fff, 0xffff])]), [i16x8([0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0xff, 0x0])]);

// ./test/core/simd/simd_conversions.wast:789
assert_return(() => invoke($1, `i16x8_high_extend_narrow_us`, [i16x8([0x8000, 0x8001, 0x7fff, 0x8000, 0x8000, 0x8001, 0x7fff, 0x8000]), i16x8([0x8000, 0x8001, 0x7fff, 0x8000, 0x8000, 0x8001, 0x7fff, 0x8000])]), [i16x8([0x80, 0x80, 0x7f, 0x80, 0x80, 0x80, 0x7f, 0x80])]);

// ./test/core/simd/simd_conversions.wast:793
assert_return(() => invoke($1, `i32x4_low_extend_narrow_ss`, [i32x4([0x80000000, 0x80000001, 0x7fffffff, 0x8000000]), i32x4([0x80000000, 0x80000001, 0x7fffffff, 0x8000000])]), [i32x4([0xffff8000, 0xffff8000, 0x7fff, 0x7fff])]);

// ./test/core/simd/simd_conversions.wast:796
assert_return(() => invoke($1, `i32x4_low_extend_narrow_su`, [i32x4([0x80000000, 0x80000001, 0x7fffffff, 0xffffffff]), i32x4([0x80000000, 0x80000001, 0x7fffffff, 0xffffffff])]), [i32x4([0x0, 0x0, 0xffffffff, 0x0])]);

// ./test/core/simd/simd_conversions.wast:799
assert_return(() => invoke($1, `i32x4_high_extend_narrow_ss`, [i32x4([0x80000000, 0x80000001, 0x7fffffff, 0x8000000]), i32x4([0x80000000, 0x80000001, 0x7fffffff, 0x8000000])]), [i32x4([0xffff8000, 0xffff8000, 0x7fff, 0x7fff])]);

// ./test/core/simd/simd_conversions.wast:802
assert_return(() => invoke($1, `i32x4_high_extend_narrow_su`, [i32x4([0x80000000, 0x80000001, 0x7fffffff, 0xffffffff]), i32x4([0x80000000, 0x80000001, 0x7fffffff, 0xffffffff])]), [i32x4([0x0, 0x0, 0xffffffff, 0x0])]);

// ./test/core/simd/simd_conversions.wast:805
assert_return(() => invoke($1, `i32x4_low_extend_narrow_uu`, [i32x4([0x80000000, 0x80000001, 0x7fffffff, 0xffffffff]), i32x4([0x80000000, 0x80000001, 0x7fffffff, 0xffffffff])]), [i32x4([0x0, 0x0, 0xffff, 0x0])]);

// ./test/core/simd/simd_conversions.wast:808
assert_return(() => invoke($1, `i32x4_low_extend_narrow_us`, [i32x4([0x80000000, 0x80000001, 0x7fffffff, 0x8000000]), i32x4([0x80000000, 0x80000001, 0x7fffffff, 0x8000000])]), [i32x4([0x8000, 0x8000, 0x7fff, 0x7fff])]);

// ./test/core/simd/simd_conversions.wast:811
assert_return(() => invoke($1, `i32x4_high_extend_narrow_uu`, [i32x4([0x80000000, 0x80000001, 0x7fffffff, 0xffffffff]), i32x4([0x80000000, 0x80000001, 0x7fffffff, 0xffffffff])]), [i32x4([0x0, 0x0, 0xffff, 0x0])]);

// ./test/core/simd/simd_conversions.wast:814
assert_return(() => invoke($1, `i32x4_high_extend_narrow_us`, [i32x4([0x80000000, 0x80000001, 0x7fffffff, 0x8000000]), i32x4([0x80000000, 0x80000001, 0x7fffffff, 0x8000000])]), [i32x4([0x8000, 0x8000, 0x7fff, 0x7fff])]);

// ./test/core/simd/simd_conversions.wast:820
assert_invalid(() => instantiate(`(module
    (func $$f32x4.convert_i32x4_s-arg-empty (result v128)
      (f32x4.convert_i32x4_s)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_conversions.wast:828
assert_invalid(() => instantiate(`(module
    (func $$f32x4.convert_i32x4_u-arg-empty (result v128)
      (f32x4.convert_i32x4_u)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_conversions.wast:836
assert_invalid(() => instantiate(`(module
    (func $$i8x16.narrow_i16x8_s-1st-arg-empty (result v128)
      (i8x16.narrow_i16x8_s (v128.const i16x8 0 0 0 0 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_conversions.wast:844
assert_invalid(() => instantiate(`(module
    (func $$i8x16.narrow_i16x8_s-arg-empty (result v128)
      (i8x16.narrow_i16x8_s)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_conversions.wast:852
assert_invalid(() => instantiate(`(module
    (func $$i8x16.narrow_i16x8_u-1st-arg-empty (result v128)
      (i8x16.narrow_i16x8_u (v128.const i16x8 0 0 0 0 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_conversions.wast:860
assert_invalid(() => instantiate(`(module
    (func $$i8x16.narrow_i16x8_u-arg-empty (result v128)
      (i8x16.narrow_i16x8_u)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_conversions.wast:868
assert_invalid(() => instantiate(`(module
    (func $$i16x8.narrow_i32x4_s-1st-arg-empty (result v128)
      (i16x8.narrow_i32x4_s (v128.const i32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_conversions.wast:876
assert_invalid(() => instantiate(`(module
    (func $$i16x8.narrow_i32x4_s-arg-empty (result v128)
      (i16x8.narrow_i32x4_s)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_conversions.wast:884
assert_invalid(() => instantiate(`(module
    (func $$i16x8.narrow_i32x4_u-1st-arg-empty (result v128)
      (i16x8.narrow_i32x4_u (v128.const i32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_conversions.wast:892
assert_invalid(() => instantiate(`(module
    (func $$i16x8.narrow_i32x4_u-arg-empty (result v128)
      (i16x8.narrow_i32x4_u)
    )
  )`), `type mismatch`);

