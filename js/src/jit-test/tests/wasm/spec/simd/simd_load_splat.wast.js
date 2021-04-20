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

// ./test/core/simd/simd_load_splat.wast

// ./test/core/simd/simd_load_splat.wast:3
let $0 = instantiate(`(module
  (memory 1)
  (data (i32.const 0) "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0A\\0B\\0C\\0D\\0E\\0F")
  (data (i32.const 65520) "\\10\\11\\12\\13\\14\\15\\16\\17\\18\\19\\1A\\1B\\1C\\1D\\1E\\1F")

  (func (export "v128.load8_splat") (param $$address i32) (result v128) (v128.load8_splat (local.get $$address)))
  (func (export "v128.load16_splat") (param $$address i32) (result v128) (v128.load16_splat (local.get $$address)))
  (func (export "v128.load32_splat") (param $$address i32) (result v128) (v128.load32_splat (local.get $$address)))
  (func (export "v128.load64_splat") (param $$address i32) (result v128) (v128.load64_splat (local.get $$address)))

  ;; Load data with different offset/align arguments
  (func (export "v8x16.offset0") (param $$address i32) (result v128) (v128.load8_splat offset=0 (local.get $$address)))
  (func (export "v8x16.align1") (param $$address i32) (result v128) (v128.load8_splat align=1 (local.get $$address)))
  (func (export "v8x16.offset1_align1") (param $$address i32) (result v128) (v128.load8_splat offset=1 align=1 (local.get $$address)))
  (func (export "v8x16.offset2_align1") (param $$address i32) (result v128) (v128.load8_splat offset=2 align=1 (local.get $$address)))
  (func (export "v8x16.offset15_align1") (param $$address i32) (result v128) (v128.load8_splat offset=15 align=1 (local.get $$address)))

  (func (export "v16x8.offset0") (param $$address i32) (result v128) (v128.load16_splat offset=0 (local.get $$address)))
  (func (export "v16x8.align1") (param $$address i32) (result v128) (v128.load16_splat align=1 (local.get $$address)))
  (func (export "v16x8.offset1_align1") (param $$address i32) (result v128) (v128.load16_splat offset=1 align=1 (local.get $$address)))
  (func (export "v16x8.offset2_align1") (param $$address i32) (result v128) (v128.load16_splat offset=2 align=1 (local.get $$address)))
  (func (export "v16x8.offset15_align2") (param $$address i32) (result v128) (v128.load16_splat offset=15 align=2 (local.get $$address)))

  (func (export "v32x4.offset0") (param $$address i32) (result v128) (v128.load32_splat offset=0 (local.get $$address)))
  (func (export "v32x4.align1") (param $$address i32) (result v128) (v128.load32_splat align=1 (local.get $$address)))
  (func (export "v32x4.offset1_align1") (param $$address i32) (result v128) (v128.load32_splat offset=1 align=1 (local.get $$address)))
  (func (export "v32x4.offset2_align2") (param $$address i32) (result v128) (v128.load32_splat offset=2 align=2 (local.get $$address)))
  (func (export "v32x4.offset15_align4") (param $$address i32) (result v128) (v128.load32_splat offset=15 align=4 (local.get $$address)))

  (func (export "v64x2.offset0") (param $$address i32) (result v128) (v128.load64_splat offset=0 (local.get $$address)))
  (func (export "v64x2.align1") (param $$address i32) (result v128) (v128.load64_splat align=1 (local.get $$address)))
  (func (export "v64x2.offset1_align2") (param $$address i32) (result v128) (v128.load64_splat offset=1 align=2 (local.get $$address)))
  (func (export "v64x2.offset2_align4") (param $$address i32) (result v128) (v128.load64_splat offset=2 align=4 (local.get $$address)))
  (func (export "v64x2.offset15_align8") (param $$address i32) (result v128) (v128.load64_splat offset=15 align=8 (local.get $$address)))

  (func (export "v8x16.offset65536") (param $$address i32) (result v128) (v128.load8_splat offset=65536 (local.get $$address)))
  (func (export "v16x8.offset65535") (param $$address i32) (result v128) (v128.load16_splat offset=65535 (local.get $$address)))
  (func (export "v32x4.offset65533") (param $$address i32) (result v128) (v128.load32_splat offset=65533 (local.get $$address)))
  (func (export "v64x2.offset65529") (param $$address i32) (result v128) (v128.load64_splat offset=65529 (local.get $$address)))
)`);

// ./test/core/simd/simd_load_splat.wast:43
assert_return(() => invoke($0, `v128.load8_splat`, [0]), [
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
]);

// ./test/core/simd/simd_load_splat.wast:44
assert_return(() => invoke($0, `v128.load8_splat`, [1]), [
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
]);

// ./test/core/simd/simd_load_splat.wast:45
assert_return(() => invoke($0, `v128.load8_splat`, [2]), [
  i8x16([
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
  ]),
]);

// ./test/core/simd/simd_load_splat.wast:46
assert_return(() => invoke($0, `v128.load8_splat`, [3]), [
  i8x16([
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
  ]),
]);

// ./test/core/simd/simd_load_splat.wast:47
assert_return(() => invoke($0, `v128.load8_splat`, [65535]), [
  i8x16([
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
  ]),
]);

// ./test/core/simd/simd_load_splat.wast:48
assert_return(() => invoke($0, `v128.load16_splat`, [4]), [
  i16x8([0x504, 0x504, 0x504, 0x504, 0x504, 0x504, 0x504, 0x504]),
]);

// ./test/core/simd/simd_load_splat.wast:49
assert_return(() => invoke($0, `v128.load16_splat`, [5]), [
  i16x8([0x605, 0x605, 0x605, 0x605, 0x605, 0x605, 0x605, 0x605]),
]);

// ./test/core/simd/simd_load_splat.wast:50
assert_return(() => invoke($0, `v128.load16_splat`, [6]), [
  i16x8([0x706, 0x706, 0x706, 0x706, 0x706, 0x706, 0x706, 0x706]),
]);

// ./test/core/simd/simd_load_splat.wast:51
assert_return(() => invoke($0, `v128.load16_splat`, [7]), [
  i16x8([0x807, 0x807, 0x807, 0x807, 0x807, 0x807, 0x807, 0x807]),
]);

// ./test/core/simd/simd_load_splat.wast:52
assert_return(() => invoke($0, `v128.load16_splat`, [65534]), [
  i16x8([0x1f1e, 0x1f1e, 0x1f1e, 0x1f1e, 0x1f1e, 0x1f1e, 0x1f1e, 0x1f1e]),
]);

// ./test/core/simd/simd_load_splat.wast:53
assert_return(() => invoke($0, `v128.load32_splat`, [8]), [
  i32x4([0xb0a0908, 0xb0a0908, 0xb0a0908, 0xb0a0908]),
]);

// ./test/core/simd/simd_load_splat.wast:54
assert_return(() => invoke($0, `v128.load32_splat`, [9]), [
  i32x4([0xc0b0a09, 0xc0b0a09, 0xc0b0a09, 0xc0b0a09]),
]);

// ./test/core/simd/simd_load_splat.wast:55
assert_return(() => invoke($0, `v128.load32_splat`, [10]), [
  i32x4([0xd0c0b0a, 0xd0c0b0a, 0xd0c0b0a, 0xd0c0b0a]),
]);

// ./test/core/simd/simd_load_splat.wast:56
assert_return(() => invoke($0, `v128.load32_splat`, [11]), [
  i32x4([0xe0d0c0b, 0xe0d0c0b, 0xe0d0c0b, 0xe0d0c0b]),
]);

// ./test/core/simd/simd_load_splat.wast:57
assert_return(() => invoke($0, `v128.load32_splat`, [65532]), [
  i32x4([0x1f1e1d1c, 0x1f1e1d1c, 0x1f1e1d1c, 0x1f1e1d1c]),
]);

// ./test/core/simd/simd_load_splat.wast:58
assert_return(() => invoke($0, `v128.load64_splat`, [12]), [
  i64x2([0xf0e0d0cn, 0xf0e0d0cn]),
]);

// ./test/core/simd/simd_load_splat.wast:59
assert_return(() => invoke($0, `v128.load64_splat`, [13]), [
  i64x2([0xf0e0dn, 0xf0e0dn]),
]);

// ./test/core/simd/simd_load_splat.wast:60
assert_return(() => invoke($0, `v128.load64_splat`, [14]), [
  i64x2([0xf0en, 0xf0en]),
]);

// ./test/core/simd/simd_load_splat.wast:61
assert_return(() => invoke($0, `v128.load64_splat`, [15]), [
  i64x2([0xfn, 0xfn]),
]);

// ./test/core/simd/simd_load_splat.wast:62
assert_return(() => invoke($0, `v128.load64_splat`, [65528]), [
  i64x2([0x1f1e1d1c1b1a1918n, 0x1f1e1d1c1b1a1918n]),
]);

// ./test/core/simd/simd_load_splat.wast:65
assert_return(() => invoke($0, `v8x16.offset0`, [0]), [
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
]);

// ./test/core/simd/simd_load_splat.wast:66
assert_return(() => invoke($0, `v8x16.align1`, [0]), [
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
]);

// ./test/core/simd/simd_load_splat.wast:67
assert_return(() => invoke($0, `v8x16.offset1_align1`, [0]), [
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
]);

// ./test/core/simd/simd_load_splat.wast:68
assert_return(() => invoke($0, `v8x16.offset2_align1`, [0]), [
  i8x16([
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
  ]),
]);

// ./test/core/simd/simd_load_splat.wast:69
assert_return(() => invoke($0, `v8x16.offset15_align1`, [0]), [
  i8x16([
    0xf,
    0xf,
    0xf,
    0xf,
    0xf,
    0xf,
    0xf,
    0xf,
    0xf,
    0xf,
    0xf,
    0xf,
    0xf,
    0xf,
    0xf,
    0xf,
  ]),
]);

// ./test/core/simd/simd_load_splat.wast:70
assert_return(() => invoke($0, `v8x16.offset0`, [1]), [
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
]);

// ./test/core/simd/simd_load_splat.wast:71
assert_return(() => invoke($0, `v8x16.align1`, [1]), [
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
]);

// ./test/core/simd/simd_load_splat.wast:72
assert_return(() => invoke($0, `v8x16.offset1_align1`, [1]), [
  i8x16([
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
    0x2,
  ]),
]);

// ./test/core/simd/simd_load_splat.wast:73
assert_return(() => invoke($0, `v8x16.offset2_align1`, [1]), [
  i8x16([
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
  ]),
]);

// ./test/core/simd/simd_load_splat.wast:74
assert_return(() => invoke($0, `v8x16.offset15_align1`, [1]), [
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
]);

// ./test/core/simd/simd_load_splat.wast:75
assert_return(() => invoke($0, `v8x16.offset0`, [65535]), [
  i8x16([
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
  ]),
]);

// ./test/core/simd/simd_load_splat.wast:76
assert_return(() => invoke($0, `v8x16.align1`, [65535]), [
  i8x16([
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
    0x1f,
  ]),
]);

// ./test/core/simd/simd_load_splat.wast:78
assert_return(() => invoke($0, `v16x8.offset0`, [0]), [
  i16x8([0x100, 0x100, 0x100, 0x100, 0x100, 0x100, 0x100, 0x100]),
]);

// ./test/core/simd/simd_load_splat.wast:79
assert_return(() => invoke($0, `v16x8.align1`, [0]), [
  i16x8([0x100, 0x100, 0x100, 0x100, 0x100, 0x100, 0x100, 0x100]),
]);

// ./test/core/simd/simd_load_splat.wast:80
assert_return(() => invoke($0, `v16x8.offset1_align1`, [0]), [
  i16x8([0x201, 0x201, 0x201, 0x201, 0x201, 0x201, 0x201, 0x201]),
]);

// ./test/core/simd/simd_load_splat.wast:81
assert_return(() => invoke($0, `v16x8.offset2_align1`, [0]), [
  i16x8([0x302, 0x302, 0x302, 0x302, 0x302, 0x302, 0x302, 0x302]),
]);

// ./test/core/simd/simd_load_splat.wast:82
assert_return(() => invoke($0, `v16x8.offset15_align2`, [0]), [
  i16x8([0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf]),
]);

// ./test/core/simd/simd_load_splat.wast:83
assert_return(() => invoke($0, `v16x8.offset0`, [1]), [
  i16x8([0x201, 0x201, 0x201, 0x201, 0x201, 0x201, 0x201, 0x201]),
]);

// ./test/core/simd/simd_load_splat.wast:84
assert_return(() => invoke($0, `v16x8.align1`, [1]), [
  i16x8([0x201, 0x201, 0x201, 0x201, 0x201, 0x201, 0x201, 0x201]),
]);

// ./test/core/simd/simd_load_splat.wast:85
assert_return(() => invoke($0, `v16x8.offset1_align1`, [1]), [
  i16x8([0x302, 0x302, 0x302, 0x302, 0x302, 0x302, 0x302, 0x302]),
]);

// ./test/core/simd/simd_load_splat.wast:86
assert_return(() => invoke($0, `v16x8.offset2_align1`, [1]), [
  i16x8([0x403, 0x403, 0x403, 0x403, 0x403, 0x403, 0x403, 0x403]),
]);

// ./test/core/simd/simd_load_splat.wast:87
assert_return(() => invoke($0, `v16x8.offset15_align2`, [1]), [
  i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
]);

// ./test/core/simd/simd_load_splat.wast:88
assert_return(() => invoke($0, `v16x8.offset0`, [65534]), [
  i16x8([0x1f1e, 0x1f1e, 0x1f1e, 0x1f1e, 0x1f1e, 0x1f1e, 0x1f1e, 0x1f1e]),
]);

// ./test/core/simd/simd_load_splat.wast:89
assert_return(() => invoke($0, `v16x8.align1`, [65534]), [
  i16x8([0x1f1e, 0x1f1e, 0x1f1e, 0x1f1e, 0x1f1e, 0x1f1e, 0x1f1e, 0x1f1e]),
]);

// ./test/core/simd/simd_load_splat.wast:91
assert_return(() => invoke($0, `v32x4.offset0`, [0]), [
  i32x4([0x3020100, 0x3020100, 0x3020100, 0x3020100]),
]);

// ./test/core/simd/simd_load_splat.wast:92
assert_return(() => invoke($0, `v32x4.align1`, [0]), [
  i32x4([0x3020100, 0x3020100, 0x3020100, 0x3020100]),
]);

// ./test/core/simd/simd_load_splat.wast:93
assert_return(() => invoke($0, `v32x4.offset1_align1`, [0]), [
  i32x4([0x4030201, 0x4030201, 0x4030201, 0x4030201]),
]);

// ./test/core/simd/simd_load_splat.wast:94
assert_return(() => invoke($0, `v32x4.offset2_align2`, [0]), [
  i32x4([0x5040302, 0x5040302, 0x5040302, 0x5040302]),
]);

// ./test/core/simd/simd_load_splat.wast:95
assert_return(() => invoke($0, `v32x4.offset15_align4`, [0]), [
  i32x4([0xf, 0xf, 0xf, 0xf]),
]);

// ./test/core/simd/simd_load_splat.wast:96
assert_return(() => invoke($0, `v32x4.offset0`, [1]), [
  i32x4([0x4030201, 0x4030201, 0x4030201, 0x4030201]),
]);

// ./test/core/simd/simd_load_splat.wast:97
assert_return(() => invoke($0, `v32x4.align1`, [1]), [
  i32x4([0x4030201, 0x4030201, 0x4030201, 0x4030201]),
]);

// ./test/core/simd/simd_load_splat.wast:98
assert_return(() => invoke($0, `v32x4.offset1_align1`, [1]), [
  i32x4([0x5040302, 0x5040302, 0x5040302, 0x5040302]),
]);

// ./test/core/simd/simd_load_splat.wast:99
assert_return(() => invoke($0, `v32x4.offset2_align2`, [1]), [
  i32x4([0x6050403, 0x6050403, 0x6050403, 0x6050403]),
]);

// ./test/core/simd/simd_load_splat.wast:100
assert_return(() => invoke($0, `v32x4.offset15_align4`, [1]), [
  i32x4([0x0, 0x0, 0x0, 0x0]),
]);

// ./test/core/simd/simd_load_splat.wast:101
assert_return(() => invoke($0, `v32x4.offset0`, [65532]), [
  i32x4([0x1f1e1d1c, 0x1f1e1d1c, 0x1f1e1d1c, 0x1f1e1d1c]),
]);

// ./test/core/simd/simd_load_splat.wast:102
assert_return(() => invoke($0, `v32x4.align1`, [65532]), [
  i32x4([0x1f1e1d1c, 0x1f1e1d1c, 0x1f1e1d1c, 0x1f1e1d1c]),
]);

// ./test/core/simd/simd_load_splat.wast:104
assert_return(() => invoke($0, `v64x2.offset0`, [0]), [
  i64x2([0x706050403020100n, 0x706050403020100n]),
]);

// ./test/core/simd/simd_load_splat.wast:105
assert_return(() => invoke($0, `v64x2.align1`, [0]), [
  i64x2([0x706050403020100n, 0x706050403020100n]),
]);

// ./test/core/simd/simd_load_splat.wast:106
assert_return(() => invoke($0, `v64x2.offset1_align2`, [0]), [
  i64x2([0x807060504030201n, 0x807060504030201n]),
]);

// ./test/core/simd/simd_load_splat.wast:107
assert_return(() => invoke($0, `v64x2.offset2_align4`, [0]), [
  i64x2([0x908070605040302n, 0x908070605040302n]),
]);

// ./test/core/simd/simd_load_splat.wast:108
assert_return(() => invoke($0, `v64x2.offset15_align8`, [0]), [
  i64x2([0xfn, 0xfn]),
]);

// ./test/core/simd/simd_load_splat.wast:109
assert_return(() => invoke($0, `v64x2.offset0`, [1]), [
  i64x2([0x807060504030201n, 0x807060504030201n]),
]);

// ./test/core/simd/simd_load_splat.wast:110
assert_return(() => invoke($0, `v64x2.align1`, [1]), [
  i64x2([0x807060504030201n, 0x807060504030201n]),
]);

// ./test/core/simd/simd_load_splat.wast:111
assert_return(() => invoke($0, `v64x2.offset1_align2`, [1]), [
  i64x2([0x908070605040302n, 0x908070605040302n]),
]);

// ./test/core/simd/simd_load_splat.wast:112
assert_return(() => invoke($0, `v64x2.offset2_align4`, [1]), [
  i64x2([0xa09080706050403n, 0xa09080706050403n]),
]);

// ./test/core/simd/simd_load_splat.wast:113
assert_return(() => invoke($0, `v64x2.offset15_align8`, [1]), [
  i64x2([0x0n, 0x0n]),
]);

// ./test/core/simd/simd_load_splat.wast:114
assert_return(() => invoke($0, `v64x2.offset0`, [65528]), [
  i64x2([0x1f1e1d1c1b1a1918n, 0x1f1e1d1c1b1a1918n]),
]);

// ./test/core/simd/simd_load_splat.wast:115
assert_return(() => invoke($0, `v64x2.align1`, [65528]), [
  i64x2([0x1f1e1d1c1b1a1918n, 0x1f1e1d1c1b1a1918n]),
]);

// ./test/core/simd/simd_load_splat.wast:119
assert_trap(
  () => invoke($0, `v128.load8_splat`, [-1]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:120
assert_trap(
  () => invoke($0, `v128.load16_splat`, [-1]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:121
assert_trap(
  () => invoke($0, `v128.load32_splat`, [-1]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:122
assert_trap(
  () => invoke($0, `v128.load64_splat`, [-1]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:123
assert_trap(
  () => invoke($0, `v128.load8_splat`, [65536]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:124
assert_trap(
  () => invoke($0, `v128.load16_splat`, [65535]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:125
assert_trap(
  () => invoke($0, `v128.load32_splat`, [65533]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:126
assert_trap(
  () => invoke($0, `v128.load64_splat`, [65529]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:128
assert_trap(
  () => invoke($0, `v8x16.offset1_align1`, [65535]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:129
assert_trap(
  () => invoke($0, `v8x16.offset2_align1`, [65535]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:130
assert_trap(
  () => invoke($0, `v8x16.offset15_align1`, [65535]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:131
assert_trap(
  () => invoke($0, `v16x8.offset1_align1`, [65534]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:132
assert_trap(
  () => invoke($0, `v16x8.offset2_align1`, [65534]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:133
assert_trap(
  () => invoke($0, `v16x8.offset15_align2`, [65534]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:134
assert_trap(
  () => invoke($0, `v32x4.offset1_align1`, [65532]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:135
assert_trap(
  () => invoke($0, `v32x4.offset2_align2`, [65532]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:136
assert_trap(
  () => invoke($0, `v32x4.offset15_align4`, [65532]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:137
assert_trap(
  () => invoke($0, `v64x2.offset1_align2`, [65528]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:138
assert_trap(
  () => invoke($0, `v64x2.offset2_align4`, [65528]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:139
assert_trap(
  () => invoke($0, `v64x2.offset15_align8`, [65528]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:141
assert_trap(
  () => invoke($0, `v8x16.offset1_align1`, [-1]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:142
assert_trap(
  () => invoke($0, `v16x8.offset1_align1`, [-1]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:143
assert_trap(
  () => invoke($0, `v32x4.offset1_align1`, [-1]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:144
assert_trap(
  () => invoke($0, `v64x2.offset1_align2`, [-1]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:146
assert_trap(
  () => invoke($0, `v8x16.offset65536`, [0]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:147
assert_trap(
  () => invoke($0, `v16x8.offset65535`, [0]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:148
assert_trap(
  () => invoke($0, `v32x4.offset65533`, [0]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:149
assert_trap(
  () => invoke($0, `v64x2.offset65529`, [0]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:150
assert_trap(
  () => invoke($0, `v8x16.offset65536`, [1]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:151
assert_trap(
  () => invoke($0, `v16x8.offset65535`, [1]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:152
assert_trap(
  () => invoke($0, `v32x4.offset65533`, [1]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:153
assert_trap(
  () => invoke($0, `v64x2.offset65529`, [1]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_load_splat.wast:158
let $1 = instantiate(`(module (memory 1)
  (data (i32.const 0) "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0A")

  (func (export "v128.load8_splat-in-block") (result v128)
      (block (result v128) (block (result v128) (v128.load8_splat (i32.const 0))))
  )
  (func (export "v128.load16_splat-in-block") (result v128)
      (block (result v128) (block (result v128) (v128.load16_splat (i32.const 1))))
  )
  (func (export "v128.load32_splat-in-block") (result v128)
      (block (result v128) (block (result v128) (v128.load32_splat (i32.const 2))))
  )
  (func (export "v128.load64_splat-in-block") (result v128)
      (block (result v128) (block (result v128) (v128.load64_splat (i32.const 9))))
  )
  (func (export "v128.load8_splat-as-br-value") (result v128)
    (block (result v128) (br 0 (v128.load8_splat (i32.const 3))))
  )
  (func (export "v128.load16_splat-as-br-value") (result v128)
    (block (result v128) (br 0 (v128.load16_splat (i32.const 4))))
  )
  (func (export "v128.load32_splat-as-br-value") (result v128)
    (block (result v128) (br 0 (v128.load32_splat (i32.const 5))))
  )
  (func (export "v128.load64_splat-as-br-value") (result v128)
    (block (result v128) (br 0 (v128.load64_splat (i32.const 10))))
  )
  (func (export "v128.load8_splat-extract_lane_s-operand") (result i32)
    (i8x16.extract_lane_s 0 (v128.load8_splat (i32.const 6)))
  )
  (func (export "v128.load16_splat-extract_lane_s-operand") (result i32)
    (i8x16.extract_lane_s 0 (v128.load16_splat (i32.const 7)))
  )
  (func (export "v128.load32_splat-extract_lane_s-operand") (result i32)
    (i8x16.extract_lane_s 0 (v128.load32_splat (i32.const 8)))
  )
  (func (export "v128.load64_splat-extract_lane_s-operand") (result i32)
    (i8x16.extract_lane_s 0 (v128.load64_splat (i32.const 11)))
  )
)`);

// ./test/core/simd/simd_load_splat.wast:198
assert_return(() => invoke($1, `v128.load8_splat-in-block`, []), [
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
]);

// ./test/core/simd/simd_load_splat.wast:199
assert_return(() => invoke($1, `v128.load16_splat-in-block`, []), [
  i16x8([0x201, 0x201, 0x201, 0x201, 0x201, 0x201, 0x201, 0x201]),
]);

// ./test/core/simd/simd_load_splat.wast:200
assert_return(() => invoke($1, `v128.load32_splat-in-block`, []), [
  i32x4([0x5040302, 0x5040302, 0x5040302, 0x5040302]),
]);

// ./test/core/simd/simd_load_splat.wast:201
assert_return(() => invoke($1, `v128.load64_splat-in-block`, []), [
  i64x2([0xa09n, 0xa09n]),
]);

// ./test/core/simd/simd_load_splat.wast:202
assert_return(() => invoke($1, `v128.load8_splat-as-br-value`, []), [
  i8x16([
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
    0x3,
  ]),
]);

// ./test/core/simd/simd_load_splat.wast:203
assert_return(() => invoke($1, `v128.load16_splat-as-br-value`, []), [
  i16x8([0x504, 0x504, 0x504, 0x504, 0x504, 0x504, 0x504, 0x504]),
]);

// ./test/core/simd/simd_load_splat.wast:204
assert_return(() => invoke($1, `v128.load32_splat-as-br-value`, []), [
  i32x4([0x8070605, 0x8070605, 0x8070605, 0x8070605]),
]);

// ./test/core/simd/simd_load_splat.wast:205
assert_return(() => invoke($1, `v128.load64_splat-as-br-value`, []), [
  i64x2([0xan, 0xan]),
]);

// ./test/core/simd/simd_load_splat.wast:206
assert_return(() => invoke($1, `v128.load8_splat-extract_lane_s-operand`, []), [
  value("i32", 6),
]);

// ./test/core/simd/simd_load_splat.wast:207
assert_return(
  () => invoke($1, `v128.load16_splat-extract_lane_s-operand`, []),
  [value("i32", 7)],
);

// ./test/core/simd/simd_load_splat.wast:208
assert_return(
  () => invoke($1, `v128.load32_splat-extract_lane_s-operand`, []),
  [value("i32", 8)],
);

// ./test/core/simd/simd_load_splat.wast:209
assert_return(
  () => invoke($1, `v128.load64_splat-extract_lane_s-operand`, []),
  [value("i32", 0)],
);

// ./test/core/simd/simd_load_splat.wast:214
assert_invalid(
  () =>
    instantiate(
      `(module (memory 0) (func (result v128) (v128.load8_splat (v128.const i32x4 0 0 0 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_load_splat.wast:215
assert_invalid(
  () =>
    instantiate(
      `(module (memory 0) (func (result v128) (v128.load16_splat (v128.const i32x4 0 0 0 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_load_splat.wast:216
assert_invalid(
  () =>
    instantiate(
      `(module (memory 0) (func (result v128) (v128.load32_splat (v128.const i32x4 0 0 0 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_load_splat.wast:217
assert_invalid(
  () =>
    instantiate(
      `(module (memory 0) (func (result v128) (v128.load64_splat (v128.const i32x4 0 0 0 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_load_splat.wast:222
assert_malformed(
  () =>
    instantiate(`(memory 1) (func (drop (i8x16.load_splat (i32.const 0)))) `),
  `unknown operator`,
);

// ./test/core/simd/simd_load_splat.wast:223
assert_malformed(
  () =>
    instantiate(`(memory 1) (func (drop (i16x8.load_splat (i32.const 0)))) `),
  `unknown operator`,
);

// ./test/core/simd/simd_load_splat.wast:224
assert_malformed(
  () =>
    instantiate(`(memory 1) (func (drop (i32x4.load_splat (i32.const 0)))) `),
  `unknown operator`,
);

// ./test/core/simd/simd_load_splat.wast:225
assert_malformed(
  () =>
    instantiate(`(memory 1) (func (drop (i64x2.load_splat (i32.const 0)))) `),
  `unknown operator`,
);

// ./test/core/simd/simd_load_splat.wast:230
assert_invalid(() =>
  instantiate(`(module (memory 0)
    (func $$v128.load8_splat-arg-empty (result v128)
      (v128.load8_splat)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_load_splat.wast:238
assert_invalid(() =>
  instantiate(`(module (memory 0)
    (func $$v128.load16_splat-arg-empty (result v128)
      (v128.load16_splat)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_load_splat.wast:246
assert_invalid(() =>
  instantiate(`(module (memory 0)
    (func $$v128.load32_splat-arg-empty (result v128)
      (v128.load32_splat)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_load_splat.wast:254
assert_invalid(() =>
  instantiate(`(module (memory 0)
    (func $$v128.load64_splat-arg-empty (result v128)
      (v128.load64_splat)
    )
  )`), `type mismatch`);
