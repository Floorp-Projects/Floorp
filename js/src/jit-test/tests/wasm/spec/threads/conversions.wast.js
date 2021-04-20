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

// ./test/core/conversions.wast

// ./test/core/conversions.wast:1
let $0 = instantiate(`(module
  (func (export "i64.extend_i32_s") (param $$x i32) (result i64) (i64.extend_i32_s (local.get $$x)))
  (func (export "i64.extend_i32_u") (param $$x i32) (result i64) (i64.extend_i32_u (local.get $$x)))
  (func (export "i32.wrap_i64") (param $$x i64) (result i32) (i32.wrap_i64 (local.get $$x)))
  (func (export "i32.trunc_f32_s") (param $$x f32) (result i32) (i32.trunc_f32_s (local.get $$x)))
  (func (export "i32.trunc_f32_u") (param $$x f32) (result i32) (i32.trunc_f32_u (local.get $$x)))
  (func (export "i32.trunc_f64_s") (param $$x f64) (result i32) (i32.trunc_f64_s (local.get $$x)))
  (func (export "i32.trunc_f64_u") (param $$x f64) (result i32) (i32.trunc_f64_u (local.get $$x)))
  (func (export "i64.trunc_f32_s") (param $$x f32) (result i64) (i64.trunc_f32_s (local.get $$x)))
  (func (export "i64.trunc_f32_u") (param $$x f32) (result i64) (i64.trunc_f32_u (local.get $$x)))
  (func (export "i64.trunc_f64_s") (param $$x f64) (result i64) (i64.trunc_f64_s (local.get $$x)))
  (func (export "i64.trunc_f64_u") (param $$x f64) (result i64) (i64.trunc_f64_u (local.get $$x)))
  (func (export "f32.convert_i32_s") (param $$x i32) (result f32) (f32.convert_i32_s (local.get $$x)))
  (func (export "f32.convert_i64_s") (param $$x i64) (result f32) (f32.convert_i64_s (local.get $$x)))
  (func (export "f64.convert_i32_s") (param $$x i32) (result f64) (f64.convert_i32_s (local.get $$x)))
  (func (export "f64.convert_i64_s") (param $$x i64) (result f64) (f64.convert_i64_s (local.get $$x)))
  (func (export "f32.convert_i32_u") (param $$x i32) (result f32) (f32.convert_i32_u (local.get $$x)))
  (func (export "f32.convert_i64_u") (param $$x i64) (result f32) (f32.convert_i64_u (local.get $$x)))
  (func (export "f64.convert_i32_u") (param $$x i32) (result f64) (f64.convert_i32_u (local.get $$x)))
  (func (export "f64.convert_i64_u") (param $$x i64) (result f64) (f64.convert_i64_u (local.get $$x)))
  (func (export "f64.promote_f32") (param $$x f32) (result f64) (f64.promote_f32 (local.get $$x)))
  (func (export "f32.demote_f64") (param $$x f64) (result f32) (f32.demote_f64 (local.get $$x)))
  (func (export "f32.reinterpret_i32") (param $$x i32) (result f32) (f32.reinterpret_i32 (local.get $$x)))
  (func (export "f64.reinterpret_i64") (param $$x i64) (result f64) (f64.reinterpret_i64 (local.get $$x)))
  (func (export "i32.reinterpret_f32") (param $$x f32) (result i32) (i32.reinterpret_f32 (local.get $$x)))
  (func (export "i64.reinterpret_f64") (param $$x f64) (result i64) (i64.reinterpret_f64 (local.get $$x)))
)`);

// ./test/core/conversions.wast:29
assert_return(() => invoke($0, `i64.extend_i32_s`, [0]), [value('i64', 0n)]);

// ./test/core/conversions.wast:30
assert_return(() => invoke($0, `i64.extend_i32_s`, [10000]), [value('i64', 10000n)]);

// ./test/core/conversions.wast:31
assert_return(() => invoke($0, `i64.extend_i32_s`, [-10000]), [value('i64', -10000n)]);

// ./test/core/conversions.wast:32
assert_return(() => invoke($0, `i64.extend_i32_s`, [-1]), [value('i64', -1n)]);

// ./test/core/conversions.wast:33
assert_return(() => invoke($0, `i64.extend_i32_s`, [2147483647]), [value('i64', 2147483647n)]);

// ./test/core/conversions.wast:34
assert_return(() => invoke($0, `i64.extend_i32_s`, [-2147483648]), [value('i64', -2147483648n)]);

// ./test/core/conversions.wast:36
assert_return(() => invoke($0, `i64.extend_i32_u`, [0]), [value('i64', 0n)]);

// ./test/core/conversions.wast:37
assert_return(() => invoke($0, `i64.extend_i32_u`, [10000]), [value('i64', 10000n)]);

// ./test/core/conversions.wast:38
assert_return(() => invoke($0, `i64.extend_i32_u`, [-10000]), [value('i64', 4294957296n)]);

// ./test/core/conversions.wast:39
assert_return(() => invoke($0, `i64.extend_i32_u`, [-1]), [value('i64', 4294967295n)]);

// ./test/core/conversions.wast:40
assert_return(() => invoke($0, `i64.extend_i32_u`, [2147483647]), [value('i64', 2147483647n)]);

// ./test/core/conversions.wast:41
assert_return(() => invoke($0, `i64.extend_i32_u`, [-2147483648]), [value('i64', 2147483648n)]);

// ./test/core/conversions.wast:43
assert_return(() => invoke($0, `i32.wrap_i64`, [-1n]), [value('i32', -1)]);

// ./test/core/conversions.wast:44
assert_return(() => invoke($0, `i32.wrap_i64`, [-100000n]), [value('i32', -100000)]);

// ./test/core/conversions.wast:45
assert_return(() => invoke($0, `i32.wrap_i64`, [2147483648n]), [value('i32', -2147483648)]);

// ./test/core/conversions.wast:46
assert_return(() => invoke($0, `i32.wrap_i64`, [-2147483649n]), [value('i32', 2147483647)]);

// ./test/core/conversions.wast:47
assert_return(() => invoke($0, `i32.wrap_i64`, [-4294967296n]), [value('i32', 0)]);

// ./test/core/conversions.wast:48
assert_return(() => invoke($0, `i32.wrap_i64`, [-4294967297n]), [value('i32', -1)]);

// ./test/core/conversions.wast:49
assert_return(() => invoke($0, `i32.wrap_i64`, [-4294967295n]), [value('i32', 1)]);

// ./test/core/conversions.wast:50
assert_return(() => invoke($0, `i32.wrap_i64`, [0n]), [value('i32', 0)]);

// ./test/core/conversions.wast:51
assert_return(() => invoke($0, `i32.wrap_i64`, [1311768467463790320n]), [value('i32', -1698898192)]);

// ./test/core/conversions.wast:52
assert_return(() => invoke($0, `i32.wrap_i64`, [4294967295n]), [value('i32', -1)]);

// ./test/core/conversions.wast:53
assert_return(() => invoke($0, `i32.wrap_i64`, [4294967296n]), [value('i32', 0)]);

// ./test/core/conversions.wast:54
assert_return(() => invoke($0, `i32.wrap_i64`, [4294967297n]), [value('i32', 1)]);

// ./test/core/conversions.wast:56
assert_return(() => invoke($0, `i32.trunc_f32_s`, [value('f32', 0)]), [value('i32', 0)]);

// ./test/core/conversions.wast:57
assert_return(() => invoke($0, `i32.trunc_f32_s`, [bytes('f32', [0x0, 0x0, 0x0, 0x80])]), [value('i32', 0)]);

// ./test/core/conversions.wast:58
assert_return(() => invoke($0, `i32.trunc_f32_s`, [value('f32', 0.000000000000000000000000000000000000000000001)]), [value('i32', 0)]);

// ./test/core/conversions.wast:59
assert_return(() => invoke($0, `i32.trunc_f32_s`, [value('f32', -0.000000000000000000000000000000000000000000001)]), [value('i32', 0)]);

// ./test/core/conversions.wast:60
assert_return(() => invoke($0, `i32.trunc_f32_s`, [value('f32', 1)]), [value('i32', 1)]);

// ./test/core/conversions.wast:61
assert_return(() => invoke($0, `i32.trunc_f32_s`, [value('f32', 1.1)]), [value('i32', 1)]);

// ./test/core/conversions.wast:62
assert_return(() => invoke($0, `i32.trunc_f32_s`, [value('f32', 1.5)]), [value('i32', 1)]);

// ./test/core/conversions.wast:63
assert_return(() => invoke($0, `i32.trunc_f32_s`, [value('f32', -1)]), [value('i32', -1)]);

// ./test/core/conversions.wast:64
assert_return(() => invoke($0, `i32.trunc_f32_s`, [value('f32', -1.1)]), [value('i32', -1)]);

// ./test/core/conversions.wast:65
assert_return(() => invoke($0, `i32.trunc_f32_s`, [value('f32', -1.5)]), [value('i32', -1)]);

// ./test/core/conversions.wast:66
assert_return(() => invoke($0, `i32.trunc_f32_s`, [value('f32', -1.9)]), [value('i32', -1)]);

// ./test/core/conversions.wast:67
assert_return(() => invoke($0, `i32.trunc_f32_s`, [value('f32', -2)]), [value('i32', -2)]);

// ./test/core/conversions.wast:68
assert_return(() => invoke($0, `i32.trunc_f32_s`, [value('f32', 2147483500)]), [value('i32', 2147483520)]);

// ./test/core/conversions.wast:69
assert_return(() => invoke($0, `i32.trunc_f32_s`, [value('f32', -2147483600)]), [value('i32', -2147483648)]);

// ./test/core/conversions.wast:70
assert_trap(() => invoke($0, `i32.trunc_f32_s`, [value('f32', 2147483600)]), `integer overflow`);

// ./test/core/conversions.wast:71
assert_trap(() => invoke($0, `i32.trunc_f32_s`, [value('f32', -2147484000)]), `integer overflow`);

// ./test/core/conversions.wast:72
assert_trap(() => invoke($0, `i32.trunc_f32_s`, [value('f32', Infinity)]), `integer overflow`);

// ./test/core/conversions.wast:73
assert_trap(() => invoke($0, `i32.trunc_f32_s`, [value('f32', -Infinity)]), `integer overflow`);

// ./test/core/conversions.wast:74
assert_trap(() => invoke($0, `i32.trunc_f32_s`, [bytes('f32', [0x0, 0x0, 0xc0, 0x7f])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:75
assert_trap(() => invoke($0, `i32.trunc_f32_s`, [bytes('f32', [0x0, 0x0, 0xa0, 0x7f])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:76
assert_trap(() => invoke($0, `i32.trunc_f32_s`, [bytes('f32', [0x0, 0x0, 0xc0, 0xff])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:77
assert_trap(() => invoke($0, `i32.trunc_f32_s`, [bytes('f32', [0x0, 0x0, 0xa0, 0xff])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:79
assert_return(() => invoke($0, `i32.trunc_f32_u`, [value('f32', 0)]), [value('i32', 0)]);

// ./test/core/conversions.wast:80
assert_return(() => invoke($0, `i32.trunc_f32_u`, [bytes('f32', [0x0, 0x0, 0x0, 0x80])]), [value('i32', 0)]);

// ./test/core/conversions.wast:81
assert_return(() => invoke($0, `i32.trunc_f32_u`, [value('f32', 0.000000000000000000000000000000000000000000001)]), [value('i32', 0)]);

// ./test/core/conversions.wast:82
assert_return(() => invoke($0, `i32.trunc_f32_u`, [value('f32', -0.000000000000000000000000000000000000000000001)]), [value('i32', 0)]);

// ./test/core/conversions.wast:83
assert_return(() => invoke($0, `i32.trunc_f32_u`, [value('f32', 1)]), [value('i32', 1)]);

// ./test/core/conversions.wast:84
assert_return(() => invoke($0, `i32.trunc_f32_u`, [value('f32', 1.1)]), [value('i32', 1)]);

// ./test/core/conversions.wast:85
assert_return(() => invoke($0, `i32.trunc_f32_u`, [value('f32', 1.5)]), [value('i32', 1)]);

// ./test/core/conversions.wast:86
assert_return(() => invoke($0, `i32.trunc_f32_u`, [value('f32', 1.9)]), [value('i32', 1)]);

// ./test/core/conversions.wast:87
assert_return(() => invoke($0, `i32.trunc_f32_u`, [value('f32', 2)]), [value('i32', 2)]);

// ./test/core/conversions.wast:88
assert_return(() => invoke($0, `i32.trunc_f32_u`, [value('f32', 2147483600)]), [value('i32', -2147483648)]);

// ./test/core/conversions.wast:89
assert_return(() => invoke($0, `i32.trunc_f32_u`, [value('f32', 4294967000)]), [value('i32', -256)]);

// ./test/core/conversions.wast:90
assert_return(() => invoke($0, `i32.trunc_f32_u`, [value('f32', -0.9)]), [value('i32', 0)]);

// ./test/core/conversions.wast:91
assert_return(() => invoke($0, `i32.trunc_f32_u`, [value('f32', -0.99999994)]), [value('i32', 0)]);

// ./test/core/conversions.wast:92
assert_trap(() => invoke($0, `i32.trunc_f32_u`, [value('f32', 4294967300)]), `integer overflow`);

// ./test/core/conversions.wast:93
assert_trap(() => invoke($0, `i32.trunc_f32_u`, [value('f32', -1)]), `integer overflow`);

// ./test/core/conversions.wast:94
assert_trap(() => invoke($0, `i32.trunc_f32_u`, [value('f32', Infinity)]), `integer overflow`);

// ./test/core/conversions.wast:95
assert_trap(() => invoke($0, `i32.trunc_f32_u`, [value('f32', -Infinity)]), `integer overflow`);

// ./test/core/conversions.wast:96
assert_trap(() => invoke($0, `i32.trunc_f32_u`, [bytes('f32', [0x0, 0x0, 0xc0, 0x7f])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:97
assert_trap(() => invoke($0, `i32.trunc_f32_u`, [bytes('f32', [0x0, 0x0, 0xa0, 0x7f])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:98
assert_trap(() => invoke($0, `i32.trunc_f32_u`, [bytes('f32', [0x0, 0x0, 0xc0, 0xff])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:99
assert_trap(() => invoke($0, `i32.trunc_f32_u`, [bytes('f32', [0x0, 0x0, 0xa0, 0xff])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:101
assert_return(() => invoke($0, `i32.trunc_f64_s`, [value('f64', 0)]), [value('i32', 0)]);

// ./test/core/conversions.wast:102
assert_return(() => invoke($0, `i32.trunc_f64_s`, [value('f64', -0)]), [value('i32', 0)]);

// ./test/core/conversions.wast:103
assert_return(() => invoke($0, `i32.trunc_f64_s`, [value('f64', 0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005)]), [value('i32', 0)]);

// ./test/core/conversions.wast:104
assert_return(() => invoke($0, `i32.trunc_f64_s`, [value('f64', -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005)]), [value('i32', 0)]);

// ./test/core/conversions.wast:105
assert_return(() => invoke($0, `i32.trunc_f64_s`, [value('f64', 1)]), [value('i32', 1)]);

// ./test/core/conversions.wast:106
assert_return(() => invoke($0, `i32.trunc_f64_s`, [value('f64', 1.1)]), [value('i32', 1)]);

// ./test/core/conversions.wast:107
assert_return(() => invoke($0, `i32.trunc_f64_s`, [value('f64', 1.5)]), [value('i32', 1)]);

// ./test/core/conversions.wast:108
assert_return(() => invoke($0, `i32.trunc_f64_s`, [value('f64', -1)]), [value('i32', -1)]);

// ./test/core/conversions.wast:109
assert_return(() => invoke($0, `i32.trunc_f64_s`, [value('f64', -1.1)]), [value('i32', -1)]);

// ./test/core/conversions.wast:110
assert_return(() => invoke($0, `i32.trunc_f64_s`, [value('f64', -1.5)]), [value('i32', -1)]);

// ./test/core/conversions.wast:111
assert_return(() => invoke($0, `i32.trunc_f64_s`, [value('f64', -1.9)]), [value('i32', -1)]);

// ./test/core/conversions.wast:112
assert_return(() => invoke($0, `i32.trunc_f64_s`, [value('f64', -2)]), [value('i32', -2)]);

// ./test/core/conversions.wast:113
assert_return(() => invoke($0, `i32.trunc_f64_s`, [value('f64', 2147483647)]), [value('i32', 2147483647)]);

// ./test/core/conversions.wast:114
assert_return(() => invoke($0, `i32.trunc_f64_s`, [value('f64', -2147483648)]), [value('i32', -2147483648)]);

// ./test/core/conversions.wast:115
assert_trap(() => invoke($0, `i32.trunc_f64_s`, [value('f64', 2147483648)]), `integer overflow`);

// ./test/core/conversions.wast:116
assert_trap(() => invoke($0, `i32.trunc_f64_s`, [value('f64', -2147483649)]), `integer overflow`);

// ./test/core/conversions.wast:117
assert_trap(() => invoke($0, `i32.trunc_f64_s`, [value('f64', Infinity)]), `integer overflow`);

// ./test/core/conversions.wast:118
assert_trap(() => invoke($0, `i32.trunc_f64_s`, [value('f64', -Infinity)]), `integer overflow`);

// ./test/core/conversions.wast:119
assert_trap(() => invoke($0, `i32.trunc_f64_s`, [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:120
assert_trap(() => invoke($0, `i32.trunc_f64_s`, [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:121
assert_trap(() => invoke($0, `i32.trunc_f64_s`, [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:122
assert_trap(() => invoke($0, `i32.trunc_f64_s`, [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0xff])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:124
assert_return(() => invoke($0, `i32.trunc_f64_u`, [value('f64', 0)]), [value('i32', 0)]);

// ./test/core/conversions.wast:125
assert_return(() => invoke($0, `i32.trunc_f64_u`, [value('f64', -0)]), [value('i32', 0)]);

// ./test/core/conversions.wast:126
assert_return(() => invoke($0, `i32.trunc_f64_u`, [value('f64', 0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005)]), [value('i32', 0)]);

// ./test/core/conversions.wast:127
assert_return(() => invoke($0, `i32.trunc_f64_u`, [value('f64', -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005)]), [value('i32', 0)]);

// ./test/core/conversions.wast:128
assert_return(() => invoke($0, `i32.trunc_f64_u`, [value('f64', 1)]), [value('i32', 1)]);

// ./test/core/conversions.wast:129
assert_return(() => invoke($0, `i32.trunc_f64_u`, [value('f64', 1.1)]), [value('i32', 1)]);

// ./test/core/conversions.wast:130
assert_return(() => invoke($0, `i32.trunc_f64_u`, [value('f64', 1.5)]), [value('i32', 1)]);

// ./test/core/conversions.wast:131
assert_return(() => invoke($0, `i32.trunc_f64_u`, [value('f64', 1.9)]), [value('i32', 1)]);

// ./test/core/conversions.wast:132
assert_return(() => invoke($0, `i32.trunc_f64_u`, [value('f64', 2)]), [value('i32', 2)]);

// ./test/core/conversions.wast:133
assert_return(() => invoke($0, `i32.trunc_f64_u`, [value('f64', 2147483648)]), [value('i32', -2147483648)]);

// ./test/core/conversions.wast:134
assert_return(() => invoke($0, `i32.trunc_f64_u`, [value('f64', 4294967295)]), [value('i32', -1)]);

// ./test/core/conversions.wast:135
assert_return(() => invoke($0, `i32.trunc_f64_u`, [value('f64', -0.9)]), [value('i32', 0)]);

// ./test/core/conversions.wast:136
assert_return(() => invoke($0, `i32.trunc_f64_u`, [value('f64', -0.9999999999999999)]), [value('i32', 0)]);

// ./test/core/conversions.wast:137
assert_return(() => invoke($0, `i32.trunc_f64_u`, [value('f64', 100000000)]), [value('i32', 100000000)]);

// ./test/core/conversions.wast:138
assert_trap(() => invoke($0, `i32.trunc_f64_u`, [value('f64', 4294967296)]), `integer overflow`);

// ./test/core/conversions.wast:139
assert_trap(() => invoke($0, `i32.trunc_f64_u`, [value('f64', -1)]), `integer overflow`);

// ./test/core/conversions.wast:140
assert_trap(() => invoke($0, `i32.trunc_f64_u`, [value('f64', 10000000000000000)]), `integer overflow`);

// ./test/core/conversions.wast:141
assert_trap(() => invoke($0, `i32.trunc_f64_u`, [value('f64', 1000000000000000000000000000000)]), `integer overflow`);

// ./test/core/conversions.wast:142
assert_trap(() => invoke($0, `i32.trunc_f64_u`, [value('f64', 9223372036854776000)]), `integer overflow`);

// ./test/core/conversions.wast:143
assert_trap(() => invoke($0, `i32.trunc_f64_u`, [value('f64', Infinity)]), `integer overflow`);

// ./test/core/conversions.wast:144
assert_trap(() => invoke($0, `i32.trunc_f64_u`, [value('f64', -Infinity)]), `integer overflow`);

// ./test/core/conversions.wast:145
assert_trap(() => invoke($0, `i32.trunc_f64_u`, [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:146
assert_trap(() => invoke($0, `i32.trunc_f64_u`, [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:147
assert_trap(() => invoke($0, `i32.trunc_f64_u`, [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:148
assert_trap(() => invoke($0, `i32.trunc_f64_u`, [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0xff])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:150
assert_return(() => invoke($0, `i64.trunc_f32_s`, [value('f32', 0)]), [value('i64', 0n)]);

// ./test/core/conversions.wast:151
assert_return(() => invoke($0, `i64.trunc_f32_s`, [bytes('f32', [0x0, 0x0, 0x0, 0x80])]), [value('i64', 0n)]);

// ./test/core/conversions.wast:152
assert_return(() => invoke($0, `i64.trunc_f32_s`, [value('f32', 0.000000000000000000000000000000000000000000001)]), [value('i64', 0n)]);

// ./test/core/conversions.wast:153
assert_return(() => invoke($0, `i64.trunc_f32_s`, [value('f32', -0.000000000000000000000000000000000000000000001)]), [value('i64', 0n)]);

// ./test/core/conversions.wast:154
assert_return(() => invoke($0, `i64.trunc_f32_s`, [value('f32', 1)]), [value('i64', 1n)]);

// ./test/core/conversions.wast:155
assert_return(() => invoke($0, `i64.trunc_f32_s`, [value('f32', 1.1)]), [value('i64', 1n)]);

// ./test/core/conversions.wast:156
assert_return(() => invoke($0, `i64.trunc_f32_s`, [value('f32', 1.5)]), [value('i64', 1n)]);

// ./test/core/conversions.wast:157
assert_return(() => invoke($0, `i64.trunc_f32_s`, [value('f32', -1)]), [value('i64', -1n)]);

// ./test/core/conversions.wast:158
assert_return(() => invoke($0, `i64.trunc_f32_s`, [value('f32', -1.1)]), [value('i64', -1n)]);

// ./test/core/conversions.wast:159
assert_return(() => invoke($0, `i64.trunc_f32_s`, [value('f32', -1.5)]), [value('i64', -1n)]);

// ./test/core/conversions.wast:160
assert_return(() => invoke($0, `i64.trunc_f32_s`, [value('f32', -1.9)]), [value('i64', -1n)]);

// ./test/core/conversions.wast:161
assert_return(() => invoke($0, `i64.trunc_f32_s`, [value('f32', -2)]), [value('i64', -2n)]);

// ./test/core/conversions.wast:162
assert_return(() => invoke($0, `i64.trunc_f32_s`, [value('f32', 4294967300)]), [value('i64', 4294967296n)]);

// ./test/core/conversions.wast:163
assert_return(() => invoke($0, `i64.trunc_f32_s`, [value('f32', -4294967300)]), [value('i64', -4294967296n)]);

// ./test/core/conversions.wast:164
assert_return(() => invoke($0, `i64.trunc_f32_s`, [value('f32', 9223371500000000000)]), [value('i64', 9223371487098961920n)]);

// ./test/core/conversions.wast:165
assert_return(() => invoke($0, `i64.trunc_f32_s`, [value('f32', -9223372000000000000)]), [value('i64', -9223372036854775808n)]);

// ./test/core/conversions.wast:166
assert_trap(() => invoke($0, `i64.trunc_f32_s`, [value('f32', 9223372000000000000)]), `integer overflow`);

// ./test/core/conversions.wast:167
assert_trap(() => invoke($0, `i64.trunc_f32_s`, [value('f32', -9223373000000000000)]), `integer overflow`);

// ./test/core/conversions.wast:168
assert_trap(() => invoke($0, `i64.trunc_f32_s`, [value('f32', Infinity)]), `integer overflow`);

// ./test/core/conversions.wast:169
assert_trap(() => invoke($0, `i64.trunc_f32_s`, [value('f32', -Infinity)]), `integer overflow`);

// ./test/core/conversions.wast:170
assert_trap(() => invoke($0, `i64.trunc_f32_s`, [bytes('f32', [0x0, 0x0, 0xc0, 0x7f])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:171
assert_trap(() => invoke($0, `i64.trunc_f32_s`, [bytes('f32', [0x0, 0x0, 0xa0, 0x7f])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:172
assert_trap(() => invoke($0, `i64.trunc_f32_s`, [bytes('f32', [0x0, 0x0, 0xc0, 0xff])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:173
assert_trap(() => invoke($0, `i64.trunc_f32_s`, [bytes('f32', [0x0, 0x0, 0xa0, 0xff])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:175
assert_return(() => invoke($0, `i64.trunc_f32_u`, [value('f32', 0)]), [value('i64', 0n)]);

// ./test/core/conversions.wast:176
assert_return(() => invoke($0, `i64.trunc_f32_u`, [bytes('f32', [0x0, 0x0, 0x0, 0x80])]), [value('i64', 0n)]);

// ./test/core/conversions.wast:177
assert_return(() => invoke($0, `i64.trunc_f32_u`, [value('f32', 0.000000000000000000000000000000000000000000001)]), [value('i64', 0n)]);

// ./test/core/conversions.wast:178
assert_return(() => invoke($0, `i64.trunc_f32_u`, [value('f32', -0.000000000000000000000000000000000000000000001)]), [value('i64', 0n)]);

// ./test/core/conversions.wast:179
assert_return(() => invoke($0, `i64.trunc_f32_u`, [value('f32', 1)]), [value('i64', 1n)]);

// ./test/core/conversions.wast:180
assert_return(() => invoke($0, `i64.trunc_f32_u`, [value('f32', 1.1)]), [value('i64', 1n)]);

// ./test/core/conversions.wast:181
assert_return(() => invoke($0, `i64.trunc_f32_u`, [value('f32', 1.5)]), [value('i64', 1n)]);

// ./test/core/conversions.wast:182
assert_return(() => invoke($0, `i64.trunc_f32_u`, [value('f32', 4294967300)]), [value('i64', 4294967296n)]);

// ./test/core/conversions.wast:183
assert_return(() => invoke($0, `i64.trunc_f32_u`, [value('f32', 18446743000000000000)]), [value('i64', -1099511627776n)]);

// ./test/core/conversions.wast:184
assert_return(() => invoke($0, `i64.trunc_f32_u`, [value('f32', -0.9)]), [value('i64', 0n)]);

// ./test/core/conversions.wast:185
assert_return(() => invoke($0, `i64.trunc_f32_u`, [value('f32', -0.99999994)]), [value('i64', 0n)]);

// ./test/core/conversions.wast:186
assert_trap(() => invoke($0, `i64.trunc_f32_u`, [value('f32', 18446744000000000000)]), `integer overflow`);

// ./test/core/conversions.wast:187
assert_trap(() => invoke($0, `i64.trunc_f32_u`, [value('f32', -1)]), `integer overflow`);

// ./test/core/conversions.wast:188
assert_trap(() => invoke($0, `i64.trunc_f32_u`, [value('f32', Infinity)]), `integer overflow`);

// ./test/core/conversions.wast:189
assert_trap(() => invoke($0, `i64.trunc_f32_u`, [value('f32', -Infinity)]), `integer overflow`);

// ./test/core/conversions.wast:190
assert_trap(() => invoke($0, `i64.trunc_f32_u`, [bytes('f32', [0x0, 0x0, 0xc0, 0x7f])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:191
assert_trap(() => invoke($0, `i64.trunc_f32_u`, [bytes('f32', [0x0, 0x0, 0xa0, 0x7f])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:192
assert_trap(() => invoke($0, `i64.trunc_f32_u`, [bytes('f32', [0x0, 0x0, 0xc0, 0xff])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:193
assert_trap(() => invoke($0, `i64.trunc_f32_u`, [bytes('f32', [0x0, 0x0, 0xa0, 0xff])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:195
assert_return(() => invoke($0, `i64.trunc_f64_s`, [value('f64', 0)]), [value('i64', 0n)]);

// ./test/core/conversions.wast:196
assert_return(() => invoke($0, `i64.trunc_f64_s`, [value('f64', -0)]), [value('i64', 0n)]);

// ./test/core/conversions.wast:197
assert_return(() => invoke($0, `i64.trunc_f64_s`, [value('f64', 0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005)]), [value('i64', 0n)]);

// ./test/core/conversions.wast:198
assert_return(() => invoke($0, `i64.trunc_f64_s`, [value('f64', -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005)]), [value('i64', 0n)]);

// ./test/core/conversions.wast:199
assert_return(() => invoke($0, `i64.trunc_f64_s`, [value('f64', 1)]), [value('i64', 1n)]);

// ./test/core/conversions.wast:200
assert_return(() => invoke($0, `i64.trunc_f64_s`, [value('f64', 1.1)]), [value('i64', 1n)]);

// ./test/core/conversions.wast:201
assert_return(() => invoke($0, `i64.trunc_f64_s`, [value('f64', 1.5)]), [value('i64', 1n)]);

// ./test/core/conversions.wast:202
assert_return(() => invoke($0, `i64.trunc_f64_s`, [value('f64', -1)]), [value('i64', -1n)]);

// ./test/core/conversions.wast:203
assert_return(() => invoke($0, `i64.trunc_f64_s`, [value('f64', -1.1)]), [value('i64', -1n)]);

// ./test/core/conversions.wast:204
assert_return(() => invoke($0, `i64.trunc_f64_s`, [value('f64', -1.5)]), [value('i64', -1n)]);

// ./test/core/conversions.wast:205
assert_return(() => invoke($0, `i64.trunc_f64_s`, [value('f64', -1.9)]), [value('i64', -1n)]);

// ./test/core/conversions.wast:206
assert_return(() => invoke($0, `i64.trunc_f64_s`, [value('f64', -2)]), [value('i64', -2n)]);

// ./test/core/conversions.wast:207
assert_return(() => invoke($0, `i64.trunc_f64_s`, [value('f64', 4294967296)]), [value('i64', 4294967296n)]);

// ./test/core/conversions.wast:208
assert_return(() => invoke($0, `i64.trunc_f64_s`, [value('f64', -4294967296)]), [value('i64', -4294967296n)]);

// ./test/core/conversions.wast:209
assert_return(() => invoke($0, `i64.trunc_f64_s`, [value('f64', 9223372036854775000)]), [value('i64', 9223372036854774784n)]);

// ./test/core/conversions.wast:210
assert_return(() => invoke($0, `i64.trunc_f64_s`, [value('f64', -9223372036854776000)]), [value('i64', -9223372036854775808n)]);

// ./test/core/conversions.wast:211
assert_trap(() => invoke($0, `i64.trunc_f64_s`, [value('f64', 9223372036854776000)]), `integer overflow`);

// ./test/core/conversions.wast:212
assert_trap(() => invoke($0, `i64.trunc_f64_s`, [value('f64', -9223372036854778000)]), `integer overflow`);

// ./test/core/conversions.wast:213
assert_trap(() => invoke($0, `i64.trunc_f64_s`, [value('f64', Infinity)]), `integer overflow`);

// ./test/core/conversions.wast:214
assert_trap(() => invoke($0, `i64.trunc_f64_s`, [value('f64', -Infinity)]), `integer overflow`);

// ./test/core/conversions.wast:215
assert_trap(() => invoke($0, `i64.trunc_f64_s`, [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:216
assert_trap(() => invoke($0, `i64.trunc_f64_s`, [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:217
assert_trap(() => invoke($0, `i64.trunc_f64_s`, [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:218
assert_trap(() => invoke($0, `i64.trunc_f64_s`, [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0xff])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:220
assert_return(() => invoke($0, `i64.trunc_f64_u`, [value('f64', 0)]), [value('i64', 0n)]);

// ./test/core/conversions.wast:221
assert_return(() => invoke($0, `i64.trunc_f64_u`, [value('f64', -0)]), [value('i64', 0n)]);

// ./test/core/conversions.wast:222
assert_return(() => invoke($0, `i64.trunc_f64_u`, [value('f64', 0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005)]), [value('i64', 0n)]);

// ./test/core/conversions.wast:223
assert_return(() => invoke($0, `i64.trunc_f64_u`, [value('f64', -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005)]), [value('i64', 0n)]);

// ./test/core/conversions.wast:224
assert_return(() => invoke($0, `i64.trunc_f64_u`, [value('f64', 1)]), [value('i64', 1n)]);

// ./test/core/conversions.wast:225
assert_return(() => invoke($0, `i64.trunc_f64_u`, [value('f64', 1.1)]), [value('i64', 1n)]);

// ./test/core/conversions.wast:226
assert_return(() => invoke($0, `i64.trunc_f64_u`, [value('f64', 1.5)]), [value('i64', 1n)]);

// ./test/core/conversions.wast:227
assert_return(() => invoke($0, `i64.trunc_f64_u`, [value('f64', 4294967295)]), [value('i64', 4294967295n)]);

// ./test/core/conversions.wast:228
assert_return(() => invoke($0, `i64.trunc_f64_u`, [value('f64', 4294967296)]), [value('i64', 4294967296n)]);

// ./test/core/conversions.wast:229
assert_return(() => invoke($0, `i64.trunc_f64_u`, [value('f64', 18446744073709550000)]), [value('i64', -2048n)]);

// ./test/core/conversions.wast:230
assert_return(() => invoke($0, `i64.trunc_f64_u`, [value('f64', -0.9)]), [value('i64', 0n)]);

// ./test/core/conversions.wast:231
assert_return(() => invoke($0, `i64.trunc_f64_u`, [value('f64', -0.9999999999999999)]), [value('i64', 0n)]);

// ./test/core/conversions.wast:232
assert_return(() => invoke($0, `i64.trunc_f64_u`, [value('f64', 100000000)]), [value('i64', 100000000n)]);

// ./test/core/conversions.wast:233
assert_return(() => invoke($0, `i64.trunc_f64_u`, [value('f64', 10000000000000000)]), [value('i64', 10000000000000000n)]);

// ./test/core/conversions.wast:234
assert_return(() => invoke($0, `i64.trunc_f64_u`, [value('f64', 9223372036854776000)]), [value('i64', -9223372036854775808n)]);

// ./test/core/conversions.wast:235
assert_trap(() => invoke($0, `i64.trunc_f64_u`, [value('f64', 18446744073709552000)]), `integer overflow`);

// ./test/core/conversions.wast:236
assert_trap(() => invoke($0, `i64.trunc_f64_u`, [value('f64', -1)]), `integer overflow`);

// ./test/core/conversions.wast:237
assert_trap(() => invoke($0, `i64.trunc_f64_u`, [value('f64', Infinity)]), `integer overflow`);

// ./test/core/conversions.wast:238
assert_trap(() => invoke($0, `i64.trunc_f64_u`, [value('f64', -Infinity)]), `integer overflow`);

// ./test/core/conversions.wast:239
assert_trap(() => invoke($0, `i64.trunc_f64_u`, [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:240
assert_trap(() => invoke($0, `i64.trunc_f64_u`, [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:241
assert_trap(() => invoke($0, `i64.trunc_f64_u`, [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:242
assert_trap(() => invoke($0, `i64.trunc_f64_u`, [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0xff])]), `invalid conversion to integer`);

// ./test/core/conversions.wast:244
assert_return(() => invoke($0, `f32.convert_i32_s`, [1]), [value('f32', 1)]);

// ./test/core/conversions.wast:245
assert_return(() => invoke($0, `f32.convert_i32_s`, [-1]), [value('f32', -1)]);

// ./test/core/conversions.wast:246
assert_return(() => invoke($0, `f32.convert_i32_s`, [0]), [value('f32', 0)]);

// ./test/core/conversions.wast:247
assert_return(() => invoke($0, `f32.convert_i32_s`, [2147483647]), [value('f32', 2147483600)]);

// ./test/core/conversions.wast:248
assert_return(() => invoke($0, `f32.convert_i32_s`, [-2147483648]), [value('f32', -2147483600)]);

// ./test/core/conversions.wast:249
assert_return(() => invoke($0, `f32.convert_i32_s`, [1234567890]), [value('f32', 1234568000)]);

// ./test/core/conversions.wast:251
assert_return(() => invoke($0, `f32.convert_i32_s`, [16777217]), [value('f32', 16777216)]);

// ./test/core/conversions.wast:252
assert_return(() => invoke($0, `f32.convert_i32_s`, [-16777217]), [value('f32', -16777216)]);

// ./test/core/conversions.wast:253
assert_return(() => invoke($0, `f32.convert_i32_s`, [16777219]), [value('f32', 16777220)]);

// ./test/core/conversions.wast:254
assert_return(() => invoke($0, `f32.convert_i32_s`, [-16777219]), [value('f32', -16777220)]);

// ./test/core/conversions.wast:256
assert_return(() => invoke($0, `f32.convert_i64_s`, [1n]), [value('f32', 1)]);

// ./test/core/conversions.wast:257
assert_return(() => invoke($0, `f32.convert_i64_s`, [-1n]), [value('f32', -1)]);

// ./test/core/conversions.wast:258
assert_return(() => invoke($0, `f32.convert_i64_s`, [0n]), [value('f32', 0)]);

// ./test/core/conversions.wast:259
assert_return(() => invoke($0, `f32.convert_i64_s`, [9223372036854775807n]), [value('f32', 9223372000000000000)]);

// ./test/core/conversions.wast:260
assert_return(() => invoke($0, `f32.convert_i64_s`, [-9223372036854775808n]), [value('f32', -9223372000000000000)]);

// ./test/core/conversions.wast:261
assert_return(() => invoke($0, `f32.convert_i64_s`, [314159265358979n]), [value('f32', 314159280000000)]);

// ./test/core/conversions.wast:263
assert_return(() => invoke($0, `f32.convert_i64_s`, [16777217n]), [value('f32', 16777216)]);

// ./test/core/conversions.wast:264
assert_return(() => invoke($0, `f32.convert_i64_s`, [-16777217n]), [value('f32', -16777216)]);

// ./test/core/conversions.wast:265
assert_return(() => invoke($0, `f32.convert_i64_s`, [16777219n]), [value('f32', 16777220)]);

// ./test/core/conversions.wast:266
assert_return(() => invoke($0, `f32.convert_i64_s`, [-16777219n]), [value('f32', -16777220)]);

// ./test/core/conversions.wast:268
assert_return(() => invoke($0, `f32.convert_i64_s`, [9223371212221054977n]), [value('f32', 9223371500000000000)]);

// ./test/core/conversions.wast:269
assert_return(() => invoke($0, `f32.convert_i64_s`, [-9223371761976868863n]), [value('f32', -9223371500000000000)]);

// ./test/core/conversions.wast:270
assert_return(() => invoke($0, `f32.convert_i64_s`, [9007199791611905n]), [value('f32', 9007200000000000)]);

// ./test/core/conversions.wast:271
assert_return(() => invoke($0, `f32.convert_i64_s`, [-9007199791611905n]), [value('f32', -9007200000000000)]);

// ./test/core/conversions.wast:273
assert_return(() => invoke($0, `f64.convert_i32_s`, [1]), [value('f64', 1)]);

// ./test/core/conversions.wast:274
assert_return(() => invoke($0, `f64.convert_i32_s`, [-1]), [value('f64', -1)]);

// ./test/core/conversions.wast:275
assert_return(() => invoke($0, `f64.convert_i32_s`, [0]), [value('f64', 0)]);

// ./test/core/conversions.wast:276
assert_return(() => invoke($0, `f64.convert_i32_s`, [2147483647]), [value('f64', 2147483647)]);

// ./test/core/conversions.wast:277
assert_return(() => invoke($0, `f64.convert_i32_s`, [-2147483648]), [value('f64', -2147483648)]);

// ./test/core/conversions.wast:278
assert_return(() => invoke($0, `f64.convert_i32_s`, [987654321]), [value('f64', 987654321)]);

// ./test/core/conversions.wast:280
assert_return(() => invoke($0, `f64.convert_i64_s`, [1n]), [value('f64', 1)]);

// ./test/core/conversions.wast:281
assert_return(() => invoke($0, `f64.convert_i64_s`, [-1n]), [value('f64', -1)]);

// ./test/core/conversions.wast:282
assert_return(() => invoke($0, `f64.convert_i64_s`, [0n]), [value('f64', 0)]);

// ./test/core/conversions.wast:283
assert_return(() => invoke($0, `f64.convert_i64_s`, [9223372036854775807n]), [value('f64', 9223372036854776000)]);

// ./test/core/conversions.wast:284
assert_return(() => invoke($0, `f64.convert_i64_s`, [-9223372036854775808n]), [value('f64', -9223372036854776000)]);

// ./test/core/conversions.wast:285
assert_return(() => invoke($0, `f64.convert_i64_s`, [4669201609102990n]), [value('f64', 4669201609102990)]);

// ./test/core/conversions.wast:287
assert_return(() => invoke($0, `f64.convert_i64_s`, [9007199254740993n]), [value('f64', 9007199254740992)]);

// ./test/core/conversions.wast:288
assert_return(() => invoke($0, `f64.convert_i64_s`, [-9007199254740993n]), [value('f64', -9007199254740992)]);

// ./test/core/conversions.wast:289
assert_return(() => invoke($0, `f64.convert_i64_s`, [9007199254740995n]), [value('f64', 9007199254740996)]);

// ./test/core/conversions.wast:290
assert_return(() => invoke($0, `f64.convert_i64_s`, [-9007199254740995n]), [value('f64', -9007199254740996)]);

// ./test/core/conversions.wast:292
assert_return(() => invoke($0, `f32.convert_i32_u`, [1]), [value('f32', 1)]);

// ./test/core/conversions.wast:293
assert_return(() => invoke($0, `f32.convert_i32_u`, [0]), [value('f32', 0)]);

// ./test/core/conversions.wast:294
assert_return(() => invoke($0, `f32.convert_i32_u`, [2147483647]), [value('f32', 2147483600)]);

// ./test/core/conversions.wast:295
assert_return(() => invoke($0, `f32.convert_i32_u`, [-2147483648]), [value('f32', 2147483600)]);

// ./test/core/conversions.wast:296
assert_return(() => invoke($0, `f32.convert_i32_u`, [305419896]), [value('f32', 305419900)]);

// ./test/core/conversions.wast:297
assert_return(() => invoke($0, `f32.convert_i32_u`, [-1]), [value('f32', 4294967300)]);

// ./test/core/conversions.wast:298
assert_return(() => invoke($0, `f32.convert_i32_u`, [-2147483520]), [value('f32', 2147483600)]);

// ./test/core/conversions.wast:299
assert_return(() => invoke($0, `f32.convert_i32_u`, [-2147483519]), [value('f32', 2147484000)]);

// ./test/core/conversions.wast:300
assert_return(() => invoke($0, `f32.convert_i32_u`, [-2147483518]), [value('f32', 2147484000)]);

// ./test/core/conversions.wast:301
assert_return(() => invoke($0, `f32.convert_i32_u`, [-384]), [value('f32', 4294966800)]);

// ./test/core/conversions.wast:302
assert_return(() => invoke($0, `f32.convert_i32_u`, [-383]), [value('f32', 4294967000)]);

// ./test/core/conversions.wast:303
assert_return(() => invoke($0, `f32.convert_i32_u`, [-382]), [value('f32', 4294967000)]);

// ./test/core/conversions.wast:305
assert_return(() => invoke($0, `f32.convert_i32_u`, [16777217]), [value('f32', 16777216)]);

// ./test/core/conversions.wast:306
assert_return(() => invoke($0, `f32.convert_i32_u`, [16777219]), [value('f32', 16777220)]);

// ./test/core/conversions.wast:308
assert_return(() => invoke($0, `f32.convert_i64_u`, [1n]), [value('f32', 1)]);

// ./test/core/conversions.wast:309
assert_return(() => invoke($0, `f32.convert_i64_u`, [0n]), [value('f32', 0)]);

// ./test/core/conversions.wast:310
assert_return(() => invoke($0, `f32.convert_i64_u`, [9223372036854775807n]), [value('f32', 9223372000000000000)]);

// ./test/core/conversions.wast:311
assert_return(() => invoke($0, `f32.convert_i64_u`, [-9223372036854775808n]), [value('f32', 9223372000000000000)]);

// ./test/core/conversions.wast:312
assert_return(() => invoke($0, `f32.convert_i64_u`, [-1n]), [value('f32', 18446744000000000000)]);

// ./test/core/conversions.wast:314
assert_return(() => invoke($0, `f32.convert_i64_u`, [16777217n]), [value('f32', 16777216)]);

// ./test/core/conversions.wast:315
assert_return(() => invoke($0, `f32.convert_i64_u`, [16777219n]), [value('f32', 16777220)]);

// ./test/core/conversions.wast:317
assert_return(() => invoke($0, `f32.convert_i64_u`, [9007199791611905n]), [value('f32', 9007200000000000)]);

// ./test/core/conversions.wast:318
assert_return(() => invoke($0, `f32.convert_i64_u`, [9223371761976868863n]), [value('f32', 9223371500000000000)]);

// ./test/core/conversions.wast:319
assert_return(() => invoke($0, `f32.convert_i64_u`, [-9223371487098961919n]), [value('f32', 9223373000000000000)]);

// ./test/core/conversions.wast:320
assert_return(() => invoke($0, `f32.convert_i64_u`, [-1649267441663n]), [value('f32', 18446743000000000000)]);

// ./test/core/conversions.wast:322
assert_return(() => invoke($0, `f64.convert_i32_u`, [1]), [value('f64', 1)]);

// ./test/core/conversions.wast:323
assert_return(() => invoke($0, `f64.convert_i32_u`, [0]), [value('f64', 0)]);

// ./test/core/conversions.wast:324
assert_return(() => invoke($0, `f64.convert_i32_u`, [2147483647]), [value('f64', 2147483647)]);

// ./test/core/conversions.wast:325
assert_return(() => invoke($0, `f64.convert_i32_u`, [-2147483648]), [value('f64', 2147483648)]);

// ./test/core/conversions.wast:326
assert_return(() => invoke($0, `f64.convert_i32_u`, [-1]), [value('f64', 4294967295)]);

// ./test/core/conversions.wast:328
assert_return(() => invoke($0, `f64.convert_i64_u`, [1n]), [value('f64', 1)]);

// ./test/core/conversions.wast:329
assert_return(() => invoke($0, `f64.convert_i64_u`, [0n]), [value('f64', 0)]);

// ./test/core/conversions.wast:330
assert_return(() => invoke($0, `f64.convert_i64_u`, [9223372036854775807n]), [value('f64', 9223372036854776000)]);

// ./test/core/conversions.wast:331
assert_return(() => invoke($0, `f64.convert_i64_u`, [-9223372036854775808n]), [value('f64', 9223372036854776000)]);

// ./test/core/conversions.wast:332
assert_return(() => invoke($0, `f64.convert_i64_u`, [-1n]), [value('f64', 18446744073709552000)]);

// ./test/core/conversions.wast:333
assert_return(() => invoke($0, `f64.convert_i64_u`, [-9223372036854774784n]), [value('f64', 9223372036854776000)]);

// ./test/core/conversions.wast:334
assert_return(() => invoke($0, `f64.convert_i64_u`, [-9223372036854774783n]), [value('f64', 9223372036854778000)]);

// ./test/core/conversions.wast:335
assert_return(() => invoke($0, `f64.convert_i64_u`, [-9223372036854774782n]), [value('f64', 9223372036854778000)]);

// ./test/core/conversions.wast:336
assert_return(() => invoke($0, `f64.convert_i64_u`, [-3072n]), [value('f64', 18446744073709548000)]);

// ./test/core/conversions.wast:337
assert_return(() => invoke($0, `f64.convert_i64_u`, [-3071n]), [value('f64', 18446744073709550000)]);

// ./test/core/conversions.wast:338
assert_return(() => invoke($0, `f64.convert_i64_u`, [-3070n]), [value('f64', 18446744073709550000)]);

// ./test/core/conversions.wast:340
assert_return(() => invoke($0, `f64.convert_i64_u`, [9007199254740993n]), [value('f64', 9007199254740992)]);

// ./test/core/conversions.wast:341
assert_return(() => invoke($0, `f64.convert_i64_u`, [9007199254740995n]), [value('f64', 9007199254740996)]);

// ./test/core/conversions.wast:343
assert_return(() => invoke($0, `f64.promote_f32`, [value('f32', 0)]), [value('f64', 0)]);

// ./test/core/conversions.wast:344
assert_return(() => invoke($0, `f64.promote_f32`, [bytes('f32', [0x0, 0x0, 0x0, 0x80])]), [value('f64', -0)]);

// ./test/core/conversions.wast:345
assert_return(() => invoke($0, `f64.promote_f32`, [value('f32', 0.000000000000000000000000000000000000000000001)]), [value('f64', 0.000000000000000000000000000000000000000000001401298464324817)]);

// ./test/core/conversions.wast:346
assert_return(() => invoke($0, `f64.promote_f32`, [value('f32', -0.000000000000000000000000000000000000000000001)]), [value('f64', -0.000000000000000000000000000000000000000000001401298464324817)]);

// ./test/core/conversions.wast:347
assert_return(() => invoke($0, `f64.promote_f32`, [value('f32', 1)]), [value('f64', 1)]);

// ./test/core/conversions.wast:348
assert_return(() => invoke($0, `f64.promote_f32`, [value('f32', -1)]), [value('f64', -1)]);

// ./test/core/conversions.wast:349
assert_return(() => invoke($0, `f64.promote_f32`, [value('f32', -340282350000000000000000000000000000000)]), [value('f64', -340282346638528860000000000000000000000)]);

// ./test/core/conversions.wast:350
assert_return(() => invoke($0, `f64.promote_f32`, [value('f32', 340282350000000000000000000000000000000)]), [value('f64', 340282346638528860000000000000000000000)]);

// ./test/core/conversions.wast:352
assert_return(() => invoke($0, `f64.promote_f32`, [value('f32', 0.0000000000000000000000000000000000015046328)]), [value('f64', 0.000000000000000000000000000000000001504632769052528)]);

// ./test/core/conversions.wast:354
assert_return(() => invoke($0, `f64.promote_f32`, [value('f32', 66382537000000000000000000000000000000)]), [value('f64', 66382536710104395000000000000000000000)]);

// ./test/core/conversions.wast:355
assert_return(() => invoke($0, `f64.promote_f32`, [value('f32', Infinity)]), [value('f64', Infinity)]);

// ./test/core/conversions.wast:356
assert_return(() => invoke($0, `f64.promote_f32`, [value('f32', -Infinity)]), [value('f64', -Infinity)]);

// ./test/core/conversions.wast:357
assert_return(() => invoke($0, `f64.promote_f32`, [bytes('f32', [0x0, 0x0, 0xc0, 0x7f])]), [`f64_canonical_nan`]);

// ./test/core/conversions.wast:358
assert_return(() => invoke($0, `f64.promote_f32`, [bytes('f32', [0x0, 0x0, 0xa0, 0x7f])]), [`f64_arithmetic_nan`]);

// ./test/core/conversions.wast:359
assert_return(() => invoke($0, `f64.promote_f32`, [bytes('f32', [0x0, 0x0, 0xc0, 0xff])]), [`f64_canonical_nan`]);

// ./test/core/conversions.wast:360
assert_return(() => invoke($0, `f64.promote_f32`, [bytes('f32', [0x0, 0x0, 0xa0, 0xff])]), [`f64_arithmetic_nan`]);

// ./test/core/conversions.wast:362
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 0)]), [value('f32', 0)]);

// ./test/core/conversions.wast:363
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', -0)]), [bytes('f32', [0x0, 0x0, 0x0, 0x80])]);

// ./test/core/conversions.wast:364
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005)]), [value('f32', 0)]);

// ./test/core/conversions.wast:365
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005)]), [bytes('f32', [0x0, 0x0, 0x0, 0x80])]);

// ./test/core/conversions.wast:366
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 1)]), [value('f32', 1)]);

// ./test/core/conversions.wast:367
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', -1)]), [value('f32', -1)]);

// ./test/core/conversions.wast:368
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 0.000000000000000000000000000000000000011754942807573643)]), [value('f32', 0.000000000000000000000000000000000000011754944)]);

// ./test/core/conversions.wast:369
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', -0.000000000000000000000000000000000000011754942807573643)]), [value('f32', -0.000000000000000000000000000000000000011754944)]);

// ./test/core/conversions.wast:370
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 0.000000000000000000000000000000000000011754942807573642)]), [value('f32', 0.000000000000000000000000000000000000011754942)]);

// ./test/core/conversions.wast:371
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', -0.000000000000000000000000000000000000011754942807573642)]), [value('f32', -0.000000000000000000000000000000000000011754942)]);

// ./test/core/conversions.wast:372
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 0.000000000000000000000000000000000000000000001401298464324817)]), [value('f32', 0.000000000000000000000000000000000000000000001)]);

// ./test/core/conversions.wast:373
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', -0.000000000000000000000000000000000000000000001401298464324817)]), [value('f32', -0.000000000000000000000000000000000000000000001)]);

// ./test/core/conversions.wast:374
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 340282336497324060000000000000000000000)]), [value('f32', 340282330000000000000000000000000000000)]);

// ./test/core/conversions.wast:375
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', -340282336497324060000000000000000000000)]), [value('f32', -340282330000000000000000000000000000000)]);

// ./test/core/conversions.wast:376
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 340282336497324100000000000000000000000)]), [value('f32', 340282350000000000000000000000000000000)]);

// ./test/core/conversions.wast:377
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', -340282336497324100000000000000000000000)]), [value('f32', -340282350000000000000000000000000000000)]);

// ./test/core/conversions.wast:378
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 340282346638528860000000000000000000000)]), [value('f32', 340282350000000000000000000000000000000)]);

// ./test/core/conversions.wast:379
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', -340282346638528860000000000000000000000)]), [value('f32', -340282350000000000000000000000000000000)]);

// ./test/core/conversions.wast:380
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 340282356779733620000000000000000000000)]), [value('f32', 340282350000000000000000000000000000000)]);

// ./test/core/conversions.wast:381
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', -340282356779733620000000000000000000000)]), [value('f32', -340282350000000000000000000000000000000)]);

// ./test/core/conversions.wast:382
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 340282356779733660000000000000000000000)]), [value('f32', Infinity)]);

// ./test/core/conversions.wast:383
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', -340282356779733660000000000000000000000)]), [value('f32', -Infinity)]);

// ./test/core/conversions.wast:384
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 0.000000000000000000000000000000000001504632769052528)]), [value('f32', 0.0000000000000000000000000000000000015046328)]);

// ./test/core/conversions.wast:385
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 66382536710104395000000000000000000000)]), [value('f32', 66382537000000000000000000000000000000)]);

// ./test/core/conversions.wast:386
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', Infinity)]), [value('f32', Infinity)]);

// ./test/core/conversions.wast:387
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', -Infinity)]), [value('f32', -Infinity)]);

// ./test/core/conversions.wast:388
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 1.0000000000000002)]), [value('f32', 1)]);

// ./test/core/conversions.wast:389
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 0.9999999999999999)]), [value('f32', 1)]);

// ./test/core/conversions.wast:390
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 1.0000000596046448)]), [value('f32', 1)]);

// ./test/core/conversions.wast:391
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 1.000000059604645)]), [value('f32', 1.0000001)]);

// ./test/core/conversions.wast:392
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 1.000000178813934)]), [value('f32', 1.0000001)]);

// ./test/core/conversions.wast:393
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 1.0000001788139343)]), [value('f32', 1.0000002)]);

// ./test/core/conversions.wast:394
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 1.0000002980232239)]), [value('f32', 1.0000002)]);

// ./test/core/conversions.wast:395
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 16777217)]), [value('f32', 16777216)]);

// ./test/core/conversions.wast:396
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 16777217.000000004)]), [value('f32', 16777218)]);

// ./test/core/conversions.wast:397
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 16777218.999999996)]), [value('f32', 16777218)]);

// ./test/core/conversions.wast:398
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 16777219)]), [value('f32', 16777220)]);

// ./test/core/conversions.wast:399
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 424258443299142700000000000000000)]), [value('f32', 424258450000000000000000000000000)]);

// ./test/core/conversions.wast:400
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 0.0000000000000000000000000000000001569262107843488)]), [value('f32', 0.00000000000000000000000000000000015692621)]);

// ./test/core/conversions.wast:401
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 0.000000000000000000000000000000000000010551773688605172)]), [value('f32', 0.000000000000000000000000000000000000010551773)]);

// ./test/core/conversions.wast:402
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', -2.8238128484141933)]), [value('f32', -2.823813)]);

// ./test/core/conversions.wast:403
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', -9063376370095757000000000000000000)]), [value('f32', -9063376000000000000000000000000000)]);

// ./test/core/conversions.wast:404
assert_return(() => invoke($0, `f32.demote_f64`, [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])]), [`f32_canonical_nan`]);

// ./test/core/conversions.wast:405
assert_return(() => invoke($0, `f32.demote_f64`, [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f])]), [`f32_arithmetic_nan`]);

// ./test/core/conversions.wast:406
assert_return(() => invoke($0, `f32.demote_f64`, [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff])]), [`f32_canonical_nan`]);

// ./test/core/conversions.wast:407
assert_return(() => invoke($0, `f32.demote_f64`, [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0xff])]), [`f32_arithmetic_nan`]);

// ./test/core/conversions.wast:408
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000022250738585072014)]), [value('f32', 0)]);

// ./test/core/conversions.wast:409
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000022250738585072014)]), [bytes('f32', [0x0, 0x0, 0x0, 0x80])]);

// ./test/core/conversions.wast:410
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 0.0000000000000000000000000000000000000000000007006492321624085)]), [value('f32', 0)]);

// ./test/core/conversions.wast:411
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', -0.0000000000000000000000000000000000000000000007006492321624085)]), [bytes('f32', [0x0, 0x0, 0x0, 0x80])]);

// ./test/core/conversions.wast:412
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', 0.0000000000000000000000000000000000000000000007006492321624087)]), [value('f32', 0.000000000000000000000000000000000000000000001)]);

// ./test/core/conversions.wast:413
assert_return(() => invoke($0, `f32.demote_f64`, [value('f64', -0.0000000000000000000000000000000000000000000007006492321624087)]), [value('f32', -0.000000000000000000000000000000000000000000001)]);

// ./test/core/conversions.wast:415
assert_return(() => invoke($0, `f32.reinterpret_i32`, [0]), [value('f32', 0)]);

// ./test/core/conversions.wast:416
assert_return(() => invoke($0, `f32.reinterpret_i32`, [-2147483648]), [bytes('f32', [0x0, 0x0, 0x0, 0x80])]);

// ./test/core/conversions.wast:417
assert_return(() => invoke($0, `f32.reinterpret_i32`, [1]), [value('f32', 0.000000000000000000000000000000000000000000001)]);

// ./test/core/conversions.wast:418
assert_return(() => invoke($0, `f32.reinterpret_i32`, [-1]), [bytes('f32', [0xff, 0xff, 0xff, 0xff])]);

// ./test/core/conversions.wast:419
assert_return(() => invoke($0, `f32.reinterpret_i32`, [123456789]), [value('f32', 0.00000000000000000000000000000000016535997)]);

// ./test/core/conversions.wast:420
assert_return(() => invoke($0, `f32.reinterpret_i32`, [-2147483647]), [value('f32', -0.000000000000000000000000000000000000000000001)]);

// ./test/core/conversions.wast:421
assert_return(() => invoke($0, `f32.reinterpret_i32`, [2139095040]), [value('f32', Infinity)]);

// ./test/core/conversions.wast:422
assert_return(() => invoke($0, `f32.reinterpret_i32`, [-8388608]), [value('f32', -Infinity)]);

// ./test/core/conversions.wast:423
assert_return(() => invoke($0, `f32.reinterpret_i32`, [2143289344]), [bytes('f32', [0x0, 0x0, 0xc0, 0x7f])]);

// ./test/core/conversions.wast:424
assert_return(() => invoke($0, `f32.reinterpret_i32`, [-4194304]), [bytes('f32', [0x0, 0x0, 0xc0, 0xff])]);

// ./test/core/conversions.wast:425
assert_return(() => invoke($0, `f32.reinterpret_i32`, [2141192192]), [bytes('f32', [0x0, 0x0, 0xa0, 0x7f])]);

// ./test/core/conversions.wast:426
assert_return(() => invoke($0, `f32.reinterpret_i32`, [-6291456]), [bytes('f32', [0x0, 0x0, 0xa0, 0xff])]);

// ./test/core/conversions.wast:428
assert_return(() => invoke($0, `f64.reinterpret_i64`, [0n]), [value('f64', 0)]);

// ./test/core/conversions.wast:429
assert_return(() => invoke($0, `f64.reinterpret_i64`, [1n]), [value('f64', 0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005)]);

// ./test/core/conversions.wast:430
assert_return(() => invoke($0, `f64.reinterpret_i64`, [-1n]), [bytes('f64', [0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])]);

// ./test/core/conversions.wast:431
assert_return(() => invoke($0, `f64.reinterpret_i64`, [-9223372036854775808n]), [value('f64', -0)]);

// ./test/core/conversions.wast:432
assert_return(() => invoke($0, `f64.reinterpret_i64`, [1234567890n]), [value('f64', 0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000609957582)]);

// ./test/core/conversions.wast:433
assert_return(() => invoke($0, `f64.reinterpret_i64`, [-9223372036854775807n]), [value('f64', -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005)]);

// ./test/core/conversions.wast:434
assert_return(() => invoke($0, `f64.reinterpret_i64`, [9218868437227405312n]), [value('f64', Infinity)]);

// ./test/core/conversions.wast:435
assert_return(() => invoke($0, `f64.reinterpret_i64`, [-4503599627370496n]), [value('f64', -Infinity)]);

// ./test/core/conversions.wast:436
assert_return(() => invoke($0, `f64.reinterpret_i64`, [9221120237041090560n]), [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])]);

// ./test/core/conversions.wast:437
assert_return(() => invoke($0, `f64.reinterpret_i64`, [-2251799813685248n]), [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff])]);

// ./test/core/conversions.wast:438
assert_return(() => invoke($0, `f64.reinterpret_i64`, [9219994337134247936n]), [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f])]);

// ./test/core/conversions.wast:439
assert_return(() => invoke($0, `f64.reinterpret_i64`, [-3377699720527872n]), [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0xff])]);

// ./test/core/conversions.wast:441
assert_return(() => invoke($0, `i32.reinterpret_f32`, [value('f32', 0)]), [value('i32', 0)]);

// ./test/core/conversions.wast:442
assert_return(() => invoke($0, `i32.reinterpret_f32`, [bytes('f32', [0x0, 0x0, 0x0, 0x80])]), [value('i32', -2147483648)]);

// ./test/core/conversions.wast:443
assert_return(() => invoke($0, `i32.reinterpret_f32`, [value('f32', 0.000000000000000000000000000000000000000000001)]), [value('i32', 1)]);

// ./test/core/conversions.wast:444
assert_return(() => invoke($0, `i32.reinterpret_f32`, [bytes('f32', [0xff, 0xff, 0xff, 0xff])]), [value('i32', -1)]);

// ./test/core/conversions.wast:445
assert_return(() => invoke($0, `i32.reinterpret_f32`, [value('f32', -0.000000000000000000000000000000000000000000001)]), [value('i32', -2147483647)]);

// ./test/core/conversions.wast:446
assert_return(() => invoke($0, `i32.reinterpret_f32`, [value('f32', 1)]), [value('i32', 1065353216)]);

// ./test/core/conversions.wast:447
assert_return(() => invoke($0, `i32.reinterpret_f32`, [value('f32', 3.1415925)]), [value('i32', 1078530010)]);

// ./test/core/conversions.wast:448
assert_return(() => invoke($0, `i32.reinterpret_f32`, [value('f32', 340282350000000000000000000000000000000)]), [value('i32', 2139095039)]);

// ./test/core/conversions.wast:449
assert_return(() => invoke($0, `i32.reinterpret_f32`, [value('f32', -340282350000000000000000000000000000000)]), [value('i32', -8388609)]);

// ./test/core/conversions.wast:450
assert_return(() => invoke($0, `i32.reinterpret_f32`, [value('f32', Infinity)]), [value('i32', 2139095040)]);

// ./test/core/conversions.wast:451
assert_return(() => invoke($0, `i32.reinterpret_f32`, [value('f32', -Infinity)]), [value('i32', -8388608)]);

// ./test/core/conversions.wast:452
assert_return(() => invoke($0, `i32.reinterpret_f32`, [bytes('f32', [0x0, 0x0, 0xc0, 0x7f])]), [value('i32', 2143289344)]);

// ./test/core/conversions.wast:453
assert_return(() => invoke($0, `i32.reinterpret_f32`, [bytes('f32', [0x0, 0x0, 0xc0, 0xff])]), [value('i32', -4194304)]);

// ./test/core/conversions.wast:454
assert_return(() => invoke($0, `i32.reinterpret_f32`, [bytes('f32', [0x0, 0x0, 0xa0, 0x7f])]), [value('i32', 2141192192)]);

// ./test/core/conversions.wast:455
assert_return(() => invoke($0, `i32.reinterpret_f32`, [bytes('f32', [0x0, 0x0, 0xa0, 0xff])]), [value('i32', -6291456)]);

// ./test/core/conversions.wast:457
assert_return(() => invoke($0, `i64.reinterpret_f64`, [value('f64', 0)]), [value('i64', 0n)]);

// ./test/core/conversions.wast:458
assert_return(() => invoke($0, `i64.reinterpret_f64`, [value('f64', -0)]), [value('i64', -9223372036854775808n)]);

// ./test/core/conversions.wast:459
assert_return(() => invoke($0, `i64.reinterpret_f64`, [value('f64', 0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005)]), [value('i64', 1n)]);

// ./test/core/conversions.wast:460
assert_return(() => invoke($0, `i64.reinterpret_f64`, [bytes('f64', [0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff])]), [value('i64', -1n)]);

// ./test/core/conversions.wast:461
assert_return(() => invoke($0, `i64.reinterpret_f64`, [value('f64', -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005)]), [value('i64', -9223372036854775807n)]);

// ./test/core/conversions.wast:462
assert_return(() => invoke($0, `i64.reinterpret_f64`, [value('f64', 1)]), [value('i64', 4607182418800017408n)]);

// ./test/core/conversions.wast:463
assert_return(() => invoke($0, `i64.reinterpret_f64`, [value('f64', 3.14159265358979)]), [value('i64', 4614256656552045841n)]);

// ./test/core/conversions.wast:464
assert_return(() => invoke($0, `i64.reinterpret_f64`, [value('f64', 179769313486231570000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]), [value('i64', 9218868437227405311n)]);

// ./test/core/conversions.wast:465
assert_return(() => invoke($0, `i64.reinterpret_f64`, [value('f64', -179769313486231570000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000)]), [value('i64', -4503599627370497n)]);

// ./test/core/conversions.wast:466
assert_return(() => invoke($0, `i64.reinterpret_f64`, [value('f64', Infinity)]), [value('i64', 9218868437227405312n)]);

// ./test/core/conversions.wast:467
assert_return(() => invoke($0, `i64.reinterpret_f64`, [value('f64', -Infinity)]), [value('i64', -4503599627370496n)]);

// ./test/core/conversions.wast:468
assert_return(() => invoke($0, `i64.reinterpret_f64`, [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])]), [value('i64', 9221120237041090560n)]);

// ./test/core/conversions.wast:469
assert_return(() => invoke($0, `i64.reinterpret_f64`, [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff])]), [value('i64', -2251799813685248n)]);

// ./test/core/conversions.wast:470
assert_return(() => invoke($0, `i64.reinterpret_f64`, [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f])]), [value('i64', 9219994337134247936n)]);

// ./test/core/conversions.wast:471
assert_return(() => invoke($0, `i64.reinterpret_f64`, [bytes('f64', [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0xff])]), [value('i64', -3377699720527872n)]);

// ./test/core/conversions.wast:475
assert_invalid(() => instantiate(`(module (func (result i32) (i32.wrap_i64 (f32.const 0))))`), `type mismatch`);

// ./test/core/conversions.wast:476
assert_invalid(() => instantiate(`(module (func (result i32) (i32.trunc_f32_s (i64.const 0))))`), `type mismatch`);

// ./test/core/conversions.wast:477
assert_invalid(() => instantiate(`(module (func (result i32) (i32.trunc_f32_u (i64.const 0))))`), `type mismatch`);

// ./test/core/conversions.wast:478
assert_invalid(() => instantiate(`(module (func (result i32) (i32.trunc_f64_s (i64.const 0))))`), `type mismatch`);

// ./test/core/conversions.wast:479
assert_invalid(() => instantiate(`(module (func (result i32) (i32.trunc_f64_u (i64.const 0))))`), `type mismatch`);

// ./test/core/conversions.wast:480
assert_invalid(() => instantiate(`(module (func (result i32) (i32.reinterpret_f32 (i64.const 0))))`), `type mismatch`);

// ./test/core/conversions.wast:481
assert_invalid(() => instantiate(`(module (func (result i64) (i64.extend_i32_s (f32.const 0))))`), `type mismatch`);

// ./test/core/conversions.wast:482
assert_invalid(() => instantiate(`(module (func (result i64) (i64.extend_i32_u (f32.const 0))))`), `type mismatch`);

// ./test/core/conversions.wast:483
assert_invalid(() => instantiate(`(module (func (result i64) (i64.trunc_f32_s (i32.const 0))))`), `type mismatch`);

// ./test/core/conversions.wast:484
assert_invalid(() => instantiate(`(module (func (result i64) (i64.trunc_f32_u (i32.const 0))))`), `type mismatch`);

// ./test/core/conversions.wast:485
assert_invalid(() => instantiate(`(module (func (result i64) (i64.trunc_f64_s (i32.const 0))))`), `type mismatch`);

// ./test/core/conversions.wast:486
assert_invalid(() => instantiate(`(module (func (result i64) (i64.trunc_f64_u (i32.const 0))))`), `type mismatch`);

// ./test/core/conversions.wast:487
assert_invalid(() => instantiate(`(module (func (result i64) (i64.reinterpret_f64 (i32.const 0))))`), `type mismatch`);

// ./test/core/conversions.wast:488
assert_invalid(() => instantiate(`(module (func (result f32) (f32.convert_i32_s (i64.const 0))))`), `type mismatch`);

// ./test/core/conversions.wast:489
assert_invalid(() => instantiate(`(module (func (result f32) (f32.convert_i32_u (i64.const 0))))`), `type mismatch`);

// ./test/core/conversions.wast:490
assert_invalid(() => instantiate(`(module (func (result f32) (f32.convert_i64_s (i32.const 0))))`), `type mismatch`);

// ./test/core/conversions.wast:491
assert_invalid(() => instantiate(`(module (func (result f32) (f32.convert_i64_u (i32.const 0))))`), `type mismatch`);

// ./test/core/conversions.wast:492
assert_invalid(() => instantiate(`(module (func (result f32) (f32.demote_f64 (i32.const 0))))`), `type mismatch`);

// ./test/core/conversions.wast:493
assert_invalid(() => instantiate(`(module (func (result f32) (f32.reinterpret_i32 (i64.const 0))))`), `type mismatch`);

// ./test/core/conversions.wast:494
assert_invalid(() => instantiate(`(module (func (result f64) (f64.convert_i32_s (i64.const 0))))`), `type mismatch`);

// ./test/core/conversions.wast:495
assert_invalid(() => instantiate(`(module (func (result f64) (f64.convert_i32_u (i64.const 0))))`), `type mismatch`);

// ./test/core/conversions.wast:496
assert_invalid(() => instantiate(`(module (func (result f64) (f64.convert_i64_s (i32.const 0))))`), `type mismatch`);

// ./test/core/conversions.wast:497
assert_invalid(() => instantiate(`(module (func (result f64) (f64.convert_i64_u (i32.const 0))))`), `type mismatch`);

// ./test/core/conversions.wast:498
assert_invalid(() => instantiate(`(module (func (result f64) (f64.promote_f32 (i32.const 0))))`), `type mismatch`);

// ./test/core/conversions.wast:499
assert_invalid(() => instantiate(`(module (func (result f64) (f64.reinterpret_i64 (i32.const 0))))`), `type mismatch`);

