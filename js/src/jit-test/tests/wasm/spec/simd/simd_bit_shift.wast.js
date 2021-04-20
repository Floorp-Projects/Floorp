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

// ./test/core/simd/simd_bit_shift.wast

// ./test/core/simd/simd_bit_shift.wast:3
let $0 = instantiate(`(module
  (func (export "i8x16.shl") (param $$0 v128) (param $$1 i32) (result v128) (i8x16.shl (local.get $$0) (local.get $$1)))
  (func (export "i8x16.shr_s") (param $$0 v128) (param $$1 i32) (result v128) (i8x16.shr_s (local.get $$0) (local.get $$1)))
  (func (export "i8x16.shr_u") (param $$0 v128) (param $$1 i32) (result v128) (i8x16.shr_u (local.get $$0) (local.get $$1)))

  (func (export "i16x8.shl") (param $$0 v128) (param $$1 i32) (result v128) (i16x8.shl (local.get $$0) (local.get $$1)))
  (func (export "i16x8.shr_s") (param $$0 v128) (param $$1 i32) (result v128) (i16x8.shr_s (local.get $$0) (local.get $$1)))
  (func (export "i16x8.shr_u") (param $$0 v128) (param $$1 i32) (result v128) (i16x8.shr_u (local.get $$0) (local.get $$1)))

  (func (export "i32x4.shl") (param $$0 v128) (param $$1 i32) (result v128) (i32x4.shl (local.get $$0) (local.get $$1)))
  (func (export "i32x4.shr_s") (param $$0 v128) (param $$1 i32) (result v128) (i32x4.shr_s (local.get $$0) (local.get $$1)))
  (func (export "i32x4.shr_u") (param $$0 v128) (param $$1 i32) (result v128) (i32x4.shr_u (local.get $$0) (local.get $$1)))

  (func (export "i64x2.shl") (param $$0 v128) (param $$1 i32) (result v128) (i64x2.shl (local.get $$0) (local.get $$1)))
  (func (export "i64x2.shr_s") (param $$0 v128) (param $$1 i32) (result v128) (i64x2.shr_s (local.get $$0) (local.get $$1)))
  (func (export "i64x2.shr_u") (param $$0 v128) (param $$1 i32) (result v128) (i64x2.shr_u (local.get $$0) (local.get $$1)))

  ;; shifting by a constant amount
  ;; i8x16
  (func (export "i8x16.shl_1") (param $$0 v128) (result v128) (i8x16.shl (local.get $$0) (i32.const 1)))
  (func (export "i8x16.shr_u_8") (param $$0 v128) (result v128) (i8x16.shr_u (local.get $$0) (i32.const 8)))
  (func (export "i8x16.shr_s_9") (param $$0 v128) (result v128) (i8x16.shr_s (local.get $$0) (i32.const 9)))

  ;; i16x8
  (func (export "i16x8.shl_1") (param $$0 v128) (result v128) (i16x8.shl (local.get $$0) (i32.const 1)))
  (func (export "i16x8.shr_u_16") (param $$0 v128) (result v128) (i16x8.shr_u (local.get $$0) (i32.const 16)))
  (func (export "i16x8.shr_s_17") (param $$0 v128) (result v128) (i16x8.shr_s (local.get $$0) (i32.const 17)))

  ;; i32x4
  (func (export "i32x4.shl_1") (param $$0 v128) (result v128) (i32x4.shl (local.get $$0) (i32.const 1)))
  (func (export "i32x4.shr_u_32") (param $$0 v128) (result v128) (i32x4.shr_u (local.get $$0) (i32.const 32)))
  (func (export "i32x4.shr_s_33") (param $$0 v128) (result v128) (i32x4.shr_s (local.get $$0) (i32.const 33)))

  ;; i64x2
  (func (export "i64x2.shl_1") (param $$0 v128) (result v128) (i64x2.shl (local.get $$0) (i32.const 1)))
  (func (export "i64x2.shr_u_64") (param $$0 v128) (result v128) (i64x2.shr_u (local.get $$0) (i32.const 64)))
  (func (export "i64x2.shr_s_65") (param $$0 v128) (result v128) (i64x2.shr_s (local.get $$0) (i32.const 65)))
)`);

// ./test/core/simd/simd_bit_shift.wast:44
assert_return(
  () =>
    invoke($0, `i8x16.shl`, [
      i8x16([
        0x80,
        0xc0,
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
      ]),
      1,
    ]),
  [i8x16([
    0x0,
    0x80,
    0x0,
    0x2,
    0x4,
    0x6,
    0x8,
    0xa,
    0xc,
    0xe,
    0x10,
    0x12,
    0x14,
    0x16,
    0x18,
    0x1a,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:47
assert_return(
  () =>
    invoke($0, `i8x16.shl`, [
      i8x16([
        0xaa,
        0xbb,
        0xcc,
        0xdd,
        0xee,
        0xff,
        0xa0,
        0xb0,
        0xc0,
        0xd0,
        0xe0,
        0xf0,
        0xa,
        0xb,
        0xc,
        0xd,
      ]),
      4,
    ]),
  [i8x16([
    0xa0,
    0xb0,
    0xc0,
    0xd0,
    0xe0,
    0xf0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0xa0,
    0xb0,
    0xc0,
    0xd0,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:51
assert_return(
  () =>
    invoke($0, `i8x16.shl`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      8,
    ]),
  [i8x16([
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
    0xe,
    0xf,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:54
assert_return(
  () =>
    invoke($0, `i8x16.shl`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      32,
    ]),
  [i8x16([
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
    0xe,
    0xf,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:57
assert_return(
  () =>
    invoke($0, `i8x16.shl`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      128,
    ]),
  [i8x16([
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
    0xe,
    0xf,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:60
assert_return(
  () =>
    invoke($0, `i8x16.shl`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      256,
    ]),
  [i8x16([
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
    0xe,
    0xf,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:64
assert_return(
  () =>
    invoke($0, `i8x16.shl`, [
      i8x16([
        0x80,
        0xc0,
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
      ]),
      9,
    ]),
  [i8x16([
    0x0,
    0x80,
    0x0,
    0x2,
    0x4,
    0x6,
    0x8,
    0xa,
    0xc,
    0xe,
    0x10,
    0x12,
    0x14,
    0x16,
    0x18,
    0x1a,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:67
assert_return(
  () =>
    invoke($0, `i8x16.shl`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      9,
    ]),
  [i8x16([
    0x0,
    0x2,
    0x4,
    0x6,
    0x8,
    0xa,
    0xc,
    0xe,
    0x10,
    0x12,
    0x14,
    0x16,
    0x18,
    0x1a,
    0x1c,
    0x1e,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:70
assert_return(
  () =>
    invoke($0, `i8x16.shl`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      17,
    ]),
  [i8x16([
    0x0,
    0x2,
    0x4,
    0x6,
    0x8,
    0xa,
    0xc,
    0xe,
    0x10,
    0x12,
    0x14,
    0x16,
    0x18,
    0x1a,
    0x1c,
    0x1e,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:73
assert_return(
  () =>
    invoke($0, `i8x16.shl`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      33,
    ]),
  [i8x16([
    0x0,
    0x2,
    0x4,
    0x6,
    0x8,
    0xa,
    0xc,
    0xe,
    0x10,
    0x12,
    0x14,
    0x16,
    0x18,
    0x1a,
    0x1c,
    0x1e,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:76
assert_return(
  () =>
    invoke($0, `i8x16.shl`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      129,
    ]),
  [i8x16([
    0x0,
    0x2,
    0x4,
    0x6,
    0x8,
    0xa,
    0xc,
    0xe,
    0x10,
    0x12,
    0x14,
    0x16,
    0x18,
    0x1a,
    0x1c,
    0x1e,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:79
assert_return(
  () =>
    invoke($0, `i8x16.shl`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      257,
    ]),
  [i8x16([
    0x0,
    0x2,
    0x4,
    0x6,
    0x8,
    0xa,
    0xc,
    0xe,
    0x10,
    0x12,
    0x14,
    0x16,
    0x18,
    0x1a,
    0x1c,
    0x1e,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:82
assert_return(
  () =>
    invoke($0, `i8x16.shl`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      513,
    ]),
  [i8x16([
    0x0,
    0x2,
    0x4,
    0x6,
    0x8,
    0xa,
    0xc,
    0xe,
    0x10,
    0x12,
    0x14,
    0x16,
    0x18,
    0x1a,
    0x1c,
    0x1e,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:85
assert_return(
  () =>
    invoke($0, `i8x16.shl`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      514,
    ]),
  [i8x16([
    0x0,
    0x4,
    0x8,
    0xc,
    0x10,
    0x14,
    0x18,
    0x1c,
    0x20,
    0x24,
    0x28,
    0x2c,
    0x30,
    0x34,
    0x38,
    0x3c,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:90
assert_return(
  () =>
    invoke($0, `i8x16.shr_u`, [
      i8x16([
        0x80,
        0xc0,
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
      ]),
      1,
    ]),
  [i8x16([
    0x40,
    0x60,
    0x0,
    0x0,
    0x1,
    0x1,
    0x2,
    0x2,
    0x3,
    0x3,
    0x4,
    0x4,
    0x5,
    0x5,
    0x6,
    0x6,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:93
assert_return(
  () =>
    invoke($0, `i8x16.shr_u`, [
      i8x16([
        0xaa,
        0xbb,
        0xcc,
        0xdd,
        0xee,
        0xff,
        0xa0,
        0xb0,
        0xc0,
        0xd0,
        0xe0,
        0xf0,
        0xa,
        0xb,
        0xc,
        0xd,
      ]),
      4,
    ]),
  [i8x16([
    0xa,
    0xb,
    0xc,
    0xd,
    0xe,
    0xf,
    0xa,
    0xb,
    0xc,
    0xd,
    0xe,
    0xf,
    0x0,
    0x0,
    0x0,
    0x0,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:97
assert_return(
  () =>
    invoke($0, `i8x16.shr_u`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      8,
    ]),
  [i8x16([
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
    0xe,
    0xf,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:100
assert_return(
  () =>
    invoke($0, `i8x16.shr_u`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      32,
    ]),
  [i8x16([
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
    0xe,
    0xf,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:103
assert_return(
  () =>
    invoke($0, `i8x16.shr_u`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      128,
    ]),
  [i8x16([
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
    0xe,
    0xf,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:106
assert_return(
  () =>
    invoke($0, `i8x16.shr_u`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      256,
    ]),
  [i8x16([
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
    0xe,
    0xf,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:110
assert_return(
  () =>
    invoke($0, `i8x16.shr_u`, [
      i8x16([
        0x80,
        0xc0,
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
      ]),
      9,
    ]),
  [i8x16([
    0x40,
    0x60,
    0x0,
    0x0,
    0x1,
    0x1,
    0x2,
    0x2,
    0x3,
    0x3,
    0x4,
    0x4,
    0x5,
    0x5,
    0x6,
    0x6,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:113
assert_return(
  () =>
    invoke($0, `i8x16.shr_u`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      9,
    ]),
  [i8x16([
    0x0,
    0x0,
    0x1,
    0x1,
    0x2,
    0x2,
    0x3,
    0x3,
    0x4,
    0x4,
    0x5,
    0x5,
    0x6,
    0x6,
    0x7,
    0x7,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:116
assert_return(
  () =>
    invoke($0, `i8x16.shr_u`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      17,
    ]),
  [i8x16([
    0x0,
    0x0,
    0x1,
    0x1,
    0x2,
    0x2,
    0x3,
    0x3,
    0x4,
    0x4,
    0x5,
    0x5,
    0x6,
    0x6,
    0x7,
    0x7,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:119
assert_return(
  () =>
    invoke($0, `i8x16.shr_u`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      33,
    ]),
  [i8x16([
    0x0,
    0x0,
    0x1,
    0x1,
    0x2,
    0x2,
    0x3,
    0x3,
    0x4,
    0x4,
    0x5,
    0x5,
    0x6,
    0x6,
    0x7,
    0x7,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:122
assert_return(
  () =>
    invoke($0, `i8x16.shr_u`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      129,
    ]),
  [i8x16([
    0x0,
    0x0,
    0x1,
    0x1,
    0x2,
    0x2,
    0x3,
    0x3,
    0x4,
    0x4,
    0x5,
    0x5,
    0x6,
    0x6,
    0x7,
    0x7,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:125
assert_return(
  () =>
    invoke($0, `i8x16.shr_u`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      257,
    ]),
  [i8x16([
    0x0,
    0x0,
    0x1,
    0x1,
    0x2,
    0x2,
    0x3,
    0x3,
    0x4,
    0x4,
    0x5,
    0x5,
    0x6,
    0x6,
    0x7,
    0x7,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:128
assert_return(
  () =>
    invoke($0, `i8x16.shr_u`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      513,
    ]),
  [i8x16([
    0x0,
    0x0,
    0x1,
    0x1,
    0x2,
    0x2,
    0x3,
    0x3,
    0x4,
    0x4,
    0x5,
    0x5,
    0x6,
    0x6,
    0x7,
    0x7,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:131
assert_return(
  () =>
    invoke($0, `i8x16.shr_u`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      514,
    ]),
  [i8x16([
    0x0,
    0x0,
    0x0,
    0x0,
    0x1,
    0x1,
    0x1,
    0x1,
    0x2,
    0x2,
    0x2,
    0x2,
    0x3,
    0x3,
    0x3,
    0x3,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:136
assert_return(
  () =>
    invoke($0, `i8x16.shr_s`, [
      i8x16([
        0x80,
        0xc0,
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
      ]),
      1,
    ]),
  [i8x16([
    0xc0,
    0xe0,
    0x0,
    0x0,
    0x1,
    0x1,
    0x2,
    0x2,
    0x3,
    0x3,
    0x4,
    0x4,
    0x5,
    0x5,
    0x6,
    0x6,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:139
assert_return(
  () =>
    invoke($0, `i8x16.shr_s`, [
      i8x16([
        0xaa,
        0xbb,
        0xcc,
        0xdd,
        0xee,
        0xff,
        0xa0,
        0xb0,
        0xc0,
        0xd0,
        0xe0,
        0xf0,
        0xa,
        0xb,
        0xc,
        0xd,
      ]),
      4,
    ]),
  [i8x16([
    0xfa,
    0xfb,
    0xfc,
    0xfd,
    0xfe,
    0xff,
    0xfa,
    0xfb,
    0xfc,
    0xfd,
    0xfe,
    0xff,
    0x0,
    0x0,
    0x0,
    0x0,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:143
assert_return(
  () =>
    invoke($0, `i8x16.shr_s`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      8,
    ]),
  [i8x16([
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
    0xe,
    0xf,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:146
assert_return(
  () =>
    invoke($0, `i8x16.shr_s`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      32,
    ]),
  [i8x16([
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
    0xe,
    0xf,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:149
assert_return(
  () =>
    invoke($0, `i8x16.shr_s`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      128,
    ]),
  [i8x16([
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
    0xe,
    0xf,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:152
assert_return(
  () =>
    invoke($0, `i8x16.shr_s`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      256,
    ]),
  [i8x16([
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
    0xe,
    0xf,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:156
assert_return(
  () =>
    invoke($0, `i8x16.shr_s`, [
      i8x16([
        0x80,
        0xc0,
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
      ]),
      9,
    ]),
  [i8x16([
    0xc0,
    0xe0,
    0x0,
    0x0,
    0x1,
    0x1,
    0x2,
    0x2,
    0x3,
    0x3,
    0x4,
    0x4,
    0x5,
    0x5,
    0x6,
    0x6,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:159
assert_return(
  () =>
    invoke($0, `i8x16.shr_s`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      9,
    ]),
  [i8x16([
    0x0,
    0x0,
    0x1,
    0x1,
    0x2,
    0x2,
    0x3,
    0x3,
    0x4,
    0x4,
    0x5,
    0x5,
    0x6,
    0x6,
    0x7,
    0x7,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:162
assert_return(
  () =>
    invoke($0, `i8x16.shr_s`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      17,
    ]),
  [i8x16([
    0x0,
    0x0,
    0x1,
    0x1,
    0x2,
    0x2,
    0x3,
    0x3,
    0x4,
    0x4,
    0x5,
    0x5,
    0x6,
    0x6,
    0x7,
    0x7,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:165
assert_return(
  () =>
    invoke($0, `i8x16.shr_s`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      33,
    ]),
  [i8x16([
    0x0,
    0x0,
    0x1,
    0x1,
    0x2,
    0x2,
    0x3,
    0x3,
    0x4,
    0x4,
    0x5,
    0x5,
    0x6,
    0x6,
    0x7,
    0x7,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:168
assert_return(
  () =>
    invoke($0, `i8x16.shr_s`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      129,
    ]),
  [i8x16([
    0x0,
    0x0,
    0x1,
    0x1,
    0x2,
    0x2,
    0x3,
    0x3,
    0x4,
    0x4,
    0x5,
    0x5,
    0x6,
    0x6,
    0x7,
    0x7,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:171
assert_return(
  () =>
    invoke($0, `i8x16.shr_s`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      257,
    ]),
  [i8x16([
    0x0,
    0x0,
    0x1,
    0x1,
    0x2,
    0x2,
    0x3,
    0x3,
    0x4,
    0x4,
    0x5,
    0x5,
    0x6,
    0x6,
    0x7,
    0x7,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:174
assert_return(
  () =>
    invoke($0, `i8x16.shr_s`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      513,
    ]),
  [i8x16([
    0x0,
    0x0,
    0x1,
    0x1,
    0x2,
    0x2,
    0x3,
    0x3,
    0x4,
    0x4,
    0x5,
    0x5,
    0x6,
    0x6,
    0x7,
    0x7,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:177
assert_return(
  () =>
    invoke($0, `i8x16.shr_s`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
      514,
    ]),
  [i8x16([
    0x0,
    0x0,
    0x0,
    0x0,
    0x1,
    0x1,
    0x1,
    0x1,
    0x2,
    0x2,
    0x2,
    0x2,
    0x3,
    0x3,
    0x3,
    0x3,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:181
assert_return(
  () =>
    invoke($0, `i8x16.shl_1`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
    ]),
  [i8x16([
    0x0,
    0x2,
    0x4,
    0x6,
    0x8,
    0xa,
    0xc,
    0xe,
    0x10,
    0x12,
    0x14,
    0x16,
    0x18,
    0x1a,
    0x1c,
    0x1e,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:183
assert_return(
  () =>
    invoke($0, `i8x16.shr_u_8`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
    ]),
  [i8x16([
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
    0xe,
    0xf,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:185
assert_return(
  () =>
    invoke($0, `i8x16.shr_s_9`, [
      i8x16([
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
        0xe,
        0xf,
      ]),
    ]),
  [i8x16([
    0x0,
    0x0,
    0x1,
    0x1,
    0x2,
    0x2,
    0x3,
    0x3,
    0x4,
    0x4,
    0x5,
    0x5,
    0x6,
    0x6,
    0x7,
    0x7,
  ])],
);

// ./test/core/simd/simd_bit_shift.wast:190
assert_return(
  () =>
    invoke($0, `i16x8.shl`, [
      i16x8([0xff80, 0xffc0, 0x0, 0x1, 0x2, 0x3, 0x4, 0x5]),
      1,
    ]),
  [i16x8([0xff00, 0xff80, 0x0, 0x2, 0x4, 0x6, 0x8, 0xa])],
);

// ./test/core/simd/simd_bit_shift.wast:193
assert_return(
  () =>
    invoke($0, `i16x8.shl`, [
      i16x8([0x3039, 0x3039, 0x3039, 0x3039, 0x3039, 0x3039, 0x3039, 0x3039]),
      2,
    ]),
  [i16x8([0xc0e4, 0xc0e4, 0xc0e4, 0xc0e4, 0xc0e4, 0xc0e4, 0xc0e4, 0xc0e4])],
);

// ./test/core/simd/simd_bit_shift.wast:196
assert_return(
  () =>
    invoke($0, `i16x8.shl`, [
      i16x8([0x1234, 0x1234, 0x1234, 0x1234, 0x1234, 0x1234, 0x1234, 0x1234]),
      2,
    ]),
  [i16x8([0x48d0, 0x48d0, 0x48d0, 0x48d0, 0x48d0, 0x48d0, 0x48d0, 0x48d0])],
);

// ./test/core/simd/simd_bit_shift.wast:199
assert_return(
  () =>
    invoke($0, `i16x8.shl`, [
      i16x8([0xaabb, 0xccdd, 0xeeff, 0xa0b0, 0xc0d0, 0xe0f0, 0xa0b, 0xc0d]),
      4,
    ]),
  [i16x8([0xabb0, 0xcdd0, 0xeff0, 0xb00, 0xd00, 0xf00, 0xa0b0, 0xc0d0])],
);

// ./test/core/simd/simd_bit_shift.wast:202
assert_return(
  () =>
    invoke($0, `i16x8.shl`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      8,
    ]),
  [i16x8([0x0, 0x100, 0x200, 0x300, 0x400, 0x500, 0x600, 0x700])],
);

// ./test/core/simd/simd_bit_shift.wast:206
assert_return(
  () =>
    invoke($0, `i16x8.shl`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      32,
    ]),
  [i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7])],
);

// ./test/core/simd/simd_bit_shift.wast:209
assert_return(
  () =>
    invoke($0, `i16x8.shl`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      128,
    ]),
  [i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7])],
);

// ./test/core/simd/simd_bit_shift.wast:212
assert_return(
  () =>
    invoke($0, `i16x8.shl`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      256,
    ]),
  [i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7])],
);

// ./test/core/simd/simd_bit_shift.wast:216
assert_return(
  () =>
    invoke($0, `i16x8.shl`, [
      i16x8([0xff80, 0xffc0, 0x0, 0x1, 0x2, 0x3, 0x4, 0x5]),
      17,
    ]),
  [i16x8([0xff00, 0xff80, 0x0, 0x2, 0x4, 0x6, 0x8, 0xa])],
);

// ./test/core/simd/simd_bit_shift.wast:219
assert_return(
  () =>
    invoke($0, `i16x8.shl`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      17,
    ]),
  [i16x8([0x0, 0x2, 0x4, 0x6, 0x8, 0xa, 0xc, 0xe])],
);

// ./test/core/simd/simd_bit_shift.wast:222
assert_return(
  () =>
    invoke($0, `i16x8.shl`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      33,
    ]),
  [i16x8([0x0, 0x2, 0x4, 0x6, 0x8, 0xa, 0xc, 0xe])],
);

// ./test/core/simd/simd_bit_shift.wast:225
assert_return(
  () =>
    invoke($0, `i16x8.shl`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      129,
    ]),
  [i16x8([0x0, 0x2, 0x4, 0x6, 0x8, 0xa, 0xc, 0xe])],
);

// ./test/core/simd/simd_bit_shift.wast:228
assert_return(
  () =>
    invoke($0, `i16x8.shl`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      257,
    ]),
  [i16x8([0x0, 0x2, 0x4, 0x6, 0x8, 0xa, 0xc, 0xe])],
);

// ./test/core/simd/simd_bit_shift.wast:231
assert_return(
  () =>
    invoke($0, `i16x8.shl`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      513,
    ]),
  [i16x8([0x0, 0x2, 0x4, 0x6, 0x8, 0xa, 0xc, 0xe])],
);

// ./test/core/simd/simd_bit_shift.wast:234
assert_return(
  () =>
    invoke($0, `i16x8.shl`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      514,
    ]),
  [i16x8([0x0, 0x4, 0x8, 0xc, 0x10, 0x14, 0x18, 0x1c])],
);

// ./test/core/simd/simd_bit_shift.wast:240
assert_return(
  () =>
    invoke($0, `i16x8.shr_u`, [
      i16x8([0xff80, 0xffc0, 0x0, 0x1, 0x2, 0x3, 0x4, 0x5]),
      1,
    ]),
  [i16x8([0x7fc0, 0x7fe0, 0x0, 0x0, 0x1, 0x1, 0x2, 0x2])],
);

// ./test/core/simd/simd_bit_shift.wast:243
assert_return(
  () =>
    invoke($0, `i16x8.shr_u`, [
      i16x8([0x3039, 0x3039, 0x3039, 0x3039, 0x3039, 0x3039, 0x3039, 0x3039]),
      2,
    ]),
  [i16x8([0xc0e, 0xc0e, 0xc0e, 0xc0e, 0xc0e, 0xc0e, 0xc0e, 0xc0e])],
);

// ./test/core/simd/simd_bit_shift.wast:246
assert_return(
  () =>
    invoke($0, `i16x8.shr_u`, [
      i16x8([0x90ab, 0x90ab, 0x90ab, 0x90ab, 0x90ab, 0x90ab, 0x90ab, 0x90ab]),
      2,
    ]),
  [i16x8([0x242a, 0x242a, 0x242a, 0x242a, 0x242a, 0x242a, 0x242a, 0x242a])],
);

// ./test/core/simd/simd_bit_shift.wast:249
assert_return(
  () =>
    invoke($0, `i16x8.shr_u`, [
      i16x8([0xaabb, 0xccdd, 0xeeff, 0xa0b0, 0xc0d0, 0xe0f0, 0xa0b, 0xc0d]),
      4,
    ]),
  [i16x8([0xaab, 0xccd, 0xeef, 0xa0b, 0xc0d, 0xe0f, 0xa0, 0xc0])],
);

// ./test/core/simd/simd_bit_shift.wast:252
assert_return(
  () =>
    invoke($0, `i16x8.shr_u`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      8,
    ]),
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_bit_shift.wast:256
assert_return(
  () =>
    invoke($0, `i16x8.shr_u`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      32,
    ]),
  [i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7])],
);

// ./test/core/simd/simd_bit_shift.wast:259
assert_return(
  () =>
    invoke($0, `i16x8.shr_u`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      128,
    ]),
  [i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7])],
);

// ./test/core/simd/simd_bit_shift.wast:262
assert_return(
  () =>
    invoke($0, `i16x8.shr_u`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      256,
    ]),
  [i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7])],
);

// ./test/core/simd/simd_bit_shift.wast:266
assert_return(
  () =>
    invoke($0, `i16x8.shr_u`, [
      i16x8([0xff80, 0xffc0, 0x0, 0x1, 0x2, 0x3, 0x4, 0x5]),
      17,
    ]),
  [i16x8([0x7fc0, 0x7fe0, 0x0, 0x0, 0x1, 0x1, 0x2, 0x2])],
);

// ./test/core/simd/simd_bit_shift.wast:269
assert_return(
  () =>
    invoke($0, `i16x8.shr_u`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      17,
    ]),
  [i16x8([0x0, 0x0, 0x1, 0x1, 0x2, 0x2, 0x3, 0x3])],
);

// ./test/core/simd/simd_bit_shift.wast:272
assert_return(
  () =>
    invoke($0, `i16x8.shr_u`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      33,
    ]),
  [i16x8([0x0, 0x0, 0x1, 0x1, 0x2, 0x2, 0x3, 0x3])],
);

// ./test/core/simd/simd_bit_shift.wast:275
assert_return(
  () =>
    invoke($0, `i16x8.shr_u`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      129,
    ]),
  [i16x8([0x0, 0x0, 0x1, 0x1, 0x2, 0x2, 0x3, 0x3])],
);

// ./test/core/simd/simd_bit_shift.wast:278
assert_return(
  () =>
    invoke($0, `i16x8.shr_u`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      257,
    ]),
  [i16x8([0x0, 0x0, 0x1, 0x1, 0x2, 0x2, 0x3, 0x3])],
);

// ./test/core/simd/simd_bit_shift.wast:281
assert_return(
  () =>
    invoke($0, `i16x8.shr_u`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      513,
    ]),
  [i16x8([0x0, 0x0, 0x1, 0x1, 0x2, 0x2, 0x3, 0x3])],
);

// ./test/core/simd/simd_bit_shift.wast:284
assert_return(
  () =>
    invoke($0, `i16x8.shr_u`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      514,
    ]),
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_bit_shift.wast:290
assert_return(
  () =>
    invoke($0, `i16x8.shr_s`, [
      i16x8([0xff80, 0xffc0, 0x0, 0x1, 0x2, 0x3, 0x4, 0x5]),
      1,
    ]),
  [i16x8([0xffc0, 0xffe0, 0x0, 0x0, 0x1, 0x1, 0x2, 0x2])],
);

// ./test/core/simd/simd_bit_shift.wast:293
assert_return(
  () =>
    invoke($0, `i16x8.shr_s`, [
      i16x8([0x3039, 0x3039, 0x3039, 0x3039, 0x3039, 0x3039, 0x3039, 0x3039]),
      2,
    ]),
  [i16x8([0xc0e, 0xc0e, 0xc0e, 0xc0e, 0xc0e, 0xc0e, 0xc0e, 0xc0e])],
);

// ./test/core/simd/simd_bit_shift.wast:296
assert_return(
  () =>
    invoke($0, `i16x8.shr_s`, [
      i16x8([0x90ab, 0x90ab, 0x90ab, 0x90ab, 0x90ab, 0x90ab, 0x90ab, 0x90ab]),
      2,
    ]),
  [i16x8([0xe42a, 0xe42a, 0xe42a, 0xe42a, 0xe42a, 0xe42a, 0xe42a, 0xe42a])],
);

// ./test/core/simd/simd_bit_shift.wast:299
assert_return(
  () =>
    invoke($0, `i16x8.shr_s`, [
      i16x8([0xaabb, 0xccdd, 0xeeff, 0xa0b0, 0xc0d0, 0xe0f0, 0xa0b, 0xc0d]),
      4,
    ]),
  [i16x8([0xfaab, 0xfccd, 0xfeef, 0xfa0b, 0xfc0d, 0xfe0f, 0xa0, 0xc0])],
);

// ./test/core/simd/simd_bit_shift.wast:302
assert_return(
  () =>
    invoke($0, `i16x8.shr_s`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      8,
    ]),
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_bit_shift.wast:306
assert_return(
  () =>
    invoke($0, `i16x8.shr_s`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      32,
    ]),
  [i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7])],
);

// ./test/core/simd/simd_bit_shift.wast:309
assert_return(
  () =>
    invoke($0, `i16x8.shr_s`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      128,
    ]),
  [i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7])],
);

// ./test/core/simd/simd_bit_shift.wast:312
assert_return(
  () =>
    invoke($0, `i16x8.shr_s`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      256,
    ]),
  [i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7])],
);

// ./test/core/simd/simd_bit_shift.wast:316
assert_return(
  () =>
    invoke($0, `i16x8.shr_s`, [
      i16x8([0xff80, 0xffc0, 0x0, 0x1, 0x2, 0x3, 0x4, 0x5]),
      17,
    ]),
  [i16x8([0xffc0, 0xffe0, 0x0, 0x0, 0x1, 0x1, 0x2, 0x2])],
);

// ./test/core/simd/simd_bit_shift.wast:319
assert_return(
  () =>
    invoke($0, `i16x8.shr_s`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      17,
    ]),
  [i16x8([0x0, 0x0, 0x1, 0x1, 0x2, 0x2, 0x3, 0x3])],
);

// ./test/core/simd/simd_bit_shift.wast:322
assert_return(
  () =>
    invoke($0, `i16x8.shr_s`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      33,
    ]),
  [i16x8([0x0, 0x0, 0x1, 0x1, 0x2, 0x2, 0x3, 0x3])],
);

// ./test/core/simd/simd_bit_shift.wast:325
assert_return(
  () =>
    invoke($0, `i16x8.shr_s`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      129,
    ]),
  [i16x8([0x0, 0x0, 0x1, 0x1, 0x2, 0x2, 0x3, 0x3])],
);

// ./test/core/simd/simd_bit_shift.wast:328
assert_return(
  () =>
    invoke($0, `i16x8.shr_s`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      257,
    ]),
  [i16x8([0x0, 0x0, 0x1, 0x1, 0x2, 0x2, 0x3, 0x3])],
);

// ./test/core/simd/simd_bit_shift.wast:331
assert_return(
  () =>
    invoke($0, `i16x8.shr_s`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      513,
    ]),
  [i16x8([0x0, 0x0, 0x1, 0x1, 0x2, 0x2, 0x3, 0x3])],
);

// ./test/core/simd/simd_bit_shift.wast:334
assert_return(
  () =>
    invoke($0, `i16x8.shr_s`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
      514,
    ]),
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_bit_shift.wast:339
assert_return(
  () =>
    invoke($0, `i16x8.shl_1`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
    ]),
  [i16x8([0x0, 0x2, 0x4, 0x6, 0x8, 0xa, 0xc, 0xe])],
);

// ./test/core/simd/simd_bit_shift.wast:341
assert_return(
  () =>
    invoke($0, `i16x8.shr_u_16`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
    ]),
  [i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7])],
);

// ./test/core/simd/simd_bit_shift.wast:343
assert_return(
  () =>
    invoke($0, `i16x8.shr_s_17`, [
      i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
    ]),
  [i16x8([0x0, 0x0, 0x1, 0x1, 0x2, 0x2, 0x3, 0x3])],
);

// ./test/core/simd/simd_bit_shift.wast:348
assert_return(
  () =>
    invoke($0, `i32x4.shl`, [
      i32x4([0x80000000, 0xffff8000, 0x0, 0xa0b0c0d]),
      1,
    ]),
  [i32x4([0x0, 0xffff0000, 0x0, 0x1416181a])],
);

// ./test/core/simd/simd_bit_shift.wast:351
assert_return(
  () =>
    invoke($0, `i32x4.shl`, [
      i32x4([0x499602d2, 0x499602d2, 0x499602d2, 0x499602d2]),
      2,
    ]),
  [i32x4([0x26580b48, 0x26580b48, 0x26580b48, 0x26580b48])],
);

// ./test/core/simd/simd_bit_shift.wast:354
assert_return(
  () =>
    invoke($0, `i32x4.shl`, [
      i32x4([0x12345678, 0x12345678, 0x12345678, 0x12345678]),
      2,
    ]),
  [i32x4([0x48d159e0, 0x48d159e0, 0x48d159e0, 0x48d159e0])],
);

// ./test/core/simd/simd_bit_shift.wast:357
assert_return(
  () =>
    invoke($0, `i32x4.shl`, [
      i32x4([0xaabbccdd, 0xeeffa0b0, 0xc0d0e0f0, 0xa0b0c0d]),
      4,
    ]),
  [i32x4([0xabbccdd0, 0xeffa0b00, 0xd0e0f00, 0xa0b0c0d0])],
);

// ./test/core/simd/simd_bit_shift.wast:360
assert_return(() => invoke($0, `i32x4.shl`, [i32x4([0x0, 0x1, 0xe, 0xf]), 8]), [
  i32x4([0x0, 0x100, 0xe00, 0xf00]),
]);

// ./test/core/simd/simd_bit_shift.wast:364
assert_return(
  () => invoke($0, `i32x4.shl`, [i32x4([0x0, 0x1, 0xe, 0xf]), 32]),
  [i32x4([0x0, 0x1, 0xe, 0xf])],
);

// ./test/core/simd/simd_bit_shift.wast:367
assert_return(
  () => invoke($0, `i32x4.shl`, [i32x4([0x0, 0x1, 0xe, 0xf]), 128]),
  [i32x4([0x0, 0x1, 0xe, 0xf])],
);

// ./test/core/simd/simd_bit_shift.wast:370
assert_return(
  () => invoke($0, `i32x4.shl`, [i32x4([0x0, 0x1, 0xe, 0xf]), 256]),
  [i32x4([0x0, 0x1, 0xe, 0xf])],
);

// ./test/core/simd/simd_bit_shift.wast:374
assert_return(
  () =>
    invoke($0, `i32x4.shl`, [
      i32x4([0x80000000, 0xffff8000, 0x0, 0xa0b0c0d]),
      33,
    ]),
  [i32x4([0x0, 0xffff0000, 0x0, 0x1416181a])],
);

// ./test/core/simd/simd_bit_shift.wast:377
assert_return(
  () => invoke($0, `i32x4.shl`, [i32x4([0x0, 0x1, 0xe, 0xf]), 33]),
  [i32x4([0x0, 0x2, 0x1c, 0x1e])],
);

// ./test/core/simd/simd_bit_shift.wast:380
assert_return(
  () => invoke($0, `i32x4.shl`, [i32x4([0x0, 0x1, 0xe, 0xf]), 65]),
  [i32x4([0x0, 0x2, 0x1c, 0x1e])],
);

// ./test/core/simd/simd_bit_shift.wast:383
assert_return(
  () => invoke($0, `i32x4.shl`, [i32x4([0x0, 0x1, 0xe, 0xf]), 129]),
  [i32x4([0x0, 0x2, 0x1c, 0x1e])],
);

// ./test/core/simd/simd_bit_shift.wast:386
assert_return(
  () => invoke($0, `i32x4.shl`, [i32x4([0x0, 0x1, 0xe, 0xf]), 257]),
  [i32x4([0x0, 0x2, 0x1c, 0x1e])],
);

// ./test/core/simd/simd_bit_shift.wast:389
assert_return(
  () => invoke($0, `i32x4.shl`, [i32x4([0x0, 0x1, 0xe, 0xf]), 513]),
  [i32x4([0x0, 0x2, 0x1c, 0x1e])],
);

// ./test/core/simd/simd_bit_shift.wast:392
assert_return(
  () => invoke($0, `i32x4.shl`, [i32x4([0x0, 0x1, 0xe, 0xf]), 514]),
  [i32x4([0x0, 0x4, 0x38, 0x3c])],
);

// ./test/core/simd/simd_bit_shift.wast:398
assert_return(
  () =>
    invoke($0, `i32x4.shr_u`, [i32x4([0x80000000, 0xffff8000, 0xc, 0xd]), 1]),
  [i32x4([0x40000000, 0x7fffc000, 0x6, 0x6])],
);

// ./test/core/simd/simd_bit_shift.wast:401
assert_return(
  () =>
    invoke($0, `i32x4.shr_u`, [
      i32x4([0x499602d2, 0x499602d2, 0x499602d2, 0x499602d2]),
      2,
    ]),
  [i32x4([0x126580b4, 0x126580b4, 0x126580b4, 0x126580b4])],
);

// ./test/core/simd/simd_bit_shift.wast:404
assert_return(
  () =>
    invoke($0, `i32x4.shr_u`, [
      i32x4([0x90abcdef, 0x90abcdef, 0x90abcdef, 0x90abcdef]),
      2,
    ]),
  [i32x4([0x242af37b, 0x242af37b, 0x242af37b, 0x242af37b])],
);

// ./test/core/simd/simd_bit_shift.wast:407
assert_return(
  () =>
    invoke($0, `i32x4.shr_u`, [
      i32x4([0xaabbccdd, 0xeeffa0b0, 0xc0d0e0f0, 0xa0b0c0d]),
      4,
    ]),
  [i32x4([0xaabbccd, 0xeeffa0b, 0xc0d0e0f, 0xa0b0c0])],
);

// ./test/core/simd/simd_bit_shift.wast:410
assert_return(
  () => invoke($0, `i32x4.shr_u`, [i32x4([0x0, 0x1, 0xe, 0xf]), 8]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_bit_shift.wast:414
assert_return(
  () => invoke($0, `i32x4.shr_u`, [i32x4([0x0, 0x1, 0xe, 0xf]), 32]),
  [i32x4([0x0, 0x1, 0xe, 0xf])],
);

// ./test/core/simd/simd_bit_shift.wast:417
assert_return(
  () => invoke($0, `i32x4.shr_u`, [i32x4([0x0, 0x1, 0xe, 0xf]), 128]),
  [i32x4([0x0, 0x1, 0xe, 0xf])],
);

// ./test/core/simd/simd_bit_shift.wast:420
assert_return(
  () => invoke($0, `i32x4.shr_u`, [i32x4([0x0, 0x1, 0xe, 0xf]), 256]),
  [i32x4([0x0, 0x1, 0xe, 0xf])],
);

// ./test/core/simd/simd_bit_shift.wast:424
assert_return(
  () =>
    invoke($0, `i32x4.shr_u`, [i32x4([0x80000000, 0xffff8000, 0xc, 0xd]), 33]),
  [i32x4([0x40000000, 0x7fffc000, 0x6, 0x6])],
);

// ./test/core/simd/simd_bit_shift.wast:427
assert_return(
  () => invoke($0, `i32x4.shr_u`, [i32x4([0x0, 0x1, 0xe, 0xf]), 33]),
  [i32x4([0x0, 0x0, 0x7, 0x7])],
);

// ./test/core/simd/simd_bit_shift.wast:430
assert_return(
  () => invoke($0, `i32x4.shr_u`, [i32x4([0x0, 0x1, 0xe, 0xf]), 65]),
  [i32x4([0x0, 0x0, 0x7, 0x7])],
);

// ./test/core/simd/simd_bit_shift.wast:433
assert_return(
  () => invoke($0, `i32x4.shr_u`, [i32x4([0x0, 0x1, 0xe, 0xf]), 129]),
  [i32x4([0x0, 0x0, 0x7, 0x7])],
);

// ./test/core/simd/simd_bit_shift.wast:436
assert_return(
  () => invoke($0, `i32x4.shr_u`, [i32x4([0x0, 0x1, 0xe, 0xf]), 257]),
  [i32x4([0x0, 0x0, 0x7, 0x7])],
);

// ./test/core/simd/simd_bit_shift.wast:439
assert_return(
  () => invoke($0, `i32x4.shr_u`, [i32x4([0x0, 0x1, 0xe, 0xf]), 513]),
  [i32x4([0x0, 0x0, 0x7, 0x7])],
);

// ./test/core/simd/simd_bit_shift.wast:442
assert_return(
  () => invoke($0, `i32x4.shr_u`, [i32x4([0x0, 0x1, 0xe, 0xf]), 514]),
  [i32x4([0x0, 0x0, 0x3, 0x3])],
);

// ./test/core/simd/simd_bit_shift.wast:448
assert_return(
  () =>
    invoke($0, `i32x4.shr_s`, [i32x4([0x80000000, 0xffff8000, 0xc, 0xd]), 1]),
  [i32x4([0xc0000000, 0xffffc000, 0x6, 0x6])],
);

// ./test/core/simd/simd_bit_shift.wast:451
assert_return(
  () =>
    invoke($0, `i32x4.shr_s`, [
      i32x4([0x499602d2, 0x499602d2, 0x499602d2, 0x499602d2]),
      2,
    ]),
  [i32x4([0x126580b4, 0x126580b4, 0x126580b4, 0x126580b4])],
);

// ./test/core/simd/simd_bit_shift.wast:454
assert_return(
  () =>
    invoke($0, `i32x4.shr_s`, [
      i32x4([0x90abcdef, 0x90abcdef, 0x90abcdef, 0x90abcdef]),
      2,
    ]),
  [i32x4([0xe42af37b, 0xe42af37b, 0xe42af37b, 0xe42af37b])],
);

// ./test/core/simd/simd_bit_shift.wast:457
assert_return(
  () =>
    invoke($0, `i32x4.shr_s`, [
      i32x4([0xaabbccdd, 0xeeffa0b0, 0xc0d0e0f0, 0xa0b0c0d]),
      4,
    ]),
  [i32x4([0xfaabbccd, 0xfeeffa0b, 0xfc0d0e0f, 0xa0b0c0])],
);

// ./test/core/simd/simd_bit_shift.wast:461
assert_return(
  () => invoke($0, `i32x4.shr_s`, [i32x4([0x0, 0x1, 0xe, 0xf]), 8]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_bit_shift.wast:464
assert_return(
  () => invoke($0, `i32x4.shr_s`, [i32x4([0x0, 0x1, 0xe, 0xf]), 32]),
  [i32x4([0x0, 0x1, 0xe, 0xf])],
);

// ./test/core/simd/simd_bit_shift.wast:467
assert_return(
  () => invoke($0, `i32x4.shr_s`, [i32x4([0x0, 0x1, 0xe, 0xf]), 128]),
  [i32x4([0x0, 0x1, 0xe, 0xf])],
);

// ./test/core/simd/simd_bit_shift.wast:470
assert_return(
  () => invoke($0, `i32x4.shr_s`, [i32x4([0x0, 0x1, 0xe, 0xf]), 256]),
  [i32x4([0x0, 0x1, 0xe, 0xf])],
);

// ./test/core/simd/simd_bit_shift.wast:474
assert_return(
  () =>
    invoke($0, `i32x4.shr_s`, [i32x4([0x80000000, 0xffff8000, 0xc, 0xd]), 33]),
  [i32x4([0xc0000000, 0xffffc000, 0x6, 0x6])],
);

// ./test/core/simd/simd_bit_shift.wast:477
assert_return(
  () => invoke($0, `i32x4.shr_s`, [i32x4([0x0, 0x1, 0xe, 0xf]), 33]),
  [i32x4([0x0, 0x0, 0x7, 0x7])],
);

// ./test/core/simd/simd_bit_shift.wast:480
assert_return(
  () => invoke($0, `i32x4.shr_s`, [i32x4([0x0, 0x1, 0xe, 0xf]), 65]),
  [i32x4([0x0, 0x0, 0x7, 0x7])],
);

// ./test/core/simd/simd_bit_shift.wast:483
assert_return(
  () => invoke($0, `i32x4.shr_s`, [i32x4([0x0, 0x1, 0xe, 0xf]), 129]),
  [i32x4([0x0, 0x0, 0x7, 0x7])],
);

// ./test/core/simd/simd_bit_shift.wast:486
assert_return(
  () => invoke($0, `i32x4.shr_s`, [i32x4([0x0, 0x1, 0xe, 0xf]), 257]),
  [i32x4([0x0, 0x0, 0x7, 0x7])],
);

// ./test/core/simd/simd_bit_shift.wast:489
assert_return(
  () => invoke($0, `i32x4.shr_s`, [i32x4([0x0, 0x1, 0xe, 0xf]), 513]),
  [i32x4([0x0, 0x0, 0x7, 0x7])],
);

// ./test/core/simd/simd_bit_shift.wast:492
assert_return(
  () => invoke($0, `i32x4.shr_s`, [i32x4([0x0, 0x1, 0xe, 0xf]), 514]),
  [i32x4([0x0, 0x0, 0x3, 0x3])],
);

// ./test/core/simd/simd_bit_shift.wast:497
assert_return(() => invoke($0, `i32x4.shl_1`, [i32x4([0x0, 0x1, 0xe, 0xf])]), [
  i32x4([0x0, 0x2, 0x1c, 0x1e]),
]);

// ./test/core/simd/simd_bit_shift.wast:499
assert_return(
  () => invoke($0, `i32x4.shr_u_32`, [i32x4([0x0, 0x1, 0xe, 0xf])]),
  [i32x4([0x0, 0x1, 0xe, 0xf])],
);

// ./test/core/simd/simd_bit_shift.wast:501
assert_return(
  () => invoke($0, `i32x4.shr_s_33`, [i32x4([0x0, 0x1, 0xe, 0xf])]),
  [i32x4([0x0, 0x0, 0x7, 0x7])],
);

// ./test/core/simd/simd_bit_shift.wast:506
assert_return(
  () =>
    invoke($0, `i64x2.shl`, [
      i64x2([0x8000000000000000n, 0xffffffff80000000n]),
      1,
    ]),
  [i64x2([0x0n, 0xffffffff00000000n])],
);

// ./test/core/simd/simd_bit_shift.wast:509
assert_return(
  () =>
    invoke($0, `i64x2.shl`, [
      i64x2([0x112210f47de98115n, 0x112210f47de98115n]),
      2,
    ]),
  [i64x2([0x448843d1f7a60454n, 0x448843d1f7a60454n])],
);

// ./test/core/simd/simd_bit_shift.wast:512
assert_return(
  () =>
    invoke($0, `i64x2.shl`, [
      i64x2([0x1234567890abcdefn, 0x1234567890abcdefn]),
      2,
    ]),
  [i64x2([0x48d159e242af37bcn, 0x48d159e242af37bcn])],
);

// ./test/core/simd/simd_bit_shift.wast:515
assert_return(
  () =>
    invoke($0, `i64x2.shl`, [
      i64x2([0xaabbccddeeffa0b0n, 0xc0d0e0f00a0b0c0dn]),
      4,
    ]),
  [i64x2([0xabbccddeeffa0b00n, 0xd0e0f00a0b0c0d0n])],
);

// ./test/core/simd/simd_bit_shift.wast:518
assert_return(
  () =>
    invoke($0, `i64x2.shl`, [
      i64x2([0xaabbccddeeffa0b0n, 0xc0d0e0f00a0b0c0dn]),
      8,
    ]),
  [i64x2([0xbbccddeeffa0b000n, 0xd0e0f00a0b0c0d00n])],
);

// ./test/core/simd/simd_bit_shift.wast:521
assert_return(() => invoke($0, `i64x2.shl`, [i64x2([0x1n, 0xfn]), 16]), [
  i64x2([0x10000n, 0xf0000n]),
]);

// ./test/core/simd/simd_bit_shift.wast:524
assert_return(() => invoke($0, `i64x2.shl`, [i64x2([0x1n, 0xfn]), 32]), [
  i64x2([0x100000000n, 0xf00000000n]),
]);

// ./test/core/simd/simd_bit_shift.wast:528
assert_return(() => invoke($0, `i64x2.shl`, [i64x2([0x1n, 0xfn]), 128]), [
  i64x2([0x1n, 0xfn]),
]);

// ./test/core/simd/simd_bit_shift.wast:531
assert_return(() => invoke($0, `i64x2.shl`, [i64x2([0x1n, 0xfn]), 256]), [
  i64x2([0x1n, 0xfn]),
]);

// ./test/core/simd/simd_bit_shift.wast:535
assert_return(() => invoke($0, `i64x2.shl`, [i64x2([0x1n, 0xfn]), 65]), [
  i64x2([0x2n, 0x1en]),
]);

// ./test/core/simd/simd_bit_shift.wast:538
assert_return(() => invoke($0, `i64x2.shl`, [i64x2([0x1n, 0xfn]), 129]), [
  i64x2([0x2n, 0x1en]),
]);

// ./test/core/simd/simd_bit_shift.wast:541
assert_return(() => invoke($0, `i64x2.shl`, [i64x2([0x1n, 0xfn]), 257]), [
  i64x2([0x2n, 0x1en]),
]);

// ./test/core/simd/simd_bit_shift.wast:544
assert_return(() => invoke($0, `i64x2.shl`, [i64x2([0x1n, 0xfn]), 513]), [
  i64x2([0x2n, 0x1en]),
]);

// ./test/core/simd/simd_bit_shift.wast:547
assert_return(() => invoke($0, `i64x2.shl`, [i64x2([0x1n, 0xfn]), 514]), [
  i64x2([0x4n, 0x3cn]),
]);

// ./test/core/simd/simd_bit_shift.wast:553
assert_return(
  () =>
    invoke($0, `i64x2.shr_u`, [
      i64x2([0x8000000000000000n, 0xffffffff80000000n]),
      1,
    ]),
  [i64x2([0x4000000000000000n, 0x7fffffffc0000000n])],
);

// ./test/core/simd/simd_bit_shift.wast:556
assert_return(
  () =>
    invoke($0, `i64x2.shr_u`, [
      i64x2([0x112210f47de98115n, 0x112210f47de98115n]),
      2,
    ]),
  [i64x2([0x448843d1f7a6045n, 0x448843d1f7a6045n])],
);

// ./test/core/simd/simd_bit_shift.wast:559
assert_return(
  () =>
    invoke($0, `i64x2.shr_u`, [
      i64x2([0x90abcdef87654321n, 0x90abcdef87654321n]),
      2,
    ]),
  [i64x2([0x242af37be1d950c8n, 0x242af37be1d950c8n])],
);

// ./test/core/simd/simd_bit_shift.wast:562
assert_return(
  () =>
    invoke($0, `i64x2.shr_u`, [
      i64x2([0xaabbccddeeffa0b0n, 0xc0d0e0f00a0b0c0dn]),
      4,
    ]),
  [i64x2([0xaabbccddeeffa0bn, 0xc0d0e0f00a0b0c0n])],
);

// ./test/core/simd/simd_bit_shift.wast:565
assert_return(
  () =>
    invoke($0, `i64x2.shr_u`, [
      i64x2([0xaabbccddeeffa0b0n, 0xc0d0e0f00a0b0c0dn]),
      8,
    ]),
  [i64x2([0xaabbccddeeffa0n, 0xc0d0e0f00a0b0cn])],
);

// ./test/core/simd/simd_bit_shift.wast:568
assert_return(() => invoke($0, `i64x2.shr_u`, [i64x2([0x1n, 0xfn]), 16]), [
  i64x2([0x0n, 0x0n]),
]);

// ./test/core/simd/simd_bit_shift.wast:571
assert_return(() => invoke($0, `i64x2.shr_u`, [i64x2([0x1n, 0xfn]), 32]), [
  i64x2([0x0n, 0x0n]),
]);

// ./test/core/simd/simd_bit_shift.wast:575
assert_return(() => invoke($0, `i64x2.shr_u`, [i64x2([0x1n, 0xfn]), 128]), [
  i64x2([0x1n, 0xfn]),
]);

// ./test/core/simd/simd_bit_shift.wast:578
assert_return(() => invoke($0, `i64x2.shr_u`, [i64x2([0x1n, 0xfn]), 256]), [
  i64x2([0x1n, 0xfn]),
]);

// ./test/core/simd/simd_bit_shift.wast:582
assert_return(() => invoke($0, `i64x2.shr_u`, [i64x2([0x1n, 0xfn]), 65]), [
  i64x2([0x0n, 0x7n]),
]);

// ./test/core/simd/simd_bit_shift.wast:585
assert_return(() => invoke($0, `i64x2.shr_u`, [i64x2([0x1n, 0xfn]), 129]), [
  i64x2([0x0n, 0x7n]),
]);

// ./test/core/simd/simd_bit_shift.wast:588
assert_return(() => invoke($0, `i64x2.shr_u`, [i64x2([0x1n, 0xfn]), 257]), [
  i64x2([0x0n, 0x7n]),
]);

// ./test/core/simd/simd_bit_shift.wast:591
assert_return(() => invoke($0, `i64x2.shr_u`, [i64x2([0x1n, 0xfn]), 513]), [
  i64x2([0x0n, 0x7n]),
]);

// ./test/core/simd/simd_bit_shift.wast:594
assert_return(() => invoke($0, `i64x2.shr_u`, [i64x2([0x0n, 0xfn]), 514]), [
  i64x2([0x0n, 0x3n]),
]);

// ./test/core/simd/simd_bit_shift.wast:600
assert_return(
  () =>
    invoke($0, `i64x2.shr_s`, [
      i64x2([0x8000000000000000n, 0xffffffff80000000n]),
      1,
    ]),
  [i64x2([0xc000000000000000n, 0xffffffffc0000000n])],
);

// ./test/core/simd/simd_bit_shift.wast:603
assert_return(
  () =>
    invoke($0, `i64x2.shr_s`, [
      i64x2([0x112210f47de98115n, 0x112210f47de98115n]),
      2,
    ]),
  [i64x2([0x448843d1f7a6045n, 0x448843d1f7a6045n])],
);

// ./test/core/simd/simd_bit_shift.wast:606
assert_return(
  () =>
    invoke($0, `i64x2.shr_s`, [
      i64x2([0x90abcdef87654321n, 0x90abcdef87654321n]),
      2,
    ]),
  [i64x2([0xe42af37be1d950c8n, 0xe42af37be1d950c8n])],
);

// ./test/core/simd/simd_bit_shift.wast:609
assert_return(
  () =>
    invoke($0, `i64x2.shr_s`, [
      i64x2([0xaabbccddeeffa0b0n, 0xc0d0e0f00a0b0c0dn]),
      4,
    ]),
  [i64x2([0xfaabbccddeeffa0bn, 0xfc0d0e0f00a0b0c0n])],
);

// ./test/core/simd/simd_bit_shift.wast:612
assert_return(
  () =>
    invoke($0, `i64x2.shr_s`, [
      i64x2([0xffaabbccddeeffa0n, 0xc0d0e0f00a0b0c0dn]),
      8,
    ]),
  [i64x2([0xffffaabbccddeeffn, 0xffc0d0e0f00a0b0cn])],
);

// ./test/core/simd/simd_bit_shift.wast:615
assert_return(() => invoke($0, `i64x2.shr_s`, [i64x2([0x1n, 0xfn]), 16]), [
  i64x2([0x0n, 0x0n]),
]);

// ./test/core/simd/simd_bit_shift.wast:618
assert_return(() => invoke($0, `i64x2.shr_s`, [i64x2([0x1n, 0xfn]), 32]), [
  i64x2([0x0n, 0x0n]),
]);

// ./test/core/simd/simd_bit_shift.wast:622
assert_return(() => invoke($0, `i64x2.shr_s`, [i64x2([0x1n, 0xfn]), 128]), [
  i64x2([0x1n, 0xfn]),
]);

// ./test/core/simd/simd_bit_shift.wast:625
assert_return(() => invoke($0, `i64x2.shr_s`, [i64x2([0x1n, 0xfn]), 256]), [
  i64x2([0x1n, 0xfn]),
]);

// ./test/core/simd/simd_bit_shift.wast:629
assert_return(
  () =>
    invoke($0, `i64x2.shr_s`, [
      i64x2([0x8000000000000000n, 0xffffffff80000000n]),
      65,
    ]),
  [i64x2([0xc000000000000000n, 0xffffffffc0000000n])],
);

// ./test/core/simd/simd_bit_shift.wast:632
assert_return(() => invoke($0, `i64x2.shr_s`, [i64x2([0xcn, 0xdn]), 65]), [
  i64x2([0x6n, 0x6n]),
]);

// ./test/core/simd/simd_bit_shift.wast:635
assert_return(() => invoke($0, `i64x2.shr_s`, [i64x2([0x1n, 0xfn]), 129]), [
  i64x2([0x0n, 0x7n]),
]);

// ./test/core/simd/simd_bit_shift.wast:638
assert_return(() => invoke($0, `i64x2.shr_s`, [i64x2([0x1n, 0xfn]), 257]), [
  i64x2([0x0n, 0x7n]),
]);

// ./test/core/simd/simd_bit_shift.wast:641
assert_return(() => invoke($0, `i64x2.shr_s`, [i64x2([0x1n, 0xfn]), 513]), [
  i64x2([0x0n, 0x7n]),
]);

// ./test/core/simd/simd_bit_shift.wast:644
assert_return(() => invoke($0, `i64x2.shr_s`, [i64x2([0x1n, 0xfn]), 514]), [
  i64x2([0x0n, 0x3n]),
]);

// ./test/core/simd/simd_bit_shift.wast:649
assert_return(() => invoke($0, `i64x2.shl_1`, [i64x2([0x1n, 0xfn])]), [
  i64x2([0x2n, 0x1en]),
]);

// ./test/core/simd/simd_bit_shift.wast:651
assert_return(() => invoke($0, `i64x2.shr_u_64`, [i64x2([0x1n, 0xfn])]), [
  i64x2([0x1n, 0xfn]),
]);

// ./test/core/simd/simd_bit_shift.wast:653
assert_return(() => invoke($0, `i64x2.shr_s_65`, [i64x2([0x1n, 0xfn])]), [
  i64x2([0x0n, 0x7n]),
]);

// ./test/core/simd/simd_bit_shift.wast:658
let $1 = instantiate(`(module (memory 1)
  (func (export "i8x16.shl-in-block")
    (block
      (drop
        (block (result v128)
          (i8x16.shl
            (block (result v128) (v128.load (i32.const 0))) (i32.const 1)
          )
        )
      )
    )
  )
  (func (export "i8x16.shr_s-in-block")
    (block
      (drop
        (block (result v128)
          (i8x16.shr_s
            (block (result v128) (v128.load (i32.const 0))) (i32.const 1)
          )
        )
      )
    )
  )
  (func (export "i8x16.shr_u-in-block")
    (block
      (drop
        (block (result v128)
          (i8x16.shr_u
            (block (result v128) (v128.load (i32.const 0))) (i32.const 1)
          )
        )
      )
    )
  )
  (func (export "i16x8.shl-in-block")
    (block
      (drop
        (block (result v128)
          (i16x8.shl
            (block (result v128) (v128.load (i32.const 0))) (i32.const 1)
          )
        )
      )
    )
  )
  (func (export "i16x8.shr_s-in-block")
    (block
      (drop
        (block (result v128)
          (i16x8.shr_s
            (block (result v128) (v128.load (i32.const 0))) (i32.const 1)
          )
        )
      )
    )
  )
  (func (export "i16x8.shr_u-in-block")
    (block
      (drop
        (block (result v128)
          (i16x8.shr_u
            (block (result v128) (v128.load (i32.const 0))) (i32.const 1)
          )
        )
      )
    )
  )
  (func (export "i32x4.shl-in-block")
    (block
      (drop
        (block (result v128)
          (i32x4.shl
            (block (result v128) (v128.load (i32.const 0))) (i32.const 1)
          )
        )
      )
    )
  )
  (func (export "i32x4.shr_s-in-block")
    (block
      (drop
        (block (result v128)
          (i32x4.shr_s
            (block (result v128) (v128.load (i32.const 0))) (i32.const 1)
          )
        )
      )
    )
  )
  (func (export "i32x4.shr_u-in-block")
    (block
      (drop
        (block (result v128)
          (i32x4.shr_u
            (block (result v128) (v128.load (i32.const 0))) (i32.const 1)
          )
        )
      )
    )
  )
  (func (export "i64x2.shl-in-block")
    (block
      (drop
        (block (result v128)
          (i64x2.shl
            (block (result v128) (v128.load (i32.const 0))) (i32.const 1)
          )
        )
      )
    )
  )
  (func (export "i64x2.shr_s-in-block")
    (block
      (drop
        (block (result v128)
          (i64x2.shr_s
            (block (result v128) (v128.load (i32.const 0))) (i32.const 1)
          )
        )
      )
    )
  )
  (func (export "i64x2.shr_u-in-block")
    (block
      (drop
        (block (result v128)
          (i64x2.shr_u
            (block (result v128) (v128.load (i32.const 0))) (i32.const 1)
          )
        )
      )
    )
  )
  (func (export "nested-i8x16.shl")
    (drop
      (i8x16.shl
        (i8x16.shl
          (i8x16.shl
            (v128.load (i32.const 0)) (i32.const 1)
          )
          (i32.const 1)
        )
        (i32.const 1)
      )
    )
  )
  (func (export "nested-i8x16.shr_s")
    (drop
      (i8x16.shr_s
        (i8x16.shr_s
          (i8x16.shr_s
            (v128.load (i32.const 0)) (i32.const 1)
          )
          (i32.const 1)
        )
        (i32.const 1)
      )
    )
  )
  (func (export "nested-i8x16.shr_u")
    (drop
      (i8x16.shr_u
        (i8x16.shr_u
          (i8x16.shr_u
            (v128.load (i32.const 0)) (i32.const 1)
          )
          (i32.const 1)
        )
        (i32.const 1)
      )
    )
  )
  (func (export "nested-i16x8.shl")
    (drop
      (i16x8.shl
        (i16x8.shl
          (i16x8.shl
            (v128.load (i32.const 0)) (i32.const 1)
          )
          (i32.const 1)
        )
        (i32.const 1)
      )
    )
  )
  (func (export "nested-i16x8.shr_s")
    (drop
      (i16x8.shr_s
        (i16x8.shr_s
          (i16x8.shr_s
            (v128.load (i32.const 0)) (i32.const 1)
          )
          (i32.const 1)
        )
        (i32.const 1)
      )
    )
  )
  (func (export "nested-i16x8.shr_u")
    (drop
      (i16x8.shr_u
        (i16x8.shr_u
          (i16x8.shr_u
            (v128.load (i32.const 0)) (i32.const 1)
          )
          (i32.const 1)
        )
        (i32.const 1)
      )
    )
  )
  (func (export "nested-i32x4.shl")
    (drop
      (i32x4.shl
        (i32x4.shl
          (i32x4.shl
            (v128.load (i32.const 0)) (i32.const 1)
          )
          (i32.const 1)
        )
        (i32.const 1)
      )
    )
  )
  (func (export "nested-i32x4.shr_s")
    (drop
      (i32x4.shr_s
        (i32x4.shr_s
          (i32x4.shr_s
            (v128.load (i32.const 0)) (i32.const 1)
          )
          (i32.const 1)
        )
        (i32.const 1)
      )
    )
  )
  (func (export "nested-i32x4.shr_u")
    (drop
      (i32x4.shr_u
        (i32x4.shr_u
          (i32x4.shr_u
            (v128.load (i32.const 0)) (i32.const 1)
          )
          (i32.const 1)
        )
        (i32.const 1)
      )
    )
  )
  (func (export "nested-i64x2.shl")
    (drop
      (i64x2.shl
        (i64x2.shl
          (i64x2.shl
            (v128.load (i32.const 0)) (i32.const 1)
          )
          (i32.const 1)
        )
        (i32.const 1)
      )
    )
  )
  (func (export "nested-i64x2.shr_s")
    (drop
      (i64x2.shr_s
        (i64x2.shr_s
          (i64x2.shr_s
            (v128.load (i32.const 0)) (i32.const 1)
          )
          (i32.const 1)
        )
        (i32.const 1)
      )
    )
  )
  (func (export "nested-i64x2.shr_u")
    (drop
      (i64x2.shr_u
        (i64x2.shr_u
          (i64x2.shr_u
            (v128.load (i32.const 0)) (i32.const 1)
          )
          (i32.const 1)
        )
        (i32.const 1)
      )
    )
  )
)`);

// ./test/core/simd/simd_bit_shift.wast:949
assert_return(() => invoke($1, `i8x16.shl-in-block`, []), []);

// ./test/core/simd/simd_bit_shift.wast:950
assert_return(() => invoke($1, `i8x16.shr_s-in-block`, []), []);

// ./test/core/simd/simd_bit_shift.wast:951
assert_return(() => invoke($1, `i8x16.shr_u-in-block`, []), []);

// ./test/core/simd/simd_bit_shift.wast:952
assert_return(() => invoke($1, `i16x8.shl-in-block`, []), []);

// ./test/core/simd/simd_bit_shift.wast:953
assert_return(() => invoke($1, `i16x8.shr_s-in-block`, []), []);

// ./test/core/simd/simd_bit_shift.wast:954
assert_return(() => invoke($1, `i16x8.shr_u-in-block`, []), []);

// ./test/core/simd/simd_bit_shift.wast:955
assert_return(() => invoke($1, `i32x4.shl-in-block`, []), []);

// ./test/core/simd/simd_bit_shift.wast:956
assert_return(() => invoke($1, `i32x4.shr_s-in-block`, []), []);

// ./test/core/simd/simd_bit_shift.wast:957
assert_return(() => invoke($1, `i32x4.shr_u-in-block`, []), []);

// ./test/core/simd/simd_bit_shift.wast:958
assert_return(() => invoke($1, `i64x2.shl-in-block`, []), []);

// ./test/core/simd/simd_bit_shift.wast:959
assert_return(() => invoke($1, `i64x2.shr_s-in-block`, []), []);

// ./test/core/simd/simd_bit_shift.wast:960
assert_return(() => invoke($1, `i64x2.shr_u-in-block`, []), []);

// ./test/core/simd/simd_bit_shift.wast:961
assert_return(() => invoke($1, `nested-i8x16.shl`, []), []);

// ./test/core/simd/simd_bit_shift.wast:962
assert_return(() => invoke($1, `nested-i8x16.shr_s`, []), []);

// ./test/core/simd/simd_bit_shift.wast:963
assert_return(() => invoke($1, `nested-i8x16.shr_u`, []), []);

// ./test/core/simd/simd_bit_shift.wast:964
assert_return(() => invoke($1, `nested-i16x8.shl`, []), []);

// ./test/core/simd/simd_bit_shift.wast:965
assert_return(() => invoke($1, `nested-i16x8.shr_s`, []), []);

// ./test/core/simd/simd_bit_shift.wast:966
assert_return(() => invoke($1, `nested-i16x8.shr_u`, []), []);

// ./test/core/simd/simd_bit_shift.wast:967
assert_return(() => invoke($1, `nested-i32x4.shl`, []), []);

// ./test/core/simd/simd_bit_shift.wast:968
assert_return(() => invoke($1, `nested-i32x4.shr_s`, []), []);

// ./test/core/simd/simd_bit_shift.wast:969
assert_return(() => invoke($1, `nested-i32x4.shr_u`, []), []);

// ./test/core/simd/simd_bit_shift.wast:970
assert_return(() => invoke($1, `nested-i64x2.shl`, []), []);

// ./test/core/simd/simd_bit_shift.wast:971
assert_return(() => invoke($1, `nested-i64x2.shr_s`, []), []);

// ./test/core/simd/simd_bit_shift.wast:972
assert_return(() => invoke($1, `nested-i64x2.shr_u`, []), []);

// ./test/core/simd/simd_bit_shift.wast:976
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i8x16.shl   (i32.const 0) (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_bit_shift.wast:977
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i8x16.shr_s (i32.const 0) (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_bit_shift.wast:978
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i8x16.shr_u (i32.const 0) (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_bit_shift.wast:979
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i16x8.shl   (i32.const 0) (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_bit_shift.wast:980
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i16x8.shr_s (i32.const 0) (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_bit_shift.wast:981
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i16x8.shr_u (i32.const 0) (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_bit_shift.wast:982
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.shl   (i32.const 0) (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_bit_shift.wast:983
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.shr_s (i32.const 0) (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_bit_shift.wast:984
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.shr_u (i32.const 0) (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_bit_shift.wast:985
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i64x2.shl   (i32.const 0) (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_bit_shift.wast:986
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i64x2.shr_s (i32.const 0) (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_bit_shift.wast:987
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i64x2.shr_u (i32.const 0) (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_bit_shift.wast:991
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i8x16.shl_s (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_bit_shift.wast:992
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i8x16.shl_r (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_bit_shift.wast:993
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i8x16.shr   (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_bit_shift.wast:994
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i16x8.shl_s (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_bit_shift.wast:995
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i16x8.shl_r (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_bit_shift.wast:996
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i16x8.shr   (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_bit_shift.wast:997
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i32x4.shl_s (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_bit_shift.wast:998
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i32x4.shl_r (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_bit_shift.wast:999
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i32x4.shr   (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_bit_shift.wast:1000
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i64x2.shl_s (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_bit_shift.wast:1001
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i64x2.shl_r (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_bit_shift.wast:1002
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i64x2.shr   (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_bit_shift.wast:1003
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (f32x4.shl   (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_bit_shift.wast:1004
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (f32x4.shr_s (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_bit_shift.wast:1005
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (f32x4.shr_u (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_bit_shift.wast:1009
assert_invalid(() =>
  instantiate(`(module
    (func $$i8x16.shl-1st-arg-empty (result v128)
      (i8x16.shl (i32.const 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_bit_shift.wast:1017
assert_invalid(() =>
  instantiate(`(module
    (func $$i8x16.shl-last-arg-empty (result v128)
      (i8x16.shl (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_bit_shift.wast:1025
assert_invalid(() =>
  instantiate(`(module
    (func $$i8x16.shl-arg-empty (result v128)
      (i8x16.shl)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_bit_shift.wast:1033
assert_invalid(() =>
  instantiate(`(module
    (func $$i16x8.shr_u-1st-arg-empty (result v128)
      (i16x8.shr_u (i32.const 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_bit_shift.wast:1041
assert_invalid(() =>
  instantiate(`(module
    (func $$i16x8.shr_u-last-arg-empty (result v128)
      (i16x8.shr_u (v128.const i16x8 0 0 0 0 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_bit_shift.wast:1049
assert_invalid(() =>
  instantiate(`(module
    (func $$i16x8.shr_u-arg-empty (result v128)
      (i16x8.shr_u)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_bit_shift.wast:1057
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.shr_s-1st-arg-empty (result v128)
      (i32x4.shr_s (i32.const 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_bit_shift.wast:1065
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.shr_s-last-arg-empty (result v128)
      (i32x4.shr_s (v128.const i32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_bit_shift.wast:1073
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.shr_s-arg-empty (result v128)
      (i32x4.shr_s)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_bit_shift.wast:1081
assert_invalid(() =>
  instantiate(`(module
    (func $$i64x2.shl-1st-arg-empty (result v128)
      (i64x2.shl (i32.const 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_bit_shift.wast:1089
assert_invalid(() =>
  instantiate(`(module
    (func $$i64x2.shr_u-last-arg-empty (result v128)
      (i64x2.shr_u (v128.const i64x2 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_bit_shift.wast:1097
assert_invalid(() =>
  instantiate(`(module
    (func $$i64x2.shr_s-arg-empty (result v128)
      (i64x2.shr_s)
    )
  )`), `type mismatch`);
