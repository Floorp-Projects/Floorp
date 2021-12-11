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

// ./test/core/simd/simd_address.wast

// ./test/core/simd/simd_address.wast:3
let $0 = instantiate(`(module
  (memory 1)
  (data (i32.const 0) "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\10\\11\\12\\13\\14\\15")
  (data (offset (i32.const 65505)) "\\16\\17\\18\\19\\20\\21\\22\\23\\24\\25\\26\\27\\28\\29\\30\\31")

  (func (export "load_data_1") (param $$i i32) (result v128)
    (v128.load offset=0 (local.get $$i))                   ;; 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x10 0x11 0x12 0x13 0x14 0x15
  )
  (func (export "load_data_2") (param $$i i32) (result v128)
    (v128.load align=1 (local.get $$i))                    ;; 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x10 0x11 0x12 0x13 0x14 0x15
  )
  (func (export "load_data_3") (param $$i i32) (result v128)
    (v128.load offset=1 align=1 (local.get $$i))           ;; 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x10 0x11 0x12 0x13 0x14 0x15 0x00
  )
  (func (export "load_data_4") (param $$i i32) (result v128)
    (v128.load offset=2 align=1 (local.get $$i))           ;; 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x10 0x11 0x12 0x13 0x14 0x15 0x00 0x00
  )
  (func (export "load_data_5") (param $$i i32) (result v128)
    (v128.load offset=15 align=1 (local.get $$i))          ;; 0x15 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  )

  (func (export "store_data_0") (result v128)
    (v128.store offset=0 (i32.const 0) (v128.const f32x4 0 1 2 3))
    (v128.load offset=0 (i32.const 0))
  )
  (func (export "store_data_1") (result v128)
    (v128.store align=1 (i32.const 0) (v128.const i32x4 0 1 2 3))
    (v128.load align=1 (i32.const 0))
  )
  (func (export "store_data_2") (result v128)
    (v128.store offset=1 align=1 (i32.const 0) (v128.const i16x8 0 1 2 3 4 5 6 7))
    (v128.load offset=1 align=1 (i32.const 0))
  )
  (func (export "store_data_3") (result v128)
    (v128.store offset=2 align=1 (i32.const 0) (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))
    (v128.load offset=2 align=1 (i32.const 0))
  )
  (func (export "store_data_4") (result v128)
    (v128.store offset=15 align=1 (i32.const 0) (v128.const i32x4 0 1 2 3))
    (v128.load offset=15 (i32.const 0))
  )
  (func (export "store_data_5") (result v128)
    (v128.store offset=65520 align=1 (i32.const 0) (v128.const i32x4 0 1 2 3))
    (v128.load offset=65520 (i32.const 0))
  )
  (func (export "store_data_6") (param $$i i32)
    (v128.store offset=1 align=1 (local.get $$i) (v128.const i32x4 0 1 2 3))
  )
)`);

// ./test/core/simd/simd_address.wast:53
assert_return(() => invoke($0, `load_data_1`, [0]), [
  i32x4([0x3020100, 0x7060504, 0x11100908, 0x15141312]),
]);

// ./test/core/simd/simd_address.wast:54
assert_return(() => invoke($0, `load_data_2`, [0]), [
  i32x4([0x3020100, 0x7060504, 0x11100908, 0x15141312]),
]);

// ./test/core/simd/simd_address.wast:55
assert_return(() => invoke($0, `load_data_3`, [0]), [
  i32x4([0x4030201, 0x8070605, 0x12111009, 0x151413]),
]);

// ./test/core/simd/simd_address.wast:56
assert_return(() => invoke($0, `load_data_4`, [0]), [
  i32x4([0x5040302, 0x9080706, 0x13121110, 0x1514]),
]);

// ./test/core/simd/simd_address.wast:57
assert_return(() => invoke($0, `load_data_5`, [0]), [
  i32x4([0x15, 0x0, 0x0, 0x0]),
]);

// ./test/core/simd/simd_address.wast:59
assert_return(() => invoke($0, `load_data_1`, [0]), [
  i16x8([0x100, 0x302, 0x504, 0x706, 0x908, 0x1110, 0x1312, 0x1514]),
]);

// ./test/core/simd/simd_address.wast:60
assert_return(() => invoke($0, `load_data_2`, [0]), [
  i16x8([0x100, 0x302, 0x504, 0x706, 0x908, 0x1110, 0x1312, 0x1514]),
]);

// ./test/core/simd/simd_address.wast:61
assert_return(() => invoke($0, `load_data_3`, [0]), [
  i16x8([0x201, 0x403, 0x605, 0x807, 0x1009, 0x1211, 0x1413, 0x15]),
]);

// ./test/core/simd/simd_address.wast:62
assert_return(() => invoke($0, `load_data_4`, [0]), [
  i16x8([0x302, 0x504, 0x706, 0x908, 0x1110, 0x1312, 0x1514, 0x0]),
]);

// ./test/core/simd/simd_address.wast:63
assert_return(() => invoke($0, `load_data_5`, [0]), [
  i16x8([0x15, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
]);

// ./test/core/simd/simd_address.wast:65
assert_return(() => invoke($0, `load_data_1`, [0]), [
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
    0x10,
    0x11,
    0x12,
    0x13,
    0x14,
    0x15,
  ]),
]);

// ./test/core/simd/simd_address.wast:66
assert_return(() => invoke($0, `load_data_2`, [0]), [
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
    0x10,
    0x11,
    0x12,
    0x13,
    0x14,
    0x15,
  ]),
]);

// ./test/core/simd/simd_address.wast:67
assert_return(() => invoke($0, `load_data_3`, [0]), [
  i8x16([
    0x1,
    0x2,
    0x3,
    0x4,
    0x5,
    0x6,
    0x7,
    0x8,
    0x9,
    0x10,
    0x11,
    0x12,
    0x13,
    0x14,
    0x15,
    0x0,
  ]),
]);

// ./test/core/simd/simd_address.wast:68
assert_return(() => invoke($0, `load_data_4`, [0]), [
  i8x16([
    0x2,
    0x3,
    0x4,
    0x5,
    0x6,
    0x7,
    0x8,
    0x9,
    0x10,
    0x11,
    0x12,
    0x13,
    0x14,
    0x15,
    0x0,
    0x0,
  ]),
]);

// ./test/core/simd/simd_address.wast:69
assert_return(() => invoke($0, `load_data_5`, [0]), [
  i8x16([
    0x15,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
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

// ./test/core/simd/simd_address.wast:71
assert_return(() => invoke($0, `load_data_1`, [65505]), [
  i32x4([0x19181716, 0x23222120, 0x27262524, 0x31302928]),
]);

// ./test/core/simd/simd_address.wast:72
assert_return(() => invoke($0, `load_data_2`, [65505]), [
  i32x4([0x19181716, 0x23222120, 0x27262524, 0x31302928]),
]);

// ./test/core/simd/simd_address.wast:73
assert_return(() => invoke($0, `load_data_3`, [65505]), [
  i32x4([0x20191817, 0x24232221, 0x28272625, 0x313029]),
]);

// ./test/core/simd/simd_address.wast:74
assert_return(() => invoke($0, `load_data_4`, [65505]), [
  i32x4([0x21201918, 0x25242322, 0x29282726, 0x3130]),
]);

// ./test/core/simd/simd_address.wast:75
assert_return(() => invoke($0, `load_data_5`, [65505]), [
  i32x4([0x31, 0x0, 0x0, 0x0]),
]);

// ./test/core/simd/simd_address.wast:77
assert_return(() => invoke($0, `load_data_1`, [65505]), [
  i16x8([0x1716, 0x1918, 0x2120, 0x2322, 0x2524, 0x2726, 0x2928, 0x3130]),
]);

// ./test/core/simd/simd_address.wast:78
assert_return(() => invoke($0, `load_data_2`, [65505]), [
  i16x8([0x1716, 0x1918, 0x2120, 0x2322, 0x2524, 0x2726, 0x2928, 0x3130]),
]);

// ./test/core/simd/simd_address.wast:79
assert_return(() => invoke($0, `load_data_3`, [65505]), [
  i16x8([0x1817, 0x2019, 0x2221, 0x2423, 0x2625, 0x2827, 0x3029, 0x31]),
]);

// ./test/core/simd/simd_address.wast:80
assert_return(() => invoke($0, `load_data_4`, [65505]), [
  i16x8([0x1918, 0x2120, 0x2322, 0x2524, 0x2726, 0x2928, 0x3130, 0x0]),
]);

// ./test/core/simd/simd_address.wast:81
assert_return(() => invoke($0, `load_data_5`, [65505]), [
  i16x8([0x31, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
]);

// ./test/core/simd/simd_address.wast:83
assert_return(() => invoke($0, `load_data_1`, [65505]), [
  i8x16([
    0x16,
    0x17,
    0x18,
    0x19,
    0x20,
    0x21,
    0x22,
    0x23,
    0x24,
    0x25,
    0x26,
    0x27,
    0x28,
    0x29,
    0x30,
    0x31,
  ]),
]);

// ./test/core/simd/simd_address.wast:84
assert_return(() => invoke($0, `load_data_2`, [65505]), [
  i8x16([
    0x16,
    0x17,
    0x18,
    0x19,
    0x20,
    0x21,
    0x22,
    0x23,
    0x24,
    0x25,
    0x26,
    0x27,
    0x28,
    0x29,
    0x30,
    0x31,
  ]),
]);

// ./test/core/simd/simd_address.wast:85
assert_return(() => invoke($0, `load_data_3`, [65505]), [
  i8x16([
    0x17,
    0x18,
    0x19,
    0x20,
    0x21,
    0x22,
    0x23,
    0x24,
    0x25,
    0x26,
    0x27,
    0x28,
    0x29,
    0x30,
    0x31,
    0x0,
  ]),
]);

// ./test/core/simd/simd_address.wast:86
assert_return(() => invoke($0, `load_data_4`, [65505]), [
  i8x16([
    0x18,
    0x19,
    0x20,
    0x21,
    0x22,
    0x23,
    0x24,
    0x25,
    0x26,
    0x27,
    0x28,
    0x29,
    0x30,
    0x31,
    0x0,
    0x0,
  ]),
]);

// ./test/core/simd/simd_address.wast:87
assert_return(() => invoke($0, `load_data_5`, [65505]), [
  i8x16([
    0x31,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
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

// ./test/core/simd/simd_address.wast:89
assert_trap(
  () => invoke($0, `load_data_3`, [-1]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_address.wast:90
assert_trap(
  () => invoke($0, `load_data_5`, [65506]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_address.wast:92
assert_return(() => invoke($0, `store_data_0`, []), [
  new F32x4Pattern(
    value("f32", 0),
    value("f32", 1),
    value("f32", 2),
    value("f32", 3),
  ),
]);

// ./test/core/simd/simd_address.wast:93
assert_return(() => invoke($0, `store_data_1`, []), [
  i32x4([0x0, 0x1, 0x2, 0x3]),
]);

// ./test/core/simd/simd_address.wast:94
assert_return(() => invoke($0, `store_data_2`, []), [
  i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]),
]);

// ./test/core/simd/simd_address.wast:95
assert_return(() => invoke($0, `store_data_3`, []), [
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
]);

// ./test/core/simd/simd_address.wast:96
assert_return(() => invoke($0, `store_data_4`, []), [
  i32x4([0x0, 0x1, 0x2, 0x3]),
]);

// ./test/core/simd/simd_address.wast:97
assert_return(() => invoke($0, `store_data_5`, []), [
  i32x4([0x0, 0x1, 0x2, 0x3]),
]);

// ./test/core/simd/simd_address.wast:99
assert_trap(
  () => invoke($0, `store_data_6`, [-1]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_address.wast:100
assert_trap(
  () => invoke($0, `store_data_6`, [65535]),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_address.wast:104
let $1 = instantiate(`(module
  (memory 1)
  (func (export "v128.load_offset_65521")
    (drop (v128.load offset=65521 (i32.const 0)))
  )
)`);

// ./test/core/simd/simd_address.wast:110
assert_trap(
  () => invoke($1, `v128.load_offset_65521`, []),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_address.wast:112
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func   (drop (v128.load offset=-1 (i32.const 0))) ) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_address.wast:122
let $2 = instantiate(`(module
  (memory 1)
  (func (export "v128.store_offset_65521")
    (v128.store offset=65521 (i32.const 0) (v128.const i32x4 0 0 0 0))
  )
)`);

// ./test/core/simd/simd_address.wast:128
assert_trap(
  () => invoke($2, `v128.store_offset_65521`, []),
  `out of bounds memory access`,
);

// ./test/core/simd/simd_address.wast:130
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func   (v128.store offset=-1 (i32.const 0) (v128.const i32x4 0 0 0 0)) ) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_address.wast:143
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (drop (v128.load offset=4294967296 (i32.const 0)))) `,
    ),
  `i32 constant`,
);

// ./test/core/simd/simd_address.wast:151
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (v128.store offset=4294967296 (i32.const 0) (v128.const i32x4 0 0 0 0))) `,
    ),
  `i32 constant`,
);
