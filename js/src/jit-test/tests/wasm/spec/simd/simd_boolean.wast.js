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

// ./test/core/simd/simd_boolean.wast

// ./test/core/simd/simd_boolean.wast:3
let $0 = instantiate(`(module
  (func (export "i8x16.any_true") (param $$0 v128) (result i32) (v128.any_true (local.get $$0)))
  (func (export "i8x16.all_true") (param $$0 v128) (result i32) (i8x16.all_true (local.get $$0)))
  (func (export "i8x16.bitmask") (param $$0 v128) (result i32) (i8x16.bitmask (local.get $$0)))

  (func (export "i16x8.any_true") (param $$0 v128) (result i32) (v128.any_true (local.get $$0)))
  (func (export "i16x8.all_true") (param $$0 v128) (result i32) (i16x8.all_true (local.get $$0)))
  (func (export "i16x8.bitmask") (param $$0 v128) (result i32) (i16x8.bitmask (local.get $$0)))

  (func (export "i32x4.any_true") (param $$0 v128) (result i32) (v128.any_true (local.get $$0)))
  (func (export "i32x4.all_true") (param $$0 v128) (result i32) (i32x4.all_true (local.get $$0)))
  (func (export "i32x4.bitmask") (param $$0 v128) (result i32) (i32x4.bitmask (local.get $$0)))

  (func (export "i64x2.all_true") (param $$0 v128) (result i32) (i64x2.all_true (local.get $$0)))
  (func (export "i64x2.bitmask") (param $$0 v128) (result i32) (i64x2.bitmask (local.get $$0)))
)`);

// ./test/core/simd/simd_boolean.wast:21
assert_return(
  () =>
    invoke($0, `i8x16.any_true`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:23
assert_return(
  () =>
    invoke($0, `i8x16.any_true`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x1,
        0x0,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:25
assert_return(
  () =>
    invoke($0, `i8x16.any_true`, [
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x0,
        0x1,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:27
assert_return(
  () =>
    invoke($0, `i8x16.any_true`, [
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:29
assert_return(
  () =>
    invoke($0, `i8x16.any_true`, [
      i8x16([
        0xff,
        0x0,
        0x1,
        0x2,
        0x3,
        0x4,
        0x5,
        0x6,
        0x7,
        0x8,
        0x9,
        0xa,
        0xb,
        0xc,
        0xd,
        0xf,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:31
assert_return(
  () =>
    invoke($0, `i8x16.any_true`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:33
assert_return(
  () =>
    invoke($0, `i8x16.any_true`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:35
assert_return(
  () =>
    invoke($0, `i8x16.any_true`, [
      i8x16([
        0xab,
        0xab,
        0xab,
        0xab,
        0xab,
        0xab,
        0xab,
        0xab,
        0xab,
        0xab,
        0xab,
        0xab,
        0xab,
        0xab,
        0xab,
        0xab,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:37
assert_return(
  () =>
    invoke($0, `i8x16.any_true`, [
      i8x16([
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:39
assert_return(
  () =>
    invoke($0, `i8x16.all_true`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:41
assert_return(
  () =>
    invoke($0, `i8x16.all_true`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x1,
        0x0,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:43
assert_return(
  () =>
    invoke($0, `i8x16.all_true`, [
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x0,
        0x1,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:45
assert_return(
  () =>
    invoke($0, `i8x16.all_true`, [
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:47
assert_return(
  () =>
    invoke($0, `i8x16.all_true`, [
      i8x16([
        0xff,
        0x0,
        0x1,
        0x2,
        0x3,
        0x4,
        0x5,
        0x6,
        0x7,
        0x8,
        0x9,
        0xa,
        0xb,
        0xc,
        0xd,
        0xf,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:49
assert_return(
  () =>
    invoke($0, `i8x16.all_true`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:51
assert_return(
  () =>
    invoke($0, `i8x16.all_true`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:53
assert_return(
  () =>
    invoke($0, `i8x16.all_true`, [
      i8x16([
        0xab,
        0xab,
        0xab,
        0xab,
        0xab,
        0xab,
        0xab,
        0xab,
        0xab,
        0xab,
        0xab,
        0xab,
        0xab,
        0xab,
        0xab,
        0xab,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:55
assert_return(
  () =>
    invoke($0, `i8x16.all_true`, [
      i8x16([
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:57
assert_return(
  () =>
    invoke($0, `i8x16.bitmask`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
    ]),
  [value("i32", 65535)],
);

// ./test/core/simd/simd_boolean.wast:59
assert_return(
  () =>
    invoke($0, `i8x16.bitmask`, [
      i8x16([
        0xff,
        0x0,
        0x1,
        0x2,
        0x3,
        0x4,
        0x5,
        0x6,
        0x7,
        0x8,
        0x9,
        0xa,
        0xb,
        0xc,
        0xd,
        0xf,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:63
assert_return(
  () =>
    invoke($0, `i16x8.any_true`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:65
assert_return(
  () =>
    invoke($0, `i16x8.any_true`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:67
assert_return(
  () =>
    invoke($0, `i16x8.any_true`, [
      i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0, 0x1]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:69
assert_return(
  () =>
    invoke($0, `i16x8.any_true`, [
      i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:71
assert_return(
  () =>
    invoke($0, `i16x8.any_true`, [
      i16x8([0xffff, 0x0, 0x1, 0x2, 0xb, 0xc, 0xd, 0xf]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:73
assert_return(
  () =>
    invoke($0, `i16x8.any_true`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:75
assert_return(
  () =>
    invoke($0, `i16x8.any_true`, [
      i16x8([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:77
assert_return(
  () =>
    invoke($0, `i16x8.any_true`, [
      i16x8([0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:79
assert_return(
  () =>
    invoke($0, `i16x8.any_true`, [
      i16x8([0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:81
assert_return(
  () =>
    invoke($0, `i16x8.any_true`, [
      i16x8([0x3039, 0x3039, 0x3039, 0x3039, 0x3039, 0x3039, 0x3039, 0x3039]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:83
assert_return(
  () =>
    invoke($0, `i16x8.any_true`, [
      i16x8([0x1234, 0x1234, 0x1234, 0x1234, 0x1234, 0x1234, 0x1234, 0x1234]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:85
assert_return(
  () =>
    invoke($0, `i16x8.all_true`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:87
assert_return(
  () =>
    invoke($0, `i16x8.all_true`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:89
assert_return(
  () =>
    invoke($0, `i16x8.all_true`, [
      i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0, 0x1]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:91
assert_return(
  () =>
    invoke($0, `i16x8.all_true`, [
      i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:93
assert_return(
  () =>
    invoke($0, `i16x8.all_true`, [
      i16x8([0xffff, 0x0, 0x1, 0x2, 0xb, 0xc, 0xd, 0xf]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:95
assert_return(
  () =>
    invoke($0, `i16x8.all_true`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:97
assert_return(
  () =>
    invoke($0, `i16x8.all_true`, [
      i16x8([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:99
assert_return(
  () =>
    invoke($0, `i16x8.all_true`, [
      i16x8([0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:101
assert_return(
  () =>
    invoke($0, `i16x8.all_true`, [
      i16x8([0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:103
assert_return(
  () =>
    invoke($0, `i16x8.all_true`, [
      i16x8([0x3039, 0x3039, 0x3039, 0x3039, 0x3039, 0x3039, 0x3039, 0x3039]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:105
assert_return(
  () =>
    invoke($0, `i16x8.all_true`, [
      i16x8([0x1234, 0x1234, 0x1234, 0x1234, 0x1234, 0x1234, 0x1234, 0x1234]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:107
assert_return(
  () =>
    invoke($0, `i16x8.bitmask`, [
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [value("i32", 255)],
);

// ./test/core/simd/simd_boolean.wast:109
assert_return(
  () =>
    invoke($0, `i16x8.bitmask`, [
      i16x8([0xffff, 0x0, 0x1, 0x2, 0xb, 0xc, 0xd, 0xf]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:113
assert_return(
  () => invoke($0, `i32x4.any_true`, [i32x4([0x0, 0x0, 0x0, 0x0])]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:115
assert_return(
  () => invoke($0, `i32x4.any_true`, [i32x4([0x0, 0x0, 0x1, 0x0])]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:117
assert_return(
  () => invoke($0, `i32x4.any_true`, [i32x4([0x1, 0x1, 0x0, 0x1])]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:119
assert_return(
  () => invoke($0, `i32x4.any_true`, [i32x4([0x1, 0x1, 0x1, 0x1])]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:121
assert_return(
  () => invoke($0, `i32x4.any_true`, [i32x4([0xffffffff, 0x0, 0x1, 0xf])]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:123
assert_return(
  () => invoke($0, `i32x4.any_true`, [i32x4([0x0, 0x0, 0x0, 0x0])]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:125
assert_return(
  () => invoke($0, `i32x4.any_true`, [i32x4([0xff, 0xff, 0xff, 0xff])]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:127
assert_return(
  () => invoke($0, `i32x4.any_true`, [i32x4([0xab, 0xab, 0xab, 0xab])]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:129
assert_return(
  () => invoke($0, `i32x4.any_true`, [i32x4([0x55, 0x55, 0x55, 0x55])]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:131
assert_return(
  () =>
    invoke($0, `i32x4.any_true`, [
      i32x4([0x499602d2, 0x499602d2, 0x499602d2, 0x499602d2]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:133
assert_return(
  () =>
    invoke($0, `i32x4.any_true`, [
      i32x4([0x12345678, 0x12345678, 0x12345678, 0x12345678]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:135
assert_return(
  () => invoke($0, `i32x4.all_true`, [i32x4([0x0, 0x0, 0x0, 0x0])]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:137
assert_return(
  () => invoke($0, `i32x4.all_true`, [i32x4([0x0, 0x0, 0x1, 0x0])]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:139
assert_return(
  () => invoke($0, `i32x4.all_true`, [i32x4([0x1, 0x1, 0x0, 0x1])]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:141
assert_return(
  () => invoke($0, `i32x4.all_true`, [i32x4([0x1, 0x1, 0x1, 0x1])]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:143
assert_return(
  () => invoke($0, `i32x4.all_true`, [i32x4([0xffffffff, 0x0, 0x1, 0xf])]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:145
assert_return(
  () => invoke($0, `i32x4.all_true`, [i32x4([0x0, 0x0, 0x0, 0x0])]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:147
assert_return(
  () => invoke($0, `i32x4.all_true`, [i32x4([0xff, 0xff, 0xff, 0xff])]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:149
assert_return(
  () => invoke($0, `i32x4.all_true`, [i32x4([0xab, 0xab, 0xab, 0xab])]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:151
assert_return(
  () => invoke($0, `i32x4.all_true`, [i32x4([0x55, 0x55, 0x55, 0x55])]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:153
assert_return(
  () =>
    invoke($0, `i32x4.all_true`, [
      i32x4([0x499602d2, 0x499602d2, 0x499602d2, 0x499602d2]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:155
assert_return(
  () =>
    invoke($0, `i32x4.all_true`, [
      i32x4([0x12345678, 0x12345678, 0x12345678, 0x12345678]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:157
assert_return(
  () =>
    invoke($0, `i32x4.bitmask`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [value("i32", 15)],
);

// ./test/core/simd/simd_boolean.wast:159
assert_return(
  () => invoke($0, `i32x4.bitmask`, [i32x4([0xffffffff, 0x0, 0x1, 0xf])]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:163
assert_return(() => invoke($0, `i64x2.all_true`, [i64x2([0x0n, 0x0n])]), [
  value("i32", 0),
]);

// ./test/core/simd/simd_boolean.wast:165
assert_return(() => invoke($0, `i64x2.all_true`, [i64x2([0x0n, 0x1n])]), [
  value("i32", 0),
]);

// ./test/core/simd/simd_boolean.wast:167
assert_return(() => invoke($0, `i64x2.all_true`, [i64x2([0x1n, 0x0n])]), [
  value("i32", 0),
]);

// ./test/core/simd/simd_boolean.wast:169
assert_return(() => invoke($0, `i64x2.all_true`, [i64x2([0x1n, 0x1n])]), [
  value("i32", 1),
]);

// ./test/core/simd/simd_boolean.wast:171
assert_return(
  () => invoke($0, `i64x2.all_true`, [i64x2([0xffffffffffffffffn, 0x0n])]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:173
assert_return(() => invoke($0, `i64x2.all_true`, [i64x2([0x0n, 0x0n])]), [
  value("i32", 0),
]);

// ./test/core/simd/simd_boolean.wast:175
assert_return(() => invoke($0, `i64x2.all_true`, [i64x2([0xffn, 0xffn])]), [
  value("i32", 1),
]);

// ./test/core/simd/simd_boolean.wast:177
assert_return(() => invoke($0, `i64x2.all_true`, [i64x2([0xabn, 0xabn])]), [
  value("i32", 1),
]);

// ./test/core/simd/simd_boolean.wast:179
assert_return(() => invoke($0, `i64x2.all_true`, [i64x2([0x55n, 0x55n])]), [
  value("i32", 1),
]);

// ./test/core/simd/simd_boolean.wast:181
assert_return(
  () =>
    invoke($0, `i64x2.bitmask`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [value("i32", 3)],
);

// ./test/core/simd/simd_boolean.wast:183
assert_return(
  () => invoke($0, `i64x2.bitmask`, [i64x2([0xffffffffffffffffn, 0xfn])]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:188
let $1 = instantiate(`(module (memory 1)
    ;; as if condition
    (func (export "i8x16_any_true_as_if_cond") (param v128) (result i32)
        (if (result i32) (v128.any_true (local.get 0))
            (then (i32.const 1))
            (else (i32.const 0))
        )
    )
    (func (export "i16x8_any_true_as_if_cond") (param v128) (result i32)
        (if (result i32) (v128.any_true (local.get 0))
            (then (i32.const 1))
            (else (i32.const 0))
        )
    )
    (func (export "i32x4_any_true_as_if_cond") (param v128) (result i32)
        (if (result i32) (v128.any_true (local.get 0))
            (then (i32.const 1))
            (else (i32.const 0))
        )
    )
    (func (export "i8x16_all_true_as_if_cond") (param v128) (result i32)
        (if (result i32) (i8x16.all_true (local.get 0))
            (then (i32.const 1))
            (else (i32.const 0))
        )
    )
    (func (export "i16x8_all_true_as_if_cond") (param v128) (result i32)
        (if (result i32) (i16x8.all_true (local.get 0))
            (then (i32.const 1))
            (else (i32.const 0))
        )
    )
    (func (export "i32x4_all_true_as_if_cond") (param v128) (result i32)
        (if (result i32) (i32x4.all_true (local.get 0))
            (then (i32.const 1))
            (else (i32.const 0))
        )
    )
    ;; any_true as select condition
    (func (export "i8x16_any_true_as_select_cond") (param v128) (result i32)
     (select (i32.const 1) (i32.const 0) (v128.any_true (local.get 0)))
    )
    (func (export "i16x8_any_true_as_select_cond") (param v128) (result i32)
     (select (i32.const 1) (i32.const 0) (v128.any_true (local.get 0)))
    )
    (func (export "i32x4_any_true_as_select_cond") (param v128) (result i32)
     (select (i32.const 1) (i32.const 0) (v128.any_true (local.get 0)))
    )
    ;; all_true as select condition
    (func (export "i8x16_all_true_as_select_cond") (param v128) (result i32)
     (select (i32.const 1) (i32.const 0) (i8x16.all_true (local.get 0)))
    )
    (func (export "i16x8_all_true_as_select_cond") (param v128) (result i32)
     (select (i32.const 1) (i32.const 0) (i16x8.all_true (local.get 0)))
    )
    (func (export "i32x4_all_true_as_select_cond") (param v128) (result i32)
     (select (i32.const 1) (i32.const 0) (i32x4.all_true (local.get 0)))
    )
    ;; any_true as br_if condition
    (func (export "i8x16_any_true_as_br_if_cond") (param $$0 v128) (result i32)
      (local $$1 i32)
      (local.set $$1 (i32.const 2))
      (block
        (local.set $$1 (i32.const 1))
        (br_if 0 (v128.any_true (local.get $$0)))
        (local.set $$1 (i32.const 0))
      )
      (local.get $$1)
    )
    (func (export "i16x8_any_true_as_br_if_cond") (param $$0 v128) (result i32)
      (local $$1 i32)
      (local.set $$1 (i32.const 2))
      (block
        (local.set $$1 (i32.const 1))
        (br_if 0 (v128.any_true (local.get $$0)))
        (local.set $$1 (i32.const 0))
      )
      (local.get $$1)
    )
    (func (export "i32x4_any_true_as_br_if_cond") (param $$0 v128) (result i32)
      (local $$1 i32)
      (local.set $$1 (i32.const 2))
      (block
        (local.set $$1 (i32.const 1))
        (br_if 0 (v128.any_true (local.get $$0)))
        (local.set $$1 (i32.const 0))
      )
      (local.get $$1)
    )
    ;; all_true as br_if condition
    (func (export "i8x16_all_true_as_br_if_cond") (param $$0 v128) (result i32)
      (local $$1 i32)
      (local.set $$1 (i32.const 2))
      (block
        (local.set $$1 (i32.const 1))
        (br_if 0 (i8x16.all_true (local.get $$0)))
        (local.set $$1 (i32.const 0))
      )
      (local.get $$1)
    )
    (func (export "i16x8_all_true_as_br_if_cond") (param $$0 v128) (result i32)
      (local $$1 i32)
      (local.set $$1 (i32.const 2))
      (block
        (local.set $$1 (i32.const 1))
        (br_if 0 (i16x8.all_true (local.get $$0)))
        (local.set $$1 (i32.const 0))
      )
      (local.get $$1)
    )
    (func (export "i32x4_all_true_as_br_if_cond") (param $$0 v128) (result i32)
      (local $$1 i32)
      (local.set $$1 (i32.const 2))
      (block
        (local.set $$1 (i32.const 1))
        (br_if 0 (i32x4.all_true (local.get $$0)))
        (local.set $$1 (i32.const 0))
      )
      (local.get $$1)
    )
    ;; any_true as i32.and operand
    (func (export "i8x16_any_true_as_i32.and_operand") (param $$0 v128) (param $$1 v128) (result i32)
      (i32.and (v128.any_true (local.get $$0)) (v128.any_true (local.get $$1)))
    )
    (func (export "i16x8_any_true_as_i32.and_operand") (param $$0 v128) (param $$1 v128) (result i32)
      (i32.and (v128.any_true (local.get $$0)) (v128.any_true (local.get $$1)))
    )
    (func (export "i32x4_any_true_as_i32.and_operand") (param $$0 v128) (param $$1 v128) (result i32)
      (i32.and (v128.any_true (local.get $$0)) (v128.any_true (local.get $$1)))
    )
     ;; any_true as i32.or operand
    (func (export "i8x16_any_true_as_i32.or_operand") (param $$0 v128) (param $$1 v128) (result i32)
      (i32.or (v128.any_true (local.get $$0)) (v128.any_true (local.get $$1)))
    )
    (func (export "i16x8_any_true_as_i32.or_operand") (param $$0 v128) (param $$1 v128) (result i32)
      (i32.or (v128.any_true (local.get $$0)) (v128.any_true (local.get $$1)))
    )
    (func (export "i32x4_any_true_as_i32.or_operand") (param $$0 v128) (param $$1 v128) (result i32)
      (i32.or (v128.any_true (local.get $$0)) (v128.any_true (local.get $$1)))
    )
     ;; any_true as i32.xor operand
    (func (export "i8x16_any_true_as_i32.xor_operand") (param $$0 v128) (param $$1 v128) (result i32)
      (i32.xor (v128.any_true (local.get $$0)) (v128.any_true (local.get $$1)))
    )
    (func (export "i16x8_any_true_as_i32.xor_operand") (param $$0 v128) (param $$1 v128) (result i32)
      (i32.xor (v128.any_true (local.get $$0)) (v128.any_true (local.get $$1)))
    )
    (func (export "i32x4_any_true_as_i32.xor_operand") (param $$0 v128) (param $$1 v128) (result i32)
      (i32.xor (v128.any_true (local.get $$0)) (v128.any_true (local.get $$1)))
    )
     ;; all_true as i32.and operand
    (func (export "i8x16_all_true_as_i32.and_operand") (param $$0 v128) (param $$1 v128) (result i32)
      (i32.and (i8x16.all_true (local.get $$0)) (i8x16.all_true (local.get $$1)))
    )
    (func (export "i16x8_all_true_as_i32.and_operand") (param $$0 v128) (param $$1 v128) (result i32)
      (i32.and (i16x8.all_true (local.get $$0)) (i16x8.all_true (local.get $$1)))
    )
    (func (export "i32x4_all_true_as_i32.and_operand") (param $$0 v128) (param $$1 v128) (result i32)
      (i32.and (i32x4.all_true (local.get $$0)) (i32x4.all_true (local.get $$1)))
    )
     ;; all_true as i32.or operand
    (func (export "i8x16_all_true_as_i32.or_operand") (param $$0 v128) (param $$1 v128) (result i32)
      (i32.or (i8x16.all_true (local.get $$0)) (i8x16.all_true (local.get $$1)))
    )
    (func (export "i16x8_all_true_as_i32.or_operand") (param $$0 v128) (param $$1 v128) (result i32)
      (i32.or (i16x8.all_true (local.get $$0)) (i16x8.all_true (local.get $$1)))
    )
    (func (export "i32x4_all_true_as_i32.or_operand") (param $$0 v128) (param $$1 v128) (result i32)
      (i32.or (i32x4.all_true (local.get $$0)) (i32x4.all_true (local.get $$1)))
    )
     ;; all_true as i32.xor operand
    (func (export "i8x16_all_true_as_i32.xor_operand") (param $$0 v128) (param $$1 v128) (result i32)
      (i32.xor (i8x16.all_true (local.get $$0)) (i8x16.all_true (local.get $$1)))
    )
    (func (export "i16x8_all_true_as_i32.xor_operand") (param $$0 v128) (param $$1 v128) (result i32)
      (i32.xor (i16x8.all_true (local.get $$0)) (i16x8.all_true (local.get $$1)))
    )
    (func (export "i32x4_all_true_as_i32.xor_operand") (param $$0 v128) (param $$1 v128) (result i32)
      (i32.xor (i32x4.all_true (local.get $$0)) (i32x4.all_true (local.get $$1)))
    )
    ;; any_true with v128.not
    (func (export "i8x16_any_true_with_v128.not") (param $$0 v128) (result i32)
       (v128.any_true (v128.not (local.get $$0)))
    )
    (func (export "i16x8_any_true_with_v128.not") (param $$0 v128) (result i32)
       (v128.any_true (v128.not (local.get $$0)))
    )
    (func (export "i32x4_any_true_with_v128.not") (param $$0 v128) (result i32)
       (v128.any_true (v128.not (local.get $$0)))
    )
    ;; any_true with v128.and
    (func (export "i8x16_any_true_with_v128.and") (param $$0 v128) (param $$1 v128) (result i32)
       (v128.any_true (v128.and (local.get $$0) (local.get $$1)))
    )
    (func (export "i16x8_any_true_with_v128.and") (param $$0 v128) (param $$1 v128) (result i32)
       (v128.any_true (v128.and (local.get $$0) (local.get $$1)))
    )
    (func (export "i32x4_any_true_with_v128.and") (param $$0 v128) (param $$1 v128) (result i32)
       (v128.any_true (v128.and (local.get $$0) (local.get $$1)))
    )
    ;; any_true with v128.or
    (func (export "i8x16_any_true_with_v128.or") (param $$0 v128) (param $$1 v128) (result i32)
       (v128.any_true (v128.or (local.get $$0) (local.get $$1)))
    )
    (func (export "i16x8_any_true_with_v128.or") (param $$0 v128) (param $$1 v128) (result i32)
       (v128.any_true (v128.or (local.get $$0) (local.get $$1)))
    )
    (func (export "i32x4_any_true_with_v128.or") (param $$0 v128) (param $$1 v128) (result i32)
       (v128.any_true (v128.or (local.get $$0) (local.get $$1)))
    )
    ;; any_true with v128.xor
    (func (export "i8x16_any_true_with_v128.xor") (param $$0 v128) (param $$1 v128) (result i32)
       (v128.any_true (v128.xor (local.get $$0) (local.get $$1)))
    )
    (func (export "i16x8_any_true_with_v128.xor") (param $$0 v128) (param $$1 v128) (result i32)
       (v128.any_true (v128.xor (local.get $$0) (local.get $$1)))
    )
    (func (export "i32x4_any_true_with_v128.xor") (param $$0 v128) (param $$1 v128) (result i32)
       (v128.any_true (v128.xor (local.get $$0) (local.get $$1)))
    )
    ;; any_true with v128.bitselect
    (func (export "i8x16_any_true_with_v128.bitselect") (param $$0 v128) (param $$1 v128) (param $$2 v128) (result i32)
       (v128.any_true (v128.bitselect (local.get $$0) (local.get $$1) (local.get $$2)))
    )
    (func (export "i16x8_any_true_with_v128.bitselect") (param $$0 v128) (param $$1 v128) (param $$2 v128) (result i32)
       (v128.any_true (v128.bitselect (local.get $$0) (local.get $$1) (local.get $$2)))
    )
    (func (export "i32x4_any_true_with_v128.bitselect") (param $$0 v128) (param $$1 v128) (param $$2 v128) (result i32)
       (v128.any_true (v128.bitselect (local.get $$0) (local.get $$1) (local.get $$2)))
    )
    ;; all_true with v128.not
    (func (export "i8x16_all_true_with_v128.not") (param $$0 v128) (result i32)
       (i8x16.all_true (v128.not (local.get $$0)))
    )
    (func (export "i16x8_all_true_with_v128.not") (param $$0 v128) (result i32)
       (i16x8.all_true (v128.not (local.get $$0)))
    )
    (func (export "i32x4_all_true_with_v128.not") (param $$0 v128) (result i32)
       (i32x4.all_true (v128.not (local.get $$0)))
    )
    ;; all_true with v128.and
    (func (export "i8x16_all_true_with_v128.and") (param $$0 v128) (param $$1 v128) (result i32)
       (i8x16.all_true (v128.and (local.get $$0) (local.get $$1)))
    )
    (func (export "i16x8_all_true_with_v128.and") (param $$0 v128) (param $$1 v128) (result i32)
       (i16x8.all_true (v128.and (local.get $$0) (local.get $$1)))
    )
    (func (export "i32x4_all_true_with_v128.and") (param $$0 v128) (param $$1 v128) (result i32)
       (i32x4.all_true (v128.and (local.get $$0) (local.get $$1)))
    )
    ;; all_true with v128.or
    (func (export "i8x16_all_true_with_v128.or") (param $$0 v128) (param $$1 v128) (result i32)
       (i8x16.all_true (v128.or (local.get $$0) (local.get $$1)))
    )
    (func (export "i16x8_all_true_with_v128.or") (param $$0 v128) (param $$1 v128) (result i32)
       (i16x8.all_true (v128.or (local.get $$0) (local.get $$1)))
    )
    (func (export "i32x4_all_true_with_v128.or") (param $$0 v128) (param $$1 v128) (result i32)
       (i32x4.all_true (v128.or (local.get $$0) (local.get $$1)))
    )
    ;; all_true with v128.xor
    (func (export "i8x16_all_true_with_v128.xor") (param $$0 v128) (param $$1 v128) (result i32)
       (i8x16.all_true (v128.xor (local.get $$0) (local.get $$1)))
    )
    (func (export "i16x8_all_true_with_v128.xor") (param $$0 v128) (param $$1 v128) (result i32)
       (i16x8.all_true (v128.xor (local.get $$0) (local.get $$1)))
    )
    (func (export "i32x4_all_true_with_v128.xor") (param $$0 v128) (param $$1 v128) (result i32)
       (i32x4.all_true (v128.xor (local.get $$0) (local.get $$1)))
    )
    ;; all_true with v128.bitselect
    (func (export "i8x16_all_true_with_v128.bitselect") (param $$0 v128) (param $$1 v128) (param $$2 v128) (result i32)
       (i8x16.all_true (v128.bitselect (local.get $$0) (local.get $$1) (local.get $$2)))
    )
    (func (export "i16x8_all_true_with_v128.bitselect") (param $$0 v128) (param $$1 v128) (param $$2 v128) (result i32)
       (i16x8.all_true (v128.bitselect (local.get $$0) (local.get $$1) (local.get $$2)))
    )
    (func (export "i32x4_all_true_with_v128.bitselect") (param $$0 v128) (param $$1 v128) (param $$2 v128) (result i32)
       (i32x4.all_true (v128.bitselect (local.get $$0) (local.get $$1) (local.get $$2)))
    )
)`);

// ./test/core/simd/simd_boolean.wast:472
assert_return(
  () =>
    invoke($1, `i8x16_any_true_as_if_cond`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:474
assert_return(
  () =>
    invoke($1, `i8x16_any_true_as_if_cond`, [
      i8x16([
        0x0,
        0x0,
        0x1,
        0x0,
        0x0,
        0x0,
        0x1,
        0x0,
        0x0,
        0x0,
        0x1,
        0x0,
        0x0,
        0x0,
        0x1,
        0x0,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:476
assert_return(
  () =>
    invoke($1, `i8x16_any_true_as_if_cond`, [
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:479
assert_return(
  () =>
    invoke($1, `i16x8_any_true_as_if_cond`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:481
assert_return(
  () =>
    invoke($1, `i16x8_any_true_as_if_cond`, [
      i16x8([0x0, 0x0, 0x1, 0x0, 0x0, 0x0, 0x1, 0x0]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:483
assert_return(
  () =>
    invoke($1, `i16x8_any_true_as_if_cond`, [
      i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:486
assert_return(
  () => invoke($1, `i32x4_any_true_as_if_cond`, [i32x4([0x0, 0x0, 0x0, 0x0])]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:488
assert_return(
  () => invoke($1, `i32x4_any_true_as_if_cond`, [i32x4([0x0, 0x0, 0x1, 0x0])]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:490
assert_return(
  () => invoke($1, `i32x4_any_true_as_if_cond`, [i32x4([0x1, 0x1, 0x1, 0x1])]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:495
assert_return(
  () =>
    invoke($1, `i8x16_all_true_as_if_cond`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:497
assert_return(
  () =>
    invoke($1, `i8x16_all_true_as_if_cond`, [
      i8x16([
        0x1,
        0x1,
        0x1,
        0x0,
        0x1,
        0x1,
        0x1,
        0x0,
        0x1,
        0x1,
        0x1,
        0x0,
        0x1,
        0x1,
        0x1,
        0x0,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:499
assert_return(
  () =>
    invoke($1, `i8x16_all_true_as_if_cond`, [
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:502
assert_return(
  () =>
    invoke($1, `i16x8_all_true_as_if_cond`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:504
assert_return(
  () =>
    invoke($1, `i16x8_all_true_as_if_cond`, [
      i16x8([0x1, 0x1, 0x1, 0x0, 0x1, 0x1, 0x1, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:506
assert_return(
  () =>
    invoke($1, `i16x8_all_true_as_if_cond`, [
      i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:509
assert_return(
  () => invoke($1, `i32x4_all_true_as_if_cond`, [i32x4([0x0, 0x0, 0x0, 0x0])]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:511
assert_return(
  () => invoke($1, `i32x4_all_true_as_if_cond`, [i32x4([0x1, 0x1, 0x1, 0x0])]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:513
assert_return(
  () => invoke($1, `i32x4_all_true_as_if_cond`, [i32x4([0x1, 0x1, 0x1, 0x1])]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:517
assert_return(
  () =>
    invoke($1, `i8x16_any_true_as_select_cond`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:519
assert_return(
  () =>
    invoke($1, `i8x16_any_true_as_select_cond`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x1,
        0x0,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:521
assert_return(
  () =>
    invoke($1, `i16x8_any_true_as_select_cond`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:523
assert_return(
  () =>
    invoke($1, `i16x8_any_true_as_select_cond`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:525
assert_return(
  () =>
    invoke($1, `i32x4_any_true_as_select_cond`, [i32x4([0x0, 0x0, 0x0, 0x0])]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:527
assert_return(
  () =>
    invoke($1, `i32x4_any_true_as_select_cond`, [i32x4([0x0, 0x0, 0x1, 0x0])]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:530
assert_return(
  () =>
    invoke($1, `i8x16_all_true_as_select_cond`, [
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:532
assert_return(
  () =>
    invoke($1, `i8x16_all_true_as_select_cond`, [
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x0,
        0x1,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:534
assert_return(
  () =>
    invoke($1, `i16x8_all_true_as_select_cond`, [
      i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:536
assert_return(
  () =>
    invoke($1, `i16x8_all_true_as_select_cond`, [
      i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0, 0x1]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:538
assert_return(
  () =>
    invoke($1, `i32x4_all_true_as_select_cond`, [i32x4([0x1, 0x1, 0x1, 0x1])]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:540
assert_return(
  () =>
    invoke($1, `i32x4_all_true_as_select_cond`, [i32x4([0x1, 0x1, 0x0, 0x1])]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:543
assert_return(
  () =>
    invoke($1, `i8x16_any_true_as_br_if_cond`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:545
assert_return(
  () =>
    invoke($1, `i8x16_any_true_as_br_if_cond`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x1,
        0x0,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:547
assert_return(
  () =>
    invoke($1, `i16x8_any_true_as_br_if_cond`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:549
assert_return(
  () =>
    invoke($1, `i16x8_any_true_as_br_if_cond`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:551
assert_return(
  () =>
    invoke($1, `i32x4_any_true_as_br_if_cond`, [i32x4([0x0, 0x0, 0x0, 0x0])]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:553
assert_return(
  () =>
    invoke($1, `i32x4_any_true_as_br_if_cond`, [i32x4([0x0, 0x0, 0x1, 0x0])]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:556
assert_return(
  () =>
    invoke($1, `i8x16_all_true_as_br_if_cond`, [
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:558
assert_return(
  () =>
    invoke($1, `i8x16_all_true_as_br_if_cond`, [
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x0,
        0x1,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:560
assert_return(
  () =>
    invoke($1, `i16x8_all_true_as_br_if_cond`, [
      i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:562
assert_return(
  () =>
    invoke($1, `i16x8_all_true_as_br_if_cond`, [
      i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0, 0x1]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:564
assert_return(
  () =>
    invoke($1, `i32x4_all_true_as_br_if_cond`, [i32x4([0x1, 0x1, 0x1, 0x1])]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:566
assert_return(
  () =>
    invoke($1, `i32x4_all_true_as_br_if_cond`, [i32x4([0x1, 0x1, 0x0, 0x1])]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:569
assert_return(
  () =>
    invoke($1, `i8x16_any_true_as_i32.and_operand`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:572
assert_return(
  () =>
    invoke($1, `i8x16_any_true_as_i32.and_operand`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x1,
        0x0,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:575
assert_return(
  () =>
    invoke($1, `i8x16_any_true_as_i32.and_operand`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x1,
        0x0,
      ]),
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x1,
        0x0,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:578
assert_return(
  () =>
    invoke($1, `i16x8_any_true_as_i32.and_operand`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:581
assert_return(
  () =>
    invoke($1, `i16x8_any_true_as_i32.and_operand`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:584
assert_return(
  () =>
    invoke($1, `i16x8_any_true_as_i32.and_operand`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:587
assert_return(
  () =>
    invoke($1, `i32x4_any_true_as_i32.and_operand`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:590
assert_return(
  () =>
    invoke($1, `i32x4_any_true_as_i32.and_operand`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x1, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:593
assert_return(
  () =>
    invoke($1, `i32x4_any_true_as_i32.and_operand`, [
      i32x4([0x0, 0x0, 0x1, 0x0]),
      i32x4([0x0, 0x0, 0x1, 0x0]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:597
assert_return(
  () =>
    invoke($1, `i8x16_any_true_as_i32.or_operand`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:600
assert_return(
  () =>
    invoke($1, `i8x16_any_true_as_i32.or_operand`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x1,
        0x0,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:603
assert_return(
  () =>
    invoke($1, `i8x16_any_true_as_i32.or_operand`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x1,
        0x0,
      ]),
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x1,
        0x0,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:606
assert_return(
  () =>
    invoke($1, `i16x8_any_true_as_i32.or_operand`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:609
assert_return(
  () =>
    invoke($1, `i16x8_any_true_as_i32.or_operand`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:612
assert_return(
  () =>
    invoke($1, `i16x8_any_true_as_i32.or_operand`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:615
assert_return(
  () =>
    invoke($1, `i32x4_any_true_as_i32.or_operand`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:618
assert_return(
  () =>
    invoke($1, `i32x4_any_true_as_i32.or_operand`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x1, 0x0]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:621
assert_return(
  () =>
    invoke($1, `i32x4_any_true_as_i32.or_operand`, [
      i32x4([0x0, 0x0, 0x1, 0x0]),
      i32x4([0x0, 0x0, 0x1, 0x0]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:625
assert_return(
  () =>
    invoke($1, `i8x16_any_true_as_i32.xor_operand`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:628
assert_return(
  () =>
    invoke($1, `i8x16_any_true_as_i32.xor_operand`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x1,
        0x0,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:631
assert_return(
  () =>
    invoke($1, `i8x16_any_true_as_i32.xor_operand`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x1,
        0x0,
      ]),
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x1,
        0x0,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:634
assert_return(
  () =>
    invoke($1, `i16x8_any_true_as_i32.xor_operand`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:637
assert_return(
  () =>
    invoke($1, `i16x8_any_true_as_i32.xor_operand`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:640
assert_return(
  () =>
    invoke($1, `i16x8_any_true_as_i32.xor_operand`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:643
assert_return(
  () =>
    invoke($1, `i32x4_any_true_as_i32.xor_operand`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:646
assert_return(
  () =>
    invoke($1, `i32x4_any_true_as_i32.xor_operand`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x1, 0x0]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:649
assert_return(
  () =>
    invoke($1, `i32x4_any_true_as_i32.xor_operand`, [
      i32x4([0x0, 0x0, 0x1, 0x0]),
      i32x4([0x0, 0x0, 0x1, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:653
assert_return(
  () =>
    invoke($1, `i8x16_all_true_as_i32.and_operand`, [
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
      ]),
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:656
assert_return(
  () =>
    invoke($1, `i8x16_all_true_as_i32.and_operand`, [
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
      ]),
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x0,
        0x1,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:659
assert_return(
  () =>
    invoke($1, `i8x16_all_true_as_i32.and_operand`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:662
assert_return(
  () =>
    invoke($1, `i16x8_all_true_as_i32.and_operand`, [
      i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1]),
      i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:665
assert_return(
  () =>
    invoke($1, `i16x8_all_true_as_i32.and_operand`, [
      i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1]),
      i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0, 0x1]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:668
assert_return(
  () =>
    invoke($1, `i16x8_all_true_as_i32.and_operand`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:671
assert_return(
  () =>
    invoke($1, `i32x4_all_true_as_i32.and_operand`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:674
assert_return(
  () =>
    invoke($1, `i32x4_all_true_as_i32.and_operand`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x1, 0x1, 0x0, 0x1]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:677
assert_return(
  () =>
    invoke($1, `i32x4_all_true_as_i32.and_operand`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x1, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:681
assert_return(
  () =>
    invoke($1, `i8x16_all_true_as_i32.or_operand`, [
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
      ]),
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:684
assert_return(
  () =>
    invoke($1, `i8x16_all_true_as_i32.or_operand`, [
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
      ]),
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x0,
        0x1,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:687
assert_return(
  () =>
    invoke($1, `i8x16_all_true_as_i32.or_operand`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:690
assert_return(
  () =>
    invoke($1, `i16x8_all_true_as_i32.or_operand`, [
      i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1]),
      i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:693
assert_return(
  () =>
    invoke($1, `i16x8_all_true_as_i32.or_operand`, [
      i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1]),
      i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0, 0x1]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:696
assert_return(
  () =>
    invoke($1, `i16x8_all_true_as_i32.or_operand`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:699
assert_return(
  () =>
    invoke($1, `i32x4_all_true_as_i32.or_operand`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:702
assert_return(
  () =>
    invoke($1, `i32x4_all_true_as_i32.or_operand`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x1, 0x1, 0x0, 0x1]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:705
assert_return(
  () =>
    invoke($1, `i32x4_all_true_as_i32.or_operand`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:709
assert_return(
  () =>
    invoke($1, `i8x16_all_true_as_i32.xor_operand`, [
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
      ]),
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:712
assert_return(
  () =>
    invoke($1, `i8x16_all_true_as_i32.xor_operand`, [
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
      ]),
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x0,
        0x1,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:715
assert_return(
  () =>
    invoke($1, `i8x16_all_true_as_i32.xor_operand`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:718
assert_return(
  () =>
    invoke($1, `i16x8_all_true_as_i32.xor_operand`, [
      i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1]),
      i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:721
assert_return(
  () =>
    invoke($1, `i16x8_all_true_as_i32.xor_operand`, [
      i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1]),
      i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0, 0x1]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:724
assert_return(
  () =>
    invoke($1, `i16x8_all_true_as_i32.xor_operand`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:727
assert_return(
  () =>
    invoke($1, `i32x4_all_true_as_i32.xor_operand`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:730
assert_return(
  () =>
    invoke($1, `i32x4_all_true_as_i32.xor_operand`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x1, 0x1, 0x0, 0x1]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:733
assert_return(
  () =>
    invoke($1, `i32x4_all_true_as_i32.xor_operand`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:737
assert_return(
  () =>
    invoke($1, `i8x16_any_true_with_v128.not`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:739
assert_return(
  () =>
    invoke($1, `i8x16_any_true_with_v128.not`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:741
assert_return(
  () =>
    invoke($1, `i8x16_any_true_with_v128.not`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0xff,
        0x0,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:743
assert_return(
  () =>
    invoke($1, `i16x8_any_true_with_v128.not`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:745
assert_return(
  () =>
    invoke($1, `i16x8_any_true_with_v128.not`, [
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:747
assert_return(
  () =>
    invoke($1, `i16x8_any_true_with_v128.not`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xffff, 0x0]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:749
assert_return(
  () =>
    invoke($1, `i32x4_any_true_with_v128.not`, [i32x4([0x0, 0x0, 0x0, 0x0])]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:751
assert_return(
  () =>
    invoke($1, `i32x4_any_true_with_v128.not`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:753
assert_return(
  () =>
    invoke($1, `i32x4_any_true_with_v128.not`, [
      i32x4([0x0, 0x0, 0xffffffff, 0x0]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:756
assert_return(
  () =>
    invoke($1, `i8x16_any_true_with_v128.and`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:759
assert_return(
  () =>
    invoke($1, `i8x16_any_true_with_v128.and`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:762
assert_return(
  () =>
    invoke($1, `i8x16_any_true_with_v128.and`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0xff,
        0x0,
      ]),
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0xff,
        0x0,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:765
assert_return(
  () =>
    invoke($1, `i16x8_any_true_with_v128.and`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:768
assert_return(
  () =>
    invoke($1, `i16x8_any_true_with_v128.and`, [
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:771
assert_return(
  () =>
    invoke($1, `i16x8_any_true_with_v128.and`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xffff, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xffff, 0x0]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:774
assert_return(
  () =>
    invoke($1, `i32x4_any_true_with_v128.and`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:777
assert_return(
  () =>
    invoke($1, `i32x4_any_true_with_v128.and`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:780
assert_return(
  () =>
    invoke($1, `i32x4_any_true_with_v128.and`, [
      i32x4([0x0, 0x0, 0xffffffff, 0x0]),
      i32x4([0x0, 0x0, 0xffffffff, 0x0]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:784
assert_return(
  () =>
    invoke($1, `i8x16_any_true_with_v128.or`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:787
assert_return(
  () =>
    invoke($1, `i8x16_any_true_with_v128.or`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:790
assert_return(
  () =>
    invoke($1, `i8x16_any_true_with_v128.or`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0xff,
        0x0,
      ]),
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0xff,
        0x0,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:793
assert_return(
  () =>
    invoke($1, `i16x8_any_true_with_v128.or`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:796
assert_return(
  () =>
    invoke($1, `i16x8_any_true_with_v128.or`, [
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:799
assert_return(
  () =>
    invoke($1, `i16x8_any_true_with_v128.or`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xffff, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xffff, 0x0]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:802
assert_return(
  () =>
    invoke($1, `i32x4_any_true_with_v128.or`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:805
assert_return(
  () =>
    invoke($1, `i32x4_any_true_with_v128.or`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:808
assert_return(
  () =>
    invoke($1, `i32x4_any_true_with_v128.or`, [
      i32x4([0x0, 0x0, 0xffffffff, 0x0]),
      i32x4([0x0, 0x0, 0xffffffff, 0x0]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:812
assert_return(
  () =>
    invoke($1, `i8x16_any_true_with_v128.xor`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:815
assert_return(
  () =>
    invoke($1, `i8x16_any_true_with_v128.xor`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:818
assert_return(
  () =>
    invoke($1, `i8x16_any_true_with_v128.xor`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0xff,
        0x0,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:821
assert_return(
  () =>
    invoke($1, `i16x8_any_true_with_v128.xor`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:824
assert_return(
  () =>
    invoke($1, `i16x8_any_true_with_v128.xor`, [
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:827
assert_return(
  () =>
    invoke($1, `i16x8_any_true_with_v128.xor`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xffff, 0x0]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:830
assert_return(
  () =>
    invoke($1, `i32x4_any_true_with_v128.xor`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:833
assert_return(
  () =>
    invoke($1, `i32x4_any_true_with_v128.xor`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:836
assert_return(
  () =>
    invoke($1, `i32x4_any_true_with_v128.xor`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0xffffffff, 0x0]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:840
assert_return(
  () =>
    invoke($1, `i8x16_any_true_with_v128.bitselect`, [
      i8x16([
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
      ]),
      i8x16([
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
      ]),
      i8x16([
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:844
assert_return(
  () =>
    invoke($1, `i8x16_any_true_with_v128.bitselect`, [
      i8x16([
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
      ]),
      i8x16([
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
      ]),
      i8x16([
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0xff,
        0x55,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:848
assert_return(
  () =>
    invoke($1, `i16x8_any_true_with_v128.bitselect`, [
      i16x8([0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa]),
      i16x8([0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55]),
      i16x8([0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:852
assert_return(
  () =>
    invoke($1, `i16x8_any_true_with_v128.bitselect`, [
      i16x8([0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa]),
      i16x8([0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55]),
      i16x8([0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xff, 0x55]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:856
assert_return(
  () =>
    invoke($1, `i32x4_any_true_with_v128.bitselect`, [
      i32x4([0xaa, 0xaa, 0xaa, 0xaa]),
      i32x4([0x55, 0x55, 0x55, 0x55]),
      i32x4([0x55, 0x55, 0x55, 0x55]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:860
assert_return(
  () =>
    invoke($1, `i32x4_any_true_with_v128.bitselect`, [
      i32x4([0xaa, 0xaa, 0xaa, 0xaa]),
      i32x4([0x55, 0x55, 0x55, 0x55]),
      i32x4([0x55, 0x55, 0xff, 0x55]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:865
assert_return(
  () =>
    invoke($1, `i8x16_all_true_with_v128.not`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:867
assert_return(
  () =>
    invoke($1, `i8x16_all_true_with_v128.not`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:869
assert_return(
  () =>
    invoke($1, `i8x16_all_true_with_v128.not`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0xff,
        0x0,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:871
assert_return(
  () =>
    invoke($1, `i16x8_all_true_with_v128.not`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:873
assert_return(
  () =>
    invoke($1, `i16x8_all_true_with_v128.not`, [
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:875
assert_return(
  () =>
    invoke($1, `i16x8_all_true_with_v128.not`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xffff, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:877
assert_return(
  () =>
    invoke($1, `i32x4_all_true_with_v128.not`, [i32x4([0x0, 0x0, 0x0, 0x0])]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:879
assert_return(
  () =>
    invoke($1, `i32x4_all_true_with_v128.not`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:881
assert_return(
  () =>
    invoke($1, `i32x4_all_true_with_v128.not`, [
      i32x4([0x0, 0x0, 0xffffffff, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:884
assert_return(
  () =>
    invoke($1, `i8x16_all_true_with_v128.and`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:887
assert_return(
  () =>
    invoke($1, `i8x16_all_true_with_v128.and`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:890
assert_return(
  () =>
    invoke($1, `i8x16_all_true_with_v128.and`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0xff,
        0x0,
      ]),
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0xff,
        0x0,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:893
assert_return(
  () =>
    invoke($1, `i16x8_all_true_with_v128.and`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:896
assert_return(
  () =>
    invoke($1, `i16x8_all_true_with_v128.and`, [
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:899
assert_return(
  () =>
    invoke($1, `i16x8_all_true_with_v128.and`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xffff, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xffff, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:902
assert_return(
  () =>
    invoke($1, `i32x4_all_true_with_v128.and`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:905
assert_return(
  () =>
    invoke($1, `i32x4_all_true_with_v128.and`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:908
assert_return(
  () =>
    invoke($1, `i32x4_all_true_with_v128.and`, [
      i32x4([0x0, 0x0, 0xffffffff, 0x0]),
      i32x4([0x0, 0x0, 0xffffffff, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:912
assert_return(
  () =>
    invoke($1, `i8x16_all_true_with_v128.or`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:915
assert_return(
  () =>
    invoke($1, `i8x16_all_true_with_v128.or`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:918
assert_return(
  () =>
    invoke($1, `i8x16_all_true_with_v128.or`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0xff,
        0x0,
      ]),
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0xff,
        0x0,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:921
assert_return(
  () =>
    invoke($1, `i16x8_all_true_with_v128.or`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:924
assert_return(
  () =>
    invoke($1, `i16x8_all_true_with_v128.or`, [
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:927
assert_return(
  () =>
    invoke($1, `i16x8_all_true_with_v128.or`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xffff, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xffff, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:930
assert_return(
  () =>
    invoke($1, `i32x4_all_true_with_v128.or`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:933
assert_return(
  () =>
    invoke($1, `i32x4_all_true_with_v128.or`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:936
assert_return(
  () =>
    invoke($1, `i32x4_all_true_with_v128.or`, [
      i32x4([0x0, 0x0, 0xffffffff, 0x0]),
      i32x4([0x0, 0x0, 0xffffffff, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:940
assert_return(
  () =>
    invoke($1, `i8x16_all_true_with_v128.xor`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:943
assert_return(
  () =>
    invoke($1, `i8x16_all_true_with_v128.xor`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:946
assert_return(
  () =>
    invoke($1, `i8x16_all_true_with_v128.xor`, [
      i8x16([
        0x0,
        0xff,
        0x0,
        0xff,
        0x0,
        0xff,
        0x0,
        0xff,
        0x0,
        0xff,
        0x0,
        0xff,
        0x0,
        0xff,
        0x0,
        0xff,
      ]),
      i8x16([
        0xff,
        0x0,
        0xff,
        0x0,
        0xff,
        0x0,
        0xff,
        0x0,
        0xff,
        0x0,
        0xff,
        0x0,
        0xff,
        0x0,
        0xff,
        0x0,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:949
assert_return(
  () =>
    invoke($1, `i16x8_all_true_with_v128.xor`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:952
assert_return(
  () =>
    invoke($1, `i16x8_all_true_with_v128.xor`, [
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:955
assert_return(
  () =>
    invoke($1, `i16x8_all_true_with_v128.xor`, [
      i16x8([0x0, 0xffff, 0x0, 0xffff, 0x0, 0xffff, 0x0, 0xffff]),
      i16x8([0xffff, 0x0, 0xffff, 0x0, 0xffff, 0x0, 0xffff, 0x0]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:958
assert_return(
  () =>
    invoke($1, `i32x4_all_true_with_v128.xor`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:961
assert_return(
  () =>
    invoke($1, `i32x4_all_true_with_v128.xor`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:964
assert_return(
  () =>
    invoke($1, `i32x4_all_true_with_v128.xor`, [
      i32x4([0x0, 0xffffffff, 0x0, 0xffffffff]),
      i32x4([0xffffffff, 0x0, 0xffffffff, 0x0]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:968
assert_return(
  () =>
    invoke($1, `i8x16_all_true_with_v128.bitselect`, [
      i8x16([
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
      ]),
      i8x16([
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
      ]),
      i8x16([
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
      ]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:972
assert_return(
  () =>
    invoke($1, `i8x16_all_true_with_v128.bitselect`, [
      i8x16([
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
      ]),
      i8x16([
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
      ]),
      i8x16([
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
      ]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:976
assert_return(
  () =>
    invoke($1, `i16x8_all_true_with_v128.bitselect`, [
      i16x8([0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa]),
      i16x8([0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55]),
      i16x8([0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:980
assert_return(
  () =>
    invoke($1, `i16x8_all_true_with_v128.bitselect`, [
      i16x8([0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa]),
      i16x8([0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55]),
      i16x8([0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:984
assert_return(
  () =>
    invoke($1, `i32x4_all_true_with_v128.bitselect`, [
      i32x4([0xaa, 0xaa, 0xaa, 0xaa]),
      i32x4([0x55, 0x55, 0x55, 0x55]),
      i32x4([0x55, 0x55, 0x55, 0x55]),
    ]),
  [value("i32", 0)],
);

// ./test/core/simd/simd_boolean.wast:988
assert_return(
  () =>
    invoke($1, `i32x4_all_true_with_v128.bitselect`, [
      i32x4([0xaa, 0xaa, 0xaa, 0xaa]),
      i32x4([0x55, 0x55, 0x55, 0x55]),
      i32x4([0xaa, 0xaa, 0xaa, 0xaa]),
    ]),
  [value("i32", 1)],
);

// ./test/core/simd/simd_boolean.wast:995
assert_invalid(
  () =>
    instantiate(`(module (func (result i32) (v128.any_true (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/simd/simd_boolean.wast:996
assert_invalid(
  () =>
    instantiate(`(module (func (result i32) (i8x16.all_true (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/simd/simd_boolean.wast:997
assert_invalid(
  () =>
    instantiate(`(module (func (result i32) (v128.any_true (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/simd/simd_boolean.wast:998
assert_invalid(
  () =>
    instantiate(`(module (func (result i32) (i16x8.all_true (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/simd/simd_boolean.wast:999
assert_invalid(
  () =>
    instantiate(`(module (func (result i32) (v128.any_true (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/simd/simd_boolean.wast:1000
assert_invalid(
  () =>
    instantiate(`(module (func (result i32) (i32x4.all_true (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/simd/simd_boolean.wast:1004
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result i32) (f32x4.any_true (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_boolean.wast:1005
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result i32) (f32x4.all_true (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_boolean.wast:1006
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result i32) (f64x2.any_true (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_boolean.wast:1007
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result i32) (f64x2.all_true (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_boolean.wast:1011
assert_invalid(() =>
  instantiate(`(module
    (func $$v128.any_true-arg-empty (result v128)
      (v128.any_true)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_boolean.wast:1019
assert_invalid(() =>
  instantiate(`(module
    (func $$i8x16.all_true-arg-empty (result v128)
      (i8x16.all_true)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_boolean.wast:1027
assert_invalid(() =>
  instantiate(`(module
    (func $$v128.any_true-arg-empty (result v128)
      (v128.any_true)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_boolean.wast:1035
assert_invalid(() =>
  instantiate(`(module
    (func $$i16x8.all_true-arg-empty (result v128)
      (i16x8.all_true)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_boolean.wast:1043
assert_invalid(() =>
  instantiate(`(module
    (func $$v128.any_true-arg-empty (result v128)
      (v128.any_true)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_boolean.wast:1051
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.all_true-arg-empty (result v128)
      (i32x4.all_true)
    )
  )`), `type mismatch`);
