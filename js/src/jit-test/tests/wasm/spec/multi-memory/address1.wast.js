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

// ./test/core/multi-memory/address1.wast

// ./test/core/multi-memory/address1.wast:3
let $0 = instantiate(`(module
  (memory $$mem0 0)
  (memory $$mem1 0)
  (memory $$mem2 0)
  (memory $$mem3 0)
  (memory $$mem4 1)
  (data (memory $$mem4) (i32.const 0) "abcdefghijklmnopqrstuvwxyz")

  (func (export "8u_good1") (param $$i i32) (result i64)
    (i64.load8_u $$mem4 offset=0 (local.get $$i))                   ;; 97 'a'
  )
  (func (export "8u_good2") (param $$i i32) (result i64)
    (i64.load8_u $$mem4 align=1 (local.get $$i))                    ;; 97 'a'
  )
  (func (export "8u_good3") (param $$i i32) (result i64)
    (i64.load8_u $$mem4 offset=1 align=1 (local.get $$i))           ;; 98 'b'
  )
  (func (export "8u_good4") (param $$i i32) (result i64)
    (i64.load8_u $$mem4 offset=2 align=1 (local.get $$i))           ;; 99 'c'
  )
  (func (export "8u_good5") (param $$i i32) (result i64)
    (i64.load8_u $$mem4 offset=25 align=1 (local.get $$i))          ;; 122 'z'
  )

  (func (export "8s_good1") (param $$i i32) (result i64)
    (i64.load8_s $$mem4 offset=0 (local.get $$i))                   ;; 97 'a'
  )
  (func (export "8s_good2") (param $$i i32) (result i64)
    (i64.load8_s $$mem4 align=1 (local.get $$i))                    ;; 97 'a'
  )
  (func (export "8s_good3") (param $$i i32) (result i64)
    (i64.load8_s $$mem4 offset=1 align=1 (local.get $$i))           ;; 98 'b'
  )
  (func (export "8s_good4") (param $$i i32) (result i64)
    (i64.load8_s $$mem4 offset=2 align=1 (local.get $$i))           ;; 99 'c'
  )
  (func (export "8s_good5") (param $$i i32) (result i64)
    (i64.load8_s $$mem4 offset=25 align=1 (local.get $$i))          ;; 122 'z'
  )

  (func (export "16u_good1") (param $$i i32) (result i64)
    (i64.load16_u $$mem4 offset=0 (local.get $$i))                 ;; 25185 'ab'
  )
  (func (export "16u_good2") (param $$i i32) (result i64)
    (i64.load16_u $$mem4 align=1 (local.get $$i))                  ;; 25185 'ab'
  )
  (func (export "16u_good3") (param $$i i32) (result i64)
    (i64.load16_u $$mem4 offset=1 align=1 (local.get $$i))         ;; 25442 'bc'
  )
  (func (export "16u_good4") (param $$i i32) (result i64)
    (i64.load16_u $$mem4 offset=2 align=2 (local.get $$i))         ;; 25699 'cd'
  )
  (func (export "16u_good5") (param $$i i32) (result i64)
    (i64.load16_u $$mem4 offset=25 align=2 (local.get $$i))        ;; 122 'z\\0'
  )

  (func (export "16s_good1") (param $$i i32) (result i64)
    (i64.load16_s $$mem4 offset=0 (local.get $$i))                 ;; 25185 'ab'
  )
  (func (export "16s_good2") (param $$i i32) (result i64)
    (i64.load16_s $$mem4 align=1 (local.get $$i))                  ;; 25185 'ab'
  )
  (func (export "16s_good3") (param $$i i32) (result i64)
    (i64.load16_s $$mem4 offset=1 align=1 (local.get $$i))         ;; 25442 'bc'
  )
  (func (export "16s_good4") (param $$i i32) (result i64)
    (i64.load16_s $$mem4 offset=2 align=2 (local.get $$i))         ;; 25699 'cd'
  )
  (func (export "16s_good5") (param $$i i32) (result i64)
    (i64.load16_s $$mem4 offset=25 align=2 (local.get $$i))        ;; 122 'z\\0'
  )

  (func (export "32u_good1") (param $$i i32) (result i64)
    (i64.load32_u $$mem4 offset=0 (local.get $$i))                 ;; 1684234849 'abcd'
  )
  (func (export "32u_good2") (param $$i i32) (result i64)
    (i64.load32_u $$mem4 align=1 (local.get $$i))                  ;; 1684234849 'abcd'
  )
  (func (export "32u_good3") (param $$i i32) (result i64)
    (i64.load32_u $$mem4 offset=1 align=1 (local.get $$i))         ;; 1701077858 'bcde'
  )
  (func (export "32u_good4") (param $$i i32) (result i64)
    (i64.load32_u $$mem4 offset=2 align=2 (local.get $$i))         ;; 1717920867 'cdef'
  )
  (func (export "32u_good5") (param $$i i32) (result i64)
    (i64.load32_u $$mem4 offset=25 align=4 (local.get $$i))        ;; 122 'z\\0\\0\\0'
  )

  (func (export "32s_good1") (param $$i i32) (result i64)
    (i64.load32_s $$mem4 offset=0 (local.get $$i))                 ;; 1684234849 'abcd'
  )
  (func (export "32s_good2") (param $$i i32) (result i64)
    (i64.load32_s $$mem4 align=1 (local.get $$i))                  ;; 1684234849 'abcd'
  )
  (func (export "32s_good3") (param $$i i32) (result i64)
    (i64.load32_s $$mem4 offset=1 align=1 (local.get $$i))         ;; 1701077858 'bcde'
  )
  (func (export "32s_good4") (param $$i i32) (result i64)
    (i64.load32_s $$mem4 offset=2 align=2 (local.get $$i))         ;; 1717920867 'cdef'
  )
  (func (export "32s_good5") (param $$i i32) (result i64)
    (i64.load32_s $$mem4 offset=25 align=4 (local.get $$i))        ;; 122 'z\\0\\0\\0'
  )

  (func (export "64_good1") (param $$i i32) (result i64)
    (i64.load $$mem4 offset=0 (local.get $$i))                     ;; 0x6867666564636261 'abcdefgh'
  )
  (func (export "64_good2") (param $$i i32) (result i64)
    (i64.load $$mem4 align=1 (local.get $$i))                      ;; 0x6867666564636261 'abcdefgh'
  )
  (func (export "64_good3") (param $$i i32) (result i64)
    (i64.load $$mem4 offset=1 align=1 (local.get $$i))             ;; 0x6968676665646362 'bcdefghi'
  )
  (func (export "64_good4") (param $$i i32) (result i64)
    (i64.load $$mem4 offset=2 align=2 (local.get $$i))             ;; 0x6a69686766656463 'cdefghij'
  )
  (func (export "64_good5") (param $$i i32) (result i64)
    (i64.load $$mem4 offset=25 align=8 (local.get $$i))            ;; 122 'z\\0\\0\\0\\0\\0\\0\\0'
  )

  (func (export "8u_bad") (param $$i i32)
    (drop (i64.load8_u $$mem4 offset=4294967295 (local.get $$i)))
  )
  (func (export "8s_bad") (param $$i i32)
    (drop (i64.load8_s $$mem4 offset=4294967295 (local.get $$i)))
  )
  (func (export "16u_bad") (param $$i i32)
    (drop (i64.load16_u $$mem4 offset=4294967295 (local.get $$i)))
  )
  (func (export "16s_bad") (param $$i i32)
    (drop (i64.load16_s $$mem4 offset=4294967295 (local.get $$i)))
  )
  (func (export "32u_bad") (param $$i i32)
    (drop (i64.load32_u $$mem4 offset=4294967295 (local.get $$i)))
  )
  (func (export "32s_bad") (param $$i i32)
    (drop (i64.load32_s $$mem4 offset=4294967295 (local.get $$i)))
  )
  (func (export "64_bad") (param $$i i32)
    (drop (i64.load $$mem4 offset=4294967295 (local.get $$i)))
  )
)`);

// ./test/core/multi-memory/address1.wast:146
assert_return(() => invoke($0, `8u_good1`, [0]), [value("i64", 97n)]);

// ./test/core/multi-memory/address1.wast:147
assert_return(() => invoke($0, `8u_good2`, [0]), [value("i64", 97n)]);

// ./test/core/multi-memory/address1.wast:148
assert_return(() => invoke($0, `8u_good3`, [0]), [value("i64", 98n)]);

// ./test/core/multi-memory/address1.wast:149
assert_return(() => invoke($0, `8u_good4`, [0]), [value("i64", 99n)]);

// ./test/core/multi-memory/address1.wast:150
assert_return(() => invoke($0, `8u_good5`, [0]), [value("i64", 122n)]);

// ./test/core/multi-memory/address1.wast:152
assert_return(() => invoke($0, `8s_good1`, [0]), [value("i64", 97n)]);

// ./test/core/multi-memory/address1.wast:153
assert_return(() => invoke($0, `8s_good2`, [0]), [value("i64", 97n)]);

// ./test/core/multi-memory/address1.wast:154
assert_return(() => invoke($0, `8s_good3`, [0]), [value("i64", 98n)]);

// ./test/core/multi-memory/address1.wast:155
assert_return(() => invoke($0, `8s_good4`, [0]), [value("i64", 99n)]);

// ./test/core/multi-memory/address1.wast:156
assert_return(() => invoke($0, `8s_good5`, [0]), [value("i64", 122n)]);

// ./test/core/multi-memory/address1.wast:158
assert_return(() => invoke($0, `16u_good1`, [0]), [value("i64", 25185n)]);

// ./test/core/multi-memory/address1.wast:159
assert_return(() => invoke($0, `16u_good2`, [0]), [value("i64", 25185n)]);

// ./test/core/multi-memory/address1.wast:160
assert_return(() => invoke($0, `16u_good3`, [0]), [value("i64", 25442n)]);

// ./test/core/multi-memory/address1.wast:161
assert_return(() => invoke($0, `16u_good4`, [0]), [value("i64", 25699n)]);

// ./test/core/multi-memory/address1.wast:162
assert_return(() => invoke($0, `16u_good5`, [0]), [value("i64", 122n)]);

// ./test/core/multi-memory/address1.wast:164
assert_return(() => invoke($0, `16s_good1`, [0]), [value("i64", 25185n)]);

// ./test/core/multi-memory/address1.wast:165
assert_return(() => invoke($0, `16s_good2`, [0]), [value("i64", 25185n)]);

// ./test/core/multi-memory/address1.wast:166
assert_return(() => invoke($0, `16s_good3`, [0]), [value("i64", 25442n)]);

// ./test/core/multi-memory/address1.wast:167
assert_return(() => invoke($0, `16s_good4`, [0]), [value("i64", 25699n)]);

// ./test/core/multi-memory/address1.wast:168
assert_return(() => invoke($0, `16s_good5`, [0]), [value("i64", 122n)]);

// ./test/core/multi-memory/address1.wast:170
assert_return(() => invoke($0, `32u_good1`, [0]), [value("i64", 1684234849n)]);

// ./test/core/multi-memory/address1.wast:171
assert_return(() => invoke($0, `32u_good2`, [0]), [value("i64", 1684234849n)]);

// ./test/core/multi-memory/address1.wast:172
assert_return(() => invoke($0, `32u_good3`, [0]), [value("i64", 1701077858n)]);

// ./test/core/multi-memory/address1.wast:173
assert_return(() => invoke($0, `32u_good4`, [0]), [value("i64", 1717920867n)]);

// ./test/core/multi-memory/address1.wast:174
assert_return(() => invoke($0, `32u_good5`, [0]), [value("i64", 122n)]);

// ./test/core/multi-memory/address1.wast:176
assert_return(() => invoke($0, `32s_good1`, [0]), [value("i64", 1684234849n)]);

// ./test/core/multi-memory/address1.wast:177
assert_return(() => invoke($0, `32s_good2`, [0]), [value("i64", 1684234849n)]);

// ./test/core/multi-memory/address1.wast:178
assert_return(() => invoke($0, `32s_good3`, [0]), [value("i64", 1701077858n)]);

// ./test/core/multi-memory/address1.wast:179
assert_return(() => invoke($0, `32s_good4`, [0]), [value("i64", 1717920867n)]);

// ./test/core/multi-memory/address1.wast:180
assert_return(() => invoke($0, `32s_good5`, [0]), [value("i64", 122n)]);

// ./test/core/multi-memory/address1.wast:182
assert_return(() => invoke($0, `64_good1`, [0]), [value("i64", 7523094288207667809n)]);

// ./test/core/multi-memory/address1.wast:183
assert_return(() => invoke($0, `64_good2`, [0]), [value("i64", 7523094288207667809n)]);

// ./test/core/multi-memory/address1.wast:184
assert_return(() => invoke($0, `64_good3`, [0]), [value("i64", 7595434461045744482n)]);

// ./test/core/multi-memory/address1.wast:185
assert_return(() => invoke($0, `64_good4`, [0]), [value("i64", 7667774633883821155n)]);

// ./test/core/multi-memory/address1.wast:186
assert_return(() => invoke($0, `64_good5`, [0]), [value("i64", 122n)]);

// ./test/core/multi-memory/address1.wast:188
assert_return(() => invoke($0, `8u_good1`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:189
assert_return(() => invoke($0, `8u_good2`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:190
assert_return(() => invoke($0, `8u_good3`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:191
assert_return(() => invoke($0, `8u_good4`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:192
assert_return(() => invoke($0, `8u_good5`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:194
assert_return(() => invoke($0, `8s_good1`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:195
assert_return(() => invoke($0, `8s_good2`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:196
assert_return(() => invoke($0, `8s_good3`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:197
assert_return(() => invoke($0, `8s_good4`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:198
assert_return(() => invoke($0, `8s_good5`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:200
assert_return(() => invoke($0, `16u_good1`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:201
assert_return(() => invoke($0, `16u_good2`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:202
assert_return(() => invoke($0, `16u_good3`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:203
assert_return(() => invoke($0, `16u_good4`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:204
assert_return(() => invoke($0, `16u_good5`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:206
assert_return(() => invoke($0, `16s_good1`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:207
assert_return(() => invoke($0, `16s_good2`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:208
assert_return(() => invoke($0, `16s_good3`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:209
assert_return(() => invoke($0, `16s_good4`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:210
assert_return(() => invoke($0, `16s_good5`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:212
assert_return(() => invoke($0, `32u_good1`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:213
assert_return(() => invoke($0, `32u_good2`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:214
assert_return(() => invoke($0, `32u_good3`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:215
assert_return(() => invoke($0, `32u_good4`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:216
assert_return(() => invoke($0, `32u_good5`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:218
assert_return(() => invoke($0, `32s_good1`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:219
assert_return(() => invoke($0, `32s_good2`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:220
assert_return(() => invoke($0, `32s_good3`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:221
assert_return(() => invoke($0, `32s_good4`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:222
assert_return(() => invoke($0, `32s_good5`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:224
assert_return(() => invoke($0, `64_good1`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:225
assert_return(() => invoke($0, `64_good2`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:226
assert_return(() => invoke($0, `64_good3`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:227
assert_return(() => invoke($0, `64_good4`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:228
assert_return(() => invoke($0, `64_good5`, [65503]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:230
assert_return(() => invoke($0, `8u_good1`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:231
assert_return(() => invoke($0, `8u_good2`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:232
assert_return(() => invoke($0, `8u_good3`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:233
assert_return(() => invoke($0, `8u_good4`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:234
assert_return(() => invoke($0, `8u_good5`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:236
assert_return(() => invoke($0, `8s_good1`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:237
assert_return(() => invoke($0, `8s_good2`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:238
assert_return(() => invoke($0, `8s_good3`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:239
assert_return(() => invoke($0, `8s_good4`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:240
assert_return(() => invoke($0, `8s_good5`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:242
assert_return(() => invoke($0, `16u_good1`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:243
assert_return(() => invoke($0, `16u_good2`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:244
assert_return(() => invoke($0, `16u_good3`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:245
assert_return(() => invoke($0, `16u_good4`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:246
assert_return(() => invoke($0, `16u_good5`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:248
assert_return(() => invoke($0, `16s_good1`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:249
assert_return(() => invoke($0, `16s_good2`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:250
assert_return(() => invoke($0, `16s_good3`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:251
assert_return(() => invoke($0, `16s_good4`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:252
assert_return(() => invoke($0, `16s_good5`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:254
assert_return(() => invoke($0, `32u_good1`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:255
assert_return(() => invoke($0, `32u_good2`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:256
assert_return(() => invoke($0, `32u_good3`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:257
assert_return(() => invoke($0, `32u_good4`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:258
assert_return(() => invoke($0, `32u_good5`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:260
assert_return(() => invoke($0, `32s_good1`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:261
assert_return(() => invoke($0, `32s_good2`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:262
assert_return(() => invoke($0, `32s_good3`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:263
assert_return(() => invoke($0, `32s_good4`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:264
assert_return(() => invoke($0, `32s_good5`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:266
assert_return(() => invoke($0, `64_good1`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:267
assert_return(() => invoke($0, `64_good2`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:268
assert_return(() => invoke($0, `64_good3`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:269
assert_return(() => invoke($0, `64_good4`, [65504]), [value("i64", 0n)]);

// ./test/core/multi-memory/address1.wast:270
assert_trap(() => invoke($0, `64_good5`, [65504]), `out of bounds memory access`);

// ./test/core/multi-memory/address1.wast:272
assert_trap(() => invoke($0, `8u_good3`, [-1]), `out of bounds memory access`);

// ./test/core/multi-memory/address1.wast:273
assert_trap(() => invoke($0, `8s_good3`, [-1]), `out of bounds memory access`);

// ./test/core/multi-memory/address1.wast:274
assert_trap(() => invoke($0, `16u_good3`, [-1]), `out of bounds memory access`);

// ./test/core/multi-memory/address1.wast:275
assert_trap(() => invoke($0, `16s_good3`, [-1]), `out of bounds memory access`);

// ./test/core/multi-memory/address1.wast:276
assert_trap(() => invoke($0, `32u_good3`, [-1]), `out of bounds memory access`);

// ./test/core/multi-memory/address1.wast:277
assert_trap(() => invoke($0, `32s_good3`, [-1]), `out of bounds memory access`);

// ./test/core/multi-memory/address1.wast:278
assert_trap(() => invoke($0, `64_good3`, [-1]), `out of bounds memory access`);

// ./test/core/multi-memory/address1.wast:280
assert_trap(() => invoke($0, `8u_bad`, [0]), `out of bounds memory access`);

// ./test/core/multi-memory/address1.wast:281
assert_trap(() => invoke($0, `8s_bad`, [0]), `out of bounds memory access`);

// ./test/core/multi-memory/address1.wast:282
assert_trap(() => invoke($0, `16u_bad`, [0]), `out of bounds memory access`);

// ./test/core/multi-memory/address1.wast:283
assert_trap(() => invoke($0, `16s_bad`, [0]), `out of bounds memory access`);

// ./test/core/multi-memory/address1.wast:284
assert_trap(() => invoke($0, `32u_bad`, [0]), `out of bounds memory access`);

// ./test/core/multi-memory/address1.wast:285
assert_trap(() => invoke($0, `32s_bad`, [0]), `out of bounds memory access`);

// ./test/core/multi-memory/address1.wast:286
assert_trap(() => invoke($0, `64_bad`, [0]), `out of bounds memory access`);

// ./test/core/multi-memory/address1.wast:288
assert_trap(() => invoke($0, `8u_bad`, [1]), `out of bounds memory access`);

// ./test/core/multi-memory/address1.wast:289
assert_trap(() => invoke($0, `8s_bad`, [1]), `out of bounds memory access`);

// ./test/core/multi-memory/address1.wast:290
assert_trap(() => invoke($0, `16u_bad`, [1]), `out of bounds memory access`);

// ./test/core/multi-memory/address1.wast:291
assert_trap(() => invoke($0, `16s_bad`, [1]), `out of bounds memory access`);

// ./test/core/multi-memory/address1.wast:292
assert_trap(() => invoke($0, `32u_bad`, [0]), `out of bounds memory access`);

// ./test/core/multi-memory/address1.wast:293
assert_trap(() => invoke($0, `32s_bad`, [0]), `out of bounds memory access`);

// ./test/core/multi-memory/address1.wast:294
assert_trap(() => invoke($0, `64_bad`, [1]), `out of bounds memory access`);
