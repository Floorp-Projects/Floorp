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

// ./test/core/int_literals.wast

// ./test/core/int_literals.wast:1
let $0 = instantiate(`(module
  (func (export "i32.test") (result i32) (return (i32.const 0x0bAdD00D)))
  (func (export "i32.umax") (result i32) (return (i32.const 0xffffffff)))
  (func (export "i32.smax") (result i32) (return (i32.const 0x7fffffff)))
  (func (export "i32.neg_smax") (result i32) (return (i32.const -0x7fffffff)))
  (func (export "i32.smin") (result i32) (return (i32.const -0x80000000)))
  (func (export "i32.alt_smin") (result i32) (return (i32.const 0x80000000)))
  (func (export "i32.inc_smin") (result i32) (return (i32.add (i32.const -0x80000000) (i32.const 1))))
  (func (export "i32.neg_zero") (result i32) (return (i32.const -0x0)))
  (func (export "i32.not_octal") (result i32) (return (i32.const 010)))
  (func (export "i32.unsigned_decimal") (result i32) (return (i32.const 4294967295)))
  (func (export "i32.plus_sign") (result i32) (return (i32.const +42)))

  (func (export "i64.test") (result i64) (return (i64.const 0x0CABBA6E0ba66a6e)))
  (func (export "i64.umax") (result i64) (return (i64.const 0xffffffffffffffff)))
  (func (export "i64.smax") (result i64) (return (i64.const 0x7fffffffffffffff)))
  (func (export "i64.neg_smax") (result i64) (return (i64.const -0x7fffffffffffffff)))
  (func (export "i64.smin") (result i64) (return (i64.const -0x8000000000000000)))
  (func (export "i64.alt_smin") (result i64) (return (i64.const 0x8000000000000000)))
  (func (export "i64.inc_smin") (result i64) (return (i64.add (i64.const -0x8000000000000000) (i64.const 1))))
  (func (export "i64.neg_zero") (result i64) (return (i64.const -0x0)))
  (func (export "i64.not_octal") (result i64) (return (i64.const 010)))
  (func (export "i64.unsigned_decimal") (result i64) (return (i64.const 18446744073709551615)))
  (func (export "i64.plus_sign") (result i64) (return (i64.const +42)))

  (func (export "i32-dec-sep1") (result i32) (i32.const 1_000_000))
  (func (export "i32-dec-sep2") (result i32) (i32.const 1_0_0_0))
  (func (export "i32-hex-sep1") (result i32) (i32.const 0xa_0f_00_99))
  (func (export "i32-hex-sep2") (result i32) (i32.const 0x1_a_A_0_f))

  (func (export "i64-dec-sep1") (result i64) (i64.const 1_000_000))
  (func (export "i64-dec-sep2") (result i64) (i64.const 1_0_0_0))
  (func (export "i64-hex-sep1") (result i64) (i64.const 0xa_f00f_0000_9999))
  (func (export "i64-hex-sep2") (result i64) (i64.const 0x1_a_A_0_f))
)`);

// ./test/core/int_literals.wast:37
assert_return(() => invoke($0, `i32.test`, []), [value("i32", 195940365)]);

// ./test/core/int_literals.wast:38
assert_return(() => invoke($0, `i32.umax`, []), [value("i32", -1)]);

// ./test/core/int_literals.wast:39
assert_return(() => invoke($0, `i32.smax`, []), [value("i32", 2147483647)]);

// ./test/core/int_literals.wast:40
assert_return(() => invoke($0, `i32.neg_smax`, []), [
  value("i32", -2147483647),
]);

// ./test/core/int_literals.wast:41
assert_return(() => invoke($0, `i32.smin`, []), [value("i32", -2147483648)]);

// ./test/core/int_literals.wast:42
assert_return(() => invoke($0, `i32.alt_smin`, []), [
  value("i32", -2147483648),
]);

// ./test/core/int_literals.wast:43
assert_return(() => invoke($0, `i32.inc_smin`, []), [
  value("i32", -2147483647),
]);

// ./test/core/int_literals.wast:44
assert_return(() => invoke($0, `i32.neg_zero`, []), [value("i32", 0)]);

// ./test/core/int_literals.wast:45
assert_return(() => invoke($0, `i32.not_octal`, []), [value("i32", 10)]);

// ./test/core/int_literals.wast:46
assert_return(() => invoke($0, `i32.unsigned_decimal`, []), [value("i32", -1)]);

// ./test/core/int_literals.wast:47
assert_return(() => invoke($0, `i32.plus_sign`, []), [value("i32", 42)]);

// ./test/core/int_literals.wast:49
assert_return(() => invoke($0, `i64.test`, []), [
  value("i64", 913028331277281902n),
]);

// ./test/core/int_literals.wast:50
assert_return(() => invoke($0, `i64.umax`, []), [value("i64", -1n)]);

// ./test/core/int_literals.wast:51
assert_return(() => invoke($0, `i64.smax`, []), [
  value("i64", 9223372036854775807n),
]);

// ./test/core/int_literals.wast:52
assert_return(() => invoke($0, `i64.neg_smax`, []), [
  value("i64", -9223372036854775807n),
]);

// ./test/core/int_literals.wast:53
assert_return(() => invoke($0, `i64.smin`, []), [
  value("i64", -9223372036854775808n),
]);

// ./test/core/int_literals.wast:54
assert_return(() => invoke($0, `i64.alt_smin`, []), [
  value("i64", -9223372036854775808n),
]);

// ./test/core/int_literals.wast:55
assert_return(() => invoke($0, `i64.inc_smin`, []), [
  value("i64", -9223372036854775807n),
]);

// ./test/core/int_literals.wast:56
assert_return(() => invoke($0, `i64.neg_zero`, []), [value("i64", 0n)]);

// ./test/core/int_literals.wast:57
assert_return(() => invoke($0, `i64.not_octal`, []), [value("i64", 10n)]);

// ./test/core/int_literals.wast:58
assert_return(() => invoke($0, `i64.unsigned_decimal`, []), [
  value("i64", -1n),
]);

// ./test/core/int_literals.wast:59
assert_return(() => invoke($0, `i64.plus_sign`, []), [value("i64", 42n)]);

// ./test/core/int_literals.wast:61
assert_return(() => invoke($0, `i32-dec-sep1`, []), [value("i32", 1000000)]);

// ./test/core/int_literals.wast:62
assert_return(() => invoke($0, `i32-dec-sep2`, []), [value("i32", 1000)]);

// ./test/core/int_literals.wast:63
assert_return(() => invoke($0, `i32-hex-sep1`, []), [value("i32", 168755353)]);

// ./test/core/int_literals.wast:64
assert_return(() => invoke($0, `i32-hex-sep2`, []), [value("i32", 109071)]);

// ./test/core/int_literals.wast:66
assert_return(() => invoke($0, `i64-dec-sep1`, []), [value("i64", 1000000n)]);

// ./test/core/int_literals.wast:67
assert_return(() => invoke($0, `i64-dec-sep2`, []), [value("i64", 1000n)]);

// ./test/core/int_literals.wast:68
assert_return(() => invoke($0, `i64-hex-sep1`, []), [
  value("i64", 3078696982321561n),
]);

// ./test/core/int_literals.wast:69
assert_return(() => invoke($0, `i64-hex-sep2`, []), [value("i64", 109071n)]);

// ./test/core/int_literals.wast:71
assert_malformed(
  () => instantiate(`(global i32 (i32.const _100)) `),
  `unknown operator`,
);

// ./test/core/int_literals.wast:75
assert_malformed(
  () => instantiate(`(global i32 (i32.const +_100)) `),
  `unknown operator`,
);

// ./test/core/int_literals.wast:79
assert_malformed(
  () => instantiate(`(global i32 (i32.const -_100)) `),
  `unknown operator`,
);

// ./test/core/int_literals.wast:83
assert_malformed(
  () => instantiate(`(global i32 (i32.const 99_)) `),
  `unknown operator`,
);

// ./test/core/int_literals.wast:87
assert_malformed(
  () => instantiate(`(global i32 (i32.const 1__000)) `),
  `unknown operator`,
);

// ./test/core/int_literals.wast:91
assert_malformed(
  () => instantiate(`(global i32 (i32.const _0x100)) `),
  `unknown operator`,
);

// ./test/core/int_literals.wast:95
assert_malformed(
  () => instantiate(`(global i32 (i32.const 0_x100)) `),
  `unknown operator`,
);

// ./test/core/int_literals.wast:99
assert_malformed(
  () => instantiate(`(global i32 (i32.const 0x_100)) `),
  `unknown operator`,
);

// ./test/core/int_literals.wast:103
assert_malformed(
  () => instantiate(`(global i32 (i32.const 0x00_)) `),
  `unknown operator`,
);

// ./test/core/int_literals.wast:107
assert_malformed(
  () => instantiate(`(global i32 (i32.const 0xff__ffff)) `),
  `unknown operator`,
);

// ./test/core/int_literals.wast:112
assert_malformed(
  () => instantiate(`(global i64 (i64.const _100)) `),
  `unknown operator`,
);

// ./test/core/int_literals.wast:116
assert_malformed(
  () => instantiate(`(global i64 (i64.const +_100)) `),
  `unknown operator`,
);

// ./test/core/int_literals.wast:120
assert_malformed(
  () => instantiate(`(global i64 (i64.const -_100)) `),
  `unknown operator`,
);

// ./test/core/int_literals.wast:124
assert_malformed(
  () => instantiate(`(global i64 (i64.const 99_)) `),
  `unknown operator`,
);

// ./test/core/int_literals.wast:128
assert_malformed(
  () => instantiate(`(global i64 (i64.const 1__000)) `),
  `unknown operator`,
);

// ./test/core/int_literals.wast:132
assert_malformed(
  () => instantiate(`(global i64 (i64.const _0x100)) `),
  `unknown operator`,
);

// ./test/core/int_literals.wast:136
assert_malformed(
  () => instantiate(`(global i64 (i64.const 0_x100)) `),
  `unknown operator`,
);

// ./test/core/int_literals.wast:140
assert_malformed(
  () => instantiate(`(global i64 (i64.const 0x_100)) `),
  `unknown operator`,
);

// ./test/core/int_literals.wast:144
assert_malformed(
  () => instantiate(`(global i64 (i64.const 0x00_)) `),
  `unknown operator`,
);

// ./test/core/int_literals.wast:148
assert_malformed(
  () => instantiate(`(global i64 (i64.const 0xff__ffff)) `),
  `unknown operator`,
);
