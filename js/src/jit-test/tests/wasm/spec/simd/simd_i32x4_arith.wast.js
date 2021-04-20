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

// ./test/core/simd/simd_i32x4_arith.wast

// ./test/core/simd/simd_i32x4_arith.wast:4
let $0 = instantiate(`(module
  (func (export "i32x4.add") (param v128 v128) (result v128) (i32x4.add (local.get 0) (local.get 1)))
  (func (export "i32x4.sub") (param v128 v128) (result v128) (i32x4.sub (local.get 0) (local.get 1)))
  (func (export "i32x4.mul") (param v128 v128) (result v128) (i32x4.mul (local.get 0) (local.get 1)))
  (func (export "i32x4.neg") (param v128) (result v128) (i32x4.neg (local.get 0)))
)`);

// ./test/core/simd/simd_i32x4_arith.wast:13
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:16
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith.wast:19
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x2, 0x2, 0x2, 0x2])],
);

// ./test/core/simd/simd_i32x4_arith.wast:22
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:25
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:28
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xfffffffe, 0xfffffffe, 0xfffffffe, 0xfffffffe])],
);

// ./test/core/simd/simd_i32x4_arith.wast:31
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x3fffffff, 0x3fffffff, 0x3fffffff, 0x3fffffff]),
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:34
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:37
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0xc0000001, 0xc0000001, 0xc0000001, 0xc0000001]),
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
    ]),
  [i32x4([0x80000001, 0x80000001, 0x80000001, 0x80000001])],
);

// ./test/core/simd/simd_i32x4_arith.wast:40
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:43
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0xbfffffff, 0xbfffffff, 0xbfffffff, 0xbfffffff]),
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:46
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x7ffffffd, 0x7ffffffd, 0x7ffffffd, 0x7ffffffd]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x7ffffffe, 0x7ffffffe, 0x7ffffffe, 0x7ffffffe])],
);

// ./test/core/simd/simd_i32x4_arith.wast:49
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x7ffffffe, 0x7ffffffe, 0x7ffffffe, 0x7ffffffe]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:52
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x80000001, 0x80000001, 0x80000001, 0x80000001])],
);

// ./test/core/simd/simd_i32x4_arith.wast:55
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x80000002, 0x80000002, 0x80000002, 0x80000002]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x80000001, 0x80000001, 0x80000001, 0x80000001])],
);

// ./test/core/simd/simd_i32x4_arith.wast:58
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x80000001, 0x80000001, 0x80000001, 0x80000001]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:61
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:64
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
    ]),
  [i32x4([0xfffffffe, 0xfffffffe, 0xfffffffe, 0xfffffffe])],
);

// ./test/core/simd/simd_i32x4_arith.wast:67
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:70
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x80000001, 0x80000001, 0x80000001, 0x80000001]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith.wast:73
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:76
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:79
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xfffffffe, 0xfffffffe, 0xfffffffe, 0xfffffffe])],
);

// ./test/core/simd/simd_i32x4_arith.wast:82
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
    ]),
  [i32x4([0x7ffffffe, 0x7ffffffe, 0x7ffffffe, 0x7ffffffe])],
);

// ./test/core/simd/simd_i32x4_arith.wast:85
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:88
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xfffffffe, 0xfffffffe, 0xfffffffe, 0xfffffffe])],
);

// ./test/core/simd/simd_i32x4_arith.wast:91
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x3fffffff, 0x3fffffff, 0x3fffffff, 0x3fffffff]),
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:94
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:97
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0xc0000001, 0xc0000001, 0xc0000001, 0xc0000001]),
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
    ]),
  [i32x4([0x80000001, 0x80000001, 0x80000001, 0x80000001])],
);

// ./test/core/simd/simd_i32x4_arith.wast:100
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:103
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
      i32x4([0xbfffffff, 0xbfffffff, 0xbfffffff, 0xbfffffff]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:106
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
    ]),
  [i32x4([0xfffffffe, 0xfffffffe, 0xfffffffe, 0xfffffffe])],
);

// ./test/core/simd/simd_i32x4_arith.wast:109
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:112
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:115
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:118
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:121
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:124
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xfffffffe, 0xfffffffe, 0xfffffffe, 0xfffffffe])],
);

// ./test/core/simd/simd_i32x4_arith.wast:127
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
      i8x16([
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x80,
      ]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:130
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
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
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:133
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
      i16x8([0x0, 0x8000, 0x0, 0x8000, 0x0, 0x8000, 0x0, 0x8000]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:136
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:139
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      f32x4([0, 0, 0, 0]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:142
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      bytes("v128", [
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x80,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:145
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      f32x4([1, 1, 1, 1]),
    ]),
  [i32x4([0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:148
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      f32x4([-1, -1, -1, -1]),
    ]),
  [i32x4([0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:151
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [i32x4([0x7f800001, 0x7f800001, 0x7f800001, 0x7f800001])],
);

// ./test/core/simd/simd_i32x4_arith.wast:154
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [i32x4([0xff800001, 0xff800001, 0xff800001, 0xff800001])],
);

// ./test/core/simd/simd_i32x4_arith.wast:157
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
    ]),
  [i32x4([0x7fc00001, 0x7fc00001, 0x7fc00001, 0x7fc00001])],
);

// ./test/core/simd/simd_i32x4_arith.wast:160
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x0, 0x1, 0x2, 0x3]),
      i32x4([0x0, 0xffffffff, 0xfffffffe, 0xfffffffd]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:163
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x0, 0x1, 0x2, 0x3]),
      i32x4([0x0, 0x2, 0x4, 0x6]),
    ]),
  [i32x4([0x0, 0x3, 0x6, 0x9])],
);

// ./test/core/simd/simd_i32x4_arith.wast:166
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x499602d2, 0x499602d2, 0x499602d2, 0x499602d2]),
      i32x4([0x499602d2, 0x499602d2, 0x499602d2, 0x499602d2]),
    ]),
  [i32x4([0x932c05a4, 0x932c05a4, 0x932c05a4, 0x932c05a4])],
);

// ./test/core/simd/simd_i32x4_arith.wast:169
assert_return(
  () =>
    invoke($0, `i32x4.add`, [
      i32x4([0x12345678, 0x12345678, 0x12345678, 0x12345678]),
      i32x4([0x90abcdef, 0x90abcdef, 0x90abcdef, 0x90abcdef]),
    ]),
  [i32x4([0xa2e02467, 0xa2e02467, 0xa2e02467, 0xa2e02467])],
);

// ./test/core/simd/simd_i32x4_arith.wast:174
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:177
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:180
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:183
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith.wast:186
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x2, 0x2, 0x2, 0x2])],
);

// ./test/core/simd/simd_i32x4_arith.wast:189
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:192
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x3fffffff, 0x3fffffff, 0x3fffffff, 0x3fffffff]),
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:195
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:198
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0xc0000001, 0xc0000001, 0xc0000001, 0xc0000001]),
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith.wast:201
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:204
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0xbfffffff, 0xbfffffff, 0xbfffffff, 0xbfffffff]),
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:207
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x7ffffffd, 0x7ffffffd, 0x7ffffffd, 0x7ffffffd]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x7ffffffc, 0x7ffffffc, 0x7ffffffc, 0x7ffffffc])],
);

// ./test/core/simd/simd_i32x4_arith.wast:210
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x7ffffffe, 0x7ffffffe, 0x7ffffffe, 0x7ffffffe]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x7ffffffd, 0x7ffffffd, 0x7ffffffd, 0x7ffffffd])],
);

// ./test/core/simd/simd_i32x4_arith.wast:213
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:216
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x80000002, 0x80000002, 0x80000002, 0x80000002]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x80000003, 0x80000003, 0x80000003, 0x80000003])],
);

// ./test/core/simd/simd_i32x4_arith.wast:219
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x80000001, 0x80000001, 0x80000001, 0x80000001]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x80000002, 0x80000002, 0x80000002, 0x80000002])],
);

// ./test/core/simd/simd_i32x4_arith.wast:222
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x80000001, 0x80000001, 0x80000001, 0x80000001])],
);

// ./test/core/simd/simd_i32x4_arith.wast:225
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:228
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:231
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x80000001, 0x80000001, 0x80000001, 0x80000001]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:234
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:237
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0xfffffffe, 0xfffffffe, 0xfffffffe, 0xfffffffe])],
);

// ./test/core/simd/simd_i32x4_arith.wast:240
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:243
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:246
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:249
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:252
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x3fffffff, 0x3fffffff, 0x3fffffff, 0x3fffffff]),
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:255
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:258
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0xc0000001, 0xc0000001, 0xc0000001, 0xc0000001]),
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith.wast:261
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:264
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
      i32x4([0xbfffffff, 0xbfffffff, 0xbfffffff, 0xbfffffff]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith.wast:267
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:270
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x7ffffffe, 0x7ffffffe, 0x7ffffffe, 0x7ffffffe])],
);

// ./test/core/simd/simd_i32x4_arith.wast:273
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x80000001, 0x80000001, 0x80000001, 0x80000001])],
);

// ./test/core/simd/simd_i32x4_arith.wast:276
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:279
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:282
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0xfffffffe, 0xfffffffe, 0xfffffffe, 0xfffffffe])],
);

// ./test/core/simd/simd_i32x4_arith.wast:285
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:288
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
      i8x16([
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x80,
      ]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:291
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
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
  [i32x4([0x2, 0x2, 0x2, 0x2])],
);

// ./test/core/simd/simd_i32x4_arith.wast:294
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
      i16x8([0x0, 0x8000, 0x0, 0x8000, 0x0, 0x8000, 0x0, 0x8000]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:297
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0x2, 0x2, 0x2, 0x2])],
);

// ./test/core/simd/simd_i32x4_arith.wast:300
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      f32x4([0, 0, 0, 0]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:303
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      bytes("v128", [
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x80,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:306
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      f32x4([1, 1, 1, 1]),
    ]),
  [i32x4([0x40800000, 0x40800000, 0x40800000, 0x40800000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:309
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      f32x4([-1, -1, -1, -1]),
    ]),
  [i32x4([0xc0800000, 0xc0800000, 0xc0800000, 0xc0800000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:312
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [i32x4([0x80800001, 0x80800001, 0x80800001, 0x80800001])],
);

// ./test/core/simd/simd_i32x4_arith.wast:315
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [i32x4([0x800001, 0x800001, 0x800001, 0x800001])],
);

// ./test/core/simd/simd_i32x4_arith.wast:318
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
    ]),
  [i32x4([0x80400001, 0x80400001, 0x80400001, 0x80400001])],
);

// ./test/core/simd/simd_i32x4_arith.wast:321
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x0, 0x1, 0x2, 0x3]),
      i32x4([0x0, 0xffffffff, 0xfffffffe, 0xfffffffd]),
    ]),
  [i32x4([0x0, 0x2, 0x4, 0x6])],
);

// ./test/core/simd/simd_i32x4_arith.wast:324
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x0, 0x1, 0x2, 0x3]),
      i32x4([0x0, 0x2, 0x4, 0x6]),
    ]),
  [i32x4([0x0, 0xffffffff, 0xfffffffe, 0xfffffffd])],
);

// ./test/core/simd/simd_i32x4_arith.wast:327
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0xbf9a69d2, 0xbf9a69d2, 0xbf9a69d2, 0xbf9a69d2]),
      i32x4([0x499602d2, 0x499602d2, 0x499602d2, 0x499602d2]),
    ]),
  [i32x4([0x76046700, 0x76046700, 0x76046700, 0x76046700])],
);

// ./test/core/simd/simd_i32x4_arith.wast:330
assert_return(
  () =>
    invoke($0, `i32x4.sub`, [
      i32x4([0x90abcdef, 0x90abcdef, 0x90abcdef, 0x90abcdef]),
      i32x4([0x12345678, 0x12345678, 0x12345678, 0x12345678]),
    ]),
  [i32x4([0x7e777777, 0x7e777777, 0x7e777777, 0x7e777777])],
);

// ./test/core/simd/simd_i32x4_arith.wast:335
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:338
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:341
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith.wast:344
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:347
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:350
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith.wast:353
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x3fffffff, 0x3fffffff, 0x3fffffff, 0x3fffffff]),
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
    ]),
  [i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:356
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:359
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0xc0000001, 0xc0000001, 0xc0000001, 0xc0000001]),
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
    ]),
  [i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:362
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:365
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0xbfffffff, 0xbfffffff, 0xbfffffff, 0xbfffffff]),
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
    ]),
  [i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:368
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x7ffffffd, 0x7ffffffd, 0x7ffffffd, 0x7ffffffd]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x7ffffffd, 0x7ffffffd, 0x7ffffffd, 0x7ffffffd])],
);

// ./test/core/simd/simd_i32x4_arith.wast:371
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x7ffffffe, 0x7ffffffe, 0x7ffffffe, 0x7ffffffe]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x7ffffffe, 0x7ffffffe, 0x7ffffffe, 0x7ffffffe])],
);

// ./test/core/simd/simd_i32x4_arith.wast:374
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:377
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x80000002, 0x80000002, 0x80000002, 0x80000002]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x7ffffffe, 0x7ffffffe, 0x7ffffffe, 0x7ffffffe])],
);

// ./test/core/simd/simd_i32x4_arith.wast:380
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x80000001, 0x80000001, 0x80000001, 0x80000001]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:383
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:386
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith.wast:389
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:392
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x80000001, 0x80000001, 0x80000001, 0x80000001]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:395
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:398
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:401
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith.wast:404
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
    ]),
  [i32x4([0x80000001, 0x80000001, 0x80000001, 0x80000001])],
);

// ./test/core/simd/simd_i32x4_arith.wast:407
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:410
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith.wast:413
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x3fffffff, 0x3fffffff, 0x3fffffff, 0x3fffffff]),
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
    ]),
  [i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:416
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
      i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:419
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0xc0000001, 0xc0000001, 0xc0000001, 0xc0000001]),
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
    ]),
  [i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:422
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:425
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0xc0000000, 0xc0000000, 0xc0000000, 0xc0000000]),
      i32x4([0xbfffffff, 0xbfffffff, 0xbfffffff, 0xbfffffff]),
    ]),
  [i32x4([0x40000000, 0x40000000, 0x40000000, 0x40000000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:428
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith.wast:431
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:434
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:437
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:440
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:443
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:446
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith.wast:449
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x10000000, 0x10000000, 0x10000000, 0x10000000]),
      i8x16([
        0x10,
        0x10,
        0x10,
        0x10,
        0x10,
        0x10,
        0x10,
        0x10,
        0x10,
        0x10,
        0x10,
        0x10,
        0x10,
        0x10,
        0x10,
        0x10,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:452
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
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
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith.wast:455
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
      i16x8([0x0, 0x2, 0x0, 0x2, 0x0, 0x2, 0x0, 0x2]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:458
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith.wast:461
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x8000, 0x8000, 0x8000, 0x8000]),
      f32x4([0, 0, 0, 0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:464
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x8000, 0x8000, 0x8000, 0x8000]),
      bytes("v128", [
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x80,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:467
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x8000, 0x8000, 0x8000, 0x8000]),
      f32x4([1, 1, 1, 1]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:470
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x8000, 0x8000, 0x8000, 0x8000]),
      f32x4([-1, -1, -1, -1]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:473
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [i32x4([0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:476
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [i32x4([0xff800000, 0xff800000, 0xff800000, 0xff800000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:479
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
    ]),
  [i32x4([0x7fc00000, 0x7fc00000, 0x7fc00000, 0x7fc00000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:482
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x0, 0x1, 0x2, 0x3]),
      i32x4([0x0, 0xffffffff, 0xfffffffe, 0xfffffffd]),
    ]),
  [i32x4([0x0, 0xffffffff, 0xfffffffc, 0xfffffff7])],
);

// ./test/core/simd/simd_i32x4_arith.wast:485
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x0, 0x1, 0x2, 0x3]),
      i32x4([0x0, 0x2, 0x4, 0x6]),
    ]),
  [i32x4([0x0, 0x2, 0x8, 0x12])],
);

// ./test/core/simd/simd_i32x4_arith.wast:488
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x75bcd15, 0x75bcd15, 0x75bcd15, 0x75bcd15]),
      i32x4([0x3ade68b1, 0x3ade68b1, 0x3ade68b1, 0x3ade68b1]),
    ]),
  [i32x4([0xfbff5385, 0xfbff5385, 0xfbff5385, 0xfbff5385])],
);

// ./test/core/simd/simd_i32x4_arith.wast:491
assert_return(
  () =>
    invoke($0, `i32x4.mul`, [
      i32x4([0x12345678, 0x12345678, 0x12345678, 0x12345678]),
      i32x4([0x90abcdef, 0x90abcdef, 0x90abcdef, 0x90abcdef]),
    ]),
  [i32x4([0x2a42d208, 0x2a42d208, 0x2a42d208, 0x2a42d208])],
);

// ./test/core/simd/simd_i32x4_arith.wast:496
assert_return(() => invoke($0, `i32x4.neg`, [i32x4([0x0, 0x0, 0x0, 0x0])]), [
  i32x4([0x0, 0x0, 0x0, 0x0]),
]);

// ./test/core/simd/simd_i32x4_arith.wast:498
assert_return(() => invoke($0, `i32x4.neg`, [i32x4([0x1, 0x1, 0x1, 0x1])]), [
  i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
]);

// ./test/core/simd/simd_i32x4_arith.wast:500
assert_return(
  () =>
    invoke($0, `i32x4.neg`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith.wast:502
assert_return(
  () =>
    invoke($0, `i32x4.neg`, [
      i32x4([0x7ffffffe, 0x7ffffffe, 0x7ffffffe, 0x7ffffffe]),
    ]),
  [i32x4([0x80000002, 0x80000002, 0x80000002, 0x80000002])],
);

// ./test/core/simd/simd_i32x4_arith.wast:504
assert_return(
  () =>
    invoke($0, `i32x4.neg`, [
      i32x4([0x80000001, 0x80000001, 0x80000001, 0x80000001]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:506
assert_return(
  () =>
    invoke($0, `i32x4.neg`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:508
assert_return(
  () =>
    invoke($0, `i32x4.neg`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
    ]),
  [i32x4([0x80000001, 0x80000001, 0x80000001, 0x80000001])],
);

// ./test/core/simd/simd_i32x4_arith.wast:510
assert_return(
  () =>
    invoke($0, `i32x4.neg`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith.wast:512
assert_return(() => invoke($0, `i32x4.neg`, [i32x4([0x1, 0x1, 0x1, 0x1])]), [
  i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
]);

// ./test/core/simd/simd_i32x4_arith.wast:514
assert_return(
  () =>
    invoke($0, `i32x4.neg`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith.wast:516
assert_return(
  () =>
    invoke($0, `i32x4.neg`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:518
assert_return(
  () =>
    invoke($0, `i32x4.neg`, [
      i32x4([0x80000001, 0x80000001, 0x80000001, 0x80000001]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff])],
);

// ./test/core/simd/simd_i32x4_arith.wast:520
assert_return(
  () =>
    invoke($0, `i32x4.neg`, [
      i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff]),
    ]),
  [i32x4([0x80000001, 0x80000001, 0x80000001, 0x80000001])],
);

// ./test/core/simd/simd_i32x4_arith.wast:522
assert_return(
  () =>
    invoke($0, `i32x4.neg`, [
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_arith.wast:524
assert_return(
  () =>
    invoke($0, `i32x4.neg`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_arith.wast:528
assert_invalid(
  () => instantiate(`(module (func (result v128) (i32x4.neg (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/simd/simd_i32x4_arith.wast:529
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.add (i32.const 0) (f32.const 0.0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i32x4_arith.wast:530
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.sub (i32.const 0) (f32.const 0.0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i32x4_arith.wast:531
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.mul (i32.const 0) (f32.const 0.0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i32x4_arith.wast:535
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.neg-arg-empty (result v128)
      (i32x4.neg)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_arith.wast:543
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.add-1st-arg-empty (result v128)
      (i32x4.add (v128.const i32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_arith.wast:551
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.add-arg-empty (result v128)
      (i32x4.add)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_arith.wast:559
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.sub-1st-arg-empty (result v128)
      (i32x4.sub (v128.const i32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_arith.wast:567
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.sub-arg-empty (result v128)
      (i32x4.sub)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_arith.wast:575
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.mul-1st-arg-empty (result v128)
      (i32x4.mul (v128.const i32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_arith.wast:583
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.mul-arg-empty (result v128)
      (i32x4.mul)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_arith.wast:593
let $1 = instantiate(`(module
  (func (export "add-sub") (param v128 v128 v128) (result v128)
    (i32x4.add (i32x4.sub (local.get 0) (local.get 1))(local.get 2)))
  (func (export "mul-add") (param v128 v128 v128) (result v128)
    (i32x4.mul (i32x4.add (local.get 0) (local.get 1))(local.get 2)))
  (func (export "mul-sub") (param v128 v128 v128) (result v128)
    (i32x4.mul (i32x4.sub (local.get 0) (local.get 1))(local.get 2)))
  (func (export "sub-add") (param v128 v128 v128) (result v128)
    (i32x4.sub (i32x4.add (local.get 0) (local.get 1))(local.get 2)))
  (func (export "add-neg") (param v128 v128) (result v128)
    (i32x4.add (i32x4.neg (local.get 0)) (local.get 1)))
  (func (export "mul-neg") (param v128 v128) (result v128)
    (i32x4.mul (i32x4.neg (local.get 0)) (local.get 1)))
  (func (export "sub-neg") (param v128 v128) (result v128)
    (i32x4.sub (i32x4.neg (local.get 0)) (local.get 1)))
)`);

// ./test/core/simd/simd_i32x4_arith.wast:610
assert_return(
  () =>
    invoke($1, `add-sub`, [
      i32x4([0x0, 0x1, 0x2, 0x3]),
      i32x4([0x0, 0x2, 0x4, 0x6]),
      i32x4([0x0, 0x2, 0x4, 0x6]),
    ]),
  [i32x4([0x0, 0x1, 0x2, 0x3])],
);

// ./test/core/simd/simd_i32x4_arith.wast:614
assert_return(
  () =>
    invoke($1, `mul-add`, [
      i32x4([0x0, 0x1, 0x2, 0x3]),
      i32x4([0x0, 0x1, 0x2, 0x3]),
      i32x4([0x2, 0x2, 0x2, 0x2]),
    ]),
  [i32x4([0x0, 0x4, 0x8, 0xc])],
);

// ./test/core/simd/simd_i32x4_arith.wast:618
assert_return(
  () =>
    invoke($1, `mul-sub`, [
      i32x4([0x0, 0x2, 0x4, 0x6]),
      i32x4([0x0, 0x1, 0x2, 0x3]),
      i32x4([0x0, 0x1, 0x2, 0x3]),
    ]),
  [i32x4([0x0, 0x1, 0x4, 0x9])],
);

// ./test/core/simd/simd_i32x4_arith.wast:622
assert_return(
  () =>
    invoke($1, `sub-add`, [
      i32x4([0x0, 0x1, 0x2, 0x3]),
      i32x4([0x0, 0x2, 0x4, 0x6]),
      i32x4([0x0, 0x2, 0x4, 0x6]),
    ]),
  [i32x4([0x0, 0x1, 0x2, 0x3])],
);

// ./test/core/simd/simd_i32x4_arith.wast:626
assert_return(
  () =>
    invoke($1, `add-neg`, [
      i32x4([0x0, 0x1, 0x2, 0x3]),
      i32x4([0x0, 0x1, 0x2, 0x3]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_arith.wast:629
assert_return(
  () =>
    invoke($1, `mul-neg`, [
      i32x4([0x0, 0x1, 0x2, 0x3]),
      i32x4([0x2, 0x2, 0x2, 0x2]),
    ]),
  [i32x4([0x0, 0xfffffffe, 0xfffffffc, 0xfffffffa])],
);

// ./test/core/simd/simd_i32x4_arith.wast:632
assert_return(
  () =>
    invoke($1, `sub-neg`, [
      i32x4([0x0, 0x1, 0x2, 0x3]),
      i32x4([0x0, 0x1, 0x2, 0x3]),
    ]),
  [i32x4([0x0, 0xfffffffe, 0xfffffffc, 0xfffffffa])],
);
