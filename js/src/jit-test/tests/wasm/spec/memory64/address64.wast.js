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

// ./test/core/address64.wast

// ./test/core/address64.wast:3
let $0 = instantiate(`(module
  (memory i64 1)
  (data (i64.const 0) "abcdefghijklmnopqrstuvwxyz")

  (func (export "8u_good1") (param $$i i64) (result i32)
    (i32.load8_u offset=0 (local.get $$i))                   ;; 97 'a'
  )
  (func (export "8u_good2") (param $$i i64) (result i32)
    (i32.load8_u align=1 (local.get $$i))                    ;; 97 'a'
  )
  (func (export "8u_good3") (param $$i i64) (result i32)
    (i32.load8_u offset=1 align=1 (local.get $$i))           ;; 98 'b'
  )
  (func (export "8u_good4") (param $$i i64) (result i32)
    (i32.load8_u offset=2 align=1 (local.get $$i))           ;; 99 'c'
  )
  (func (export "8u_good5") (param $$i i64) (result i32)
    (i32.load8_u offset=25 align=1 (local.get $$i))          ;; 122 'z'
  )

  (func (export "8s_good1") (param $$i i64) (result i32)
    (i32.load8_s offset=0 (local.get $$i))                   ;; 97 'a'
  )
  (func (export "8s_good2") (param $$i i64) (result i32)
    (i32.load8_s align=1 (local.get $$i))                    ;; 97 'a'
  )
  (func (export "8s_good3") (param $$i i64) (result i32)
    (i32.load8_s offset=1 align=1 (local.get $$i))           ;; 98 'b'
  )
  (func (export "8s_good4") (param $$i i64) (result i32)
    (i32.load8_s offset=2 align=1 (local.get $$i))           ;; 99 'c'
  )
  (func (export "8s_good5") (param $$i i64) (result i32)
    (i32.load8_s offset=25 align=1 (local.get $$i))          ;; 122 'z'
  )

  (func (export "16u_good1") (param $$i i64) (result i32)
    (i32.load16_u offset=0 (local.get $$i))                  ;; 25185 'ab'
  )
  (func (export "16u_good2") (param $$i i64) (result i32)
    (i32.load16_u align=1 (local.get $$i))                   ;; 25185 'ab'
  )
  (func (export "16u_good3") (param $$i i64) (result i32)
    (i32.load16_u offset=1 align=1 (local.get $$i))          ;; 25442 'bc'
  )
  (func (export "16u_good4") (param $$i i64) (result i32)
    (i32.load16_u offset=2 align=2 (local.get $$i))          ;; 25699 'cd'
  )
  (func (export "16u_good5") (param $$i i64) (result i32)
    (i32.load16_u offset=25 align=2 (local.get $$i))         ;; 122 'z\\0'
  )

  (func (export "16s_good1") (param $$i i64) (result i32)
    (i32.load16_s offset=0 (local.get $$i))                  ;; 25185 'ab'
  )
  (func (export "16s_good2") (param $$i i64) (result i32)
    (i32.load16_s align=1 (local.get $$i))                   ;; 25185 'ab'
  )
  (func (export "16s_good3") (param $$i i64) (result i32)
    (i32.load16_s offset=1 align=1 (local.get $$i))          ;; 25442 'bc'
  )
  (func (export "16s_good4") (param $$i i64) (result i32)
    (i32.load16_s offset=2 align=2 (local.get $$i))          ;; 25699 'cd'
  )
  (func (export "16s_good5") (param $$i i64) (result i32)
    (i32.load16_s offset=25 align=2 (local.get $$i))         ;; 122 'z\\0'
  )

  (func (export "32_good1") (param $$i i64) (result i32)
    (i32.load offset=0 (local.get $$i))                      ;; 1684234849 'abcd'
  )
  (func (export "32_good2") (param $$i i64) (result i32)
    (i32.load align=1 (local.get $$i))                       ;; 1684234849 'abcd'
  )
  (func (export "32_good3") (param $$i i64) (result i32)
    (i32.load offset=1 align=1 (local.get $$i))              ;; 1701077858 'bcde'
  )
  (func (export "32_good4") (param $$i i64) (result i32)
    (i32.load offset=2 align=2 (local.get $$i))              ;; 1717920867 'cdef'
  )
  (func (export "32_good5") (param $$i i64) (result i32)
    (i32.load offset=25 align=4 (local.get $$i))             ;; 122 'z\\0\\0\\0'
  )

  (func (export "8u_bad") (param $$i i64)
    (drop (i32.load8_u offset=4294967295 (local.get $$i)))
  )
  (func (export "8s_bad") (param $$i i64)
    (drop (i32.load8_s offset=4294967295 (local.get $$i)))
  )
  (func (export "16u_bad") (param $$i i64)
    (drop (i32.load16_u offset=4294967295 (local.get $$i)))
  )
  (func (export "16s_bad") (param $$i i64)
    (drop (i32.load16_s offset=4294967295 (local.get $$i)))
  )
  (func (export "32_bad") (param $$i i64)
    (drop (i32.load offset=4294967295 (local.get $$i)))
  )
)`);

// ./test/core/address64.wast:104
assert_return(() => invoke($0, `8u_good1`, [0n]), [value("i32", 97)]);

// ./test/core/address64.wast:105
assert_return(() => invoke($0, `8u_good2`, [0n]), [value("i32", 97)]);

// ./test/core/address64.wast:106
assert_return(() => invoke($0, `8u_good3`, [0n]), [value("i32", 98)]);

// ./test/core/address64.wast:107
assert_return(() => invoke($0, `8u_good4`, [0n]), [value("i32", 99)]);

// ./test/core/address64.wast:108
assert_return(() => invoke($0, `8u_good5`, [0n]), [value("i32", 122)]);

// ./test/core/address64.wast:110
assert_return(() => invoke($0, `8s_good1`, [0n]), [value("i32", 97)]);

// ./test/core/address64.wast:111
assert_return(() => invoke($0, `8s_good2`, [0n]), [value("i32", 97)]);

// ./test/core/address64.wast:112
assert_return(() => invoke($0, `8s_good3`, [0n]), [value("i32", 98)]);

// ./test/core/address64.wast:113
assert_return(() => invoke($0, `8s_good4`, [0n]), [value("i32", 99)]);

// ./test/core/address64.wast:114
assert_return(() => invoke($0, `8s_good5`, [0n]), [value("i32", 122)]);

// ./test/core/address64.wast:116
assert_return(() => invoke($0, `16u_good1`, [0n]), [value("i32", 25185)]);

// ./test/core/address64.wast:117
assert_return(() => invoke($0, `16u_good2`, [0n]), [value("i32", 25185)]);

// ./test/core/address64.wast:118
assert_return(() => invoke($0, `16u_good3`, [0n]), [value("i32", 25442)]);

// ./test/core/address64.wast:119
assert_return(() => invoke($0, `16u_good4`, [0n]), [value("i32", 25699)]);

// ./test/core/address64.wast:120
assert_return(() => invoke($0, `16u_good5`, [0n]), [value("i32", 122)]);

// ./test/core/address64.wast:122
assert_return(() => invoke($0, `16s_good1`, [0n]), [value("i32", 25185)]);

// ./test/core/address64.wast:123
assert_return(() => invoke($0, `16s_good2`, [0n]), [value("i32", 25185)]);

// ./test/core/address64.wast:124
assert_return(() => invoke($0, `16s_good3`, [0n]), [value("i32", 25442)]);

// ./test/core/address64.wast:125
assert_return(() => invoke($0, `16s_good4`, [0n]), [value("i32", 25699)]);

// ./test/core/address64.wast:126
assert_return(() => invoke($0, `16s_good5`, [0n]), [value("i32", 122)]);

// ./test/core/address64.wast:128
assert_return(() => invoke($0, `32_good1`, [0n]), [value("i32", 1684234849)]);

// ./test/core/address64.wast:129
assert_return(() => invoke($0, `32_good2`, [0n]), [value("i32", 1684234849)]);

// ./test/core/address64.wast:130
assert_return(() => invoke($0, `32_good3`, [0n]), [value("i32", 1701077858)]);

// ./test/core/address64.wast:131
assert_return(() => invoke($0, `32_good4`, [0n]), [value("i32", 1717920867)]);

// ./test/core/address64.wast:132
assert_return(() => invoke($0, `32_good5`, [0n]), [value("i32", 122)]);

// ./test/core/address64.wast:134
assert_return(() => invoke($0, `8u_good1`, [65507n]), [value("i32", 0)]);

// ./test/core/address64.wast:135
assert_return(() => invoke($0, `8u_good2`, [65507n]), [value("i32", 0)]);

// ./test/core/address64.wast:136
assert_return(() => invoke($0, `8u_good3`, [65507n]), [value("i32", 0)]);

// ./test/core/address64.wast:137
assert_return(() => invoke($0, `8u_good4`, [65507n]), [value("i32", 0)]);

// ./test/core/address64.wast:138
assert_return(() => invoke($0, `8u_good5`, [65507n]), [value("i32", 0)]);

// ./test/core/address64.wast:140
assert_return(() => invoke($0, `8s_good1`, [65507n]), [value("i32", 0)]);

// ./test/core/address64.wast:141
assert_return(() => invoke($0, `8s_good2`, [65507n]), [value("i32", 0)]);

// ./test/core/address64.wast:142
assert_return(() => invoke($0, `8s_good3`, [65507n]), [value("i32", 0)]);

// ./test/core/address64.wast:143
assert_return(() => invoke($0, `8s_good4`, [65507n]), [value("i32", 0)]);

// ./test/core/address64.wast:144
assert_return(() => invoke($0, `8s_good5`, [65507n]), [value("i32", 0)]);

// ./test/core/address64.wast:146
assert_return(() => invoke($0, `16u_good1`, [65507n]), [value("i32", 0)]);

// ./test/core/address64.wast:147
assert_return(() => invoke($0, `16u_good2`, [65507n]), [value("i32", 0)]);

// ./test/core/address64.wast:148
assert_return(() => invoke($0, `16u_good3`, [65507n]), [value("i32", 0)]);

// ./test/core/address64.wast:149
assert_return(() => invoke($0, `16u_good4`, [65507n]), [value("i32", 0)]);

// ./test/core/address64.wast:150
assert_return(() => invoke($0, `16u_good5`, [65507n]), [value("i32", 0)]);

// ./test/core/address64.wast:152
assert_return(() => invoke($0, `16s_good1`, [65507n]), [value("i32", 0)]);

// ./test/core/address64.wast:153
assert_return(() => invoke($0, `16s_good2`, [65507n]), [value("i32", 0)]);

// ./test/core/address64.wast:154
assert_return(() => invoke($0, `16s_good3`, [65507n]), [value("i32", 0)]);

// ./test/core/address64.wast:155
assert_return(() => invoke($0, `16s_good4`, [65507n]), [value("i32", 0)]);

// ./test/core/address64.wast:156
assert_return(() => invoke($0, `16s_good5`, [65507n]), [value("i32", 0)]);

// ./test/core/address64.wast:158
assert_return(() => invoke($0, `32_good1`, [65507n]), [value("i32", 0)]);

// ./test/core/address64.wast:159
assert_return(() => invoke($0, `32_good2`, [65507n]), [value("i32", 0)]);

// ./test/core/address64.wast:160
assert_return(() => invoke($0, `32_good3`, [65507n]), [value("i32", 0)]);

// ./test/core/address64.wast:161
assert_return(() => invoke($0, `32_good4`, [65507n]), [value("i32", 0)]);

// ./test/core/address64.wast:162
assert_return(() => invoke($0, `32_good5`, [65507n]), [value("i32", 0)]);

// ./test/core/address64.wast:164
assert_return(() => invoke($0, `8u_good1`, [65508n]), [value("i32", 0)]);

// ./test/core/address64.wast:165
assert_return(() => invoke($0, `8u_good2`, [65508n]), [value("i32", 0)]);

// ./test/core/address64.wast:166
assert_return(() => invoke($0, `8u_good3`, [65508n]), [value("i32", 0)]);

// ./test/core/address64.wast:167
assert_return(() => invoke($0, `8u_good4`, [65508n]), [value("i32", 0)]);

// ./test/core/address64.wast:168
assert_return(() => invoke($0, `8u_good5`, [65508n]), [value("i32", 0)]);

// ./test/core/address64.wast:170
assert_return(() => invoke($0, `8s_good1`, [65508n]), [value("i32", 0)]);

// ./test/core/address64.wast:171
assert_return(() => invoke($0, `8s_good2`, [65508n]), [value("i32", 0)]);

// ./test/core/address64.wast:172
assert_return(() => invoke($0, `8s_good3`, [65508n]), [value("i32", 0)]);

// ./test/core/address64.wast:173
assert_return(() => invoke($0, `8s_good4`, [65508n]), [value("i32", 0)]);

// ./test/core/address64.wast:174
assert_return(() => invoke($0, `8s_good5`, [65508n]), [value("i32", 0)]);

// ./test/core/address64.wast:176
assert_return(() => invoke($0, `16u_good1`, [65508n]), [value("i32", 0)]);

// ./test/core/address64.wast:177
assert_return(() => invoke($0, `16u_good2`, [65508n]), [value("i32", 0)]);

// ./test/core/address64.wast:178
assert_return(() => invoke($0, `16u_good3`, [65508n]), [value("i32", 0)]);

// ./test/core/address64.wast:179
assert_return(() => invoke($0, `16u_good4`, [65508n]), [value("i32", 0)]);

// ./test/core/address64.wast:180
assert_return(() => invoke($0, `16u_good5`, [65508n]), [value("i32", 0)]);

// ./test/core/address64.wast:182
assert_return(() => invoke($0, `16s_good1`, [65508n]), [value("i32", 0)]);

// ./test/core/address64.wast:183
assert_return(() => invoke($0, `16s_good2`, [65508n]), [value("i32", 0)]);

// ./test/core/address64.wast:184
assert_return(() => invoke($0, `16s_good3`, [65508n]), [value("i32", 0)]);

// ./test/core/address64.wast:185
assert_return(() => invoke($0, `16s_good4`, [65508n]), [value("i32", 0)]);

// ./test/core/address64.wast:186
assert_return(() => invoke($0, `16s_good5`, [65508n]), [value("i32", 0)]);

// ./test/core/address64.wast:188
assert_return(() => invoke($0, `32_good1`, [65508n]), [value("i32", 0)]);

// ./test/core/address64.wast:189
assert_return(() => invoke($0, `32_good2`, [65508n]), [value("i32", 0)]);

// ./test/core/address64.wast:190
assert_return(() => invoke($0, `32_good3`, [65508n]), [value("i32", 0)]);

// ./test/core/address64.wast:191
assert_return(() => invoke($0, `32_good4`, [65508n]), [value("i32", 0)]);

// ./test/core/address64.wast:192
assert_trap(() => invoke($0, `32_good5`, [65508n]), `out of bounds memory access`);

// ./test/core/address64.wast:194
assert_trap(() => invoke($0, `8u_bad`, [0n]), `out of bounds memory access`);

// ./test/core/address64.wast:195
assert_trap(() => invoke($0, `8s_bad`, [0n]), `out of bounds memory access`);

// ./test/core/address64.wast:196
assert_trap(() => invoke($0, `16u_bad`, [0n]), `out of bounds memory access`);

// ./test/core/address64.wast:197
assert_trap(() => invoke($0, `16s_bad`, [0n]), `out of bounds memory access`);

// ./test/core/address64.wast:198
assert_trap(() => invoke($0, `32_bad`, [0n]), `out of bounds memory access`);

// ./test/core/address64.wast:200
assert_trap(() => invoke($0, `8u_bad`, [1n]), `out of bounds memory access`);

// ./test/core/address64.wast:201
assert_trap(() => invoke($0, `8s_bad`, [1n]), `out of bounds memory access`);

// ./test/core/address64.wast:202
assert_trap(() => invoke($0, `16u_bad`, [1n]), `out of bounds memory access`);

// ./test/core/address64.wast:203
assert_trap(() => invoke($0, `16s_bad`, [1n]), `out of bounds memory access`);

// ./test/core/address64.wast:204
assert_trap(() => invoke($0, `32_bad`, [1n]), `out of bounds memory access`);

// ./test/core/address64.wast:209
let $1 = instantiate(`(module
  (memory i64 1)
  (data (i64.const 0) "abcdefghijklmnopqrstuvwxyz")

  (func (export "8u_good1") (param $$i i64) (result i64)
    (i64.load8_u offset=0 (local.get $$i))                   ;; 97 'a'
  )
  (func (export "8u_good2") (param $$i i64) (result i64)
    (i64.load8_u align=1 (local.get $$i))                    ;; 97 'a'
  )
  (func (export "8u_good3") (param $$i i64) (result i64)
    (i64.load8_u offset=1 align=1 (local.get $$i))           ;; 98 'b'
  )
  (func (export "8u_good4") (param $$i i64) (result i64)
    (i64.load8_u offset=2 align=1 (local.get $$i))           ;; 99 'c'
  )
  (func (export "8u_good5") (param $$i i64) (result i64)
    (i64.load8_u offset=25 align=1 (local.get $$i))          ;; 122 'z'
  )

  (func (export "8s_good1") (param $$i i64) (result i64)
    (i64.load8_s offset=0 (local.get $$i))                   ;; 97 'a'
  )
  (func (export "8s_good2") (param $$i i64) (result i64)
    (i64.load8_s align=1 (local.get $$i))                    ;; 97 'a'
  )
  (func (export "8s_good3") (param $$i i64) (result i64)
    (i64.load8_s offset=1 align=1 (local.get $$i))           ;; 98 'b'
  )
  (func (export "8s_good4") (param $$i i64) (result i64)
    (i64.load8_s offset=2 align=1 (local.get $$i))           ;; 99 'c'
  )
  (func (export "8s_good5") (param $$i i64) (result i64)
    (i64.load8_s offset=25 align=1 (local.get $$i))          ;; 122 'z'
  )

  (func (export "16u_good1") (param $$i i64) (result i64)
    (i64.load16_u offset=0 (local.get $$i))                 ;; 25185 'ab'
  )
  (func (export "16u_good2") (param $$i i64) (result i64)
    (i64.load16_u align=1 (local.get $$i))                  ;; 25185 'ab'
  )
  (func (export "16u_good3") (param $$i i64) (result i64)
    (i64.load16_u offset=1 align=1 (local.get $$i))         ;; 25442 'bc'
  )
  (func (export "16u_good4") (param $$i i64) (result i64)
    (i64.load16_u offset=2 align=2 (local.get $$i))         ;; 25699 'cd'
  )
  (func (export "16u_good5") (param $$i i64) (result i64)
    (i64.load16_u offset=25 align=2 (local.get $$i))        ;; 122 'z\\0'
  )

  (func (export "16s_good1") (param $$i i64) (result i64)
    (i64.load16_s offset=0 (local.get $$i))                 ;; 25185 'ab'
  )
  (func (export "16s_good2") (param $$i i64) (result i64)
    (i64.load16_s align=1 (local.get $$i))                  ;; 25185 'ab'
  )
  (func (export "16s_good3") (param $$i i64) (result i64)
    (i64.load16_s offset=1 align=1 (local.get $$i))         ;; 25442 'bc'
  )
  (func (export "16s_good4") (param $$i i64) (result i64)
    (i64.load16_s offset=2 align=2 (local.get $$i))         ;; 25699 'cd'
  )
  (func (export "16s_good5") (param $$i i64) (result i64)
    (i64.load16_s offset=25 align=2 (local.get $$i))        ;; 122 'z\\0'
  )

  (func (export "32u_good1") (param $$i i64) (result i64)
    (i64.load32_u offset=0 (local.get $$i))                 ;; 1684234849 'abcd'
  )
  (func (export "32u_good2") (param $$i i64) (result i64)
    (i64.load32_u align=1 (local.get $$i))                  ;; 1684234849 'abcd'
  )
  (func (export "32u_good3") (param $$i i64) (result i64)
    (i64.load32_u offset=1 align=1 (local.get $$i))         ;; 1701077858 'bcde'
  )
  (func (export "32u_good4") (param $$i i64) (result i64)
    (i64.load32_u offset=2 align=2 (local.get $$i))         ;; 1717920867 'cdef'
  )
  (func (export "32u_good5") (param $$i i64) (result i64)
    (i64.load32_u offset=25 align=4 (local.get $$i))        ;; 122 'z\\0\\0\\0'
  )

  (func (export "32s_good1") (param $$i i64) (result i64)
    (i64.load32_s offset=0 (local.get $$i))                 ;; 1684234849 'abcd'
  )
  (func (export "32s_good2") (param $$i i64) (result i64)
    (i64.load32_s align=1 (local.get $$i))                  ;; 1684234849 'abcd'
  )
  (func (export "32s_good3") (param $$i i64) (result i64)
    (i64.load32_s offset=1 align=1 (local.get $$i))         ;; 1701077858 'bcde'
  )
  (func (export "32s_good4") (param $$i i64) (result i64)
    (i64.load32_s offset=2 align=2 (local.get $$i))         ;; 1717920867 'cdef'
  )
  (func (export "32s_good5") (param $$i i64) (result i64)
    (i64.load32_s offset=25 align=4 (local.get $$i))        ;; 122 'z\\0\\0\\0'
  )

  (func (export "64_good1") (param $$i i64) (result i64)
    (i64.load offset=0 (local.get $$i))                     ;; 0x6867666564636261 'abcdefgh'
  )
  (func (export "64_good2") (param $$i i64) (result i64)
    (i64.load align=1 (local.get $$i))                      ;; 0x6867666564636261 'abcdefgh'
  )
  (func (export "64_good3") (param $$i i64) (result i64)
    (i64.load offset=1 align=1 (local.get $$i))             ;; 0x6968676665646362 'bcdefghi'
  )
  (func (export "64_good4") (param $$i i64) (result i64)
    (i64.load offset=2 align=2 (local.get $$i))             ;; 0x6a69686766656463 'cdefghij'
  )
  (func (export "64_good5") (param $$i i64) (result i64)
    (i64.load offset=25 align=8 (local.get $$i))            ;; 122 'z\\0\\0\\0\\0\\0\\0\\0'
  )

  (func (export "8u_bad") (param $$i i64)
    (drop (i64.load8_u offset=4294967295 (local.get $$i)))
  )
  (func (export "8s_bad") (param $$i i64)
    (drop (i64.load8_s offset=4294967295 (local.get $$i)))
  )
  (func (export "16u_bad") (param $$i i64)
    (drop (i64.load16_u offset=4294967295 (local.get $$i)))
  )
  (func (export "16s_bad") (param $$i i64)
    (drop (i64.load16_s offset=4294967295 (local.get $$i)))
  )
  (func (export "32u_bad") (param $$i i64)
    (drop (i64.load32_u offset=4294967295 (local.get $$i)))
  )
  (func (export "32s_bad") (param $$i i64)
    (drop (i64.load32_s offset=4294967295 (local.get $$i)))
  )
  (func (export "64_bad") (param $$i i64)
    (drop (i64.load offset=4294967295 (local.get $$i)))
  )
)`);

// ./test/core/address64.wast:348
assert_return(() => invoke($1, `8u_good1`, [0n]), [value("i64", 97n)]);

// ./test/core/address64.wast:349
assert_return(() => invoke($1, `8u_good2`, [0n]), [value("i64", 97n)]);

// ./test/core/address64.wast:350
assert_return(() => invoke($1, `8u_good3`, [0n]), [value("i64", 98n)]);

// ./test/core/address64.wast:351
assert_return(() => invoke($1, `8u_good4`, [0n]), [value("i64", 99n)]);

// ./test/core/address64.wast:352
assert_return(() => invoke($1, `8u_good5`, [0n]), [value("i64", 122n)]);

// ./test/core/address64.wast:354
assert_return(() => invoke($1, `8s_good1`, [0n]), [value("i64", 97n)]);

// ./test/core/address64.wast:355
assert_return(() => invoke($1, `8s_good2`, [0n]), [value("i64", 97n)]);

// ./test/core/address64.wast:356
assert_return(() => invoke($1, `8s_good3`, [0n]), [value("i64", 98n)]);

// ./test/core/address64.wast:357
assert_return(() => invoke($1, `8s_good4`, [0n]), [value("i64", 99n)]);

// ./test/core/address64.wast:358
assert_return(() => invoke($1, `8s_good5`, [0n]), [value("i64", 122n)]);

// ./test/core/address64.wast:360
assert_return(() => invoke($1, `16u_good1`, [0n]), [value("i64", 25185n)]);

// ./test/core/address64.wast:361
assert_return(() => invoke($1, `16u_good2`, [0n]), [value("i64", 25185n)]);

// ./test/core/address64.wast:362
assert_return(() => invoke($1, `16u_good3`, [0n]), [value("i64", 25442n)]);

// ./test/core/address64.wast:363
assert_return(() => invoke($1, `16u_good4`, [0n]), [value("i64", 25699n)]);

// ./test/core/address64.wast:364
assert_return(() => invoke($1, `16u_good5`, [0n]), [value("i64", 122n)]);

// ./test/core/address64.wast:366
assert_return(() => invoke($1, `16s_good1`, [0n]), [value("i64", 25185n)]);

// ./test/core/address64.wast:367
assert_return(() => invoke($1, `16s_good2`, [0n]), [value("i64", 25185n)]);

// ./test/core/address64.wast:368
assert_return(() => invoke($1, `16s_good3`, [0n]), [value("i64", 25442n)]);

// ./test/core/address64.wast:369
assert_return(() => invoke($1, `16s_good4`, [0n]), [value("i64", 25699n)]);

// ./test/core/address64.wast:370
assert_return(() => invoke($1, `16s_good5`, [0n]), [value("i64", 122n)]);

// ./test/core/address64.wast:372
assert_return(() => invoke($1, `32u_good1`, [0n]), [value("i64", 1684234849n)]);

// ./test/core/address64.wast:373
assert_return(() => invoke($1, `32u_good2`, [0n]), [value("i64", 1684234849n)]);

// ./test/core/address64.wast:374
assert_return(() => invoke($1, `32u_good3`, [0n]), [value("i64", 1701077858n)]);

// ./test/core/address64.wast:375
assert_return(() => invoke($1, `32u_good4`, [0n]), [value("i64", 1717920867n)]);

// ./test/core/address64.wast:376
assert_return(() => invoke($1, `32u_good5`, [0n]), [value("i64", 122n)]);

// ./test/core/address64.wast:378
assert_return(() => invoke($1, `32s_good1`, [0n]), [value("i64", 1684234849n)]);

// ./test/core/address64.wast:379
assert_return(() => invoke($1, `32s_good2`, [0n]), [value("i64", 1684234849n)]);

// ./test/core/address64.wast:380
assert_return(() => invoke($1, `32s_good3`, [0n]), [value("i64", 1701077858n)]);

// ./test/core/address64.wast:381
assert_return(() => invoke($1, `32s_good4`, [0n]), [value("i64", 1717920867n)]);

// ./test/core/address64.wast:382
assert_return(() => invoke($1, `32s_good5`, [0n]), [value("i64", 122n)]);

// ./test/core/address64.wast:384
assert_return(() => invoke($1, `64_good1`, [0n]), [value("i64", 7523094288207667809n)]);

// ./test/core/address64.wast:385
assert_return(() => invoke($1, `64_good2`, [0n]), [value("i64", 7523094288207667809n)]);

// ./test/core/address64.wast:386
assert_return(() => invoke($1, `64_good3`, [0n]), [value("i64", 7595434461045744482n)]);

// ./test/core/address64.wast:387
assert_return(() => invoke($1, `64_good4`, [0n]), [value("i64", 7667774633883821155n)]);

// ./test/core/address64.wast:388
assert_return(() => invoke($1, `64_good5`, [0n]), [value("i64", 122n)]);

// ./test/core/address64.wast:390
assert_return(() => invoke($1, `8u_good1`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:391
assert_return(() => invoke($1, `8u_good2`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:392
assert_return(() => invoke($1, `8u_good3`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:393
assert_return(() => invoke($1, `8u_good4`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:394
assert_return(() => invoke($1, `8u_good5`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:396
assert_return(() => invoke($1, `8s_good1`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:397
assert_return(() => invoke($1, `8s_good2`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:398
assert_return(() => invoke($1, `8s_good3`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:399
assert_return(() => invoke($1, `8s_good4`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:400
assert_return(() => invoke($1, `8s_good5`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:402
assert_return(() => invoke($1, `16u_good1`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:403
assert_return(() => invoke($1, `16u_good2`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:404
assert_return(() => invoke($1, `16u_good3`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:405
assert_return(() => invoke($1, `16u_good4`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:406
assert_return(() => invoke($1, `16u_good5`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:408
assert_return(() => invoke($1, `16s_good1`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:409
assert_return(() => invoke($1, `16s_good2`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:410
assert_return(() => invoke($1, `16s_good3`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:411
assert_return(() => invoke($1, `16s_good4`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:412
assert_return(() => invoke($1, `16s_good5`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:414
assert_return(() => invoke($1, `32u_good1`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:415
assert_return(() => invoke($1, `32u_good2`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:416
assert_return(() => invoke($1, `32u_good3`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:417
assert_return(() => invoke($1, `32u_good4`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:418
assert_return(() => invoke($1, `32u_good5`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:420
assert_return(() => invoke($1, `32s_good1`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:421
assert_return(() => invoke($1, `32s_good2`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:422
assert_return(() => invoke($1, `32s_good3`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:423
assert_return(() => invoke($1, `32s_good4`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:424
assert_return(() => invoke($1, `32s_good5`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:426
assert_return(() => invoke($1, `64_good1`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:427
assert_return(() => invoke($1, `64_good2`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:428
assert_return(() => invoke($1, `64_good3`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:429
assert_return(() => invoke($1, `64_good4`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:430
assert_return(() => invoke($1, `64_good5`, [65503n]), [value("i64", 0n)]);

// ./test/core/address64.wast:432
assert_return(() => invoke($1, `8u_good1`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:433
assert_return(() => invoke($1, `8u_good2`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:434
assert_return(() => invoke($1, `8u_good3`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:435
assert_return(() => invoke($1, `8u_good4`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:436
assert_return(() => invoke($1, `8u_good5`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:438
assert_return(() => invoke($1, `8s_good1`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:439
assert_return(() => invoke($1, `8s_good2`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:440
assert_return(() => invoke($1, `8s_good3`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:441
assert_return(() => invoke($1, `8s_good4`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:442
assert_return(() => invoke($1, `8s_good5`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:444
assert_return(() => invoke($1, `16u_good1`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:445
assert_return(() => invoke($1, `16u_good2`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:446
assert_return(() => invoke($1, `16u_good3`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:447
assert_return(() => invoke($1, `16u_good4`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:448
assert_return(() => invoke($1, `16u_good5`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:450
assert_return(() => invoke($1, `16s_good1`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:451
assert_return(() => invoke($1, `16s_good2`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:452
assert_return(() => invoke($1, `16s_good3`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:453
assert_return(() => invoke($1, `16s_good4`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:454
assert_return(() => invoke($1, `16s_good5`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:456
assert_return(() => invoke($1, `32u_good1`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:457
assert_return(() => invoke($1, `32u_good2`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:458
assert_return(() => invoke($1, `32u_good3`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:459
assert_return(() => invoke($1, `32u_good4`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:460
assert_return(() => invoke($1, `32u_good5`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:462
assert_return(() => invoke($1, `32s_good1`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:463
assert_return(() => invoke($1, `32s_good2`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:464
assert_return(() => invoke($1, `32s_good3`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:465
assert_return(() => invoke($1, `32s_good4`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:466
assert_return(() => invoke($1, `32s_good5`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:468
assert_return(() => invoke($1, `64_good1`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:469
assert_return(() => invoke($1, `64_good2`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:470
assert_return(() => invoke($1, `64_good3`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:471
assert_return(() => invoke($1, `64_good4`, [65504n]), [value("i64", 0n)]);

// ./test/core/address64.wast:472
assert_trap(() => invoke($1, `64_good5`, [65504n]), `out of bounds memory access`);

// ./test/core/address64.wast:474
assert_trap(() => invoke($1, `8u_bad`, [0n]), `out of bounds memory access`);

// ./test/core/address64.wast:475
assert_trap(() => invoke($1, `8s_bad`, [0n]), `out of bounds memory access`);

// ./test/core/address64.wast:476
assert_trap(() => invoke($1, `16u_bad`, [0n]), `out of bounds memory access`);

// ./test/core/address64.wast:477
assert_trap(() => invoke($1, `16s_bad`, [0n]), `out of bounds memory access`);

// ./test/core/address64.wast:478
assert_trap(() => invoke($1, `32u_bad`, [0n]), `out of bounds memory access`);

// ./test/core/address64.wast:479
assert_trap(() => invoke($1, `32s_bad`, [0n]), `out of bounds memory access`);

// ./test/core/address64.wast:480
assert_trap(() => invoke($1, `64_bad`, [0n]), `out of bounds memory access`);

// ./test/core/address64.wast:482
assert_trap(() => invoke($1, `8u_bad`, [1n]), `out of bounds memory access`);

// ./test/core/address64.wast:483
assert_trap(() => invoke($1, `8s_bad`, [1n]), `out of bounds memory access`);

// ./test/core/address64.wast:484
assert_trap(() => invoke($1, `16u_bad`, [1n]), `out of bounds memory access`);

// ./test/core/address64.wast:485
assert_trap(() => invoke($1, `16s_bad`, [1n]), `out of bounds memory access`);

// ./test/core/address64.wast:486
assert_trap(() => invoke($1, `32u_bad`, [0n]), `out of bounds memory access`);

// ./test/core/address64.wast:487
assert_trap(() => invoke($1, `32s_bad`, [0n]), `out of bounds memory access`);

// ./test/core/address64.wast:488
assert_trap(() => invoke($1, `64_bad`, [1n]), `out of bounds memory access`);

// ./test/core/address64.wast:492
let $2 = instantiate(`(module
  (memory i64 1)
  (data (i64.const 0) "\\00\\00\\00\\00\\00\\00\\a0\\7f\\01\\00\\d0\\7f")

  (func (export "32_good1") (param $$i i64) (result f32)
    (f32.load offset=0 (local.get $$i))                   ;; 0.0 '\\00\\00\\00\\00'
  )
  (func (export "32_good2") (param $$i i64) (result f32)
    (f32.load align=1 (local.get $$i))                    ;; 0.0 '\\00\\00\\00\\00'
  )
  (func (export "32_good3") (param $$i i64) (result f32)
    (f32.load offset=1 align=1 (local.get $$i))           ;; 0.0 '\\00\\00\\00\\00'
  )
  (func (export "32_good4") (param $$i i64) (result f32)
    (f32.load offset=2 align=2 (local.get $$i))           ;; 0.0 '\\00\\00\\00\\00'
  )
  (func (export "32_good5") (param $$i i64) (result f32)
    (f32.load offset=8 align=4 (local.get $$i))           ;; nan:0x500001 '\\01\\00\\d0\\7f'
  )
  (func (export "32_bad") (param $$i i64)
    (drop (f32.load offset=4294967295 (local.get $$i)))
  )
)`);

// ./test/core/address64.wast:516
assert_return(() => invoke($2, `32_good1`, [0n]), [value("f32", 0)]);

// ./test/core/address64.wast:517
assert_return(() => invoke($2, `32_good2`, [0n]), [value("f32", 0)]);

// ./test/core/address64.wast:518
assert_return(() => invoke($2, `32_good3`, [0n]), [value("f32", 0)]);

// ./test/core/address64.wast:519
assert_return(() => invoke($2, `32_good4`, [0n]), [value("f32", 0)]);

// ./test/core/address64.wast:520
assert_return(() => invoke($2, `32_good5`, [0n]), [bytes("f32", [0x1, 0x0, 0xd0, 0x7f])]);

// ./test/core/address64.wast:522
assert_return(() => invoke($2, `32_good1`, [65524n]), [value("f32", 0)]);

// ./test/core/address64.wast:523
assert_return(() => invoke($2, `32_good2`, [65524n]), [value("f32", 0)]);

// ./test/core/address64.wast:524
assert_return(() => invoke($2, `32_good3`, [65524n]), [value("f32", 0)]);

// ./test/core/address64.wast:525
assert_return(() => invoke($2, `32_good4`, [65524n]), [value("f32", 0)]);

// ./test/core/address64.wast:526
assert_return(() => invoke($2, `32_good5`, [65524n]), [value("f32", 0)]);

// ./test/core/address64.wast:528
assert_return(() => invoke($2, `32_good1`, [65525n]), [value("f32", 0)]);

// ./test/core/address64.wast:529
assert_return(() => invoke($2, `32_good2`, [65525n]), [value("f32", 0)]);

// ./test/core/address64.wast:530
assert_return(() => invoke($2, `32_good3`, [65525n]), [value("f32", 0)]);

// ./test/core/address64.wast:531
assert_return(() => invoke($2, `32_good4`, [65525n]), [value("f32", 0)]);

// ./test/core/address64.wast:532
assert_trap(() => invoke($2, `32_good5`, [65525n]), `out of bounds memory access`);

// ./test/core/address64.wast:534
assert_trap(() => invoke($2, `32_bad`, [0n]), `out of bounds memory access`);

// ./test/core/address64.wast:535
assert_trap(() => invoke($2, `32_bad`, [1n]), `out of bounds memory access`);

// ./test/core/address64.wast:539
let $3 = instantiate(`(module
  (memory i64 1)
  (data (i64.const 0) "\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\f4\\7f\\01\\00\\00\\00\\00\\00\\fc\\7f")

  (func (export "64_good1") (param $$i i64) (result f64)
    (f64.load offset=0 (local.get $$i))                     ;; 0.0 '\\00\\00\\00\\00\\00\\00\\00\\00'
  )
  (func (export "64_good2") (param $$i i64) (result f64)
    (f64.load align=1 (local.get $$i))                      ;; 0.0 '\\00\\00\\00\\00\\00\\00\\00\\00'
  )
  (func (export "64_good3") (param $$i i64) (result f64)
    (f64.load offset=1 align=1 (local.get $$i))             ;; 0.0 '\\00\\00\\00\\00\\00\\00\\00\\00'
  )
  (func (export "64_good4") (param $$i i64) (result f64)
    (f64.load offset=2 align=2 (local.get $$i))             ;; 0.0 '\\00\\00\\00\\00\\00\\00\\00\\00'
  )
  (func (export "64_good5") (param $$i i64) (result f64)
    (f64.load offset=18 align=8 (local.get $$i))            ;; nan:0xc000000000001 '\\01\\00\\00\\00\\00\\00\\fc\\7f'
  )
  (func (export "64_bad") (param $$i i64)
    (drop (f64.load offset=4294967295 (local.get $$i)))
  )
)`);

// ./test/core/address64.wast:563
assert_return(() => invoke($3, `64_good1`, [0n]), [value("f64", 0)]);

// ./test/core/address64.wast:564
assert_return(() => invoke($3, `64_good2`, [0n]), [value("f64", 0)]);

// ./test/core/address64.wast:565
assert_return(() => invoke($3, `64_good3`, [0n]), [value("f64", 0)]);

// ./test/core/address64.wast:566
assert_return(() => invoke($3, `64_good4`, [0n]), [value("f64", 0)]);

// ./test/core/address64.wast:567
assert_return(
  () => invoke($3, `64_good5`, [0n]),
  [bytes("f64", [0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0xfc, 0x7f])],
);

// ./test/core/address64.wast:569
assert_return(() => invoke($3, `64_good1`, [65510n]), [value("f64", 0)]);

// ./test/core/address64.wast:570
assert_return(() => invoke($3, `64_good2`, [65510n]), [value("f64", 0)]);

// ./test/core/address64.wast:571
assert_return(() => invoke($3, `64_good3`, [65510n]), [value("f64", 0)]);

// ./test/core/address64.wast:572
assert_return(() => invoke($3, `64_good4`, [65510n]), [value("f64", 0)]);

// ./test/core/address64.wast:573
assert_return(() => invoke($3, `64_good5`, [65510n]), [value("f64", 0)]);

// ./test/core/address64.wast:575
assert_return(() => invoke($3, `64_good1`, [65511n]), [value("f64", 0)]);

// ./test/core/address64.wast:576
assert_return(() => invoke($3, `64_good2`, [65511n]), [value("f64", 0)]);

// ./test/core/address64.wast:577
assert_return(() => invoke($3, `64_good3`, [65511n]), [value("f64", 0)]);

// ./test/core/address64.wast:578
assert_return(() => invoke($3, `64_good4`, [65511n]), [value("f64", 0)]);

// ./test/core/address64.wast:579
assert_trap(() => invoke($3, `64_good5`, [65511n]), `out of bounds memory access`);

// ./test/core/address64.wast:581
assert_trap(() => invoke($3, `64_bad`, [0n]), `out of bounds memory access`);

// ./test/core/address64.wast:582
assert_trap(() => invoke($3, `64_bad`, [1n]), `out of bounds memory access`);
