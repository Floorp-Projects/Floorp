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

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:4
let $0 = instantiate(`(module
  (func (export "i64x2.extmul_low_i32x4_s") (param v128 v128) (result v128) (i64x2.extmul_low_i32x4_s (local.get 0) (local.get 1)))
  (func (export "i64x2.extmul_high_i32x4_s") (param v128 v128) (result v128) (i64x2.extmul_high_i32x4_s (local.get 0) (local.get 1)))
  (func (export "i64x2.extmul_low_i32x4_u") (param v128 v128) (result v128) (i64x2.extmul_low_i32x4_u (local.get 0) (local.get 1)))
  (func (export "i64x2.extmul_high_i32x4_u") (param v128 v128) (result v128) (i64x2.extmul_high_i32x4_u (local.get 0) (local.get 1)))
)`);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:13
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:16
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:19
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_s`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:22
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:25
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_s`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:28
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:31
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_s`, [
      i32x4([0x3fffffff, 0x3fffffff, 0x3fffffff, 0x3fffffff]),
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
    ]),
  [i64x2([0xfffffffc0000000n, 0xfffffffc0000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:34
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_s`, [
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
    ]),
  [i64x2([0x1000000000000000n, 0x1000000000000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:37
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_s`, [
      i32x4([0xc0000001, 0xc0000001, 0xc0000001, 0xc0000001]),
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
    ]),
  [i64x2([0xfffffffc0000000n, 0xfffffffc0000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:40
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_s`, [
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
    ]),
  [i64x2([0x1000000000000000n, 0x1000000000000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:43
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_s`, [
      i32x4([0xbfffffff, 0xbfffffff, 0xbfffffff, 0xbfffffff]),
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
    ]),
  [i64x2([0x1000000040000000n, 0x1000000040000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:46
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_s`, [
      i32x4([0x7ffffffd, 0x7ffffffd, 0x7ffffffd, 0x7ffffffd]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i64x2([0x7ffffffdn, 0x7ffffffdn])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:49
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_s`, [
      i32x4([0x7ffffffe, 0x7ffffffe, 0x7ffffffe, 0x7ffffffe]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i64x2([0x7ffffffen, 0x7ffffffen])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:52
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_s`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i64x2([0xffffffff80000000n, 0xffffffff80000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:55
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_s`, [
      i32x4([0x80000002, 0x80000002, 0x80000002, 0x80000002]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x7ffffffen, 0x7ffffffen])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:58
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_s`, [
      i32x4([0x80000001, 0x80000001, 0x80000001, 0x80000001]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x7fffffffn, 0x7fffffffn])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:61
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_s`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x80000000n, 0x80000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:64
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_s`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
    ]),
  [i64x2([0x3fffffff00000001n, 0x3fffffff00000001n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:67
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_s`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i64x2([0x4000000000000000n, 0x4000000000000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:70
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_s`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x80000001, 0x80000001, 0x80000001, 0x80000001]),
    ]),
  [i64x2([0x3fffffff80000000n, 0x3fffffff80000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:73
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:76
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:79
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:82
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
    ]),
  [i64x2([0xffffffff80000001n, 0xffffffff80000001n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:85
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i64x2([0x80000000n, 0x80000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:88
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:93
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:96
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:99
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_s`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:102
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:105
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_s`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:108
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:111
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_s`, [
      i32x4([0x3fffffff, 0x3fffffff, 0x3fffffff, 0x3fffffff]),
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
    ]),
  [i64x2([0xfffffffc0000000n, 0xfffffffc0000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:114
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_s`, [
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
    ]),
  [i64x2([0x1000000000000000n, 0x1000000000000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:117
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_s`, [
      i32x4([0xc0000001, 0xc0000001, 0xc0000001, 0xc0000001]),
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
    ]),
  [i64x2([0xfffffffc0000000n, 0xfffffffc0000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:120
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_s`, [
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
    ]),
  [i64x2([0x1000000000000000n, 0x1000000000000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:123
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_s`, [
      i32x4([0xbfffffff, 0xbfffffff, 0xbfffffff, 0xbfffffff]),
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
    ]),
  [i64x2([0x1000000040000000n, 0x1000000040000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:126
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_s`, [
      i32x4([0x7ffffffd, 0x7ffffffd, 0x7ffffffd, 0x7ffffffd]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i64x2([0x7ffffffdn, 0x7ffffffdn])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:129
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_s`, [
      i32x4([0x7ffffffe, 0x7ffffffe, 0x7ffffffe, 0x7ffffffe]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i64x2([0x7ffffffen, 0x7ffffffen])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:132
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_s`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i64x2([0xffffffff80000000n, 0xffffffff80000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:135
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_s`, [
      i32x4([0x80000002, 0x80000002, 0x80000002, 0x80000002]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x7ffffffen, 0x7ffffffen])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:138
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_s`, [
      i32x4([0x80000001, 0x80000001, 0x80000001, 0x80000001]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x7fffffffn, 0x7fffffffn])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:141
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_s`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x80000000n, 0x80000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:144
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_s`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
    ]),
  [i64x2([0x3fffffff00000001n, 0x3fffffff00000001n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:147
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_s`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i64x2([0x4000000000000000n, 0x4000000000000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:150
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_s`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x80000001, 0x80000001, 0x80000001, 0x80000001]),
    ]),
  [i64x2([0x3fffffff80000000n, 0x3fffffff80000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:153
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:156
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:159
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:162
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
    ]),
  [i64x2([0xffffffff80000001n, 0xffffffff80000001n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:165
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i64x2([0x80000000n, 0x80000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:168
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:173
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:176
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:179
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_u`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:182
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:185
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_u`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0xffffffffn, 0xffffffffn])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:188
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0xfffffffe00000001n, 0xfffffffe00000001n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:191
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_u`, [
      i32x4([0x3fffffff, 0x3fffffff, 0x3fffffff, 0x3fffffff]),
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
    ]),
  [i64x2([0xfffffffc0000000n, 0xfffffffc0000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:194
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_u`, [
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
    ]),
  [i64x2([0x1000000000000000n, 0x1000000000000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:197
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_u`, [
      i32x4([0xc0000001, 0xc0000001, 0xc0000001, 0xc0000001]),
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
    ]),
  [i64x2([0x90000000c0000000n, 0x90000000c0000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:200
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_u`, [
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
    ]),
  [i64x2([0x9000000000000000n, 0x9000000000000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:203
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_u`, [
      i32x4([0xbfffffff, 0xbfffffff, 0xbfffffff, 0xbfffffff]),
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
    ]),
  [i64x2([0x8fffffff40000000n, 0x8fffffff40000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:206
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_u`, [
      i32x4([0x7ffffffd, 0x7ffffffd, 0x7ffffffd, 0x7ffffffd]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i64x2([0x7ffffffdn, 0x7ffffffdn])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:209
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_u`, [
      i32x4([0x7ffffffe, 0x7ffffffe, 0x7ffffffe, 0x7ffffffe]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i64x2([0x7ffffffen, 0x7ffffffen])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:212
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_u`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i64x2([0x80000000n, 0x80000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:215
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_u`, [
      i32x4([0x80000002, 0x80000002, 0x80000002, 0x80000002]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x800000017ffffffen, 0x800000017ffffffen])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:218
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_u`, [
      i32x4([0x80000001, 0x80000001, 0x80000001, 0x80000001]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x800000007fffffffn, 0x800000007fffffffn])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:221
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_u`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x7fffffff80000000n, 0x7fffffff80000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:224
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_u`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
    ]),
  [i64x2([0x3fffffff00000001n, 0x3fffffff00000001n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:227
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_u`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i64x2([0x4000000000000000n, 0x4000000000000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:230
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_u`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x80000001, 0x80000001, 0x80000001, 0x80000001]),
    ]),
  [i64x2([0x4000000080000000n, 0x4000000080000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:233
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:236
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i64x2([0xffffffffn, 0xffffffffn])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:239
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0xfffffffe00000001n, 0xfffffffe00000001n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:242
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
    ]),
  [i64x2([0x7ffffffe80000001n, 0x7ffffffe80000001n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:245
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i64x2([0x7fffffff80000000n, 0x7fffffff80000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:248
assert_return(
  () =>
    invoke($0, `i64x2.extmul_low_i32x4_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0xfffffffe00000001n, 0xfffffffe00000001n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:253
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:256
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:259
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_u`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:262
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:265
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_u`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0xffffffffn, 0xffffffffn])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:268
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0xfffffffe00000001n, 0xfffffffe00000001n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:271
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_u`, [
      i32x4([0x3fffffff, 0x3fffffff, 0x3fffffff, 0x3fffffff]),
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
    ]),
  [i64x2([0xfffffffc0000000n, 0xfffffffc0000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:274
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_u`, [
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
    ]),
  [i64x2([0x1000000000000000n, 0x1000000000000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:277
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_u`, [
      i32x4([0xc0000001, 0xc0000001, 0xc0000001, 0xc0000001]),
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
    ]),
  [i64x2([0x90000000c0000000n, 0x90000000c0000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:280
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_u`, [
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
    ]),
  [i64x2([0x9000000000000000n, 0x9000000000000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:283
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_u`, [
      i32x4([0xbfffffff, 0xbfffffff, 0xbfffffff, 0xbfffffff]),
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
    ]),
  [i64x2([0x8fffffff40000000n, 0x8fffffff40000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:286
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_u`, [
      i32x4([0x7ffffffd, 0x7ffffffd, 0x7ffffffd, 0x7ffffffd]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i64x2([0x7ffffffdn, 0x7ffffffdn])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:289
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_u`, [
      i32x4([0x7ffffffe, 0x7ffffffe, 0x7ffffffe, 0x7ffffffe]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i64x2([0x7ffffffen, 0x7ffffffen])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:292
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_u`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i64x2([0x80000000n, 0x80000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:295
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_u`, [
      i32x4([0x80000002, 0x80000002, 0x80000002, 0x80000002]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x800000017ffffffen, 0x800000017ffffffen])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:298
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_u`, [
      i32x4([0x80000001, 0x80000001, 0x80000001, 0x80000001]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x800000007fffffffn, 0x800000007fffffffn])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:301
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_u`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x7fffffff80000000n, 0x7fffffff80000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:304
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_u`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
    ]),
  [i64x2([0x3fffffff00000001n, 0x3fffffff00000001n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:307
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_u`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i64x2([0x4000000000000000n, 0x4000000000000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:310
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_u`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x80000001, 0x80000001, 0x80000001, 0x80000001]),
    ]),
  [i64x2([0x4000000080000000n, 0x4000000080000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:313
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:316
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i64x2([0xffffffffn, 0xffffffffn])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:319
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0xfffffffe00000001n, 0xfffffffe00000001n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:322
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
    ]),
  [i64x2([0x7ffffffe80000001n, 0x7ffffffe80000001n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:325
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i64x2([0x7fffffff80000000n, 0x7fffffff80000000n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:328
assert_return(
  () =>
    invoke($0, `i64x2.extmul_high_i32x4_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0xfffffffe00000001n, 0xfffffffe00000001n])],
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:333
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i64x2.extmul_low_i32x4_s (i32.const 0) (f32.const 0.0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:334
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i64x2.extmul_high_i32x4_s (i32.const 0) (f32.const 0.0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:335
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i64x2.extmul_low_i32x4_u (i32.const 0) (f32.const 0.0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:336
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i64x2.extmul_high_i32x4_u (i32.const 0) (f32.const 0.0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:340
assert_invalid(() =>
  instantiate(`(module
    (func $$i64x2.extmul_low_i32x4_s-1st-arg-empty (result v128)
      (i64x2.extmul_low_i32x4_s (v128.const i64x2 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:348
assert_invalid(() =>
  instantiate(`(module
    (func $$i64x2.extmul_low_i32x4_s-arg-empty (result v128)
      (i64x2.extmul_low_i32x4_s)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:356
assert_invalid(() =>
  instantiate(`(module
    (func $$i64x2.extmul_high_i32x4_s-1st-arg-empty (result v128)
      (i64x2.extmul_high_i32x4_s (v128.const i64x2 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:364
assert_invalid(() =>
  instantiate(`(module
    (func $$i64x2.extmul_high_i32x4_s-arg-empty (result v128)
      (i64x2.extmul_high_i32x4_s)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:372
assert_invalid(() =>
  instantiate(`(module
    (func $$i64x2.extmul_low_i32x4_u-1st-arg-empty (result v128)
      (i64x2.extmul_low_i32x4_u (v128.const i64x2 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:380
assert_invalid(() =>
  instantiate(`(module
    (func $$i64x2.extmul_low_i32x4_u-arg-empty (result v128)
      (i64x2.extmul_low_i32x4_u)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:388
assert_invalid(() =>
  instantiate(`(module
    (func $$i64x2.extmul_high_i32x4_u-1st-arg-empty (result v128)
      (i64x2.extmul_high_i32x4_u (v128.const i64x2 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i64x2_extmul_i32x4.wast:396
assert_invalid(() =>
  instantiate(`(module
    (func $$i64x2.extmul_high_i32x4_u-arg-empty (result v128)
      (i64x2.extmul_high_i32x4_u)
    )
  )`), `type mismatch`);
