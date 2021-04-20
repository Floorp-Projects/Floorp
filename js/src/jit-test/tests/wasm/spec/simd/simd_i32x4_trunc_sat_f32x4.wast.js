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

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:3
let $0 = instantiate(`(module
  (func (export "i32x4.trunc_sat_f32x4_s") (param v128) (result v128) (i32x4.trunc_sat_f32x4_s (local.get 0)))
  (func (export "i32x4.trunc_sat_f32x4_u") (param v128) (result v128) (i32x4.trunc_sat_f32x4_u (local.get 0)))
)`);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:10
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f32x4_s`, [f32x4([0, 0, 0, 0])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:12
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
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

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:14
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f32x4_s`, [f32x4([1.5, 1.5, 1.5, 1.5])]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:16
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [f32x4([-1.5, -1.5, -1.5, -1.5])]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:18
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f32x4_s`, [f32x4([1.9, 1.9, 1.9, 1.9])]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:20
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f32x4_s`, [f32x4([2, 2, 2, 2])]),
  [i32x4([0x2, 0x2, 0x2, 0x2])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:22
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [f32x4([-1.9, -1.9, -1.9, -1.9])]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:24
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f32x4_s`, [f32x4([-2, -2, -2, -2])]),
  [i32x4([0xfffffffe, 0xfffffffe, 0xfffffffe, 0xfffffffe])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:26
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      f32x4([2147483500, 2147483500, 2147483500, 2147483500]),
    ]),
  [i32x4([0x7fffff80, 0x7fffff80, 0x7fffff80, 0x7fffff80])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:28
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      f32x4([-2147483500, -2147483500, -2147483500, -2147483500]),
    ]),
  [i32x4([0x80000080, 0x80000080, 0x80000080, 0x80000080])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:30
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      f32x4([2147483600, 2147483600, 2147483600, 2147483600]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:32
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      f32x4([-2147483600, -2147483600, -2147483600, -2147483600]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:34
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      f32x4([4294967300, 4294967300, 4294967300, 4294967300]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:36
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      f32x4([-4294967300, -4294967300, -4294967300, -4294967300]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:38
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      f32x4([2147483600, 2147483600, 2147483600, 2147483600]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:40
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      f32x4([-2147483600, -2147483600, -2147483600, -2147483600]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:42
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      f32x4([4294967300, 4294967300, 4294967300, 4294967300]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:44
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      f32x4([4294967300, 4294967300, 4294967300, 4294967300]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:46
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      f32x4([4294967300, 4294967300, 4294967300, 4294967300]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:48
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:50
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:52
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:54
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:56
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f32x4_s`, [f32x4([0.5, 0.5, 0.5, 0.5])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:58
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [f32x4([-0.5, -0.5, -0.5, -0.5])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:60
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f32x4_s`, [f32x4([1, 1, 1, 1])]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:62
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f32x4_s`, [f32x4([-1, -1, -1, -1])]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:64
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f32x4_s`, [f32x4([1.1, 1.1, 1.1, 1.1])]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:66
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [f32x4([-1.1, -1.1, -1.1, -1.1])]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:68
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [i32x4([0x6, 0x6, 0x6, 0x6])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:70
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [i32x4([0xfffffffa, 0xfffffffa, 0xfffffffa, 0xfffffffa])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:72
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:74
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:76
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f32x4_s`, [f32x4([0.9, 0.9, 0.9, 0.9])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:78
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [f32x4([-0.9, -0.9, -0.9, -0.9])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:80
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      f32x4([0.99999994, 0.99999994, 0.99999994, 0.99999994]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:82
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      f32x4([-0.99999994, -0.99999994, -0.99999994, -0.99999994]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:84
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [i32x4([0x6, 0x6, 0x6, 0x6])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:86
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [i32x4([0xfffffffa, 0xfffffffa, 0xfffffffa, 0xfffffffa])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:88
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:90
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:92
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [i32x4([0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:94
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:96
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
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
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:98
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:100
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      bytes("v128", [
        0x44,
        0x44,
        0xc4,
        0x7f,
        0x44,
        0x44,
        0xc4,
        0x7f,
        0x44,
        0x44,
        0xc4,
        0x7f,
        0x44,
        0x44,
        0xc4,
        0x7f,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:102
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      bytes("v128", [
        0x44,
        0x44,
        0xc4,
        0xff,
        0x44,
        0x44,
        0xc4,
        0xff,
        0x44,
        0x44,
        0xc4,
        0xff,
        0x44,
        0x44,
        0xc4,
        0xff,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:104
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f32x4_s`, [f32x4([42, 42, 42, 42])]),
  [i32x4([0x2a, 0x2a, 0x2a, 0x2a])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:106
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f32x4_s`, [f32x4([-42, -42, -42, -42])]),
  [i32x4([0xffffffd6, 0xffffffd6, 0xffffffd6, 0xffffffd6])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:108
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      f32x4([123456790, 123456790, 123456790, 123456790]),
    ]),
  [i32x4([0x75bcd18, 0x75bcd18, 0x75bcd18, 0x75bcd18])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:110
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_s`, [
      f32x4([1234568000, 1234568000, 1234568000, 1234568000]),
    ]),
  [i32x4([0x49960300, 0x49960300, 0x49960300, 0x49960300])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:114
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f32x4_u`, [f32x4([0, 0, 0, 0])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:116
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
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

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:118
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f32x4_u`, [f32x4([1.5, 1.5, 1.5, 1.5])]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:120
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [f32x4([-1.5, -1.5, -1.5, -1.5])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:122
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f32x4_u`, [f32x4([1.9, 1.9, 1.9, 1.9])]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:124
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f32x4_u`, [f32x4([2, 2, 2, 2])]),
  [i32x4([0x2, 0x2, 0x2, 0x2])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:126
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [f32x4([-1.9, -1.9, -1.9, -1.9])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:128
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f32x4_u`, [f32x4([-2, -2, -2, -2])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:130
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      f32x4([2147483500, 2147483500, 2147483500, 2147483500]),
    ]),
  [i32x4([0x7fffff80, 0x7fffff80, 0x7fffff80, 0x7fffff80])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:132
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      f32x4([-2147483500, -2147483500, -2147483500, -2147483500]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:134
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      f32x4([2147483600, 2147483600, 2147483600, 2147483600]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:136
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      f32x4([-2147483600, -2147483600, -2147483600, -2147483600]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:138
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      f32x4([4294967300, 4294967300, 4294967300, 4294967300]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:140
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      f32x4([-4294967300, -4294967300, -4294967300, -4294967300]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:142
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      f32x4([2147483600, 2147483600, 2147483600, 2147483600]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:144
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      f32x4([-2147483600, -2147483600, -2147483600, -2147483600]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:146
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      f32x4([4294967300, 4294967300, 4294967300, 4294967300]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:148
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      f32x4([4294967300, 4294967300, 4294967300, 4294967300]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:150
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      f32x4([4294967300, 4294967300, 4294967300, 4294967300]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:152
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:154
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:156
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:158
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:160
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f32x4_u`, [f32x4([0.5, 0.5, 0.5, 0.5])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:162
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [f32x4([-0.5, -0.5, -0.5, -0.5])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:164
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f32x4_u`, [f32x4([1, 1, 1, 1])]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:166
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f32x4_u`, [f32x4([-1, -1, -1, -1])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:168
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f32x4_u`, [f32x4([1.1, 1.1, 1.1, 1.1])]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:170
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [f32x4([-1.1, -1.1, -1.1, -1.1])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:172
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [i32x4([0x6, 0x6, 0x6, 0x6])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:174
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:176
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:178
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:180
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f32x4_u`, [f32x4([0.9, 0.9, 0.9, 0.9])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:182
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [f32x4([-0.9, -0.9, -0.9, -0.9])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:184
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      f32x4([0.99999994, 0.99999994, 0.99999994, 0.99999994]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:186
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      f32x4([-0.99999994, -0.99999994, -0.99999994, -0.99999994]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:188
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [i32x4([0x6, 0x6, 0x6, 0x6])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:190
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:192
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:194
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:196
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:198
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:200
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
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
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:202
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:204
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      bytes("v128", [
        0x44,
        0x44,
        0xc4,
        0x7f,
        0x44,
        0x44,
        0xc4,
        0x7f,
        0x44,
        0x44,
        0xc4,
        0x7f,
        0x44,
        0x44,
        0xc4,
        0x7f,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:206
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      bytes("v128", [
        0x44,
        0x44,
        0xc4,
        0xff,
        0x44,
        0x44,
        0xc4,
        0xff,
        0x44,
        0x44,
        0xc4,
        0xff,
        0x44,
        0x44,
        0xc4,
        0xff,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:208
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f32x4_u`, [f32x4([42, 42, 42, 42])]),
  [i32x4([0x2a, 0x2a, 0x2a, 0x2a])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:210
assert_return(
  () => invoke($0, `i32x4.trunc_sat_f32x4_u`, [f32x4([-42, -42, -42, -42])]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:212
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      f32x4([123456790, 123456790, 123456790, 123456790]),
    ]),
  [i32x4([0x75bcd18, 0x75bcd18, 0x75bcd18, 0x75bcd18])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:214
assert_return(
  () =>
    invoke($0, `i32x4.trunc_sat_f32x4_u`, [
      f32x4([1234568000, 1234568000, 1234568000, 1234568000]),
    ]),
  [i32x4([0x49960300, 0x49960300, 0x49960300, 0x49960300])],
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:218
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.trunc_sat_f32x4_s (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:219
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.trunc_sat_f32x4_u (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:223
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.trunc_sat_f32x4_s-arg-empty (result v128)
      (i32x4.trunc_sat_f32x4_s)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_trunc_sat_f32x4.wast:231
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.trunc_sat_f32x4_u-arg-empty (result v128)
      (i32x4.trunc_sat_f32x4_u)
    )
  )`), `type mismatch`);
