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

// ./test/core/multi-memory/address0.wast

// ./test/core/multi-memory/address0.wast:3
let $0 = instantiate(`(module
  (memory $$mem0 0)
  (memory $$mem1 1)
  (data (memory $$mem1) (i32.const 0) "abcdefghijklmnopqrstuvwxyz")

  (func (export "8u_good1") (param $$i i32) (result i32)
    (i32.load8_u $$mem1 offset=0 (local.get $$i))                   ;; 97 'a'
  )
  (func (export "8u_good2") (param $$i i32) (result i32)
    (i32.load8_u $$mem1 align=1 (local.get $$i))                    ;; 97 'a'
  )
  (func (export "8u_good3") (param $$i i32) (result i32)
    (i32.load8_u $$mem1 offset=1 align=1 (local.get $$i))           ;; 98 'b'
  )
  (func (export "8u_good4") (param $$i i32) (result i32)
    (i32.load8_u $$mem1 offset=2 align=1 (local.get $$i))           ;; 99 'c'
  )
  (func (export "8u_good5") (param $$i i32) (result i32)
    (i32.load8_u $$mem1 offset=25 align=1 (local.get $$i))          ;; 122 'z'
  )

  (func (export "8s_good1") (param $$i i32) (result i32)
    (i32.load8_s $$mem1 offset=0 (local.get $$i))                   ;; 97 'a'
  )
  (func (export "8s_good2") (param $$i i32) (result i32)
    (i32.load8_s $$mem1 align=1 (local.get $$i))                    ;; 97 'a'
  )
  (func (export "8s_good3") (param $$i i32) (result i32)
    (i32.load8_s $$mem1 offset=1 align=1 (local.get $$i))           ;; 98 'b'
  )
  (func (export "8s_good4") (param $$i i32) (result i32)
    (i32.load8_s $$mem1 offset=2 align=1 (local.get $$i))           ;; 99 'c'
  )
  (func (export "8s_good5") (param $$i i32) (result i32)
    (i32.load8_s $$mem1 offset=25 align=1 (local.get $$i))          ;; 122 'z'
  )

  (func (export "16u_good1") (param $$i i32) (result i32)
    (i32.load16_u $$mem1 offset=0 (local.get $$i))                  ;; 25185 'ab'
  )
  (func (export "16u_good2") (param $$i i32) (result i32)
    (i32.load16_u $$mem1 align=1 (local.get $$i))                   ;; 25185 'ab'
  )
  (func (export "16u_good3") (param $$i i32) (result i32)
    (i32.load16_u $$mem1 offset=1 align=1 (local.get $$i))          ;; 25442 'bc'
  )
  (func (export "16u_good4") (param $$i i32) (result i32)
    (i32.load16_u $$mem1 offset=2 align=2 (local.get $$i))          ;; 25699 'cd'
  )
  (func (export "16u_good5") (param $$i i32) (result i32)
    (i32.load16_u $$mem1 offset=25 align=2 (local.get $$i))         ;; 122 'z\\0'
  )

  (func (export "16s_good1") (param $$i i32) (result i32)
    (i32.load16_s $$mem1 offset=0 (local.get $$i))                  ;; 25185 'ab'
  )
  (func (export "16s_good2") (param $$i i32) (result i32)
    (i32.load16_s $$mem1 align=1 (local.get $$i))                   ;; 25185 'ab'
  )
  (func (export "16s_good3") (param $$i i32) (result i32)
    (i32.load16_s $$mem1 offset=1 align=1 (local.get $$i))          ;; 25442 'bc'
  )
  (func (export "16s_good4") (param $$i i32) (result i32)
    (i32.load16_s $$mem1 offset=2 align=2 (local.get $$i))          ;; 25699 'cd'
  )
  (func (export "16s_good5") (param $$i i32) (result i32)
    (i32.load16_s $$mem1 offset=25 align=2 (local.get $$i))         ;; 122 'z\\0'
  )

  (func (export "32_good1") (param $$i i32) (result i32)
    (i32.load $$mem1 offset=0 (local.get $$i))                      ;; 1684234849 'abcd'
  )
  (func (export "32_good2") (param $$i i32) (result i32)
    (i32.load $$mem1 align=1 (local.get $$i))                       ;; 1684234849 'abcd'
  )
  (func (export "32_good3") (param $$i i32) (result i32)
    (i32.load $$mem1 offset=1 align=1 (local.get $$i))              ;; 1701077858 'bcde'
  )
  (func (export "32_good4") (param $$i i32) (result i32)
    (i32.load $$mem1 offset=2 align=2 (local.get $$i))              ;; 1717920867 'cdef'
  )
  (func (export "32_good5") (param $$i i32) (result i32)
    (i32.load $$mem1 offset=25 align=4 (local.get $$i))             ;; 122 'z\\0\\0\\0'
  )

  (func (export "8u_bad") (param $$i i32)
    (drop (i32.load8_u $$mem1 offset=4294967295 (local.get $$i)))
  )
  (func (export "8s_bad") (param $$i i32)
    (drop (i32.load8_s $$mem1 offset=4294967295 (local.get $$i)))
  )
  (func (export "16u_bad") (param $$i i32)
    (drop (i32.load16_u $$mem1 offset=4294967295 (local.get $$i)))
  )
  (func (export "16s_bad") (param $$i i32)
    (drop (i32.load16_s $$mem1 offset=4294967295 (local.get $$i)))
  )
  (func (export "32_bad") (param $$i i32)
    (drop (i32.load $$mem1 offset=4294967295 (local.get $$i)))
  )
)`);

// ./test/core/multi-memory/address0.wast:105
assert_return(() => invoke($0, `8u_good1`, [0]), [value("i32", 97)]);

// ./test/core/multi-memory/address0.wast:106
assert_return(() => invoke($0, `8u_good2`, [0]), [value("i32", 97)]);

// ./test/core/multi-memory/address0.wast:107
assert_return(() => invoke($0, `8u_good3`, [0]), [value("i32", 98)]);

// ./test/core/multi-memory/address0.wast:108
assert_return(() => invoke($0, `8u_good4`, [0]), [value("i32", 99)]);

// ./test/core/multi-memory/address0.wast:109
assert_return(() => invoke($0, `8u_good5`, [0]), [value("i32", 122)]);

// ./test/core/multi-memory/address0.wast:111
assert_return(() => invoke($0, `8s_good1`, [0]), [value("i32", 97)]);

// ./test/core/multi-memory/address0.wast:112
assert_return(() => invoke($0, `8s_good2`, [0]), [value("i32", 97)]);

// ./test/core/multi-memory/address0.wast:113
assert_return(() => invoke($0, `8s_good3`, [0]), [value("i32", 98)]);

// ./test/core/multi-memory/address0.wast:114
assert_return(() => invoke($0, `8s_good4`, [0]), [value("i32", 99)]);

// ./test/core/multi-memory/address0.wast:115
assert_return(() => invoke($0, `8s_good5`, [0]), [value("i32", 122)]);

// ./test/core/multi-memory/address0.wast:117
assert_return(() => invoke($0, `16u_good1`, [0]), [value("i32", 25185)]);

// ./test/core/multi-memory/address0.wast:118
assert_return(() => invoke($0, `16u_good2`, [0]), [value("i32", 25185)]);

// ./test/core/multi-memory/address0.wast:119
assert_return(() => invoke($0, `16u_good3`, [0]), [value("i32", 25442)]);

// ./test/core/multi-memory/address0.wast:120
assert_return(() => invoke($0, `16u_good4`, [0]), [value("i32", 25699)]);

// ./test/core/multi-memory/address0.wast:121
assert_return(() => invoke($0, `16u_good5`, [0]), [value("i32", 122)]);

// ./test/core/multi-memory/address0.wast:123
assert_return(() => invoke($0, `16s_good1`, [0]), [value("i32", 25185)]);

// ./test/core/multi-memory/address0.wast:124
assert_return(() => invoke($0, `16s_good2`, [0]), [value("i32", 25185)]);

// ./test/core/multi-memory/address0.wast:125
assert_return(() => invoke($0, `16s_good3`, [0]), [value("i32", 25442)]);

// ./test/core/multi-memory/address0.wast:126
assert_return(() => invoke($0, `16s_good4`, [0]), [value("i32", 25699)]);

// ./test/core/multi-memory/address0.wast:127
assert_return(() => invoke($0, `16s_good5`, [0]), [value("i32", 122)]);

// ./test/core/multi-memory/address0.wast:129
assert_return(() => invoke($0, `32_good1`, [0]), [value("i32", 1684234849)]);

// ./test/core/multi-memory/address0.wast:130
assert_return(() => invoke($0, `32_good2`, [0]), [value("i32", 1684234849)]);

// ./test/core/multi-memory/address0.wast:131
assert_return(() => invoke($0, `32_good3`, [0]), [value("i32", 1701077858)]);

// ./test/core/multi-memory/address0.wast:132
assert_return(() => invoke($0, `32_good4`, [0]), [value("i32", 1717920867)]);

// ./test/core/multi-memory/address0.wast:133
assert_return(() => invoke($0, `32_good5`, [0]), [value("i32", 122)]);

// ./test/core/multi-memory/address0.wast:135
assert_return(() => invoke($0, `8u_good1`, [65507]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:136
assert_return(() => invoke($0, `8u_good2`, [65507]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:137
assert_return(() => invoke($0, `8u_good3`, [65507]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:138
assert_return(() => invoke($0, `8u_good4`, [65507]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:139
assert_return(() => invoke($0, `8u_good5`, [65507]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:141
assert_return(() => invoke($0, `8s_good1`, [65507]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:142
assert_return(() => invoke($0, `8s_good2`, [65507]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:143
assert_return(() => invoke($0, `8s_good3`, [65507]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:144
assert_return(() => invoke($0, `8s_good4`, [65507]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:145
assert_return(() => invoke($0, `8s_good5`, [65507]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:147
assert_return(() => invoke($0, `16u_good1`, [65507]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:148
assert_return(() => invoke($0, `16u_good2`, [65507]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:149
assert_return(() => invoke($0, `16u_good3`, [65507]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:150
assert_return(() => invoke($0, `16u_good4`, [65507]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:151
assert_return(() => invoke($0, `16u_good5`, [65507]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:153
assert_return(() => invoke($0, `16s_good1`, [65507]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:154
assert_return(() => invoke($0, `16s_good2`, [65507]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:155
assert_return(() => invoke($0, `16s_good3`, [65507]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:156
assert_return(() => invoke($0, `16s_good4`, [65507]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:157
assert_return(() => invoke($0, `16s_good5`, [65507]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:159
assert_return(() => invoke($0, `32_good1`, [65507]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:160
assert_return(() => invoke($0, `32_good2`, [65507]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:161
assert_return(() => invoke($0, `32_good3`, [65507]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:162
assert_return(() => invoke($0, `32_good4`, [65507]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:163
assert_return(() => invoke($0, `32_good5`, [65507]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:165
assert_return(() => invoke($0, `8u_good1`, [65508]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:166
assert_return(() => invoke($0, `8u_good2`, [65508]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:167
assert_return(() => invoke($0, `8u_good3`, [65508]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:168
assert_return(() => invoke($0, `8u_good4`, [65508]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:169
assert_return(() => invoke($0, `8u_good5`, [65508]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:171
assert_return(() => invoke($0, `8s_good1`, [65508]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:172
assert_return(() => invoke($0, `8s_good2`, [65508]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:173
assert_return(() => invoke($0, `8s_good3`, [65508]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:174
assert_return(() => invoke($0, `8s_good4`, [65508]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:175
assert_return(() => invoke($0, `8s_good5`, [65508]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:177
assert_return(() => invoke($0, `16u_good1`, [65508]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:178
assert_return(() => invoke($0, `16u_good2`, [65508]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:179
assert_return(() => invoke($0, `16u_good3`, [65508]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:180
assert_return(() => invoke($0, `16u_good4`, [65508]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:181
assert_return(() => invoke($0, `16u_good5`, [65508]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:183
assert_return(() => invoke($0, `16s_good1`, [65508]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:184
assert_return(() => invoke($0, `16s_good2`, [65508]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:185
assert_return(() => invoke($0, `16s_good3`, [65508]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:186
assert_return(() => invoke($0, `16s_good4`, [65508]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:187
assert_return(() => invoke($0, `16s_good5`, [65508]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:189
assert_return(() => invoke($0, `32_good1`, [65508]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:190
assert_return(() => invoke($0, `32_good2`, [65508]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:191
assert_return(() => invoke($0, `32_good3`, [65508]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:192
assert_return(() => invoke($0, `32_good4`, [65508]), [value("i32", 0)]);

// ./test/core/multi-memory/address0.wast:193
assert_trap(() => invoke($0, `32_good5`, [65508]), `out of bounds memory access`);

// ./test/core/multi-memory/address0.wast:195
assert_trap(() => invoke($0, `8u_good3`, [-1]), `out of bounds memory access`);

// ./test/core/multi-memory/address0.wast:196
assert_trap(() => invoke($0, `8s_good3`, [-1]), `out of bounds memory access`);

// ./test/core/multi-memory/address0.wast:197
assert_trap(() => invoke($0, `16u_good3`, [-1]), `out of bounds memory access`);

// ./test/core/multi-memory/address0.wast:198
assert_trap(() => invoke($0, `16s_good3`, [-1]), `out of bounds memory access`);

// ./test/core/multi-memory/address0.wast:199
assert_trap(() => invoke($0, `32_good3`, [-1]), `out of bounds memory access`);

// ./test/core/multi-memory/address0.wast:200
assert_trap(() => invoke($0, `32_good3`, [-1]), `out of bounds memory access`);

// ./test/core/multi-memory/address0.wast:202
assert_trap(() => invoke($0, `8u_bad`, [0]), `out of bounds memory access`);

// ./test/core/multi-memory/address0.wast:203
assert_trap(() => invoke($0, `8s_bad`, [0]), `out of bounds memory access`);

// ./test/core/multi-memory/address0.wast:204
assert_trap(() => invoke($0, `16u_bad`, [0]), `out of bounds memory access`);

// ./test/core/multi-memory/address0.wast:205
assert_trap(() => invoke($0, `16s_bad`, [0]), `out of bounds memory access`);

// ./test/core/multi-memory/address0.wast:206
assert_trap(() => invoke($0, `32_bad`, [0]), `out of bounds memory access`);

// ./test/core/multi-memory/address0.wast:208
assert_trap(() => invoke($0, `8u_bad`, [1]), `out of bounds memory access`);

// ./test/core/multi-memory/address0.wast:209
assert_trap(() => invoke($0, `8s_bad`, [1]), `out of bounds memory access`);

// ./test/core/multi-memory/address0.wast:210
assert_trap(() => invoke($0, `16u_bad`, [1]), `out of bounds memory access`);

// ./test/core/multi-memory/address0.wast:211
assert_trap(() => invoke($0, `16s_bad`, [1]), `out of bounds memory access`);

// ./test/core/multi-memory/address0.wast:212
assert_trap(() => invoke($0, `32_bad`, [1]), `out of bounds memory access`);
