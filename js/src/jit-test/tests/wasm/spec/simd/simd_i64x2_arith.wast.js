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

// ./test/core/simd/simd_i64x2_arith.wast

// ./test/core/simd/simd_i64x2_arith.wast:4
let $0 = instantiate(`(module
  (func (export "i64x2.add") (param v128 v128) (result v128) (i64x2.add (local.get 0) (local.get 1)))
  (func (export "i64x2.sub") (param v128 v128) (result v128) (i64x2.sub (local.get 0) (local.get 1)))
  (func (export "i64x2.mul") (param v128 v128) (result v128) (i64x2.mul (local.get 0) (local.get 1)))
  (func (export "i64x2.neg") (param v128) (result v128) (i64x2.neg (local.get 0)))
)`);

// ./test/core/simd/simd_i64x2_arith.wast:13
assert_return(
  () => invoke($0, `i64x2.add`, [i64x2([0x0n, 0x0n]), i64x2([0x0n, 0x0n])]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:16
assert_return(
  () => invoke($0, `i64x2.add`, [i64x2([0x0n, 0x0n]), i64x2([0x1n, 0x1n])]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:19
assert_return(
  () => invoke($0, `i64x2.add`, [i64x2([0x1n, 0x1n]), i64x2([0x1n, 0x1n])]),
  [i64x2([0x2n, 0x2n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:22
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x0n, 0x0n]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:25
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x1n, 0x1n]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:28
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0xfffffffffffffffen, 0xfffffffffffffffen])],
);

// ./test/core/simd/simd_i64x2_arith.wast:31
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x3fffffffffffffffn, 0x3fffffffffffffffn]),
      i64x2([0x4000000000000000n, 0x4000000000000000n]),
    ]),
  [i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:34
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x4000000000000000n, 0x4000000000000000n]),
      i64x2([0x4000000000000000n, 0x4000000000000000n]),
    ]),
  [i64x2([0x8000000000000000n, 0x8000000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:37
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0xc000000000000001n, 0xc000000000000001n]),
      i64x2([0xc000000000000000n, 0xc000000000000000n]),
    ]),
  [i64x2([0x8000000000000001n, 0x8000000000000001n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:40
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0xc000000000000000n, 0xc000000000000000n]),
      i64x2([0xc000000000000000n, 0xc000000000000000n]),
    ]),
  [i64x2([0x8000000000000000n, 0x8000000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:43
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0xbfffffffffffffffn, 0xbfffffffffffffffn]),
      i64x2([0xc000000000000000n, 0xc000000000000000n]),
    ]),
  [i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:46
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x7ffffffffffffffdn, 0x7ffffffffffffffdn]),
      i64x2([0x1n, 0x1n]),
    ]),
  [i64x2([0x7ffffffffffffffen, 0x7ffffffffffffffen])],
);

// ./test/core/simd/simd_i64x2_arith.wast:49
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x7ffffffffffffffen, 0x7ffffffffffffffen]),
      i64x2([0x1n, 0x1n]),
    ]),
  [i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:52
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
      i64x2([0x1n, 0x1n]),
    ]),
  [i64x2([0x8000000000000001n, 0x8000000000000001n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:55
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x8000000000000002n, 0x8000000000000002n]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x8000000000000001n, 0x8000000000000001n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:58
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x8000000000000001n, 0x8000000000000001n]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x8000000000000000n, 0x8000000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:61
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:64
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn]),
      i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn]),
    ]),
  [i64x2([0xfffffffffffffffen, 0xfffffffffffffffen])],
);

// ./test/core/simd/simd_i64x2_arith.wast:67
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:70
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
      i64x2([0x8000000000000001n, 0x8000000000000001n]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:73
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0x0n, 0x0n]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:76
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0x1n, 0x1n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:79
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0xfffffffffffffffen, 0xfffffffffffffffen])],
);

// ./test/core/simd/simd_i64x2_arith.wast:82
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn]),
    ]),
  [i64x2([0x7ffffffffffffffen, 0x7ffffffffffffffen])],
);

// ./test/core/simd/simd_i64x2_arith.wast:85
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
    ]),
  [i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:88
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0xfffffffffffffffen, 0xfffffffffffffffen])],
);

// ./test/core/simd/simd_i64x2_arith.wast:91
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x3fffffffffffffffn, 0x3fffffffffffffffn]),
      i64x2([0x4000000000000000n, 0x4000000000000000n]),
    ]),
  [i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:94
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x4000000000000000n, 0x4000000000000000n]),
      i64x2([0x4000000000000000n, 0x4000000000000000n]),
    ]),
  [i64x2([0x8000000000000000n, 0x8000000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:97
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0xc000000000000001n, 0xc000000000000001n]),
      i64x2([0xfbfffffff0000001n, 0xfbfffffff0000001n]),
    ]),
  [i64x2([0xbbfffffff0000002n, 0xbbfffffff0000002n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:100
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0xc000000000000000n, 0xc000000000000000n]),
      i64x2([0xfc00000000000000n, 0xfc00000000000000n]),
    ]),
  [i64x2([0xbc00000000000000n, 0xbc00000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:103
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0xc000000000000000n, 0xc000000000000000n]),
      i64x2([0xfbffffffffffffffn, 0xfbffffffffffffffn]),
    ]),
  [i64x2([0xbbffffffffffffffn, 0xbbffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:106
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn]),
      i64x2([0x7ffffffffffffffn, 0x7ffffffffffffffn]),
    ]),
  [i64x2([0x87fffffffffffffen, 0x87fffffffffffffen])],
);

// ./test/core/simd/simd_i64x2_arith.wast:109
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn]),
      i64x2([0x1n, 0x1n]),
    ]),
  [i64x2([0x8000000000000000n, 0x8000000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:112
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:115
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn]),
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:118
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:121
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0x1n, 0x1n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:124
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0xfffffffffffffffen, 0xfffffffffffffffen])],
);

// ./test/core/simd/simd_i64x2_arith.wast:127
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn]),
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x80,
      ]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:130
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x1n, 0x1n]),
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
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:133
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn]),
      i16x8([0x0, 0x0, 0x0, 0x8000, 0x0, 0x0, 0x0, 0x8000]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:136
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x1n, 0x1n]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:139
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn]),
      i32x4([0x0, 0x80000000, 0x0, 0x80000000]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:142
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x1n, 0x1n]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:145
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
      f64x2([0, 0]),
    ]),
  [i64x2([0x8000000000000000n, 0x8000000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:148
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
      f64x2([-0, -0]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:151
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
      f64x2([1, 1]),
    ]),
  [i64x2([0xbff0000000000000n, 0xbff0000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:154
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
      f64x2([-1, -1]),
    ]),
  [i64x2([0x3ff0000000000000n, 0x3ff0000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:157
assert_return(
  () =>
    invoke($0, `i64x2.add`, [i64x2([0x1n, 0x1n]), f64x2([Infinity, Infinity])]),
  [i64x2([0x7ff0000000000001n, 0x7ff0000000000001n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:160
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x1n, 0x1n]),
      f64x2([-Infinity, -Infinity]),
    ]),
  [i64x2([0xfff0000000000001n, 0xfff0000000000001n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:163
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x1n, 0x1n]),
      bytes("v128", [
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0xf8,
        0x7f,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0xf8,
        0x7f,
      ]),
    ]),
  [i64x2([0x7ff8000000000001n, 0x7ff8000000000001n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:166
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x0n, 0x1n]),
      i64x2([0x0n, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:169
assert_return(
  () => invoke($0, `i64x2.add`, [i64x2([0x0n, 0x1n]), i64x2([0x0n, 0x2n])]),
  [i64x2([0x0n, 0x3n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:172
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x112210f47de98115n, 0x112210f47de98115n]),
      i64x2([0x112210f47de98115n, 0x112210f47de98115n]),
    ]),
  [i64x2([0x224421e8fbd3022an, 0x224421e8fbd3022an])],
);

// ./test/core/simd/simd_i64x2_arith.wast:175
assert_return(
  () =>
    invoke($0, `i64x2.add`, [
      i64x2([0x1234567890abcdefn, 0x1234567890abcdefn]),
      i64x2([0x90abcdef12345678n, 0x90abcdef12345678n]),
    ]),
  [i64x2([0xa2e02467a2e02467n, 0xa2e02467a2e02467n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:180
assert_return(
  () => invoke($0, `i64x2.sub`, [i64x2([0x0n, 0x0n]), i64x2([0x0n, 0x0n])]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:183
assert_return(
  () => invoke($0, `i64x2.sub`, [i64x2([0x0n, 0x0n]), i64x2([0x1n, 0x1n])]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:186
assert_return(
  () => invoke($0, `i64x2.sub`, [i64x2([0x1n, 0x1n]), i64x2([0x1n, 0x1n])]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:189
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x0n, 0x0n]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:192
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x1n, 0x1n]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x2n, 0x2n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:195
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:198
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x3fffffffffffffffn, 0x3fffffffffffffffn]),
      i64x2([0x4000000000000000n, 0x4000000000000000n]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:201
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x4000000000000000n, 0x4000000000000000n]),
      i64x2([0x4000000000000000n, 0x4000000000000000n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:204
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0xc000000000000001n, 0xc000000000000001n]),
      i64x2([0xc000000000000000n, 0xc000000000000000n]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:207
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0xc000000000000000n, 0xc000000000000000n]),
      i64x2([0xc000000000000000n, 0xc000000000000000n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:210
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0xbfffffffffffffffn, 0xbfffffffffffffffn]),
      i64x2([0xc000000000000000n, 0xc000000000000000n]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:213
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x7ffffffffffffffdn, 0x7ffffffffffffffdn]),
      i64x2([0x1n, 0x1n]),
    ]),
  [i64x2([0x7ffffffffffffffcn, 0x7ffffffffffffffcn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:216
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x7ffffffffffffffen, 0x7ffffffffffffffen]),
      i64x2([0x1n, 0x1n]),
    ]),
  [i64x2([0x7ffffffffffffffdn, 0x7ffffffffffffffdn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:219
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
      i64x2([0x1n, 0x1n]),
    ]),
  [i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:222
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x8000000000000002n, 0x8000000000000002n]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x8000000000000003n, 0x8000000000000003n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:225
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x8000000000000001n, 0x8000000000000001n]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x8000000000000002n, 0x8000000000000002n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:228
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x8000000000000001n, 0x8000000000000001n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:231
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn]),
      i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:234
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:237
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
      i64x2([0x8000000000000001n, 0x8000000000000001n]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:240
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0x0n, 0x0n]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:243
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0x1n, 0x1n]),
    ]),
  [i64x2([0xfffffffffffffffen, 0xfffffffffffffffen])],
);

// ./test/core/simd/simd_i64x2_arith.wast:246
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:249
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn]),
    ]),
  [i64x2([0x8000000000000000n, 0x8000000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:252
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
    ]),
  [i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:255
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:258
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x3fffffffffffffffn, 0x3fffffffffffffffn]),
      i64x2([0x4000000000000000n, 0x4000000000000000n]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:261
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x4000000000000000n, 0x4000000000000000n]),
      i64x2([0x4000000000000000n, 0x4000000000000000n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:264
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0xc000000000000001n, 0xc000000000000001n]),
      i64x2([0xfbfffffff0000001n, 0xfbfffffff0000001n]),
    ]),
  [i64x2([0xc400000010000000n, 0xc400000010000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:267
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0xc000000000000000n, 0xc000000000000000n]),
      i64x2([0xfc00000000000000n, 0xfc00000000000000n]),
    ]),
  [i64x2([0xc400000000000000n, 0xc400000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:270
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0xc000000000000000n, 0xc000000000000000n]),
      i64x2([0xfbffffffffffffffn, 0xfbffffffffffffffn]),
    ]),
  [i64x2([0xc400000000000001n, 0xc400000000000001n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:273
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn]),
      i64x2([0x7ffffffffffffffn, 0x7ffffffffffffffn]),
    ]),
  [i64x2([0x7800000000000000n, 0x7800000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:276
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn]),
      i64x2([0x1n, 0x1n]),
    ]),
  [i64x2([0x7ffffffffffffffen, 0x7ffffffffffffffen])],
);

// ./test/core/simd/simd_i64x2_arith.wast:279
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x8000000000000001n, 0x8000000000000001n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:282
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn]),
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:285
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:288
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0x1n, 0x1n]),
    ]),
  [i64x2([0xfffffffffffffffen, 0xfffffffffffffffen])],
);

// ./test/core/simd/simd_i64x2_arith.wast:291
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:294
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn]),
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x80,
      ]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:297
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x1n, 0x1n]),
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
  [i64x2([0x2n, 0x2n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:300
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn]),
      i16x8([0x0, 0x0, 0x0, 0x8000, 0x0, 0x0, 0x0, 0x8000]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:303
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x1n, 0x1n]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i64x2([0x2n, 0x2n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:306
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn]),
      i32x4([0x0, 0x80000000, 0x0, 0x80000000]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:309
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x1n, 0x1n]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x2n, 0x2n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:312
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
      f64x2([0, 0]),
    ]),
  [i64x2([0x8000000000000000n, 0x8000000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:315
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
      f64x2([-0, -0]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:318
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
      f64x2([1, 1]),
    ]),
  [i64x2([0x4010000000000000n, 0x4010000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:321
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
      f64x2([-1, -1]),
    ]),
  [i64x2([0xc010000000000000n, 0xc010000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:324
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [i64x2([0x1n, 0x1n]), f64x2([Infinity, Infinity])]),
  [i64x2([0x8010000000000001n, 0x8010000000000001n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:327
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x1n, 0x1n]),
      f64x2([-Infinity, -Infinity]),
    ]),
  [i64x2([0x10000000000001n, 0x10000000000001n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:330
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x1n, 0x1n]),
      bytes("v128", [
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0xf8,
        0x7f,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0xf8,
        0x7f,
      ]),
    ]),
  [i64x2([0x8008000000000001n, 0x8008000000000001n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:333
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x0n, 0x1n]),
      i64x2([0x0n, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x2n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:336
assert_return(
  () => invoke($0, `i64x2.sub`, [i64x2([0x0n, 0x1n]), i64x2([0x0n, 0x2n])]),
  [i64x2([0x0n, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:339
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x2c9c7076ed2f8115n, 0x2c9c7076ed2f8115n]),
      i64x2([0x112210f47de98115n, 0x112210f47de98115n]),
    ]),
  [i64x2([0x1b7a5f826f460000n, 0x1b7a5f826f460000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:342
assert_return(
  () =>
    invoke($0, `i64x2.sub`, [
      i64x2([0x90abcdef87654321n, 0x90abcdef87654321n]),
      i64x2([0x1234567890abcdefn, 0x1234567890abcdefn]),
    ]),
  [i64x2([0x7e777776f6b97532n, 0x7e777776f6b97532n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:347
assert_return(
  () => invoke($0, `i64x2.mul`, [i64x2([0x0n, 0x0n]), i64x2([0x0n, 0x0n])]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:350
assert_return(
  () => invoke($0, `i64x2.mul`, [i64x2([0x0n, 0x0n]), i64x2([0x1n, 0x1n])]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:353
assert_return(
  () => invoke($0, `i64x2.mul`, [i64x2([0x1n, 0x1n]), i64x2([0x1n, 0x1n])]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:356
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x0n, 0x0n]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:359
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x1n, 0x1n]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:362
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:365
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x3fffffffffffffffn, 0x3fffffffffffffffn]),
      i64x2([0x4000000000000000n, 0x4000000000000000n]),
    ]),
  [i64x2([0xc000000000000000n, 0xc000000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:368
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x4000000000000000n, 0x4000000000000000n]),
      i64x2([0x4000000000000000n, 0x4000000000000000n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:371
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0xc000000000000001n, 0xc000000000000001n]),
      i64x2([0xc000000000000000n, 0xc000000000000000n]),
    ]),
  [i64x2([0xc000000000000000n, 0xc000000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:374
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0xc000000000000000n, 0xc000000000000000n]),
      i64x2([0xc000000000000000n, 0xc000000000000000n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:377
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0xbfffffffffffffffn, 0xbfffffffffffffffn]),
      i64x2([0xc000000000000000n, 0xc000000000000000n]),
    ]),
  [i64x2([0x4000000000000000n, 0x4000000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:380
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x7ffffffffffffffdn, 0x7ffffffffffffffdn]),
      i64x2([0x1n, 0x1n]),
    ]),
  [i64x2([0x7ffffffffffffffdn, 0x7ffffffffffffffdn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:383
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x7ffffffffffffffen, 0x7ffffffffffffffen]),
      i64x2([0x1n, 0x1n]),
    ]),
  [i64x2([0x7ffffffffffffffen, 0x7ffffffffffffffen])],
);

// ./test/core/simd/simd_i64x2_arith.wast:386
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
      i64x2([0x1n, 0x1n]),
    ]),
  [i64x2([0x8000000000000000n, 0x8000000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:389
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x8000000000000002n, 0x8000000000000002n]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x7ffffffffffffffen, 0x7ffffffffffffffen])],
);

// ./test/core/simd/simd_i64x2_arith.wast:392
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x8000000000000001n, 0x8000000000000001n]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:395
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x8000000000000000n, 0x8000000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:398
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn]),
      i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:401
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:404
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
      i64x2([0x8000000000000001n, 0x8000000000000001n]),
    ]),
  [i64x2([0x8000000000000000n, 0x8000000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:407
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0x0n, 0x0n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:410
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0x1n, 0x1n]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:413
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:416
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn]),
    ]),
  [i64x2([0x8000000000000001n, 0x8000000000000001n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:419
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
    ]),
  [i64x2([0x8000000000000000n, 0x8000000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:422
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:425
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x3fffffffffffffffn, 0x3fffffffffffffffn]),
      i64x2([0x4000000000000000n, 0x4000000000000000n]),
    ]),
  [i64x2([0xc000000000000000n, 0xc000000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:428
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x4000000000000000n, 0x4000000000000000n]),
      i64x2([0x4000000000000000n, 0x4000000000000000n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:431
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0xc000000000000001n, 0xc000000000000001n]),
      i64x2([0xfbfffffff0000001n, 0xfbfffffff0000001n]),
    ]),
  [i64x2([0xbbfffffff0000001n, 0xbbfffffff0000001n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:434
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0xc000000000000000n, 0xc000000000000000n]),
      i64x2([0xfc00000000000000n, 0xfc00000000000000n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:437
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0xc000000000000000n, 0xc000000000000000n]),
      i64x2([0xfbffffffffffffffn, 0xfbffffffffffffffn]),
    ]),
  [i64x2([0x4000000000000000n, 0x4000000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:440
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn]),
      i64x2([0x7ffffffffffffffn, 0x7ffffffffffffffn]),
    ]),
  [i64x2([0x7800000000000001n, 0x7800000000000001n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:443
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn]),
      i64x2([0x1n, 0x1n]),
    ]),
  [i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:446
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x8000000000000000n, 0x8000000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:449
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn]),
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
    ]),
  [i64x2([0x8000000000000000n, 0x8000000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:452
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:455
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0x1n, 0x1n]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:458
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:461
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
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
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:464
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
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
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:467
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
      i16x8([0x0, 0x0, 0x0, 0x2, 0x0, 0x0, 0x0, 0x2]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:470
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:473
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
      i32x4([0x0, 0x2, 0x0, 0x2]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:476
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:479
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [i64x2([0x80000000n, 0x80000000n]), f64x2([0, 0])]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:482
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x80000000n, 0x80000000n]),
      f64x2([-0, -0]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:485
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [i64x2([0x80000000n, 0x80000000n]), f64x2([1, 1])]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:488
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x80000000n, 0x80000000n]),
      f64x2([-1, -1]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:491
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [i64x2([0x1n, 0x1n]), f64x2([Infinity, Infinity])]),
  [i64x2([0x7ff0000000000000n, 0x7ff0000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:494
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x1n, 0x1n]),
      f64x2([-Infinity, -Infinity]),
    ]),
  [i64x2([0xfff0000000000000n, 0xfff0000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:497
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x1n, 0x1n]),
      bytes("v128", [
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0xf8,
        0x7f,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0xf8,
        0x7f,
      ]),
    ]),
  [i64x2([0x7ff8000000000000n, 0x7ff8000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:500
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x0n, 0x1n]),
      i64x2([0x0n, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:503
assert_return(
  () => invoke($0, `i64x2.mul`, [i64x2([0x0n, 0x1n]), i64x2([0x0n, 0x2n])]),
  [i64x2([0x0n, 0x2n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:506
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x112210f47de98115n, 0x112210f47de98115n]),
      i64x2([0x112210f47de98115n, 0x112210f47de98115n]),
    ]),
  [i64x2([0x86c28d12bb502bb9n, 0x86c28d12bb502bb9n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:509
assert_return(
  () =>
    invoke($0, `i64x2.mul`, [
      i64x2([0x1234567890abcdefn, 0x1234567890abcdefn]),
      i64x2([0x90abcdef87654321n, 0x90abcdef87654321n]),
    ]),
  [i64x2([0x602f05e9e55618cfn, 0x602f05e9e55618cfn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:514
assert_return(() => invoke($0, `i64x2.neg`, [i64x2([0x0n, 0x0n])]), [
  i64x2([0x0n, 0x0n]),
]);

// ./test/core/simd/simd_i64x2_arith.wast:516
assert_return(() => invoke($0, `i64x2.neg`, [i64x2([0x1n, 0x1n])]), [
  i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
]);

// ./test/core/simd/simd_i64x2_arith.wast:518
assert_return(
  () =>
    invoke($0, `i64x2.neg`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:520
assert_return(
  () =>
    invoke($0, `i64x2.neg`, [
      i64x2([0x7ffffffffffffffen, 0x7ffffffffffffffen]),
    ]),
  [i64x2([0x8000000000000002n, 0x8000000000000002n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:522
assert_return(
  () =>
    invoke($0, `i64x2.neg`, [
      i64x2([0x8000000000000001n, 0x8000000000000001n]),
    ]),
  [i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:524
assert_return(
  () =>
    invoke($0, `i64x2.neg`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
    ]),
  [i64x2([0x8000000000000000n, 0x8000000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:526
assert_return(
  () =>
    invoke($0, `i64x2.neg`, [
      i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn]),
    ]),
  [i64x2([0x8000000000000001n, 0x8000000000000001n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:528
assert_return(
  () =>
    invoke($0, `i64x2.neg`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:530
assert_return(() => invoke($0, `i64x2.neg`, [i64x2([0x1n, 0x1n])]), [
  i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
]);

// ./test/core/simd/simd_i64x2_arith.wast:532
assert_return(
  () =>
    invoke($0, `i64x2.neg`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:534
assert_return(
  () =>
    invoke($0, `i64x2.neg`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
    ]),
  [i64x2([0x8000000000000000n, 0x8000000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:536
assert_return(
  () =>
    invoke($0, `i64x2.neg`, [
      i64x2([0x8000000000000001n, 0x8000000000000001n]),
    ]),
  [i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith.wast:538
assert_return(
  () =>
    invoke($0, `i64x2.neg`, [
      i64x2([0x7fffffffffffffffn, 0x7fffffffffffffffn]),
    ]),
  [i64x2([0x8000000000000001n, 0x8000000000000001n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:540
assert_return(
  () =>
    invoke($0, `i64x2.neg`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
    ]),
  [i64x2([0x8000000000000000n, 0x8000000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:542
assert_return(
  () =>
    invoke($0, `i64x2.neg`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:546
assert_invalid(
  () => instantiate(`(module (func (result v128) (i64x2.neg (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/simd/simd_i64x2_arith.wast:547
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i64x2.add (i32.const 0) (f32.const 0.0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i64x2_arith.wast:548
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i64x2.sub (i32.const 0) (f32.const 0.0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i64x2_arith.wast:549
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i64x2.mul (i32.const 0) (f32.const 0.0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i64x2_arith.wast:553
assert_invalid(() =>
  instantiate(`(module
    (func $$i64x2.neg-arg-empty (result v128)
      (i64x2.neg)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i64x2_arith.wast:561
assert_invalid(() =>
  instantiate(`(module
    (func $$i64x2.add-1st-arg-empty (result v128)
      (i64x2.add (v128.const i64x2 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i64x2_arith.wast:569
assert_invalid(() =>
  instantiate(`(module
    (func $$i64x2.add-arg-empty (result v128)
      (i64x2.add)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i64x2_arith.wast:577
assert_invalid(() =>
  instantiate(`(module
    (func $$i64x2.sub-1st-arg-empty (result v128)
      (i64x2.sub (v128.const i64x2 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i64x2_arith.wast:585
assert_invalid(() =>
  instantiate(`(module
    (func $$i64x2.sub-arg-empty (result v128)
      (i64x2.sub)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i64x2_arith.wast:593
assert_invalid(() =>
  instantiate(`(module
    (func $$i64x2.mul-1st-arg-empty (result v128)
      (i64x2.mul (v128.const i64x2 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i64x2_arith.wast:601
assert_invalid(() =>
  instantiate(`(module
    (func $$i64x2.mul-arg-empty (result v128)
      (i64x2.mul)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i64x2_arith.wast:611
let $1 = instantiate(`(module
  (func (export "add-sub") (param v128 v128 v128) (result v128)
    (i64x2.add (i64x2.sub (local.get 0) (local.get 1))(local.get 2)))
  (func (export "mul-add") (param v128 v128 v128) (result v128)
    (i64x2.mul (i64x2.add (local.get 0) (local.get 1))(local.get 2)))
  (func (export "mul-sub") (param v128 v128 v128) (result v128)
    (i64x2.mul (i64x2.sub (local.get 0) (local.get 1))(local.get 2)))
  (func (export "sub-add") (param v128 v128 v128) (result v128)
    (i64x2.sub (i64x2.add (local.get 0) (local.get 1))(local.get 2)))
  (func (export "add-neg") (param v128 v128) (result v128)
    (i64x2.add (i64x2.neg (local.get 0)) (local.get 1)))
  (func (export "mul-neg") (param v128 v128) (result v128)
    (i64x2.mul (i64x2.neg (local.get 0)) (local.get 1)))
  (func (export "sub-neg") (param v128 v128) (result v128)
    (i64x2.sub (i64x2.neg (local.get 0)) (local.get 1)))
)`);

// ./test/core/simd/simd_i64x2_arith.wast:628
assert_return(
  () =>
    invoke($1, `add-sub`, [
      i64x2([0x0n, 0x1n]),
      i64x2([0x0n, 0x2n]),
      i64x2([0x0n, 0x2n]),
    ]),
  [i64x2([0x0n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:632
assert_return(
  () =>
    invoke($1, `mul-add`, [
      i64x2([0x0n, 0x1n]),
      i64x2([0x0n, 0x1n]),
      i64x2([0x2n, 0x2n]),
    ]),
  [i64x2([0x0n, 0x4n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:636
assert_return(
  () =>
    invoke($1, `mul-sub`, [
      i64x2([0x0n, 0x2n]),
      i64x2([0x0n, 0x1n]),
      i64x2([0x0n, 0x1n]),
    ]),
  [i64x2([0x0n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:640
assert_return(
  () =>
    invoke($1, `sub-add`, [
      i64x2([0x0n, 0x1n]),
      i64x2([0x0n, 0x2n]),
      i64x2([0x0n, 0x2n]),
    ]),
  [i64x2([0x0n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:644
assert_return(
  () => invoke($1, `add-neg`, [i64x2([0x0n, 0x1n]), i64x2([0x0n, 0x1n])]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_arith.wast:647
assert_return(
  () => invoke($1, `mul-neg`, [i64x2([0x0n, 0x1n]), i64x2([0x2n, 0x2n])]),
  [i64x2([0x0n, 0xfffffffffffffffen])],
);

// ./test/core/simd/simd_i64x2_arith.wast:650
assert_return(
  () => invoke($1, `sub-neg`, [i64x2([0x0n, 0x1n]), i64x2([0x0n, 0x1n])]),
  [i64x2([0x0n, 0xfffffffffffffffen])],
);
