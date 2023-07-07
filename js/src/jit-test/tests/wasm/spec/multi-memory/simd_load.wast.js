// |jit-test| skip-if: !wasmSimdEnabled()

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

// ./test/core/simd/simd_load.wast

// ./test/core/simd/simd_load.wast:3
let $0 = instantiate(`(module
  (memory 1)
  (data (i32.const 0) "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f\\00\\01\\02\\03")
  (func (export "v128.load") (result v128)
    (v128.load (i32.const 0))
  )
)`);

// ./test/core/simd/simd_load.wast:11
assert_return(
  () => invoke($0, `v128.load`, []),
  [
    i8x16([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf]),
  ],
);

// ./test/core/simd/simd_load.wast:12
assert_return(
  () => invoke($0, `v128.load`, []),
  [i16x8([0x100, 0x302, 0x504, 0x706, 0x908, 0xb0a, 0xd0c, 0xf0e])],
);

// ./test/core/simd/simd_load.wast:13
assert_return(() => invoke($0, `v128.load`, []), [i32x4([0x3020100, 0x7060504, 0xb0a0908, 0xf0e0d0c])]);

// ./test/core/simd/simd_load.wast:18
let $1 = instantiate(`(module (memory 1)
  (data (i32.const 0) "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f\\00\\01\\02\\03")
  (func (export "as-i8x16_extract_lane_s-value/0") (result i32)
    (i8x16.extract_lane_s 0 (v128.load (i32.const 0)))
  )
)`);

// ./test/core/simd/simd_load.wast:24
assert_return(() => invoke($1, `as-i8x16_extract_lane_s-value/0`, []), [value("i32", 0)]);

// ./test/core/simd/simd_load.wast:26
let $2 = instantiate(`(module (memory 1)
  (data (i32.const 0) "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f\\00\\01\\02\\03")
  (func (export "as-i8x16.eq-operand") (result v128)
    (i8x16.eq (v128.load offset=0 (i32.const 0)) (v128.load offset=16 (i32.const 0)))
  )
)`);

// ./test/core/simd/simd_load.wast:32
assert_return(() => invoke($2, `as-i8x16.eq-operand`, []), [i32x4([0xffffffff, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_load.wast:34
let $3 = instantiate(`(module (memory 1)
  (data (i32.const 0) "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f\\00\\01\\02\\03")
  (func (export "as-v128.not-operand") (result v128)
    (v128.not (v128.load (i32.const 0)))
  )
  (func (export "as-i8x16.all_true-operand") (result i32)
    (i8x16.all_true (v128.load (i32.const 0)))
  )
)`);

// ./test/core/simd/simd_load.wast:43
assert_return(
  () => invoke($3, `as-v128.not-operand`, []),
  [i32x4([0xfcfdfeff, 0xf8f9fafb, 0xf4f5f6f7, 0xf0f1f2f3])],
);

// ./test/core/simd/simd_load.wast:44
assert_return(() => invoke($3, `as-i8x16.all_true-operand`, []), [value("i32", 0)]);

// ./test/core/simd/simd_load.wast:46
let $4 = instantiate(`(module (memory 1)
  (data (offset (i32.const 0))  "\\AA\\AA\\AA\\AA\\AA\\AA\\AA\\AA\\AA\\AA\\AA\\AA\\AA\\AA\\AA\\AA")
  (data (offset (i32.const 16)) "\\BB\\BB\\BB\\BB\\BB\\BB\\BB\\BB\\BB\\BB\\BB\\BB\\BB\\BB\\BB\\BB")
  (data (offset (i32.const 32)) "\\F0\\F0\\F0\\F0\\FF\\FF\\FF\\FF\\00\\00\\00\\00\\FF\\00\\FF\\00")
  (func (export "as-v128.bitselect-operand") (result v128)
    (v128.bitselect (v128.load (i32.const 0)) (v128.load (i32.const 16)) (v128.load (i32.const 32)))
  )
)`);

// ./test/core/simd/simd_load.wast:54
assert_return(
  () => invoke($4, `as-v128.bitselect-operand`, []),
  [i32x4([0xabababab, 0xaaaaaaaa, 0xbbbbbbbb, 0xbbaabbaa])],
);

// ./test/core/simd/simd_load.wast:56
let $5 = instantiate(`(module (memory 1)
  (data (offset (i32.const 0)) "\\AA\\AA\\AA\\AA\\AA\\AA\\AA\\AA\\AA\\AA\\AA\\AA\\AA\\AA\\AA\\AA")
  (func (export "as-i8x16.shl-operand") (result v128)
    (i8x16.shl (v128.load (i32.const 0)) (i32.const 1))
  )
)`);

// ./test/core/simd/simd_load.wast:62
assert_return(
  () => invoke($5, `as-i8x16.shl-operand`, []),
  [i32x4([0x54545454, 0x54545454, 0x54545454, 0x54545454])],
);

// ./test/core/simd/simd_load.wast:64
let $6 = instantiate(`(module (memory 1)
  (data (offset (i32.const 0))  "\\02\\00\\00\\00\\02\\00\\00\\00\\02\\00\\00\\00\\02\\00\\00\\00")
  (data (offset (i32.const 16)) "\\03\\00\\00\\00\\03\\00\\00\\00\\03\\00\\00\\00\\03\\00\\00\\00")
  (func (export "as-add/sub-operand") (result v128)
    ;; 2 2 2 2 + 3 3 3 3 = 5 5 5 5
    ;; 5 5 5 5 - 3 3 3 3 = 2 2 2 2
    (i8x16.sub
      (i8x16.add (v128.load (i32.const 0)) (v128.load (i32.const 16)))
      (v128.load (i32.const 16))
    )
  )
)`);

// ./test/core/simd/simd_load.wast:76
assert_return(() => invoke($6, `as-add/sub-operand`, []), [i32x4([0x2, 0x2, 0x2, 0x2])]);

// ./test/core/simd/simd_load.wast:78
let $7 = instantiate(`(module (memory 1)
  (data (offset (i32.const 0))  "\\00\\00\\00\\43\\00\\00\\80\\3f\\66\\66\\e6\\3f\\00\\00\\80\\bf")  ;; 128 1.0 1.8 -1
  (data (offset (i32.const 16)) "\\00\\00\\00\\40\\00\\00\\00\\40\\00\\00\\00\\40\\00\\00\\00\\40")  ;; 2.0 2.0 2.0 2.0
  (func (export "as-f32x4.mul-operand") (result v128)
    (f32x4.mul (v128.load (i32.const 0)) (v128.load (i32.const 16)))
  )
)`);

// ./test/core/simd/simd_load.wast:85
assert_return(
  () => invoke($7, `as-f32x4.mul-operand`, []),
  [
    new F32x4Pattern(
      value("f32", 256),
      value("f32", 2),
      value("f32", 3.6),
      value("f32", -2),
    ),
  ],
);

// ./test/core/simd/simd_load.wast:87
let $8 = instantiate(`(module (memory 1)
  (data (offset (i32.const 0)) "\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff")  ;; 1111 ...
  (func (export "as-f32x4.abs-operand") (result v128)
    (f32x4.abs (v128.load (i32.const 0)))
  )
)`);

// ./test/core/simd/simd_load.wast:93
assert_return(
  () => invoke($8, `as-f32x4.abs-operand`, []),
  [i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff])],
);

// ./test/core/simd/simd_load.wast:95
let $9 = instantiate(`(module (memory 1)
  (data (offset (i32.const 0)) "\\AA\\AA\\AA\\AA\\AA\\AA\\AA\\AA\\AA\\AA\\AA\\AA\\AA\\AA\\AA\\AA")
  (data (offset (i32.const 16)) "\\02\\00\\00\\00\\02\\00\\00\\00\\02\\00\\00\\00\\02\\00\\00\\00")
  (func (export "as-f32x4.min-operand") (result v128)
    (f32x4.min (v128.load (i32.const 0)) (v128.load offset=16 (i32.const 1)))
  )
)`);

// ./test/core/simd/simd_load.wast:102
assert_return(
  () => invoke($9, `as-f32x4.min-operand`, []),
  [i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa])],
);

// ./test/core/simd/simd_load.wast:104
let $10 = instantiate(`(module (memory 1)
  (data (offset (i32.const 0))  "\\00\\00\\00\\43\\00\\00\\80\\3f\\66\\66\\e6\\3f\\00\\00\\80\\bf")  ;; 128 1.0 1.8 -1
  (func (export "as-i32x4.trunc_sat_f32x4_s-operand") (result v128)
    (i32x4.trunc_sat_f32x4_s (v128.load (i32.const 0)))
  )
)`);

// ./test/core/simd/simd_load.wast:110
assert_return(
  () => invoke($10, `as-i32x4.trunc_sat_f32x4_s-operand`, []),
  [i32x4([0x80, 0x1, 0x1, 0xffffffff])],
);

// ./test/core/simd/simd_load.wast:112
let $11 = instantiate(`(module (memory 1)
  (data (offset (i32.const 0)) "\\02\\00\\00\\00\\02\\00\\00\\00\\02\\00\\00\\00\\02\\00\\00\\00")
  (func (export "as-f32x4.convert_i32x4_u-operand") (result v128)
    (f32x4.convert_i32x4_u (v128.load (i32.const 0)))
  )
)`);

// ./test/core/simd/simd_load.wast:118
assert_return(
  () => invoke($11, `as-f32x4.convert_i32x4_u-operand`, []),
  [
    new F32x4Pattern(
      value("f32", 2),
      value("f32", 2),
      value("f32", 2),
      value("f32", 2),
    ),
  ],
);

// ./test/core/simd/simd_load.wast:120
let $12 = instantiate(`(module (memory 1)
  (data (offset (i32.const 0)) "\\64\\65\\66\\67\\68\\69\\6a\\6b\\6c\\6d\\6e\\6f\\70\\71\\72\\73")  ;; 100 101 102 103 104 105 106 107 108 109 110 111 112 113 114 115
  (data (offset (i32.const 16)) "\\0f\\0e\\0d\\0c\\0b\\0a\\09\\08\\07\\06\\05\\04\\03\\02\\01\\00")  ;;  15  14  13  12  11  10  09  08  07  06  05  04  03  02  01  00
  (func (export "as-i8x16.swizzle-operand") (result v128)
    (i8x16.swizzle (v128.load (i32.const 0)) (v128.load offset=15 (i32.const 1)))
  )
)`);

// ./test/core/simd/simd_load.wast:127
assert_return(
  () => invoke($12, `as-i8x16.swizzle-operand`, []),
  [
    i8x16([0x73, 0x72, 0x71, 0x70, 0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x69, 0x68, 0x67, 0x66, 0x65, 0x64]),
  ],
);

// ./test/core/simd/simd_load.wast:129
let $13 = instantiate(`(module (memory 1)
  (data (i32.const 0) "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f\\00\\01\\02\\03")
  (func (export "as-br-value") (result v128)
    (block (result v128) (br 0 (v128.load (i32.const 0))))
  )
)`);

// ./test/core/simd/simd_load.wast:135
assert_return(() => invoke($13, `as-br-value`, []), [i32x4([0x3020100, 0x7060504, 0xb0a0908, 0xf0e0d0c])]);

// ./test/core/simd/simd_load.wast:140
assert_malformed(
  () => instantiate(`(memory 1) (func (local v128) (drop (v128.load8 (i32.const 0)))) `),
  `unknown operator`,
);

// ./test/core/simd/simd_load.wast:147
assert_malformed(
  () => instantiate(`(memory 1) (func (local v128) (drop (v128.load16 (i32.const 0)))) `),
  `unknown operator`,
);

// ./test/core/simd/simd_load.wast:154
assert_malformed(
  () => instantiate(`(memory 1) (func (local v128) (drop (v128.load32 (i32.const 0)))) `),
  `unknown operator`,
);

// ./test/core/simd/simd_load.wast:165
assert_invalid(
  () => instantiate(`(module (memory 1) (func (local v128) (drop (v128.load (f32.const 0)))))`),
  `type mismatch`,
);

// ./test/core/simd/simd_load.wast:169
assert_invalid(
  () => instantiate(`(module (memory 1) (func (local v128) (block (br_if 0 (v128.load (i32.const 0))))))`),
  `type mismatch`,
);

// ./test/core/simd/simd_load.wast:173
assert_invalid(
  () => instantiate(`(module (memory 1) (func (local v128) (v128.load (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/simd/simd_load.wast:181
assert_invalid(
  () => instantiate(`(module (memory 1) (func (drop (v128.load (local.get 2)))))`),
  `unknown local 2`,
);

// ./test/core/simd/simd_load.wast:185
assert_invalid(
  () => instantiate(`(module (memory 1) (func (drop (v128.load))))`),
  `type mismatch`,
);
