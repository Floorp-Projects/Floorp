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

// ./test/core/multi-memory/memory_trap1.wast

// ./test/core/multi-memory/memory_trap1.wast:1
let $0 = instantiate(`(module
  (memory 0)
  (memory 0)
  (memory $$m 1)
  (data (memory 2) (i32.const 0) "abcdefgh")
  (data (memory 2) (i32.const 0xfff8) "abcdefgh")

  (func (export "i32.load") (param $$a i32) (result i32)
    (i32.load $$m (local.get $$a))
  )
  (func (export "i64.load") (param $$a i32) (result i64)
    (i64.load $$m (local.get $$a))
  )
  (func (export "f32.load") (param $$a i32) (result f32)
    (f32.load $$m (local.get $$a))
  )
  (func (export "f64.load") (param $$a i32) (result f64)
    (f64.load $$m (local.get $$a))
  )
  (func (export "i32.load8_s") (param $$a i32) (result i32)
    (i32.load8_s $$m (local.get $$a))
  )
  (func (export "i32.load8_u") (param $$a i32) (result i32)
    (i32.load8_u $$m (local.get $$a))
  )
  (func (export "i32.load16_s") (param $$a i32) (result i32)
    (i32.load16_s $$m (local.get $$a))
  )
  (func (export "i32.load16_u") (param $$a i32) (result i32)
    (i32.load16_u $$m (local.get $$a))
  )
  (func (export "i64.load8_s") (param $$a i32) (result i64)
    (i64.load8_s $$m (local.get $$a))
  )
  (func (export "i64.load8_u") (param $$a i32) (result i64)
    (i64.load8_u $$m (local.get $$a))
  )
  (func (export "i64.load16_s") (param $$a i32) (result i64)
    (i64.load16_s $$m (local.get $$a))
  )
  (func (export "i64.load16_u") (param $$a i32) (result i64)
    (i64.load16_u $$m (local.get $$a))
  )
  (func (export "i64.load32_s") (param $$a i32) (result i64)
    (i64.load32_s $$m (local.get $$a))
  )
  (func (export "i64.load32_u") (param $$a i32) (result i64)
    (i64.load32_u $$m (local.get $$a))
  )
  (func (export "i32.store") (param $$a i32) (param $$v i32)
    (i32.store $$m (local.get $$a) (local.get $$v))
  )
  (func (export "i64.store") (param $$a i32) (param $$v i64)
    (i64.store $$m (local.get $$a) (local.get $$v))
  )
  (func (export "f32.store") (param $$a i32) (param $$v f32)
    (f32.store $$m (local.get $$a) (local.get $$v))
  )
  (func (export "f64.store") (param $$a i32) (param $$v f64)
    (f64.store $$m (local.get $$a) (local.get $$v))
  )
  (func (export "i32.store8") (param $$a i32) (param $$v i32)
    (i32.store8 $$m (local.get $$a) (local.get $$v))
  )
  (func (export "i32.store16") (param $$a i32) (param $$v i32)
    (i32.store16 $$m (local.get $$a) (local.get $$v))
  )
  (func (export "i64.store8") (param $$a i32) (param $$v i64)
    (i64.store8 $$m (local.get $$a) (local.get $$v))
  )
  (func (export "i64.store16") (param $$a i32) (param $$v i64)
    (i64.store16 $$m (local.get $$a) (local.get $$v))
  )
  (func (export "i64.store32") (param $$a i32) (param $$v i64)
    (i64.store32 $$m (local.get $$a) (local.get $$v))
  )
)`);

// ./test/core/multi-memory/memory_trap1.wast:79
assert_trap(() => invoke($0, `i32.store`, [65536, 0]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:80
assert_trap(() => invoke($0, `i32.store`, [65535, 0]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:81
assert_trap(() => invoke($0, `i32.store`, [65534, 0]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:82
assert_trap(() => invoke($0, `i32.store`, [65533, 0]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:83
assert_trap(() => invoke($0, `i32.store`, [-1, 0]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:84
assert_trap(() => invoke($0, `i32.store`, [-2, 0]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:85
assert_trap(() => invoke($0, `i32.store`, [-3, 0]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:86
assert_trap(() => invoke($0, `i32.store`, [-4, 0]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:87
assert_trap(() => invoke($0, `i64.store`, [65536, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:88
assert_trap(() => invoke($0, `i64.store`, [65535, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:89
assert_trap(() => invoke($0, `i64.store`, [65534, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:90
assert_trap(() => invoke($0, `i64.store`, [65533, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:91
assert_trap(() => invoke($0, `i64.store`, [65532, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:92
assert_trap(() => invoke($0, `i64.store`, [65531, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:93
assert_trap(() => invoke($0, `i64.store`, [65530, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:94
assert_trap(() => invoke($0, `i64.store`, [65529, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:95
assert_trap(() => invoke($0, `i64.store`, [-1, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:96
assert_trap(() => invoke($0, `i64.store`, [-2, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:97
assert_trap(() => invoke($0, `i64.store`, [-3, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:98
assert_trap(() => invoke($0, `i64.store`, [-4, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:99
assert_trap(() => invoke($0, `i64.store`, [-5, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:100
assert_trap(() => invoke($0, `i64.store`, [-6, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:101
assert_trap(() => invoke($0, `i64.store`, [-7, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:102
assert_trap(() => invoke($0, `i64.store`, [-8, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:103
assert_trap(() => invoke($0, `f32.store`, [65536, value("f32", 0)]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:104
assert_trap(() => invoke($0, `f32.store`, [65535, value("f32", 0)]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:105
assert_trap(() => invoke($0, `f32.store`, [65534, value("f32", 0)]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:106
assert_trap(() => invoke($0, `f32.store`, [65533, value("f32", 0)]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:107
assert_trap(() => invoke($0, `f32.store`, [-1, value("f32", 0)]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:108
assert_trap(() => invoke($0, `f32.store`, [-2, value("f32", 0)]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:109
assert_trap(() => invoke($0, `f32.store`, [-3, value("f32", 0)]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:110
assert_trap(() => invoke($0, `f32.store`, [-4, value("f32", 0)]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:111
assert_trap(() => invoke($0, `f64.store`, [65536, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:112
assert_trap(() => invoke($0, `f64.store`, [65535, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:113
assert_trap(() => invoke($0, `f64.store`, [65534, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:114
assert_trap(() => invoke($0, `f64.store`, [65533, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:115
assert_trap(() => invoke($0, `f64.store`, [65532, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:116
assert_trap(() => invoke($0, `f64.store`, [65531, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:117
assert_trap(() => invoke($0, `f64.store`, [65530, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:118
assert_trap(() => invoke($0, `f64.store`, [65529, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:119
assert_trap(() => invoke($0, `f64.store`, [-1, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:120
assert_trap(() => invoke($0, `f64.store`, [-2, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:121
assert_trap(() => invoke($0, `f64.store`, [-3, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:122
assert_trap(() => invoke($0, `f64.store`, [-4, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:123
assert_trap(() => invoke($0, `f64.store`, [-5, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:124
assert_trap(() => invoke($0, `f64.store`, [-6, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:125
assert_trap(() => invoke($0, `f64.store`, [-7, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:126
assert_trap(() => invoke($0, `f64.store`, [-8, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:127
assert_trap(() => invoke($0, `i32.store8`, [65536, 0]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:128
assert_trap(() => invoke($0, `i32.store8`, [-1, 0]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:129
assert_trap(() => invoke($0, `i32.store16`, [65536, 0]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:130
assert_trap(() => invoke($0, `i32.store16`, [65535, 0]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:131
assert_trap(() => invoke($0, `i32.store16`, [-1, 0]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:132
assert_trap(() => invoke($0, `i32.store16`, [-2, 0]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:133
assert_trap(() => invoke($0, `i64.store8`, [65536, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:134
assert_trap(() => invoke($0, `i64.store8`, [-1, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:135
assert_trap(() => invoke($0, `i64.store16`, [65536, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:136
assert_trap(() => invoke($0, `i64.store16`, [65535, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:137
assert_trap(() => invoke($0, `i64.store16`, [-1, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:138
assert_trap(() => invoke($0, `i64.store16`, [-2, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:139
assert_trap(() => invoke($0, `i64.store32`, [65536, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:140
assert_trap(() => invoke($0, `i64.store32`, [65535, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:141
assert_trap(() => invoke($0, `i64.store32`, [65534, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:142
assert_trap(() => invoke($0, `i64.store32`, [65533, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:143
assert_trap(() => invoke($0, `i64.store32`, [-1, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:144
assert_trap(() => invoke($0, `i64.store32`, [-2, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:145
assert_trap(() => invoke($0, `i64.store32`, [-3, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:146
assert_trap(() => invoke($0, `i64.store32`, [-4, 0n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:147
assert_trap(() => invoke($0, `i32.load`, [65536]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:148
assert_trap(() => invoke($0, `i32.load`, [65535]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:149
assert_trap(() => invoke($0, `i32.load`, [65534]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:150
assert_trap(() => invoke($0, `i32.load`, [65533]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:151
assert_trap(() => invoke($0, `i32.load`, [-1]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:152
assert_trap(() => invoke($0, `i32.load`, [-2]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:153
assert_trap(() => invoke($0, `i32.load`, [-3]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:154
assert_trap(() => invoke($0, `i32.load`, [-4]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:155
assert_trap(() => invoke($0, `i64.load`, [65536]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:156
assert_trap(() => invoke($0, `i64.load`, [65535]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:157
assert_trap(() => invoke($0, `i64.load`, [65534]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:158
assert_trap(() => invoke($0, `i64.load`, [65533]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:159
assert_trap(() => invoke($0, `i64.load`, [65532]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:160
assert_trap(() => invoke($0, `i64.load`, [65531]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:161
assert_trap(() => invoke($0, `i64.load`, [65530]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:162
assert_trap(() => invoke($0, `i64.load`, [65529]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:163
assert_trap(() => invoke($0, `i64.load`, [-1]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:164
assert_trap(() => invoke($0, `i64.load`, [-2]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:165
assert_trap(() => invoke($0, `i64.load`, [-3]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:166
assert_trap(() => invoke($0, `i64.load`, [-4]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:167
assert_trap(() => invoke($0, `i64.load`, [-5]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:168
assert_trap(() => invoke($0, `i64.load`, [-6]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:169
assert_trap(() => invoke($0, `i64.load`, [-7]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:170
assert_trap(() => invoke($0, `i64.load`, [-8]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:171
assert_trap(() => invoke($0, `f32.load`, [65536]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:172
assert_trap(() => invoke($0, `f32.load`, [65535]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:173
assert_trap(() => invoke($0, `f32.load`, [65534]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:174
assert_trap(() => invoke($0, `f32.load`, [65533]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:175
assert_trap(() => invoke($0, `f32.load`, [-1]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:176
assert_trap(() => invoke($0, `f32.load`, [-2]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:177
assert_trap(() => invoke($0, `f32.load`, [-3]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:178
assert_trap(() => invoke($0, `f32.load`, [-4]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:179
assert_trap(() => invoke($0, `f64.load`, [65536]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:180
assert_trap(() => invoke($0, `f64.load`, [65535]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:181
assert_trap(() => invoke($0, `f64.load`, [65534]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:182
assert_trap(() => invoke($0, `f64.load`, [65533]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:183
assert_trap(() => invoke($0, `f64.load`, [65532]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:184
assert_trap(() => invoke($0, `f64.load`, [65531]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:185
assert_trap(() => invoke($0, `f64.load`, [65530]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:186
assert_trap(() => invoke($0, `f64.load`, [65529]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:187
assert_trap(() => invoke($0, `f64.load`, [-1]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:188
assert_trap(() => invoke($0, `f64.load`, [-2]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:189
assert_trap(() => invoke($0, `f64.load`, [-3]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:190
assert_trap(() => invoke($0, `f64.load`, [-4]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:191
assert_trap(() => invoke($0, `f64.load`, [-5]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:192
assert_trap(() => invoke($0, `f64.load`, [-6]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:193
assert_trap(() => invoke($0, `f64.load`, [-7]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:194
assert_trap(() => invoke($0, `f64.load`, [-8]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:195
assert_trap(() => invoke($0, `i32.load8_s`, [65536]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:196
assert_trap(() => invoke($0, `i32.load8_s`, [-1]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:197
assert_trap(() => invoke($0, `i32.load8_u`, [65536]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:198
assert_trap(() => invoke($0, `i32.load8_u`, [-1]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:199
assert_trap(() => invoke($0, `i32.load16_s`, [65536]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:200
assert_trap(() => invoke($0, `i32.load16_s`, [65535]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:201
assert_trap(() => invoke($0, `i32.load16_s`, [-1]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:202
assert_trap(() => invoke($0, `i32.load16_s`, [-2]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:203
assert_trap(() => invoke($0, `i32.load16_u`, [65536]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:204
assert_trap(() => invoke($0, `i32.load16_u`, [65535]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:205
assert_trap(() => invoke($0, `i32.load16_u`, [-1]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:206
assert_trap(() => invoke($0, `i32.load16_u`, [-2]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:207
assert_trap(() => invoke($0, `i64.load8_s`, [65536]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:208
assert_trap(() => invoke($0, `i64.load8_s`, [-1]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:209
assert_trap(() => invoke($0, `i64.load8_u`, [65536]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:210
assert_trap(() => invoke($0, `i64.load8_u`, [-1]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:211
assert_trap(() => invoke($0, `i64.load16_s`, [65536]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:212
assert_trap(() => invoke($0, `i64.load16_s`, [65535]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:213
assert_trap(() => invoke($0, `i64.load16_s`, [-1]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:214
assert_trap(() => invoke($0, `i64.load16_s`, [-2]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:215
assert_trap(() => invoke($0, `i64.load16_u`, [65536]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:216
assert_trap(() => invoke($0, `i64.load16_u`, [65535]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:217
assert_trap(() => invoke($0, `i64.load16_u`, [-1]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:218
assert_trap(() => invoke($0, `i64.load16_u`, [-2]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:219
assert_trap(() => invoke($0, `i64.load32_s`, [65536]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:220
assert_trap(() => invoke($0, `i64.load32_s`, [65535]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:221
assert_trap(() => invoke($0, `i64.load32_s`, [65534]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:222
assert_trap(() => invoke($0, `i64.load32_s`, [65533]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:223
assert_trap(() => invoke($0, `i64.load32_s`, [-1]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:224
assert_trap(() => invoke($0, `i64.load32_s`, [-2]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:225
assert_trap(() => invoke($0, `i64.load32_s`, [-3]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:226
assert_trap(() => invoke($0, `i64.load32_s`, [-4]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:227
assert_trap(() => invoke($0, `i64.load32_u`, [65536]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:228
assert_trap(() => invoke($0, `i64.load32_u`, [65535]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:229
assert_trap(() => invoke($0, `i64.load32_u`, [65534]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:230
assert_trap(() => invoke($0, `i64.load32_u`, [65533]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:231
assert_trap(() => invoke($0, `i64.load32_u`, [-1]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:232
assert_trap(() => invoke($0, `i64.load32_u`, [-2]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:233
assert_trap(() => invoke($0, `i64.load32_u`, [-3]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:234
assert_trap(() => invoke($0, `i64.load32_u`, [-4]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:237
assert_return(() => invoke($0, `i64.load`, [65528]), [value("i64", 7523094288207667809n)]);

// ./test/core/multi-memory/memory_trap1.wast:238
assert_return(() => invoke($0, `i64.load`, [0]), [value("i64", 7523094288207667809n)]);

// ./test/core/multi-memory/memory_trap1.wast:242
assert_return(() => invoke($0, `i64.store`, [65528, 0n]), []);

// ./test/core/multi-memory/memory_trap1.wast:243
assert_trap(() => invoke($0, `i32.store`, [65533, 305419896]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:244
assert_return(() => invoke($0, `i32.load`, [65532]), [value("i32", 0)]);

// ./test/core/multi-memory/memory_trap1.wast:245
assert_trap(() => invoke($0, `i64.store`, [65529, 1311768467294899695n]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap1.wast:246
assert_return(() => invoke($0, `i64.load`, [65528]), [value("i64", 0n)]);

// ./test/core/multi-memory/memory_trap1.wast:247
assert_trap(
  () => invoke($0, `f32.store`, [65533, value("f32", 305419900)]),
  `out of bounds memory access`,
);

// ./test/core/multi-memory/memory_trap1.wast:248
assert_return(() => invoke($0, `f32.load`, [65532]), [value("f32", 0)]);

// ./test/core/multi-memory/memory_trap1.wast:249
assert_trap(
  () => invoke($0, `f64.store`, [65529, value("f64", 1311768467294899700)]),
  `out of bounds memory access`,
);

// ./test/core/multi-memory/memory_trap1.wast:250
assert_return(() => invoke($0, `f64.load`, [65528]), [value("f64", 0)]);
