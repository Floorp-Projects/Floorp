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

// ./test/core/memory_trap64.wast

// ./test/core/memory_trap64.wast:1
let $0 = instantiate(`(module
    (memory i64 1)

    (func $$addr_limit (result i64)
      (i64.mul (memory.size) (i64.const 0x10000))
    )

    (func (export "store") (param $$i i64) (param $$v i32)
      (i32.store (i64.add (call $$addr_limit) (local.get $$i)) (local.get $$v))
    )

    (func (export "load") (param $$i i64) (result i32)
      (i32.load (i64.add (call $$addr_limit) (local.get $$i)))
    )

    (func (export "memory.grow") (param i64) (result i64)
      (memory.grow (local.get 0))
    )
)`);

// ./test/core/memory_trap64.wast:21
assert_return(() => invoke($0, `store`, [-4n, 42]), []);

// ./test/core/memory_trap64.wast:22
assert_return(() => invoke($0, `load`, [-4n]), [value("i32", 42)]);

// ./test/core/memory_trap64.wast:23
assert_trap(() => invoke($0, `store`, [-3n, 13]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:24
assert_trap(() => invoke($0, `load`, [-3n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:25
assert_trap(() => invoke($0, `store`, [-2n, 13]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:26
assert_trap(() => invoke($0, `load`, [-2n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:27
assert_trap(() => invoke($0, `store`, [-1n, 13]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:28
assert_trap(() => invoke($0, `load`, [-1n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:29
assert_trap(() => invoke($0, `store`, [0n, 13]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:30
assert_trap(() => invoke($0, `load`, [0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:31
assert_trap(() => invoke($0, `store`, [2147483648n, 13]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:32
assert_trap(() => invoke($0, `load`, [2147483648n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:34
let $1 = instantiate(`(module
  (memory i64 1)
  (data (i64.const 0) "abcdefgh")
  (data (i64.const 0xfff8) "abcdefgh")

  (func (export "i32.load") (param $$a i64) (result i32)
    (i32.load (local.get $$a))
  )
  (func (export "i64.load") (param $$a i64) (result i64)
    (i64.load (local.get $$a))
  )
  (func (export "f32.load") (param $$a i64) (result f32)
    (f32.load (local.get $$a))
  )
  (func (export "f64.load") (param $$a i64) (result f64)
    (f64.load (local.get $$a))
  )
  (func (export "i32.load8_s") (param $$a i64) (result i32)
    (i32.load8_s (local.get $$a))
  )
  (func (export "i32.load8_u") (param $$a i64) (result i32)
    (i32.load8_u (local.get $$a))
  )
  (func (export "i32.load16_s") (param $$a i64) (result i32)
    (i32.load16_s (local.get $$a))
  )
  (func (export "i32.load16_u") (param $$a i64) (result i32)
    (i32.load16_u (local.get $$a))
  )
  (func (export "i64.load8_s") (param $$a i64) (result i64)
    (i64.load8_s (local.get $$a))
  )
  (func (export "i64.load8_u") (param $$a i64) (result i64)
    (i64.load8_u (local.get $$a))
  )
  (func (export "i64.load16_s") (param $$a i64) (result i64)
    (i64.load16_s (local.get $$a))
  )
  (func (export "i64.load16_u") (param $$a i64) (result i64)
    (i64.load16_u (local.get $$a))
  )
  (func (export "i64.load32_s") (param $$a i64) (result i64)
    (i64.load32_s (local.get $$a))
  )
  (func (export "i64.load32_u") (param $$a i64) (result i64)
    (i64.load32_u (local.get $$a))
  )
  (func (export "i32.store") (param $$a i64) (param $$v i32)
    (i32.store (local.get $$a) (local.get $$v))
  )
  (func (export "i64.store") (param $$a i64) (param $$v i64)
    (i64.store (local.get $$a) (local.get $$v))
  )
  (func (export "f32.store") (param $$a i64) (param $$v f32)
    (f32.store (local.get $$a) (local.get $$v))
  )
  (func (export "f64.store") (param $$a i64) (param $$v f64)
    (f64.store (local.get $$a) (local.get $$v))
  )
  (func (export "i32.store8") (param $$a i64) (param $$v i32)
    (i32.store8 (local.get $$a) (local.get $$v))
  )
  (func (export "i32.store16") (param $$a i64) (param $$v i32)
    (i32.store16 (local.get $$a) (local.get $$v))
  )
  (func (export "i64.store8") (param $$a i64) (param $$v i64)
    (i64.store8 (local.get $$a) (local.get $$v))
  )
  (func (export "i64.store16") (param $$a i64) (param $$v i64)
    (i64.store16 (local.get $$a) (local.get $$v))
  )
  (func (export "i64.store32") (param $$a i64) (param $$v i64)
    (i64.store32 (local.get $$a) (local.get $$v))
  )
)`);

// ./test/core/memory_trap64.wast:110
assert_trap(() => invoke($1, `i32.store`, [65536n, 0]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:111
assert_trap(() => invoke($1, `i32.store`, [65535n, 0]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:112
assert_trap(() => invoke($1, `i32.store`, [65534n, 0]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:113
assert_trap(() => invoke($1, `i32.store`, [65533n, 0]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:114
assert_trap(() => invoke($1, `i32.store`, [-1n, 0]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:115
assert_trap(() => invoke($1, `i32.store`, [-2n, 0]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:116
assert_trap(() => invoke($1, `i32.store`, [-3n, 0]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:117
assert_trap(() => invoke($1, `i32.store`, [-4n, 0]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:118
assert_trap(() => invoke($1, `i64.store`, [65536n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:119
assert_trap(() => invoke($1, `i64.store`, [65535n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:120
assert_trap(() => invoke($1, `i64.store`, [65534n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:121
assert_trap(() => invoke($1, `i64.store`, [65533n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:122
assert_trap(() => invoke($1, `i64.store`, [65532n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:123
assert_trap(() => invoke($1, `i64.store`, [65531n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:124
assert_trap(() => invoke($1, `i64.store`, [65530n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:125
assert_trap(() => invoke($1, `i64.store`, [65529n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:126
assert_trap(() => invoke($1, `i64.store`, [-1n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:127
assert_trap(() => invoke($1, `i64.store`, [-2n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:128
assert_trap(() => invoke($1, `i64.store`, [-3n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:129
assert_trap(() => invoke($1, `i64.store`, [-4n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:130
assert_trap(() => invoke($1, `i64.store`, [-5n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:131
assert_trap(() => invoke($1, `i64.store`, [-6n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:132
assert_trap(() => invoke($1, `i64.store`, [-7n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:133
assert_trap(() => invoke($1, `i64.store`, [-8n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:134
assert_trap(() => invoke($1, `f32.store`, [65536n, value("f32", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:135
assert_trap(() => invoke($1, `f32.store`, [65535n, value("f32", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:136
assert_trap(() => invoke($1, `f32.store`, [65534n, value("f32", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:137
assert_trap(() => invoke($1, `f32.store`, [65533n, value("f32", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:138
assert_trap(() => invoke($1, `f32.store`, [-1n, value("f32", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:139
assert_trap(() => invoke($1, `f32.store`, [-2n, value("f32", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:140
assert_trap(() => invoke($1, `f32.store`, [-3n, value("f32", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:141
assert_trap(() => invoke($1, `f32.store`, [-4n, value("f32", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:142
assert_trap(() => invoke($1, `f64.store`, [65536n, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:143
assert_trap(() => invoke($1, `f64.store`, [65535n, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:144
assert_trap(() => invoke($1, `f64.store`, [65534n, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:145
assert_trap(() => invoke($1, `f64.store`, [65533n, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:146
assert_trap(() => invoke($1, `f64.store`, [65532n, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:147
assert_trap(() => invoke($1, `f64.store`, [65531n, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:148
assert_trap(() => invoke($1, `f64.store`, [65530n, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:149
assert_trap(() => invoke($1, `f64.store`, [65529n, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:150
assert_trap(() => invoke($1, `f64.store`, [-1n, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:151
assert_trap(() => invoke($1, `f64.store`, [-2n, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:152
assert_trap(() => invoke($1, `f64.store`, [-3n, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:153
assert_trap(() => invoke($1, `f64.store`, [-4n, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:154
assert_trap(() => invoke($1, `f64.store`, [-5n, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:155
assert_trap(() => invoke($1, `f64.store`, [-6n, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:156
assert_trap(() => invoke($1, `f64.store`, [-7n, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:157
assert_trap(() => invoke($1, `f64.store`, [-8n, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:158
assert_trap(() => invoke($1, `i32.store8`, [65536n, 0]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:159
assert_trap(() => invoke($1, `i32.store8`, [-1n, 0]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:160
assert_trap(() => invoke($1, `i32.store16`, [65536n, 0]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:161
assert_trap(() => invoke($1, `i32.store16`, [65535n, 0]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:162
assert_trap(() => invoke($1, `i32.store16`, [-1n, 0]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:163
assert_trap(() => invoke($1, `i32.store16`, [-2n, 0]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:164
assert_trap(() => invoke($1, `i64.store8`, [65536n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:165
assert_trap(() => invoke($1, `i64.store8`, [-1n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:166
assert_trap(() => invoke($1, `i64.store16`, [65536n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:167
assert_trap(() => invoke($1, `i64.store16`, [65535n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:168
assert_trap(() => invoke($1, `i64.store16`, [-1n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:169
assert_trap(() => invoke($1, `i64.store16`, [-2n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:170
assert_trap(() => invoke($1, `i64.store32`, [65536n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:171
assert_trap(() => invoke($1, `i64.store32`, [65535n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:172
assert_trap(() => invoke($1, `i64.store32`, [65534n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:173
assert_trap(() => invoke($1, `i64.store32`, [65533n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:174
assert_trap(() => invoke($1, `i64.store32`, [-1n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:175
assert_trap(() => invoke($1, `i64.store32`, [-2n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:176
assert_trap(() => invoke($1, `i64.store32`, [-3n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:177
assert_trap(() => invoke($1, `i64.store32`, [-4n, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:178
assert_trap(() => invoke($1, `i32.load`, [65536n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:179
assert_trap(() => invoke($1, `i32.load`, [65535n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:180
assert_trap(() => invoke($1, `i32.load`, [65534n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:181
assert_trap(() => invoke($1, `i32.load`, [65533n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:182
assert_trap(() => invoke($1, `i32.load`, [-1n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:183
assert_trap(() => invoke($1, `i32.load`, [-2n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:184
assert_trap(() => invoke($1, `i32.load`, [-3n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:185
assert_trap(() => invoke($1, `i32.load`, [-4n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:186
assert_trap(() => invoke($1, `i64.load`, [65536n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:187
assert_trap(() => invoke($1, `i64.load`, [65535n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:188
assert_trap(() => invoke($1, `i64.load`, [65534n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:189
assert_trap(() => invoke($1, `i64.load`, [65533n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:190
assert_trap(() => invoke($1, `i64.load`, [65532n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:191
assert_trap(() => invoke($1, `i64.load`, [65531n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:192
assert_trap(() => invoke($1, `i64.load`, [65530n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:193
assert_trap(() => invoke($1, `i64.load`, [65529n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:194
assert_trap(() => invoke($1, `i64.load`, [-1n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:195
assert_trap(() => invoke($1, `i64.load`, [-2n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:196
assert_trap(() => invoke($1, `i64.load`, [-3n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:197
assert_trap(() => invoke($1, `i64.load`, [-4n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:198
assert_trap(() => invoke($1, `i64.load`, [-5n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:199
assert_trap(() => invoke($1, `i64.load`, [-6n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:200
assert_trap(() => invoke($1, `i64.load`, [-7n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:201
assert_trap(() => invoke($1, `i64.load`, [-8n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:202
assert_trap(() => invoke($1, `f32.load`, [65536n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:203
assert_trap(() => invoke($1, `f32.load`, [65535n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:204
assert_trap(() => invoke($1, `f32.load`, [65534n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:205
assert_trap(() => invoke($1, `f32.load`, [65533n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:206
assert_trap(() => invoke($1, `f32.load`, [-1n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:207
assert_trap(() => invoke($1, `f32.load`, [-2n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:208
assert_trap(() => invoke($1, `f32.load`, [-3n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:209
assert_trap(() => invoke($1, `f32.load`, [-4n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:210
assert_trap(() => invoke($1, `f64.load`, [65536n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:211
assert_trap(() => invoke($1, `f64.load`, [65535n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:212
assert_trap(() => invoke($1, `f64.load`, [65534n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:213
assert_trap(() => invoke($1, `f64.load`, [65533n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:214
assert_trap(() => invoke($1, `f64.load`, [65532n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:215
assert_trap(() => invoke($1, `f64.load`, [65531n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:216
assert_trap(() => invoke($1, `f64.load`, [65530n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:217
assert_trap(() => invoke($1, `f64.load`, [65529n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:218
assert_trap(() => invoke($1, `f64.load`, [-1n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:219
assert_trap(() => invoke($1, `f64.load`, [-2n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:220
assert_trap(() => invoke($1, `f64.load`, [-3n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:221
assert_trap(() => invoke($1, `f64.load`, [-4n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:222
assert_trap(() => invoke($1, `f64.load`, [-5n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:223
assert_trap(() => invoke($1, `f64.load`, [-6n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:224
assert_trap(() => invoke($1, `f64.load`, [-7n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:225
assert_trap(() => invoke($1, `f64.load`, [-8n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:226
assert_trap(() => invoke($1, `i32.load8_s`, [65536n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:227
assert_trap(() => invoke($1, `i32.load8_s`, [-1n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:228
assert_trap(() => invoke($1, `i32.load8_u`, [65536n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:229
assert_trap(() => invoke($1, `i32.load8_u`, [-1n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:230
assert_trap(() => invoke($1, `i32.load16_s`, [65536n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:231
assert_trap(() => invoke($1, `i32.load16_s`, [65535n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:232
assert_trap(() => invoke($1, `i32.load16_s`, [-1n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:233
assert_trap(() => invoke($1, `i32.load16_s`, [-2n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:234
assert_trap(() => invoke($1, `i32.load16_u`, [65536n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:235
assert_trap(() => invoke($1, `i32.load16_u`, [65535n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:236
assert_trap(() => invoke($1, `i32.load16_u`, [-1n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:237
assert_trap(() => invoke($1, `i32.load16_u`, [-2n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:238
assert_trap(() => invoke($1, `i64.load8_s`, [65536n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:239
assert_trap(() => invoke($1, `i64.load8_s`, [-1n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:240
assert_trap(() => invoke($1, `i64.load8_u`, [65536n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:241
assert_trap(() => invoke($1, `i64.load8_u`, [-1n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:242
assert_trap(() => invoke($1, `i64.load16_s`, [65536n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:243
assert_trap(() => invoke($1, `i64.load16_s`, [65535n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:244
assert_trap(() => invoke($1, `i64.load16_s`, [-1n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:245
assert_trap(() => invoke($1, `i64.load16_s`, [-2n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:246
assert_trap(() => invoke($1, `i64.load16_u`, [65536n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:247
assert_trap(() => invoke($1, `i64.load16_u`, [65535n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:248
assert_trap(() => invoke($1, `i64.load16_u`, [-1n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:249
assert_trap(() => invoke($1, `i64.load16_u`, [-2n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:250
assert_trap(() => invoke($1, `i64.load32_s`, [65536n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:251
assert_trap(() => invoke($1, `i64.load32_s`, [65535n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:252
assert_trap(() => invoke($1, `i64.load32_s`, [65534n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:253
assert_trap(() => invoke($1, `i64.load32_s`, [65533n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:254
assert_trap(() => invoke($1, `i64.load32_s`, [-1n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:255
assert_trap(() => invoke($1, `i64.load32_s`, [-2n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:256
assert_trap(() => invoke($1, `i64.load32_s`, [-3n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:257
assert_trap(() => invoke($1, `i64.load32_s`, [-4n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:258
assert_trap(() => invoke($1, `i64.load32_u`, [65536n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:259
assert_trap(() => invoke($1, `i64.load32_u`, [65535n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:260
assert_trap(() => invoke($1, `i64.load32_u`, [65534n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:261
assert_trap(() => invoke($1, `i64.load32_u`, [65533n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:262
assert_trap(() => invoke($1, `i64.load32_u`, [-1n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:263
assert_trap(() => invoke($1, `i64.load32_u`, [-2n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:264
assert_trap(() => invoke($1, `i64.load32_u`, [-3n]), `out of bounds memory access`);

// ./test/core/memory_trap64.wast:265
assert_trap(() => invoke($1, `i64.load32_u`, [-4n]), `out of bounds memory access`);

// Bug 1737225 - do not observe the partial store caused by bug 1666747 on
// some native platforms.
if (!partialOobWriteMayWritePartialData()) {
    // ./test/core/memory_trap64.wast:268
    assert_return(() => invoke($1, `i64.load`, [65528n]), [
        value("i64", 7523094288207667809n),
    ]);

    // ./test/core/memory_trap64.wast:269
    assert_return(() => invoke($1, `i64.load`, [0n]), [
        value("i64", 7523094288207667809n),
    ]);
}
