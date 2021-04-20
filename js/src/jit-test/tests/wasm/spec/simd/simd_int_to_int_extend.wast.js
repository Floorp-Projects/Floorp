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

// ./test/core/simd/simd_int_to_int_extend.wast

// ./test/core/simd/simd_int_to_int_extend.wast:3
let $0 = instantiate(`(module
  (func (export "i16x8.extend_high_i8x16_s") (param v128) (result v128) (i16x8.extend_high_i8x16_s (local.get 0)))
  (func (export "i16x8.extend_high_i8x16_u") (param v128) (result v128) (i16x8.extend_high_i8x16_u (local.get 0)))
  (func (export "i16x8.extend_low_i8x16_s") (param v128) (result v128) (i16x8.extend_low_i8x16_s (local.get 0)))
  (func (export "i16x8.extend_low_i8x16_u") (param v128) (result v128) (i16x8.extend_low_i8x16_u (local.get 0)))
  (func (export "i32x4.extend_high_i16x8_s") (param v128) (result v128) (i32x4.extend_high_i16x8_s (local.get 0)))
  (func (export "i32x4.extend_high_i16x8_u") (param v128) (result v128) (i32x4.extend_high_i16x8_u (local.get 0)))
  (func (export "i32x4.extend_low_i16x8_s") (param v128) (result v128) (i32x4.extend_low_i16x8_s (local.get 0)))
  (func (export "i32x4.extend_low_i16x8_u") (param v128) (result v128) (i32x4.extend_low_i16x8_u (local.get 0)))
  (func (export "i64x2.extend_high_i32x4_s") (param v128) (result v128) (i64x2.extend_high_i32x4_s (local.get 0)))
  (func (export "i64x2.extend_high_i32x4_u") (param v128) (result v128) (i64x2.extend_high_i32x4_u (local.get 0)))
  (func (export "i64x2.extend_low_i32x4_s") (param v128) (result v128) (i64x2.extend_low_i32x4_s (local.get 0)))
  (func (export "i64x2.extend_low_i32x4_u") (param v128) (result v128) (i64x2.extend_low_i32x4_u (local.get 0)))
)`);

// ./test/core/simd/simd_int_to_int_extend.wast:18
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_s`, [
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
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:20
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_s`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
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
  [i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:22
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_s`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
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
  [i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:24
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_s`, [
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
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
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:26
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_s`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
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
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:28
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_s`, [
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
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
  [i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:30
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_s`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
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
  [i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:32
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_s`, [
      i8x16([
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
      ]),
    ]),
  [i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:34
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_s`, [
      i8x16([
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
      ]),
    ]),
  [i16x8([0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:36
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_s`, [
      i8x16([
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
      ]),
    ]),
  [i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:38
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_s`, [
      i8x16([
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
      ]),
    ]),
  [i16x8([0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:40
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_s`, [
      i8x16([
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
      ]),
    ]),
  [i16x8([0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:42
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_s`, [
      i8x16([
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
      ]),
    ]),
  [i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:44
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_s`, [
      i8x16([
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
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
  [i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:46
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_s`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
      ]),
    ]),
  [i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:48
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_s`, [
      i8x16([
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
      ]),
    ]),
  [i16x8([0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:50
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_s`, [
      i8x16([
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
      ]),
    ]),
  [i16x8([0xff81, 0xff81, 0xff81, 0xff81, 0xff81, 0xff81, 0xff81, 0xff81])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:52
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_s`, [
      i8x16([
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
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
  [i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:54
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_s`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
      ]),
    ]),
  [i16x8([0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:57
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_u`, [
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
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:59
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_u`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
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
  [i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:61
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_u`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
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
  [i16x8([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:63
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_u`, [
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
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
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:65
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_u`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
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
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:67
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_u`, [
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
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
  [i16x8([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:69
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_u`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
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
  [i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:71
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_u`, [
      i8x16([
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
      ]),
    ]),
  [i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:73
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_u`, [
      i8x16([
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
      ]),
    ]),
  [i16x8([0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:75
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_u`, [
      i8x16([
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
      ]),
    ]),
  [i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:77
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_u`, [
      i8x16([
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
      ]),
    ]),
  [i16x8([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:79
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_u`, [
      i8x16([
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
      ]),
    ]),
  [i16x8([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:81
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_u`, [
      i8x16([
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
      ]),
    ]),
  [i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:83
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_u`, [
      i8x16([
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
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
  [i16x8([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:85
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_u`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
      ]),
    ]),
  [i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:87
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_u`, [
      i8x16([
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
      ]),
    ]),
  [i16x8([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:89
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_u`, [
      i8x16([
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
      ]),
    ]),
  [i16x8([0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:91
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_u`, [
      i8x16([
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
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
  [i16x8([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:93
assert_return(
  () =>
    invoke($0, `i16x8.extend_high_i8x16_u`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
      ]),
    ]),
  [i16x8([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:96
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_s`, [
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
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:98
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_s`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
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
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:100
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_s`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
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
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:102
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_s`, [
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
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
  [i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:104
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_s`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
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
  [i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:106
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_s`, [
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
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
  [i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:108
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_s`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
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
  [i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:110
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_s`, [
      i8x16([
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
      ]),
    ]),
  [i16x8([0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:112
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_s`, [
      i8x16([
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
      ]),
    ]),
  [i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:114
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_s`, [
      i8x16([
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
      ]),
    ]),
  [i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:116
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_s`, [
      i8x16([
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
      ]),
    ]),
  [i16x8([0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:118
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_s`, [
      i8x16([
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
      ]),
    ]),
  [i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:120
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_s`, [
      i8x16([
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
      ]),
    ]),
  [i16x8([0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:122
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_s`, [
      i8x16([
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
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
  [i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:124
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_s`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
      ]),
    ]),
  [i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:126
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_s`, [
      i8x16([
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
      ]),
    ]),
  [i16x8([0xff81, 0xff81, 0xff81, 0xff81, 0xff81, 0xff81, 0xff81, 0xff81])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:128
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_s`, [
      i8x16([
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
      ]),
    ]),
  [i16x8([0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:130
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_s`, [
      i8x16([
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
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
  [i16x8([0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80, 0xff80])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:132
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_s`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
      ]),
    ]),
  [i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:135
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_u`, [
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
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:137
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_u`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
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
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:139
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_u`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
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
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:141
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_u`, [
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
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
  [i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:143
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_u`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
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
  [i16x8([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:145
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_u`, [
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
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
  [i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:147
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_u`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
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
  [i16x8([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:149
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_u`, [
      i8x16([
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
      ]),
    ]),
  [i16x8([0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:151
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_u`, [
      i8x16([
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
      ]),
    ]),
  [i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:153
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_u`, [
      i8x16([
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
      ]),
    ]),
  [i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:155
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_u`, [
      i8x16([
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
      ]),
    ]),
  [i16x8([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:157
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_u`, [
      i8x16([
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
      ]),
    ]),
  [i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:159
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_u`, [
      i8x16([
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
      ]),
    ]),
  [i16x8([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:161
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_u`, [
      i8x16([
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
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
  [i16x8([0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:163
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_u`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
      ]),
    ]),
  [i16x8([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:165
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_u`, [
      i8x16([
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
      ]),
    ]),
  [i16x8([0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:167
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_u`, [
      i8x16([
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
      ]),
    ]),
  [i16x8([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:169
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_u`, [
      i8x16([
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
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
  [i16x8([0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:171
assert_return(
  () =>
    invoke($0, `i16x8.extend_low_i8x16_u`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
      ]),
    ]),
  [i16x8([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:174
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_s`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:176
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_s`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:178
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_s`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:180
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_s`, [
      i16x8([0x1, 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:182
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_s`, [
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:184
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_s`, [
      i16x8([0x1, 0x1, 0x1, 0x1, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:186
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_s`, [
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:188
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_s`, [
      i16x8([0x7ffe, 0x7ffe, 0x7ffe, 0x7ffe, 0x7fff, 0x7fff, 0x7fff, 0x7fff]),
    ]),
  [i32x4([0x7fff, 0x7fff, 0x7fff, 0x7fff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:190
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_s`, [
      i16x8([0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7ffe, 0x7ffe, 0x7ffe, 0x7ffe]),
    ]),
  [i32x4([0x7ffe, 0x7ffe, 0x7ffe, 0x7ffe])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:192
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_s`, [
      i16x8([0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff]),
    ]),
  [i32x4([0x7fff, 0x7fff, 0x7fff, 0x7fff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:194
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_s`, [
      i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000]),
    ]),
  [i32x4([0xffff8000, 0xffff8000, 0xffff8000, 0xffff8000])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:196
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_s`, [
      i16x8([0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x8000, 0x8000, 0x8000, 0x8000]),
    ]),
  [i32x4([0xffff8000, 0xffff8000, 0xffff8000, 0xffff8000])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:198
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_s`, [
      i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0x7fff, 0x7fff, 0x7fff, 0x7fff]),
    ]),
  [i32x4([0x7fff, 0x7fff, 0x7fff, 0x7fff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:200
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_s`, [
      i16x8([0x7fff, 0x7fff, 0x7fff, 0x7fff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:202
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_s`, [
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0x7fff, 0x7fff, 0x7fff, 0x7fff]),
    ]),
  [i32x4([0x7fff, 0x7fff, 0x7fff, 0x7fff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:204
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_s`, [
      i16x8([0x8001, 0x8001, 0x8001, 0x8001, 0x8000, 0x8000, 0x8000, 0x8000]),
    ]),
  [i32x4([0xffff8000, 0xffff8000, 0xffff8000, 0xffff8000])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:206
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_s`, [
      i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0x8001, 0x8001, 0x8001, 0x8001]),
    ]),
  [i32x4([0xffff8001, 0xffff8001, 0xffff8001, 0xffff8001])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:208
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_s`, [
      i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:210
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_s`, [
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0x8000, 0x8000, 0x8000, 0x8000]),
    ]),
  [i32x4([0xffff8000, 0xffff8000, 0xffff8000, 0xffff8000])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:213
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_u`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:215
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_u`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:217
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_u`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0xffff, 0xffff, 0xffff, 0xffff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:219
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_u`, [
      i16x8([0x1, 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:221
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_u`, [
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:223
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_u`, [
      i16x8([0x1, 0x1, 0x1, 0x1, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0xffff, 0xffff, 0xffff, 0xffff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:225
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_u`, [
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:227
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_u`, [
      i16x8([0x7ffe, 0x7ffe, 0x7ffe, 0x7ffe, 0x7fff, 0x7fff, 0x7fff, 0x7fff]),
    ]),
  [i32x4([0x7fff, 0x7fff, 0x7fff, 0x7fff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:229
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_u`, [
      i16x8([0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7ffe, 0x7ffe, 0x7ffe, 0x7ffe]),
    ]),
  [i32x4([0x7ffe, 0x7ffe, 0x7ffe, 0x7ffe])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:231
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_u`, [
      i16x8([0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff]),
    ]),
  [i32x4([0x7fff, 0x7fff, 0x7fff, 0x7fff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:233
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_u`, [
      i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000]),
    ]),
  [i32x4([0x8000, 0x8000, 0x8000, 0x8000])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:235
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_u`, [
      i16x8([0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x8000, 0x8000, 0x8000, 0x8000]),
    ]),
  [i32x4([0x8000, 0x8000, 0x8000, 0x8000])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:237
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_u`, [
      i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0x7fff, 0x7fff, 0x7fff, 0x7fff]),
    ]),
  [i32x4([0x7fff, 0x7fff, 0x7fff, 0x7fff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:239
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_u`, [
      i16x8([0x7fff, 0x7fff, 0x7fff, 0x7fff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0xffff, 0xffff, 0xffff, 0xffff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:241
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_u`, [
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0x7fff, 0x7fff, 0x7fff, 0x7fff]),
    ]),
  [i32x4([0x7fff, 0x7fff, 0x7fff, 0x7fff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:243
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_u`, [
      i16x8([0x8001, 0x8001, 0x8001, 0x8001, 0x8000, 0x8000, 0x8000, 0x8000]),
    ]),
  [i32x4([0x8000, 0x8000, 0x8000, 0x8000])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:245
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_u`, [
      i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0x8001, 0x8001, 0x8001, 0x8001]),
    ]),
  [i32x4([0x8001, 0x8001, 0x8001, 0x8001])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:247
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_u`, [
      i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0xffff, 0xffff, 0xffff, 0xffff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:249
assert_return(
  () =>
    invoke($0, `i32x4.extend_high_i16x8_u`, [
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0x8000, 0x8000, 0x8000, 0x8000]),
    ]),
  [i32x4([0x8000, 0x8000, 0x8000, 0x8000])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:252
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_s`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:254
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_s`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:256
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_s`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:258
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_s`, [
      i16x8([0x1, 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:260
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_s`, [
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:262
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_s`, [
      i16x8([0x1, 0x1, 0x1, 0x1, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:264
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_s`, [
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:266
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_s`, [
      i16x8([0x7ffe, 0x7ffe, 0x7ffe, 0x7ffe, 0x7fff, 0x7fff, 0x7fff, 0x7fff]),
    ]),
  [i32x4([0x7ffe, 0x7ffe, 0x7ffe, 0x7ffe])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:268
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_s`, [
      i16x8([0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7ffe, 0x7ffe, 0x7ffe, 0x7ffe]),
    ]),
  [i32x4([0x7fff, 0x7fff, 0x7fff, 0x7fff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:270
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_s`, [
      i16x8([0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff]),
    ]),
  [i32x4([0x7fff, 0x7fff, 0x7fff, 0x7fff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:272
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_s`, [
      i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000]),
    ]),
  [i32x4([0xffff8000, 0xffff8000, 0xffff8000, 0xffff8000])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:274
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_s`, [
      i16x8([0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x8000, 0x8000, 0x8000, 0x8000]),
    ]),
  [i32x4([0x7fff, 0x7fff, 0x7fff, 0x7fff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:276
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_s`, [
      i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0x7fff, 0x7fff, 0x7fff, 0x7fff]),
    ]),
  [i32x4([0xffff8000, 0xffff8000, 0xffff8000, 0xffff8000])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:278
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_s`, [
      i16x8([0x7fff, 0x7fff, 0x7fff, 0x7fff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0x7fff, 0x7fff, 0x7fff, 0x7fff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:280
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_s`, [
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0x7fff, 0x7fff, 0x7fff, 0x7fff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:282
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_s`, [
      i16x8([0x8001, 0x8001, 0x8001, 0x8001, 0x8000, 0x8000, 0x8000, 0x8000]),
    ]),
  [i32x4([0xffff8001, 0xffff8001, 0xffff8001, 0xffff8001])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:284
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_s`, [
      i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0x8001, 0x8001, 0x8001, 0x8001]),
    ]),
  [i32x4([0xffff8000, 0xffff8000, 0xffff8000, 0xffff8000])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:286
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_s`, [
      i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0xffff8000, 0xffff8000, 0xffff8000, 0xffff8000])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:288
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_s`, [
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0x8000, 0x8000, 0x8000, 0x8000]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:291
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_u`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:293
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_u`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:295
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_u`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:297
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_u`, [
      i16x8([0x1, 0x1, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:299
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_u`, [
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0xffff, 0xffff, 0xffff, 0xffff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:301
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_u`, [
      i16x8([0x1, 0x1, 0x1, 0x1, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:303
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_u`, [
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0xffff, 0xffff, 0xffff, 0xffff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:305
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_u`, [
      i16x8([0x7ffe, 0x7ffe, 0x7ffe, 0x7ffe, 0x7fff, 0x7fff, 0x7fff, 0x7fff]),
    ]),
  [i32x4([0x7ffe, 0x7ffe, 0x7ffe, 0x7ffe])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:307
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_u`, [
      i16x8([0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7ffe, 0x7ffe, 0x7ffe, 0x7ffe]),
    ]),
  [i32x4([0x7fff, 0x7fff, 0x7fff, 0x7fff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:309
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_u`, [
      i16x8([0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff]),
    ]),
  [i32x4([0x7fff, 0x7fff, 0x7fff, 0x7fff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:311
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_u`, [
      i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000]),
    ]),
  [i32x4([0x8000, 0x8000, 0x8000, 0x8000])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:313
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_u`, [
      i16x8([0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x8000, 0x8000, 0x8000, 0x8000]),
    ]),
  [i32x4([0x7fff, 0x7fff, 0x7fff, 0x7fff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:315
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_u`, [
      i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0x7fff, 0x7fff, 0x7fff, 0x7fff]),
    ]),
  [i32x4([0x8000, 0x8000, 0x8000, 0x8000])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:317
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_u`, [
      i16x8([0x7fff, 0x7fff, 0x7fff, 0x7fff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0x7fff, 0x7fff, 0x7fff, 0x7fff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:319
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_u`, [
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0x7fff, 0x7fff, 0x7fff, 0x7fff]),
    ]),
  [i32x4([0xffff, 0xffff, 0xffff, 0xffff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:321
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_u`, [
      i16x8([0x8001, 0x8001, 0x8001, 0x8001, 0x8000, 0x8000, 0x8000, 0x8000]),
    ]),
  [i32x4([0x8001, 0x8001, 0x8001, 0x8001])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:323
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_u`, [
      i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0x8001, 0x8001, 0x8001, 0x8001]),
    ]),
  [i32x4([0x8000, 0x8000, 0x8000, 0x8000])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:325
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_u`, [
      i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0x8000, 0x8000, 0x8000, 0x8000])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:327
assert_return(
  () =>
    invoke($0, `i32x4.extend_low_i16x8_u`, [
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0x8000, 0x8000, 0x8000, 0x8000]),
    ]),
  [i32x4([0xffff, 0xffff, 0xffff, 0xffff])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:330
assert_return(
  () => invoke($0, `i64x2.extend_high_i32x4_s`, [i32x4([0x0, 0x0, 0x0, 0x0])]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:332
assert_return(
  () => invoke($0, `i64x2.extend_high_i32x4_s`, [i32x4([0x0, 0x0, 0x1, 0x1])]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:334
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_s`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:336
assert_return(
  () => invoke($0, `i64x2.extend_high_i32x4_s`, [i32x4([0x1, 0x1, 0x0, 0x0])]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:338
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_s`, [
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:340
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_s`, [
      i32x4([0x1, 0x1, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:342
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_s`, [
      i32x4([0xffffffff, 0xffffffff, 0x1, 0x1]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:344
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_s`, [
      i32x4([0x7ffffffe, 0x7ffffffe, 0x7fffffff, 0x7fffffff]),
    ]),
  [i64x2([0x7fffffffn, 0x7fffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:346
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_s`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7ffffffe, 0x7ffffffe]),
    ]),
  [i64x2([0x7ffffffen, 0x7ffffffen])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:348
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_s`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
    ]),
  [i64x2([0x7fffffffn, 0x7fffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:350
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_s`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i64x2([0xffffffff80000000n, 0xffffffff80000000n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:352
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_s`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x80000000, 0x80000000]),
    ]),
  [i64x2([0xffffffff80000000n, 0xffffffff80000000n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:354
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_s`, [
      i32x4([0x80000000, 0x80000000, 0x7fffffff, 0x7fffffff]),
    ]),
  [i64x2([0x7fffffffn, 0x7fffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:356
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_s`, [
      i32x4([0x7fffffff, 0x7fffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:358
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_s`, [
      i32x4([0xffffffff, 0xffffffff, 0x7fffffff, 0x7fffffff]),
    ]),
  [i64x2([0x7fffffffn, 0x7fffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:360
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_s`, [
      i32x4([0x80000001, 0x80000001, 0x80000000, 0x80000000]),
    ]),
  [i64x2([0xffffffff80000000n, 0xffffffff80000000n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:362
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_s`, [
      i32x4([0x80000000, 0x80000000, 0x80000001, 0x80000001]),
    ]),
  [i64x2([0xffffffff80000001n, 0xffffffff80000001n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:364
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_s`, [
      i32x4([0x80000000, 0x80000000, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:366
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_s`, [
      i32x4([0xffffffff, 0xffffffff, 0x80000000, 0x80000000]),
    ]),
  [i64x2([0xffffffff80000000n, 0xffffffff80000000n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:369
assert_return(
  () => invoke($0, `i64x2.extend_high_i32x4_u`, [i32x4([0x0, 0x0, 0x0, 0x0])]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:371
assert_return(
  () => invoke($0, `i64x2.extend_high_i32x4_u`, [i32x4([0x0, 0x0, 0x1, 0x1])]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:373
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_u`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0xffffffffn, 0xffffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:375
assert_return(
  () => invoke($0, `i64x2.extend_high_i32x4_u`, [i32x4([0x1, 0x1, 0x0, 0x0])]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:377
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_u`, [
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:379
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_u`, [
      i32x4([0x1, 0x1, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0xffffffffn, 0xffffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:381
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_u`, [
      i32x4([0xffffffff, 0xffffffff, 0x1, 0x1]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:383
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_u`, [
      i32x4([0x7ffffffe, 0x7ffffffe, 0x7fffffff, 0x7fffffff]),
    ]),
  [i64x2([0x7fffffffn, 0x7fffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:385
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_u`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7ffffffe, 0x7ffffffe]),
    ]),
  [i64x2([0x7ffffffen, 0x7ffffffen])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:387
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_u`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
    ]),
  [i64x2([0x7fffffffn, 0x7fffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:389
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_u`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i64x2([0x80000000n, 0x80000000n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:391
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_u`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x80000000, 0x80000000]),
    ]),
  [i64x2([0x80000000n, 0x80000000n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:393
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_u`, [
      i32x4([0x80000000, 0x80000000, 0x7fffffff, 0x7fffffff]),
    ]),
  [i64x2([0x7fffffffn, 0x7fffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:395
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_u`, [
      i32x4([0x7fffffff, 0x7fffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0xffffffffn, 0xffffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:397
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_u`, [
      i32x4([0xffffffff, 0xffffffff, 0x7fffffff, 0x7fffffff]),
    ]),
  [i64x2([0x7fffffffn, 0x7fffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:399
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_u`, [
      i32x4([0x80000001, 0x80000001, 0x80000000, 0x80000000]),
    ]),
  [i64x2([0x80000000n, 0x80000000n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:401
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_u`, [
      i32x4([0x80000000, 0x80000000, 0x80000001, 0x80000001]),
    ]),
  [i64x2([0x80000001n, 0x80000001n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:403
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_u`, [
      i32x4([0x80000000, 0x80000000, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0xffffffffn, 0xffffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:405
assert_return(
  () =>
    invoke($0, `i64x2.extend_high_i32x4_u`, [
      i32x4([0xffffffff, 0xffffffff, 0x80000000, 0x80000000]),
    ]),
  [i64x2([0x80000000n, 0x80000000n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:408
assert_return(
  () => invoke($0, `i64x2.extend_low_i32x4_s`, [i32x4([0x0, 0x0, 0x0, 0x0])]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:410
assert_return(
  () => invoke($0, `i64x2.extend_low_i32x4_s`, [i32x4([0x0, 0x0, 0x1, 0x1])]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:412
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_s`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:414
assert_return(
  () => invoke($0, `i64x2.extend_low_i32x4_s`, [i32x4([0x1, 0x1, 0x0, 0x0])]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:416
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_s`, [
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:418
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_s`, [
      i32x4([0x1, 0x1, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:420
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_s`, [
      i32x4([0xffffffff, 0xffffffff, 0x1, 0x1]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:422
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_s`, [
      i32x4([0x7ffffffe, 0x7ffffffe, 0x7fffffff, 0x7fffffff]),
    ]),
  [i64x2([0x7ffffffen, 0x7ffffffen])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:424
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_s`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7ffffffe, 0x7ffffffe]),
    ]),
  [i64x2([0x7fffffffn, 0x7fffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:426
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_s`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
    ]),
  [i64x2([0x7fffffffn, 0x7fffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:428
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_s`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i64x2([0xffffffff80000000n, 0xffffffff80000000n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:430
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_s`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x80000000, 0x80000000]),
    ]),
  [i64x2([0x7fffffffn, 0x7fffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:432
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_s`, [
      i32x4([0x80000000, 0x80000000, 0x7fffffff, 0x7fffffff]),
    ]),
  [i64x2([0xffffffff80000000n, 0xffffffff80000000n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:434
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_s`, [
      i32x4([0x7fffffff, 0x7fffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x7fffffffn, 0x7fffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:436
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_s`, [
      i32x4([0xffffffff, 0xffffffff, 0x7fffffff, 0x7fffffff]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:438
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_s`, [
      i32x4([0x80000001, 0x80000001, 0x80000000, 0x80000000]),
    ]),
  [i64x2([0xffffffff80000001n, 0xffffffff80000001n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:440
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_s`, [
      i32x4([0x80000000, 0x80000000, 0x80000001, 0x80000001]),
    ]),
  [i64x2([0xffffffff80000000n, 0xffffffff80000000n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:442
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_s`, [
      i32x4([0x80000000, 0x80000000, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0xffffffff80000000n, 0xffffffff80000000n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:444
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_s`, [
      i32x4([0xffffffff, 0xffffffff, 0x80000000, 0x80000000]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:447
assert_return(
  () => invoke($0, `i64x2.extend_low_i32x4_u`, [i32x4([0x0, 0x0, 0x0, 0x0])]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:449
assert_return(
  () => invoke($0, `i64x2.extend_low_i32x4_u`, [i32x4([0x0, 0x0, 0x1, 0x1])]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:451
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_u`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:453
assert_return(
  () => invoke($0, `i64x2.extend_low_i32x4_u`, [i32x4([0x1, 0x1, 0x0, 0x0])]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:455
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_u`, [
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i64x2([0xffffffffn, 0xffffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:457
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_u`, [
      i32x4([0x1, 0x1, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:459
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_u`, [
      i32x4([0xffffffff, 0xffffffff, 0x1, 0x1]),
    ]),
  [i64x2([0xffffffffn, 0xffffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:461
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_u`, [
      i32x4([0x7ffffffe, 0x7ffffffe, 0x7fffffff, 0x7fffffff]),
    ]),
  [i64x2([0x7ffffffen, 0x7ffffffen])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:463
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_u`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7ffffffe, 0x7ffffffe]),
    ]),
  [i64x2([0x7fffffffn, 0x7fffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:465
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_u`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
    ]),
  [i64x2([0x7fffffffn, 0x7fffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:467
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_u`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i64x2([0x80000000n, 0x80000000n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:469
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_u`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x80000000, 0x80000000]),
    ]),
  [i64x2([0x7fffffffn, 0x7fffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:471
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_u`, [
      i32x4([0x80000000, 0x80000000, 0x7fffffff, 0x7fffffff]),
    ]),
  [i64x2([0x80000000n, 0x80000000n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:473
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_u`, [
      i32x4([0x7fffffff, 0x7fffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x7fffffffn, 0x7fffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:475
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_u`, [
      i32x4([0xffffffff, 0xffffffff, 0x7fffffff, 0x7fffffff]),
    ]),
  [i64x2([0xffffffffn, 0xffffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:477
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_u`, [
      i32x4([0x80000001, 0x80000001, 0x80000000, 0x80000000]),
    ]),
  [i64x2([0x80000001n, 0x80000001n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:479
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_u`, [
      i32x4([0x80000000, 0x80000000, 0x80000001, 0x80000001]),
    ]),
  [i64x2([0x80000000n, 0x80000000n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:481
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_u`, [
      i32x4([0x80000000, 0x80000000, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x80000000n, 0x80000000n])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:483
assert_return(
  () =>
    invoke($0, `i64x2.extend_low_i32x4_u`, [
      i32x4([0xffffffff, 0xffffffff, 0x80000000, 0x80000000]),
    ]),
  [i64x2([0xffffffffn, 0xffffffffn])],
);

// ./test/core/simd/simd_int_to_int_extend.wast:488
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i16x8.extend_high_i8x16_s (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_int_to_int_extend.wast:489
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i16x8.extend_high_i8x16_u (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_int_to_int_extend.wast:490
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i16x8.extend_low_i8x16_s (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_int_to_int_extend.wast:491
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i16x8.extend_low_i8x16_u (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_int_to_int_extend.wast:492
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.extend_high_i16x8_s (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_int_to_int_extend.wast:493
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.extend_high_i16x8_u (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_int_to_int_extend.wast:494
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.extend_low_i16x8_s (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_int_to_int_extend.wast:495
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.extend_low_i16x8_u (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_int_to_int_extend.wast:496
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i64x2.extend_high_i32x4_s (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_int_to_int_extend.wast:497
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i64x2.extend_high_i32x4_u (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_int_to_int_extend.wast:498
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i64x2.extend_low_i32x4_s (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_int_to_int_extend.wast:499
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i64x2.extend_low_i32x4_u (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_int_to_int_extend.wast:503
assert_invalid(() =>
  instantiate(`(module
    (func $$i16x8.extend_high_i8x16_s-arg-empty (result v128)
      (i16x8.extend_high_i8x16_s)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_int_to_int_extend.wast:511
assert_invalid(() =>
  instantiate(`(module
    (func $$i16x8.extend_high_i8x16_u-arg-empty (result v128)
      (i16x8.extend_high_i8x16_u)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_int_to_int_extend.wast:519
assert_invalid(() =>
  instantiate(`(module
    (func $$i16x8.extend_low_i8x16_s-arg-empty (result v128)
      (i16x8.extend_low_i8x16_s)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_int_to_int_extend.wast:527
assert_invalid(() =>
  instantiate(`(module
    (func $$i16x8.extend_low_i8x16_u-arg-empty (result v128)
      (i16x8.extend_low_i8x16_u)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_int_to_int_extend.wast:535
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.extend_high_i16x8_s-arg-empty (result v128)
      (i32x4.extend_high_i16x8_s)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_int_to_int_extend.wast:543
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.extend_high_i16x8_u-arg-empty (result v128)
      (i32x4.extend_high_i16x8_u)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_int_to_int_extend.wast:551
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.extend_low_i16x8_s-arg-empty (result v128)
      (i32x4.extend_low_i16x8_s)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_int_to_int_extend.wast:559
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.extend_low_i16x8_u-arg-empty (result v128)
      (i32x4.extend_low_i16x8_u)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_int_to_int_extend.wast:567
assert_invalid(() =>
  instantiate(`(module
    (func $$i64x2.extend_high_i32x4_s-arg-empty (result v128)
      (i64x2.extend_high_i32x4_s)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_int_to_int_extend.wast:575
assert_invalid(() =>
  instantiate(`(module
    (func $$i64x2.extend_high_i32x4_u-arg-empty (result v128)
      (i64x2.extend_high_i32x4_u)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_int_to_int_extend.wast:583
assert_invalid(() =>
  instantiate(`(module
    (func $$i64x2.extend_low_i32x4_s-arg-empty (result v128)
      (i64x2.extend_low_i32x4_s)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_int_to_int_extend.wast:591
assert_invalid(() =>
  instantiate(`(module
    (func $$i64x2.extend_low_i32x4_u-arg-empty (result v128)
      (i64x2.extend_low_i32x4_u)
    )
  )`), `type mismatch`);
