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

// ./test/core/simd/simd_i32x4_arith2.wast

// ./test/core/simd/simd_i32x4_arith2.wast:3
let $0 = instantiate(`(module
  (func (export "i32x4.min_s") (param v128 v128) (result v128) (i32x4.min_s (local.get 0) (local.get 1)))
  (func (export "i32x4.min_u") (param v128 v128) (result v128) (i32x4.min_u (local.get 0) (local.get 1)))
  (func (export "i32x4.max_s") (param v128 v128) (result v128) (i32x4.max_s (local.get 0) (local.get 1)))
  (func (export "i32x4.max_u") (param v128 v128) (result v128) (i32x4.max_u (local.get 0) (local.get 1)))
  (func (export "i32x4.abs") (param v128) (result v128) (i32x4.abs (local.get 0)))
  (func (export "i32x4.min_s_with_const_0") (result v128) (i32x4.min_s (v128.const i32x4 -2147483648 2147483647 1073741824 4294967295) (v128.const i32x4 4294967295 1073741824 2147483647 -2147483648)))
  (func (export "i32x4.min_s_with_const_1") (result v128) (i32x4.min_s (v128.const i32x4 0 1 2 3) (v128.const i32x4 3 2 1 0)))
  (func (export "i32x4.min_u_with_const_2") (result v128) (i32x4.min_u (v128.const i32x4 -2147483648 2147483647 1073741824 4294967295) (v128.const i32x4 4294967295 1073741824 2147483647 -2147483648)))
  (func (export "i32x4.min_u_with_const_3") (result v128) (i32x4.min_u (v128.const i32x4 0 1 2 3) (v128.const i32x4 3 2 1 0)))
  (func (export "i32x4.max_s_with_const_4") (result v128) (i32x4.max_s (v128.const i32x4 -2147483648 2147483647 1073741824 4294967295) (v128.const i32x4 4294967295 1073741824 2147483647 -2147483648)))
  (func (export "i32x4.max_s_with_const_5") (result v128) (i32x4.max_s (v128.const i32x4 0 1 2 3) (v128.const i32x4 3 2 1 0)))
  (func (export "i32x4.max_u_with_const_6") (result v128) (i32x4.max_u (v128.const i32x4 -2147483648 2147483647 1073741824 4294967295) (v128.const i32x4 4294967295 1073741824 2147483647 -2147483648)))
  (func (export "i32x4.max_u_with_const_7") (result v128) (i32x4.max_u (v128.const i32x4 0 1 2 3) (v128.const i32x4 3 2 1 0)))
  (func (export "i32x4.abs_with_const_8") (result v128) (i32x4.abs (v128.const i32x4 -2147483648 2147483647 1073741824 4294967295)))
  (func (export "i32x4.min_s_with_const_9") (param v128) (result v128) (i32x4.min_s (local.get 0) (v128.const i32x4 -2147483648 2147483647 1073741824 4294967295)))
  (func (export "i32x4.min_s_with_const_10") (param v128) (result v128) (i32x4.min_s (local.get 0) (v128.const i32x4 0 1 2 3)))
  (func (export "i32x4.min_u_with_const_11") (param v128) (result v128) (i32x4.min_u (local.get 0) (v128.const i32x4 -2147483648 2147483647 1073741824 4294967295)))
  (func (export "i32x4.min_u_with_const_12") (param v128) (result v128) (i32x4.min_u (local.get 0) (v128.const i32x4 0 1 2 3)))
  (func (export "i32x4.max_s_with_const_13") (param v128) (result v128) (i32x4.max_s (local.get 0) (v128.const i32x4 -2147483648 2147483647 1073741824 4294967295)))
  (func (export "i32x4.max_s_with_const_14") (param v128) (result v128) (i32x4.max_s (local.get 0) (v128.const i32x4 0 1 2 3)))
  (func (export "i32x4.max_u_with_const_15") (param v128) (result v128) (i32x4.max_u (local.get 0) (v128.const i32x4 -2147483648 2147483647 1073741824 4294967295)))
  (func (export "i32x4.max_u_with_const_16") (param v128) (result v128) (i32x4.max_u (local.get 0) (v128.const i32x4 0 1 2 3)))
)`);

// ./test/core/simd/simd_i32x4_arith2.wast:28
assert_return(
  () =>
    invoke($0, `i32x4.min_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:31
assert_return(
  () =>
    invoke($0, `i32x4.min_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:34
assert_return(
  () =>
    invoke($0, `i32x4.min_s`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0xffffffff, 0x0, 0xffffffff]),
    ]),
  [i32x4([0x0, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:37
assert_return(
  () =>
    invoke($0, `i32x4.min_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:40
assert_return(
  () =>
    invoke($0, `i32x4.min_s`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:43
assert_return(
  () =>
    invoke($0, `i32x4.min_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:46
assert_return(
  () =>
    invoke($0, `i32x4.min_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x80, 0x80, 0x80, 0x80]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:49
assert_return(
  () =>
    invoke($0, `i32x4.min_s`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:52
assert_return(
  () =>
    invoke($0, `i32x4.min_s`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:55
assert_return(
  () =>
    invoke($0, `i32x4.min_s`, [
      i32x4([0x7b, 0x7b, 0x7b, 0x7b]),
      i32x4([0x7b, 0x7b, 0x7b, 0x7b]),
    ]),
  [i32x4([0x7b, 0x7b, 0x7b, 0x7b])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:58
assert_return(
  () =>
    invoke($0, `i32x4.min_s`, [
      i32x4([0x80, 0x80, 0x80, 0x80]),
      i32x4([0x80, 0x80, 0x80, 0x80]),
    ]),
  [i32x4([0x80, 0x80, 0x80, 0x80])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:61
assert_return(
  () =>
    invoke($0, `i32x4.min_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:64
assert_return(
  () =>
    invoke($0, `i32x4.min_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:67
assert_return(
  () =>
    invoke($0, `i32x4.min_u`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0xffffffff, 0x0, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:70
assert_return(
  () =>
    invoke($0, `i32x4.min_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:73
assert_return(
  () =>
    invoke($0, `i32x4.min_u`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:76
assert_return(
  () =>
    invoke($0, `i32x4.min_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:79
assert_return(
  () =>
    invoke($0, `i32x4.min_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x80, 0x80, 0x80, 0x80]),
    ]),
  [i32x4([0x80, 0x80, 0x80, 0x80])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:82
assert_return(
  () =>
    invoke($0, `i32x4.min_u`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:85
assert_return(
  () =>
    invoke($0, `i32x4.min_u`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:88
assert_return(
  () =>
    invoke($0, `i32x4.min_u`, [
      i32x4([0x7b, 0x7b, 0x7b, 0x7b]),
      i32x4([0x7b, 0x7b, 0x7b, 0x7b]),
    ]),
  [i32x4([0x7b, 0x7b, 0x7b, 0x7b])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:91
assert_return(
  () =>
    invoke($0, `i32x4.min_u`, [
      i32x4([0x80, 0x80, 0x80, 0x80]),
      i32x4([0x80, 0x80, 0x80, 0x80]),
    ]),
  [i32x4([0x80, 0x80, 0x80, 0x80])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:94
assert_return(
  () =>
    invoke($0, `i32x4.max_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:97
assert_return(
  () =>
    invoke($0, `i32x4.max_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:100
assert_return(
  () =>
    invoke($0, `i32x4.max_s`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0xffffffff, 0x0, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:103
assert_return(
  () =>
    invoke($0, `i32x4.max_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:106
assert_return(
  () =>
    invoke($0, `i32x4.max_s`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:109
assert_return(
  () =>
    invoke($0, `i32x4.max_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:112
assert_return(
  () =>
    invoke($0, `i32x4.max_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x80, 0x80, 0x80, 0x80]),
    ]),
  [i32x4([0x80, 0x80, 0x80, 0x80])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:115
assert_return(
  () =>
    invoke($0, `i32x4.max_s`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:118
assert_return(
  () =>
    invoke($0, `i32x4.max_s`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:121
assert_return(
  () =>
    invoke($0, `i32x4.max_s`, [
      i32x4([0x7b, 0x7b, 0x7b, 0x7b]),
      i32x4([0x7b, 0x7b, 0x7b, 0x7b]),
    ]),
  [i32x4([0x7b, 0x7b, 0x7b, 0x7b])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:124
assert_return(
  () =>
    invoke($0, `i32x4.max_s`, [
      i32x4([0x80, 0x80, 0x80, 0x80]),
      i32x4([0x80, 0x80, 0x80, 0x80]),
    ]),
  [i32x4([0x80, 0x80, 0x80, 0x80])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:127
assert_return(
  () =>
    invoke($0, `i32x4.max_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:130
assert_return(
  () =>
    invoke($0, `i32x4.max_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:133
assert_return(
  () =>
    invoke($0, `i32x4.max_u`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0xffffffff, 0x0, 0xffffffff]),
    ]),
  [i32x4([0x0, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:136
assert_return(
  () =>
    invoke($0, `i32x4.max_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:139
assert_return(
  () =>
    invoke($0, `i32x4.max_u`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:142
assert_return(
  () =>
    invoke($0, `i32x4.max_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:145
assert_return(
  () =>
    invoke($0, `i32x4.max_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x80, 0x80, 0x80, 0x80]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:148
assert_return(
  () =>
    invoke($0, `i32x4.max_u`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:151
assert_return(
  () =>
    invoke($0, `i32x4.max_u`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:154
assert_return(
  () =>
    invoke($0, `i32x4.max_u`, [
      i32x4([0x7b, 0x7b, 0x7b, 0x7b]),
      i32x4([0x7b, 0x7b, 0x7b, 0x7b]),
    ]),
  [i32x4([0x7b, 0x7b, 0x7b, 0x7b])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:157
assert_return(
  () =>
    invoke($0, `i32x4.max_u`, [
      i32x4([0x80, 0x80, 0x80, 0x80]),
      i32x4([0x80, 0x80, 0x80, 0x80]),
    ]),
  [i32x4([0x80, 0x80, 0x80, 0x80])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:160
assert_return(() => invoke($0, `i32x4.abs`, [i32x4([0x1, 0x1, 0x1, 0x1])]), [
  i32x4([0x1, 0x1, 0x1, 0x1]),
]);

// ./test/core/simd/simd_i32x4_arith2.wast:162
assert_return(
  () =>
    invoke($0, `i32x4.abs`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:164
assert_return(
  () =>
    invoke($0, `i32x4.abs`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:166
assert_return(
  () =>
    invoke($0, `i32x4.abs`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:168
assert_return(
  () =>
    invoke($0, `i32x4.abs`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:170
assert_return(
  () =>
    invoke($0, `i32x4.abs`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:172
assert_return(
  () =>
    invoke($0, `i32x4.abs`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:174
assert_return(
  () =>
    invoke($0, `i32x4.abs`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:176
assert_return(
  () => invoke($0, `i32x4.abs`, [i32x4([0x7b, 0x7b, 0x7b, 0x7b])]),
  [i32x4([0x7b, 0x7b, 0x7b, 0x7b])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:178
assert_return(
  () =>
    invoke($0, `i32x4.abs`, [
      i32x4([0xffffff85, 0xffffff85, 0xffffff85, 0xffffff85]),
    ]),
  [i32x4([0x7b, 0x7b, 0x7b, 0x7b])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:180
assert_return(
  () => invoke($0, `i32x4.abs`, [i32x4([0x80, 0x80, 0x80, 0x80])]),
  [i32x4([0x80, 0x80, 0x80, 0x80])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:182
assert_return(
  () =>
    invoke($0, `i32x4.abs`, [
      i32x4([0xffffff80, 0xffffff80, 0xffffff80, 0xffffff80]),
    ]),
  [i32x4([0x80, 0x80, 0x80, 0x80])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:184
assert_return(
  () => invoke($0, `i32x4.abs`, [i32x4([0x80, 0x80, 0x80, 0x80])]),
  [i32x4([0x80, 0x80, 0x80, 0x80])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:186
assert_return(
  () =>
    invoke($0, `i32x4.abs`, [
      i32x4([0xffffff80, 0xffffff80, 0xffffff80, 0xffffff80]),
    ]),
  [i32x4([0x80, 0x80, 0x80, 0x80])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:190
assert_return(() => invoke($0, `i32x4.min_s_with_const_0`, []), [
  i32x4([0x80000000, 0x40000000, 0x40000000, 0x80000000]),
]);

// ./test/core/simd/simd_i32x4_arith2.wast:191
assert_return(() => invoke($0, `i32x4.min_s_with_const_1`, []), [
  i32x4([0x0, 0x1, 0x1, 0x0]),
]);

// ./test/core/simd/simd_i32x4_arith2.wast:192
assert_return(() => invoke($0, `i32x4.min_u_with_const_2`, []), [
  i32x4([0x80000000, 0x40000000, 0x40000000, 0x80000000]),
]);

// ./test/core/simd/simd_i32x4_arith2.wast:193
assert_return(() => invoke($0, `i32x4.min_u_with_const_3`, []), [
  i32x4([0x0, 0x1, 0x1, 0x0]),
]);

// ./test/core/simd/simd_i32x4_arith2.wast:194
assert_return(() => invoke($0, `i32x4.max_s_with_const_4`, []), [
  i32x4([0xffffffff, 0x7fffffff, 0x7fffffff, 0xffffffff]),
]);

// ./test/core/simd/simd_i32x4_arith2.wast:195
assert_return(() => invoke($0, `i32x4.max_s_with_const_5`, []), [
  i32x4([0x3, 0x2, 0x2, 0x3]),
]);

// ./test/core/simd/simd_i32x4_arith2.wast:196
assert_return(() => invoke($0, `i32x4.max_u_with_const_6`, []), [
  i32x4([0xffffffff, 0x7fffffff, 0x7fffffff, 0xffffffff]),
]);

// ./test/core/simd/simd_i32x4_arith2.wast:197
assert_return(() => invoke($0, `i32x4.max_u_with_const_7`, []), [
  i32x4([0x3, 0x2, 0x2, 0x3]),
]);

// ./test/core/simd/simd_i32x4_arith2.wast:198
assert_return(() => invoke($0, `i32x4.abs_with_const_8`, []), [
  i32x4([0x80000000, 0x7fffffff, 0x40000000, 0x1]),
]);

// ./test/core/simd/simd_i32x4_arith2.wast:201
assert_return(
  () =>
    invoke($0, `i32x4.min_s_with_const_9`, [
      i32x4([0xffffffff, 0x40000000, 0x7fffffff, 0x80000000]),
    ]),
  [i32x4([0x80000000, 0x40000000, 0x40000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:203
assert_return(
  () => invoke($0, `i32x4.min_s_with_const_10`, [i32x4([0x3, 0x2, 0x1, 0x0])]),
  [i32x4([0x0, 0x1, 0x1, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:205
assert_return(
  () =>
    invoke($0, `i32x4.min_u_with_const_11`, [
      i32x4([0xffffffff, 0x40000000, 0x7fffffff, 0x80000000]),
    ]),
  [i32x4([0x80000000, 0x40000000, 0x40000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:207
assert_return(
  () => invoke($0, `i32x4.min_u_with_const_12`, [i32x4([0x3, 0x2, 0x1, 0x0])]),
  [i32x4([0x0, 0x1, 0x1, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:209
assert_return(
  () =>
    invoke($0, `i32x4.max_s_with_const_13`, [
      i32x4([0xffffffff, 0x40000000, 0x7fffffff, 0x80000000]),
    ]),
  [i32x4([0xffffffff, 0x7fffffff, 0x7fffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:211
assert_return(
  () => invoke($0, `i32x4.max_s_with_const_14`, [i32x4([0x3, 0x2, 0x1, 0x0])]),
  [i32x4([0x3, 0x2, 0x2, 0x3])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:213
assert_return(
  () =>
    invoke($0, `i32x4.max_u_with_const_15`, [
      i32x4([0xffffffff, 0x40000000, 0x7fffffff, 0x80000000]),
    ]),
  [i32x4([0xffffffff, 0x7fffffff, 0x7fffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:215
assert_return(
  () => invoke($0, `i32x4.max_u_with_const_16`, [i32x4([0x3, 0x2, 0x1, 0x0])]),
  [i32x4([0x3, 0x2, 0x2, 0x3])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:219
assert_return(
  () =>
    invoke($0, `i32x4.min_s`, [
      i32x4([0x80000000, 0x7fffffff, 0x40000000, 0xffffffff]),
      i32x4([0xffffffff, 0x40000000, 0x7fffffff, 0x80000000]),
    ]),
  [i32x4([0x80000000, 0x40000000, 0x40000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:222
assert_return(
  () =>
    invoke($0, `i32x4.min_s`, [
      i32x4([0x0, 0x1, 0x2, 0x80]),
      i32x4([0x0, 0x2, 0x1, 0x80]),
    ]),
  [i32x4([0x0, 0x1, 0x1, 0x80])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:225
assert_return(
  () =>
    invoke($0, `i32x4.min_u`, [
      i32x4([0x80000000, 0x7fffffff, 0x40000000, 0xffffffff]),
      i32x4([0xffffffff, 0x40000000, 0x7fffffff, 0x80000000]),
    ]),
  [i32x4([0x80000000, 0x40000000, 0x40000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:228
assert_return(
  () =>
    invoke($0, `i32x4.min_u`, [
      i32x4([0x0, 0x1, 0x2, 0x80]),
      i32x4([0x0, 0x2, 0x1, 0x80]),
    ]),
  [i32x4([0x0, 0x1, 0x1, 0x80])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:231
assert_return(
  () =>
    invoke($0, `i32x4.max_s`, [
      i32x4([0x80000000, 0x7fffffff, 0x40000000, 0xffffffff]),
      i32x4([0xffffffff, 0x40000000, 0x7fffffff, 0x80000000]),
    ]),
  [i32x4([0xffffffff, 0x7fffffff, 0x7fffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:234
assert_return(
  () =>
    invoke($0, `i32x4.max_s`, [
      i32x4([0x0, 0x1, 0x2, 0x80]),
      i32x4([0x0, 0x2, 0x1, 0x80]),
    ]),
  [i32x4([0x0, 0x2, 0x2, 0x80])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:237
assert_return(
  () =>
    invoke($0, `i32x4.max_u`, [
      i32x4([0x80000000, 0x7fffffff, 0x40000000, 0xffffffff]),
      i32x4([0xffffffff, 0x40000000, 0x7fffffff, 0x80000000]),
    ]),
  [i32x4([0xffffffff, 0x7fffffff, 0x7fffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:240
assert_return(
  () =>
    invoke($0, `i32x4.max_u`, [
      i32x4([0x0, 0x1, 0x2, 0x80]),
      i32x4([0x0, 0x2, 0x1, 0x80]),
    ]),
  [i32x4([0x0, 0x2, 0x2, 0x80])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:243
assert_return(
  () =>
    invoke($0, `i32x4.abs`, [
      i32x4([0x80000000, 0x7fffffff, 0x40000000, 0xffffffff]),
    ]),
  [i32x4([0x80000000, 0x7fffffff, 0x40000000, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:247
assert_return(
  () =>
    invoke($0, `i32x4.min_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:250
assert_return(
  () =>
    invoke($0, `i32x4.min_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:253
assert_return(
  () =>
    invoke($0, `i32x4.min_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:256
assert_return(
  () =>
    invoke($0, `i32x4.min_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:259
assert_return(
  () =>
    invoke($0, `i32x4.max_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:262
assert_return(
  () =>
    invoke($0, `i32x4.max_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:265
assert_return(
  () =>
    invoke($0, `i32x4.max_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:268
assert_return(
  () =>
    invoke($0, `i32x4.max_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:271
assert_return(() => invoke($0, `i32x4.abs`, [i32x4([0x0, 0x0, 0x0, 0x0])]), [
  i32x4([0x0, 0x0, 0x0, 0x0]),
]);

// ./test/core/simd/simd_i32x4_arith2.wast:273
assert_return(() => invoke($0, `i32x4.abs`, [i32x4([0x0, 0x0, 0x0, 0x0])]), [
  i32x4([0x0, 0x0, 0x0, 0x0]),
]);

// ./test/core/simd/simd_i32x4_arith2.wast:275
assert_return(() => invoke($0, `i32x4.abs`, [i32x4([0x0, 0x0, 0x0, 0x0])]), [
  i32x4([0x0, 0x0, 0x0, 0x0]),
]);

// ./test/core/simd/simd_i32x4_arith2.wast:277
assert_return(() => invoke($0, `i32x4.abs`, [i32x4([0x0, 0x0, 0x0, 0x0])]), [
  i32x4([0x0, 0x0, 0x0, 0x0]),
]);

// ./test/core/simd/simd_i32x4_arith2.wast:281
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (f32x4.min_s (v128.const i32x4 0 0 0 0) (v128.const i32x4 1 1 1 1))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_i32x4_arith2.wast:282
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (f32x4.min_u (v128.const i32x4 0 0 0 0) (v128.const i32x4 1 1 1 1))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_i32x4_arith2.wast:283
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (f32x4.max_s (v128.const i32x4 0 0 0 0) (v128.const i32x4 1 1 1 1))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_i32x4_arith2.wast:284
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (f32x4.max_u (v128.const i32x4 0 0 0 0) (v128.const i32x4 1 1 1 1))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_i32x4_arith2.wast:285
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i64x2.min_s (v128.const i32x4 0 0 0 0) (v128.const i32x4 1 1 1 1))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_i32x4_arith2.wast:286
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i64x2.min_u (v128.const i32x4 0 0 0 0) (v128.const i32x4 1 1 1 1))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_i32x4_arith2.wast:287
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i64x2.max_s (v128.const i32x4 0 0 0 0) (v128.const i32x4 1 1 1 1))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_i32x4_arith2.wast:288
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i64x2.max_u (v128.const i32x4 0 0 0 0) (v128.const i32x4 1 1 1 1))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_i32x4_arith2.wast:289
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (f64x2.min_s (v128.const i32x4 0 0 0 0) (v128.const i32x4 1 1 1 1))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_i32x4_arith2.wast:290
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (f64x2.min_u (v128.const i32x4 0 0 0 0) (v128.const i32x4 1 1 1 1))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_i32x4_arith2.wast:291
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (f64x2.max_s (v128.const i32x4 0 0 0 0) (v128.const i32x4 1 1 1 1))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_i32x4_arith2.wast:292
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (f64x2.max_u (v128.const i32x4 0 0 0 0) (v128.const i32x4 1 1 1 1))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_i32x4_arith2.wast:295
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.min_s (i32.const 0) (f32.const 0.0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i32x4_arith2.wast:296
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.min_u (i32.const 0) (f32.const 0.0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i32x4_arith2.wast:297
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.max_s (i32.const 0) (f32.const 0.0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i32x4_arith2.wast:298
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.max_u (i32.const 0) (f32.const 0.0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i32x4_arith2.wast:299
assert_invalid(
  () =>
    instantiate(`(module (func (result v128) (i32x4.abs (f32.const 0.0))))`),
  `type mismatch`,
);

// ./test/core/simd/simd_i32x4_arith2.wast:303
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.min_s-1st-arg-empty (result v128)
      (i32x4.min_s (v128.const i32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_arith2.wast:311
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.min_s-arg-empty (result v128)
      (i32x4.min_s)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_arith2.wast:319
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.min_u-1st-arg-empty (result v128)
      (i32x4.min_u (v128.const i32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_arith2.wast:327
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.min_u-arg-empty (result v128)
      (i32x4.min_u)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_arith2.wast:335
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.max_s-1st-arg-empty (result v128)
      (i32x4.max_s (v128.const i32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_arith2.wast:343
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.max_s-arg-empty (result v128)
      (i32x4.max_s)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_arith2.wast:351
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.max_u-1st-arg-empty (result v128)
      (i32x4.max_u (v128.const i32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_arith2.wast:359
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.max_u-arg-empty (result v128)
      (i32x4.max_u)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_arith2.wast:367
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.abs-arg-empty (result v128)
      (i32x4.abs)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_arith2.wast:377
let $1 = instantiate(`(module
  (func (export "i32x4.min_s-i32x4.max_u") (param v128 v128 v128) (result v128) (i32x4.min_s (i32x4.max_u (local.get 0) (local.get 1))(local.get 2)))
  (func (export "i32x4.min_s-i32x4.max_s") (param v128 v128 v128) (result v128) (i32x4.min_s (i32x4.max_s (local.get 0) (local.get 1))(local.get 2)))
  (func (export "i32x4.min_s-i32x4.min_u") (param v128 v128 v128) (result v128) (i32x4.min_s (i32x4.min_u (local.get 0) (local.get 1))(local.get 2)))
  (func (export "i32x4.min_s-i32x4.min_s") (param v128 v128 v128) (result v128) (i32x4.min_s (i32x4.min_s (local.get 0) (local.get 1))(local.get 2)))
  (func (export "i32x4.min_s-i32x4.abs") (param v128 v128) (result v128) (i32x4.min_s (i32x4.abs (local.get 0))(local.get 1)))
  (func (export "i32x4.abs-i32x4.min_s") (param v128 v128) (result v128) (i32x4.abs (i32x4.min_s (local.get 0) (local.get 1))))
  (func (export "i32x4.min_u-i32x4.max_u") (param v128 v128 v128) (result v128) (i32x4.min_u (i32x4.max_u (local.get 0) (local.get 1))(local.get 2)))
  (func (export "i32x4.min_u-i32x4.max_s") (param v128 v128 v128) (result v128) (i32x4.min_u (i32x4.max_s (local.get 0) (local.get 1))(local.get 2)))
  (func (export "i32x4.min_u-i32x4.min_u") (param v128 v128 v128) (result v128) (i32x4.min_u (i32x4.min_u (local.get 0) (local.get 1))(local.get 2)))
  (func (export "i32x4.min_u-i32x4.min_s") (param v128 v128 v128) (result v128) (i32x4.min_u (i32x4.min_s (local.get 0) (local.get 1))(local.get 2)))
  (func (export "i32x4.min_u-i32x4.abs") (param v128 v128) (result v128) (i32x4.min_u (i32x4.abs (local.get 0))(local.get 1)))
  (func (export "i32x4.abs-i32x4.min_u") (param v128 v128) (result v128) (i32x4.abs (i32x4.min_u (local.get 0) (local.get 1))))
  (func (export "i32x4.max_s-i32x4.max_u") (param v128 v128 v128) (result v128) (i32x4.max_s (i32x4.max_u (local.get 0) (local.get 1))(local.get 2)))
  (func (export "i32x4.max_s-i32x4.max_s") (param v128 v128 v128) (result v128) (i32x4.max_s (i32x4.max_s (local.get 0) (local.get 1))(local.get 2)))
  (func (export "i32x4.max_s-i32x4.min_u") (param v128 v128 v128) (result v128) (i32x4.max_s (i32x4.min_u (local.get 0) (local.get 1))(local.get 2)))
  (func (export "i32x4.max_s-i32x4.min_s") (param v128 v128 v128) (result v128) (i32x4.max_s (i32x4.min_s (local.get 0) (local.get 1))(local.get 2)))
  (func (export "i32x4.max_s-i32x4.abs") (param v128 v128) (result v128) (i32x4.max_s (i32x4.abs (local.get 0))(local.get 1)))
  (func (export "i32x4.abs-i32x4.max_s") (param v128 v128) (result v128) (i32x4.abs (i32x4.max_s (local.get 0) (local.get 1))))
  (func (export "i32x4.max_u-i32x4.max_u") (param v128 v128 v128) (result v128) (i32x4.max_u (i32x4.max_u (local.get 0) (local.get 1))(local.get 2)))
  (func (export "i32x4.max_u-i32x4.max_s") (param v128 v128 v128) (result v128) (i32x4.max_u (i32x4.max_s (local.get 0) (local.get 1))(local.get 2)))
  (func (export "i32x4.max_u-i32x4.min_u") (param v128 v128 v128) (result v128) (i32x4.max_u (i32x4.min_u (local.get 0) (local.get 1))(local.get 2)))
  (func (export "i32x4.max_u-i32x4.min_s") (param v128 v128 v128) (result v128) (i32x4.max_u (i32x4.min_s (local.get 0) (local.get 1))(local.get 2)))
  (func (export "i32x4.max_u-i32x4.abs") (param v128 v128) (result v128) (i32x4.max_u (i32x4.abs (local.get 0))(local.get 1)))
  (func (export "i32x4.abs-i32x4.max_u") (param v128 v128) (result v128) (i32x4.abs (i32x4.max_u (local.get 0) (local.get 1))))
  (func (export "i32x4.abs-i32x4.abs") (param v128) (result v128) (i32x4.abs (i32x4.abs (local.get 0))))
)`);

// ./test/core/simd/simd_i32x4_arith2.wast:405
assert_return(
  () =>
    invoke($1, `i32x4.min_s-i32x4.max_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x2, 0x2, 0x2, 0x2]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:409
assert_return(
  () =>
    invoke($1, `i32x4.min_s-i32x4.max_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x2, 0x2, 0x2, 0x2]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:413
assert_return(
  () =>
    invoke($1, `i32x4.min_s-i32x4.min_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x2, 0x2, 0x2, 0x2]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:417
assert_return(
  () =>
    invoke($1, `i32x4.min_s-i32x4.min_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x2, 0x2, 0x2, 0x2]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:421
assert_return(
  () =>
    invoke($1, `i32x4.min_s-i32x4.abs`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:424
assert_return(
  () =>
    invoke($1, `i32x4.abs-i32x4.min_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:427
assert_return(
  () =>
    invoke($1, `i32x4.min_u-i32x4.max_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x2, 0x2, 0x2, 0x2]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:431
assert_return(
  () =>
    invoke($1, `i32x4.min_u-i32x4.max_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x2, 0x2, 0x2, 0x2]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:435
assert_return(
  () =>
    invoke($1, `i32x4.min_u-i32x4.min_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x2, 0x2, 0x2, 0x2]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:439
assert_return(
  () =>
    invoke($1, `i32x4.min_u-i32x4.min_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x2, 0x2, 0x2, 0x2]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:443
assert_return(
  () =>
    invoke($1, `i32x4.min_u-i32x4.abs`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:446
assert_return(
  () =>
    invoke($1, `i32x4.abs-i32x4.min_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:449
assert_return(
  () =>
    invoke($1, `i32x4.max_s-i32x4.max_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x2, 0x2, 0x2, 0x2]),
    ]),
  [i32x4([0x2, 0x2, 0x2, 0x2])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:453
assert_return(
  () =>
    invoke($1, `i32x4.max_s-i32x4.max_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x2, 0x2, 0x2, 0x2]),
    ]),
  [i32x4([0x2, 0x2, 0x2, 0x2])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:457
assert_return(
  () =>
    invoke($1, `i32x4.max_s-i32x4.min_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x2, 0x2, 0x2, 0x2]),
    ]),
  [i32x4([0x2, 0x2, 0x2, 0x2])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:461
assert_return(
  () =>
    invoke($1, `i32x4.max_s-i32x4.min_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x2, 0x2, 0x2, 0x2]),
    ]),
  [i32x4([0x2, 0x2, 0x2, 0x2])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:465
assert_return(
  () =>
    invoke($1, `i32x4.max_s-i32x4.abs`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:468
assert_return(
  () =>
    invoke($1, `i32x4.abs-i32x4.max_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:471
assert_return(
  () =>
    invoke($1, `i32x4.max_u-i32x4.max_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x2, 0x2, 0x2, 0x2]),
    ]),
  [i32x4([0x2, 0x2, 0x2, 0x2])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:475
assert_return(
  () =>
    invoke($1, `i32x4.max_u-i32x4.max_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x2, 0x2, 0x2, 0x2]),
    ]),
  [i32x4([0x2, 0x2, 0x2, 0x2])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:479
assert_return(
  () =>
    invoke($1, `i32x4.max_u-i32x4.min_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x2, 0x2, 0x2, 0x2]),
    ]),
  [i32x4([0x2, 0x2, 0x2, 0x2])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:483
assert_return(
  () =>
    invoke($1, `i32x4.max_u-i32x4.min_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x2, 0x2, 0x2, 0x2]),
    ]),
  [i32x4([0x2, 0x2, 0x2, 0x2])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:487
assert_return(
  () =>
    invoke($1, `i32x4.max_u-i32x4.abs`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:490
assert_return(
  () =>
    invoke($1, `i32x4.abs-i32x4.max_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith2.wast:493
assert_return(
  () =>
    invoke($1, `i32x4.abs-i32x4.abs`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);
