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

// ./test/core/int_exprs.wast

// ./test/core/int_exprs.wast:6
let $0 = instantiate(`(module
  (func (export "i32.no_fold_cmp_s_offset") (param $$x i32) (param $$y i32) (result i32)
    (i32.lt_s (i32.add (local.get $$x) (i32.const 1)) (i32.add (local.get $$y) (i32.const 1))))
  (func (export "i32.no_fold_cmp_u_offset") (param $$x i32) (param $$y i32) (result i32)
    (i32.lt_u (i32.add (local.get $$x) (i32.const 1)) (i32.add (local.get $$y) (i32.const 1))))

  (func (export "i64.no_fold_cmp_s_offset") (param $$x i64) (param $$y i64) (result i32)
    (i64.lt_s (i64.add (local.get $$x) (i64.const 1)) (i64.add (local.get $$y) (i64.const 1))))
  (func (export "i64.no_fold_cmp_u_offset") (param $$x i64) (param $$y i64) (result i32)
    (i64.lt_u (i64.add (local.get $$x) (i64.const 1)) (i64.add (local.get $$y) (i64.const 1))))
)`);

// ./test/core/int_exprs.wast:18
assert_return(() => invoke($0, `i32.no_fold_cmp_s_offset`, [2147483647, 0]), [
  value("i32", 1),
]);

// ./test/core/int_exprs.wast:19
assert_return(() => invoke($0, `i32.no_fold_cmp_u_offset`, [-1, 0]), [
  value("i32", 1),
]);

// ./test/core/int_exprs.wast:20
assert_return(
  () => invoke($0, `i64.no_fold_cmp_s_offset`, [9223372036854775807n, 0n]),
  [value("i32", 1)],
);

// ./test/core/int_exprs.wast:21
assert_return(() => invoke($0, `i64.no_fold_cmp_u_offset`, [-1n, 0n]), [
  value("i32", 1),
]);

// ./test/core/int_exprs.wast:25
let $1 = instantiate(`(module
  (func (export "i64.no_fold_wrap_extend_s") (param $$x i64) (result i64)
    (i64.extend_i32_s (i32.wrap_i64 (local.get $$x))))
)`);

// ./test/core/int_exprs.wast:30
assert_return(
  () => invoke($1, `i64.no_fold_wrap_extend_s`, [4538991236898928n]),
  [value("i64", 1079009392n)],
);

// ./test/core/int_exprs.wast:31
assert_return(
  () => invoke($1, `i64.no_fold_wrap_extend_s`, [45230338458316960n]),
  [value("i64", -790564704n)],
);

// ./test/core/int_exprs.wast:35
let $2 = instantiate(`(module
  (func (export "i64.no_fold_wrap_extend_u") (param $$x i64) (result i64)
    (i64.extend_i32_u (i32.wrap_i64 (local.get $$x))))
)`);

// ./test/core/int_exprs.wast:40
assert_return(
  () => invoke($2, `i64.no_fold_wrap_extend_u`, [4538991236898928n]),
  [value("i64", 1079009392n)],
);

// ./test/core/int_exprs.wast:44
let $3 = instantiate(`(module
  (func (export "i32.no_fold_shl_shr_s") (param $$x i32) (result i32)
    (i32.shr_s (i32.shl (local.get $$x) (i32.const 1)) (i32.const 1)))
  (func (export "i32.no_fold_shl_shr_u") (param $$x i32) (result i32)
    (i32.shr_u (i32.shl (local.get $$x) (i32.const 1)) (i32.const 1)))

  (func (export "i64.no_fold_shl_shr_s") (param $$x i64) (result i64)
    (i64.shr_s (i64.shl (local.get $$x) (i64.const 1)) (i64.const 1)))
  (func (export "i64.no_fold_shl_shr_u") (param $$x i64) (result i64)
    (i64.shr_u (i64.shl (local.get $$x) (i64.const 1)) (i64.const 1)))
)`);

// ./test/core/int_exprs.wast:56
assert_return(() => invoke($3, `i32.no_fold_shl_shr_s`, [-2147483648]), [
  value("i32", 0),
]);

// ./test/core/int_exprs.wast:57
assert_return(() => invoke($3, `i32.no_fold_shl_shr_u`, [-2147483648]), [
  value("i32", 0),
]);

// ./test/core/int_exprs.wast:58
assert_return(
  () => invoke($3, `i64.no_fold_shl_shr_s`, [-9223372036854775808n]),
  [value("i64", 0n)],
);

// ./test/core/int_exprs.wast:59
assert_return(
  () => invoke($3, `i64.no_fold_shl_shr_u`, [-9223372036854775808n]),
  [value("i64", 0n)],
);

// ./test/core/int_exprs.wast:63
let $4 = instantiate(`(module
  (func (export "i32.no_fold_shr_s_shl") (param $$x i32) (result i32)
    (i32.shl (i32.shr_s (local.get $$x) (i32.const 1)) (i32.const 1)))
  (func (export "i32.no_fold_shr_u_shl") (param $$x i32) (result i32)
    (i32.shl (i32.shr_u (local.get $$x) (i32.const 1)) (i32.const 1)))

  (func (export "i64.no_fold_shr_s_shl") (param $$x i64) (result i64)
    (i64.shl (i64.shr_s (local.get $$x) (i64.const 1)) (i64.const 1)))
  (func (export "i64.no_fold_shr_u_shl") (param $$x i64) (result i64)
    (i64.shl (i64.shr_u (local.get $$x) (i64.const 1)) (i64.const 1)))
)`);

// ./test/core/int_exprs.wast:75
assert_return(() => invoke($4, `i32.no_fold_shr_s_shl`, [1]), [
  value("i32", 0),
]);

// ./test/core/int_exprs.wast:76
assert_return(() => invoke($4, `i32.no_fold_shr_u_shl`, [1]), [
  value("i32", 0),
]);

// ./test/core/int_exprs.wast:77
assert_return(() => invoke($4, `i64.no_fold_shr_s_shl`, [1n]), [
  value("i64", 0n),
]);

// ./test/core/int_exprs.wast:78
assert_return(() => invoke($4, `i64.no_fold_shr_u_shl`, [1n]), [
  value("i64", 0n),
]);

// ./test/core/int_exprs.wast:82
let $5 = instantiate(`(module
  (func (export "i32.no_fold_div_s_mul") (param $$x i32) (result i32)
    (i32.mul (i32.div_s (local.get $$x) (i32.const 6)) (i32.const 6)))
  (func (export "i32.no_fold_div_u_mul") (param $$x i32) (result i32)
    (i32.mul (i32.div_u (local.get $$x) (i32.const 6)) (i32.const 6)))

  (func (export "i64.no_fold_div_s_mul") (param $$x i64) (result i64)
    (i64.mul (i64.div_s (local.get $$x) (i64.const 6)) (i64.const 6)))
  (func (export "i64.no_fold_div_u_mul") (param $$x i64) (result i64)
    (i64.mul (i64.div_u (local.get $$x) (i64.const 6)) (i64.const 6)))
)`);

// ./test/core/int_exprs.wast:94
assert_return(() => invoke($5, `i32.no_fold_div_s_mul`, [1]), [
  value("i32", 0),
]);

// ./test/core/int_exprs.wast:95
assert_return(() => invoke($5, `i32.no_fold_div_u_mul`, [1]), [
  value("i32", 0),
]);

// ./test/core/int_exprs.wast:96
assert_return(() => invoke($5, `i64.no_fold_div_s_mul`, [1n]), [
  value("i64", 0n),
]);

// ./test/core/int_exprs.wast:97
assert_return(() => invoke($5, `i64.no_fold_div_u_mul`, [1n]), [
  value("i64", 0n),
]);

// ./test/core/int_exprs.wast:101
let $6 = instantiate(`(module
  (func (export "i32.no_fold_div_s_self") (param $$x i32) (result i32)
    (i32.div_s (local.get $$x) (local.get $$x)))
  (func (export "i32.no_fold_div_u_self") (param $$x i32) (result i32)
    (i32.div_u (local.get $$x) (local.get $$x)))

  (func (export "i64.no_fold_div_s_self") (param $$x i64) (result i64)
    (i64.div_s (local.get $$x) (local.get $$x)))
  (func (export "i64.no_fold_div_u_self") (param $$x i64) (result i64)
    (i64.div_u (local.get $$x) (local.get $$x)))
)`);

// ./test/core/int_exprs.wast:113
assert_trap(
  () => invoke($6, `i32.no_fold_div_s_self`, [0]),
  `integer divide by zero`,
);

// ./test/core/int_exprs.wast:114
assert_trap(
  () => invoke($6, `i32.no_fold_div_u_self`, [0]),
  `integer divide by zero`,
);

// ./test/core/int_exprs.wast:115
assert_trap(
  () => invoke($6, `i64.no_fold_div_s_self`, [0n]),
  `integer divide by zero`,
);

// ./test/core/int_exprs.wast:116
assert_trap(
  () => invoke($6, `i64.no_fold_div_u_self`, [0n]),
  `integer divide by zero`,
);

// ./test/core/int_exprs.wast:120
let $7 = instantiate(`(module
  (func (export "i32.no_fold_rem_s_self") (param $$x i32) (result i32)
    (i32.rem_s (local.get $$x) (local.get $$x)))
  (func (export "i32.no_fold_rem_u_self") (param $$x i32) (result i32)
    (i32.rem_u (local.get $$x) (local.get $$x)))

  (func (export "i64.no_fold_rem_s_self") (param $$x i64) (result i64)
    (i64.rem_s (local.get $$x) (local.get $$x)))
  (func (export "i64.no_fold_rem_u_self") (param $$x i64) (result i64)
    (i64.rem_u (local.get $$x) (local.get $$x)))
)`);

// ./test/core/int_exprs.wast:132
assert_trap(
  () => invoke($7, `i32.no_fold_rem_s_self`, [0]),
  `integer divide by zero`,
);

// ./test/core/int_exprs.wast:133
assert_trap(
  () => invoke($7, `i32.no_fold_rem_u_self`, [0]),
  `integer divide by zero`,
);

// ./test/core/int_exprs.wast:134
assert_trap(
  () => invoke($7, `i64.no_fold_rem_s_self`, [0n]),
  `integer divide by zero`,
);

// ./test/core/int_exprs.wast:135
assert_trap(
  () => invoke($7, `i64.no_fold_rem_u_self`, [0n]),
  `integer divide by zero`,
);

// ./test/core/int_exprs.wast:139
let $8 = instantiate(`(module
  (func (export "i32.no_fold_mul_div_s") (param $$x i32) (result i32)
    (i32.div_s (i32.mul (local.get $$x) (i32.const 6)) (i32.const 6)))
  (func (export "i32.no_fold_mul_div_u") (param $$x i32) (result i32)
    (i32.div_u (i32.mul (local.get $$x) (i32.const 6)) (i32.const 6)))

  (func (export "i64.no_fold_mul_div_s") (param $$x i64) (result i64)
    (i64.div_s (i64.mul (local.get $$x) (i64.const 6)) (i64.const 6)))
  (func (export "i64.no_fold_mul_div_u") (param $$x i64) (result i64)
    (i64.div_u (i64.mul (local.get $$x) (i64.const 6)) (i64.const 6)))
)`);

// ./test/core/int_exprs.wast:151
assert_return(() => invoke($8, `i32.no_fold_mul_div_s`, [-2147483648]), [
  value("i32", 0),
]);

// ./test/core/int_exprs.wast:152
assert_return(() => invoke($8, `i32.no_fold_mul_div_u`, [-2147483648]), [
  value("i32", 0),
]);

// ./test/core/int_exprs.wast:153
assert_return(
  () => invoke($8, `i64.no_fold_mul_div_s`, [-9223372036854775808n]),
  [value("i64", 0n)],
);

// ./test/core/int_exprs.wast:154
assert_return(
  () => invoke($8, `i64.no_fold_mul_div_u`, [-9223372036854775808n]),
  [value("i64", 0n)],
);

// ./test/core/int_exprs.wast:158
let $9 = instantiate(`(module
  (func (export "i32.no_fold_div_s_2") (param $$x i32) (result i32)
    (i32.div_s (local.get $$x) (i32.const 2)))

  (func (export "i64.no_fold_div_s_2") (param $$x i64) (result i64)
    (i64.div_s (local.get $$x) (i64.const 2)))
)`);

// ./test/core/int_exprs.wast:166
assert_return(() => invoke($9, `i32.no_fold_div_s_2`, [-11]), [
  value("i32", -5),
]);

// ./test/core/int_exprs.wast:167
assert_return(() => invoke($9, `i64.no_fold_div_s_2`, [-11n]), [
  value("i64", -5n),
]);

// ./test/core/int_exprs.wast:171
let $10 = instantiate(`(module
  (func (export "i32.no_fold_rem_s_2") (param $$x i32) (result i32)
    (i32.rem_s (local.get $$x) (i32.const 2)))

  (func (export "i64.no_fold_rem_s_2") (param $$x i64) (result i64)
    (i64.rem_s (local.get $$x) (i64.const 2)))
)`);

// ./test/core/int_exprs.wast:179
assert_return(() => invoke($10, `i32.no_fold_rem_s_2`, [-11]), [
  value("i32", -1),
]);

// ./test/core/int_exprs.wast:180
assert_return(() => invoke($10, `i64.no_fold_rem_s_2`, [-11n]), [
  value("i64", -1n),
]);

// ./test/core/int_exprs.wast:184
let $11 = instantiate(`(module
  (func (export "i32.div_s_0") (param $$x i32) (result i32)
    (i32.div_s (local.get $$x) (i32.const 0)))
  (func (export "i32.div_u_0") (param $$x i32) (result i32)
    (i32.div_u (local.get $$x) (i32.const 0)))

  (func (export "i64.div_s_0") (param $$x i64) (result i64)
    (i64.div_s (local.get $$x) (i64.const 0)))
  (func (export "i64.div_u_0") (param $$x i64) (result i64)
    (i64.div_u (local.get $$x) (i64.const 0)))
)`);

// ./test/core/int_exprs.wast:196
assert_trap(() => invoke($11, `i32.div_s_0`, [71]), `integer divide by zero`);

// ./test/core/int_exprs.wast:197
assert_trap(() => invoke($11, `i32.div_u_0`, [71]), `integer divide by zero`);

// ./test/core/int_exprs.wast:198
assert_trap(() => invoke($11, `i64.div_s_0`, [71n]), `integer divide by zero`);

// ./test/core/int_exprs.wast:199
assert_trap(() => invoke($11, `i64.div_u_0`, [71n]), `integer divide by zero`);

// ./test/core/int_exprs.wast:203
let $12 = instantiate(`(module
  (func (export "i32.div_s_3") (param $$x i32) (result i32)
    (i32.div_s (local.get $$x) (i32.const 3)))
  (func (export "i32.div_u_3") (param $$x i32) (result i32)
    (i32.div_u (local.get $$x) (i32.const 3)))

  (func (export "i64.div_s_3") (param $$x i64) (result i64)
    (i64.div_s (local.get $$x) (i64.const 3)))
  (func (export "i64.div_u_3") (param $$x i64) (result i64)
    (i64.div_u (local.get $$x) (i64.const 3)))
)`);

// ./test/core/int_exprs.wast:215
assert_return(() => invoke($12, `i32.div_s_3`, [71]), [value("i32", 23)]);

// ./test/core/int_exprs.wast:216
assert_return(() => invoke($12, `i32.div_s_3`, [1610612736]), [
  value("i32", 536870912),
]);

// ./test/core/int_exprs.wast:217
assert_return(() => invoke($12, `i32.div_u_3`, [71]), [value("i32", 23)]);

// ./test/core/int_exprs.wast:218
assert_return(() => invoke($12, `i32.div_u_3`, [-1073741824]), [
  value("i32", 1073741824),
]);

// ./test/core/int_exprs.wast:219
assert_return(() => invoke($12, `i64.div_s_3`, [71n]), [value("i64", 23n)]);

// ./test/core/int_exprs.wast:220
assert_return(() => invoke($12, `i64.div_s_3`, [3458764513820540928n]), [
  value("i64", 1152921504606846976n),
]);

// ./test/core/int_exprs.wast:221
assert_return(() => invoke($12, `i64.div_u_3`, [71n]), [value("i64", 23n)]);

// ./test/core/int_exprs.wast:222
assert_return(() => invoke($12, `i64.div_u_3`, [-4611686018427387904n]), [
  value("i64", 4611686018427387904n),
]);

// ./test/core/int_exprs.wast:226
let $13 = instantiate(`(module
  (func (export "i32.div_s_5") (param $$x i32) (result i32)
    (i32.div_s (local.get $$x) (i32.const 5)))
  (func (export "i32.div_u_5") (param $$x i32) (result i32)
    (i32.div_u (local.get $$x) (i32.const 5)))

  (func (export "i64.div_s_5") (param $$x i64) (result i64)
    (i64.div_s (local.get $$x) (i64.const 5)))
  (func (export "i64.div_u_5") (param $$x i64) (result i64)
    (i64.div_u (local.get $$x) (i64.const 5)))
)`);

// ./test/core/int_exprs.wast:238
assert_return(() => invoke($13, `i32.div_s_5`, [71]), [value("i32", 14)]);

// ./test/core/int_exprs.wast:239
assert_return(() => invoke($13, `i32.div_s_5`, [1342177280]), [
  value("i32", 268435456),
]);

// ./test/core/int_exprs.wast:240
assert_return(() => invoke($13, `i32.div_u_5`, [71]), [value("i32", 14)]);

// ./test/core/int_exprs.wast:241
assert_return(() => invoke($13, `i32.div_u_5`, [-1610612736]), [
  value("i32", 536870912),
]);

// ./test/core/int_exprs.wast:242
assert_return(() => invoke($13, `i64.div_s_5`, [71n]), [value("i64", 14n)]);

// ./test/core/int_exprs.wast:243
assert_return(() => invoke($13, `i64.div_s_5`, [5764607523034234880n]), [
  value("i64", 1152921504606846976n),
]);

// ./test/core/int_exprs.wast:244
assert_return(() => invoke($13, `i64.div_u_5`, [71n]), [value("i64", 14n)]);

// ./test/core/int_exprs.wast:245
assert_return(() => invoke($13, `i64.div_u_5`, [-6917529027641081856n]), [
  value("i64", 2305843009213693952n),
]);

// ./test/core/int_exprs.wast:249
let $14 = instantiate(`(module
  (func (export "i32.div_s_7") (param $$x i32) (result i32)
    (i32.div_s (local.get $$x) (i32.const 7)))
  (func (export "i32.div_u_7") (param $$x i32) (result i32)
    (i32.div_u (local.get $$x) (i32.const 7)))

  (func (export "i64.div_s_7") (param $$x i64) (result i64)
    (i64.div_s (local.get $$x) (i64.const 7)))
  (func (export "i64.div_u_7") (param $$x i64) (result i64)
    (i64.div_u (local.get $$x) (i64.const 7)))
)`);

// ./test/core/int_exprs.wast:261
assert_return(() => invoke($14, `i32.div_s_7`, [71]), [value("i32", 10)]);

// ./test/core/int_exprs.wast:262
assert_return(() => invoke($14, `i32.div_s_7`, [1879048192]), [
  value("i32", 268435456),
]);

// ./test/core/int_exprs.wast:263
assert_return(() => invoke($14, `i32.div_u_7`, [71]), [value("i32", 10)]);

// ./test/core/int_exprs.wast:264
assert_return(() => invoke($14, `i32.div_u_7`, [-536870912]), [
  value("i32", 536870912),
]);

// ./test/core/int_exprs.wast:265
assert_return(() => invoke($14, `i64.div_s_7`, [71n]), [value("i64", 10n)]);

// ./test/core/int_exprs.wast:266
assert_return(() => invoke($14, `i64.div_s_7`, [8070450532247928832n]), [
  value("i64", 1152921504606846976n),
]);

// ./test/core/int_exprs.wast:267
assert_return(() => invoke($14, `i64.div_u_7`, [71n]), [value("i64", 10n)]);

// ./test/core/int_exprs.wast:268
assert_return(() => invoke($14, `i64.div_u_7`, [-2305843009213693952n]), [
  value("i64", 2305843009213693952n),
]);

// ./test/core/int_exprs.wast:272
let $15 = instantiate(`(module
  (func (export "i32.rem_s_3") (param $$x i32) (result i32)
    (i32.rem_s (local.get $$x) (i32.const 3)))
  (func (export "i32.rem_u_3") (param $$x i32) (result i32)
    (i32.rem_u (local.get $$x) (i32.const 3)))

  (func (export "i64.rem_s_3") (param $$x i64) (result i64)
    (i64.rem_s (local.get $$x) (i64.const 3)))
  (func (export "i64.rem_u_3") (param $$x i64) (result i64)
    (i64.rem_u (local.get $$x) (i64.const 3)))
)`);

// ./test/core/int_exprs.wast:284
assert_return(() => invoke($15, `i32.rem_s_3`, [71]), [value("i32", 2)]);

// ./test/core/int_exprs.wast:285
assert_return(() => invoke($15, `i32.rem_s_3`, [1610612736]), [
  value("i32", 0),
]);

// ./test/core/int_exprs.wast:286
assert_return(() => invoke($15, `i32.rem_u_3`, [71]), [value("i32", 2)]);

// ./test/core/int_exprs.wast:287
assert_return(() => invoke($15, `i32.rem_u_3`, [-1073741824]), [
  value("i32", 0),
]);

// ./test/core/int_exprs.wast:288
assert_return(() => invoke($15, `i64.rem_s_3`, [71n]), [value("i64", 2n)]);

// ./test/core/int_exprs.wast:289
assert_return(() => invoke($15, `i64.rem_s_3`, [3458764513820540928n]), [
  value("i64", 0n),
]);

// ./test/core/int_exprs.wast:290
assert_return(() => invoke($15, `i64.rem_u_3`, [71n]), [value("i64", 2n)]);

// ./test/core/int_exprs.wast:291
assert_return(() => invoke($15, `i64.rem_u_3`, [-4611686018427387904n]), [
  value("i64", 0n),
]);

// ./test/core/int_exprs.wast:295
let $16 = instantiate(`(module
  (func (export "i32.rem_s_5") (param $$x i32) (result i32)
    (i32.rem_s (local.get $$x) (i32.const 5)))
  (func (export "i32.rem_u_5") (param $$x i32) (result i32)
    (i32.rem_u (local.get $$x) (i32.const 5)))

  (func (export "i64.rem_s_5") (param $$x i64) (result i64)
    (i64.rem_s (local.get $$x) (i64.const 5)))
  (func (export "i64.rem_u_5") (param $$x i64) (result i64)
    (i64.rem_u (local.get $$x) (i64.const 5)))
)`);

// ./test/core/int_exprs.wast:307
assert_return(() => invoke($16, `i32.rem_s_5`, [71]), [value("i32", 1)]);

// ./test/core/int_exprs.wast:308
assert_return(() => invoke($16, `i32.rem_s_5`, [1342177280]), [
  value("i32", 0),
]);

// ./test/core/int_exprs.wast:309
assert_return(() => invoke($16, `i32.rem_u_5`, [71]), [value("i32", 1)]);

// ./test/core/int_exprs.wast:310
assert_return(() => invoke($16, `i32.rem_u_5`, [-1610612736]), [
  value("i32", 0),
]);

// ./test/core/int_exprs.wast:311
assert_return(() => invoke($16, `i64.rem_s_5`, [71n]), [value("i64", 1n)]);

// ./test/core/int_exprs.wast:312
assert_return(() => invoke($16, `i64.rem_s_5`, [5764607523034234880n]), [
  value("i64", 0n),
]);

// ./test/core/int_exprs.wast:313
assert_return(() => invoke($16, `i64.rem_u_5`, [71n]), [value("i64", 1n)]);

// ./test/core/int_exprs.wast:314
assert_return(() => invoke($16, `i64.rem_u_5`, [-6917529027641081856n]), [
  value("i64", 0n),
]);

// ./test/core/int_exprs.wast:318
let $17 = instantiate(`(module
  (func (export "i32.rem_s_7") (param $$x i32) (result i32)
    (i32.rem_s (local.get $$x) (i32.const 7)))
  (func (export "i32.rem_u_7") (param $$x i32) (result i32)
    (i32.rem_u (local.get $$x) (i32.const 7)))

  (func (export "i64.rem_s_7") (param $$x i64) (result i64)
    (i64.rem_s (local.get $$x) (i64.const 7)))
  (func (export "i64.rem_u_7") (param $$x i64) (result i64)
    (i64.rem_u (local.get $$x) (i64.const 7)))
)`);

// ./test/core/int_exprs.wast:330
assert_return(() => invoke($17, `i32.rem_s_7`, [71]), [value("i32", 1)]);

// ./test/core/int_exprs.wast:331
assert_return(() => invoke($17, `i32.rem_s_7`, [1879048192]), [
  value("i32", 0),
]);

// ./test/core/int_exprs.wast:332
assert_return(() => invoke($17, `i32.rem_u_7`, [71]), [value("i32", 1)]);

// ./test/core/int_exprs.wast:333
assert_return(() => invoke($17, `i32.rem_u_7`, [-536870912]), [
  value("i32", 0),
]);

// ./test/core/int_exprs.wast:334
assert_return(() => invoke($17, `i64.rem_s_7`, [71n]), [value("i64", 1n)]);

// ./test/core/int_exprs.wast:335
assert_return(() => invoke($17, `i64.rem_s_7`, [8070450532247928832n]), [
  value("i64", 0n),
]);

// ./test/core/int_exprs.wast:336
assert_return(() => invoke($17, `i64.rem_u_7`, [71n]), [value("i64", 1n)]);

// ./test/core/int_exprs.wast:337
assert_return(() => invoke($17, `i64.rem_u_7`, [-2305843009213693952n]), [
  value("i64", 0n),
]);

// ./test/core/int_exprs.wast:341
let $18 = instantiate(`(module
  (func (export "i32.no_fold_div_neg1") (param $$x i32) (result i32)
    (i32.div_s (local.get $$x) (i32.const -1)))

  (func (export "i64.no_fold_div_neg1") (param $$x i64) (result i64)
    (i64.div_s (local.get $$x) (i64.const -1)))
)`);

// ./test/core/int_exprs.wast:349
assert_trap(
  () => invoke($18, `i32.no_fold_div_neg1`, [-2147483648]),
  `integer overflow`,
);

// ./test/core/int_exprs.wast:350
assert_trap(
  () => invoke($18, `i64.no_fold_div_neg1`, [-9223372036854775808n]),
  `integer overflow`,
);
