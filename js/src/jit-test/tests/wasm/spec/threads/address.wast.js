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

// ./test/core/address.wast

// ./test/core/address.wast:3
let $0 = instantiate(`(module
  (memory 1)
  (data (i32.const 0) "abcdefghijklmnopqrstuvwxyz")

  (func (export "8u_good1") (param $$i i32) (result i32)
    (i32.load8_u offset=0 (local.get $$i))                   ;; 97 'a'
  )
  (func (export "8u_good2") (param $$i i32) (result i32)
    (i32.load8_u align=1 (local.get $$i))                    ;; 97 'a'
  )
  (func (export "8u_good3") (param $$i i32) (result i32)
    (i32.load8_u offset=1 align=1 (local.get $$i))           ;; 98 'b'
  )
  (func (export "8u_good4") (param $$i i32) (result i32)
    (i32.load8_u offset=2 align=1 (local.get $$i))           ;; 99 'c'
  )
  (func (export "8u_good5") (param $$i i32) (result i32)
    (i32.load8_u offset=25 align=1 (local.get $$i))          ;; 122 'z'
  )

  (func (export "8s_good1") (param $$i i32) (result i32)
    (i32.load8_s offset=0 (local.get $$i))                   ;; 97 'a'
  )
  (func (export "8s_good2") (param $$i i32) (result i32)
    (i32.load8_s align=1 (local.get $$i))                    ;; 97 'a'
  )
  (func (export "8s_good3") (param $$i i32) (result i32)
    (i32.load8_s offset=1 align=1 (local.get $$i))           ;; 98 'b'
  )
  (func (export "8s_good4") (param $$i i32) (result i32)
    (i32.load8_s offset=2 align=1 (local.get $$i))           ;; 99 'c'
  )
  (func (export "8s_good5") (param $$i i32) (result i32)
    (i32.load8_s offset=25 align=1 (local.get $$i))          ;; 122 'z'
  )

  (func (export "16u_good1") (param $$i i32) (result i32)
    (i32.load16_u offset=0 (local.get $$i))                  ;; 25185 'ab'
  )
  (func (export "16u_good2") (param $$i i32) (result i32)
    (i32.load16_u align=1 (local.get $$i))                   ;; 25185 'ab'
  )
  (func (export "16u_good3") (param $$i i32) (result i32)
    (i32.load16_u offset=1 align=1 (local.get $$i))          ;; 25442 'bc'
  )
  (func (export "16u_good4") (param $$i i32) (result i32)
    (i32.load16_u offset=2 align=2 (local.get $$i))          ;; 25699 'cd'
  )
  (func (export "16u_good5") (param $$i i32) (result i32)
    (i32.load16_u offset=25 align=2 (local.get $$i))         ;; 122 'z\\0'
  )

  (func (export "16s_good1") (param $$i i32) (result i32)
    (i32.load16_s offset=0 (local.get $$i))                  ;; 25185 'ab'
  )
  (func (export "16s_good2") (param $$i i32) (result i32)
    (i32.load16_s align=1 (local.get $$i))                   ;; 25185 'ab'
  )
  (func (export "16s_good3") (param $$i i32) (result i32)
    (i32.load16_s offset=1 align=1 (local.get $$i))          ;; 25442 'bc'
  )
  (func (export "16s_good4") (param $$i i32) (result i32)
    (i32.load16_s offset=2 align=2 (local.get $$i))          ;; 25699 'cd'
  )
  (func (export "16s_good5") (param $$i i32) (result i32)
    (i32.load16_s offset=25 align=2 (local.get $$i))         ;; 122 'z\\0'
  )

  (func (export "32_good1") (param $$i i32) (result i32)
    (i32.load offset=0 (local.get $$i))                      ;; 1684234849 'abcd'
  )
  (func (export "32_good2") (param $$i i32) (result i32)
    (i32.load align=1 (local.get $$i))                       ;; 1684234849 'abcd'
  )
  (func (export "32_good3") (param $$i i32) (result i32)
    (i32.load offset=1 align=1 (local.get $$i))              ;; 1701077858 'bcde'
  )
  (func (export "32_good4") (param $$i i32) (result i32)
    (i32.load offset=2 align=2 (local.get $$i))              ;; 1717920867 'cdef'
  )
  (func (export "32_good5") (param $$i i32) (result i32)
    (i32.load offset=25 align=4 (local.get $$i))             ;; 122 'z\\0\\0\\0'
  )

  (func (export "8u_bad") (param $$i i32)
    (drop (i32.load8_u offset=4294967295 (local.get $$i)))
  )
  (func (export "8s_bad") (param $$i i32)
    (drop (i32.load8_s offset=4294967295 (local.get $$i)))
  )
  (func (export "16u_bad") (param $$i i32)
    (drop (i32.load16_u offset=4294967295 (local.get $$i)))
  )
  (func (export "16s_bad") (param $$i i32)
    (drop (i32.load16_s offset=4294967295 (local.get $$i)))
  )
  (func (export "32_bad") (param $$i i32)
    (drop (i32.load offset=4294967295 (local.get $$i)))
  )
)`);

// ./test/core/address.wast:104
assert_return(() => invoke($0, `8u_good1`, [0]), [value("i32", 97)]);

// ./test/core/address.wast:105
assert_return(() => invoke($0, `8u_good2`, [0]), [value("i32", 97)]);

// ./test/core/address.wast:106
assert_return(() => invoke($0, `8u_good3`, [0]), [value("i32", 98)]);

// ./test/core/address.wast:107
assert_return(() => invoke($0, `8u_good4`, [0]), [value("i32", 99)]);

// ./test/core/address.wast:108
assert_return(() => invoke($0, `8u_good5`, [0]), [value("i32", 122)]);

// ./test/core/address.wast:110
assert_return(() => invoke($0, `8s_good1`, [0]), [value("i32", 97)]);

// ./test/core/address.wast:111
assert_return(() => invoke($0, `8s_good2`, [0]), [value("i32", 97)]);

// ./test/core/address.wast:112
assert_return(() => invoke($0, `8s_good3`, [0]), [value("i32", 98)]);

// ./test/core/address.wast:113
assert_return(() => invoke($0, `8s_good4`, [0]), [value("i32", 99)]);

// ./test/core/address.wast:114
assert_return(() => invoke($0, `8s_good5`, [0]), [value("i32", 122)]);

// ./test/core/address.wast:116
assert_return(() => invoke($0, `16u_good1`, [0]), [value("i32", 25185)]);

// ./test/core/address.wast:117
assert_return(() => invoke($0, `16u_good2`, [0]), [value("i32", 25185)]);

// ./test/core/address.wast:118
assert_return(() => invoke($0, `16u_good3`, [0]), [value("i32", 25442)]);

// ./test/core/address.wast:119
assert_return(() => invoke($0, `16u_good4`, [0]), [value("i32", 25699)]);

// ./test/core/address.wast:120
assert_return(() => invoke($0, `16u_good5`, [0]), [value("i32", 122)]);

// ./test/core/address.wast:122
assert_return(() => invoke($0, `16s_good1`, [0]), [value("i32", 25185)]);

// ./test/core/address.wast:123
assert_return(() => invoke($0, `16s_good2`, [0]), [value("i32", 25185)]);

// ./test/core/address.wast:124
assert_return(() => invoke($0, `16s_good3`, [0]), [value("i32", 25442)]);

// ./test/core/address.wast:125
assert_return(() => invoke($0, `16s_good4`, [0]), [value("i32", 25699)]);

// ./test/core/address.wast:126
assert_return(() => invoke($0, `16s_good5`, [0]), [value("i32", 122)]);

// ./test/core/address.wast:128
assert_return(() => invoke($0, `32_good1`, [0]), [value("i32", 1684234849)]);

// ./test/core/address.wast:129
assert_return(() => invoke($0, `32_good2`, [0]), [value("i32", 1684234849)]);

// ./test/core/address.wast:130
assert_return(() => invoke($0, `32_good3`, [0]), [value("i32", 1701077858)]);

// ./test/core/address.wast:131
assert_return(() => invoke($0, `32_good4`, [0]), [value("i32", 1717920867)]);

// ./test/core/address.wast:132
assert_return(() => invoke($0, `32_good5`, [0]), [value("i32", 122)]);

// ./test/core/address.wast:134
assert_return(() => invoke($0, `8u_good1`, [65507]), [value("i32", 0)]);

// ./test/core/address.wast:135
assert_return(() => invoke($0, `8u_good2`, [65507]), [value("i32", 0)]);

// ./test/core/address.wast:136
assert_return(() => invoke($0, `8u_good3`, [65507]), [value("i32", 0)]);

// ./test/core/address.wast:137
assert_return(() => invoke($0, `8u_good4`, [65507]), [value("i32", 0)]);

// ./test/core/address.wast:138
assert_return(() => invoke($0, `8u_good5`, [65507]), [value("i32", 0)]);

// ./test/core/address.wast:140
assert_return(() => invoke($0, `8s_good1`, [65507]), [value("i32", 0)]);

// ./test/core/address.wast:141
assert_return(() => invoke($0, `8s_good2`, [65507]), [value("i32", 0)]);

// ./test/core/address.wast:142
assert_return(() => invoke($0, `8s_good3`, [65507]), [value("i32", 0)]);

// ./test/core/address.wast:143
assert_return(() => invoke($0, `8s_good4`, [65507]), [value("i32", 0)]);

// ./test/core/address.wast:144
assert_return(() => invoke($0, `8s_good5`, [65507]), [value("i32", 0)]);

// ./test/core/address.wast:146
assert_return(() => invoke($0, `16u_good1`, [65507]), [value("i32", 0)]);

// ./test/core/address.wast:147
assert_return(() => invoke($0, `16u_good2`, [65507]), [value("i32", 0)]);

// ./test/core/address.wast:148
assert_return(() => invoke($0, `16u_good3`, [65507]), [value("i32", 0)]);

// ./test/core/address.wast:149
assert_return(() => invoke($0, `16u_good4`, [65507]), [value("i32", 0)]);

// ./test/core/address.wast:150
assert_return(() => invoke($0, `16u_good5`, [65507]), [value("i32", 0)]);

// ./test/core/address.wast:152
assert_return(() => invoke($0, `16s_good1`, [65507]), [value("i32", 0)]);

// ./test/core/address.wast:153
assert_return(() => invoke($0, `16s_good2`, [65507]), [value("i32", 0)]);

// ./test/core/address.wast:154
assert_return(() => invoke($0, `16s_good3`, [65507]), [value("i32", 0)]);

// ./test/core/address.wast:155
assert_return(() => invoke($0, `16s_good4`, [65507]), [value("i32", 0)]);

// ./test/core/address.wast:156
assert_return(() => invoke($0, `16s_good5`, [65507]), [value("i32", 0)]);

// ./test/core/address.wast:158
assert_return(() => invoke($0, `32_good1`, [65507]), [value("i32", 0)]);

// ./test/core/address.wast:159
assert_return(() => invoke($0, `32_good2`, [65507]), [value("i32", 0)]);

// ./test/core/address.wast:160
assert_return(() => invoke($0, `32_good3`, [65507]), [value("i32", 0)]);

// ./test/core/address.wast:161
assert_return(() => invoke($0, `32_good4`, [65507]), [value("i32", 0)]);

// ./test/core/address.wast:162
assert_return(() => invoke($0, `32_good5`, [65507]), [value("i32", 0)]);

// ./test/core/address.wast:164
assert_return(() => invoke($0, `8u_good1`, [65508]), [value("i32", 0)]);

// ./test/core/address.wast:165
assert_return(() => invoke($0, `8u_good2`, [65508]), [value("i32", 0)]);

// ./test/core/address.wast:166
assert_return(() => invoke($0, `8u_good3`, [65508]), [value("i32", 0)]);

// ./test/core/address.wast:167
assert_return(() => invoke($0, `8u_good4`, [65508]), [value("i32", 0)]);

// ./test/core/address.wast:168
assert_return(() => invoke($0, `8u_good5`, [65508]), [value("i32", 0)]);

// ./test/core/address.wast:170
assert_return(() => invoke($0, `8s_good1`, [65508]), [value("i32", 0)]);

// ./test/core/address.wast:171
assert_return(() => invoke($0, `8s_good2`, [65508]), [value("i32", 0)]);

// ./test/core/address.wast:172
assert_return(() => invoke($0, `8s_good3`, [65508]), [value("i32", 0)]);

// ./test/core/address.wast:173
assert_return(() => invoke($0, `8s_good4`, [65508]), [value("i32", 0)]);

// ./test/core/address.wast:174
assert_return(() => invoke($0, `8s_good5`, [65508]), [value("i32", 0)]);

// ./test/core/address.wast:176
assert_return(() => invoke($0, `16u_good1`, [65508]), [value("i32", 0)]);

// ./test/core/address.wast:177
assert_return(() => invoke($0, `16u_good2`, [65508]), [value("i32", 0)]);

// ./test/core/address.wast:178
assert_return(() => invoke($0, `16u_good3`, [65508]), [value("i32", 0)]);

// ./test/core/address.wast:179
assert_return(() => invoke($0, `16u_good4`, [65508]), [value("i32", 0)]);

// ./test/core/address.wast:180
assert_return(() => invoke($0, `16u_good5`, [65508]), [value("i32", 0)]);

// ./test/core/address.wast:182
assert_return(() => invoke($0, `16s_good1`, [65508]), [value("i32", 0)]);

// ./test/core/address.wast:183
assert_return(() => invoke($0, `16s_good2`, [65508]), [value("i32", 0)]);

// ./test/core/address.wast:184
assert_return(() => invoke($0, `16s_good3`, [65508]), [value("i32", 0)]);

// ./test/core/address.wast:185
assert_return(() => invoke($0, `16s_good4`, [65508]), [value("i32", 0)]);

// ./test/core/address.wast:186
assert_return(() => invoke($0, `16s_good5`, [65508]), [value("i32", 0)]);

// ./test/core/address.wast:188
assert_return(() => invoke($0, `32_good1`, [65508]), [value("i32", 0)]);

// ./test/core/address.wast:189
assert_return(() => invoke($0, `32_good2`, [65508]), [value("i32", 0)]);

// ./test/core/address.wast:190
assert_return(() => invoke($0, `32_good3`, [65508]), [value("i32", 0)]);

// ./test/core/address.wast:191
assert_return(() => invoke($0, `32_good4`, [65508]), [value("i32", 0)]);

// ./test/core/address.wast:192
assert_trap(
  () => invoke($0, `32_good5`, [65508]),
  `out of bounds memory access`,
);

// ./test/core/address.wast:194
assert_trap(() => invoke($0, `8u_bad`, [0]), `out of bounds memory access`);

// ./test/core/address.wast:195
assert_trap(() => invoke($0, `8s_bad`, [0]), `out of bounds memory access`);

// ./test/core/address.wast:196
assert_trap(() => invoke($0, `16u_bad`, [0]), `out of bounds memory access`);

// ./test/core/address.wast:197
assert_trap(() => invoke($0, `16s_bad`, [0]), `out of bounds memory access`);

// ./test/core/address.wast:198
assert_trap(() => invoke($0, `32_bad`, [0]), `out of bounds memory access`);

// ./test/core/address.wast:200
assert_trap(() => invoke($0, `8u_bad`, [1]), `out of bounds memory access`);

// ./test/core/address.wast:201
assert_trap(() => invoke($0, `8s_bad`, [1]), `out of bounds memory access`);

// ./test/core/address.wast:202
assert_trap(() => invoke($0, `16u_bad`, [1]), `out of bounds memory access`);

// ./test/core/address.wast:203
assert_trap(() => invoke($0, `16s_bad`, [1]), `out of bounds memory access`);

// ./test/core/address.wast:204
assert_trap(() => invoke($0, `32_bad`, [1]), `out of bounds memory access`);

// ./test/core/address.wast:206
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (drop (i32.load offset=4294967296 (i32.const 0)))) `,
    ),
  `i32 constant`,
);

// ./test/core/address.wast:216
let $1 = instantiate(`(module
  (memory 1)
  (data (i32.const 0) "abcdefghijklmnopqrstuvwxyz")

  (func (export "8u_good1") (param $$i i32) (result i64)
    (i64.load8_u offset=0 (local.get $$i))                   ;; 97 'a'
  )
  (func (export "8u_good2") (param $$i i32) (result i64)
    (i64.load8_u align=1 (local.get $$i))                    ;; 97 'a'
  )
  (func (export "8u_good3") (param $$i i32) (result i64)
    (i64.load8_u offset=1 align=1 (local.get $$i))           ;; 98 'b'
  )
  (func (export "8u_good4") (param $$i i32) (result i64)
    (i64.load8_u offset=2 align=1 (local.get $$i))           ;; 99 'c'
  )
  (func (export "8u_good5") (param $$i i32) (result i64)
    (i64.load8_u offset=25 align=1 (local.get $$i))          ;; 122 'z'
  )

  (func (export "8s_good1") (param $$i i32) (result i64)
    (i64.load8_s offset=0 (local.get $$i))                   ;; 97 'a'
  )
  (func (export "8s_good2") (param $$i i32) (result i64)
    (i64.load8_s align=1 (local.get $$i))                    ;; 97 'a'
  )
  (func (export "8s_good3") (param $$i i32) (result i64)
    (i64.load8_s offset=1 align=1 (local.get $$i))           ;; 98 'b'
  )
  (func (export "8s_good4") (param $$i i32) (result i64)
    (i64.load8_s offset=2 align=1 (local.get $$i))           ;; 99 'c'
  )
  (func (export "8s_good5") (param $$i i32) (result i64)
    (i64.load8_s offset=25 align=1 (local.get $$i))          ;; 122 'z'
  )

  (func (export "16u_good1") (param $$i i32) (result i64)
    (i64.load16_u offset=0 (local.get $$i))                 ;; 25185 'ab'
  )
  (func (export "16u_good2") (param $$i i32) (result i64)
    (i64.load16_u align=1 (local.get $$i))                  ;; 25185 'ab'
  )
  (func (export "16u_good3") (param $$i i32) (result i64)
    (i64.load16_u offset=1 align=1 (local.get $$i))         ;; 25442 'bc'
  )
  (func (export "16u_good4") (param $$i i32) (result i64)
    (i64.load16_u offset=2 align=2 (local.get $$i))         ;; 25699 'cd'
  )
  (func (export "16u_good5") (param $$i i32) (result i64)
    (i64.load16_u offset=25 align=2 (local.get $$i))        ;; 122 'z\\0'
  )

  (func (export "16s_good1") (param $$i i32) (result i64)
    (i64.load16_s offset=0 (local.get $$i))                 ;; 25185 'ab'
  )
  (func (export "16s_good2") (param $$i i32) (result i64)
    (i64.load16_s align=1 (local.get $$i))                  ;; 25185 'ab'
  )
  (func (export "16s_good3") (param $$i i32) (result i64)
    (i64.load16_s offset=1 align=1 (local.get $$i))         ;; 25442 'bc'
  )
  (func (export "16s_good4") (param $$i i32) (result i64)
    (i64.load16_s offset=2 align=2 (local.get $$i))         ;; 25699 'cd'
  )
  (func (export "16s_good5") (param $$i i32) (result i64)
    (i64.load16_s offset=25 align=2 (local.get $$i))        ;; 122 'z\\0'
  )

  (func (export "32u_good1") (param $$i i32) (result i64)
    (i64.load32_u offset=0 (local.get $$i))                 ;; 1684234849 'abcd'
  )
  (func (export "32u_good2") (param $$i i32) (result i64)
    (i64.load32_u align=1 (local.get $$i))                  ;; 1684234849 'abcd'
  )
  (func (export "32u_good3") (param $$i i32) (result i64)
    (i64.load32_u offset=1 align=1 (local.get $$i))         ;; 1701077858 'bcde'
  )
  (func (export "32u_good4") (param $$i i32) (result i64)
    (i64.load32_u offset=2 align=2 (local.get $$i))         ;; 1717920867 'cdef'
  )
  (func (export "32u_good5") (param $$i i32) (result i64)
    (i64.load32_u offset=25 align=4 (local.get $$i))        ;; 122 'z\\0\\0\\0'
  )

  (func (export "32s_good1") (param $$i i32) (result i64)
    (i64.load32_s offset=0 (local.get $$i))                 ;; 1684234849 'abcd'
  )
  (func (export "32s_good2") (param $$i i32) (result i64)
    (i64.load32_s align=1 (local.get $$i))                  ;; 1684234849 'abcd'
  )
  (func (export "32s_good3") (param $$i i32) (result i64)
    (i64.load32_s offset=1 align=1 (local.get $$i))         ;; 1701077858 'bcde'
  )
  (func (export "32s_good4") (param $$i i32) (result i64)
    (i64.load32_s offset=2 align=2 (local.get $$i))         ;; 1717920867 'cdef'
  )
  (func (export "32s_good5") (param $$i i32) (result i64)
    (i64.load32_s offset=25 align=4 (local.get $$i))        ;; 122 'z\\0\\0\\0'
  )

  (func (export "64_good1") (param $$i i32) (result i64)
    (i64.load offset=0 (local.get $$i))                     ;; 0x6867666564636261 'abcdefgh'
  )
  (func (export "64_good2") (param $$i i32) (result i64)
    (i64.load align=1 (local.get $$i))                      ;; 0x6867666564636261 'abcdefgh'
  )
  (func (export "64_good3") (param $$i i32) (result i64)
    (i64.load offset=1 align=1 (local.get $$i))             ;; 0x6968676665646362 'bcdefghi'
  )
  (func (export "64_good4") (param $$i i32) (result i64)
    (i64.load offset=2 align=2 (local.get $$i))             ;; 0x6a69686766656463 'cdefghij'
  )
  (func (export "64_good5") (param $$i i32) (result i64)
    (i64.load offset=25 align=8 (local.get $$i))            ;; 122 'z\\0\\0\\0\\0\\0\\0\\0'
  )

  (func (export "8u_bad") (param $$i i32)
    (drop (i64.load8_u offset=4294967295 (local.get $$i)))
  )
  (func (export "8s_bad") (param $$i i32)
    (drop (i64.load8_s offset=4294967295 (local.get $$i)))
  )
  (func (export "16u_bad") (param $$i i32)
    (drop (i64.load16_u offset=4294967295 (local.get $$i)))
  )
  (func (export "16s_bad") (param $$i i32)
    (drop (i64.load16_s offset=4294967295 (local.get $$i)))
  )
  (func (export "32u_bad") (param $$i i32)
    (drop (i64.load32_u offset=4294967295 (local.get $$i)))
  )
  (func (export "32s_bad") (param $$i i32)
    (drop (i64.load32_s offset=4294967295 (local.get $$i)))
  )
  (func (export "64_bad") (param $$i i32)
    (drop (i64.load offset=4294967295 (local.get $$i)))
  )
)`);

// ./test/core/address.wast:355
assert_return(() => invoke($1, `8u_good1`, [0]), [value("i64", 97n)]);

// ./test/core/address.wast:356
assert_return(() => invoke($1, `8u_good2`, [0]), [value("i64", 97n)]);

// ./test/core/address.wast:357
assert_return(() => invoke($1, `8u_good3`, [0]), [value("i64", 98n)]);

// ./test/core/address.wast:358
assert_return(() => invoke($1, `8u_good4`, [0]), [value("i64", 99n)]);

// ./test/core/address.wast:359
assert_return(() => invoke($1, `8u_good5`, [0]), [value("i64", 122n)]);

// ./test/core/address.wast:361
assert_return(() => invoke($1, `8s_good1`, [0]), [value("i64", 97n)]);

// ./test/core/address.wast:362
assert_return(() => invoke($1, `8s_good2`, [0]), [value("i64", 97n)]);

// ./test/core/address.wast:363
assert_return(() => invoke($1, `8s_good3`, [0]), [value("i64", 98n)]);

// ./test/core/address.wast:364
assert_return(() => invoke($1, `8s_good4`, [0]), [value("i64", 99n)]);

// ./test/core/address.wast:365
assert_return(() => invoke($1, `8s_good5`, [0]), [value("i64", 122n)]);

// ./test/core/address.wast:367
assert_return(() => invoke($1, `16u_good1`, [0]), [value("i64", 25185n)]);

// ./test/core/address.wast:368
assert_return(() => invoke($1, `16u_good2`, [0]), [value("i64", 25185n)]);

// ./test/core/address.wast:369
assert_return(() => invoke($1, `16u_good3`, [0]), [value("i64", 25442n)]);

// ./test/core/address.wast:370
assert_return(() => invoke($1, `16u_good4`, [0]), [value("i64", 25699n)]);

// ./test/core/address.wast:371
assert_return(() => invoke($1, `16u_good5`, [0]), [value("i64", 122n)]);

// ./test/core/address.wast:373
assert_return(() => invoke($1, `16s_good1`, [0]), [value("i64", 25185n)]);

// ./test/core/address.wast:374
assert_return(() => invoke($1, `16s_good2`, [0]), [value("i64", 25185n)]);

// ./test/core/address.wast:375
assert_return(() => invoke($1, `16s_good3`, [0]), [value("i64", 25442n)]);

// ./test/core/address.wast:376
assert_return(() => invoke($1, `16s_good4`, [0]), [value("i64", 25699n)]);

// ./test/core/address.wast:377
assert_return(() => invoke($1, `16s_good5`, [0]), [value("i64", 122n)]);

// ./test/core/address.wast:379
assert_return(() => invoke($1, `32u_good1`, [0]), [value("i64", 1684234849n)]);

// ./test/core/address.wast:380
assert_return(() => invoke($1, `32u_good2`, [0]), [value("i64", 1684234849n)]);

// ./test/core/address.wast:381
assert_return(() => invoke($1, `32u_good3`, [0]), [value("i64", 1701077858n)]);

// ./test/core/address.wast:382
assert_return(() => invoke($1, `32u_good4`, [0]), [value("i64", 1717920867n)]);

// ./test/core/address.wast:383
assert_return(() => invoke($1, `32u_good5`, [0]), [value("i64", 122n)]);

// ./test/core/address.wast:385
assert_return(() => invoke($1, `32s_good1`, [0]), [value("i64", 1684234849n)]);

// ./test/core/address.wast:386
assert_return(() => invoke($1, `32s_good2`, [0]), [value("i64", 1684234849n)]);

// ./test/core/address.wast:387
assert_return(() => invoke($1, `32s_good3`, [0]), [value("i64", 1701077858n)]);

// ./test/core/address.wast:388
assert_return(() => invoke($1, `32s_good4`, [0]), [value("i64", 1717920867n)]);

// ./test/core/address.wast:389
assert_return(() => invoke($1, `32s_good5`, [0]), [value("i64", 122n)]);

// ./test/core/address.wast:391
assert_return(() => invoke($1, `64_good1`, [0]), [
  value("i64", 7523094288207667809n),
]);

// ./test/core/address.wast:392
assert_return(() => invoke($1, `64_good2`, [0]), [
  value("i64", 7523094288207667809n),
]);

// ./test/core/address.wast:393
assert_return(() => invoke($1, `64_good3`, [0]), [
  value("i64", 7595434461045744482n),
]);

// ./test/core/address.wast:394
assert_return(() => invoke($1, `64_good4`, [0]), [
  value("i64", 7667774633883821155n),
]);

// ./test/core/address.wast:395
assert_return(() => invoke($1, `64_good5`, [0]), [value("i64", 122n)]);

// ./test/core/address.wast:397
assert_return(() => invoke($1, `8u_good1`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:398
assert_return(() => invoke($1, `8u_good2`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:399
assert_return(() => invoke($1, `8u_good3`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:400
assert_return(() => invoke($1, `8u_good4`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:401
assert_return(() => invoke($1, `8u_good5`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:403
assert_return(() => invoke($1, `8s_good1`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:404
assert_return(() => invoke($1, `8s_good2`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:405
assert_return(() => invoke($1, `8s_good3`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:406
assert_return(() => invoke($1, `8s_good4`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:407
assert_return(() => invoke($1, `8s_good5`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:409
assert_return(() => invoke($1, `16u_good1`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:410
assert_return(() => invoke($1, `16u_good2`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:411
assert_return(() => invoke($1, `16u_good3`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:412
assert_return(() => invoke($1, `16u_good4`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:413
assert_return(() => invoke($1, `16u_good5`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:415
assert_return(() => invoke($1, `16s_good1`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:416
assert_return(() => invoke($1, `16s_good2`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:417
assert_return(() => invoke($1, `16s_good3`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:418
assert_return(() => invoke($1, `16s_good4`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:419
assert_return(() => invoke($1, `16s_good5`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:421
assert_return(() => invoke($1, `32u_good1`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:422
assert_return(() => invoke($1, `32u_good2`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:423
assert_return(() => invoke($1, `32u_good3`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:424
assert_return(() => invoke($1, `32u_good4`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:425
assert_return(() => invoke($1, `32u_good5`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:427
assert_return(() => invoke($1, `32s_good1`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:428
assert_return(() => invoke($1, `32s_good2`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:429
assert_return(() => invoke($1, `32s_good3`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:430
assert_return(() => invoke($1, `32s_good4`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:431
assert_return(() => invoke($1, `32s_good5`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:433
assert_return(() => invoke($1, `64_good1`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:434
assert_return(() => invoke($1, `64_good2`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:435
assert_return(() => invoke($1, `64_good3`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:436
assert_return(() => invoke($1, `64_good4`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:437
assert_return(() => invoke($1, `64_good5`, [65503]), [value("i64", 0n)]);

// ./test/core/address.wast:439
assert_return(() => invoke($1, `8u_good1`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:440
assert_return(() => invoke($1, `8u_good2`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:441
assert_return(() => invoke($1, `8u_good3`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:442
assert_return(() => invoke($1, `8u_good4`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:443
assert_return(() => invoke($1, `8u_good5`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:445
assert_return(() => invoke($1, `8s_good1`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:446
assert_return(() => invoke($1, `8s_good2`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:447
assert_return(() => invoke($1, `8s_good3`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:448
assert_return(() => invoke($1, `8s_good4`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:449
assert_return(() => invoke($1, `8s_good5`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:451
assert_return(() => invoke($1, `16u_good1`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:452
assert_return(() => invoke($1, `16u_good2`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:453
assert_return(() => invoke($1, `16u_good3`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:454
assert_return(() => invoke($1, `16u_good4`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:455
assert_return(() => invoke($1, `16u_good5`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:457
assert_return(() => invoke($1, `16s_good1`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:458
assert_return(() => invoke($1, `16s_good2`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:459
assert_return(() => invoke($1, `16s_good3`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:460
assert_return(() => invoke($1, `16s_good4`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:461
assert_return(() => invoke($1, `16s_good5`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:463
assert_return(() => invoke($1, `32u_good1`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:464
assert_return(() => invoke($1, `32u_good2`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:465
assert_return(() => invoke($1, `32u_good3`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:466
assert_return(() => invoke($1, `32u_good4`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:467
assert_return(() => invoke($1, `32u_good5`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:469
assert_return(() => invoke($1, `32s_good1`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:470
assert_return(() => invoke($1, `32s_good2`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:471
assert_return(() => invoke($1, `32s_good3`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:472
assert_return(() => invoke($1, `32s_good4`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:473
assert_return(() => invoke($1, `32s_good5`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:475
assert_return(() => invoke($1, `64_good1`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:476
assert_return(() => invoke($1, `64_good2`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:477
assert_return(() => invoke($1, `64_good3`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:478
assert_return(() => invoke($1, `64_good4`, [65504]), [value("i64", 0n)]);

// ./test/core/address.wast:479
assert_trap(
  () => invoke($1, `64_good5`, [65504]),
  `out of bounds memory access`,
);

// ./test/core/address.wast:481
assert_trap(() => invoke($1, `8u_bad`, [0]), `out of bounds memory access`);

// ./test/core/address.wast:482
assert_trap(() => invoke($1, `8s_bad`, [0]), `out of bounds memory access`);

// ./test/core/address.wast:483
assert_trap(() => invoke($1, `16u_bad`, [0]), `out of bounds memory access`);

// ./test/core/address.wast:484
assert_trap(() => invoke($1, `16s_bad`, [0]), `out of bounds memory access`);

// ./test/core/address.wast:485
assert_trap(() => invoke($1, `32u_bad`, [0]), `out of bounds memory access`);

// ./test/core/address.wast:486
assert_trap(() => invoke($1, `32s_bad`, [0]), `out of bounds memory access`);

// ./test/core/address.wast:487
assert_trap(() => invoke($1, `64_bad`, [0]), `out of bounds memory access`);

// ./test/core/address.wast:489
assert_trap(() => invoke($1, `8u_bad`, [1]), `out of bounds memory access`);

// ./test/core/address.wast:490
assert_trap(() => invoke($1, `8s_bad`, [1]), `out of bounds memory access`);

// ./test/core/address.wast:491
assert_trap(() => invoke($1, `16u_bad`, [1]), `out of bounds memory access`);

// ./test/core/address.wast:492
assert_trap(() => invoke($1, `16s_bad`, [1]), `out of bounds memory access`);

// ./test/core/address.wast:493
assert_trap(() => invoke($1, `32u_bad`, [0]), `out of bounds memory access`);

// ./test/core/address.wast:494
assert_trap(() => invoke($1, `32s_bad`, [0]), `out of bounds memory access`);

// ./test/core/address.wast:495
assert_trap(() => invoke($1, `64_bad`, [1]), `out of bounds memory access`);

// ./test/core/address.wast:499
let $2 = instantiate(`(module
  (memory 1)
  (data (i32.const 0) "\\00\\00\\00\\00\\00\\00\\a0\\7f\\01\\00\\d0\\7f")

  (func (export "32_good1") (param $$i i32) (result f32)
    (f32.load offset=0 (local.get $$i))                   ;; 0.0 '\\00\\00\\00\\00'
  )
  (func (export "32_good2") (param $$i i32) (result f32)
    (f32.load align=1 (local.get $$i))                    ;; 0.0 '\\00\\00\\00\\00'
  )
  (func (export "32_good3") (param $$i i32) (result f32)
    (f32.load offset=1 align=1 (local.get $$i))           ;; 0.0 '\\00\\00\\00\\00'
  )
  (func (export "32_good4") (param $$i i32) (result f32)
    (f32.load offset=2 align=2 (local.get $$i))           ;; 0.0 '\\00\\00\\00\\00'
  )
  (func (export "32_good5") (param $$i i32) (result f32)
    (f32.load offset=8 align=4 (local.get $$i))           ;; nan:0x500001 '\\01\\00\\d0\\7f'
  )
  (func (export "32_bad") (param $$i i32)
    (drop (f32.load offset=4294967295 (local.get $$i)))
  )
)`);

// ./test/core/address.wast:523
assert_return(() => invoke($2, `32_good1`, [0]), [value("f32", 0)]);

// ./test/core/address.wast:524
assert_return(() => invoke($2, `32_good2`, [0]), [value("f32", 0)]);

// ./test/core/address.wast:525
assert_return(() => invoke($2, `32_good3`, [0]), [value("f32", 0)]);

// ./test/core/address.wast:526
assert_return(() => invoke($2, `32_good4`, [0]), [value("f32", 0)]);

// ./test/core/address.wast:527
assert_return(() => invoke($2, `32_good5`, [0]), [
  bytes("f32", [0x1, 0x0, 0xd0, 0x7f]),
]);

// ./test/core/address.wast:529
assert_return(() => invoke($2, `32_good1`, [65524]), [value("f32", 0)]);

// ./test/core/address.wast:530
assert_return(() => invoke($2, `32_good2`, [65524]), [value("f32", 0)]);

// ./test/core/address.wast:531
assert_return(() => invoke($2, `32_good3`, [65524]), [value("f32", 0)]);

// ./test/core/address.wast:532
assert_return(() => invoke($2, `32_good4`, [65524]), [value("f32", 0)]);

// ./test/core/address.wast:533
assert_return(() => invoke($2, `32_good5`, [65524]), [value("f32", 0)]);

// ./test/core/address.wast:535
assert_return(() => invoke($2, `32_good1`, [65525]), [value("f32", 0)]);

// ./test/core/address.wast:536
assert_return(() => invoke($2, `32_good2`, [65525]), [value("f32", 0)]);

// ./test/core/address.wast:537
assert_return(() => invoke($2, `32_good3`, [65525]), [value("f32", 0)]);

// ./test/core/address.wast:538
assert_return(() => invoke($2, `32_good4`, [65525]), [value("f32", 0)]);

// ./test/core/address.wast:539
assert_trap(
  () => invoke($2, `32_good5`, [65525]),
  `out of bounds memory access`,
);

// ./test/core/address.wast:541
assert_trap(() => invoke($2, `32_bad`, [0]), `out of bounds memory access`);

// ./test/core/address.wast:542
assert_trap(() => invoke($2, `32_bad`, [1]), `out of bounds memory access`);

// ./test/core/address.wast:546
let $3 = instantiate(`(module
  (memory 1)
  (data (i32.const 0) "\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\f4\\7f\\01\\00\\00\\00\\00\\00\\fc\\7f")

  (func (export "64_good1") (param $$i i32) (result f64)
    (f64.load offset=0 (local.get $$i))                     ;; 0.0 '\\00\\00\\00\\00\\00\\00\\00\\00'
  )
  (func (export "64_good2") (param $$i i32) (result f64)
    (f64.load align=1 (local.get $$i))                      ;; 0.0 '\\00\\00\\00\\00\\00\\00\\00\\00'
  )
  (func (export "64_good3") (param $$i i32) (result f64)
    (f64.load offset=1 align=1 (local.get $$i))             ;; 0.0 '\\00\\00\\00\\00\\00\\00\\00\\00'
  )
  (func (export "64_good4") (param $$i i32) (result f64)
    (f64.load offset=2 align=2 (local.get $$i))             ;; 0.0 '\\00\\00\\00\\00\\00\\00\\00\\00'
  )
  (func (export "64_good5") (param $$i i32) (result f64)
    (f64.load offset=18 align=8 (local.get $$i))            ;; nan:0xc000000000001 '\\01\\00\\00\\00\\00\\00\\fc\\7f'
  )
  (func (export "64_bad") (param $$i i32)
    (drop (f64.load offset=4294967295 (local.get $$i)))
  )
)`);

// ./test/core/address.wast:570
assert_return(() => invoke($3, `64_good1`, [0]), [value("f64", 0)]);

// ./test/core/address.wast:571
assert_return(() => invoke($3, `64_good2`, [0]), [value("f64", 0)]);

// ./test/core/address.wast:572
assert_return(() => invoke($3, `64_good3`, [0]), [value("f64", 0)]);

// ./test/core/address.wast:573
assert_return(() => invoke($3, `64_good4`, [0]), [value("f64", 0)]);

// ./test/core/address.wast:574
assert_return(() => invoke($3, `64_good5`, [0]), [
  bytes("f64", [0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0xfc, 0x7f]),
]);

// ./test/core/address.wast:576
assert_return(() => invoke($3, `64_good1`, [65510]), [value("f64", 0)]);

// ./test/core/address.wast:577
assert_return(() => invoke($3, `64_good2`, [65510]), [value("f64", 0)]);

// ./test/core/address.wast:578
assert_return(() => invoke($3, `64_good3`, [65510]), [value("f64", 0)]);

// ./test/core/address.wast:579
assert_return(() => invoke($3, `64_good4`, [65510]), [value("f64", 0)]);

// ./test/core/address.wast:580
assert_return(() => invoke($3, `64_good5`, [65510]), [value("f64", 0)]);

// ./test/core/address.wast:582
assert_return(() => invoke($3, `64_good1`, [65511]), [value("f64", 0)]);

// ./test/core/address.wast:583
assert_return(() => invoke($3, `64_good2`, [65511]), [value("f64", 0)]);

// ./test/core/address.wast:584
assert_return(() => invoke($3, `64_good3`, [65511]), [value("f64", 0)]);

// ./test/core/address.wast:585
assert_return(() => invoke($3, `64_good4`, [65511]), [value("f64", 0)]);

// ./test/core/address.wast:586
assert_trap(
  () => invoke($3, `64_good5`, [65511]),
  `out of bounds memory access`,
);

// ./test/core/address.wast:588
assert_trap(() => invoke($3, `64_bad`, [0]), `out of bounds memory access`);

// ./test/core/address.wast:589
assert_trap(() => invoke($3, `64_bad`, [1]), `out of bounds memory access`);
