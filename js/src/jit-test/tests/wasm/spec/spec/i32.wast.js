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

// ./test/core/i32.wast

// ./test/core/i32.wast:3
let $0 = instantiate(`(module
  (func (export "add") (param $$x i32) (param $$y i32) (result i32) (i32.add (local.get $$x) (local.get $$y)))
  (func (export "sub") (param $$x i32) (param $$y i32) (result i32) (i32.sub (local.get $$x) (local.get $$y)))
  (func (export "mul") (param $$x i32) (param $$y i32) (result i32) (i32.mul (local.get $$x) (local.get $$y)))
  (func (export "div_s") (param $$x i32) (param $$y i32) (result i32) (i32.div_s (local.get $$x) (local.get $$y)))
  (func (export "div_u") (param $$x i32) (param $$y i32) (result i32) (i32.div_u (local.get $$x) (local.get $$y)))
  (func (export "rem_s") (param $$x i32) (param $$y i32) (result i32) (i32.rem_s (local.get $$x) (local.get $$y)))
  (func (export "rem_u") (param $$x i32) (param $$y i32) (result i32) (i32.rem_u (local.get $$x) (local.get $$y)))
  (func (export "and") (param $$x i32) (param $$y i32) (result i32) (i32.and (local.get $$x) (local.get $$y)))
  (func (export "or") (param $$x i32) (param $$y i32) (result i32) (i32.or (local.get $$x) (local.get $$y)))
  (func (export "xor") (param $$x i32) (param $$y i32) (result i32) (i32.xor (local.get $$x) (local.get $$y)))
  (func (export "shl") (param $$x i32) (param $$y i32) (result i32) (i32.shl (local.get $$x) (local.get $$y)))
  (func (export "shr_s") (param $$x i32) (param $$y i32) (result i32) (i32.shr_s (local.get $$x) (local.get $$y)))
  (func (export "shr_u") (param $$x i32) (param $$y i32) (result i32) (i32.shr_u (local.get $$x) (local.get $$y)))
  (func (export "rotl") (param $$x i32) (param $$y i32) (result i32) (i32.rotl (local.get $$x) (local.get $$y)))
  (func (export "rotr") (param $$x i32) (param $$y i32) (result i32) (i32.rotr (local.get $$x) (local.get $$y)))
  (func (export "clz") (param $$x i32) (result i32) (i32.clz (local.get $$x)))
  (func (export "ctz") (param $$x i32) (result i32) (i32.ctz (local.get $$x)))
  (func (export "popcnt") (param $$x i32) (result i32) (i32.popcnt (local.get $$x)))
  (func (export "extend8_s") (param $$x i32) (result i32) (i32.extend8_s (local.get $$x)))
  (func (export "extend16_s") (param $$x i32) (result i32) (i32.extend16_s (local.get $$x)))
  (func (export "eqz") (param $$x i32) (result i32) (i32.eqz (local.get $$x)))
  (func (export "eq") (param $$x i32) (param $$y i32) (result i32) (i32.eq (local.get $$x) (local.get $$y)))
  (func (export "ne") (param $$x i32) (param $$y i32) (result i32) (i32.ne (local.get $$x) (local.get $$y)))
  (func (export "lt_s") (param $$x i32) (param $$y i32) (result i32) (i32.lt_s (local.get $$x) (local.get $$y)))
  (func (export "lt_u") (param $$x i32) (param $$y i32) (result i32) (i32.lt_u (local.get $$x) (local.get $$y)))
  (func (export "le_s") (param $$x i32) (param $$y i32) (result i32) (i32.le_s (local.get $$x) (local.get $$y)))
  (func (export "le_u") (param $$x i32) (param $$y i32) (result i32) (i32.le_u (local.get $$x) (local.get $$y)))
  (func (export "gt_s") (param $$x i32) (param $$y i32) (result i32) (i32.gt_s (local.get $$x) (local.get $$y)))
  (func (export "gt_u") (param $$x i32) (param $$y i32) (result i32) (i32.gt_u (local.get $$x) (local.get $$y)))
  (func (export "ge_s") (param $$x i32) (param $$y i32) (result i32) (i32.ge_s (local.get $$x) (local.get $$y)))
  (func (export "ge_u") (param $$x i32) (param $$y i32) (result i32) (i32.ge_u (local.get $$x) (local.get $$y)))
)`);

// ./test/core/i32.wast:37
assert_return(() => invoke($0, `add`, [1, 1]), [value("i32", 2)]);

// ./test/core/i32.wast:38
assert_return(() => invoke($0, `add`, [1, 0]), [value("i32", 1)]);

// ./test/core/i32.wast:39
assert_return(() => invoke($0, `add`, [-1, -1]), [value("i32", -2)]);

// ./test/core/i32.wast:40
assert_return(() => invoke($0, `add`, [-1, 1]), [value("i32", 0)]);

// ./test/core/i32.wast:41
assert_return(() => invoke($0, `add`, [2147483647, 1]), [
  value("i32", -2147483648),
]);

// ./test/core/i32.wast:42
assert_return(() => invoke($0, `add`, [-2147483648, -1]), [
  value("i32", 2147483647),
]);

// ./test/core/i32.wast:43
assert_return(() => invoke($0, `add`, [-2147483648, -2147483648]), [
  value("i32", 0),
]);

// ./test/core/i32.wast:44
assert_return(() => invoke($0, `add`, [1073741823, 1]), [
  value("i32", 1073741824),
]);

// ./test/core/i32.wast:46
assert_return(() => invoke($0, `sub`, [1, 1]), [value("i32", 0)]);

// ./test/core/i32.wast:47
assert_return(() => invoke($0, `sub`, [1, 0]), [value("i32", 1)]);

// ./test/core/i32.wast:48
assert_return(() => invoke($0, `sub`, [-1, -1]), [value("i32", 0)]);

// ./test/core/i32.wast:49
assert_return(() => invoke($0, `sub`, [2147483647, -1]), [
  value("i32", -2147483648),
]);

// ./test/core/i32.wast:50
assert_return(() => invoke($0, `sub`, [-2147483648, 1]), [
  value("i32", 2147483647),
]);

// ./test/core/i32.wast:51
assert_return(() => invoke($0, `sub`, [-2147483648, -2147483648]), [
  value("i32", 0),
]);

// ./test/core/i32.wast:52
assert_return(() => invoke($0, `sub`, [1073741823, -1]), [
  value("i32", 1073741824),
]);

// ./test/core/i32.wast:54
assert_return(() => invoke($0, `mul`, [1, 1]), [value("i32", 1)]);

// ./test/core/i32.wast:55
assert_return(() => invoke($0, `mul`, [1, 0]), [value("i32", 0)]);

// ./test/core/i32.wast:56
assert_return(() => invoke($0, `mul`, [-1, -1]), [value("i32", 1)]);

// ./test/core/i32.wast:57
assert_return(() => invoke($0, `mul`, [268435456, 4096]), [value("i32", 0)]);

// ./test/core/i32.wast:58
assert_return(() => invoke($0, `mul`, [-2147483648, 0]), [value("i32", 0)]);

// ./test/core/i32.wast:59
assert_return(() => invoke($0, `mul`, [-2147483648, -1]), [
  value("i32", -2147483648),
]);

// ./test/core/i32.wast:60
assert_return(() => invoke($0, `mul`, [2147483647, -1]), [
  value("i32", -2147483647),
]);

// ./test/core/i32.wast:61
assert_return(() => invoke($0, `mul`, [19088743, 1985229328]), [
  value("i32", 898528368),
]);

// ./test/core/i32.wast:62
assert_return(() => invoke($0, `mul`, [2147483647, 2147483647]), [
  value("i32", 1),
]);

// ./test/core/i32.wast:64
assert_trap(() => invoke($0, `div_s`, [1, 0]), `integer divide by zero`);

// ./test/core/i32.wast:65
assert_trap(() => invoke($0, `div_s`, [0, 0]), `integer divide by zero`);

// ./test/core/i32.wast:66
assert_trap(() => invoke($0, `div_s`, [-2147483648, -1]), `integer overflow`);

// ./test/core/i32.wast:67
assert_trap(
  () => invoke($0, `div_s`, [-2147483648, 0]),
  `integer divide by zero`,
);

// ./test/core/i32.wast:68
assert_return(() => invoke($0, `div_s`, [1, 1]), [value("i32", 1)]);

// ./test/core/i32.wast:69
assert_return(() => invoke($0, `div_s`, [0, 1]), [value("i32", 0)]);

// ./test/core/i32.wast:70
assert_return(() => invoke($0, `div_s`, [0, -1]), [value("i32", 0)]);

// ./test/core/i32.wast:71
assert_return(() => invoke($0, `div_s`, [-1, -1]), [value("i32", 1)]);

// ./test/core/i32.wast:72
assert_return(() => invoke($0, `div_s`, [-2147483648, 2]), [
  value("i32", -1073741824),
]);

// ./test/core/i32.wast:73
assert_return(() => invoke($0, `div_s`, [-2147483647, 1000]), [
  value("i32", -2147483),
]);

// ./test/core/i32.wast:74
assert_return(() => invoke($0, `div_s`, [5, 2]), [value("i32", 2)]);

// ./test/core/i32.wast:75
assert_return(() => invoke($0, `div_s`, [-5, 2]), [value("i32", -2)]);

// ./test/core/i32.wast:76
assert_return(() => invoke($0, `div_s`, [5, -2]), [value("i32", -2)]);

// ./test/core/i32.wast:77
assert_return(() => invoke($0, `div_s`, [-5, -2]), [value("i32", 2)]);

// ./test/core/i32.wast:78
assert_return(() => invoke($0, `div_s`, [7, 3]), [value("i32", 2)]);

// ./test/core/i32.wast:79
assert_return(() => invoke($0, `div_s`, [-7, 3]), [value("i32", -2)]);

// ./test/core/i32.wast:80
assert_return(() => invoke($0, `div_s`, [7, -3]), [value("i32", -2)]);

// ./test/core/i32.wast:81
assert_return(() => invoke($0, `div_s`, [-7, -3]), [value("i32", 2)]);

// ./test/core/i32.wast:82
assert_return(() => invoke($0, `div_s`, [11, 5]), [value("i32", 2)]);

// ./test/core/i32.wast:83
assert_return(() => invoke($0, `div_s`, [17, 7]), [value("i32", 2)]);

// ./test/core/i32.wast:85
assert_trap(() => invoke($0, `div_u`, [1, 0]), `integer divide by zero`);

// ./test/core/i32.wast:86
assert_trap(() => invoke($0, `div_u`, [0, 0]), `integer divide by zero`);

// ./test/core/i32.wast:87
assert_return(() => invoke($0, `div_u`, [1, 1]), [value("i32", 1)]);

// ./test/core/i32.wast:88
assert_return(() => invoke($0, `div_u`, [0, 1]), [value("i32", 0)]);

// ./test/core/i32.wast:89
assert_return(() => invoke($0, `div_u`, [-1, -1]), [value("i32", 1)]);

// ./test/core/i32.wast:90
assert_return(() => invoke($0, `div_u`, [-2147483648, -1]), [value("i32", 0)]);

// ./test/core/i32.wast:91
assert_return(() => invoke($0, `div_u`, [-2147483648, 2]), [
  value("i32", 1073741824),
]);

// ./test/core/i32.wast:92
assert_return(() => invoke($0, `div_u`, [-1880092688, 65537]), [
  value("i32", 36847),
]);

// ./test/core/i32.wast:93
assert_return(() => invoke($0, `div_u`, [-2147483647, 1000]), [
  value("i32", 2147483),
]);

// ./test/core/i32.wast:94
assert_return(() => invoke($0, `div_u`, [5, 2]), [value("i32", 2)]);

// ./test/core/i32.wast:95
assert_return(() => invoke($0, `div_u`, [-5, 2]), [value("i32", 2147483645)]);

// ./test/core/i32.wast:96
assert_return(() => invoke($0, `div_u`, [5, -2]), [value("i32", 0)]);

// ./test/core/i32.wast:97
assert_return(() => invoke($0, `div_u`, [-5, -2]), [value("i32", 0)]);

// ./test/core/i32.wast:98
assert_return(() => invoke($0, `div_u`, [7, 3]), [value("i32", 2)]);

// ./test/core/i32.wast:99
assert_return(() => invoke($0, `div_u`, [11, 5]), [value("i32", 2)]);

// ./test/core/i32.wast:100
assert_return(() => invoke($0, `div_u`, [17, 7]), [value("i32", 2)]);

// ./test/core/i32.wast:102
assert_trap(() => invoke($0, `rem_s`, [1, 0]), `integer divide by zero`);

// ./test/core/i32.wast:103
assert_trap(() => invoke($0, `rem_s`, [0, 0]), `integer divide by zero`);

// ./test/core/i32.wast:104
assert_return(() => invoke($0, `rem_s`, [2147483647, -1]), [value("i32", 0)]);

// ./test/core/i32.wast:105
assert_return(() => invoke($0, `rem_s`, [1, 1]), [value("i32", 0)]);

// ./test/core/i32.wast:106
assert_return(() => invoke($0, `rem_s`, [0, 1]), [value("i32", 0)]);

// ./test/core/i32.wast:107
assert_return(() => invoke($0, `rem_s`, [0, -1]), [value("i32", 0)]);

// ./test/core/i32.wast:108
assert_return(() => invoke($0, `rem_s`, [-1, -1]), [value("i32", 0)]);

// ./test/core/i32.wast:109
assert_return(() => invoke($0, `rem_s`, [-2147483648, -1]), [value("i32", 0)]);

// ./test/core/i32.wast:110
assert_return(() => invoke($0, `rem_s`, [-2147483648, 2]), [value("i32", 0)]);

// ./test/core/i32.wast:111
assert_return(() => invoke($0, `rem_s`, [-2147483647, 1000]), [
  value("i32", -647),
]);

// ./test/core/i32.wast:112
assert_return(() => invoke($0, `rem_s`, [5, 2]), [value("i32", 1)]);

// ./test/core/i32.wast:113
assert_return(() => invoke($0, `rem_s`, [-5, 2]), [value("i32", -1)]);

// ./test/core/i32.wast:114
assert_return(() => invoke($0, `rem_s`, [5, -2]), [value("i32", 1)]);

// ./test/core/i32.wast:115
assert_return(() => invoke($0, `rem_s`, [-5, -2]), [value("i32", -1)]);

// ./test/core/i32.wast:116
assert_return(() => invoke($0, `rem_s`, [7, 3]), [value("i32", 1)]);

// ./test/core/i32.wast:117
assert_return(() => invoke($0, `rem_s`, [-7, 3]), [value("i32", -1)]);

// ./test/core/i32.wast:118
assert_return(() => invoke($0, `rem_s`, [7, -3]), [value("i32", 1)]);

// ./test/core/i32.wast:119
assert_return(() => invoke($0, `rem_s`, [-7, -3]), [value("i32", -1)]);

// ./test/core/i32.wast:120
assert_return(() => invoke($0, `rem_s`, [11, 5]), [value("i32", 1)]);

// ./test/core/i32.wast:121
assert_return(() => invoke($0, `rem_s`, [17, 7]), [value("i32", 3)]);

// ./test/core/i32.wast:123
assert_trap(() => invoke($0, `rem_u`, [1, 0]), `integer divide by zero`);

// ./test/core/i32.wast:124
assert_trap(() => invoke($0, `rem_u`, [0, 0]), `integer divide by zero`);

// ./test/core/i32.wast:125
assert_return(() => invoke($0, `rem_u`, [1, 1]), [value("i32", 0)]);

// ./test/core/i32.wast:126
assert_return(() => invoke($0, `rem_u`, [0, 1]), [value("i32", 0)]);

// ./test/core/i32.wast:127
assert_return(() => invoke($0, `rem_u`, [-1, -1]), [value("i32", 0)]);

// ./test/core/i32.wast:128
assert_return(() => invoke($0, `rem_u`, [-2147483648, -1]), [
  value("i32", -2147483648),
]);

// ./test/core/i32.wast:129
assert_return(() => invoke($0, `rem_u`, [-2147483648, 2]), [value("i32", 0)]);

// ./test/core/i32.wast:130
assert_return(() => invoke($0, `rem_u`, [-1880092688, 65537]), [
  value("i32", 32769),
]);

// ./test/core/i32.wast:131
assert_return(() => invoke($0, `rem_u`, [-2147483647, 1000]), [
  value("i32", 649),
]);

// ./test/core/i32.wast:132
assert_return(() => invoke($0, `rem_u`, [5, 2]), [value("i32", 1)]);

// ./test/core/i32.wast:133
assert_return(() => invoke($0, `rem_u`, [-5, 2]), [value("i32", 1)]);

// ./test/core/i32.wast:134
assert_return(() => invoke($0, `rem_u`, [5, -2]), [value("i32", 5)]);

// ./test/core/i32.wast:135
assert_return(() => invoke($0, `rem_u`, [-5, -2]), [value("i32", -5)]);

// ./test/core/i32.wast:136
assert_return(() => invoke($0, `rem_u`, [7, 3]), [value("i32", 1)]);

// ./test/core/i32.wast:137
assert_return(() => invoke($0, `rem_u`, [11, 5]), [value("i32", 1)]);

// ./test/core/i32.wast:138
assert_return(() => invoke($0, `rem_u`, [17, 7]), [value("i32", 3)]);

// ./test/core/i32.wast:140
assert_return(() => invoke($0, `and`, [1, 0]), [value("i32", 0)]);

// ./test/core/i32.wast:141
assert_return(() => invoke($0, `and`, [0, 1]), [value("i32", 0)]);

// ./test/core/i32.wast:142
assert_return(() => invoke($0, `and`, [1, 1]), [value("i32", 1)]);

// ./test/core/i32.wast:143
assert_return(() => invoke($0, `and`, [0, 0]), [value("i32", 0)]);

// ./test/core/i32.wast:144
assert_return(() => invoke($0, `and`, [2147483647, -2147483648]), [
  value("i32", 0),
]);

// ./test/core/i32.wast:145
assert_return(() => invoke($0, `and`, [2147483647, -1]), [
  value("i32", 2147483647),
]);

// ./test/core/i32.wast:146
assert_return(() => invoke($0, `and`, [-252641281, -3856]), [
  value("i32", -252645136),
]);

// ./test/core/i32.wast:147
assert_return(() => invoke($0, `and`, [-1, -1]), [value("i32", -1)]);

// ./test/core/i32.wast:149
assert_return(() => invoke($0, `or`, [1, 0]), [value("i32", 1)]);

// ./test/core/i32.wast:150
assert_return(() => invoke($0, `or`, [0, 1]), [value("i32", 1)]);

// ./test/core/i32.wast:151
assert_return(() => invoke($0, `or`, [1, 1]), [value("i32", 1)]);

// ./test/core/i32.wast:152
assert_return(() => invoke($0, `or`, [0, 0]), [value("i32", 0)]);

// ./test/core/i32.wast:153
assert_return(() => invoke($0, `or`, [2147483647, -2147483648]), [
  value("i32", -1),
]);

// ./test/core/i32.wast:154
assert_return(() => invoke($0, `or`, [-2147483648, 0]), [
  value("i32", -2147483648),
]);

// ./test/core/i32.wast:155
assert_return(() => invoke($0, `or`, [-252641281, -3856]), [value("i32", -1)]);

// ./test/core/i32.wast:156
assert_return(() => invoke($0, `or`, [-1, -1]), [value("i32", -1)]);

// ./test/core/i32.wast:158
assert_return(() => invoke($0, `xor`, [1, 0]), [value("i32", 1)]);

// ./test/core/i32.wast:159
assert_return(() => invoke($0, `xor`, [0, 1]), [value("i32", 1)]);

// ./test/core/i32.wast:160
assert_return(() => invoke($0, `xor`, [1, 1]), [value("i32", 0)]);

// ./test/core/i32.wast:161
assert_return(() => invoke($0, `xor`, [0, 0]), [value("i32", 0)]);

// ./test/core/i32.wast:162
assert_return(() => invoke($0, `xor`, [2147483647, -2147483648]), [
  value("i32", -1),
]);

// ./test/core/i32.wast:163
assert_return(() => invoke($0, `xor`, [-2147483648, 0]), [
  value("i32", -2147483648),
]);

// ./test/core/i32.wast:164
assert_return(() => invoke($0, `xor`, [-1, -2147483648]), [
  value("i32", 2147483647),
]);

// ./test/core/i32.wast:165
assert_return(() => invoke($0, `xor`, [-1, 2147483647]), [
  value("i32", -2147483648),
]);

// ./test/core/i32.wast:166
assert_return(() => invoke($0, `xor`, [-252641281, -3856]), [
  value("i32", 252645135),
]);

// ./test/core/i32.wast:167
assert_return(() => invoke($0, `xor`, [-1, -1]), [value("i32", 0)]);

// ./test/core/i32.wast:169
assert_return(() => invoke($0, `shl`, [1, 1]), [value("i32", 2)]);

// ./test/core/i32.wast:170
assert_return(() => invoke($0, `shl`, [1, 0]), [value("i32", 1)]);

// ./test/core/i32.wast:171
assert_return(() => invoke($0, `shl`, [2147483647, 1]), [value("i32", -2)]);

// ./test/core/i32.wast:172
assert_return(() => invoke($0, `shl`, [-1, 1]), [value("i32", -2)]);

// ./test/core/i32.wast:173
assert_return(() => invoke($0, `shl`, [-2147483648, 1]), [value("i32", 0)]);

// ./test/core/i32.wast:174
assert_return(() => invoke($0, `shl`, [1073741824, 1]), [
  value("i32", -2147483648),
]);

// ./test/core/i32.wast:175
assert_return(() => invoke($0, `shl`, [1, 31]), [value("i32", -2147483648)]);

// ./test/core/i32.wast:176
assert_return(() => invoke($0, `shl`, [1, 32]), [value("i32", 1)]);

// ./test/core/i32.wast:177
assert_return(() => invoke($0, `shl`, [1, 33]), [value("i32", 2)]);

// ./test/core/i32.wast:178
assert_return(() => invoke($0, `shl`, [1, -1]), [value("i32", -2147483648)]);

// ./test/core/i32.wast:179
assert_return(() => invoke($0, `shl`, [1, 2147483647]), [
  value("i32", -2147483648),
]);

// ./test/core/i32.wast:181
assert_return(() => invoke($0, `shr_s`, [1, 1]), [value("i32", 0)]);

// ./test/core/i32.wast:182
assert_return(() => invoke($0, `shr_s`, [1, 0]), [value("i32", 1)]);

// ./test/core/i32.wast:183
assert_return(() => invoke($0, `shr_s`, [-1, 1]), [value("i32", -1)]);

// ./test/core/i32.wast:184
assert_return(() => invoke($0, `shr_s`, [2147483647, 1]), [
  value("i32", 1073741823),
]);

// ./test/core/i32.wast:185
assert_return(() => invoke($0, `shr_s`, [-2147483648, 1]), [
  value("i32", -1073741824),
]);

// ./test/core/i32.wast:186
assert_return(() => invoke($0, `shr_s`, [1073741824, 1]), [
  value("i32", 536870912),
]);

// ./test/core/i32.wast:187
assert_return(() => invoke($0, `shr_s`, [1, 32]), [value("i32", 1)]);

// ./test/core/i32.wast:188
assert_return(() => invoke($0, `shr_s`, [1, 33]), [value("i32", 0)]);

// ./test/core/i32.wast:189
assert_return(() => invoke($0, `shr_s`, [1, -1]), [value("i32", 0)]);

// ./test/core/i32.wast:190
assert_return(() => invoke($0, `shr_s`, [1, 2147483647]), [value("i32", 0)]);

// ./test/core/i32.wast:191
assert_return(() => invoke($0, `shr_s`, [1, -2147483648]), [value("i32", 1)]);

// ./test/core/i32.wast:192
assert_return(() => invoke($0, `shr_s`, [-2147483648, 31]), [value("i32", -1)]);

// ./test/core/i32.wast:193
assert_return(() => invoke($0, `shr_s`, [-1, 32]), [value("i32", -1)]);

// ./test/core/i32.wast:194
assert_return(() => invoke($0, `shr_s`, [-1, 33]), [value("i32", -1)]);

// ./test/core/i32.wast:195
assert_return(() => invoke($0, `shr_s`, [-1, -1]), [value("i32", -1)]);

// ./test/core/i32.wast:196
assert_return(() => invoke($0, `shr_s`, [-1, 2147483647]), [value("i32", -1)]);

// ./test/core/i32.wast:197
assert_return(() => invoke($0, `shr_s`, [-1, -2147483648]), [value("i32", -1)]);

// ./test/core/i32.wast:199
assert_return(() => invoke($0, `shr_u`, [1, 1]), [value("i32", 0)]);

// ./test/core/i32.wast:200
assert_return(() => invoke($0, `shr_u`, [1, 0]), [value("i32", 1)]);

// ./test/core/i32.wast:201
assert_return(() => invoke($0, `shr_u`, [-1, 1]), [value("i32", 2147483647)]);

// ./test/core/i32.wast:202
assert_return(() => invoke($0, `shr_u`, [2147483647, 1]), [
  value("i32", 1073741823),
]);

// ./test/core/i32.wast:203
assert_return(() => invoke($0, `shr_u`, [-2147483648, 1]), [
  value("i32", 1073741824),
]);

// ./test/core/i32.wast:204
assert_return(() => invoke($0, `shr_u`, [1073741824, 1]), [
  value("i32", 536870912),
]);

// ./test/core/i32.wast:205
assert_return(() => invoke($0, `shr_u`, [1, 32]), [value("i32", 1)]);

// ./test/core/i32.wast:206
assert_return(() => invoke($0, `shr_u`, [1, 33]), [value("i32", 0)]);

// ./test/core/i32.wast:207
assert_return(() => invoke($0, `shr_u`, [1, -1]), [value("i32", 0)]);

// ./test/core/i32.wast:208
assert_return(() => invoke($0, `shr_u`, [1, 2147483647]), [value("i32", 0)]);

// ./test/core/i32.wast:209
assert_return(() => invoke($0, `shr_u`, [1, -2147483648]), [value("i32", 1)]);

// ./test/core/i32.wast:210
assert_return(() => invoke($0, `shr_u`, [-2147483648, 31]), [value("i32", 1)]);

// ./test/core/i32.wast:211
assert_return(() => invoke($0, `shr_u`, [-1, 32]), [value("i32", -1)]);

// ./test/core/i32.wast:212
assert_return(() => invoke($0, `shr_u`, [-1, 33]), [value("i32", 2147483647)]);

// ./test/core/i32.wast:213
assert_return(() => invoke($0, `shr_u`, [-1, -1]), [value("i32", 1)]);

// ./test/core/i32.wast:214
assert_return(() => invoke($0, `shr_u`, [-1, 2147483647]), [value("i32", 1)]);

// ./test/core/i32.wast:215
assert_return(() => invoke($0, `shr_u`, [-1, -2147483648]), [value("i32", -1)]);

// ./test/core/i32.wast:217
assert_return(() => invoke($0, `rotl`, [1, 1]), [value("i32", 2)]);

// ./test/core/i32.wast:218
assert_return(() => invoke($0, `rotl`, [1, 0]), [value("i32", 1)]);

// ./test/core/i32.wast:219
assert_return(() => invoke($0, `rotl`, [-1, 1]), [value("i32", -1)]);

// ./test/core/i32.wast:220
assert_return(() => invoke($0, `rotl`, [1, 32]), [value("i32", 1)]);

// ./test/core/i32.wast:221
assert_return(() => invoke($0, `rotl`, [-1412589450, 1]), [
  value("i32", 1469788397),
]);

// ./test/core/i32.wast:222
assert_return(() => invoke($0, `rotl`, [-33498112, 4]), [
  value("i32", -535969777),
]);

// ./test/core/i32.wast:223
assert_return(() => invoke($0, `rotl`, [-1329474845, 5]), [
  value("i32", 406477942),
]);

// ./test/core/i32.wast:224
assert_return(() => invoke($0, `rotl`, [32768, 37]), [value("i32", 1048576)]);

// ./test/core/i32.wast:225
assert_return(() => invoke($0, `rotl`, [-1329474845, 65285]), [
  value("i32", 406477942),
]);

// ./test/core/i32.wast:226
assert_return(() => invoke($0, `rotl`, [1989852383, -19]), [
  value("i32", 1469837011),
]);

// ./test/core/i32.wast:227
assert_return(() => invoke($0, `rotl`, [1989852383, -2147483635]), [
  value("i32", 1469837011),
]);

// ./test/core/i32.wast:228
assert_return(() => invoke($0, `rotl`, [1, 31]), [value("i32", -2147483648)]);

// ./test/core/i32.wast:229
assert_return(() => invoke($0, `rotl`, [-2147483648, 1]), [value("i32", 1)]);

// ./test/core/i32.wast:231
assert_return(() => invoke($0, `rotr`, [1, 1]), [value("i32", -2147483648)]);

// ./test/core/i32.wast:232
assert_return(() => invoke($0, `rotr`, [1, 0]), [value("i32", 1)]);

// ./test/core/i32.wast:233
assert_return(() => invoke($0, `rotr`, [-1, 1]), [value("i32", -1)]);

// ./test/core/i32.wast:234
assert_return(() => invoke($0, `rotr`, [1, 32]), [value("i32", 1)]);

// ./test/core/i32.wast:235
assert_return(() => invoke($0, `rotr`, [-16724992, 1]), [
  value("i32", 2139121152),
]);

// ./test/core/i32.wast:236
assert_return(() => invoke($0, `rotr`, [524288, 4]), [value("i32", 32768)]);

// ./test/core/i32.wast:237
assert_return(() => invoke($0, `rotr`, [-1329474845, 5]), [
  value("i32", 495324823),
]);

// ./test/core/i32.wast:238
assert_return(() => invoke($0, `rotr`, [32768, 37]), [value("i32", 1024)]);

// ./test/core/i32.wast:239
assert_return(() => invoke($0, `rotr`, [-1329474845, 65285]), [
  value("i32", 495324823),
]);

// ./test/core/i32.wast:240
assert_return(() => invoke($0, `rotr`, [1989852383, -19]), [
  value("i32", -419711787),
]);

// ./test/core/i32.wast:241
assert_return(() => invoke($0, `rotr`, [1989852383, -2147483635]), [
  value("i32", -419711787),
]);

// ./test/core/i32.wast:242
assert_return(() => invoke($0, `rotr`, [1, 31]), [value("i32", 2)]);

// ./test/core/i32.wast:243
assert_return(() => invoke($0, `rotr`, [-2147483648, 31]), [value("i32", 1)]);

// ./test/core/i32.wast:245
assert_return(() => invoke($0, `clz`, [-1]), [value("i32", 0)]);

// ./test/core/i32.wast:246
assert_return(() => invoke($0, `clz`, [0]), [value("i32", 32)]);

// ./test/core/i32.wast:247
assert_return(() => invoke($0, `clz`, [32768]), [value("i32", 16)]);

// ./test/core/i32.wast:248
assert_return(() => invoke($0, `clz`, [255]), [value("i32", 24)]);

// ./test/core/i32.wast:249
assert_return(() => invoke($0, `clz`, [-2147483648]), [value("i32", 0)]);

// ./test/core/i32.wast:250
assert_return(() => invoke($0, `clz`, [1]), [value("i32", 31)]);

// ./test/core/i32.wast:251
assert_return(() => invoke($0, `clz`, [2]), [value("i32", 30)]);

// ./test/core/i32.wast:252
assert_return(() => invoke($0, `clz`, [2147483647]), [value("i32", 1)]);

// ./test/core/i32.wast:254
assert_return(() => invoke($0, `ctz`, [-1]), [value("i32", 0)]);

// ./test/core/i32.wast:255
assert_return(() => invoke($0, `ctz`, [0]), [value("i32", 32)]);

// ./test/core/i32.wast:256
assert_return(() => invoke($0, `ctz`, [32768]), [value("i32", 15)]);

// ./test/core/i32.wast:257
assert_return(() => invoke($0, `ctz`, [65536]), [value("i32", 16)]);

// ./test/core/i32.wast:258
assert_return(() => invoke($0, `ctz`, [-2147483648]), [value("i32", 31)]);

// ./test/core/i32.wast:259
assert_return(() => invoke($0, `ctz`, [2147483647]), [value("i32", 0)]);

// ./test/core/i32.wast:261
assert_return(() => invoke($0, `popcnt`, [-1]), [value("i32", 32)]);

// ./test/core/i32.wast:262
assert_return(() => invoke($0, `popcnt`, [0]), [value("i32", 0)]);

// ./test/core/i32.wast:263
assert_return(() => invoke($0, `popcnt`, [32768]), [value("i32", 1)]);

// ./test/core/i32.wast:264
assert_return(() => invoke($0, `popcnt`, [-2147450880]), [value("i32", 2)]);

// ./test/core/i32.wast:265
assert_return(() => invoke($0, `popcnt`, [2147483647]), [value("i32", 31)]);

// ./test/core/i32.wast:266
assert_return(() => invoke($0, `popcnt`, [-1431655766]), [value("i32", 16)]);

// ./test/core/i32.wast:267
assert_return(() => invoke($0, `popcnt`, [1431655765]), [value("i32", 16)]);

// ./test/core/i32.wast:268
assert_return(() => invoke($0, `popcnt`, [-559038737]), [value("i32", 24)]);

// ./test/core/i32.wast:270
assert_return(() => invoke($0, `extend8_s`, [0]), [value("i32", 0)]);

// ./test/core/i32.wast:271
assert_return(() => invoke($0, `extend8_s`, [127]), [value("i32", 127)]);

// ./test/core/i32.wast:272
assert_return(() => invoke($0, `extend8_s`, [128]), [value("i32", -128)]);

// ./test/core/i32.wast:273
assert_return(() => invoke($0, `extend8_s`, [255]), [value("i32", -1)]);

// ./test/core/i32.wast:274
assert_return(() => invoke($0, `extend8_s`, [19088640]), [value("i32", 0)]);

// ./test/core/i32.wast:275
assert_return(() => invoke($0, `extend8_s`, [-19088768]), [value("i32", -128)]);

// ./test/core/i32.wast:276
assert_return(() => invoke($0, `extend8_s`, [-1]), [value("i32", -1)]);

// ./test/core/i32.wast:278
assert_return(() => invoke($0, `extend16_s`, [0]), [value("i32", 0)]);

// ./test/core/i32.wast:279
assert_return(() => invoke($0, `extend16_s`, [32767]), [value("i32", 32767)]);

// ./test/core/i32.wast:280
assert_return(() => invoke($0, `extend16_s`, [32768]), [value("i32", -32768)]);

// ./test/core/i32.wast:281
assert_return(() => invoke($0, `extend16_s`, [65535]), [value("i32", -1)]);

// ./test/core/i32.wast:282
assert_return(() => invoke($0, `extend16_s`, [19070976]), [value("i32", 0)]);

// ./test/core/i32.wast:283
assert_return(() => invoke($0, `extend16_s`, [-19103744]), [
  value("i32", -32768),
]);

// ./test/core/i32.wast:284
assert_return(() => invoke($0, `extend16_s`, [-1]), [value("i32", -1)]);

// ./test/core/i32.wast:286
assert_return(() => invoke($0, `eqz`, [0]), [value("i32", 1)]);

// ./test/core/i32.wast:287
assert_return(() => invoke($0, `eqz`, [1]), [value("i32", 0)]);

// ./test/core/i32.wast:288
assert_return(() => invoke($0, `eqz`, [-2147483648]), [value("i32", 0)]);

// ./test/core/i32.wast:289
assert_return(() => invoke($0, `eqz`, [2147483647]), [value("i32", 0)]);

// ./test/core/i32.wast:290
assert_return(() => invoke($0, `eqz`, [-1]), [value("i32", 0)]);

// ./test/core/i32.wast:292
assert_return(() => invoke($0, `eq`, [0, 0]), [value("i32", 1)]);

// ./test/core/i32.wast:293
assert_return(() => invoke($0, `eq`, [1, 1]), [value("i32", 1)]);

// ./test/core/i32.wast:294
assert_return(() => invoke($0, `eq`, [-1, 1]), [value("i32", 0)]);

// ./test/core/i32.wast:295
assert_return(() => invoke($0, `eq`, [-2147483648, -2147483648]), [
  value("i32", 1),
]);

// ./test/core/i32.wast:296
assert_return(() => invoke($0, `eq`, [2147483647, 2147483647]), [
  value("i32", 1),
]);

// ./test/core/i32.wast:297
assert_return(() => invoke($0, `eq`, [-1, -1]), [value("i32", 1)]);

// ./test/core/i32.wast:298
assert_return(() => invoke($0, `eq`, [1, 0]), [value("i32", 0)]);

// ./test/core/i32.wast:299
assert_return(() => invoke($0, `eq`, [0, 1]), [value("i32", 0)]);

// ./test/core/i32.wast:300
assert_return(() => invoke($0, `eq`, [-2147483648, 0]), [value("i32", 0)]);

// ./test/core/i32.wast:301
assert_return(() => invoke($0, `eq`, [0, -2147483648]), [value("i32", 0)]);

// ./test/core/i32.wast:302
assert_return(() => invoke($0, `eq`, [-2147483648, -1]), [value("i32", 0)]);

// ./test/core/i32.wast:303
assert_return(() => invoke($0, `eq`, [-1, -2147483648]), [value("i32", 0)]);

// ./test/core/i32.wast:304
assert_return(() => invoke($0, `eq`, [-2147483648, 2147483647]), [
  value("i32", 0),
]);

// ./test/core/i32.wast:305
assert_return(() => invoke($0, `eq`, [2147483647, -2147483648]), [
  value("i32", 0),
]);

// ./test/core/i32.wast:307
assert_return(() => invoke($0, `ne`, [0, 0]), [value("i32", 0)]);

// ./test/core/i32.wast:308
assert_return(() => invoke($0, `ne`, [1, 1]), [value("i32", 0)]);

// ./test/core/i32.wast:309
assert_return(() => invoke($0, `ne`, [-1, 1]), [value("i32", 1)]);

// ./test/core/i32.wast:310
assert_return(() => invoke($0, `ne`, [-2147483648, -2147483648]), [
  value("i32", 0),
]);

// ./test/core/i32.wast:311
assert_return(() => invoke($0, `ne`, [2147483647, 2147483647]), [
  value("i32", 0),
]);

// ./test/core/i32.wast:312
assert_return(() => invoke($0, `ne`, [-1, -1]), [value("i32", 0)]);

// ./test/core/i32.wast:313
assert_return(() => invoke($0, `ne`, [1, 0]), [value("i32", 1)]);

// ./test/core/i32.wast:314
assert_return(() => invoke($0, `ne`, [0, 1]), [value("i32", 1)]);

// ./test/core/i32.wast:315
assert_return(() => invoke($0, `ne`, [-2147483648, 0]), [value("i32", 1)]);

// ./test/core/i32.wast:316
assert_return(() => invoke($0, `ne`, [0, -2147483648]), [value("i32", 1)]);

// ./test/core/i32.wast:317
assert_return(() => invoke($0, `ne`, [-2147483648, -1]), [value("i32", 1)]);

// ./test/core/i32.wast:318
assert_return(() => invoke($0, `ne`, [-1, -2147483648]), [value("i32", 1)]);

// ./test/core/i32.wast:319
assert_return(() => invoke($0, `ne`, [-2147483648, 2147483647]), [
  value("i32", 1),
]);

// ./test/core/i32.wast:320
assert_return(() => invoke($0, `ne`, [2147483647, -2147483648]), [
  value("i32", 1),
]);

// ./test/core/i32.wast:322
assert_return(() => invoke($0, `lt_s`, [0, 0]), [value("i32", 0)]);

// ./test/core/i32.wast:323
assert_return(() => invoke($0, `lt_s`, [1, 1]), [value("i32", 0)]);

// ./test/core/i32.wast:324
assert_return(() => invoke($0, `lt_s`, [-1, 1]), [value("i32", 1)]);

// ./test/core/i32.wast:325
assert_return(() => invoke($0, `lt_s`, [-2147483648, -2147483648]), [
  value("i32", 0),
]);

// ./test/core/i32.wast:326
assert_return(() => invoke($0, `lt_s`, [2147483647, 2147483647]), [
  value("i32", 0),
]);

// ./test/core/i32.wast:327
assert_return(() => invoke($0, `lt_s`, [-1, -1]), [value("i32", 0)]);

// ./test/core/i32.wast:328
assert_return(() => invoke($0, `lt_s`, [1, 0]), [value("i32", 0)]);

// ./test/core/i32.wast:329
assert_return(() => invoke($0, `lt_s`, [0, 1]), [value("i32", 1)]);

// ./test/core/i32.wast:330
assert_return(() => invoke($0, `lt_s`, [-2147483648, 0]), [value("i32", 1)]);

// ./test/core/i32.wast:331
assert_return(() => invoke($0, `lt_s`, [0, -2147483648]), [value("i32", 0)]);

// ./test/core/i32.wast:332
assert_return(() => invoke($0, `lt_s`, [-2147483648, -1]), [value("i32", 1)]);

// ./test/core/i32.wast:333
assert_return(() => invoke($0, `lt_s`, [-1, -2147483648]), [value("i32", 0)]);

// ./test/core/i32.wast:334
assert_return(() => invoke($0, `lt_s`, [-2147483648, 2147483647]), [
  value("i32", 1),
]);

// ./test/core/i32.wast:335
assert_return(() => invoke($0, `lt_s`, [2147483647, -2147483648]), [
  value("i32", 0),
]);

// ./test/core/i32.wast:337
assert_return(() => invoke($0, `lt_u`, [0, 0]), [value("i32", 0)]);

// ./test/core/i32.wast:338
assert_return(() => invoke($0, `lt_u`, [1, 1]), [value("i32", 0)]);

// ./test/core/i32.wast:339
assert_return(() => invoke($0, `lt_u`, [-1, 1]), [value("i32", 0)]);

// ./test/core/i32.wast:340
assert_return(() => invoke($0, `lt_u`, [-2147483648, -2147483648]), [
  value("i32", 0),
]);

// ./test/core/i32.wast:341
assert_return(() => invoke($0, `lt_u`, [2147483647, 2147483647]), [
  value("i32", 0),
]);

// ./test/core/i32.wast:342
assert_return(() => invoke($0, `lt_u`, [-1, -1]), [value("i32", 0)]);

// ./test/core/i32.wast:343
assert_return(() => invoke($0, `lt_u`, [1, 0]), [value("i32", 0)]);

// ./test/core/i32.wast:344
assert_return(() => invoke($0, `lt_u`, [0, 1]), [value("i32", 1)]);

// ./test/core/i32.wast:345
assert_return(() => invoke($0, `lt_u`, [-2147483648, 0]), [value("i32", 0)]);

// ./test/core/i32.wast:346
assert_return(() => invoke($0, `lt_u`, [0, -2147483648]), [value("i32", 1)]);

// ./test/core/i32.wast:347
assert_return(() => invoke($0, `lt_u`, [-2147483648, -1]), [value("i32", 1)]);

// ./test/core/i32.wast:348
assert_return(() => invoke($0, `lt_u`, [-1, -2147483648]), [value("i32", 0)]);

// ./test/core/i32.wast:349
assert_return(() => invoke($0, `lt_u`, [-2147483648, 2147483647]), [
  value("i32", 0),
]);

// ./test/core/i32.wast:350
assert_return(() => invoke($0, `lt_u`, [2147483647, -2147483648]), [
  value("i32", 1),
]);

// ./test/core/i32.wast:352
assert_return(() => invoke($0, `le_s`, [0, 0]), [value("i32", 1)]);

// ./test/core/i32.wast:353
assert_return(() => invoke($0, `le_s`, [1, 1]), [value("i32", 1)]);

// ./test/core/i32.wast:354
assert_return(() => invoke($0, `le_s`, [-1, 1]), [value("i32", 1)]);

// ./test/core/i32.wast:355
assert_return(() => invoke($0, `le_s`, [-2147483648, -2147483648]), [
  value("i32", 1),
]);

// ./test/core/i32.wast:356
assert_return(() => invoke($0, `le_s`, [2147483647, 2147483647]), [
  value("i32", 1),
]);

// ./test/core/i32.wast:357
assert_return(() => invoke($0, `le_s`, [-1, -1]), [value("i32", 1)]);

// ./test/core/i32.wast:358
assert_return(() => invoke($0, `le_s`, [1, 0]), [value("i32", 0)]);

// ./test/core/i32.wast:359
assert_return(() => invoke($0, `le_s`, [0, 1]), [value("i32", 1)]);

// ./test/core/i32.wast:360
assert_return(() => invoke($0, `le_s`, [-2147483648, 0]), [value("i32", 1)]);

// ./test/core/i32.wast:361
assert_return(() => invoke($0, `le_s`, [0, -2147483648]), [value("i32", 0)]);

// ./test/core/i32.wast:362
assert_return(() => invoke($0, `le_s`, [-2147483648, -1]), [value("i32", 1)]);

// ./test/core/i32.wast:363
assert_return(() => invoke($0, `le_s`, [-1, -2147483648]), [value("i32", 0)]);

// ./test/core/i32.wast:364
assert_return(() => invoke($0, `le_s`, [-2147483648, 2147483647]), [
  value("i32", 1),
]);

// ./test/core/i32.wast:365
assert_return(() => invoke($0, `le_s`, [2147483647, -2147483648]), [
  value("i32", 0),
]);

// ./test/core/i32.wast:367
assert_return(() => invoke($0, `le_u`, [0, 0]), [value("i32", 1)]);

// ./test/core/i32.wast:368
assert_return(() => invoke($0, `le_u`, [1, 1]), [value("i32", 1)]);

// ./test/core/i32.wast:369
assert_return(() => invoke($0, `le_u`, [-1, 1]), [value("i32", 0)]);

// ./test/core/i32.wast:370
assert_return(() => invoke($0, `le_u`, [-2147483648, -2147483648]), [
  value("i32", 1),
]);

// ./test/core/i32.wast:371
assert_return(() => invoke($0, `le_u`, [2147483647, 2147483647]), [
  value("i32", 1),
]);

// ./test/core/i32.wast:372
assert_return(() => invoke($0, `le_u`, [-1, -1]), [value("i32", 1)]);

// ./test/core/i32.wast:373
assert_return(() => invoke($0, `le_u`, [1, 0]), [value("i32", 0)]);

// ./test/core/i32.wast:374
assert_return(() => invoke($0, `le_u`, [0, 1]), [value("i32", 1)]);

// ./test/core/i32.wast:375
assert_return(() => invoke($0, `le_u`, [-2147483648, 0]), [value("i32", 0)]);

// ./test/core/i32.wast:376
assert_return(() => invoke($0, `le_u`, [0, -2147483648]), [value("i32", 1)]);

// ./test/core/i32.wast:377
assert_return(() => invoke($0, `le_u`, [-2147483648, -1]), [value("i32", 1)]);

// ./test/core/i32.wast:378
assert_return(() => invoke($0, `le_u`, [-1, -2147483648]), [value("i32", 0)]);

// ./test/core/i32.wast:379
assert_return(() => invoke($0, `le_u`, [-2147483648, 2147483647]), [
  value("i32", 0),
]);

// ./test/core/i32.wast:380
assert_return(() => invoke($0, `le_u`, [2147483647, -2147483648]), [
  value("i32", 1),
]);

// ./test/core/i32.wast:382
assert_return(() => invoke($0, `gt_s`, [0, 0]), [value("i32", 0)]);

// ./test/core/i32.wast:383
assert_return(() => invoke($0, `gt_s`, [1, 1]), [value("i32", 0)]);

// ./test/core/i32.wast:384
assert_return(() => invoke($0, `gt_s`, [-1, 1]), [value("i32", 0)]);

// ./test/core/i32.wast:385
assert_return(() => invoke($0, `gt_s`, [-2147483648, -2147483648]), [
  value("i32", 0),
]);

// ./test/core/i32.wast:386
assert_return(() => invoke($0, `gt_s`, [2147483647, 2147483647]), [
  value("i32", 0),
]);

// ./test/core/i32.wast:387
assert_return(() => invoke($0, `gt_s`, [-1, -1]), [value("i32", 0)]);

// ./test/core/i32.wast:388
assert_return(() => invoke($0, `gt_s`, [1, 0]), [value("i32", 1)]);

// ./test/core/i32.wast:389
assert_return(() => invoke($0, `gt_s`, [0, 1]), [value("i32", 0)]);

// ./test/core/i32.wast:390
assert_return(() => invoke($0, `gt_s`, [-2147483648, 0]), [value("i32", 0)]);

// ./test/core/i32.wast:391
assert_return(() => invoke($0, `gt_s`, [0, -2147483648]), [value("i32", 1)]);

// ./test/core/i32.wast:392
assert_return(() => invoke($0, `gt_s`, [-2147483648, -1]), [value("i32", 0)]);

// ./test/core/i32.wast:393
assert_return(() => invoke($0, `gt_s`, [-1, -2147483648]), [value("i32", 1)]);

// ./test/core/i32.wast:394
assert_return(() => invoke($0, `gt_s`, [-2147483648, 2147483647]), [
  value("i32", 0),
]);

// ./test/core/i32.wast:395
assert_return(() => invoke($0, `gt_s`, [2147483647, -2147483648]), [
  value("i32", 1),
]);

// ./test/core/i32.wast:397
assert_return(() => invoke($0, `gt_u`, [0, 0]), [value("i32", 0)]);

// ./test/core/i32.wast:398
assert_return(() => invoke($0, `gt_u`, [1, 1]), [value("i32", 0)]);

// ./test/core/i32.wast:399
assert_return(() => invoke($0, `gt_u`, [-1, 1]), [value("i32", 1)]);

// ./test/core/i32.wast:400
assert_return(() => invoke($0, `gt_u`, [-2147483648, -2147483648]), [
  value("i32", 0),
]);

// ./test/core/i32.wast:401
assert_return(() => invoke($0, `gt_u`, [2147483647, 2147483647]), [
  value("i32", 0),
]);

// ./test/core/i32.wast:402
assert_return(() => invoke($0, `gt_u`, [-1, -1]), [value("i32", 0)]);

// ./test/core/i32.wast:403
assert_return(() => invoke($0, `gt_u`, [1, 0]), [value("i32", 1)]);

// ./test/core/i32.wast:404
assert_return(() => invoke($0, `gt_u`, [0, 1]), [value("i32", 0)]);

// ./test/core/i32.wast:405
assert_return(() => invoke($0, `gt_u`, [-2147483648, 0]), [value("i32", 1)]);

// ./test/core/i32.wast:406
assert_return(() => invoke($0, `gt_u`, [0, -2147483648]), [value("i32", 0)]);

// ./test/core/i32.wast:407
assert_return(() => invoke($0, `gt_u`, [-2147483648, -1]), [value("i32", 0)]);

// ./test/core/i32.wast:408
assert_return(() => invoke($0, `gt_u`, [-1, -2147483648]), [value("i32", 1)]);

// ./test/core/i32.wast:409
assert_return(() => invoke($0, `gt_u`, [-2147483648, 2147483647]), [
  value("i32", 1),
]);

// ./test/core/i32.wast:410
assert_return(() => invoke($0, `gt_u`, [2147483647, -2147483648]), [
  value("i32", 0),
]);

// ./test/core/i32.wast:412
assert_return(() => invoke($0, `ge_s`, [0, 0]), [value("i32", 1)]);

// ./test/core/i32.wast:413
assert_return(() => invoke($0, `ge_s`, [1, 1]), [value("i32", 1)]);

// ./test/core/i32.wast:414
assert_return(() => invoke($0, `ge_s`, [-1, 1]), [value("i32", 0)]);

// ./test/core/i32.wast:415
assert_return(() => invoke($0, `ge_s`, [-2147483648, -2147483648]), [
  value("i32", 1),
]);

// ./test/core/i32.wast:416
assert_return(() => invoke($0, `ge_s`, [2147483647, 2147483647]), [
  value("i32", 1),
]);

// ./test/core/i32.wast:417
assert_return(() => invoke($0, `ge_s`, [-1, -1]), [value("i32", 1)]);

// ./test/core/i32.wast:418
assert_return(() => invoke($0, `ge_s`, [1, 0]), [value("i32", 1)]);

// ./test/core/i32.wast:419
assert_return(() => invoke($0, `ge_s`, [0, 1]), [value("i32", 0)]);

// ./test/core/i32.wast:420
assert_return(() => invoke($0, `ge_s`, [-2147483648, 0]), [value("i32", 0)]);

// ./test/core/i32.wast:421
assert_return(() => invoke($0, `ge_s`, [0, -2147483648]), [value("i32", 1)]);

// ./test/core/i32.wast:422
assert_return(() => invoke($0, `ge_s`, [-2147483648, -1]), [value("i32", 0)]);

// ./test/core/i32.wast:423
assert_return(() => invoke($0, `ge_s`, [-1, -2147483648]), [value("i32", 1)]);

// ./test/core/i32.wast:424
assert_return(() => invoke($0, `ge_s`, [-2147483648, 2147483647]), [
  value("i32", 0),
]);

// ./test/core/i32.wast:425
assert_return(() => invoke($0, `ge_s`, [2147483647, -2147483648]), [
  value("i32", 1),
]);

// ./test/core/i32.wast:427
assert_return(() => invoke($0, `ge_u`, [0, 0]), [value("i32", 1)]);

// ./test/core/i32.wast:428
assert_return(() => invoke($0, `ge_u`, [1, 1]), [value("i32", 1)]);

// ./test/core/i32.wast:429
assert_return(() => invoke($0, `ge_u`, [-1, 1]), [value("i32", 1)]);

// ./test/core/i32.wast:430
assert_return(() => invoke($0, `ge_u`, [-2147483648, -2147483648]), [
  value("i32", 1),
]);

// ./test/core/i32.wast:431
assert_return(() => invoke($0, `ge_u`, [2147483647, 2147483647]), [
  value("i32", 1),
]);

// ./test/core/i32.wast:432
assert_return(() => invoke($0, `ge_u`, [-1, -1]), [value("i32", 1)]);

// ./test/core/i32.wast:433
assert_return(() => invoke($0, `ge_u`, [1, 0]), [value("i32", 1)]);

// ./test/core/i32.wast:434
assert_return(() => invoke($0, `ge_u`, [0, 1]), [value("i32", 0)]);

// ./test/core/i32.wast:435
assert_return(() => invoke($0, `ge_u`, [-2147483648, 0]), [value("i32", 1)]);

// ./test/core/i32.wast:436
assert_return(() => invoke($0, `ge_u`, [0, -2147483648]), [value("i32", 0)]);

// ./test/core/i32.wast:437
assert_return(() => invoke($0, `ge_u`, [-2147483648, -1]), [value("i32", 0)]);

// ./test/core/i32.wast:438
assert_return(() => invoke($0, `ge_u`, [-1, -2147483648]), [value("i32", 1)]);

// ./test/core/i32.wast:439
assert_return(() => invoke($0, `ge_u`, [-2147483648, 2147483647]), [
  value("i32", 1),
]);

// ./test/core/i32.wast:440
assert_return(() => invoke($0, `ge_u`, [2147483647, -2147483648]), [
  value("i32", 0),
]);

// ./test/core/i32.wast:443
assert_invalid(() =>
  instantiate(`(module
    (func $$type-unary-operand-empty
      (i32.eqz) (drop)
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:451
assert_invalid(() =>
  instantiate(`(module
    (func $$type-unary-operand-empty-in-block
      (i32.const 0)
      (block (i32.eqz) (drop))
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:460
assert_invalid(() =>
  instantiate(`(module
    (func $$type-unary-operand-empty-in-loop
      (i32.const 0)
      (loop (i32.eqz) (drop))
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:469
assert_invalid(() =>
  instantiate(`(module
    (func $$type-unary-operand-empty-in-if
      (i32.const 0) (i32.const 0)
      (if (then (i32.eqz) (drop)))
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:478
assert_invalid(() =>
  instantiate(`(module
    (func $$type-unary-operand-empty-in-else
      (i32.const 0) (i32.const 0)
      (if (result i32) (then (i32.const 0)) (else (i32.eqz))) (drop)
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:487
assert_invalid(() =>
  instantiate(`(module
    (func $$type-unary-operand-empty-in-br
      (i32.const 0)
      (block (br 0 (i32.eqz)) (drop))
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:496
assert_invalid(() =>
  instantiate(`(module
    (func $$type-unary-operand-empty-in-br_if
      (i32.const 0)
      (block (br_if 0 (i32.eqz) (i32.const 1)) (drop))
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:505
assert_invalid(() =>
  instantiate(`(module
    (func $$type-unary-operand-empty-in-br_table
      (i32.const 0)
      (block (br_table 0 (i32.eqz)) (drop))
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:514
assert_invalid(() =>
  instantiate(`(module
    (func $$type-unary-operand-empty-in-return
      (return (i32.eqz)) (drop)
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:522
assert_invalid(() =>
  instantiate(`(module
    (func $$type-unary-operand-empty-in-select
      (select (i32.eqz) (i32.const 1) (i32.const 2)) (drop)
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:530
assert_invalid(() =>
  instantiate(`(module
    (func $$type-unary-operand-empty-in-call
      (call 1 (i32.eqz)) (drop)
    )
    (func (param i32) (result i32) (local.get 0))
  )`), `type mismatch`);

// ./test/core/i32.wast:539
assert_invalid(() =>
  instantiate(`(module
    (func $$f (param i32) (result i32) (local.get 0))
    (type $$sig (func (param i32) (result i32)))
    (table funcref (elem $$f))
    (func $$type-unary-operand-empty-in-call_indirect
      (block (result i32)
        (call_indirect (type $$sig)
          (i32.eqz) (i32.const 0)
        )
        (drop)
      )
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:555
assert_invalid(() =>
  instantiate(`(module
    (func $$type-unary-operand-empty-in-local.set
      (local i32)
      (local.set 0 (i32.eqz)) (local.get 0) (drop)
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:564
assert_invalid(() =>
  instantiate(`(module
    (func $$type-unary-operand-empty-in-local.tee
      (local i32)
      (local.tee 0 (i32.eqz)) (drop)
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:573
assert_invalid(() =>
  instantiate(`(module
    (global $$x (mut i32) (i32.const 0))
    (func $$type-unary-operand-empty-in-global.set
      (global.set $$x (i32.eqz)) (global.get $$x) (drop)
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:582
assert_invalid(() =>
  instantiate(`(module
    (memory 0)
    (func $$type-unary-operand-empty-in-memory.grow
      (memory.grow (i32.eqz)) (drop)
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:591
assert_invalid(() =>
  instantiate(`(module
    (memory 0)
    (func $$type-unary-operand-empty-in-load
      (i32.load (i32.eqz)) (drop)
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:600
assert_invalid(() =>
  instantiate(`(module
    (memory 1)
    (func $$type-unary-operand-empty-in-store
      (i32.store (i32.eqz) (i32.const 1))
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:610
assert_invalid(() =>
  instantiate(`(module
    (func $$type-binary-1st-operand-empty
      (i32.add) (drop)
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:618
assert_invalid(() =>
  instantiate(`(module
    (func $$type-binary-2nd-operand-empty
      (i32.const 0) (i32.add) (drop)
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:626
assert_invalid(() =>
  instantiate(`(module
    (func $$type-binary-1st-operand-empty-in-block
      (i32.const 0) (i32.const 0)
      (block (i32.add) (drop))
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:635
assert_invalid(() =>
  instantiate(`(module
    (func $$type-binary-2nd-operand-empty-in-block
      (i32.const 0)
      (block (i32.const 0) (i32.add) (drop))
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:644
assert_invalid(() =>
  instantiate(`(module
    (func $$type-binary-1st-operand-empty-in-loop
      (i32.const 0) (i32.const 0)
      (loop (i32.add) (drop))
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:653
assert_invalid(() =>
  instantiate(`(module
    (func $$type-binary-2nd-operand-empty-in-loop
      (i32.const 0)
      (loop (i32.const 0) (i32.add) (drop))
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:662
assert_invalid(() =>
  instantiate(`(module
    (func $$type-binary-1st-operand-empty-in-if
      (i32.const 0) (i32.const 0) (i32.const 0)
      (if (i32.add) (then (drop)))
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:671
assert_invalid(() =>
  instantiate(`(module
    (func $$type-binary-2nd-operand-empty-in-if
      (i32.const 0) (i32.const 0)
      (if (i32.const 0) (then (i32.add)) (else (drop)))
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:680
assert_invalid(() =>
  instantiate(`(module
    (func $$type-binary-1st-operand-empty-in-else
      (i32.const 0) (i32.const 0) (i32.const 0)
      (if (result i32) (then (i32.const 0)) (else (i32.add) (i32.const 0)))
      (drop) (drop)
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:690
assert_invalid(() =>
  instantiate(`(module
    (func $$type-binary-2nd-operand-empty-in-else
      (i32.const 0) (i32.const 0)
      (if (result i32) (then (i32.const 0)) (else (i32.add)))
      (drop)
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:700
assert_invalid(() =>
  instantiate(`(module
    (func $$type-binary-1st-operand-empty-in-br
      (i32.const 0) (i32.const 0)
      (block (br 0 (i32.add)) (drop))
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:709
assert_invalid(() =>
  instantiate(`(module
    (func $$type-binary-2nd-operand-empty-in-br
      (i32.const 0)
      (block (br 0 (i32.const 0) (i32.add)) (drop))
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:718
assert_invalid(() =>
  instantiate(`(module
    (func $$type-binary-1st-operand-empty-in-br_if
      (i32.const 0) (i32.const 0)
      (block (br_if 0 (i32.add) (i32.const 1)) (drop))
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:727
assert_invalid(() =>
  instantiate(`(module
    (func $$type-binary-2nd-operand-empty-in-br_if
      (i32.const 0)
      (block (br_if 0 (i32.const 0) (i32.add) (i32.const 1)) (drop))
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:736
assert_invalid(() =>
  instantiate(`(module
    (func $$type-binary-1st-operand-empty-in-br_table
      (i32.const 0) (i32.const 0)
      (block (br_table 0 (i32.add)) (drop))
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:745
assert_invalid(() =>
  instantiate(`(module
    (func $$type-binary-2nd-operand-empty-in-br_table
      (i32.const 0)
      (block (br_table 0 (i32.const 0) (i32.add)) (drop))
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:754
assert_invalid(() =>
  instantiate(`(module
    (func $$type-binary-1st-operand-empty-in-return
      (return (i32.add)) (drop)
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:762
assert_invalid(() =>
  instantiate(`(module
    (func $$type-binary-2nd-operand-empty-in-return
      (return (i32.const 0) (i32.add)) (drop)
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:770
assert_invalid(() =>
  instantiate(`(module
    (func $$type-binary-1st-operand-empty-in-select
      (select (i32.add) (i32.const 1) (i32.const 2)) (drop)
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:778
assert_invalid(() =>
  instantiate(`(module
    (func $$type-binary-2nd-operand-empty-in-select
      (select (i32.const 0) (i32.add) (i32.const 1) (i32.const 2)) (drop)
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:786
assert_invalid(() =>
  instantiate(`(module
    (func $$type-binary-1st-operand-empty-in-call
      (call 1 (i32.add)) (drop)
    )
    (func (param i32 i32) (result i32) (local.get 0))
  )`), `type mismatch`);

// ./test/core/i32.wast:795
assert_invalid(() =>
  instantiate(`(module
    (func $$type-binary-2nd-operand-empty-in-call
      (call 1 (i32.const 0) (i32.add)) (drop)
    )
    (func (param i32 i32) (result i32) (local.get 0))
  )`), `type mismatch`);

// ./test/core/i32.wast:804
assert_invalid(() =>
  instantiate(`(module
    (func $$f (param i32) (result i32) (local.get 0))
    (type $$sig (func (param i32) (result i32)))
    (table funcref (elem $$f))
    (func $$type-binary-1st-operand-empty-in-call_indirect
      (block (result i32)
        (call_indirect (type $$sig)
          (i32.add) (i32.const 0)
        )
        (drop)
      )
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:820
assert_invalid(() =>
  instantiate(`(module
    (func $$f (param i32) (result i32) (local.get 0))
    (type $$sig (func (param i32) (result i32)))
    (table funcref (elem $$f))
    (func $$type-binary-2nd-operand-empty-in-call_indirect
      (block (result i32)
        (call_indirect (type $$sig)
          (i32.const 0) (i32.add) (i32.const 0)
        )
        (drop)
      )
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:836
assert_invalid(() =>
  instantiate(`(module
    (func $$type-binary-1st-operand-empty-in-local.set
      (local i32)
      (local.set 0 (i32.add)) (local.get 0) (drop)
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:845
assert_invalid(() =>
  instantiate(`(module
    (func $$type-binary-2nd-operand-empty-in-local.set
      (local i32)
      (local.set 0 (i32.const 0) (i32.add)) (local.get 0) (drop)
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:854
assert_invalid(() =>
  instantiate(`(module
    (func $$type-binary-1st-operand-empty-in-local.tee
      (local i32)
      (local.tee 0 (i32.add)) (drop)
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:863
assert_invalid(() =>
  instantiate(`(module
    (func $$type-binary-2nd-operand-empty-in-local.tee
      (local i32)
      (local.tee 0 (i32.const 0) (i32.add)) (drop)
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:872
assert_invalid(() =>
  instantiate(`(module
    (global $$x (mut i32) (i32.const 0))
    (func $$type-binary-1st-operand-empty-in-global.set
      (global.set $$x (i32.add)) (global.get $$x) (drop)
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:881
assert_invalid(() =>
  instantiate(`(module
    (global $$x (mut i32) (i32.const 0))
    (func $$type-binary-2nd-operand-empty-in-global.set
      (global.set $$x (i32.const 0) (i32.add)) (global.get $$x) (drop)
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:890
assert_invalid(() =>
  instantiate(`(module
    (memory 0)
    (func $$type-binary-1st-operand-empty-in-memory.grow
      (memory.grow (i32.add)) (drop)
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:899
assert_invalid(() =>
  instantiate(`(module
    (memory 0)
    (func $$type-binary-2nd-operand-empty-in-memory.grow
      (memory.grow (i32.const 0) (i32.add)) (drop)
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:908
assert_invalid(() =>
  instantiate(`(module
    (memory 0)
    (func $$type-binary-1st-operand-empty-in-load
      (i32.load (i32.add)) (drop)
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:917
assert_invalid(() =>
  instantiate(`(module
    (memory 0)
    (func $$type-binary-2nd-operand-empty-in-load
      (i32.load (i32.const 0) (i32.add)) (drop)
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:926
assert_invalid(() =>
  instantiate(`(module
    (memory 1)
    (func $$type-binary-1st-operand-empty-in-store
      (i32.store (i32.add) (i32.const 1))
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:935
assert_invalid(() =>
  instantiate(`(module
    (memory 1)
    (func $$type-binary-2nd-operand-empty-in-store
      (i32.store (i32.const 1) (i32.add) (i32.const 0))
    )
  )`), `type mismatch`);

// ./test/core/i32.wast:948
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i32) (i32.add (i64.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i32.wast:949
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i32) (i32.and (i64.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i32.wast:950
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i32) (i32.div_s (i64.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i32.wast:951
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i32) (i32.div_u (i64.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i32.wast:952
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i32) (i32.mul (i64.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i32.wast:953
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i32) (i32.or (i64.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i32.wast:954
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i32) (i32.rem_s (i64.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i32.wast:955
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i32) (i32.rem_u (i64.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i32.wast:956
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i32) (i32.rotl (i64.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i32.wast:957
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i32) (i32.rotr (i64.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i32.wast:958
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i32) (i32.shl (i64.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i32.wast:959
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i32) (i32.shr_s (i64.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i32.wast:960
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i32) (i32.shr_u (i64.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i32.wast:961
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i32) (i32.sub (i64.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i32.wast:962
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i32) (i32.xor (i64.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i32.wast:963
assert_invalid(
  () => instantiate(`(module (func (result i32) (i32.eqz (i64.const 0))))`),
  `type mismatch`,
);

// ./test/core/i32.wast:964
assert_invalid(
  () => instantiate(`(module (func (result i32) (i32.clz (i64.const 0))))`),
  `type mismatch`,
);

// ./test/core/i32.wast:965
assert_invalid(
  () => instantiate(`(module (func (result i32) (i32.ctz (i64.const 0))))`),
  `type mismatch`,
);

// ./test/core/i32.wast:966
assert_invalid(
  () => instantiate(`(module (func (result i32) (i32.popcnt (i64.const 0))))`),
  `type mismatch`,
);

// ./test/core/i32.wast:967
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i32) (i32.eq (i64.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i32.wast:968
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i32) (i32.ge_s (i64.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i32.wast:969
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i32) (i32.ge_u (i64.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i32.wast:970
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i32) (i32.gt_s (i64.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i32.wast:971
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i32) (i32.gt_u (i64.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i32.wast:972
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i32) (i32.le_s (i64.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i32.wast:973
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i32) (i32.le_u (i64.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i32.wast:974
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i32) (i32.lt_s (i64.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i32.wast:975
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i32) (i32.lt_u (i64.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/i32.wast:976
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i32) (i32.ne (i64.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);
