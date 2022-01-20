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

// ./test/core/i64.wast

// ./test/core/i64.wast:3
let $0 = instantiate(`(module
  (func (export "add") (param $$x i64) (param $$y i64) (result i64) (i64.add (local.get $$x) (local.get $$y)))
  (func (export "sub") (param $$x i64) (param $$y i64) (result i64) (i64.sub (local.get $$x) (local.get $$y)))
  (func (export "mul") (param $$x i64) (param $$y i64) (result i64) (i64.mul (local.get $$x) (local.get $$y)))
  (func (export "div_s") (param $$x i64) (param $$y i64) (result i64) (i64.div_s (local.get $$x) (local.get $$y)))
  (func (export "div_u") (param $$x i64) (param $$y i64) (result i64) (i64.div_u (local.get $$x) (local.get $$y)))
  (func (export "rem_s") (param $$x i64) (param $$y i64) (result i64) (i64.rem_s (local.get $$x) (local.get $$y)))
  (func (export "rem_u") (param $$x i64) (param $$y i64) (result i64) (i64.rem_u (local.get $$x) (local.get $$y)))
  (func (export "and") (param $$x i64) (param $$y i64) (result i64) (i64.and (local.get $$x) (local.get $$y)))
  (func (export "or") (param $$x i64) (param $$y i64) (result i64) (i64.or (local.get $$x) (local.get $$y)))
  (func (export "xor") (param $$x i64) (param $$y i64) (result i64) (i64.xor (local.get $$x) (local.get $$y)))
  (func (export "shl") (param $$x i64) (param $$y i64) (result i64) (i64.shl (local.get $$x) (local.get $$y)))
  (func (export "shr_s") (param $$x i64) (param $$y i64) (result i64) (i64.shr_s (local.get $$x) (local.get $$y)))
  (func (export "shr_u") (param $$x i64) (param $$y i64) (result i64) (i64.shr_u (local.get $$x) (local.get $$y)))
  (func (export "rotl") (param $$x i64) (param $$y i64) (result i64) (i64.rotl (local.get $$x) (local.get $$y)))
  (func (export "rotr") (param $$x i64) (param $$y i64) (result i64) (i64.rotr (local.get $$x) (local.get $$y)))
  (func (export "clz") (param $$x i64) (result i64) (i64.clz (local.get $$x)))
  (func (export "ctz") (param $$x i64) (result i64) (i64.ctz (local.get $$x)))
  (func (export "popcnt") (param $$x i64) (result i64) (i64.popcnt (local.get $$x)))
  (func (export "extend8_s") (param $$x i64) (result i64) (i64.extend8_s (local.get $$x)))
  (func (export "extend16_s") (param $$x i64) (result i64) (i64.extend16_s (local.get $$x)))
  (func (export "extend32_s") (param $$x i64) (result i64) (i64.extend32_s (local.get $$x)))
  (func (export "eqz") (param $$x i64) (result i32) (i64.eqz (local.get $$x)))
  (func (export "eq") (param $$x i64) (param $$y i64) (result i32) (i64.eq (local.get $$x) (local.get $$y)))
  (func (export "ne") (param $$x i64) (param $$y i64) (result i32) (i64.ne (local.get $$x) (local.get $$y)))
  (func (export "lt_s") (param $$x i64) (param $$y i64) (result i32) (i64.lt_s (local.get $$x) (local.get $$y)))
  (func (export "lt_u") (param $$x i64) (param $$y i64) (result i32) (i64.lt_u (local.get $$x) (local.get $$y)))
  (func (export "le_s") (param $$x i64) (param $$y i64) (result i32) (i64.le_s (local.get $$x) (local.get $$y)))
  (func (export "le_u") (param $$x i64) (param $$y i64) (result i32) (i64.le_u (local.get $$x) (local.get $$y)))
  (func (export "gt_s") (param $$x i64) (param $$y i64) (result i32) (i64.gt_s (local.get $$x) (local.get $$y)))
  (func (export "gt_u") (param $$x i64) (param $$y i64) (result i32) (i64.gt_u (local.get $$x) (local.get $$y)))
  (func (export "ge_s") (param $$x i64) (param $$y i64) (result i32) (i64.ge_s (local.get $$x) (local.get $$y)))
  (func (export "ge_u") (param $$x i64) (param $$y i64) (result i32) (i64.ge_u (local.get $$x) (local.get $$y)))
)`);

// ./test/core/i64.wast:38
assert_return(() => invoke($0, `add`, [1n, 1n]), [value("i64", 2n)]);

// ./test/core/i64.wast:39
assert_return(() => invoke($0, `add`, [1n, 0n]), [value("i64", 1n)]);

// ./test/core/i64.wast:40
assert_return(() => invoke($0, `add`, [-1n, -1n]), [value("i64", -2n)]);

// ./test/core/i64.wast:41
assert_return(() => invoke($0, `add`, [-1n, 1n]), [value("i64", 0n)]);

// ./test/core/i64.wast:42
assert_return(() => invoke($0, `add`, [9223372036854775807n, 1n]), [
  value("i64", -9223372036854775808n),
]);

// ./test/core/i64.wast:43
assert_return(() => invoke($0, `add`, [-9223372036854775808n, -1n]), [
  value("i64", 9223372036854775807n),
]);

// ./test/core/i64.wast:44
assert_return(
  () => invoke($0, `add`, [-9223372036854775808n, -9223372036854775808n]),
  [value("i64", 0n)],
);

// ./test/core/i64.wast:45
assert_return(() => invoke($0, `add`, [1073741823n, 1n]), [
  value("i64", 1073741824n),
]);

// ./test/core/i64.wast:47
assert_return(() => invoke($0, `sub`, [1n, 1n]), [value("i64", 0n)]);

// ./test/core/i64.wast:48
assert_return(() => invoke($0, `sub`, [1n, 0n]), [value("i64", 1n)]);

// ./test/core/i64.wast:49
assert_return(() => invoke($0, `sub`, [-1n, -1n]), [value("i64", 0n)]);

// ./test/core/i64.wast:50
assert_return(() => invoke($0, `sub`, [9223372036854775807n, -1n]), [
  value("i64", -9223372036854775808n),
]);

// ./test/core/i64.wast:51
assert_return(() => invoke($0, `sub`, [-9223372036854775808n, 1n]), [
  value("i64", 9223372036854775807n),
]);

// ./test/core/i64.wast:52
assert_return(
  () => invoke($0, `sub`, [-9223372036854775808n, -9223372036854775808n]),
  [value("i64", 0n)],
);

// ./test/core/i64.wast:53
assert_return(() => invoke($0, `sub`, [1073741823n, -1n]), [
  value("i64", 1073741824n),
]);

// ./test/core/i64.wast:55
assert_return(() => invoke($0, `mul`, [1n, 1n]), [value("i64", 1n)]);

// ./test/core/i64.wast:56
assert_return(() => invoke($0, `mul`, [1n, 0n]), [value("i64", 0n)]);

// ./test/core/i64.wast:57
assert_return(() => invoke($0, `mul`, [-1n, -1n]), [value("i64", 1n)]);

// ./test/core/i64.wast:58
assert_return(() => invoke($0, `mul`, [1152921504606846976n, 4096n]), [
  value("i64", 0n),
]);

// ./test/core/i64.wast:59
assert_return(() => invoke($0, `mul`, [-9223372036854775808n, 0n]), [
  value("i64", 0n),
]);

// ./test/core/i64.wast:60
assert_return(() => invoke($0, `mul`, [-9223372036854775808n, -1n]), [
  value("i64", -9223372036854775808n),
]);

// ./test/core/i64.wast:61
assert_return(() => invoke($0, `mul`, [9223372036854775807n, -1n]), [
  value("i64", -9223372036854775807n),
]);

// ./test/core/i64.wast:62
assert_return(
  () => invoke($0, `mul`, [81985529216486895n, -81985529216486896n]),
  [value("i64", 2465395958572223728n)],
);

// ./test/core/i64.wast:63
assert_return(
  () => invoke($0, `mul`, [9223372036854775807n, 9223372036854775807n]),
  [value("i64", 1n)],
);

// ./test/core/i64.wast:65
assert_trap(() => invoke($0, `div_s`, [1n, 0n]), `integer divide by zero`);

// ./test/core/i64.wast:66
assert_trap(() => invoke($0, `div_s`, [0n, 0n]), `integer divide by zero`);

// ./test/core/i64.wast:67
assert_trap(
  () => invoke($0, `div_s`, [-9223372036854775808n, -1n]),
  `integer overflow`,
);

// ./test/core/i64.wast:68
assert_trap(
  () => invoke($0, `div_s`, [-9223372036854775808n, 0n]),
  `integer divide by zero`,
);

// ./test/core/i64.wast:69
assert_return(() => invoke($0, `div_s`, [1n, 1n]), [value("i64", 1n)]);

// ./test/core/i64.wast:70
assert_return(() => invoke($0, `div_s`, [0n, 1n]), [value("i64", 0n)]);

// ./test/core/i64.wast:71
assert_return(() => invoke($0, `div_s`, [0n, -1n]), [value("i64", 0n)]);

// ./test/core/i64.wast:72
assert_return(() => invoke($0, `div_s`, [-1n, -1n]), [value("i64", 1n)]);

// ./test/core/i64.wast:73
assert_return(() => invoke($0, `div_s`, [-9223372036854775808n, 2n]), [
  value("i64", -4611686018427387904n),
]);

// ./test/core/i64.wast:74
assert_return(() => invoke($0, `div_s`, [-9223372036854775807n, 1000n]), [
  value("i64", -9223372036854775n),
]);

// ./test/core/i64.wast:75
assert_return(() => invoke($0, `div_s`, [5n, 2n]), [value("i64", 2n)]);

// ./test/core/i64.wast:76
assert_return(() => invoke($0, `div_s`, [-5n, 2n]), [value("i64", -2n)]);

// ./test/core/i64.wast:77
assert_return(() => invoke($0, `div_s`, [5n, -2n]), [value("i64", -2n)]);

// ./test/core/i64.wast:78
assert_return(() => invoke($0, `div_s`, [-5n, -2n]), [value("i64", 2n)]);

// ./test/core/i64.wast:79
assert_return(() => invoke($0, `div_s`, [7n, 3n]), [value("i64", 2n)]);

// ./test/core/i64.wast:80
assert_return(() => invoke($0, `div_s`, [-7n, 3n]), [value("i64", -2n)]);

// ./test/core/i64.wast:81
assert_return(() => invoke($0, `div_s`, [7n, -3n]), [value("i64", -2n)]);

// ./test/core/i64.wast:82
assert_return(() => invoke($0, `div_s`, [-7n, -3n]), [value("i64", 2n)]);

// ./test/core/i64.wast:83
assert_return(() => invoke($0, `div_s`, [11n, 5n]), [value("i64", 2n)]);

// ./test/core/i64.wast:84
assert_return(() => invoke($0, `div_s`, [17n, 7n]), [value("i64", 2n)]);

// ./test/core/i64.wast:86
assert_trap(() => invoke($0, `div_u`, [1n, 0n]), `integer divide by zero`);

// ./test/core/i64.wast:87
assert_trap(() => invoke($0, `div_u`, [0n, 0n]), `integer divide by zero`);

// ./test/core/i64.wast:88
assert_return(() => invoke($0, `div_u`, [1n, 1n]), [value("i64", 1n)]);

// ./test/core/i64.wast:89
assert_return(() => invoke($0, `div_u`, [0n, 1n]), [value("i64", 0n)]);

// ./test/core/i64.wast:90
assert_return(() => invoke($0, `div_u`, [-1n, -1n]), [value("i64", 1n)]);

// ./test/core/i64.wast:91
assert_return(() => invoke($0, `div_u`, [-9223372036854775808n, -1n]), [
  value("i64", 0n),
]);

// ./test/core/i64.wast:92
assert_return(() => invoke($0, `div_u`, [-9223372036854775808n, 2n]), [
  value("i64", 4611686018427387904n),
]);

// ./test/core/i64.wast:93
assert_return(() => invoke($0, `div_u`, [-8074936608141340688n, 4294967297n]), [
  value("i64", 2414874607n),
]);

// ./test/core/i64.wast:94
assert_return(() => invoke($0, `div_u`, [-9223372036854775807n, 1000n]), [
  value("i64", 9223372036854775n),
]);

// ./test/core/i64.wast:95
assert_return(() => invoke($0, `div_u`, [5n, 2n]), [value("i64", 2n)]);

// ./test/core/i64.wast:96
assert_return(() => invoke($0, `div_u`, [-5n, 2n]), [
  value("i64", 9223372036854775805n),
]);

// ./test/core/i64.wast:97
assert_return(() => invoke($0, `div_u`, [5n, -2n]), [value("i64", 0n)]);

// ./test/core/i64.wast:98
assert_return(() => invoke($0, `div_u`, [-5n, -2n]), [value("i64", 0n)]);

// ./test/core/i64.wast:99
assert_return(() => invoke($0, `div_u`, [7n, 3n]), [value("i64", 2n)]);

// ./test/core/i64.wast:100
assert_return(() => invoke($0, `div_u`, [11n, 5n]), [value("i64", 2n)]);

// ./test/core/i64.wast:101
assert_return(() => invoke($0, `div_u`, [17n, 7n]), [value("i64", 2n)]);

// ./test/core/i64.wast:103
assert_trap(() => invoke($0, `rem_s`, [1n, 0n]), `integer divide by zero`);

// ./test/core/i64.wast:104
assert_trap(() => invoke($0, `rem_s`, [0n, 0n]), `integer divide by zero`);

// ./test/core/i64.wast:105
assert_return(() => invoke($0, `rem_s`, [9223372036854775807n, -1n]), [
  value("i64", 0n),
]);

// ./test/core/i64.wast:106
assert_return(() => invoke($0, `rem_s`, [1n, 1n]), [value("i64", 0n)]);

// ./test/core/i64.wast:107
assert_return(() => invoke($0, `rem_s`, [0n, 1n]), [value("i64", 0n)]);

// ./test/core/i64.wast:108
assert_return(() => invoke($0, `rem_s`, [0n, -1n]), [value("i64", 0n)]);

// ./test/core/i64.wast:109
assert_return(() => invoke($0, `rem_s`, [-1n, -1n]), [value("i64", 0n)]);

// ./test/core/i64.wast:110
assert_return(() => invoke($0, `rem_s`, [-9223372036854775808n, -1n]), [
  value("i64", 0n),
]);

// ./test/core/i64.wast:111
assert_return(() => invoke($0, `rem_s`, [-9223372036854775808n, 2n]), [
  value("i64", 0n),
]);

// ./test/core/i64.wast:112
assert_return(() => invoke($0, `rem_s`, [-9223372036854775807n, 1000n]), [
  value("i64", -807n),
]);

// ./test/core/i64.wast:113
assert_return(() => invoke($0, `rem_s`, [5n, 2n]), [value("i64", 1n)]);

// ./test/core/i64.wast:114
assert_return(() => invoke($0, `rem_s`, [-5n, 2n]), [value("i64", -1n)]);

// ./test/core/i64.wast:115
assert_return(() => invoke($0, `rem_s`, [5n, -2n]), [value("i64", 1n)]);

// ./test/core/i64.wast:116
assert_return(() => invoke($0, `rem_s`, [-5n, -2n]), [value("i64", -1n)]);

// ./test/core/i64.wast:117
assert_return(() => invoke($0, `rem_s`, [7n, 3n]), [value("i64", 1n)]);

// ./test/core/i64.wast:118
assert_return(() => invoke($0, `rem_s`, [-7n, 3n]), [value("i64", -1n)]);

// ./test/core/i64.wast:119
assert_return(() => invoke($0, `rem_s`, [7n, -3n]), [value("i64", 1n)]);

// ./test/core/i64.wast:120
assert_return(() => invoke($0, `rem_s`, [-7n, -3n]), [value("i64", -1n)]);

// ./test/core/i64.wast:121
assert_return(() => invoke($0, `rem_s`, [11n, 5n]), [value("i64", 1n)]);

// ./test/core/i64.wast:122
assert_return(() => invoke($0, `rem_s`, [17n, 7n]), [value("i64", 3n)]);

// ./test/core/i64.wast:124
assert_trap(() => invoke($0, `rem_u`, [1n, 0n]), `integer divide by zero`);

// ./test/core/i64.wast:125
assert_trap(() => invoke($0, `rem_u`, [0n, 0n]), `integer divide by zero`);

// ./test/core/i64.wast:126
assert_return(() => invoke($0, `rem_u`, [1n, 1n]), [value("i64", 0n)]);

// ./test/core/i64.wast:127
assert_return(() => invoke($0, `rem_u`, [0n, 1n]), [value("i64", 0n)]);

// ./test/core/i64.wast:128
assert_return(() => invoke($0, `rem_u`, [-1n, -1n]), [value("i64", 0n)]);

// ./test/core/i64.wast:129
assert_return(() => invoke($0, `rem_u`, [-9223372036854775808n, -1n]), [
  value("i64", -9223372036854775808n),
]);

// ./test/core/i64.wast:130
assert_return(() => invoke($0, `rem_u`, [-9223372036854775808n, 2n]), [
  value("i64", 0n),
]);

// ./test/core/i64.wast:131
assert_return(() => invoke($0, `rem_u`, [-8074936608141340688n, 4294967297n]), [
  value("i64", 2147483649n),
]);

// ./test/core/i64.wast:132
assert_return(() => invoke($0, `rem_u`, [-9223372036854775807n, 1000n]), [
  value("i64", 809n),
]);

// ./test/core/i64.wast:133
assert_return(() => invoke($0, `rem_u`, [5n, 2n]), [value("i64", 1n)]);

// ./test/core/i64.wast:134
assert_return(() => invoke($0, `rem_u`, [-5n, 2n]), [value("i64", 1n)]);

// ./test/core/i64.wast:135
assert_return(() => invoke($0, `rem_u`, [5n, -2n]), [value("i64", 5n)]);

// ./test/core/i64.wast:136
assert_return(() => invoke($0, `rem_u`, [-5n, -2n]), [value("i64", -5n)]);

// ./test/core/i64.wast:137
assert_return(() => invoke($0, `rem_u`, [7n, 3n]), [value("i64", 1n)]);

// ./test/core/i64.wast:138
assert_return(() => invoke($0, `rem_u`, [11n, 5n]), [value("i64", 1n)]);

// ./test/core/i64.wast:139
assert_return(() => invoke($0, `rem_u`, [17n, 7n]), [value("i64", 3n)]);

// ./test/core/i64.wast:141
assert_return(() => invoke($0, `and`, [1n, 0n]), [value("i64", 0n)]);

// ./test/core/i64.wast:142
assert_return(() => invoke($0, `and`, [0n, 1n]), [value("i64", 0n)]);

// ./test/core/i64.wast:143
assert_return(() => invoke($0, `and`, [1n, 1n]), [value("i64", 1n)]);

// ./test/core/i64.wast:144
assert_return(() => invoke($0, `and`, [0n, 0n]), [value("i64", 0n)]);

// ./test/core/i64.wast:145
assert_return(
  () => invoke($0, `and`, [9223372036854775807n, -9223372036854775808n]),
  [value("i64", 0n)],
);

// ./test/core/i64.wast:146
assert_return(() => invoke($0, `and`, [9223372036854775807n, -1n]), [
  value("i64", 9223372036854775807n),
]);

// ./test/core/i64.wast:147
assert_return(() => invoke($0, `and`, [4042326015n, 4294963440n]), [
  value("i64", 4042322160n),
]);

// ./test/core/i64.wast:148
assert_return(() => invoke($0, `and`, [-1n, -1n]), [value("i64", -1n)]);

// ./test/core/i64.wast:150
assert_return(() => invoke($0, `or`, [1n, 0n]), [value("i64", 1n)]);

// ./test/core/i64.wast:151
assert_return(() => invoke($0, `or`, [0n, 1n]), [value("i64", 1n)]);

// ./test/core/i64.wast:152
assert_return(() => invoke($0, `or`, [1n, 1n]), [value("i64", 1n)]);

// ./test/core/i64.wast:153
assert_return(() => invoke($0, `or`, [0n, 0n]), [value("i64", 0n)]);

// ./test/core/i64.wast:154
assert_return(
  () => invoke($0, `or`, [9223372036854775807n, -9223372036854775808n]),
  [value("i64", -1n)],
);

// ./test/core/i64.wast:155
assert_return(() => invoke($0, `or`, [-9223372036854775808n, 0n]), [
  value("i64", -9223372036854775808n),
]);

// ./test/core/i64.wast:156
assert_return(() => invoke($0, `or`, [4042326015n, 4294963440n]), [
  value("i64", 4294967295n),
]);

// ./test/core/i64.wast:157
assert_return(() => invoke($0, `or`, [-1n, -1n]), [value("i64", -1n)]);

// ./test/core/i64.wast:159
assert_return(() => invoke($0, `xor`, [1n, 0n]), [value("i64", 1n)]);

// ./test/core/i64.wast:160
assert_return(() => invoke($0, `xor`, [0n, 1n]), [value("i64", 1n)]);

// ./test/core/i64.wast:161
assert_return(() => invoke($0, `xor`, [1n, 1n]), [value("i64", 0n)]);

// ./test/core/i64.wast:162
assert_return(() => invoke($0, `xor`, [0n, 0n]), [value("i64", 0n)]);

// ./test/core/i64.wast:163
assert_return(
  () => invoke($0, `xor`, [9223372036854775807n, -9223372036854775808n]),
  [value("i64", -1n)],
);

// ./test/core/i64.wast:164
assert_return(() => invoke($0, `xor`, [-9223372036854775808n, 0n]), [
  value("i64", -9223372036854775808n),
]);

// ./test/core/i64.wast:165
assert_return(() => invoke($0, `xor`, [-1n, -9223372036854775808n]), [
  value("i64", 9223372036854775807n),
]);

// ./test/core/i64.wast:166
assert_return(() => invoke($0, `xor`, [-1n, 9223372036854775807n]), [
  value("i64", -9223372036854775808n),
]);

// ./test/core/i64.wast:167
assert_return(() => invoke($0, `xor`, [4042326015n, 4294963440n]), [
  value("i64", 252645135n),
]);

// ./test/core/i64.wast:168
assert_return(() => invoke($0, `xor`, [-1n, -1n]), [value("i64", 0n)]);

// ./test/core/i64.wast:170
assert_return(() => invoke($0, `shl`, [1n, 1n]), [value("i64", 2n)]);

// ./test/core/i64.wast:171
assert_return(() => invoke($0, `shl`, [1n, 0n]), [value("i64", 1n)]);

// ./test/core/i64.wast:172
assert_return(() => invoke($0, `shl`, [9223372036854775807n, 1n]), [
  value("i64", -2n),
]);

// ./test/core/i64.wast:173
assert_return(() => invoke($0, `shl`, [-1n, 1n]), [value("i64", -2n)]);

// ./test/core/i64.wast:174
assert_return(() => invoke($0, `shl`, [-9223372036854775808n, 1n]), [
  value("i64", 0n),
]);

// ./test/core/i64.wast:175
assert_return(() => invoke($0, `shl`, [4611686018427387904n, 1n]), [
  value("i64", -9223372036854775808n),
]);

// ./test/core/i64.wast:176
assert_return(() => invoke($0, `shl`, [1n, 63n]), [
  value("i64", -9223372036854775808n),
]);

// ./test/core/i64.wast:177
assert_return(() => invoke($0, `shl`, [1n, 64n]), [value("i64", 1n)]);

// ./test/core/i64.wast:178
assert_return(() => invoke($0, `shl`, [1n, 65n]), [value("i64", 2n)]);

// ./test/core/i64.wast:179
assert_return(() => invoke($0, `shl`, [1n, -1n]), [
  value("i64", -9223372036854775808n),
]);

// ./test/core/i64.wast:180
assert_return(() => invoke($0, `shl`, [1n, 9223372036854775807n]), [
  value("i64", -9223372036854775808n),
]);

// ./test/core/i64.wast:182
assert_return(() => invoke($0, `shr_s`, [1n, 1n]), [value("i64", 0n)]);

// ./test/core/i64.wast:183
assert_return(() => invoke($0, `shr_s`, [1n, 0n]), [value("i64", 1n)]);

// ./test/core/i64.wast:184
assert_return(() => invoke($0, `shr_s`, [-1n, 1n]), [value("i64", -1n)]);

// ./test/core/i64.wast:185
assert_return(() => invoke($0, `shr_s`, [9223372036854775807n, 1n]), [
  value("i64", 4611686018427387903n),
]);

// ./test/core/i64.wast:186
assert_return(() => invoke($0, `shr_s`, [-9223372036854775808n, 1n]), [
  value("i64", -4611686018427387904n),
]);

// ./test/core/i64.wast:187
assert_return(() => invoke($0, `shr_s`, [4611686018427387904n, 1n]), [
  value("i64", 2305843009213693952n),
]);

// ./test/core/i64.wast:188
assert_return(() => invoke($0, `shr_s`, [1n, 64n]), [value("i64", 1n)]);

// ./test/core/i64.wast:189
assert_return(() => invoke($0, `shr_s`, [1n, 65n]), [value("i64", 0n)]);

// ./test/core/i64.wast:190
assert_return(() => invoke($0, `shr_s`, [1n, -1n]), [value("i64", 0n)]);

// ./test/core/i64.wast:191
assert_return(() => invoke($0, `shr_s`, [1n, 9223372036854775807n]), [
  value("i64", 0n),
]);

// ./test/core/i64.wast:192
assert_return(() => invoke($0, `shr_s`, [1n, -9223372036854775808n]), [
  value("i64", 1n),
]);

// ./test/core/i64.wast:193
assert_return(() => invoke($0, `shr_s`, [-9223372036854775808n, 63n]), [
  value("i64", -1n),
]);

// ./test/core/i64.wast:194
assert_return(() => invoke($0, `shr_s`, [-1n, 64n]), [value("i64", -1n)]);

// ./test/core/i64.wast:195
assert_return(() => invoke($0, `shr_s`, [-1n, 65n]), [value("i64", -1n)]);

// ./test/core/i64.wast:196
assert_return(() => invoke($0, `shr_s`, [-1n, -1n]), [value("i64", -1n)]);

// ./test/core/i64.wast:197
assert_return(() => invoke($0, `shr_s`, [-1n, 9223372036854775807n]), [
  value("i64", -1n),
]);

// ./test/core/i64.wast:198
assert_return(() => invoke($0, `shr_s`, [-1n, -9223372036854775808n]), [
  value("i64", -1n),
]);

// ./test/core/i64.wast:200
assert_return(() => invoke($0, `shr_u`, [1n, 1n]), [value("i64", 0n)]);

// ./test/core/i64.wast:201
assert_return(() => invoke($0, `shr_u`, [1n, 0n]), [value("i64", 1n)]);

// ./test/core/i64.wast:202
assert_return(() => invoke($0, `shr_u`, [-1n, 1n]), [
  value("i64", 9223372036854775807n),
]);

// ./test/core/i64.wast:203
assert_return(() => invoke($0, `shr_u`, [9223372036854775807n, 1n]), [
  value("i64", 4611686018427387903n),
]);

// ./test/core/i64.wast:204
assert_return(() => invoke($0, `shr_u`, [-9223372036854775808n, 1n]), [
  value("i64", 4611686018427387904n),
]);

// ./test/core/i64.wast:205
assert_return(() => invoke($0, `shr_u`, [4611686018427387904n, 1n]), [
  value("i64", 2305843009213693952n),
]);

// ./test/core/i64.wast:206
assert_return(() => invoke($0, `shr_u`, [1n, 64n]), [value("i64", 1n)]);

// ./test/core/i64.wast:207
assert_return(() => invoke($0, `shr_u`, [1n, 65n]), [value("i64", 0n)]);

// ./test/core/i64.wast:208
assert_return(() => invoke($0, `shr_u`, [1n, -1n]), [value("i64", 0n)]);

// ./test/core/i64.wast:209
assert_return(() => invoke($0, `shr_u`, [1n, 9223372036854775807n]), [
  value("i64", 0n),
]);

// ./test/core/i64.wast:210
assert_return(() => invoke($0, `shr_u`, [1n, -9223372036854775808n]), [
  value("i64", 1n),
]);

// ./test/core/i64.wast:211
assert_return(() => invoke($0, `shr_u`, [-9223372036854775808n, 63n]), [
  value("i64", 1n),
]);

// ./test/core/i64.wast:212
assert_return(() => invoke($0, `shr_u`, [-1n, 64n]), [value("i64", -1n)]);

// ./test/core/i64.wast:213
assert_return(() => invoke($0, `shr_u`, [-1n, 65n]), [
  value("i64", 9223372036854775807n),
]);

// ./test/core/i64.wast:214
assert_return(() => invoke($0, `shr_u`, [-1n, -1n]), [value("i64", 1n)]);

// ./test/core/i64.wast:215
assert_return(() => invoke($0, `shr_u`, [-1n, 9223372036854775807n]), [
  value("i64", 1n),
]);

// ./test/core/i64.wast:216
assert_return(() => invoke($0, `shr_u`, [-1n, -9223372036854775808n]), [
  value("i64", -1n),
]);

// ./test/core/i64.wast:218
assert_return(() => invoke($0, `rotl`, [1n, 1n]), [value("i64", 2n)]);

// ./test/core/i64.wast:219
assert_return(() => invoke($0, `rotl`, [1n, 0n]), [value("i64", 1n)]);

// ./test/core/i64.wast:220
assert_return(() => invoke($0, `rotl`, [-1n, 1n]), [value("i64", -1n)]);

// ./test/core/i64.wast:221
assert_return(() => invoke($0, `rotl`, [1n, 64n]), [value("i64", 1n)]);

// ./test/core/i64.wast:222
assert_return(() => invoke($0, `rotl`, [-6067025490386449714n, 1n]), [
  value("i64", 6312693092936652189n),
]);

// ./test/core/i64.wast:223
assert_return(() => invoke($0, `rotl`, [-144115184384868352n, 4n]), [
  value("i64", -2305842950157893617n),
]);

// ./test/core/i64.wast:224
assert_return(() => invoke($0, `rotl`, [-6067173104435169271n, 53n]), [
  value("i64", 87109505680009935n),
]);

// ./test/core/i64.wast:225
assert_return(() => invoke($0, `rotl`, [-6066028401059725156n, 63n]), [
  value("i64", 6190357836324913230n),
]);

// ./test/core/i64.wast:226
assert_return(() => invoke($0, `rotl`, [-6067173104435169271n, 245n]), [
  value("i64", 87109505680009935n),
]);

// ./test/core/i64.wast:227
assert_return(() => invoke($0, `rotl`, [-6067067139002042359n, -19n]), [
  value("i64", -3530481836149793302n),
]);

// ./test/core/i64.wast:228
assert_return(
  () => invoke($0, `rotl`, [-6066028401059725156n, -9223372036854775745n]),
  [value("i64", 6190357836324913230n)],
);

// ./test/core/i64.wast:229
assert_return(() => invoke($0, `rotl`, [1n, 63n]), [
  value("i64", -9223372036854775808n),
]);

// ./test/core/i64.wast:230
assert_return(() => invoke($0, `rotl`, [-9223372036854775808n, 1n]), [
  value("i64", 1n),
]);

// ./test/core/i64.wast:232
assert_return(() => invoke($0, `rotr`, [1n, 1n]), [
  value("i64", -9223372036854775808n),
]);

// ./test/core/i64.wast:233
assert_return(() => invoke($0, `rotr`, [1n, 0n]), [value("i64", 1n)]);

// ./test/core/i64.wast:234
assert_return(() => invoke($0, `rotr`, [-1n, 1n]), [value("i64", -1n)]);

// ./test/core/i64.wast:235
assert_return(() => invoke($0, `rotr`, [1n, 64n]), [value("i64", 1n)]);

// ./test/core/i64.wast:236
assert_return(() => invoke($0, `rotr`, [-6067025490386449714n, 1n]), [
  value("i64", 6189859291661550951n),
]);

// ./test/core/i64.wast:237
assert_return(() => invoke($0, `rotr`, [-144115184384868352n, 4n]), [
  value("i64", 1143914305582792704n),
]);

// ./test/core/i64.wast:238
assert_return(() => invoke($0, `rotr`, [-6067173104435169271n, 53n]), [
  value("i64", 7534987797011123550n),
]);

// ./test/core/i64.wast:239
assert_return(() => invoke($0, `rotr`, [-6066028401059725156n, 63n]), [
  value("i64", 6314687271590101305n),
]);

// ./test/core/i64.wast:240
assert_return(() => invoke($0, `rotr`, [-6067173104435169271n, 245n]), [
  value("i64", 7534987797011123550n),
]);

// ./test/core/i64.wast:241
assert_return(() => invoke($0, `rotr`, [-6067067139002042359n, -19n]), [
  value("i64", -7735078922541506965n),
]);

// ./test/core/i64.wast:242
assert_return(
  () => invoke($0, `rotr`, [-6066028401059725156n, -9223372036854775745n]),
  [value("i64", 6314687271590101305n)],
);

// ./test/core/i64.wast:243
assert_return(() => invoke($0, `rotr`, [1n, 63n]), [value("i64", 2n)]);

// ./test/core/i64.wast:244
assert_return(() => invoke($0, `rotr`, [-9223372036854775808n, 63n]), [
  value("i64", 1n),
]);

// ./test/core/i64.wast:246
assert_return(() => invoke($0, `clz`, [-1n]), [value("i64", 0n)]);

// ./test/core/i64.wast:247
assert_return(() => invoke($0, `clz`, [0n]), [value("i64", 64n)]);

// ./test/core/i64.wast:248
assert_return(() => invoke($0, `clz`, [32768n]), [value("i64", 48n)]);

// ./test/core/i64.wast:249
assert_return(() => invoke($0, `clz`, [255n]), [value("i64", 56n)]);

// ./test/core/i64.wast:250
assert_return(() => invoke($0, `clz`, [-9223372036854775808n]), [
  value("i64", 0n),
]);

// ./test/core/i64.wast:251
assert_return(() => invoke($0, `clz`, [1n]), [value("i64", 63n)]);

// ./test/core/i64.wast:252
assert_return(() => invoke($0, `clz`, [2n]), [value("i64", 62n)]);

// ./test/core/i64.wast:253
assert_return(() => invoke($0, `clz`, [9223372036854775807n]), [
  value("i64", 1n),
]);

// ./test/core/i64.wast:255
assert_return(() => invoke($0, `ctz`, [-1n]), [value("i64", 0n)]);

// ./test/core/i64.wast:256
assert_return(() => invoke($0, `ctz`, [0n]), [value("i64", 64n)]);

// ./test/core/i64.wast:257
assert_return(() => invoke($0, `ctz`, [32768n]), [value("i64", 15n)]);

// ./test/core/i64.wast:258
assert_return(() => invoke($0, `ctz`, [65536n]), [value("i64", 16n)]);

// ./test/core/i64.wast:259
assert_return(() => invoke($0, `ctz`, [-9223372036854775808n]), [
  value("i64", 63n),
]);

// ./test/core/i64.wast:260
assert_return(() => invoke($0, `ctz`, [9223372036854775807n]), [
  value("i64", 0n),
]);

// ./test/core/i64.wast:262
assert_return(() => invoke($0, `popcnt`, [-1n]), [value("i64", 64n)]);

// ./test/core/i64.wast:263
assert_return(() => invoke($0, `popcnt`, [0n]), [value("i64", 0n)]);

// ./test/core/i64.wast:264
assert_return(() => invoke($0, `popcnt`, [32768n]), [value("i64", 1n)]);

// ./test/core/i64.wast:265
assert_return(() => invoke($0, `popcnt`, [-9223231297218904064n]), [
  value("i64", 4n),
]);

// ./test/core/i64.wast:266
assert_return(() => invoke($0, `popcnt`, [9223372036854775807n]), [
  value("i64", 63n),
]);

// ./test/core/i64.wast:267
assert_return(() => invoke($0, `popcnt`, [-6148914692668172971n]), [
  value("i64", 32n),
]);

// ./test/core/i64.wast:268
assert_return(() => invoke($0, `popcnt`, [-7378697629197489494n]), [
  value("i64", 32n),
]);

// ./test/core/i64.wast:269
assert_return(() => invoke($0, `popcnt`, [-2401053088876216593n]), [
  value("i64", 48n),
]);

// ./test/core/i64.wast:271
assert_return(() => invoke($0, `extend8_s`, [0n]), [value("i64", 0n)]);

// ./test/core/i64.wast:272
assert_return(() => invoke($0, `extend8_s`, [127n]), [value("i64", 127n)]);

// ./test/core/i64.wast:273
assert_return(() => invoke($0, `extend8_s`, [128n]), [value("i64", -128n)]);

// ./test/core/i64.wast:274
assert_return(() => invoke($0, `extend8_s`, [255n]), [value("i64", -1n)]);

// ./test/core/i64.wast:275
assert_return(() => invoke($0, `extend8_s`, [81985529216486656n]), [
  value("i64", 0n),
]);

// ./test/core/i64.wast:276
assert_return(() => invoke($0, `extend8_s`, [-81985529216486784n]), [
  value("i64", -128n),
]);

// ./test/core/i64.wast:277
assert_return(() => invoke($0, `extend8_s`, [-1n]), [value("i64", -1n)]);

// ./test/core/i64.wast:279
assert_return(() => invoke($0, `extend16_s`, [0n]), [value("i64", 0n)]);

// ./test/core/i64.wast:280
assert_return(() => invoke($0, `extend16_s`, [32767n]), [value("i64", 32767n)]);

// ./test/core/i64.wast:281
assert_return(() => invoke($0, `extend16_s`, [32768n]), [
  value("i64", -32768n),
]);

// ./test/core/i64.wast:282
assert_return(() => invoke($0, `extend16_s`, [65535n]), [value("i64", -1n)]);

// ./test/core/i64.wast:283
assert_return(() => invoke($0, `extend16_s`, [1311768467463733248n]), [
  value("i64", 0n),
]);

// ./test/core/i64.wast:284
assert_return(() => invoke($0, `extend16_s`, [-81985529216466944n]), [
  value("i64", -32768n),
]);

// ./test/core/i64.wast:285
assert_return(() => invoke($0, `extend16_s`, [-1n]), [value("i64", -1n)]);

// ./test/core/i64.wast:287
assert_return(() => invoke($0, `extend32_s`, [0n]), [value("i64", 0n)]);

// ./test/core/i64.wast:288
assert_return(() => invoke($0, `extend32_s`, [32767n]), [value("i64", 32767n)]);

// ./test/core/i64.wast:289
assert_return(() => invoke($0, `extend32_s`, [32768n]), [value("i64", 32768n)]);

// ./test/core/i64.wast:290
assert_return(() => invoke($0, `extend32_s`, [65535n]), [value("i64", 65535n)]);

// ./test/core/i64.wast:291
assert_return(() => invoke($0, `extend32_s`, [2147483647n]), [
  value("i64", 2147483647n),
]);

// ./test/core/i64.wast:292
assert_return(() => invoke($0, `extend32_s`, [2147483648n]), [
  value("i64", -2147483648n),
]);

// ./test/core/i64.wast:293
assert_return(() => invoke($0, `extend32_s`, [4294967295n]), [
  value("i64", -1n),
]);

// ./test/core/i64.wast:294
assert_return(() => invoke($0, `extend32_s`, [81985526906748928n]), [
  value("i64", 0n),
]);

// ./test/core/i64.wast:295
assert_return(() => invoke($0, `extend32_s`, [-81985529054232576n]), [
  value("i64", -2147483648n),
]);

// ./test/core/i64.wast:296
assert_return(() => invoke($0, `extend32_s`, [-1n]), [value("i64", -1n)]);

// ./test/core/i64.wast:298
assert_return(() => invoke($0, `eqz`, [0n]), [value("i32", 1)]);

// ./test/core/i64.wast:299
assert_return(() => invoke($0, `eqz`, [1n]), [value("i32", 0)]);

// ./test/core/i64.wast:300
assert_return(() => invoke($0, `eqz`, [-9223372036854775808n]), [
  value("i32", 0),
]);

// ./test/core/i64.wast:301
assert_return(() => invoke($0, `eqz`, [9223372036854775807n]), [
  value("i32", 0),
]);

// ./test/core/i64.wast:302
assert_return(() => invoke($0, `eqz`, [-1n]), [value("i32", 0)]);

// ./test/core/i64.wast:304
assert_return(() => invoke($0, `eq`, [0n, 0n]), [value("i32", 1)]);

// ./test/core/i64.wast:305
assert_return(() => invoke($0, `eq`, [1n, 1n]), [value("i32", 1)]);

// ./test/core/i64.wast:306
assert_return(() => invoke($0, `eq`, [-1n, 1n]), [value("i32", 0)]);

// ./test/core/i64.wast:307
assert_return(
  () => invoke($0, `eq`, [-9223372036854775808n, -9223372036854775808n]),
  [value("i32", 1)],
);

// ./test/core/i64.wast:308
assert_return(
  () => invoke($0, `eq`, [9223372036854775807n, 9223372036854775807n]),
  [value("i32", 1)],
);

// ./test/core/i64.wast:309
assert_return(() => invoke($0, `eq`, [-1n, -1n]), [value("i32", 1)]);

// ./test/core/i64.wast:310
assert_return(() => invoke($0, `eq`, [1n, 0n]), [value("i32", 0)]);

// ./test/core/i64.wast:311
assert_return(() => invoke($0, `eq`, [0n, 1n]), [value("i32", 0)]);

// ./test/core/i64.wast:312
assert_return(() => invoke($0, `eq`, [-9223372036854775808n, 0n]), [
  value("i32", 0),
]);

// ./test/core/i64.wast:313
assert_return(() => invoke($0, `eq`, [0n, -9223372036854775808n]), [
  value("i32", 0),
]);

// ./test/core/i64.wast:314
assert_return(() => invoke($0, `eq`, [-9223372036854775808n, -1n]), [
  value("i32", 0),
]);

// ./test/core/i64.wast:315
assert_return(() => invoke($0, `eq`, [-1n, -9223372036854775808n]), [
  value("i32", 0),
]);

// ./test/core/i64.wast:316
assert_return(
  () => invoke($0, `eq`, [-9223372036854775808n, 9223372036854775807n]),
  [value("i32", 0)],
);

// ./test/core/i64.wast:317
assert_return(
  () => invoke($0, `eq`, [9223372036854775807n, -9223372036854775808n]),
  [value("i32", 0)],
);

// ./test/core/i64.wast:319
assert_return(() => invoke($0, `ne`, [0n, 0n]), [value("i32", 0)]);

// ./test/core/i64.wast:320
assert_return(() => invoke($0, `ne`, [1n, 1n]), [value("i32", 0)]);

// ./test/core/i64.wast:321
assert_return(() => invoke($0, `ne`, [-1n, 1n]), [value("i32", 1)]);

// ./test/core/i64.wast:322
assert_return(
  () => invoke($0, `ne`, [-9223372036854775808n, -9223372036854775808n]),
  [value("i32", 0)],
);

// ./test/core/i64.wast:323
assert_return(
  () => invoke($0, `ne`, [9223372036854775807n, 9223372036854775807n]),
  [value("i32", 0)],
);

// ./test/core/i64.wast:324
assert_return(() => invoke($0, `ne`, [-1n, -1n]), [value("i32", 0)]);

// ./test/core/i64.wast:325
assert_return(() => invoke($0, `ne`, [1n, 0n]), [value("i32", 1)]);

// ./test/core/i64.wast:326
assert_return(() => invoke($0, `ne`, [0n, 1n]), [value("i32", 1)]);

// ./test/core/i64.wast:327
assert_return(() => invoke($0, `ne`, [-9223372036854775808n, 0n]), [
  value("i32", 1),
]);

// ./test/core/i64.wast:328
assert_return(() => invoke($0, `ne`, [0n, -9223372036854775808n]), [
  value("i32", 1),
]);

// ./test/core/i64.wast:329
assert_return(() => invoke($0, `ne`, [-9223372036854775808n, -1n]), [
  value("i32", 1),
]);

// ./test/core/i64.wast:330
assert_return(() => invoke($0, `ne`, [-1n, -9223372036854775808n]), [
  value("i32", 1),
]);

// ./test/core/i64.wast:331
assert_return(
  () => invoke($0, `ne`, [-9223372036854775808n, 9223372036854775807n]),
  [value("i32", 1)],
);

// ./test/core/i64.wast:332
assert_return(
  () => invoke($0, `ne`, [9223372036854775807n, -9223372036854775808n]),
  [value("i32", 1)],
);

// ./test/core/i64.wast:334
assert_return(() => invoke($0, `lt_s`, [0n, 0n]), [value("i32", 0)]);

// ./test/core/i64.wast:335
assert_return(() => invoke($0, `lt_s`, [1n, 1n]), [value("i32", 0)]);

// ./test/core/i64.wast:336
assert_return(() => invoke($0, `lt_s`, [-1n, 1n]), [value("i32", 1)]);

// ./test/core/i64.wast:337
assert_return(
  () => invoke($0, `lt_s`, [-9223372036854775808n, -9223372036854775808n]),
  [value("i32", 0)],
);

// ./test/core/i64.wast:338
assert_return(
  () => invoke($0, `lt_s`, [9223372036854775807n, 9223372036854775807n]),
  [value("i32", 0)],
);

// ./test/core/i64.wast:339
assert_return(() => invoke($0, `lt_s`, [-1n, -1n]), [value("i32", 0)]);

// ./test/core/i64.wast:340
assert_return(() => invoke($0, `lt_s`, [1n, 0n]), [value("i32", 0)]);

// ./test/core/i64.wast:341
assert_return(() => invoke($0, `lt_s`, [0n, 1n]), [value("i32", 1)]);

// ./test/core/i64.wast:342
assert_return(() => invoke($0, `lt_s`, [-9223372036854775808n, 0n]), [
  value("i32", 1),
]);

// ./test/core/i64.wast:343
assert_return(() => invoke($0, `lt_s`, [0n, -9223372036854775808n]), [
  value("i32", 0),
]);

// ./test/core/i64.wast:344
assert_return(() => invoke($0, `lt_s`, [-9223372036854775808n, -1n]), [
  value("i32", 1),
]);

// ./test/core/i64.wast:345
assert_return(() => invoke($0, `lt_s`, [-1n, -9223372036854775808n]), [
  value("i32", 0),
]);

// ./test/core/i64.wast:346
assert_return(
  () => invoke($0, `lt_s`, [-9223372036854775808n, 9223372036854775807n]),
  [value("i32", 1)],
);

// ./test/core/i64.wast:347
assert_return(
  () => invoke($0, `lt_s`, [9223372036854775807n, -9223372036854775808n]),
  [value("i32", 0)],
);

// ./test/core/i64.wast:349
assert_return(() => invoke($0, `lt_u`, [0n, 0n]), [value("i32", 0)]);

// ./test/core/i64.wast:350
assert_return(() => invoke($0, `lt_u`, [1n, 1n]), [value("i32", 0)]);

// ./test/core/i64.wast:351
assert_return(() => invoke($0, `lt_u`, [-1n, 1n]), [value("i32", 0)]);

// ./test/core/i64.wast:352
assert_return(
  () => invoke($0, `lt_u`, [-9223372036854775808n, -9223372036854775808n]),
  [value("i32", 0)],
);

// ./test/core/i64.wast:353
assert_return(
  () => invoke($0, `lt_u`, [9223372036854775807n, 9223372036854775807n]),
  [value("i32", 0)],
);

// ./test/core/i64.wast:354
assert_return(() => invoke($0, `lt_u`, [-1n, -1n]), [value("i32", 0)]);

// ./test/core/i64.wast:355
assert_return(() => invoke($0, `lt_u`, [1n, 0n]), [value("i32", 0)]);

// ./test/core/i64.wast:356
assert_return(() => invoke($0, `lt_u`, [0n, 1n]), [value("i32", 1)]);

// ./test/core/i64.wast:357
assert_return(() => invoke($0, `lt_u`, [-9223372036854775808n, 0n]), [
  value("i32", 0),
]);

// ./test/core/i64.wast:358
assert_return(() => invoke($0, `lt_u`, [0n, -9223372036854775808n]), [
  value("i32", 1),
]);

// ./test/core/i64.wast:359
assert_return(() => invoke($0, `lt_u`, [-9223372036854775808n, -1n]), [
  value("i32", 1),
]);

// ./test/core/i64.wast:360
assert_return(() => invoke($0, `lt_u`, [-1n, -9223372036854775808n]), [
  value("i32", 0),
]);

// ./test/core/i64.wast:361
assert_return(
  () => invoke($0, `lt_u`, [-9223372036854775808n, 9223372036854775807n]),
  [value("i32", 0)],
);

// ./test/core/i64.wast:362
assert_return(
  () => invoke($0, `lt_u`, [9223372036854775807n, -9223372036854775808n]),
  [value("i32", 1)],
);

// ./test/core/i64.wast:364
assert_return(() => invoke($0, `le_s`, [0n, 0n]), [value("i32", 1)]);

// ./test/core/i64.wast:365
assert_return(() => invoke($0, `le_s`, [1n, 1n]), [value("i32", 1)]);

// ./test/core/i64.wast:366
assert_return(() => invoke($0, `le_s`, [-1n, 1n]), [value("i32", 1)]);

// ./test/core/i64.wast:367
assert_return(
  () => invoke($0, `le_s`, [-9223372036854775808n, -9223372036854775808n]),
  [value("i32", 1)],
);

// ./test/core/i64.wast:368
assert_return(
  () => invoke($0, `le_s`, [9223372036854775807n, 9223372036854775807n]),
  [value("i32", 1)],
);

// ./test/core/i64.wast:369
assert_return(() => invoke($0, `le_s`, [-1n, -1n]), [value("i32", 1)]);

// ./test/core/i64.wast:370
assert_return(() => invoke($0, `le_s`, [1n, 0n]), [value("i32", 0)]);

// ./test/core/i64.wast:371
assert_return(() => invoke($0, `le_s`, [0n, 1n]), [value("i32", 1)]);

// ./test/core/i64.wast:372
assert_return(() => invoke($0, `le_s`, [-9223372036854775808n, 0n]), [
  value("i32", 1),
]);

// ./test/core/i64.wast:373
assert_return(() => invoke($0, `le_s`, [0n, -9223372036854775808n]), [
  value("i32", 0),
]);

// ./test/core/i64.wast:374
assert_return(() => invoke($0, `le_s`, [-9223372036854775808n, -1n]), [
  value("i32", 1),
]);

// ./test/core/i64.wast:375
assert_return(() => invoke($0, `le_s`, [-1n, -9223372036854775808n]), [
  value("i32", 0),
]);

// ./test/core/i64.wast:376
assert_return(
  () => invoke($0, `le_s`, [-9223372036854775808n, 9223372036854775807n]),
  [value("i32", 1)],
);

// ./test/core/i64.wast:377
assert_return(
  () => invoke($0, `le_s`, [9223372036854775807n, -9223372036854775808n]),
  [value("i32", 0)],
);

// ./test/core/i64.wast:379
assert_return(() => invoke($0, `le_u`, [0n, 0n]), [value("i32", 1)]);

// ./test/core/i64.wast:380
assert_return(() => invoke($0, `le_u`, [1n, 1n]), [value("i32", 1)]);

// ./test/core/i64.wast:381
assert_return(() => invoke($0, `le_u`, [-1n, 1n]), [value("i32", 0)]);

// ./test/core/i64.wast:382
assert_return(
  () => invoke($0, `le_u`, [-9223372036854775808n, -9223372036854775808n]),
  [value("i32", 1)],
);

// ./test/core/i64.wast:383
assert_return(
  () => invoke($0, `le_u`, [9223372036854775807n, 9223372036854775807n]),
  [value("i32", 1)],
);

// ./test/core/i64.wast:384
assert_return(() => invoke($0, `le_u`, [-1n, -1n]), [value("i32", 1)]);

// ./test/core/i64.wast:385
assert_return(() => invoke($0, `le_u`, [1n, 0n]), [value("i32", 0)]);

// ./test/core/i64.wast:386
assert_return(() => invoke($0, `le_u`, [0n, 1n]), [value("i32", 1)]);

// ./test/core/i64.wast:387
assert_return(() => invoke($0, `le_u`, [-9223372036854775808n, 0n]), [
  value("i32", 0),
]);

// ./test/core/i64.wast:388
assert_return(() => invoke($0, `le_u`, [0n, -9223372036854775808n]), [
  value("i32", 1),
]);

// ./test/core/i64.wast:389
assert_return(() => invoke($0, `le_u`, [-9223372036854775808n, -1n]), [
  value("i32", 1),
]);

// ./test/core/i64.wast:390
assert_return(() => invoke($0, `le_u`, [-1n, -9223372036854775808n]), [
  value("i32", 0),
]);

// ./test/core/i64.wast:391
assert_return(
  () => invoke($0, `le_u`, [-9223372036854775808n, 9223372036854775807n]),
  [value("i32", 0)],
);

// ./test/core/i64.wast:392
assert_return(
  () => invoke($0, `le_u`, [9223372036854775807n, -9223372036854775808n]),
  [value("i32", 1)],
);

// ./test/core/i64.wast:394
assert_return(() => invoke($0, `gt_s`, [0n, 0n]), [value("i32", 0)]);

// ./test/core/i64.wast:395
assert_return(() => invoke($0, `gt_s`, [1n, 1n]), [value("i32", 0)]);

// ./test/core/i64.wast:396
assert_return(() => invoke($0, `gt_s`, [-1n, 1n]), [value("i32", 0)]);

// ./test/core/i64.wast:397
assert_return(
  () => invoke($0, `gt_s`, [-9223372036854775808n, -9223372036854775808n]),
  [value("i32", 0)],
);

// ./test/core/i64.wast:398
assert_return(
  () => invoke($0, `gt_s`, [9223372036854775807n, 9223372036854775807n]),
  [value("i32", 0)],
);

// ./test/core/i64.wast:399
assert_return(() => invoke($0, `gt_s`, [-1n, -1n]), [value("i32", 0)]);

// ./test/core/i64.wast:400
assert_return(() => invoke($0, `gt_s`, [1n, 0n]), [value("i32", 1)]);

// ./test/core/i64.wast:401
assert_return(() => invoke($0, `gt_s`, [0n, 1n]), [value("i32", 0)]);

// ./test/core/i64.wast:402
assert_return(() => invoke($0, `gt_s`, [-9223372036854775808n, 0n]), [
  value("i32", 0),
]);

// ./test/core/i64.wast:403
assert_return(() => invoke($0, `gt_s`, [0n, -9223372036854775808n]), [
  value("i32", 1),
]);

// ./test/core/i64.wast:404
assert_return(() => invoke($0, `gt_s`, [-9223372036854775808n, -1n]), [
  value("i32", 0),
]);

// ./test/core/i64.wast:405
assert_return(() => invoke($0, `gt_s`, [-1n, -9223372036854775808n]), [
  value("i32", 1),
]);

// ./test/core/i64.wast:406
assert_return(
  () => invoke($0, `gt_s`, [-9223372036854775808n, 9223372036854775807n]),
  [value("i32", 0)],
);

// ./test/core/i64.wast:407
assert_return(
  () => invoke($0, `gt_s`, [9223372036854775807n, -9223372036854775808n]),
  [value("i32", 1)],
);

// ./test/core/i64.wast:409
assert_return(() => invoke($0, `gt_u`, [0n, 0n]), [value("i32", 0)]);

// ./test/core/i64.wast:410
assert_return(() => invoke($0, `gt_u`, [1n, 1n]), [value("i32", 0)]);

// ./test/core/i64.wast:411
assert_return(() => invoke($0, `gt_u`, [-1n, 1n]), [value("i32", 1)]);

// ./test/core/i64.wast:412
assert_return(
  () => invoke($0, `gt_u`, [-9223372036854775808n, -9223372036854775808n]),
  [value("i32", 0)],
);

// ./test/core/i64.wast:413
assert_return(
  () => invoke($0, `gt_u`, [9223372036854775807n, 9223372036854775807n]),
  [value("i32", 0)],
);

// ./test/core/i64.wast:414
assert_return(() => invoke($0, `gt_u`, [-1n, -1n]), [value("i32", 0)]);

// ./test/core/i64.wast:415
assert_return(() => invoke($0, `gt_u`, [1n, 0n]), [value("i32", 1)]);

// ./test/core/i64.wast:416
assert_return(() => invoke($0, `gt_u`, [0n, 1n]), [value("i32", 0)]);

// ./test/core/i64.wast:417
assert_return(() => invoke($0, `gt_u`, [-9223372036854775808n, 0n]), [
  value("i32", 1),
]);

// ./test/core/i64.wast:418
assert_return(() => invoke($0, `gt_u`, [0n, -9223372036854775808n]), [
  value("i32", 0),
]);

// ./test/core/i64.wast:419
assert_return(() => invoke($0, `gt_u`, [-9223372036854775808n, -1n]), [
  value("i32", 0),
]);

// ./test/core/i64.wast:420
assert_return(() => invoke($0, `gt_u`, [-1n, -9223372036854775808n]), [
  value("i32", 1),
]);

// ./test/core/i64.wast:421
assert_return(
  () => invoke($0, `gt_u`, [-9223372036854775808n, 9223372036854775807n]),
  [value("i32", 1)],
);

// ./test/core/i64.wast:422
assert_return(
  () => invoke($0, `gt_u`, [9223372036854775807n, -9223372036854775808n]),
  [value("i32", 0)],
);

// ./test/core/i64.wast:424
assert_return(() => invoke($0, `ge_s`, [0n, 0n]), [value("i32", 1)]);

// ./test/core/i64.wast:425
assert_return(() => invoke($0, `ge_s`, [1n, 1n]), [value("i32", 1)]);

// ./test/core/i64.wast:426
assert_return(() => invoke($0, `ge_s`, [-1n, 1n]), [value("i32", 0)]);

// ./test/core/i64.wast:427
assert_return(
  () => invoke($0, `ge_s`, [-9223372036854775808n, -9223372036854775808n]),
  [value("i32", 1)],
);

// ./test/core/i64.wast:428
assert_return(
  () => invoke($0, `ge_s`, [9223372036854775807n, 9223372036854775807n]),
  [value("i32", 1)],
);

// ./test/core/i64.wast:429
assert_return(() => invoke($0, `ge_s`, [-1n, -1n]), [value("i32", 1)]);

// ./test/core/i64.wast:430
assert_return(() => invoke($0, `ge_s`, [1n, 0n]), [value("i32", 1)]);

// ./test/core/i64.wast:431
assert_return(() => invoke($0, `ge_s`, [0n, 1n]), [value("i32", 0)]);

// ./test/core/i64.wast:432
assert_return(() => invoke($0, `ge_s`, [-9223372036854775808n, 0n]), [
  value("i32", 0),
]);

// ./test/core/i64.wast:433
assert_return(() => invoke($0, `ge_s`, [0n, -9223372036854775808n]), [
  value("i32", 1),
]);

// ./test/core/i64.wast:434
assert_return(() => invoke($0, `ge_s`, [-9223372036854775808n, -1n]), [
  value("i32", 0),
]);

// ./test/core/i64.wast:435
assert_return(() => invoke($0, `ge_s`, [-1n, -9223372036854775808n]), [
  value("i32", 1),
]);

// ./test/core/i64.wast:436
assert_return(
  () => invoke($0, `ge_s`, [-9223372036854775808n, 9223372036854775807n]),
  [value("i32", 0)],
);

// ./test/core/i64.wast:437
assert_return(
  () => invoke($0, `ge_s`, [9223372036854775807n, -9223372036854775808n]),
  [value("i32", 1)],
);

// ./test/core/i64.wast:439
assert_return(() => invoke($0, `ge_u`, [0n, 0n]), [value("i32", 1)]);

// ./test/core/i64.wast:440
assert_return(() => invoke($0, `ge_u`, [1n, 1n]), [value("i32", 1)]);

// ./test/core/i64.wast:441
assert_return(() => invoke($0, `ge_u`, [-1n, 1n]), [value("i32", 1)]);

// ./test/core/i64.wast:442
assert_return(
  () => invoke($0, `ge_u`, [-9223372036854775808n, -9223372036854775808n]),
  [value("i32", 1)],
);

// ./test/core/i64.wast:443
assert_return(
  () => invoke($0, `ge_u`, [9223372036854775807n, 9223372036854775807n]),
  [value("i32", 1)],
);

// ./test/core/i64.wast:444
assert_return(() => invoke($0, `ge_u`, [-1n, -1n]), [value("i32", 1)]);

// ./test/core/i64.wast:445
assert_return(() => invoke($0, `ge_u`, [1n, 0n]), [value("i32", 1)]);

// ./test/core/i64.wast:446
assert_return(() => invoke($0, `ge_u`, [0n, 1n]), [value("i32", 0)]);

// ./test/core/i64.wast:447
assert_return(() => invoke($0, `ge_u`, [-9223372036854775808n, 0n]), [
  value("i32", 1),
]);

// ./test/core/i64.wast:448
assert_return(() => invoke($0, `ge_u`, [0n, -9223372036854775808n]), [
  value("i32", 0),
]);

// ./test/core/i64.wast:449
assert_return(() => invoke($0, `ge_u`, [-9223372036854775808n, -1n]), [
  value("i32", 0),
]);

// ./test/core/i64.wast:450
assert_return(() => invoke($0, `ge_u`, [-1n, -9223372036854775808n]), [
  value("i32", 1),
]);

// ./test/core/i64.wast:451
assert_return(
  () => invoke($0, `ge_u`, [-9223372036854775808n, 9223372036854775807n]),
  [value("i32", 1)],
);

// ./test/core/i64.wast:452
assert_return(
  () => invoke($0, `ge_u`, [9223372036854775807n, -9223372036854775808n]),
  [value("i32", 0)],
);

// ./test/core/i64.wast:457
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i64) (i64.add (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i64.wast:458
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i64) (i64.and (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i64.wast:459
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i64) (i64.div_s (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i64.wast:460
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i64) (i64.div_u (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i64.wast:461
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i64) (i64.mul (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i64.wast:462
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i64) (i64.or (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i64.wast:463
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i64) (i64.rem_s (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i64.wast:464
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i64) (i64.rem_u (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i64.wast:465
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i64) (i64.rotl (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i64.wast:466
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i64) (i64.rotr (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i64.wast:467
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i64) (i64.shl (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i64.wast:468
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i64) (i64.shr_s (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i64.wast:469
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i64) (i64.shr_u (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i64.wast:470
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i64) (i64.sub (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i64.wast:471
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i64) (i64.xor (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i64.wast:472
assert_invalid(
  () => instantiate(`(module (func (result i64) (i64.eqz (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/i64.wast:473
assert_invalid(
  () => instantiate(`(module (func (result i64) (i64.clz (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/i64.wast:474
assert_invalid(
  () => instantiate(`(module (func (result i64) (i64.ctz (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/i64.wast:475
assert_invalid(
  () => instantiate(`(module (func (result i64) (i64.popcnt (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/i64.wast:476
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i64) (i64.eq (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i64.wast:477
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i64) (i64.ge_s (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i64.wast:478
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i64) (i64.ge_u (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i64.wast:479
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i64) (i64.gt_s (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i64.wast:480
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i64) (i64.gt_u (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i64.wast:481
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i64) (i64.le_s (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i64.wast:482
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i64) (i64.le_u (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i64.wast:483
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i64) (i64.lt_s (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i64.wast:484
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i64) (i64.lt_u (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i64.wast:485
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i64) (i64.ne (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);
