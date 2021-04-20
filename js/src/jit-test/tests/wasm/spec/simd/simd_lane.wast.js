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

// ./test/core/simd/simd_lane.wast

// ./test/core/simd/simd_lane.wast:4
let $0 = instantiate(`(module
  (func (export "i8x16_extract_lane_s-first") (param v128) (result i32)
    (i8x16.extract_lane_s 0 (local.get 0)))
  (func (export "i8x16_extract_lane_s-last") (param v128) (result i32)
    (i8x16.extract_lane_s 15 (local.get 0)))
  (func (export "i8x16_extract_lane_u-first") (param v128) (result i32)
    (i8x16.extract_lane_u 0 (local.get 0)))
  (func (export "i8x16_extract_lane_u-last") (param v128) (result i32)
    (i8x16.extract_lane_u 15 (local.get 0)))
  (func (export "i16x8_extract_lane_s-first") (param v128) (result i32)
    (i16x8.extract_lane_s 0 (local.get 0)))
  (func (export "i16x8_extract_lane_s-last") (param v128) (result i32)
    (i16x8.extract_lane_s 7 (local.get 0)))
  (func (export "i16x8_extract_lane_u-first") (param v128) (result i32)
    (i16x8.extract_lane_u 0 (local.get 0)))
  (func (export "i16x8_extract_lane_u-last") (param v128) (result i32)
    (i16x8.extract_lane_u 7 (local.get 0)))
  (func (export "i32x4_extract_lane-first") (param v128) (result i32)
    (i32x4.extract_lane 0 (local.get 0)))
  (func (export "i32x4_extract_lane-last") (param v128) (result i32)
    (i32x4.extract_lane 3 (local.get 0)))
  (func (export "f32x4_extract_lane-first") (param v128) (result f32)
    (f32x4.extract_lane 0 (local.get 0)))
  (func (export "f32x4_extract_lane-last") (param v128) (result f32)
    (f32x4.extract_lane 3 (local.get 0)))
  (func (export "i8x16_replace_lane-first") (param v128 i32) (result v128)
    (i8x16.replace_lane 0 (local.get 0) (local.get 1)))
  (func (export "i8x16_replace_lane-last") (param v128 i32) (result v128)
    (i8x16.replace_lane 15 (local.get 0) (local.get 1)))
  (func (export "i16x8_replace_lane-first") (param v128 i32) (result v128)
    (i16x8.replace_lane 0 (local.get 0) (local.get 1)))
  (func (export "i16x8_replace_lane-last") (param v128 i32) (result v128)
    (i16x8.replace_lane 7 (local.get 0) (local.get 1)))
  (func (export "i32x4_replace_lane-first") (param v128 i32) (result v128)
    (i32x4.replace_lane 0 (local.get 0) (local.get 1)))
  (func (export "i32x4_replace_lane-last") (param v128 i32) (result v128)
    (i32x4.replace_lane 3 (local.get 0) (local.get 1)))
  (func (export "f32x4_replace_lane-first") (param v128 f32) (result v128)
    (f32x4.replace_lane 0 (local.get 0) (local.get 1)))
  (func (export "f32x4_replace_lane-last") (param v128 f32) (result v128)
    (f32x4.replace_lane 3 (local.get 0) (local.get 1)))
  (func (export "i64x2_extract_lane-first") (param v128) (result i64)
    (i64x2.extract_lane 0 (local.get 0)))
  (func (export "i64x2_extract_lane-last") (param v128) (result i64)
    (i64x2.extract_lane 1 (local.get 0)))
  (func (export "f64x2_extract_lane-first") (param v128) (result f64)
    (f64x2.extract_lane 0 (local.get 0)))
  (func (export "f64x2_extract_lane-last") (param v128) (result f64)
    (f64x2.extract_lane 1 (local.get 0)))
  (func (export "i64x2_replace_lane-first") (param v128 i64) (result v128)
    (i64x2.replace_lane 0 (local.get 0) (local.get 1)))
  (func (export "i64x2_replace_lane-last") (param v128 i64) (result v128)
    (i64x2.replace_lane 1 (local.get 0) (local.get 1)))
  (func (export "f64x2_replace_lane-first") (param v128 f64) (result v128)
    (f64x2.replace_lane 0 (local.get 0) (local.get 1)))
  (func (export "f64x2_replace_lane-last") (param v128 f64) (result v128)
    (f64x2.replace_lane 1 (local.get 0) (local.get 1)))

  ;; Swizzle and shuffle
  (func (export "v8x16_swizzle") (param v128 v128) (result v128)
    (i8x16.swizzle (local.get 0) (local.get 1)))
  (func (export "v8x16_shuffle-1") (param v128 v128) (result v128)
    (i8x16.shuffle  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 (local.get 0) (local.get 1)))
  (func (export "v8x16_shuffle-2") (param v128 v128) (result v128)
    (i8x16.shuffle 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 (local.get 0) (local.get 1)))
  (func (export "v8x16_shuffle-3") (param v128 v128) (result v128)
    (i8x16.shuffle 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 (local.get 0) (local.get 1)))
  (func (export "v8x16_shuffle-4") (param v128 v128) (result v128)
    (i8x16.shuffle 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0 (local.get 0) (local.get 1)))
  (func (export "v8x16_shuffle-5") (param v128 v128) (result v128)
    (i8x16.shuffle  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  0 (local.get 0) (local.get 1)))
  (func (export "v8x16_shuffle-6") (param v128 v128) (result v128)
    (i8x16.shuffle 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 (local.get 0) (local.get 1)))
  (func (export "v8x16_shuffle-7") (param v128 v128) (result v128)
    (i8x16.shuffle  0  0  0  0  0  0  0  0 16 16 16 16 16 16 16 16 (local.get 0) (local.get 1)))
)`);

// ./test/core/simd/simd_lane.wast:81
assert_return(() => invoke($0, `i8x16_extract_lane_s-first`, [i8x16([0x7f, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]), [value('i32', 127)]);

// ./test/core/simd/simd_lane.wast:82
assert_return(() => invoke($0, `i8x16_extract_lane_s-first`, [i8x16([0x7f, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]), [value('i32', 127)]);

// ./test/core/simd/simd_lane.wast:83
assert_return(() => invoke($0, `i8x16_extract_lane_s-first`, [i8x16([0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]), [value('i32', -1)]);

// ./test/core/simd/simd_lane.wast:84
assert_return(() => invoke($0, `i8x16_extract_lane_s-first`, [i8x16([0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]), [value('i32', -1)]);

// ./test/core/simd/simd_lane.wast:85
assert_return(() => invoke($0, `i8x16_extract_lane_u-first`, [i8x16([0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]), [value('i32', 255)]);

// ./test/core/simd/simd_lane.wast:86
assert_return(() => invoke($0, `i8x16_extract_lane_u-first`, [i8x16([0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]), [value('i32', 255)]);

// ./test/core/simd/simd_lane.wast:87
assert_return(() => invoke($0, `i8x16_extract_lane_s-last`, [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x80])]), [value('i32', -128)]);

// ./test/core/simd/simd_lane.wast:88
assert_return(() => invoke($0, `i8x16_extract_lane_s-last`, [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x80])]), [value('i32', -128)]);

// ./test/core/simd/simd_lane.wast:89
assert_return(() => invoke($0, `i8x16_extract_lane_u-last`, [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xff])]), [value('i32', 255)]);

// ./test/core/simd/simd_lane.wast:90
assert_return(() => invoke($0, `i8x16_extract_lane_u-last`, [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xff])]), [value('i32', 255)]);

// ./test/core/simd/simd_lane.wast:91
assert_return(() => invoke($0, `i8x16_extract_lane_u-last`, [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x80])]), [value('i32', 128)]);

// ./test/core/simd/simd_lane.wast:92
assert_return(() => invoke($0, `i8x16_extract_lane_u-last`, [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x80])]), [value('i32', 128)]);

// ./test/core/simd/simd_lane.wast:94
assert_return(() => invoke($0, `i16x8_extract_lane_s-first`, [i16x8([0x7fff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]), [value('i32', 32767)]);

// ./test/core/simd/simd_lane.wast:95
assert_return(() => invoke($0, `i16x8_extract_lane_s-first`, [i16x8([0x7fff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]), [value('i32', 32767)]);

// ./test/core/simd/simd_lane.wast:96
assert_return(() => invoke($0, `i16x8_extract_lane_s-first`, [i16x8([0xffff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]), [value('i32', -1)]);

// ./test/core/simd/simd_lane.wast:97
assert_return(() => invoke($0, `i16x8_extract_lane_s-first`, [i16x8([0xffff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]), [value('i32', -1)]);

// ./test/core/simd/simd_lane.wast:98
assert_return(() => invoke($0, `i16x8_extract_lane_s-first`, [i16x8([0x3039, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]), [value('i32', 12345)]);

// ./test/core/simd/simd_lane.wast:99
assert_return(() => invoke($0, `i16x8_extract_lane_s-first`, [i16x8([0xedcc, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]), [value('i32', -4660)]);

// ./test/core/simd/simd_lane.wast:100
assert_return(() => invoke($0, `i16x8_extract_lane_u-first`, [i16x8([0xffff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]), [value('i32', 65535)]);

// ./test/core/simd/simd_lane.wast:101
assert_return(() => invoke($0, `i16x8_extract_lane_u-first`, [i16x8([0xffff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]), [value('i32', 65535)]);

// ./test/core/simd/simd_lane.wast:102
assert_return(() => invoke($0, `i16x8_extract_lane_u-first`, [i16x8([0x3039, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]), [value('i32', 12345)]);

// ./test/core/simd/simd_lane.wast:103
assert_return(() => invoke($0, `i16x8_extract_lane_u-first`, [i16x8([0xedcc, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]), [value('i32', 60876)]);

// ./test/core/simd/simd_lane.wast:104
assert_return(() => invoke($0, `i16x8_extract_lane_s-last`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8000])]), [value('i32', -32768)]);

// ./test/core/simd/simd_lane.wast:105
assert_return(() => invoke($0, `i16x8_extract_lane_s-last`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8000])]), [value('i32', -32768)]);

// ./test/core/simd/simd_lane.wast:106
assert_return(() => invoke($0, `i16x8_extract_lane_s-last`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1a85])]), [value('i32', 6789)]);

// ./test/core/simd/simd_lane.wast:107
assert_return(() => invoke($0, `i16x8_extract_lane_s-last`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x9877])]), [value('i32', -26505)]);

// ./test/core/simd/simd_lane.wast:108
assert_return(() => invoke($0, `i16x8_extract_lane_u-last`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xffff])]), [value('i32', 65535)]);

// ./test/core/simd/simd_lane.wast:109
assert_return(() => invoke($0, `i16x8_extract_lane_u-last`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xffff])]), [value('i32', 65535)]);

// ./test/core/simd/simd_lane.wast:110
assert_return(() => invoke($0, `i16x8_extract_lane_u-last`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8000])]), [value('i32', 32768)]);

// ./test/core/simd/simd_lane.wast:111
assert_return(() => invoke($0, `i16x8_extract_lane_u-last`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8000])]), [value('i32', 32768)]);

// ./test/core/simd/simd_lane.wast:112
assert_return(() => invoke($0, `i16x8_extract_lane_u-last`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1a85])]), [value('i32', 6789)]);

// ./test/core/simd/simd_lane.wast:113
assert_return(() => invoke($0, `i16x8_extract_lane_u-last`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x9877])]), [value('i32', 39031)]);

// ./test/core/simd/simd_lane.wast:115
assert_return(() => invoke($0, `i32x4_extract_lane-first`, [i32x4([0x7fffffff, 0x0, 0x0, 0x0])]), [value('i32', 2147483647)]);

// ./test/core/simd/simd_lane.wast:116
assert_return(() => invoke($0, `i32x4_extract_lane-first`, [i32x4([0x7fffffff, 0x0, 0x0, 0x0])]), [value('i32', 2147483647)]);

// ./test/core/simd/simd_lane.wast:117
assert_return(() => invoke($0, `i32x4_extract_lane-first`, [i32x4([0xffffffff, 0x0, 0x0, 0x0])]), [value('i32', -1)]);

// ./test/core/simd/simd_lane.wast:118
assert_return(() => invoke($0, `i32x4_extract_lane-first`, [i32x4([0xffffffff, 0x0, 0x0, 0x0])]), [value('i32', -1)]);

// ./test/core/simd/simd_lane.wast:119
assert_return(() => invoke($0, `i32x4_extract_lane-first`, [i32x4([0x499602d2, 0x0, 0x0, 0x0])]), [value('i32', 1234567890)]);

// ./test/core/simd/simd_lane.wast:120
assert_return(() => invoke($0, `i32x4_extract_lane-first`, [i32x4([0xedcba988, 0x0, 0x0, 0x0])]), [value('i32', -305419896)]);

// ./test/core/simd/simd_lane.wast:121
assert_return(() => invoke($0, `i32x4_extract_lane-last`, [i32x4([0x0, 0x0, 0x0, 0x80000000])]), [value('i32', -2147483648)]);

// ./test/core/simd/simd_lane.wast:122
assert_return(() => invoke($0, `i32x4_extract_lane-last`, [i32x4([0x0, 0x0, 0x0, 0x80000000])]), [value('i32', -2147483648)]);

// ./test/core/simd/simd_lane.wast:123
assert_return(() => invoke($0, `i32x4_extract_lane-last`, [i32x4([0x0, 0x0, 0x0, 0xffffffff])]), [value('i32', -1)]);

// ./test/core/simd/simd_lane.wast:124
assert_return(() => invoke($0, `i32x4_extract_lane-last`, [i32x4([0x0, 0x0, 0x0, 0xffffffff])]), [value('i32', -1)]);

// ./test/core/simd/simd_lane.wast:125
assert_return(() => invoke($0, `i32x4_extract_lane-last`, [i32x4([0x0, 0x0, 0x0, 0x3ade68b1])]), [value('i32', 987654321)]);

// ./test/core/simd/simd_lane.wast:126
assert_return(() => invoke($0, `i32x4_extract_lane-last`, [i32x4([0x0, 0x0, 0x0, 0xedcba988])]), [value('i32', -305419896)]);

// ./test/core/simd/simd_lane.wast:128
assert_return(() => invoke($0, `i64x2_extract_lane-first`, [i64x2([0x7fffffffffffffffn, 0x0n])]), [value('i64', 9223372036854775807n)]);

// ./test/core/simd/simd_lane.wast:129
assert_return(() => invoke($0, `i64x2_extract_lane-first`, [i64x2([0x7ffffffffffffffen, 0x0n])]), [value('i64', 9223372036854775806n)]);

// ./test/core/simd/simd_lane.wast:130
assert_return(() => invoke($0, `i64x2_extract_lane-first`, [i64x2([0xffffffffffffffffn, 0x0n])]), [value('i64', -1n)]);

// ./test/core/simd/simd_lane.wast:131
assert_return(() => invoke($0, `i64x2_extract_lane-first`, [i64x2([0xffffffffffffffffn, 0x0n])]), [value('i64', -1n)]);

// ./test/core/simd/simd_lane.wast:132
assert_return(() => invoke($0, `i64x2_extract_lane-first`, [i64x2([0x112210f47de98115n, 0x0n])]), [value('i64', 1234567890123456789n)]);

// ./test/core/simd/simd_lane.wast:133
assert_return(() => invoke($0, `i64x2_extract_lane-first`, [i64x2([0x1234567890abcdefn, 0x0n])]), [value('i64', 1311768467294899695n)]);

// ./test/core/simd/simd_lane.wast:134
assert_return(() => invoke($0, `i64x2_extract_lane-last`, [i64x2([0x0n, 0x8000000000000000n])]), [value('i64', -9223372036854775808n)]);

// ./test/core/simd/simd_lane.wast:135
assert_return(() => invoke($0, `i64x2_extract_lane-last`, [i64x2([0x0n, 0x8000000000000000n])]), [value('i64', -9223372036854775808n)]);

// ./test/core/simd/simd_lane.wast:136
assert_return(() => invoke($0, `i64x2_extract_lane-last`, [i64x2([0x0n, 0x8000000000000000n])]), [value('i64', -9223372036854775808n)]);

// ./test/core/simd/simd_lane.wast:137
assert_return(() => invoke($0, `i64x2_extract_lane-last`, [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f])]), [value('i64', 9223372036854775807n)]);

// ./test/core/simd/simd_lane.wast:138
assert_return(() => invoke($0, `i64x2_extract_lane-last`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8000])]), [value('i64', -9223372036854775808n)]);

// ./test/core/simd/simd_lane.wast:139
assert_return(() => invoke($0, `i64x2_extract_lane-last`, [i32x4([0x0, 0x0, 0xffffffff, 0x7fffffff])]), [value('i64', 9223372036854775807n)]);

// ./test/core/simd/simd_lane.wast:140
assert_return(() => invoke($0, `i64x2_extract_lane-last`, [f64x2([-Infinity, Infinity])]), [value('i64', 9218868437227405312n)]);

// ./test/core/simd/simd_lane.wast:141
assert_return(() => invoke($0, `i64x2_extract_lane-last`, [i64x2([0x0n, 0x112210f47de98115n])]), [value('i64', 1234567890123456789n)]);

// ./test/core/simd/simd_lane.wast:142
assert_return(() => invoke($0, `i64x2_extract_lane-last`, [i64x2([0x0n, 0x1234567890abcdefn])]), [value('i64', 1311768467294899695n)]);

// ./test/core/simd/simd_lane.wast:144
assert_return(() => invoke($0, `f32x4_extract_lane-first`, [f32x4([-5, 0, 0, 0])]), [value('f32', -5)]);

// ./test/core/simd/simd_lane.wast:145
assert_return(() => invoke($0, `f32x4_extract_lane-first`, [f32x4([100000000000000000000000000000000000000, 0, 0, 0])]), [value('f32', 100000000000000000000000000000000000000)]);

// ./test/core/simd/simd_lane.wast:146
assert_return(() => invoke($0, `f32x4_extract_lane-first`, [f32x4([340282350000000000000000000000000000000, 0, 0, 0])]), [value('f32', 340282350000000000000000000000000000000)]);

// ./test/core/simd/simd_lane.wast:147
assert_return(() => invoke($0, `f32x4_extract_lane-first`, [f32x4([170141180000000000000000000000000000000, 0, 0, 0])]), [value('f32', 170141180000000000000000000000000000000)]);

// ./test/core/simd/simd_lane.wast:148
assert_return(() => invoke($0, `f32x4_extract_lane-first`, [f32x4([Infinity, 0, 0, 0])]), [value('f32', Infinity)]);

// ./test/core/simd/simd_lane.wast:149
assert_return(() => invoke($0, `f32x4_extract_lane-first`, [bytes('v128', [0x0, 0x0, 0xc0, 0x7f, 0x0, 0x0, 0x80, 0x7f, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]), [bytes('f32', [0x0, 0x0, 0xc0, 0x7f])]);

// ./test/core/simd/simd_lane.wast:150
assert_return(() => invoke($0, `f32x4_extract_lane-first`, [f32x4([1234567900000000000000000000, 0, 0, 0])]), [value('f32', 1234567900000000000000000000)]);

// ./test/core/simd/simd_lane.wast:151
assert_return(() => invoke($0, `f32x4_extract_lane-first`, [f32x4([156374990000, 0, 0, 0])]), [value('f32', 156374990000)]);

// ./test/core/simd/simd_lane.wast:152
assert_return(() => invoke($0, `f32x4_extract_lane-last`, [f32x4([0, 0, 0, -100000000000000000000000000000000000000])]), [value('f32', -100000000000000000000000000000000000000)]);

// ./test/core/simd/simd_lane.wast:153
assert_return(() => invoke($0, `f32x4_extract_lane-last`, [f32x4([0, 0, 0, -340282350000000000000000000000000000000])]), [value('f32', -340282350000000000000000000000000000000)]);

// ./test/core/simd/simd_lane.wast:154
assert_return(() => invoke($0, `f32x4_extract_lane-last`, [f32x4([0, 0, 0, -170141180000000000000000000000000000000])]), [value('f32', -170141180000000000000000000000000000000)]);

// ./test/core/simd/simd_lane.wast:155
assert_return(() => invoke($0, `f32x4_extract_lane-last`, [f32x4([0, 0, 0, -Infinity])]), [value('f32', -Infinity)]);

// ./test/core/simd/simd_lane.wast:156
assert_return(() => invoke($0, `f32x4_extract_lane-last`, [bytes('v128', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x80, 0xff, 0x0, 0x0, 0xc0, 0x7f])]), [bytes('f32', [0x0, 0x0, 0xc0, 0x7f])]);

// ./test/core/simd/simd_lane.wast:157
assert_return(() => invoke($0, `f32x4_extract_lane-last`, [f32x4([0, 0, 0, 123456790])]), [value('f32', 123456790)]);

// ./test/core/simd/simd_lane.wast:158
assert_return(() => invoke($0, `f32x4_extract_lane-last`, [f32x4([0, 0, 0, 81985530000000000])]), [value('f32', 81985530000000000)]);

// ./test/core/simd/simd_lane.wast:160
assert_return(() => invoke($0, `f64x2_extract_lane-first`, [f64x2([-1.5, 0])]), [value('f64', -1.5)]);

// ./test/core/simd/simd_lane.wast:161
assert_return(() => invoke($0, `f64x2_extract_lane-first`, [f64x2([1.5, 0])]), [value('f64', 1.5)]);

// ./test/core/simd/simd_lane.wast:162
assert_return(() => invoke($0, `f64x2_extract_lane-first`, [f64x2([-0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000017976931348623155, 0])]), [value('f64', -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000017976931348623155)]);

// ./test/core/simd/simd_lane.wast:163
assert_return(() => invoke($0, `f64x2_extract_lane-first`, [f64x2([0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000017976931348623155, 0])]), [value('f64', 0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000017976931348623155)]);

// ./test/core/simd/simd_lane.wast:164
assert_return(() => invoke($0, `f64x2_extract_lane-first`, [f64x2([-0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000022250738585072014, 0])]), [value('f64', -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000022250738585072014)]);

// ./test/core/simd/simd_lane.wast:165
assert_return(() => invoke($0, `f64x2_extract_lane-first`, [f64x2([0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000022250738585072014, 0])]), [value('f64', 0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000022250738585072014)]);

// ./test/core/simd/simd_lane.wast:166
assert_return(() => invoke($0, `f64x2_extract_lane-first`, [f64x2([-Infinity, 0])]), [value('f64', -Infinity)]);

// ./test/core/simd/simd_lane.wast:167
assert_return(() => invoke($0, `f64x2_extract_lane-first`, [f64x2([Infinity, 0])]), [value('f64', Infinity)]);

// ./test/core/simd/simd_lane.wast:168
assert_return(() => invoke($0, `f64x2_extract_lane-first`, [bytes('v128', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x80])]), [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff])]);

// ./test/core/simd/simd_lane.wast:169
assert_return(() => invoke($0, `f64x2_extract_lane-first`, [bytes('v128', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]), [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])]);

// ./test/core/simd/simd_lane.wast:170
assert_return(() => invoke($0, `f64x2_extract_lane-first`, [f64x2([1234567890123456900000000000, 0])]), [value('f64', 1234567890123456900000000000)]);

// ./test/core/simd/simd_lane.wast:171
assert_return(() => invoke($0, `f64x2_extract_lane-first`, [f64x2([2623536934927580700, 0])]), [value('f64', 2623536934927580700)]);

// ./test/core/simd/simd_lane.wast:172
assert_return(() => invoke($0, `f64x2_extract_lane-last`, [f64x2([0, 2.25])]), [value('f64', 2.25)]);

// ./test/core/simd/simd_lane.wast:173
assert_return(() => invoke($0, `f64x2_extract_lane-last`, [f64x2([0, -2.25])]), [value('f64', -2.25)]);

// ./test/core/simd/simd_lane.wast:174
assert_return(() => invoke($0, `f64x2_extract_lane-last`, [f64x2([0, -179769313486231570000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000])]), [value('f64', -179769313486231570000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/simd/simd_lane.wast:175
assert_return(() => invoke($0, `f64x2_extract_lane-last`, [f64x2([0, 179769313486231570000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000])]), [value('f64', 179769313486231570000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/simd/simd_lane.wast:176
assert_return(() => invoke($0, `f64x2_extract_lane-last`, [f64x2([0, -179769313486231570000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000])]), [value('f64', -179769313486231570000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/simd/simd_lane.wast:177
assert_return(() => invoke($0, `f64x2_extract_lane-last`, [f64x2([0, 179769313486231570000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000])]), [value('f64', 179769313486231570000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/simd/simd_lane.wast:178
assert_return(() => invoke($0, `f64x2_extract_lane-last`, [f64x2([-0, -Infinity])]), [value('f64', -Infinity)]);

// ./test/core/simd/simd_lane.wast:179
assert_return(() => invoke($0, `f64x2_extract_lane-last`, [f64x2([0, Infinity])]), [value('f64', Infinity)]);

// ./test/core/simd/simd_lane.wast:180
assert_return(() => invoke($0, `f64x2_extract_lane-last`, [bytes('v128', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff])]), [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff])]);

// ./test/core/simd/simd_lane.wast:181
assert_return(() => invoke($0, `f64x2_extract_lane-last`, [bytes('v128', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])]), [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])]);

// ./test/core/simd/simd_lane.wast:182
assert_return(() => invoke($0, `f64x2_extract_lane-last`, [f64x2([0, 123456789])]), [value('f64', 123456789)]);

// ./test/core/simd/simd_lane.wast:183
assert_return(() => invoke($0, `f64x2_extract_lane-last`, [f64x2([0, 1375488932539311400000000])]), [value('f64', 1375488932539311400000000)]);

// ./test/core/simd/simd_lane.wast:185
assert_return(() => invoke($0, `f64x2_extract_lane-last`, [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]), [value('f64', 0)]);

// ./test/core/simd/simd_lane.wast:186
assert_return(() => invoke($0, `f64x2_extract_lane-last`, [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x80])]), [value('f64', -0)]);

// ./test/core/simd/simd_lane.wast:187
assert_return(() => invoke($0, `f64x2_extract_lane-last`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x4000])]), [value('f64', 2)]);

// ./test/core/simd/simd_lane.wast:188
assert_return(() => invoke($0, `f64x2_extract_lane-last`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xc000])]), [value('f64', -2)]);

// ./test/core/simd/simd_lane.wast:189
assert_return(() => invoke($0, `f64x2_extract_lane-last`, [i32x4([0x0, 0x0, 0xffffffff, 0x7fefffff])]), [value('f64', 179769313486231570000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]);

// ./test/core/simd/simd_lane.wast:190
assert_return(() => invoke($0, `f64x2_extract_lane-last`, [i32x4([0x0, 0x0, 0x0, 0x100000])]), [value('f64', 0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000022250738585072014)]);

// ./test/core/simd/simd_lane.wast:191
assert_return(() => invoke($0, `f64x2_extract_lane-last`, [i32x4([0x0, 0x0, 0xffffffff, 0xfffff])]), [value('f64', 0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002225073858507201)]);

// ./test/core/simd/simd_lane.wast:192
assert_return(() => invoke($0, `f64x2_extract_lane-last`, [i32x4([0x0, 0x0, 0x1, 0x0])]), [value('f64', 0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005)]);

// ./test/core/simd/simd_lane.wast:194
assert_return(() => invoke($0, `i8x16_replace_lane-first`, [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), 127]), [i8x16([0x7f, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_lane.wast:195
assert_return(() => invoke($0, `i8x16_replace_lane-first`, [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), 128]), [i8x16([0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_lane.wast:196
assert_return(() => invoke($0, `i8x16_replace_lane-first`, [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), 255]), [i8x16([0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_lane.wast:197
assert_return(() => invoke($0, `i8x16_replace_lane-first`, [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), 256]), [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_lane.wast:198
assert_return(() => invoke($0, `i8x16_replace_lane-last`, [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), -128]), [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x80])]);

// ./test/core/simd/simd_lane.wast:199
assert_return(() => invoke($0, `i8x16_replace_lane-last`, [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), -129]), [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7f])]);

// ./test/core/simd/simd_lane.wast:200
assert_return(() => invoke($0, `i8x16_replace_lane-last`, [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), 32767]), [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xff])]);

// ./test/core/simd/simd_lane.wast:201
assert_return(() => invoke($0, `i8x16_replace_lane-last`, [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), -32768]), [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_lane.wast:203
assert_return(() => invoke($0, `i16x8_replace_lane-first`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), 32767]), [i16x8([0x7fff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_lane.wast:204
assert_return(() => invoke($0, `i16x8_replace_lane-first`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), 32768]), [i16x8([0x8000, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_lane.wast:205
assert_return(() => invoke($0, `i16x8_replace_lane-first`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), 65535]), [i16x8([0xffff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_lane.wast:206
assert_return(() => invoke($0, `i16x8_replace_lane-first`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), 65536]), [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_lane.wast:207
assert_return(() => invoke($0, `i16x8_replace_lane-first`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), 12345]), [i16x8([0x3039, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_lane.wast:208
assert_return(() => invoke($0, `i16x8_replace_lane-first`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), -4660]), [i16x8([0xedcc, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_lane.wast:209
assert_return(() => invoke($0, `i16x8_replace_lane-last`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), -32768]), [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8000])]);

// ./test/core/simd/simd_lane.wast:210
assert_return(() => invoke($0, `i16x8_replace_lane-last`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), -32769]), [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7fff])]);

// ./test/core/simd/simd_lane.wast:211
assert_return(() => invoke($0, `i16x8_replace_lane-last`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), 2147483647]), [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xffff])]);

// ./test/core/simd/simd_lane.wast:212
assert_return(() => invoke($0, `i16x8_replace_lane-last`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), -2147483648]), [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_lane.wast:213
assert_return(() => invoke($0, `i16x8_replace_lane-last`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), 54321]), [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xd431])]);

// ./test/core/simd/simd_lane.wast:214
assert_return(() => invoke($0, `i16x8_replace_lane-last`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), -17185]), [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xbcdf])]);

// ./test/core/simd/simd_lane.wast:216
assert_return(() => invoke($0, `i32x4_replace_lane-first`, [i32x4([0x0, 0x0, 0x0, 0x0]), 2147483647]), [i32x4([0x7fffffff, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_lane.wast:217
assert_return(() => invoke($0, `i32x4_replace_lane-first`, [i32x4([0x0, 0x0, 0x0, 0x0]), -1]), [i32x4([0xffffffff, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_lane.wast:218
assert_return(() => invoke($0, `i32x4_replace_lane-first`, [i32x4([0x0, 0x0, 0x0, 0x0]), 1234567890]), [i32x4([0x499602d2, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_lane.wast:219
assert_return(() => invoke($0, `i32x4_replace_lane-first`, [i32x4([0x0, 0x0, 0x0, 0x0]), -305419896]), [i32x4([0xedcba988, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_lane.wast:220
assert_return(() => invoke($0, `i32x4_replace_lane-last`, [i32x4([0x0, 0x0, 0x0, 0x0]), -2147483648]), [i32x4([0x0, 0x0, 0x0, 0x80000000])]);

// ./test/core/simd/simd_lane.wast:221
assert_return(() => invoke($0, `i32x4_replace_lane-last`, [i32x4([0x0, 0x0, 0x0, 0x0]), -2147483648]), [i32x4([0x0, 0x0, 0x0, 0x80000000])]);

// ./test/core/simd/simd_lane.wast:222
assert_return(() => invoke($0, `i32x4_replace_lane-last`, [i32x4([0x0, 0x0, 0x0, 0x0]), 1234567890]), [i32x4([0x0, 0x0, 0x0, 0x499602d2])]);

// ./test/core/simd/simd_lane.wast:223
assert_return(() => invoke($0, `i32x4_replace_lane-last`, [i32x4([0x0, 0x0, 0x0, 0x0]), -305419896]), [i32x4([0x0, 0x0, 0x0, 0xedcba988])]);

// ./test/core/simd/simd_lane.wast:225
assert_return(() => invoke($0, `f32x4_replace_lane-first`, [f32x4([0, 0, 0, 0]), value('f32', 53)]), [new F32x4Pattern(value('f32', 53), value('f32', 0), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_lane.wast:226
assert_return(() => invoke($0, `f32x4_replace_lane-first`, [i32x4([0x0, 0x0, 0x0, 0x0]), value('f32', 53)]), [new F32x4Pattern(value('f32', 53), value('f32', 0), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_lane.wast:227
assert_return(() => invoke($0, `f32x4_replace_lane-first`, [f32x4([0, 0, 0, 0]), bytes('f32', [0x0, 0x0, 0xc0, 0x7f])]), [new F32x4Pattern(bytes('f32', [0x0, 0x0, 0xc0, 0x7f]), value('f32', 0), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_lane.wast:228
assert_return(() => invoke($0, `f32x4_replace_lane-first`, [f32x4([0, 0, 0, 0]), value('f32', Infinity)]), [new F32x4Pattern(value('f32', Infinity), value('f32', 0), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_lane.wast:229
assert_return(() => invoke($0, `f32x4_replace_lane-first`, [bytes('v128', [0x0, 0x0, 0xc0, 0x7f, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), value('f32', 3.14)]), [new F32x4Pattern(value('f32', 3.14), value('f32', 0), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_lane.wast:230
assert_return(() => invoke($0, `f32x4_replace_lane-first`, [f32x4([Infinity, 0, 0, 0]), value('f32', 100000000000000000000000000000000000000)]), [new F32x4Pattern(value('f32', 100000000000000000000000000000000000000), value('f32', 0), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_lane.wast:231
assert_return(() => invoke($0, `f32x4_replace_lane-first`, [f32x4([Infinity, 0, 0, 0]), value('f32', 340282350000000000000000000000000000000)]), [new F32x4Pattern(value('f32', 340282350000000000000000000000000000000), value('f32', 0), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_lane.wast:232
assert_return(() => invoke($0, `f32x4_replace_lane-first`, [f32x4([Infinity, 0, 0, 0]), value('f32', 170141180000000000000000000000000000000)]), [new F32x4Pattern(value('f32', 170141180000000000000000000000000000000), value('f32', 0), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_lane.wast:233
assert_return(() => invoke($0, `f32x4_replace_lane-first`, [f32x4([0, 0, 0, 0]), value('f32', 123456790)]), [new F32x4Pattern(value('f32', 123456790), value('f32', 0), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_lane.wast:234
assert_return(() => invoke($0, `f32x4_replace_lane-first`, [f32x4([0, 0, 0, 0]), value('f32', 123456790)]), [new F32x4Pattern(value('f32', 123456790), value('f32', 0), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_lane.wast:235
assert_return(() => invoke($0, `f32x4_replace_lane-first`, [f32x4([0, 0, 0, 0]), value('f32', 81985530000000000)]), [new F32x4Pattern(value('f32', 81985530000000000), value('f32', 0), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_lane.wast:236
assert_return(() => invoke($0, `f32x4_replace_lane-first`, [f32x4([0, 0, 0, 0]), value('f32', 81985530000000000)]), [new F32x4Pattern(value('f32', 81985530000000000), value('f32', 0), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_lane.wast:237
assert_return(() => invoke($0, `f32x4_replace_lane-last`, [f32x4([0, 0, 0, 0]), value('f32', -53)]), [new F32x4Pattern(value('f32', 0), value('f32', 0), value('f32', 0), value('f32', -53))]);

// ./test/core/simd/simd_lane.wast:238
assert_return(() => invoke($0, `f32x4_replace_lane-last`, [i32x4([0x0, 0x0, 0x0, 0x0]), value('f32', -53)]), [new F32x4Pattern(value('f32', 0), value('f32', 0), value('f32', 0), value('f32', -53))]);

// ./test/core/simd/simd_lane.wast:239
assert_return(() => invoke($0, `f32x4_replace_lane-last`, [f32x4([0, 0, 0, 0]), bytes('f32', [0x0, 0x0, 0xc0, 0x7f])]), [new F32x4Pattern(value('f32', 0), value('f32', 0), value('f32', 0), bytes('f32', [0x0, 0x0, 0xc0, 0x7f]))]);

// ./test/core/simd/simd_lane.wast:240
assert_return(() => invoke($0, `f32x4_replace_lane-last`, [f32x4([0, 0, 0, 0]), value('f32', -Infinity)]), [new F32x4Pattern(value('f32', 0), value('f32', 0), value('f32', 0), value('f32', -Infinity))]);

// ./test/core/simd/simd_lane.wast:241
assert_return(() => invoke($0, `f32x4_replace_lane-last`, [bytes('v128', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xc0, 0x7f]), value('f32', 3.14)]), [new F32x4Pattern(value('f32', 0), value('f32', 0), value('f32', 0), value('f32', 3.14))]);

// ./test/core/simd/simd_lane.wast:242
assert_return(() => invoke($0, `f32x4_replace_lane-last`, [f32x4([0, 0, 0, -Infinity]), value('f32', -100000000000000000000000000000000000000)]), [new F32x4Pattern(value('f32', 0), value('f32', 0), value('f32', 0), value('f32', -100000000000000000000000000000000000000))]);

// ./test/core/simd/simd_lane.wast:243
assert_return(() => invoke($0, `f32x4_replace_lane-last`, [f32x4([0, 0, 0, -Infinity]), value('f32', -340282350000000000000000000000000000000)]), [new F32x4Pattern(value('f32', 0), value('f32', 0), value('f32', 0), value('f32', -340282350000000000000000000000000000000))]);

// ./test/core/simd/simd_lane.wast:244
assert_return(() => invoke($0, `f32x4_replace_lane-last`, [f32x4([0, 0, 0, -Infinity]), value('f32', -170141180000000000000000000000000000000)]), [new F32x4Pattern(value('f32', 0), value('f32', 0), value('f32', 0), value('f32', -170141180000000000000000000000000000000))]);

// ./test/core/simd/simd_lane.wast:245
assert_return(() => invoke($0, `f32x4_replace_lane-last`, [f32x4([0, 0, 0, 0]), value('f32', 1234567900000000000000000000)]), [new F32x4Pattern(value('f32', 0), value('f32', 0), value('f32', 0), value('f32', 1234567900000000000000000000))]);

// ./test/core/simd/simd_lane.wast:246
assert_return(() => invoke($0, `f32x4_replace_lane-last`, [f32x4([0, 0, 0, 0]), value('f32', 1234567900000000000000000000)]), [new F32x4Pattern(value('f32', 0), value('f32', 0), value('f32', 0), value('f32', 1234567900000000000000000000))]);

// ./test/core/simd/simd_lane.wast:247
assert_return(() => invoke($0, `f32x4_replace_lane-last`, [f32x4([0, 0, 0, 0]), value('f32', 42984030000000000000000)]), [new F32x4Pattern(value('f32', 0), value('f32', 0), value('f32', 0), value('f32', 42984030000000000000000))]);

// ./test/core/simd/simd_lane.wast:248
assert_return(() => invoke($0, `f32x4_replace_lane-last`, [f32x4([0, 0, 0, 0]), value('f32', 156374990000)]), [new F32x4Pattern(value('f32', 0), value('f32', 0), value('f32', 0), value('f32', 156374990000))]);

// ./test/core/simd/simd_lane.wast:250
assert_return(() => invoke($0, `i64x2_replace_lane-first`, [i64x2([0x0n, 0x0n]), 9223372036854775807n]), [i64x2([0x7fffffffffffffffn, 0x0n])]);

// ./test/core/simd/simd_lane.wast:251
assert_return(() => invoke($0, `i64x2_replace_lane-first`, [i64x2([0x0n, 0x0n]), -1n]), [i64x2([0xffffffffffffffffn, 0x0n])]);

// ./test/core/simd/simd_lane.wast:252
assert_return(() => invoke($0, `i64x2_replace_lane-first`, [i64x2([0x0n, 0x0n]), 1234567890123456789n]), [i64x2([0x112210f47de98115n, 0x0n])]);

// ./test/core/simd/simd_lane.wast:253
assert_return(() => invoke($0, `i64x2_replace_lane-first`, [i64x2([0x0n, 0x0n]), 1311768467294899695n]), [i64x2([0x1234567890abcdefn, 0x0n])]);

// ./test/core/simd/simd_lane.wast:254
assert_return(() => invoke($0, `i64x2_replace_lane-last`, [i64x2([0x0n, 0x0n]), -9223372036854775808n]), [i64x2([0x0n, 0x8000000000000000n])]);

// ./test/core/simd/simd_lane.wast:255
assert_return(() => invoke($0, `i64x2_replace_lane-last`, [i64x2([0x0n, 0x0n]), -9223372036854775808n]), [i64x2([0x0n, 0x8000000000000000n])]);

// ./test/core/simd/simd_lane.wast:256
assert_return(() => invoke($0, `i64x2_replace_lane-last`, [i64x2([0x0n, 0x0n]), 1234567890123456789n]), [i64x2([0x0n, 0x112210f47de98115n])]);

// ./test/core/simd/simd_lane.wast:257
assert_return(() => invoke($0, `i64x2_replace_lane-last`, [i64x2([0x0n, 0x0n]), 1311768467294899695n]), [i64x2([0x0n, 0x1234567890abcdefn])]);

// ./test/core/simd/simd_lane.wast:259
assert_return(() => invoke($0, `f64x2_replace_lane-first`, [f64x2([1, 1]), value('f64', 0)]), [new F64x2Pattern(value('f64', 0), value('f64', 1))]);

// ./test/core/simd/simd_lane.wast:260
assert_return(() => invoke($0, `f64x2_replace_lane-first`, [f64x2([-1, -1]), value('f64', -0)]), [new F64x2Pattern(value('f64', -0), value('f64', -1))]);

// ./test/core/simd/simd_lane.wast:261
assert_return(() => invoke($0, `f64x2_replace_lane-first`, [f64x2([0, 0]), value('f64', 1.25)]), [new F64x2Pattern(value('f64', 1.25), value('f64', 0))]);

// ./test/core/simd/simd_lane.wast:262
assert_return(() => invoke($0, `f64x2_replace_lane-first`, [f64x2([0, 0]), value('f64', -1.25)]), [new F64x2Pattern(value('f64', -1.25), value('f64', 0))]);

// ./test/core/simd/simd_lane.wast:263
assert_return(() => invoke($0, `f64x2_replace_lane-first`, [bytes('v128', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), value('f64', -179769313486231570000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]), [new F64x2Pattern(value('f64', -179769313486231570000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000), value('f64', 0))]);

// ./test/core/simd/simd_lane.wast:264
assert_return(() => invoke($0, `f64x2_replace_lane-first`, [bytes('v128', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), value('f64', 179769313486231570000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]), [new F64x2Pattern(value('f64', 179769313486231570000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000), value('f64', 0))]);

// ./test/core/simd/simd_lane.wast:265
assert_return(() => invoke($0, `f64x2_replace_lane-first`, [f64x2([-Infinity, 0]), value('f64', -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000022250738585072014)]), [new F64x2Pattern(value('f64', -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000022250738585072014), value('f64', 0))]);

// ./test/core/simd/simd_lane.wast:266
assert_return(() => invoke($0, `f64x2_replace_lane-first`, [f64x2([Infinity, 0]), value('f64', 0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000022250738585072014)]), [new F64x2Pattern(value('f64', 0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000022250738585072014), value('f64', 0))]);

// ./test/core/simd/simd_lane.wast:267
assert_return(() => invoke($0, `f64x2_replace_lane-first`, [f64x2([0, 0]), bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff])]), [new F64x2Pattern(bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff]), value('f64', 0))]);

// ./test/core/simd/simd_lane.wast:268
assert_return(() => invoke($0, `f64x2_replace_lane-first`, [f64x2([0, 0]), bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])]), [new F64x2Pattern(bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]), value('f64', 0))]);

// ./test/core/simd/simd_lane.wast:269
assert_return(() => invoke($0, `f64x2_replace_lane-first`, [f64x2([0, 0]), value('f64', -Infinity)]), [new F64x2Pattern(value('f64', -Infinity), value('f64', 0))]);

// ./test/core/simd/simd_lane.wast:270
assert_return(() => invoke($0, `f64x2_replace_lane-first`, [f64x2([0, 0]), value('f64', Infinity)]), [new F64x2Pattern(value('f64', Infinity), value('f64', 0))]);

// ./test/core/simd/simd_lane.wast:271
assert_return(() => invoke($0, `f64x2_replace_lane-first`, [f64x2([0, 0]), value('f64', 123456789)]), [new F64x2Pattern(value('f64', 123456789), value('f64', 0))]);

// ./test/core/simd/simd_lane.wast:272
assert_return(() => invoke($0, `f64x2_replace_lane-first`, [f64x2([0, 0]), value('f64', 123456789)]), [new F64x2Pattern(value('f64', 123456789), value('f64', 0))]);

// ./test/core/simd/simd_lane.wast:273
assert_return(() => invoke($0, `f64x2_replace_lane-first`, [f64x2([0, 0]), value('f64', 1375488932539311400000000)]), [new F64x2Pattern(value('f64', 1375488932539311400000000), value('f64', 0))]);

// ./test/core/simd/simd_lane.wast:274
assert_return(() => invoke($0, `f64x2_replace_lane-first`, [f64x2([0, 0]), value('f64', 1375488932539311400000000)]), [new F64x2Pattern(value('f64', 1375488932539311400000000), value('f64', 0))]);

// ./test/core/simd/simd_lane.wast:275
assert_return(() => invoke($0, `f64x2_replace_lane-last`, [f64x2([2, 2]), value('f64', 0)]), [new F64x2Pattern(value('f64', 2), value('f64', 0))]);

// ./test/core/simd/simd_lane.wast:276
assert_return(() => invoke($0, `f64x2_replace_lane-last`, [f64x2([-2, -2]), value('f64', -0)]), [new F64x2Pattern(value('f64', -2), value('f64', -0))]);

// ./test/core/simd/simd_lane.wast:277
assert_return(() => invoke($0, `f64x2_replace_lane-last`, [f64x2([0, 0]), value('f64', 2.25)]), [new F64x2Pattern(value('f64', 0), value('f64', 2.25))]);

// ./test/core/simd/simd_lane.wast:278
assert_return(() => invoke($0, `f64x2_replace_lane-last`, [f64x2([0, 0]), value('f64', -2.25)]), [new F64x2Pattern(value('f64', 0), value('f64', -2.25))]);

// ./test/core/simd/simd_lane.wast:279
assert_return(() => invoke($0, `f64x2_replace_lane-last`, [bytes('v128', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff]), value('f64', -179769313486231570000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]), [new F64x2Pattern(value('f64', 0), value('f64', -179769313486231570000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000))]);

// ./test/core/simd/simd_lane.wast:280
assert_return(() => invoke($0, `f64x2_replace_lane-last`, [bytes('v128', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]), value('f64', 179769313486231570000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]), [new F64x2Pattern(value('f64', 0), value('f64', 179769313486231570000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000))]);

// ./test/core/simd/simd_lane.wast:281
assert_return(() => invoke($0, `f64x2_replace_lane-last`, [f64x2([0, -Infinity]), value('f64', -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000022250738585072014)]), [new F64x2Pattern(value('f64', 0), value('f64', -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000022250738585072014))]);

// ./test/core/simd/simd_lane.wast:282
assert_return(() => invoke($0, `f64x2_replace_lane-last`, [f64x2([0, Infinity]), value('f64', 0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000022250738585072014)]), [new F64x2Pattern(value('f64', 0), value('f64', 0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000022250738585072014))]);

// ./test/core/simd/simd_lane.wast:283
assert_return(() => invoke($0, `f64x2_replace_lane-last`, [f64x2([0, 0]), bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff])]), [new F64x2Pattern(value('f64', 0), bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff]))]);

// ./test/core/simd/simd_lane.wast:284
assert_return(() => invoke($0, `f64x2_replace_lane-last`, [f64x2([0, 0]), bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])]), [new F64x2Pattern(value('f64', 0), bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]))]);

// ./test/core/simd/simd_lane.wast:285
assert_return(() => invoke($0, `f64x2_replace_lane-last`, [f64x2([0, 0]), value('f64', -Infinity)]), [new F64x2Pattern(value('f64', 0), value('f64', -Infinity))]);

// ./test/core/simd/simd_lane.wast:286
assert_return(() => invoke($0, `f64x2_replace_lane-last`, [f64x2([0, 0]), value('f64', Infinity)]), [new F64x2Pattern(value('f64', 0), value('f64', Infinity))]);

// ./test/core/simd/simd_lane.wast:287
assert_return(() => invoke($0, `f64x2_replace_lane-last`, [f64x2([0, 0]), value('f64', 1234567890000000000000000000)]), [new F64x2Pattern(value('f64', 0), value('f64', 1234567890000000000000000000))]);

// ./test/core/simd/simd_lane.wast:288
assert_return(() => invoke($0, `f64x2_replace_lane-last`, [f64x2([0, 0]), value('f64', 1234567890000000000000000000)]), [new F64x2Pattern(value('f64', 0), value('f64', 1234567890000000000000000000))]);

// ./test/core/simd/simd_lane.wast:289
assert_return(() => invoke($0, `f64x2_replace_lane-last`, [f64x2([0, 0]), value('f64', 1234567890000000000000000000)]), [new F64x2Pattern(value('f64', 0), value('f64', 1234567890000000000000000000))]);

// ./test/core/simd/simd_lane.wast:290
assert_return(() => invoke($0, `f64x2_replace_lane-last`, [f64x2([0, 0]), value('f64', 0.0000000000123456789)]), [new F64x2Pattern(value('f64', 0), value('f64', 0.0000000000123456789))]);

// ./test/core/simd/simd_lane.wast:292
assert_return(() => invoke($0, `v8x16_swizzle`, [i8x16([0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f]), i8x16([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf])]), [i8x16([0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f])]);

// ./test/core/simd/simd_lane.wast:296
assert_return(() => invoke($0, `v8x16_swizzle`, [i8x16([0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff]), i8x16([0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17])]), [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_lane.wast:300
assert_return(() => invoke($0, `v8x16_swizzle`, [i8x16([0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73]), i8x16([0xf, 0xe, 0xd, 0xc, 0xb, 0xa, 0x9, 0x8, 0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1, 0x0])]), [i8x16([0x73, 0x72, 0x71, 0x70, 0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x69, 0x68, 0x67, 0x66, 0x65, 0x64])]);

// ./test/core/simd/simd_lane.wast:304
assert_return(() => invoke($0, `v8x16_swizzle`, [i8x16([0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73]), i8x16([0xff, 0x1, 0xfe, 0x2, 0xfd, 0x3, 0xfc, 0x4, 0xfb, 0x5, 0xfa, 0x6, 0xf9, 0x7, 0xf8, 0x8])]), [i8x16([0x0, 0x65, 0x0, 0x66, 0x0, 0x67, 0x0, 0x68, 0x0, 0x69, 0x0, 0x6a, 0x0, 0x6b, 0x0, 0x6c])]);

// ./test/core/simd/simd_lane.wast:308
assert_return(() => invoke($0, `v8x16_swizzle`, [i8x16([0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73]), i8x16([0x9, 0x10, 0xa, 0x11, 0xb, 0x12, 0xc, 0x13, 0xd, 0x14, 0xe, 0x15, 0xf, 0x16, 0x10, 0x17])]), [i8x16([0x6d, 0x0, 0x6e, 0x0, 0x6f, 0x0, 0x70, 0x0, 0x71, 0x0, 0x72, 0x0, 0x73, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_lane.wast:312
assert_return(() => invoke($0, `v8x16_swizzle`, [i8x16([0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73]), i8x16([0x9, 0x10, 0xa, 0x11, 0xb, 0x12, 0xc, 0x13, 0xd, 0x14, 0xe, 0x15, 0xf, 0x16, 0x10, 0x17])]), [i8x16([0x6d, 0x0, 0x6e, 0x0, 0x6f, 0x0, 0x70, 0x0, 0x71, 0x0, 0x72, 0x0, 0x73, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_lane.wast:316
assert_return(() => invoke($0, `v8x16_swizzle`, [i16x8([0x6465, 0x6667, 0x6869, 0x6a6b, 0x6c6d, 0x6e6f, 0x7071, 0x7273]), i8x16([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf])]), [i16x8([0x6465, 0x6667, 0x6869, 0x6a6b, 0x6c6d, 0x6e6f, 0x7071, 0x7273])]);

// ./test/core/simd/simd_lane.wast:320
assert_return(() => invoke($0, `v8x16_swizzle`, [i32x4([0x64656667, 0x68696a6b, 0x6c6d6e6f, 0x70717273]), i8x16([0xf, 0xe, 0xd, 0xc, 0xb, 0xa, 0x9, 0x8, 0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1, 0x0])]), [i32x4([0x73727170, 0x6f6e6d6c, 0x6b6a6968, 0x67666564])]);

// ./test/core/simd/simd_lane.wast:324
assert_return(() => invoke($0, `v8x16_swizzle`, [bytes('v128', [0x0, 0x0, 0xc0, 0x7f, 0x0, 0x0, 0xc0, 0xff, 0x0, 0x0, 0x80, 0x7f, 0x0, 0x0, 0x80, 0xff]), i8x16([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf])]), [i32x4([0x7fc00000, 0xffc00000, 0x7f800000, 0xff800000])]);

// ./test/core/simd/simd_lane.wast:328
assert_return(() => invoke($0, `v8x16_swizzle`, [i32x4([0x67666564, 0x6b6a6968, 0x6f6e6d5c, 0x73727170]), bytes('v128', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x80, 0x0, 0x0, 0x80, 0x7f, 0x0, 0x0, 0x80, 0xff])]), [i32x4([0x64646464, 0x646464, 0x6464, 0x6464])]);

// ./test/core/simd/simd_lane.wast:333
assert_return(() => invoke($0, `v8x16_shuffle-1`, [i8x16([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf]), i8x16([0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f])]), [i8x16([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf])]);

// ./test/core/simd/simd_lane.wast:337
assert_return(() => invoke($0, `v8x16_shuffle-2`, [i8x16([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf]), i8x16([0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff])]), [i8x16([0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff])]);

// ./test/core/simd/simd_lane.wast:341
assert_return(() => invoke($0, `v8x16_shuffle-3`, [i8x16([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf]), i8x16([0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff])]), [i8x16([0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8, 0xf7, 0xf6, 0xf5, 0xf4, 0xf3, 0xf2, 0xf1, 0xf0])]);

// ./test/core/simd/simd_lane.wast:345
assert_return(() => invoke($0, `v8x16_shuffle-4`, [i8x16([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf]), i8x16([0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff])]), [i8x16([0xf, 0xe, 0xd, 0xc, 0xb, 0xa, 0x9, 0x8, 0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1, 0x0])]);

// ./test/core/simd/simd_lane.wast:349
assert_return(() => invoke($0, `v8x16_shuffle-5`, [i8x16([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf]), i8x16([0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff])]), [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_lane.wast:353
assert_return(() => invoke($0, `v8x16_shuffle-6`, [i8x16([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf]), i8x16([0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff])]), [i8x16([0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0])]);

// ./test/core/simd/simd_lane.wast:357
assert_return(() => invoke($0, `v8x16_shuffle-7`, [i8x16([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf]), i8x16([0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff])]), [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0])]);

// ./test/core/simd/simd_lane.wast:361
assert_return(() => invoke($0, `v8x16_shuffle-1`, [i8x16([0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73]), i8x16([0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff])]), [i8x16([0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73])]);

// ./test/core/simd/simd_lane.wast:365
assert_return(() => invoke($0, `v8x16_shuffle-1`, [i16x8([0x100, 0x302, 0x504, 0x706, 0x908, 0xb0a, 0xd0c, 0xf0e]), i8x16([0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff])]), [i16x8([0x100, 0x302, 0x504, 0x706, 0x908, 0xb0a, 0xd0c, 0xf0e])]);

// ./test/core/simd/simd_lane.wast:369
assert_return(() => invoke($0, `v8x16_shuffle-2`, [i8x16([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf]), i32x4([0xf3f2f1f0, 0xf7f6f5f4, 0xfbfaf9f8, 0xfffefdfc])]), [i32x4([0xf3f2f1f0, 0xf7f6f5f4, 0xfbfaf9f8, 0xfffefdfc])]);

// ./test/core/simd/simd_lane.wast:373
assert_return(() => invoke($0, `v8x16_shuffle-1`, [i32x4([0x10203, 0x4050607, 0x8090a0b, 0xc0d0e0f]), i8x16([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf])]), [i32x4([0x10203, 0x4050607, 0x8090a0b, 0xc0d0e0f])]);

// ./test/core/simd/simd_lane.wast:377
assert_return(() => invoke($0, `v8x16_shuffle-1`, [bytes('v128', [0x0, 0x0, 0x80, 0x3f, 0x0, 0x0, 0xc0, 0x7f, 0x0, 0x0, 0x80, 0x7f, 0x0, 0x0, 0x80, 0xff]), i8x16([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf])]), [i32x4([0x3f800000, 0x7fc00000, 0x7f800000, 0xff800000])]);

// ./test/core/simd/simd_lane.wast:381
assert_return(() => invoke($0, `v8x16_shuffle-1`, [i32x4([0x10203, 0x4050607, 0x8090a0b, 0xc0d0e0f]), bytes('v128', [0x0, 0x0, 0x0, 0x80, 0x0, 0x0, 0xc0, 0x7f, 0x0, 0x0, 0x80, 0x7f, 0x0, 0x0, 0x80, 0xff])]), [i32x4([0x10203, 0x4050607, 0x8090a0b, 0xc0d0e0f])]);

// ./test/core/simd/simd_lane.wast:387
assert_return(() => invoke($0, `v8x16_swizzle`, [i32x4([0x499602d2, 0x12345678, 0x499602d2, 0x12345678]), i8x16([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf])]), [i32x4([0x499602d2, 0x12345678, 0x499602d2, 0x12345678])]);

// ./test/core/simd/simd_lane.wast:391
assert_return(() => invoke($0, `v8x16_shuffle-1`, [i64x2([0xab54a98ceb1f0ad2n, 0x1234567890abcdefn]), i64x2([0xab54a98ceb1f0ad2n, 0x1234567890abcdefn])]), [i32x4([0xeb1f0ad2, 0xab54a98c, 0x90abcdef, 0x12345678])]);

// ./test/core/simd/simd_lane.wast:398
assert_malformed(() => instantiate(`(func (result i32) (i8x16.extract_lane_s  -1 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:399
assert_malformed(() => instantiate(`(func (result i32) (i8x16.extract_lane_u  -1 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:400
assert_malformed(() => instantiate(`(func (result i32) (i16x8.extract_lane_s  -1 (v128.const i16x8 0 0 0 0 0 0 0 0))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:401
assert_malformed(() => instantiate(`(func (result i32) (i16x8.extract_lane_u  -1 (v128.const i16x8 0 0 0 0 0 0 0 0))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:402
assert_malformed(() => instantiate(`(func (result i32) (i32x4.extract_lane  -1 (v128.const i32x4 0 0 0 0))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:403
assert_malformed(() => instantiate(`(func (result f32) (f32x4.extract_lane  -1 (v128.const f32x4 0 0 0 0))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:404
assert_malformed(() => instantiate(`(func (result v128) (i8x16.replace_lane  -1 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (i32.const 1))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:405
assert_malformed(() => instantiate(`(func (result v128) (i16x8.replace_lane  -1 (v128.const i16x8 0 0 0 0 0 0 0 0) (i32.const 1))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:406
assert_malformed(() => instantiate(`(func (result v128) (i32x4.replace_lane  -1 (v128.const i32x4 0 0 0 0) (i32.const 1))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:407
assert_malformed(() => instantiate(`(func (result v128) (f32x4.replace_lane  -1 (v128.const f32x4 0 0 0 0) (i32.const 1))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:408
assert_malformed(() => instantiate(`(func (result i64) (i64x2.extract_lane  -1 (v128.const i64x2 0 0))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:409
assert_malformed(() => instantiate(`(func (result f64) (f64x2.extract_lane  -1 (v128.const f64x2 0 0))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:410
assert_malformed(() => instantiate(`(func (result v128) (i64x2.replace_lane  -1 (v128.const i64x2 0 0) (i64.const 1))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:411
assert_malformed(() => instantiate(`(func (result v128) (f64x2.replace_lane  -1 (v128.const f64x2 0 0) (f64.const 1))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:415
assert_malformed(() => instantiate(`(func (result i32) (i8x16.extract_lane_s 256 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))) `), `malformed lane index`);

// ./test/core/simd/simd_lane.wast:416
assert_malformed(() => instantiate(`(func (result i32) (i8x16.extract_lane_u 256 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))) `), `malformed lane index`);

// ./test/core/simd/simd_lane.wast:417
assert_malformed(() => instantiate(`(func (result i32) (i16x8.extract_lane_s 256 (v128.const i16x8 0 0 0 0 0 0 0 0))) `), `malformed lane index`);

// ./test/core/simd/simd_lane.wast:418
assert_malformed(() => instantiate(`(func (result i32) (i16x8.extract_lane_u 256 (v128.const i16x8 0 0 0 0 0 0 0 0))) `), `malformed lane index`);

// ./test/core/simd/simd_lane.wast:419
assert_malformed(() => instantiate(`(func (result i32) (i32x4.extract_lane 256 (v128.const i32x4 0 0 0 0))) `), `malformed lane index`);

// ./test/core/simd/simd_lane.wast:420
assert_malformed(() => instantiate(`(func (result f32) (f32x4.extract_lane 256 (v128.const f32x4 0 0 0 0))) `), `malformed lane index`);

// ./test/core/simd/simd_lane.wast:421
assert_malformed(() => instantiate(`(func (result v128) (i8x16.replace_lane 256 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (i32.const 1))) `), `malformed lane index`);

// ./test/core/simd/simd_lane.wast:422
assert_malformed(() => instantiate(`(func (result v128) (i16x8.replace_lane 256 (v128.const i16x8 0 0 0 0 0 0 0 0) (i32.const 1))) `), `malformed lane index`);

// ./test/core/simd/simd_lane.wast:423
assert_malformed(() => instantiate(`(func (result v128) (i32x4.replace_lane 256 (v128.const i32x4 0 0 0 0) (i32.const 1))) `), `malformed lane index`);

// ./test/core/simd/simd_lane.wast:424
assert_malformed(() => instantiate(`(func (result v128) (f32x4.replace_lane 256 (v128.const f32x4 0 0 0 0) (i32.const 1))) `), `malformed lane index`);

// ./test/core/simd/simd_lane.wast:425
assert_malformed(() => instantiate(`(func (result i64) (i64x2.extract_lane 256 (v128.const i64x2 0 0))) `), `malformed lane index`);

// ./test/core/simd/simd_lane.wast:426
assert_malformed(() => instantiate(`(func (result f64) (f64x2.extract_lane 256 (v128.const f64x2 0 0))) `), `malformed lane index`);

// ./test/core/simd/simd_lane.wast:427
assert_malformed(() => instantiate(`(func (result v128) (i64x2.replace_lane 256 (v128.const i64x2 0 0) (i64.const 1))) `), `malformed lane index`);

// ./test/core/simd/simd_lane.wast:428
assert_malformed(() => instantiate(`(func (result v128) (f64x2.replace_lane 256 (v128.const f64x2 0 0) (f64.const 1))) `), `malformed lane index`);

// ./test/core/simd/simd_lane.wast:432
assert_invalid(() => instantiate(`(module (func (result i32) (i8x16.extract_lane_s 16 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:433
assert_invalid(() => instantiate(`(module (func (result i32) (i8x16.extract_lane_s 255 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:434
assert_invalid(() => instantiate(`(module (func (result i32) (i8x16.extract_lane_u 16 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:435
assert_invalid(() => instantiate(`(module (func (result i32) (i8x16.extract_lane_u 255 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:436
assert_invalid(() => instantiate(`(module (func (result i32) (i16x8.extract_lane_s 8 (v128.const i16x8 0 0 0 0 0 0 0 0))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:437
assert_invalid(() => instantiate(`(module (func (result i32) (i16x8.extract_lane_s 255 (v128.const i16x8 0 0 0 0 0 0 0 0))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:438
assert_invalid(() => instantiate(`(module (func (result i32) (i16x8.extract_lane_u 8 (v128.const i16x8 0 0 0 0 0 0 0 0))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:439
assert_invalid(() => instantiate(`(module (func (result i32) (i16x8.extract_lane_u 255 (v128.const i16x8 0 0 0 0 0 0 0 0))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:440
assert_invalid(() => instantiate(`(module (func (result i32) (i32x4.extract_lane 4 (v128.const i32x4 0 0 0 0))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:441
assert_invalid(() => instantiate(`(module (func (result i32) (i32x4.extract_lane 255 (v128.const i32x4 0 0 0 0))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:442
assert_invalid(() => instantiate(`(module (func (result f32) (f32x4.extract_lane 4 (v128.const f32x4 0 0 0 0))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:443
assert_invalid(() => instantiate(`(module (func (result f32) (f32x4.extract_lane 255 (v128.const f32x4 0 0 0 0))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:444
assert_invalid(() => instantiate(`(module (func (result v128) (i8x16.replace_lane 16 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (i32.const 1))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:445
assert_invalid(() => instantiate(`(module (func (result v128) (i8x16.replace_lane 255 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (i32.const 1))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:446
assert_invalid(() => instantiate(`(module (func (result v128) (i16x8.replace_lane 16 (v128.const i16x8 0 0 0 0 0 0 0 0) (i32.const 1))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:447
assert_invalid(() => instantiate(`(module (func (result v128) (i16x8.replace_lane 255 (v128.const i16x8 0 0 0 0 0 0 0 0) (i32.const 1))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:448
assert_invalid(() => instantiate(`(module (func (result v128) (i32x4.replace_lane 4 (v128.const i32x4 0 0 0 0) (i32.const 1))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:449
assert_invalid(() => instantiate(`(module (func (result v128) (i32x4.replace_lane 255 (v128.const i32x4 0 0 0 0) (i32.const 1))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:450
assert_invalid(() => instantiate(`(module (func (result v128) (f32x4.replace_lane 4 (v128.const f32x4 0 0 0 0) (i32.const 1))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:451
assert_invalid(() => instantiate(`(module (func (result v128) (f32x4.replace_lane 255 (v128.const f32x4 0 0 0 0) (i32.const 1))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:452
assert_invalid(() => instantiate(`(module (func (result i64) (i64x2.extract_lane 2 (v128.const i64x2 0 0))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:453
assert_invalid(() => instantiate(`(module (func (result i64) (i64x2.extract_lane 255 (v128.const i64x2 0 0))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:454
assert_invalid(() => instantiate(`(module (func (result f64) (f64x2.extract_lane 2 (v128.const f64x2 0 0))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:455
assert_invalid(() => instantiate(`(module (func (result f64) (f64x2.extract_lane 255 (v128.const f64x2 0 0))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:456
assert_invalid(() => instantiate(`(module (func (result v128) (i64x2.replace_lane 2 (v128.const i64x2 0 0) (i64.const 1))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:457
assert_invalid(() => instantiate(`(module (func (result v128) (i64x2.replace_lane 255 (v128.const i64x2 0 0) (i64.const 1))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:458
assert_invalid(() => instantiate(`(module (func (result v128) (f64x2.replace_lane 2 (v128.const f64x2 0 0) (f64.const 1))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:459
assert_invalid(() => instantiate(`(module (func (result v128) (f64x2.replace_lane 255 (v128.const f64x2 0 0) (f64.const 1.0))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:463
assert_invalid(() => instantiate(`(module (func (result i32) (i16x8.extract_lane_s 8 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:464
assert_invalid(() => instantiate(`(module (func (result i32) (i16x8.extract_lane_u 8 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:465
assert_invalid(() => instantiate(`(module (func (result i32) (i32x4.extract_lane 4 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:466
assert_invalid(() => instantiate(`(module (func (result i32) (f32x4.extract_lane 4 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:467
assert_invalid(() => instantiate(`(module (func (result v128) (i16x8.replace_lane 8 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (i32.const 1))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:468
assert_invalid(() => instantiate(`(module (func (result v128) (i32x4.replace_lane 4 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (i32.const 1))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:469
assert_invalid(() => instantiate(`(module (func (result v128) (f32x4.replace_lane 4 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (i32.const 1))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:470
assert_invalid(() => instantiate(`(module (func (result i64) (i64x2.extract_lane 2 (v128.const i64x2 0 0))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:471
assert_invalid(() => instantiate(`(module (func (result f64) (f64x2.extract_lane 2 (v128.const f64x2 0 0))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:472
assert_invalid(() => instantiate(`(module (func (result v128) (i64x2.replace_lane 2 (v128.const i64x2 0 0) (i64.const 1))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:473
assert_invalid(() => instantiate(`(module (func (result v128) (f64x2.replace_lane 2 (v128.const f64x2 0 0) (f64.const 1.0))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:477
assert_invalid(() => instantiate(`(module (func (result i32) (i8x16.extract_lane_s 0 (i32.const 0))))`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:478
assert_invalid(() => instantiate(`(module (func (result i32) (i8x16.extract_lane_u 0 (i64.const 0))))`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:479
assert_invalid(() => instantiate(`(module (func (result i32) (i8x16.extract_lane_s 0 (f32.const 0.0))))`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:480
assert_invalid(() => instantiate(`(module (func (result i32) (i8x16.extract_lane_u 0 (f64.const 0.0))))`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:481
assert_invalid(() => instantiate(`(module (func (result i32) (i32x4.extract_lane 0 (i32.const 0))))`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:482
assert_invalid(() => instantiate(`(module (func (result f32) (f32x4.extract_lane 0 (f32.const 0.0))))`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:483
assert_invalid(() => instantiate(`(module (func (result v128) (i8x16.replace_lane 0 (i32.const 0) (i32.const 1))))`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:484
assert_invalid(() => instantiate(`(module (func (result v128) (i16x8.replace_lane 0 (i64.const 0) (i32.const 1))))`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:485
assert_invalid(() => instantiate(`(module (func (result v128) (i32x4.replace_lane 0 (i32.const 0) (i32.const 1))))`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:486
assert_invalid(() => instantiate(`(module (func (result v128) (f32x4.replace_lane 0 (f32.const 0.0) (i32.const 1))))`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:487
assert_invalid(() => instantiate(`(module (func (result i64) (i64x2.extract_lane 0 (i64.const 0))))`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:488
assert_invalid(() => instantiate(`(module (func (result f64) (f64x2.extract_lane 0 (f64.const 0.0))))`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:489
assert_invalid(() => instantiate(`(module (func (result v128) (i32x4.replace_lane 0 (i32.const 0) (i32.const 1))))`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:490
assert_invalid(() => instantiate(`(module (func (result v128) (f32x4.replace_lane 0 (f32.const 0.0) (i32.const 1))))`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:494
assert_invalid(() => instantiate(`(module (func (result v128) (i8x16.replace_lane 0 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (f32.const 1.0))))`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:495
assert_invalid(() => instantiate(`(module (func (result v128) (i16x8.replace_lane 0 (v128.const i16x8 0 0 0 0 0 0 0 0) (f64.const 1.0))))`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:496
assert_invalid(() => instantiate(`(module (func (result v128) (i32x4.replace_lane 0 (v128.const i32x4 0 0 0 0) (f32.const 1.0))))`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:497
assert_invalid(() => instantiate(`(module (func (result v128) (f32x4.replace_lane 0 (v128.const f32x4 0 0 0 0) (i32.const 1))))`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:499
assert_invalid(() => instantiate(`(module (func (result v128) (i64x2.replace_lane 0 (v128.const i64x2 0 0) (f64.const 1.0))))`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:500
assert_invalid(() => instantiate(`(module (func (result v128) (f64x2.replace_lane 0 (v128.const f64x2 0 0) (i64.const 1))))`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:503
assert_invalid(() => instantiate(`(module (func (result v128)
  (i8x16.swizzle (i32.const 1) (v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0))))`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:505
assert_invalid(() => instantiate(`(module (func (result v128)
  (i8x16.swizzle (v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0) (i32.const 2))))`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:507
assert_invalid(() => instantiate(`(module (func (result v128)
  (i8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 (f32.const 3.0)
  (v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0))))`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:510
assert_invalid(() => instantiate(`(module (func (result v128)
  (i8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
  (v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0) (f32.const 4.0))))`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:515
assert_malformed(() => instantiate(`(func (param v128) (result v128) (i8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 (local.get 0) (local.get 0))) `), `invalid lane length`);

// ./test/core/simd/simd_lane.wast:518
assert_malformed(() => instantiate(`(func (param v128) (result v128) (i8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 (local.get 0) (local.get 0))) `), `invalid lane length`);

// ./test/core/simd/simd_lane.wast:521
assert_malformed(() => instantiate(`(func (result v128) (i8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 -1 (v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0) (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))) `), `malformed lane index`);

// ./test/core/simd/simd_lane.wast:525
assert_malformed(() => instantiate(`(func (result v128) (i8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 256 (v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0) (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))) `), `malformed lane index`);

// ./test/core/simd/simd_lane.wast:529
assert_invalid(() => instantiate(`(module (func (result v128)
  (i8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 255
  (v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0)
  (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))))`), `invalid lane index`);

// ./test/core/simd/simd_lane.wast:536
assert_malformed(() => instantiate(`(func (result i32) (i8x16.extract_lane 0 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))) `), `unknown operator`);

// ./test/core/simd/simd_lane.wast:537
assert_malformed(() => instantiate(`(func (result i32) (i16x8.extract_lane 0 (v128.const i16x8 0 0 0 0 0 0 0 0))) `), `unknown operator`);

// ./test/core/simd/simd_lane.wast:538
assert_malformed(() => instantiate(`(func (result i32) (i32x4.extract_lane_s 0 (v128.const i32x4 0 0 0 0))) `), `unknown operator`);

// ./test/core/simd/simd_lane.wast:539
assert_malformed(() => instantiate(`(func (result i32) (i32x4.extract_lane_u 0 (v128.const i32x4 0 0 0 0))) `), `unknown operator`);

// ./test/core/simd/simd_lane.wast:540
assert_malformed(() => instantiate(`(func (result i32) (i64x2.extract_lane_s 0 (v128.const i64x2 0 0))) `), `unknown operator`);

// ./test/core/simd/simd_lane.wast:541
assert_malformed(() => instantiate(`(func (result i32) (i64x2.extract_lane_u 0 (v128.const i64x2 0 0))) `), `unknown operator`);

// ./test/core/simd/simd_lane.wast:545
assert_malformed(() => instantiate(`(func (result v128)  (i8x16.shuffle1 (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15)  (v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0))) `), `unknown operator`);

// ./test/core/simd/simd_lane.wast:549
assert_malformed(() => instantiate(`(func (result v128)  (i8x16.shuffle2_imm  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15  (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15)  (v128.const i8x16 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31))) `), `unknown operator`);

// ./test/core/simd/simd_lane.wast:555
assert_malformed(() => instantiate(`(func (result v128)  (v8x16.swizzle (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15)  (v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0))) `), `unknown operator`);

// ./test/core/simd/simd_lane.wast:559
assert_malformed(() => instantiate(`(func (result v128)  (v8x16.shuffle  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15  (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15)  (v128.const i8x16 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31))) `), `unknown operator`);

// ./test/core/simd/simd_lane.wast:570
assert_malformed(() => instantiate(`(func (param i32) (result i32) (i8x16.extract_lane_s (local.get 0) (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:571
assert_malformed(() => instantiate(`(func (param i32) (result i32) (i8x16.extract_lane_u (local.get 0) (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:572
assert_malformed(() => instantiate(`(func (param i32) (result i32) (i16x8.extract_lane_s (local.get 0) (v128.const i16x8 0 0 0 0 0 0 0 0))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:573
assert_malformed(() => instantiate(`(func (param i32) (result i32) (i16x8.extract_lane_u (local.get 0) (v128.const i16x8 0 0 0 0 0 0 0 0))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:574
assert_malformed(() => instantiate(`(func (param i32) (result i32) (i32x4.extract_lane (local.get 0) (v128.const i32x4 0 0 0 0))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:575
assert_malformed(() => instantiate(`(func (param i32) (result f32) (f32x4.extract_lane (local.get 0) (v128.const f32x4 0 0 0 0))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:576
assert_malformed(() => instantiate(`(func (param i32) (result v128) (i8x16.replace_lane (local.get 0) (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (i32.const 1))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:577
assert_malformed(() => instantiate(`(func (param i32) (result v128) (i16x8.replace_lane (local.get 0) (v128.const i16x8 0 0 0 0 0 0 0 0) (i32.const 1))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:578
assert_malformed(() => instantiate(`(func (param i32) (result v128) (i32x4.replace_lane (local.get 0) (v128.const i32x4 0 0 0 0) (i32.const 1))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:579
assert_malformed(() => instantiate(`(func (param i32) (result v128) (f32x4.replace_lane (local.get 0) (v128.const f32x4 0 0 0 0) (f32.const 1.0))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:581
assert_malformed(() => instantiate(`(func (param i32) (result i64) (i64x2.extract_lane (local.get 0) (v128.const i64x2 0 0))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:582
assert_malformed(() => instantiate(`(func (param i32) (result f64) (f64x2.extract_lane (local.get 0) (v128.const f64x2 0 0))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:583
assert_malformed(() => instantiate(`(func (param i32) (result v128) (i64x2.replace_lane (local.get 0) (v128.const i64x2 0 0) (i64.const 1))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:584
assert_malformed(() => instantiate(`(func (param i32) (result v128) (f64x2.replace_lane (local.get 0) (v128.const f64x2 0 0) (f64.const 1.0))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:588
assert_malformed(() => instantiate(`(func (result i32) (i8x16.extract_lane_s 1.5 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:589
assert_malformed(() => instantiate(`(func (result i32) (i8x16.extract_lane_u nan (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:590
assert_malformed(() => instantiate(`(func (result i32) (i16x8.extract_lane_s inf (v128.const i16x8 0 0 0 0 0 0 0 0))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:591
assert_malformed(() => instantiate(`(func (result i32) (i16x8.extract_lane_u -inf (v128.const i16x8 0 0 0 0 0 0 0 0))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:592
assert_malformed(() => instantiate(`(func (result i32) (i32x4.extract_lane nan (v128.const i32x4 0 0 0 0))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:593
assert_malformed(() => instantiate(`(func (result f32) (f32x4.extract_lane nan (v128.const f32x4 0 0 0 0))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:594
assert_malformed(() => instantiate(`(func (result v128) (i8x16.replace_lane -2.5 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (i32.const 1))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:595
assert_malformed(() => instantiate(`(func (result v128) (i16x8.replace_lane nan (v128.const i16x8 0 0 0 0 0 0 0 0) (i32.const 1))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:596
assert_malformed(() => instantiate(`(func (result v128) (i32x4.replace_lane inf (v128.const i32x4 0 0 0 0) (i32.const 1))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:597
assert_malformed(() => instantiate(`(func (result v128) (f32x4.replace_lane -inf (v128.const f32x4 0 0 0 0) (f32.const 1.1))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:600
assert_malformed(() => instantiate(`(func (result v128)  (i8x16.shuffle (v128.const i8x16 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31)  (v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0)  (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))) `), `invalid lane length`);

// ./test/core/simd/simd_lane.wast:604
assert_malformed(() => instantiate(`(func (result v128)  (i8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15.0)  (v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0)  (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))) `), `malformed lane index`);

// ./test/core/simd/simd_lane.wast:608
assert_malformed(() => instantiate(`(func (result v128)  (i8x16.shuffle 0.5 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15)  (v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0)  (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))) `), `malformed lane index`);

// ./test/core/simd/simd_lane.wast:612
assert_malformed(() => instantiate(`(func (result v128)  (i8x16.shuffle -inf 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15)  (v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0)  (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))) `), `malformed lane index`);

// ./test/core/simd/simd_lane.wast:616
assert_malformed(() => instantiate(`(func (result v128)  (i8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 inf)  (v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0)  (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))) `), `malformed lane index`);

// ./test/core/simd/simd_lane.wast:620
assert_malformed(() => instantiate(`(func (result v128)  (i8x16.shuffle nan 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15)  (v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0)  (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))) `), `malformed lane index`);

// ./test/core/simd/simd_lane.wast:628
let $1 = instantiate(`(module
  ;; as *.replace_lane's operand
  (func (export "i8x16_extract_lane_s") (param v128 v128) (result v128)
    (i8x16.replace_lane 0 (local.get 0) (i8x16.extract_lane_s 0 (local.get 1))))
  (func (export "i8x16_extract_lane_u") (param v128 v128) (result v128)
    (i8x16.replace_lane 0 (local.get 0) (i8x16.extract_lane_u 0 (local.get 1))))
  (func (export "i16x8_extract_lane_s") (param v128 v128) (result v128)
    (i16x8.replace_lane 0 (local.get 0) (i16x8.extract_lane_s 0 (local.get 1))))
  (func (export "i16x8_extract_lane_u") (param v128 v128) (result v128)
    (i16x8.replace_lane 0 (local.get 0) (i16x8.extract_lane_u 0 (local.get 1))))
  (func (export "i32x4_extract_lane") (param v128 v128) (result v128)
    (i32x4.replace_lane 0 (local.get 0) (i32x4.extract_lane 0 (local.get 1))))
  (func (export "f32x4_extract_lane") (param v128 v128) (result v128)
    (i32x4.replace_lane 0 (local.get 0) (i32x4.extract_lane 0 (local.get 1))))
  (func (export "i64x2_extract_lane") (param v128 v128) (result v128)
    (i64x2.replace_lane 0 (local.get 0) (i64x2.extract_lane 0 (local.get 1))))
  (func (export "f64x2_extract_lane") (param v128 v128) (result v128)
    (f64x2.replace_lane 0 (local.get 0) (f64x2.extract_lane 0 (local.get 1))))

  ;; as *.extract_lane's operand
  (func (export "i8x16_replace_lane-s") (param v128 i32) (result i32)
    (i8x16.extract_lane_s 15 (i8x16.replace_lane 15 (local.get 0) (local.get 1))))
  (func (export "i8x16_replace_lane-u") (param v128 i32) (result i32)
    (i8x16.extract_lane_u 15 (i8x16.replace_lane 15 (local.get 0) (local.get 1))))
  (func (export "i16x8_replace_lane-s") (param v128 i32) (result i32)
    (i16x8.extract_lane_s 7 (i16x8.replace_lane 7 (local.get 0) (local.get 1))))
  (func (export "i16x8_replace_lane-u") (param v128 i32) (result i32)
    (i16x8.extract_lane_u 7 (i16x8.replace_lane 7 (local.get 0) (local.get 1))))
  (func (export "i32x4_replace_lane") (param v128 i32) (result i32)
    (i32x4.extract_lane 3 (i32x4.replace_lane 3 (local.get 0) (local.get 1))))
  (func (export "f32x4_replace_lane") (param v128 f32) (result f32)
    (f32x4.extract_lane 3 (f32x4.replace_lane 3 (local.get 0) (local.get 1))))
  (func (export "i64x2_replace_lane") (param v128 i64) (result i64)
    (i64x2.extract_lane 1 (i64x2.replace_lane 1 (local.get 0) (local.get 1))))
  (func (export "f64x2_replace_lane") (param v128 f64) (result f64)
    (f64x2.extract_lane 1 (f64x2.replace_lane 1 (local.get 0) (local.get 1))))

  ;; i8x16.replace outputs as shuffle operand
  (func (export "as-v8x16_swizzle-operand") (param v128 i32 v128) (result v128)
    (i8x16.swizzle (i8x16.replace_lane 0 (local.get 0) (local.get 1)) (local.get 2)))
  (func (export "as-v8x16_shuffle-operands") (param v128 i32 v128 i32) (result v128)
    (i8x16.shuffle 16 1 18 3 20 5 22 7 24 9 26 11 28 13 30 15
      (i8x16.replace_lane 0 (local.get 0) (local.get 1))
      (i8x16.replace_lane 15 (local.get 2) (local.get 3))))
)`);

// ./test/core/simd/simd_lane.wast:674
assert_return(() => invoke($1, `i8x16_extract_lane_s`, [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), i8x16([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])]), [i8x16([0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_lane.wast:675
assert_return(() => invoke($1, `i8x16_extract_lane_u`, [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), i8x16([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])]), [i8x16([0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_lane.wast:676
assert_return(() => invoke($1, `i16x8_extract_lane_s`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff])]), [i16x8([0xffff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_lane.wast:677
assert_return(() => invoke($1, `i16x8_extract_lane_u`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff])]), [i16x8([0xffff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_lane.wast:678
assert_return(() => invoke($1, `i32x4_extract_lane`, [i32x4([0x0, 0x0, 0x0, 0x0]), i32x4([0x10000, 0xffffffff, 0xffffffff, 0xffffffff])]), [i32x4([0x10000, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_lane.wast:679
assert_return(() => invoke($1, `f32x4_extract_lane`, [f32x4([0, 0, 0, 0]), bytes('v128', [0x99, 0x76, 0x96, 0x7e, 0x0, 0x0, 0xc0, 0x7f, 0x0, 0x0, 0xc0, 0x7f, 0x0, 0x0, 0xc0, 0x7f])]), [new F32x4Pattern(value('f32', 100000000000000000000000000000000000000), value('f32', 0), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_lane.wast:680
assert_return(() => invoke($1, `i8x16_replace_lane-s`, [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), 255]), [value('i32', -1)]);

// ./test/core/simd/simd_lane.wast:681
assert_return(() => invoke($1, `i8x16_replace_lane-u`, [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), 255]), [value('i32', 255)]);

// ./test/core/simd/simd_lane.wast:682
assert_return(() => invoke($1, `i16x8_replace_lane-s`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), 65535]), [value('i32', -1)]);

// ./test/core/simd/simd_lane.wast:683
assert_return(() => invoke($1, `i16x8_replace_lane-u`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), 65535]), [value('i32', 65535)]);

// ./test/core/simd/simd_lane.wast:684
assert_return(() => invoke($1, `i32x4_replace_lane`, [i32x4([0x0, 0x0, 0x0, 0x0]), -1]), [value('i32', -1)]);

// ./test/core/simd/simd_lane.wast:685
assert_return(() => invoke($1, `f32x4_replace_lane`, [f32x4([0, 0, 0, 0]), value('f32', 1.25)]), [value('f32', 1.25)]);

// ./test/core/simd/simd_lane.wast:687
assert_return(() => invoke($1, `i64x2_extract_lane`, [i64x2([0x0n, 0x0n]), i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])]), [i64x2([0xffffffffffffffffn, 0x0n])]);

// ./test/core/simd/simd_lane.wast:688
assert_return(() => invoke($1, `f64x2_extract_lane`, [f64x2([0, 0]), bytes('v128', [0xa0, 0xc8, 0xeb, 0x85, 0xf3, 0xcc, 0xe1, 0x7f, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])]), [new F64x2Pattern(value('f64', 100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000), value('f64', 0))]);

// ./test/core/simd/simd_lane.wast:689
assert_return(() => invoke($1, `i64x2_replace_lane`, [i64x2([0x0n, 0x0n]), -1n]), [value('i64', -1n)]);

// ./test/core/simd/simd_lane.wast:690
assert_return(() => invoke($1, `f64x2_replace_lane`, [f64x2([0, 0]), value('f64', 2.5)]), [value('f64', 2.5)]);

// ./test/core/simd/simd_lane.wast:692
assert_return(() => invoke($1, `as-v8x16_swizzle-operand`, [i8x16([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf]), 255, i8x16([0xff, 0xf, 0xe, 0xd, 0xc, 0xb, 0xa, 0x9, 0x8, 0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1])]), [i8x16([0x0, 0xf, 0xe, 0xd, 0xc, 0xb, 0xa, 0x9, 0x8, 0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1])]);

// ./test/core/simd/simd_lane.wast:696
assert_return(() => invoke($1, `as-v8x16_shuffle-operands`, [i8x16([0x0, 0xff, 0x0, 0xff, 0xf, 0xff, 0x0, 0xff, 0xff, 0xff, 0x0, 0xff, 0x7f, 0xff, 0x0, 0xff]), 1, i8x16([0x55, 0x0, 0x55, 0x0, 0x55, 0x0, 0x55, 0x0, 0x55, 0x0, 0x55, 0x0, 0x55, 0x1, 0x55, 0xff]), 0]), [i8x16([0x55, 0xff, 0x55, 0xff, 0x55, 0xff, 0x55, 0xff, 0x55, 0xff, 0x55, 0xff, 0x55, 0xff, 0x55, 0xff])]);

// ./test/core/simd/simd_lane.wast:703
let $2 = instantiate(`(module
  ;; Constructing SIMD values
  (func (export "as-i8x16_splat-operand") (param v128) (result v128)
    (i8x16.splat (i8x16.extract_lane_s 0 (local.get 0))))
  (func (export "as-i16x8_splat-operand") (param v128) (result v128)
    (i16x8.splat (i16x8.extract_lane_u 0 (local.get 0))))
  (func (export "as-i32x4_splat-operand") (param v128) (result v128)
    (i32x4.splat (i32x4.extract_lane 0 (local.get 0))))
  (func (export "as-f32x4_splat-operand") (param v128) (result v128)
    (f32x4.splat (f32x4.extract_lane 0 (local.get 0))))
  (func (export "as-i64x2_splat-operand") (param v128) (result v128)
    (i64x2.splat (i64x2.extract_lane 0 (local.get 0))))
  (func (export "as-f64x2_splat-operand") (param v128) (result v128)
    (f64x2.splat (f64x2.extract_lane 0 (local.get 0))))

  ;; Integer arithmetic
  (func (export "as-i8x16_add-operands") (param v128 i32 v128 i32) (result v128)
    (i8x16.add (i8x16.replace_lane 0 (local.get 0) (local.get 1)) (i8x16.replace_lane 15 (local.get 2) (local.get 3))))
  (func (export "as-i16x8_add-operands") (param v128 i32 v128 i32) (result v128)
    (i16x8.add (i16x8.replace_lane 0 (local.get 0) (local.get 1)) (i16x8.replace_lane 7 (local.get 2) (local.get 3))))
  (func (export "as-i32x4_add-operands") (param v128 i32 v128 i32) (result v128)
    (i32x4.add (i32x4.replace_lane 0 (local.get 0) (local.get 1)) (i32x4.replace_lane 3 (local.get 2) (local.get 3))))
  (func (export "as-i64x2_add-operands") (param v128 i64 v128 i64) (result v128)
    (i64x2.add (i64x2.replace_lane 0 (local.get 0) (local.get 1)) (i64x2.replace_lane 1 (local.get 2) (local.get 3))))

  (func (export "swizzle-as-i8x16_add-operands") (param v128 v128 v128 v128) (result v128)
    (i8x16.add (i8x16.swizzle (local.get 0) (local.get 1)) (i8x16.swizzle (local.get 2) (local.get 3))))
  (func (export "shuffle-as-i8x16_sub-operands") (param v128 v128 v128 v128) (result v128)
    (i8x16.sub (i8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 (local.get 0) (local.get 1))
      (i8x16.shuffle 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 (local.get 2) (local.get 3))))

  ;; Boolean horizontal reductions
  (func (export "as-i8x16_any_true-operand") (param v128 i32) (result i32)
    (v128.any_true (i8x16.replace_lane 0 (local.get 0) (local.get 1))))
  (func (export "as-i16x8_any_true-operand") (param v128 i32) (result i32)
    (v128.any_true (i16x8.replace_lane 0 (local.get 0) (local.get 1))))
  (func (export "as-i32x4_any_true-operand1") (param v128 i32) (result i32)
    (v128.any_true (i32x4.replace_lane 0 (local.get 0) (local.get 1))))
  (func (export "as-i32x4_any_true-operand2") (param v128 i64) (result i32)
    (v128.any_true (i64x2.replace_lane 0 (local.get 0) (local.get 1))))

  (func (export "swizzle-as-i8x16_all_true-operands") (param v128 v128) (result i32)
    (i8x16.all_true (i8x16.swizzle (local.get 0) (local.get 1))))
  (func (export "shuffle-as-i8x16_any_true-operands") (param v128 v128) (result i32)
    (v128.any_true (i8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 (local.get 0) (local.get 1))))
)`);

// ./test/core/simd/simd_lane.wast:750
assert_return(() => invoke($2, `as-i8x16_splat-operand`, [i8x16([0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]), [i8x16([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])]);

// ./test/core/simd/simd_lane.wast:751
assert_return(() => invoke($2, `as-i16x8_splat-operand`, [i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0x0, 0x0, 0x0, 0x0])]), [i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff])]);

// ./test/core/simd/simd_lane.wast:752
assert_return(() => invoke($2, `as-i32x4_splat-operand`, [i32x4([0x10000, 0x0, 0x0, 0x0])]), [i32x4([0x10000, 0x10000, 0x10000, 0x10000])]);

// ./test/core/simd/simd_lane.wast:753
assert_return(() => invoke($2, `as-f32x4_splat-operand`, [bytes('v128', [0xc3, 0xf5, 0x48, 0x40, 0x0, 0x0, 0xc0, 0x7f, 0x0, 0x0, 0xc0, 0x7f, 0x0, 0x0, 0xc0, 0x7f])]), [new F32x4Pattern(value('f32', 3.14), value('f32', 3.14), value('f32', 3.14), value('f32', 3.14))]);

// ./test/core/simd/simd_lane.wast:754
assert_return(() => invoke($2, `as-i64x2_splat-operand`, [i64x2([0xffffffffffffffffn, 0x0n])]), [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])]);

// ./test/core/simd/simd_lane.wast:755
assert_return(() => invoke($2, `as-f64x2_splat-operand`, [bytes('v128', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf0, 0x7f, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])]), [new F64x2Pattern(value('f64', Infinity), value('f64', Infinity))]);

// ./test/core/simd/simd_lane.wast:756
assert_return(() => invoke($2, `as-i8x16_add-operands`, [i8x16([0xff, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10]), 1, i8x16([0x10, 0xf, 0xe, 0xd, 0xc, 0xb, 0xa, 0x9, 0x8, 0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0xff]), 1]), [i8x16([0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11])]);

// ./test/core/simd/simd_lane.wast:760
assert_return(() => invoke($2, `as-i16x8_add-operands`, [i16x8([0xffff, 0x4, 0x9, 0x10, 0x19, 0x24, 0x31, 0x40]), 1, i16x8([0x40, 0x31, 0x24, 0x19, 0x10, 0x9, 0x4, 0xffff]), 1]), [i16x8([0x41, 0x35, 0x2d, 0x29, 0x29, 0x2d, 0x35, 0x41])]);

// ./test/core/simd/simd_lane.wast:764
assert_return(() => invoke($2, `as-i32x4_add-operands`, [i32x4([0xffffffff, 0x8, 0x1b, 0x40]), 1, i32x4([0x40, 0x1b, 0x8, 0xffffffff]), 1]), [i32x4([0x41, 0x23, 0x23, 0x41])]);

// ./test/core/simd/simd_lane.wast:766
assert_return(() => invoke($2, `as-i64x2_add-operands`, [i64x2([0xffffffffffffffffn, 0x8n]), 1n, i64x2([0x40n, 0x1bn]), 1n]), [i64x2([0x41n, 0x9n])]);

// ./test/core/simd/simd_lane.wast:769
assert_return(() => invoke($2, `swizzle-as-i8x16_add-operands`, [i8x16([0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff]), i8x16([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf]), i8x16([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf]), i8x16([0xf, 0xe, 0xd, 0xc, 0xb, 0xa, 0x9, 0x8, 0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1, 0x0])]), [i8x16([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])]);

// ./test/core/simd/simd_lane.wast:775
assert_return(() => invoke($2, `shuffle-as-i8x16_sub-operands`, [i8x16([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf]), i8x16([0xf, 0xe, 0xd, 0xc, 0xb, 0xa, 0x9, 0x8, 0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1, 0x0]), i8x16([0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff]), i8x16([0xf, 0xe, 0xd, 0xc, 0xb, 0xa, 0x9, 0x8, 0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1, 0x0])]), [i8x16([0xf1, 0xf3, 0xf5, 0xf7, 0xf9, 0xfb, 0xfd, 0xff, 0x1, 0x3, 0x5, 0x7, 0x9, 0xb, 0xd, 0xf])]);

// ./test/core/simd/simd_lane.wast:782
assert_return(() => invoke($2, `as-i8x16_any_true-operand`, [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), 1]), [value('i32', 1)]);

// ./test/core/simd/simd_lane.wast:783
assert_return(() => invoke($2, `as-i16x8_any_true-operand`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), 1]), [value('i32', 1)]);

// ./test/core/simd/simd_lane.wast:784
assert_return(() => invoke($2, `as-i32x4_any_true-operand1`, [i32x4([0x1, 0x0, 0x0, 0x0]), 0]), [value('i32', 0)]);

// ./test/core/simd/simd_lane.wast:785
assert_return(() => invoke($2, `as-i32x4_any_true-operand2`, [i64x2([0x1n, 0x0n]), 0n]), [value('i32', 0)]);

// ./test/core/simd/simd_lane.wast:787
assert_return(() => invoke($2, `swizzle-as-i8x16_all_true-operands`, [i8x16([0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10]), i8x16([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf])]), [value('i32', 1)]);

// ./test/core/simd/simd_lane.wast:790
assert_return(() => invoke($2, `swizzle-as-i8x16_all_true-operands`, [i8x16([0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff]), i8x16([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0x10])]), [value('i32', 0)]);

// ./test/core/simd/simd_lane.wast:793
assert_return(() => invoke($2, `shuffle-as-i8x16_any_true-operands`, [i8x16([0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff]), i8x16([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf])]), [value('i32', 1)]);

// ./test/core/simd/simd_lane.wast:799
let $3 = instantiate(`(module
  (memory 1)
  (func (export "as-v128_store-operand-1") (param v128 i32) (result v128)
    (v128.store (i32.const 0) (i8x16.replace_lane 0 (local.get 0) (local.get 1)))
    (v128.load (i32.const 0)))
  (func (export "as-v128_store-operand-2") (param v128 i32) (result v128)
    (v128.store (i32.const 0) (i16x8.replace_lane 0 (local.get 0) (local.get 1)))
    (v128.load (i32.const 0)))
  (func (export "as-v128_store-operand-3") (param v128 i32) (result v128)
    (v128.store (i32.const 0) (i32x4.replace_lane 0 (local.get 0) (local.get 1)))
    (v128.load (i32.const 0)))
  (func (export "as-v128_store-operand-4") (param v128 f32) (result v128)
    (v128.store (i32.const 0) (f32x4.replace_lane 0 (local.get 0) (local.get 1)))
    (v128.load (i32.const 0)))
  (func (export "as-v128_store-operand-5") (param v128 i64) (result v128)
    (v128.store (i32.const 0) (i64x2.replace_lane 0 (local.get 0) (local.get 1)))
    (v128.load (i32.const 0)))
  (func (export "as-v128_store-operand-6") (param v128 f64) (result v128)
    (v128.store (i32.const 0) (f64x2.replace_lane 0 (local.get 0) (local.get 1)))
    (v128.load (i32.const 0)))
)`);

// ./test/core/simd/simd_lane.wast:821
assert_return(() => invoke($3, `as-v128_store-operand-1`, [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), 1]), [i8x16([0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_lane.wast:822
assert_return(() => invoke($3, `as-v128_store-operand-2`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), 256]), [i16x8([0x100, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_lane.wast:823
assert_return(() => invoke($3, `as-v128_store-operand-3`, [i32x4([0x0, 0x0, 0x0, 0x0]), -1]), [i32x4([0xffffffff, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_lane.wast:824
assert_return(() => invoke($3, `as-v128_store-operand-4`, [f32x4([0, 0, 0, 0]), value('f32', 3.14)]), [new F32x4Pattern(value('f32', 3.14), value('f32', 0), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_lane.wast:825
assert_return(() => invoke($3, `as-v128_store-operand-5`, [i64x2([0x0n, 0x0n]), -1n]), [i64x2([0xffffffffffffffffn, 0x0n])]);

// ./test/core/simd/simd_lane.wast:826
assert_return(() => invoke($3, `as-v128_store-operand-6`, [f64x2([0, 0]), value('f64', 3.14)]), [new F64x2Pattern(value('f64', 3.14), value('f64', 0))]);

// ./test/core/simd/simd_lane.wast:830
let $4 = instantiate(`(module
  (global $$g (mut v128) (v128.const f32x4 0.0 0.0 0.0 0.0))
  (global $$h (mut v128) (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))
  (func (export "as-if-condition-value") (param v128) (result i32)
    (if (result i32) (i8x16.extract_lane_s 0 (local.get 0)) (then (i32.const 0xff)) (else (i32.const 0))))
  (func (export "as-return-value-1") (param v128 i32) (result v128)
    (return (i16x8.replace_lane 0 (local.get 0) (local.get 1))))
  (func (export "as-local_set-value") (param v128) (result i32) (local i32)
    (local.set 1 (i32x4.extract_lane 0 (local.get 0)))
    (return (local.get 1)))
  (func (export "as-global_set-value-1") (param v128 f32) (result v128)
    (global.set $$g (f32x4.replace_lane 0 (local.get 0) (local.get 1)))
    (return (global.get $$g)))

   (func (export "as-return-value-2") (param v128 v128) (result v128)
    (return (i8x16.swizzle (local.get 0) (local.get 1))))
  (func (export "as-global_set-value-2") (param v128 v128) (result v128)
    (global.set $$h (i8x16.shuffle 0 1 2 3 4 5 6 7 24 25 26 27 28 29 30 31 (local.get 0) (local.get 1)))
    (return (global.get $$h)))

  (func (export "as-local_set-value-1") (param v128) (result i64) (local i64)
    (local.set 1 (i64x2.extract_lane 0 (local.get 0)))
    (return (local.get 1)))
  (func (export "as-global_set-value-3") (param v128 f64) (result v128)
    (global.set $$g (f64x2.replace_lane 0 (local.get 0) (local.get 1)))
    (return (global.get $$g)))
)`);

// ./test/core/simd/simd_lane.wast:858
assert_return(() => invoke($4, `as-if-condition-value`, [i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]), [value('i32', 0)]);

// ./test/core/simd/simd_lane.wast:859
assert_return(() => invoke($4, `as-return-value-1`, [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]), 1]), [i16x8([0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_lane.wast:860
assert_return(() => invoke($4, `as-local_set-value`, [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])]), [value('i32', -1)]);

// ./test/core/simd/simd_lane.wast:861
assert_return(() => invoke($4, `as-global_set-value-1`, [f32x4([0, 0, 0, 0]), value('f32', 3.14)]), [new F32x4Pattern(value('f32', 3.14), value('f32', 0), value('f32', 0), value('f32', 0))]);

// ./test/core/simd/simd_lane.wast:863
assert_return(() => invoke($4, `as-return-value-2`, [i8x16([0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff]), i8x16([0xf, 0xe, 0xd, 0xc, 0xb, 0xa, 0x9, 0x8, 0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1, 0x0])]), [i8x16([0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8, 0xf7, 0xf6, 0xf5, 0xf4, 0xf3, 0xf2, 0xf1, 0xf0])]);

// ./test/core/simd/simd_lane.wast:867
assert_return(() => invoke($4, `as-global_set-value-2`, [i8x16([0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff]), i8x16([0x10, 0xf, 0xe, 0xd, 0xc, 0xb, 0xa, 0x9, 0x8, 0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1])]), [i8x16([0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0x8, 0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1])]);

// ./test/core/simd/simd_lane.wast:872
assert_return(() => invoke($4, `as-local_set-value-1`, [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])]), [value('i64', -1n)]);

// ./test/core/simd/simd_lane.wast:873
assert_return(() => invoke($4, `as-global_set-value-3`, [f64x2([0, 0]), value('f64', 3.14)]), [new F64x2Pattern(value('f64', 3.14), value('f64', 0))]);

// ./test/core/simd/simd_lane.wast:877
assert_malformed(() => instantiate(`(func (result i32) (i8x16.extract_lane_u +0x0f (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:878
assert_malformed(() => instantiate(`(func (result f32) (f32x4.extract_lane +03 (v128.const f32x4 0 0 0 0))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:879
assert_malformed(() => instantiate(`(func (result i64) (i64x2.extract_lane +1 (v128.const i64x2 0 0))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:880
assert_malformed(() => instantiate(`(func (result v128) (i8x16.replace_lane +015 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (i32.const 1))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:881
assert_malformed(() => instantiate(`(func (result v128) (i16x8.replace_lane +0x7 (v128.const i16x8 0 0 0 0 0 0 0 0) (i32.const 1))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:882
assert_malformed(() => instantiate(`(func (result v128) (i32x4.replace_lane +3 (v128.const i32x4 0 0 0 0) (i32.const 1))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:883
assert_malformed(() => instantiate(`(func (result v128) (f64x2.replace_lane +0x01 (v128.const f64x2 0 0) (f64.const 1.0))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:887
let $5 = instantiate(`(module (func (result i32) (i8x16.extract_lane_s 0x0f (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))))`);

// ./test/core/simd/simd_lane.wast:888
let $6 = instantiate(`(module (func (result i32) (i16x8.extract_lane_s 0x07 (v128.const i16x8 0 0 0 0 0 0 0 0))))`);

// ./test/core/simd/simd_lane.wast:889
let $7 = instantiate(`(module (func (result i32) (i16x8.extract_lane_u 0x0_7 (v128.const i16x8 0 0 0 0 0 0 0 0))))`);

// ./test/core/simd/simd_lane.wast:890
let $8 = instantiate(`(module (func (result i32) (i32x4.extract_lane 03 (v128.const i32x4 0 0 0 0))))`);

// ./test/core/simd/simd_lane.wast:891
let $9 = instantiate(`(module (func (result f64) (f64x2.extract_lane 0x1 (v128.const f64x2 0 0))))`);

// ./test/core/simd/simd_lane.wast:892
let $10 = instantiate(`(module (func (result v128) (f32x4.replace_lane 0x3 (v128.const f32x4 0 0 0 0) (f32.const 1.0))))`);

// ./test/core/simd/simd_lane.wast:893
let $11 = instantiate(`(module (func (result v128) (i64x2.replace_lane 01 (v128.const i64x2 0 0) (i64.const 1))))`);

// ./test/core/simd/simd_lane.wast:897
assert_malformed(() => instantiate(`(func (result i32) (i8x16.extract_lane_s 1.0 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:901
assert_malformed(() => instantiate(`(func $$i8x16.extract_lane_s-1st-arg-empty (result i32)   (i8x16.extract_lane_s (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0)) ) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:909
assert_invalid(() => instantiate(`(module
    (func $$i8x16.extract_lane_s-2nd-arg-empty (result i32)
      (i8x16.extract_lane_s 0)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:917
assert_malformed(() => instantiate(`(func $$i8x16.extract_lane_s-arg-empty (result i32)   (i8x16.extract_lane_s) ) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:925
assert_malformed(() => instantiate(`(func $$i16x8.extract_lane_u-1st-arg-empty (result i32)   (i16x8.extract_lane_u (v128.const i16x8 0 0 0 0 0 0 0 0)) ) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:933
assert_invalid(() => instantiate(`(module
    (func $$i16x8.extract_lane_u-2nd-arg-empty (result i32)
      (i16x8.extract_lane_u 0)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:941
assert_malformed(() => instantiate(`(func $$i16x8.extract_lane_u-arg-empty (result i32)   (i16x8.extract_lane_u) ) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:949
assert_malformed(() => instantiate(`(func $$i32x4.extract_lane-1st-arg-empty (result i32)   (i32x4.extract_lane (v128.const i32x4 0 0 0 0)) ) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:957
assert_invalid(() => instantiate(`(module
    (func $$i32x4.extract_lane-2nd-arg-empty (result i32)
      (i32x4.extract_lane 0)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:965
assert_malformed(() => instantiate(`(func $$i32x4.extract_lane-arg-empty (result i32)   (i32x4.extract_lane) ) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:973
assert_malformed(() => instantiate(`(func $$i64x2.extract_lane-1st-arg-empty (result i64)   (i64x2.extract_lane (v128.const i64x2 0 0)) ) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:981
assert_invalid(() => instantiate(`(module
    (func $$i64x2.extract_lane-2nd-arg-empty (result i64)
      (i64x2.extract_lane 0)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:989
assert_malformed(() => instantiate(`(func $$i64x2.extract_lane-arg-empty (result i64)   (i64x2.extract_lane) ) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:997
assert_malformed(() => instantiate(`(func $$f32x4.extract_lane-1st-arg-empty (result f32)   (f32x4.extract_lane (v128.const f32x4 0 0 0 0)) ) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:1005
assert_invalid(() => instantiate(`(module
    (func $$f32x4.extract_lane-2nd-arg-empty (result f32)
      (f32x4.extract_lane 0)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:1013
assert_malformed(() => instantiate(`(func $$f32x4.extract_lane-arg-empty (result f32)   (f32x4.extract_lane) ) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:1021
assert_malformed(() => instantiate(`(func $$f64x2.extract_lane-1st-arg-empty (result f64)   (f64x2.extract_lane (v128.const f64x2 0 0)) ) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:1029
assert_invalid(() => instantiate(`(module
    (func $$f64x2.extract_lane-2nd-arg-empty (result f64)
      (f64x2.extract_lane 0)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:1037
assert_malformed(() => instantiate(`(func $$f64x2.extract_lane-arg-empty (result f64)   (f64x2.extract_lane) ) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:1045
assert_malformed(() => instantiate(`(func $$i8x16.replace_lane-1st-arg-empty (result v128)   (i8x16.replace_lane (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (i32.const 1)) ) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:1053
assert_invalid(() => instantiate(`(module
    (func $$i8x16.replace_lane-2nd-arg-empty (result v128)
      (i8x16.replace_lane 0 (i32.const 1))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:1061
assert_invalid(() => instantiate(`(module
    (func $$i8x16.replace_lane-3rd-arg-empty (result v128)
      (i8x16.replace_lane 0 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:1069
assert_malformed(() => instantiate(`(func $$i8x16.replace_lane-arg-empty (result v128)   (i8x16.replace_lane) ) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:1077
assert_malformed(() => instantiate(`(func $$i16x8.replace_lane-1st-arg-empty (result v128)   (i16x8.replace_lane (v128.const i16x8 0 0 0 0 0 0 0 0) (i32.const 1)) ) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:1085
assert_invalid(() => instantiate(`(module
    (func $$i16x8.replace_lane-2nd-arg-empty (result v128)
      (i16x8.replace_lane 0 (i32.const 1))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:1093
assert_invalid(() => instantiate(`(module
    (func $$i16x8.replace_lane-3rd-arg-empty (result v128)
      (i16x8.replace_lane 0 (v128.const i16x8 0 0 0 0 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:1101
assert_malformed(() => instantiate(`(func $$i16x8.replace_lane-arg-empty (result v128)   (i16x8.replace_lane) ) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:1109
assert_malformed(() => instantiate(`(func $$i32x4.replace_lane-1st-arg-empty (result v128)   (i32x4.replace_lane (v128.const i32x4 0 0 0 0) (i32.const 1)) ) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:1117
assert_invalid(() => instantiate(`(module
    (func $$i32x4.replace_lane-2nd-arg-empty (result v128)
      (i32x4.replace_lane 0 (i32.const 1))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:1125
assert_invalid(() => instantiate(`(module
    (func $$i32x4.replace_lane-3rd-arg-empty (result v128)
      (i32x4.replace_lane 0 (v128.const i32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:1133
assert_malformed(() => instantiate(`(func $$i32x4.replace_lane-arg-empty (result v128)   (i32x4.replace_lane) ) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:1141
assert_malformed(() => instantiate(`(func $$f32x4.replace_lane-1st-arg-empty (result v128)   (f32x4.replace_lane (v128.const f32x4 0 0 0 0) (f32.const 1.0)) ) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:1149
assert_invalid(() => instantiate(`(module
    (func $$f32x4.replace_lane-2nd-arg-empty (result v128)
      (f32x4.replace_lane 0 (f32.const 1.0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:1157
assert_invalid(() => instantiate(`(module
    (func $$f32x4.replace_lane-3rd-arg-empty (result v128)
      (f32x4.replace_lane 0 (v128.const f32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:1165
assert_malformed(() => instantiate(`(func $$f32x4.replace_lane-arg-empty (result v128)   (f32x4.replace_lane) ) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:1173
assert_malformed(() => instantiate(`(func $$i64x2.replace_lane-1st-arg-empty (result v128)   (i64x2.replace_lane (v128.const i64x2 0 0) (i64.const 1)) ) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:1181
assert_invalid(() => instantiate(`(module
    (func $$i64x2.replace_lane-2nd-arg-empty (result v128)
      (i64x2.replace_lane 0 (i64.const 1))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:1189
assert_invalid(() => instantiate(`(module
    (func $$i64x2.replace_lane-3rd-arg-empty (result v128)
      (i64x2.replace_lane 0 (v128.const i64x2 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:1197
assert_malformed(() => instantiate(`(func $$i64x2.replace_lane-arg-empty (result v128)   (i64x2.replace_lane) ) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:1205
assert_malformed(() => instantiate(`(func $$f64x2.replace_lane-1st-arg-empty (result v128)   (f64x2.replace_lane (v128.const f64x2 0 0) (f64.const 1.0)) ) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:1213
assert_invalid(() => instantiate(`(module
    (func $$f64x2.replace_lane-2nd-arg-empty (result v128)
      (f64x2.replace_lane 0 (f64.const 1.0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:1221
assert_invalid(() => instantiate(`(module
    (func $$f64x2.replace_lane-3rd-arg-empty (result v128)
      (f64x2.replace_lane 0 (v128.const f64x2 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:1229
assert_malformed(() => instantiate(`(func $$f64x2.replace_lane-arg-empty (result v128)   (f64x2.replace_lane) ) `), `unexpected token`);

// ./test/core/simd/simd_lane.wast:1237
assert_malformed(() => instantiate(`(func $$i8x16.shuffle-1st-arg-empty (result v128)   (i8x16.shuffle     (v128.const i8x16 0 1 2 3 5 6 6 7 8 9 10 11 12 13 14 15)     (v128.const i8x16 1 2 3 5 6 6 7 8 9 10 11 12 13 14 15 16)   ) ) `), `invalid lane length`);

// ./test/core/simd/simd_lane.wast:1248
assert_invalid(() => instantiate(`(module
    (func $$i8x16.shuffle-2nd-arg-empty (result v128)
      (i8x16.shuffle 0 1 2 3 5 6 6 7 8 9 10 11 12 13 14 15
        (v128.const i8x16 1 2 3 5 6 6 7 8 9 10 11 12 13 14 15 16)
      )
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_lane.wast:1258
assert_malformed(() => instantiate(`(func $$i8x16.shuffle-arg-empty (result v128)   (i8x16.shuffle) ) `), `invalid lane length`);

