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

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:3
let $0 = instantiate(`(module
  (func (export "i32x4.trunc_sat_f64x2_s_zero") (param v128) (result v128) (i32x4.trunc_sat_f64x2_s_zero (local.get 0)))
  (func (export "i32x4.trunc_sat_f64x2_u_zero") (param v128) (result v128) (i32x4.trunc_sat_f64x2_u_zero (local.get 0)))
)`);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:10
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [f64x2([0, 0])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:12
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [f64x2([-0, -0])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:14
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [f64x2([1.5, 1.5])]),
  [i32x4([0x1, 0x1, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:16
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [f64x2([-1.5, -1.5])]),
  [i32x4([0xffffffff, 0xffffffff, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:18
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [f64x2([1.9, 1.9])]),
  [i32x4([0x1, 0x1, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:20
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [f64x2([2, 2])]),
  [i32x4([0x2, 0x2, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:22
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [f64x2([-1.9, -1.9])]),
  [i32x4([0xffffffff, 0xffffffff, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:24
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [f64x2([-2, -2])]),
  [i32x4([0xfffffffe, 0xfffffffe, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:26
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([2147483520, 2147483520]),
    ]),
  [i32x4([0x7fffff80, 0x7fffff80, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:28
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([-2147483520, -2147483520]),
    ]),
  [i32x4([0x80000080, 0x80000080, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:30
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([2147483648, 2147483648]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:32
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([-2147483648, -2147483648]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:34
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([4294967294, 4294967294]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:36
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([-4294967294, -4294967294]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:38
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([2147483647, 2147483647]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:40
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([-2147483647, -2147483647]),
    ]),
  [i32x4([0x80000001, 0x80000001, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:42
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([4294967294, 4294967294]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:44
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([4294967295, 4294967295]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:46
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([4294967296, 4294967296]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:48
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([
        0.000000000000000000000000000000000000000000001401298464324817,
        0.000000000000000000000000000000000000000000001401298464324817,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:50
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([
        -0.000000000000000000000000000000000000000000001401298464324817,
        -0.000000000000000000000000000000000000000000001401298464324817,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:52
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([
        0.000000000000000000000000000000000000011754943508222875,
        0.000000000000000000000000000000000000011754943508222875,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:54
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([
        -0.000000000000000000000000000000000000011754943508222875,
        -0.000000000000000000000000000000000000011754943508222875,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:56
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [f64x2([0.5, 0.5])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:58
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [f64x2([-0.5, -0.5])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:60
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [f64x2([1, 1])]),
  [i32x4([0x1, 0x1, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:62
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [f64x2([-1, -1])]),
  [i32x4([0xffffffff, 0xffffffff, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:64
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([1.100000023841858, 1.100000023841858]),
    ]),
  [i32x4([0x1, 0x1, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:66
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([-1.100000023841858, -1.100000023841858]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:68
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([6.2831854820251465, 6.2831854820251465]),
    ]),
  [i32x4([0x6, 0x6, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:70
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([-6.2831854820251465, -6.2831854820251465]),
    ]),
  [i32x4([0xfffffffa, 0xfffffffa, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:72
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([
        340282346638528860000000000000000000000,
        340282346638528860000000000000000000000,
      ]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:74
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([
        -340282346638528860000000000000000000000,
        -340282346638528860000000000000000000000,
      ]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:76
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([0.8999999761581421, 0.8999999761581421]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:78
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([-0.8999999761581421, -0.8999999761581421]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:80
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([0.9999999403953552, 0.9999999403953552]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:82
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([-0.9999999403953552, -0.9999999403953552]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:84
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([6.2831854820251465, 6.2831854820251465]),
    ]),
  [i32x4([0x6, 0x6, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:86
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([-6.2831854820251465, -6.2831854820251465]),
    ]),
  [i32x4([0xfffffffa, 0xfffffffa, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:88
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([
        340282346638528860000000000000000000000,
        340282346638528860000000000000000000000,
      ]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:90
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([
        -340282346638528860000000000000000000000,
        -340282346638528860000000000000000000000,
      ]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:92
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [f64x2([Infinity, Infinity])]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:94
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [f64x2([-Infinity, -Infinity])]),
  [i32x4([0x80000000, 0x80000000, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:96
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
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
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:98
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      bytes("v128", [
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0xf8,
        0xff,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0xf8,
        0xff,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:100
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      bytes("v128", [
        0x44,
        0x44,
        0x44,
        0x0,
        0x0,
        0x0,
        0xf0,
        0x7f,
        0x44,
        0x44,
        0x44,
        0x0,
        0x0,
        0x0,
        0xf0,
        0x7f,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:102
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      bytes("v128", [
        0x44,
        0x44,
        0x44,
        0x0,
        0x0,
        0x0,
        0xf0,
        0xff,
        0x44,
        0x44,
        0x44,
        0x0,
        0x0,
        0x0,
        0xf0,
        0xff,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:104
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [f64x2([42, 42])]),
  [i32x4([0x2a, 0x2a, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:106
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [f64x2([-42, -42])]),
  [i32x4([0xffffffd6, 0xffffffd6, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:108
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [f64x2([123456792, 123456792])]),
  [i32x4([0x75bcd18, 0x75bcd18, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:110
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_s_zero`, [
      f64x2([1234567890, 1234567890]),
    ]),
  [i32x4([0x499602d2, 0x499602d2, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:114
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [f64x2([0, 0])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:116
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [f64x2([-0, -0])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:118
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [f64x2([1.5, 1.5])]),
  [i32x4([0x1, 0x1, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:120
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [f64x2([-1.5, -1.5])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:122
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [f64x2([1.9, 1.9])]),
  [i32x4([0x1, 0x1, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:124
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [f64x2([2, 2])]),
  [i32x4([0x2, 0x2, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:126
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [f64x2([-1.9, -1.9])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:128
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [f64x2([-2, -2])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:130
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([2147483520, 2147483520]),
    ]),
  [i32x4([0x7fffff80, 0x7fffff80, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:132
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([-2147483520, -2147483520]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:134
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([2147483648, 2147483648]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:136
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([-2147483648, -2147483648]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:138
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([4294967294, 4294967294]),
    ]),
  [i32x4([0xfffffffe, 0xfffffffe, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:140
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([-4294967294, -4294967294]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:142
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([2147483647, 2147483647]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:144
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([-2147483647, -2147483647]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:146
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([4294967294, 4294967294]),
    ]),
  [i32x4([0xfffffffe, 0xfffffffe, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:148
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([4294967295, 4294967295]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:150
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([4294967296, 4294967296]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:152
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([
        0.000000000000000000000000000000000000000000001401298464324817,
        0.000000000000000000000000000000000000000000001401298464324817,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:154
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([
        -0.000000000000000000000000000000000000000000001401298464324817,
        -0.000000000000000000000000000000000000000000001401298464324817,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:156
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([
        0.000000000000000000000000000000000000011754943508222875,
        0.000000000000000000000000000000000000011754943508222875,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:158
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([
        -0.000000000000000000000000000000000000011754943508222875,
        -0.000000000000000000000000000000000000011754943508222875,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:160
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [f64x2([0.5, 0.5])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:162
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [f64x2([-0.5, -0.5])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:164
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [f64x2([1, 1])]),
  [i32x4([0x1, 0x1, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:166
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [f64x2([-1, -1])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:168
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([1.100000023841858, 1.100000023841858]),
    ]),
  [i32x4([0x1, 0x1, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:170
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([-1.100000023841858, -1.100000023841858]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:172
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([6.2831854820251465, 6.2831854820251465]),
    ]),
  [i32x4([0x6, 0x6, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:174
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([-6.2831854820251465, -6.2831854820251465]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:176
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([
        340282346638528860000000000000000000000,
        340282346638528860000000000000000000000,
      ]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:178
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([
        -340282346638528860000000000000000000000,
        -340282346638528860000000000000000000000,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:180
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([0.8999999761581421, 0.8999999761581421]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:182
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([-0.8999999761581421, -0.8999999761581421]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:184
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([0.9999999403953552, 0.9999999403953552]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:186
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([-0.9999999403953552, -0.9999999403953552]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:188
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([6.2831854820251465, 6.2831854820251465]),
    ]),
  [i32x4([0x6, 0x6, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:190
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([-6.2831854820251465, -6.2831854820251465]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:192
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([
        340282346638528860000000000000000000000,
        340282346638528860000000000000000000000,
      ]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:194
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([
        -340282346638528860000000000000000000000,
        -340282346638528860000000000000000000000,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:196
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [f64x2([Infinity, Infinity])]),
  [i32x4([0xffffffff, 0xffffffff, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:198
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [f64x2([-Infinity, -Infinity])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:200
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
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
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:202
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      bytes("v128", [
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0xf8,
        0xff,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0xf8,
        0xff,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:204
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      bytes("v128", [
        0x44,
        0x44,
        0x44,
        0x0,
        0x0,
        0x0,
        0xf0,
        0x7f,
        0x44,
        0x44,
        0x44,
        0x0,
        0x0,
        0x0,
        0xf0,
        0x7f,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:206
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      bytes("v128", [
        0x44,
        0x44,
        0x44,
        0x0,
        0x0,
        0x0,
        0xf0,
        0xff,
        0x44,
        0x44,
        0x44,
        0x0,
        0x0,
        0x0,
        0xf0,
        0xff,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:208
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [f64x2([42, 42])]),
  [i32x4([0x2a, 0x2a, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:210
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [f64x2([-42, -42])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:212
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [f64x2([123456792, 123456792])]),
  [i32x4([0x75bcd18, 0x75bcd18, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:214
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f64x2_u_zero`, [
      f64x2([1234567890, 1234567890]),
    ]),
  [i32x4([0x499602d2, 0x499602d2, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:218
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.trunc_sat_f64x2_s_zero (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:219
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.trunc_sat_f64x2_u_zero (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:223
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.trunc_sat_f64x2_s_zero-arg-empty (result v128)
      (i32x4.trunc_sat_f64x2_s_zero)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_trunc_sat_f64x2.wast:231
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.trunc_sat_f64x2_u_zero-arg-empty (result v128)
      (i32x4.trunc_sat_f64x2_u_zero)
    )
  )`), `type mismatch`);
