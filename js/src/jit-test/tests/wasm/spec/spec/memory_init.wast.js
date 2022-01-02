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

// ./test/core/memory_init.wast

// ./test/core/memory_init.wast:6
let $0 = instantiate(`(module
  (memory (export "memory0") 1 1)
  (data (i32.const 2) "\\03\\01\\04\\01")
  (data "\\02\\07\\01\\08")
  (data (i32.const 12) "\\07\\05\\02\\03\\06")
  (data "\\05\\09\\02\\07\\06")
  (func (export "test")
    (nop))
  (func (export "load8_u") (param i32) (result i32)
    (i32.load8_u (local.get 0))))`);

// ./test/core/memory_init.wast:17
invoke($0, `test`, []);

// ./test/core/memory_init.wast:19
assert_return(() => invoke($0, `load8_u`, [0]), [value("i32", 0)]);

// ./test/core/memory_init.wast:20
assert_return(() => invoke($0, `load8_u`, [1]), [value("i32", 0)]);

// ./test/core/memory_init.wast:21
assert_return(() => invoke($0, `load8_u`, [2]), [value("i32", 3)]);

// ./test/core/memory_init.wast:22
assert_return(() => invoke($0, `load8_u`, [3]), [value("i32", 1)]);

// ./test/core/memory_init.wast:23
assert_return(() => invoke($0, `load8_u`, [4]), [value("i32", 4)]);

// ./test/core/memory_init.wast:24
assert_return(() => invoke($0, `load8_u`, [5]), [value("i32", 1)]);

// ./test/core/memory_init.wast:25
assert_return(() => invoke($0, `load8_u`, [6]), [value("i32", 0)]);

// ./test/core/memory_init.wast:26
assert_return(() => invoke($0, `load8_u`, [7]), [value("i32", 0)]);

// ./test/core/memory_init.wast:27
assert_return(() => invoke($0, `load8_u`, [8]), [value("i32", 0)]);

// ./test/core/memory_init.wast:28
assert_return(() => invoke($0, `load8_u`, [9]), [value("i32", 0)]);

// ./test/core/memory_init.wast:29
assert_return(() => invoke($0, `load8_u`, [10]), [value("i32", 0)]);

// ./test/core/memory_init.wast:30
assert_return(() => invoke($0, `load8_u`, [11]), [value("i32", 0)]);

// ./test/core/memory_init.wast:31
assert_return(() => invoke($0, `load8_u`, [12]), [value("i32", 7)]);

// ./test/core/memory_init.wast:32
assert_return(() => invoke($0, `load8_u`, [13]), [value("i32", 5)]);

// ./test/core/memory_init.wast:33
assert_return(() => invoke($0, `load8_u`, [14]), [value("i32", 2)]);

// ./test/core/memory_init.wast:34
assert_return(() => invoke($0, `load8_u`, [15]), [value("i32", 3)]);

// ./test/core/memory_init.wast:35
assert_return(() => invoke($0, `load8_u`, [16]), [value("i32", 6)]);

// ./test/core/memory_init.wast:36
assert_return(() => invoke($0, `load8_u`, [17]), [value("i32", 0)]);

// ./test/core/memory_init.wast:37
assert_return(() => invoke($0, `load8_u`, [18]), [value("i32", 0)]);

// ./test/core/memory_init.wast:38
assert_return(() => invoke($0, `load8_u`, [19]), [value("i32", 0)]);

// ./test/core/memory_init.wast:39
assert_return(() => invoke($0, `load8_u`, [20]), [value("i32", 0)]);

// ./test/core/memory_init.wast:40
assert_return(() => invoke($0, `load8_u`, [21]), [value("i32", 0)]);

// ./test/core/memory_init.wast:41
assert_return(() => invoke($0, `load8_u`, [22]), [value("i32", 0)]);

// ./test/core/memory_init.wast:42
assert_return(() => invoke($0, `load8_u`, [23]), [value("i32", 0)]);

// ./test/core/memory_init.wast:43
assert_return(() => invoke($0, `load8_u`, [24]), [value("i32", 0)]);

// ./test/core/memory_init.wast:44
assert_return(() => invoke($0, `load8_u`, [25]), [value("i32", 0)]);

// ./test/core/memory_init.wast:45
assert_return(() => invoke($0, `load8_u`, [26]), [value("i32", 0)]);

// ./test/core/memory_init.wast:46
assert_return(() => invoke($0, `load8_u`, [27]), [value("i32", 0)]);

// ./test/core/memory_init.wast:47
assert_return(() => invoke($0, `load8_u`, [28]), [value("i32", 0)]);

// ./test/core/memory_init.wast:48
assert_return(() => invoke($0, `load8_u`, [29]), [value("i32", 0)]);

// ./test/core/memory_init.wast:50
let $1 = instantiate(`(module
  (memory (export "memory0") 1 1)
  (data (i32.const 2) "\\03\\01\\04\\01")
  (data "\\02\\07\\01\\08")
  (data (i32.const 12) "\\07\\05\\02\\03\\06")
  (data "\\05\\09\\02\\07\\06")
  (func (export "test")
    (memory.init 1 (i32.const 7) (i32.const 0) (i32.const 4)))
  (func (export "load8_u") (param i32) (result i32)
    (i32.load8_u (local.get 0))))`);

// ./test/core/memory_init.wast:61
invoke($1, `test`, []);

// ./test/core/memory_init.wast:63
assert_return(() => invoke($1, `load8_u`, [0]), [value("i32", 0)]);

// ./test/core/memory_init.wast:64
assert_return(() => invoke($1, `load8_u`, [1]), [value("i32", 0)]);

// ./test/core/memory_init.wast:65
assert_return(() => invoke($1, `load8_u`, [2]), [value("i32", 3)]);

// ./test/core/memory_init.wast:66
assert_return(() => invoke($1, `load8_u`, [3]), [value("i32", 1)]);

// ./test/core/memory_init.wast:67
assert_return(() => invoke($1, `load8_u`, [4]), [value("i32", 4)]);

// ./test/core/memory_init.wast:68
assert_return(() => invoke($1, `load8_u`, [5]), [value("i32", 1)]);

// ./test/core/memory_init.wast:69
assert_return(() => invoke($1, `load8_u`, [6]), [value("i32", 0)]);

// ./test/core/memory_init.wast:70
assert_return(() => invoke($1, `load8_u`, [7]), [value("i32", 2)]);

// ./test/core/memory_init.wast:71
assert_return(() => invoke($1, `load8_u`, [8]), [value("i32", 7)]);

// ./test/core/memory_init.wast:72
assert_return(() => invoke($1, `load8_u`, [9]), [value("i32", 1)]);

// ./test/core/memory_init.wast:73
assert_return(() => invoke($1, `load8_u`, [10]), [value("i32", 8)]);

// ./test/core/memory_init.wast:74
assert_return(() => invoke($1, `load8_u`, [11]), [value("i32", 0)]);

// ./test/core/memory_init.wast:75
assert_return(() => invoke($1, `load8_u`, [12]), [value("i32", 7)]);

// ./test/core/memory_init.wast:76
assert_return(() => invoke($1, `load8_u`, [13]), [value("i32", 5)]);

// ./test/core/memory_init.wast:77
assert_return(() => invoke($1, `load8_u`, [14]), [value("i32", 2)]);

// ./test/core/memory_init.wast:78
assert_return(() => invoke($1, `load8_u`, [15]), [value("i32", 3)]);

// ./test/core/memory_init.wast:79
assert_return(() => invoke($1, `load8_u`, [16]), [value("i32", 6)]);

// ./test/core/memory_init.wast:80
assert_return(() => invoke($1, `load8_u`, [17]), [value("i32", 0)]);

// ./test/core/memory_init.wast:81
assert_return(() => invoke($1, `load8_u`, [18]), [value("i32", 0)]);

// ./test/core/memory_init.wast:82
assert_return(() => invoke($1, `load8_u`, [19]), [value("i32", 0)]);

// ./test/core/memory_init.wast:83
assert_return(() => invoke($1, `load8_u`, [20]), [value("i32", 0)]);

// ./test/core/memory_init.wast:84
assert_return(() => invoke($1, `load8_u`, [21]), [value("i32", 0)]);

// ./test/core/memory_init.wast:85
assert_return(() => invoke($1, `load8_u`, [22]), [value("i32", 0)]);

// ./test/core/memory_init.wast:86
assert_return(() => invoke($1, `load8_u`, [23]), [value("i32", 0)]);

// ./test/core/memory_init.wast:87
assert_return(() => invoke($1, `load8_u`, [24]), [value("i32", 0)]);

// ./test/core/memory_init.wast:88
assert_return(() => invoke($1, `load8_u`, [25]), [value("i32", 0)]);

// ./test/core/memory_init.wast:89
assert_return(() => invoke($1, `load8_u`, [26]), [value("i32", 0)]);

// ./test/core/memory_init.wast:90
assert_return(() => invoke($1, `load8_u`, [27]), [value("i32", 0)]);

// ./test/core/memory_init.wast:91
assert_return(() => invoke($1, `load8_u`, [28]), [value("i32", 0)]);

// ./test/core/memory_init.wast:92
assert_return(() => invoke($1, `load8_u`, [29]), [value("i32", 0)]);

// ./test/core/memory_init.wast:94
let $2 = instantiate(`(module
  (memory (export "memory0") 1 1)
  (data (i32.const 2) "\\03\\01\\04\\01")
  (data "\\02\\07\\01\\08")
  (data (i32.const 12) "\\07\\05\\02\\03\\06")
  (data "\\05\\09\\02\\07\\06")
  (func (export "test")
    (memory.init 3 (i32.const 15) (i32.const 1) (i32.const 3)))
  (func (export "load8_u") (param i32) (result i32)
    (i32.load8_u (local.get 0))))`);

// ./test/core/memory_init.wast:105
invoke($2, `test`, []);

// ./test/core/memory_init.wast:107
assert_return(() => invoke($2, `load8_u`, [0]), [value("i32", 0)]);

// ./test/core/memory_init.wast:108
assert_return(() => invoke($2, `load8_u`, [1]), [value("i32", 0)]);

// ./test/core/memory_init.wast:109
assert_return(() => invoke($2, `load8_u`, [2]), [value("i32", 3)]);

// ./test/core/memory_init.wast:110
assert_return(() => invoke($2, `load8_u`, [3]), [value("i32", 1)]);

// ./test/core/memory_init.wast:111
assert_return(() => invoke($2, `load8_u`, [4]), [value("i32", 4)]);

// ./test/core/memory_init.wast:112
assert_return(() => invoke($2, `load8_u`, [5]), [value("i32", 1)]);

// ./test/core/memory_init.wast:113
assert_return(() => invoke($2, `load8_u`, [6]), [value("i32", 0)]);

// ./test/core/memory_init.wast:114
assert_return(() => invoke($2, `load8_u`, [7]), [value("i32", 0)]);

// ./test/core/memory_init.wast:115
assert_return(() => invoke($2, `load8_u`, [8]), [value("i32", 0)]);

// ./test/core/memory_init.wast:116
assert_return(() => invoke($2, `load8_u`, [9]), [value("i32", 0)]);

// ./test/core/memory_init.wast:117
assert_return(() => invoke($2, `load8_u`, [10]), [value("i32", 0)]);

// ./test/core/memory_init.wast:118
assert_return(() => invoke($2, `load8_u`, [11]), [value("i32", 0)]);

// ./test/core/memory_init.wast:119
assert_return(() => invoke($2, `load8_u`, [12]), [value("i32", 7)]);

// ./test/core/memory_init.wast:120
assert_return(() => invoke($2, `load8_u`, [13]), [value("i32", 5)]);

// ./test/core/memory_init.wast:121
assert_return(() => invoke($2, `load8_u`, [14]), [value("i32", 2)]);

// ./test/core/memory_init.wast:122
assert_return(() => invoke($2, `load8_u`, [15]), [value("i32", 9)]);

// ./test/core/memory_init.wast:123
assert_return(() => invoke($2, `load8_u`, [16]), [value("i32", 2)]);

// ./test/core/memory_init.wast:124
assert_return(() => invoke($2, `load8_u`, [17]), [value("i32", 7)]);

// ./test/core/memory_init.wast:125
assert_return(() => invoke($2, `load8_u`, [18]), [value("i32", 0)]);

// ./test/core/memory_init.wast:126
assert_return(() => invoke($2, `load8_u`, [19]), [value("i32", 0)]);

// ./test/core/memory_init.wast:127
assert_return(() => invoke($2, `load8_u`, [20]), [value("i32", 0)]);

// ./test/core/memory_init.wast:128
assert_return(() => invoke($2, `load8_u`, [21]), [value("i32", 0)]);

// ./test/core/memory_init.wast:129
assert_return(() => invoke($2, `load8_u`, [22]), [value("i32", 0)]);

// ./test/core/memory_init.wast:130
assert_return(() => invoke($2, `load8_u`, [23]), [value("i32", 0)]);

// ./test/core/memory_init.wast:131
assert_return(() => invoke($2, `load8_u`, [24]), [value("i32", 0)]);

// ./test/core/memory_init.wast:132
assert_return(() => invoke($2, `load8_u`, [25]), [value("i32", 0)]);

// ./test/core/memory_init.wast:133
assert_return(() => invoke($2, `load8_u`, [26]), [value("i32", 0)]);

// ./test/core/memory_init.wast:134
assert_return(() => invoke($2, `load8_u`, [27]), [value("i32", 0)]);

// ./test/core/memory_init.wast:135
assert_return(() => invoke($2, `load8_u`, [28]), [value("i32", 0)]);

// ./test/core/memory_init.wast:136
assert_return(() => invoke($2, `load8_u`, [29]), [value("i32", 0)]);

// ./test/core/memory_init.wast:138
let $3 = instantiate(`(module
  (memory (export "memory0") 1 1)
  (data (i32.const 2) "\\03\\01\\04\\01")
  (data "\\02\\07\\01\\08")
  (data (i32.const 12) "\\07\\05\\02\\03\\06")
  (data "\\05\\09\\02\\07\\06")
  (func (export "test")
    (memory.init 1 (i32.const 7) (i32.const 0) (i32.const 4))
    (data.drop 1)
    (memory.init 3 (i32.const 15) (i32.const 1) (i32.const 3))
    (data.drop 3)
    (memory.copy (i32.const 20) (i32.const 15) (i32.const 5))
    (memory.copy (i32.const 21) (i32.const 29) (i32.const 1))
    (memory.copy (i32.const 24) (i32.const 10) (i32.const 1))
    (memory.copy (i32.const 13) (i32.const 11) (i32.const 4))
    (memory.copy (i32.const 19) (i32.const 20) (i32.const 5)))
  (func (export "load8_u") (param i32) (result i32)
    (i32.load8_u (local.get 0))))`);

// ./test/core/memory_init.wast:157
invoke($3, `test`, []);

// ./test/core/memory_init.wast:159
assert_return(() => invoke($3, `load8_u`, [0]), [value("i32", 0)]);

// ./test/core/memory_init.wast:160
assert_return(() => invoke($3, `load8_u`, [1]), [value("i32", 0)]);

// ./test/core/memory_init.wast:161
assert_return(() => invoke($3, `load8_u`, [2]), [value("i32", 3)]);

// ./test/core/memory_init.wast:162
assert_return(() => invoke($3, `load8_u`, [3]), [value("i32", 1)]);

// ./test/core/memory_init.wast:163
assert_return(() => invoke($3, `load8_u`, [4]), [value("i32", 4)]);

// ./test/core/memory_init.wast:164
assert_return(() => invoke($3, `load8_u`, [5]), [value("i32", 1)]);

// ./test/core/memory_init.wast:165
assert_return(() => invoke($3, `load8_u`, [6]), [value("i32", 0)]);

// ./test/core/memory_init.wast:166
assert_return(() => invoke($3, `load8_u`, [7]), [value("i32", 2)]);

// ./test/core/memory_init.wast:167
assert_return(() => invoke($3, `load8_u`, [8]), [value("i32", 7)]);

// ./test/core/memory_init.wast:168
assert_return(() => invoke($3, `load8_u`, [9]), [value("i32", 1)]);

// ./test/core/memory_init.wast:169
assert_return(() => invoke($3, `load8_u`, [10]), [value("i32", 8)]);

// ./test/core/memory_init.wast:170
assert_return(() => invoke($3, `load8_u`, [11]), [value("i32", 0)]);

// ./test/core/memory_init.wast:171
assert_return(() => invoke($3, `load8_u`, [12]), [value("i32", 7)]);

// ./test/core/memory_init.wast:172
assert_return(() => invoke($3, `load8_u`, [13]), [value("i32", 0)]);

// ./test/core/memory_init.wast:173
assert_return(() => invoke($3, `load8_u`, [14]), [value("i32", 7)]);

// ./test/core/memory_init.wast:174
assert_return(() => invoke($3, `load8_u`, [15]), [value("i32", 5)]);

// ./test/core/memory_init.wast:175
assert_return(() => invoke($3, `load8_u`, [16]), [value("i32", 2)]);

// ./test/core/memory_init.wast:176
assert_return(() => invoke($3, `load8_u`, [17]), [value("i32", 7)]);

// ./test/core/memory_init.wast:177
assert_return(() => invoke($3, `load8_u`, [18]), [value("i32", 0)]);

// ./test/core/memory_init.wast:178
assert_return(() => invoke($3, `load8_u`, [19]), [value("i32", 9)]);

// ./test/core/memory_init.wast:179
assert_return(() => invoke($3, `load8_u`, [20]), [value("i32", 0)]);

// ./test/core/memory_init.wast:180
assert_return(() => invoke($3, `load8_u`, [21]), [value("i32", 7)]);

// ./test/core/memory_init.wast:181
assert_return(() => invoke($3, `load8_u`, [22]), [value("i32", 0)]);

// ./test/core/memory_init.wast:182
assert_return(() => invoke($3, `load8_u`, [23]), [value("i32", 8)]);

// ./test/core/memory_init.wast:183
assert_return(() => invoke($3, `load8_u`, [24]), [value("i32", 8)]);

// ./test/core/memory_init.wast:184
assert_return(() => invoke($3, `load8_u`, [25]), [value("i32", 0)]);

// ./test/core/memory_init.wast:185
assert_return(() => invoke($3, `load8_u`, [26]), [value("i32", 0)]);

// ./test/core/memory_init.wast:186
assert_return(() => invoke($3, `load8_u`, [27]), [value("i32", 0)]);

// ./test/core/memory_init.wast:187
assert_return(() => invoke($3, `load8_u`, [28]), [value("i32", 0)]);

// ./test/core/memory_init.wast:188
assert_return(() => invoke($3, `load8_u`, [29]), [value("i32", 0)]);

// ./test/core/memory_init.wast:189
assert_invalid(() =>
  instantiate(`(module
     (func (export "test")
       (data.drop 0)))`), `unknown data segment`);

// ./test/core/memory_init.wast:195
assert_invalid(() =>
  instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (data.drop 4)))`), `unknown data segment`);

// ./test/core/memory_init.wast:203
let $4 = instantiate(`(module
  (memory 1)
    (data "\\37")
  (func (export "test")
    (data.drop 0)
    (data.drop 0)))`);

// ./test/core/memory_init.wast:209
invoke($4, `test`, []);

// ./test/core/memory_init.wast:211
let $5 = instantiate(`(module
  (memory 1)
    (data "\\37")
  (func (export "test")
    (data.drop 0)
    (memory.init 0 (i32.const 1234) (i32.const 1) (i32.const 1))))`);

// ./test/core/memory_init.wast:217
assert_trap(() => invoke($5, `test`, []), `out of bounds memory access`);

// ./test/core/memory_init.wast:219
let $6 = instantiate(`(module
   (memory 1)
   (data (i32.const 0) "\\37")
   (func (export "test")
     (memory.init 0 (i32.const 1234) (i32.const 1) (i32.const 1))))`);

// ./test/core/memory_init.wast:224
assert_trap(() => invoke($6, `test`, []), `out of bounds memory access`);

// ./test/core/memory_init.wast:226
assert_invalid(
  () =>
    instantiate(`(module
    (func (export "test")
      (memory.init 1 (i32.const 1234) (i32.const 1) (i32.const 1))))`),
  `unknown memory 0`,
);

// ./test/core/memory_init.wast:232
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 1 (i32.const 1234) (i32.const 1) (i32.const 1))))`),
  `unknown data segment 1`,
);

// ./test/core/memory_init.wast:240
let $7 = instantiate(`(module
  (memory 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i32.const 1) (i32.const 0) (i32.const 1))
    (memory.init 0 (i32.const 1) (i32.const 0) (i32.const 1))))`);

// ./test/core/memory_init.wast:246
invoke($7, `test`, []);

// ./test/core/memory_init.wast:248
let $8 = instantiate(`(module
  (memory 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i32.const 1234) (i32.const 0) (i32.const 5))))`);

// ./test/core/memory_init.wast:253
assert_trap(() => invoke($8, `test`, []), `out of bounds memory access`);

// ./test/core/memory_init.wast:255
let $9 = instantiate(`(module
  (memory 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i32.const 1234) (i32.const 2) (i32.const 3))))`);

// ./test/core/memory_init.wast:260
assert_trap(() => invoke($9, `test`, []), `out of bounds memory access`);

// ./test/core/memory_init.wast:262
let $10 = instantiate(`(module
  (memory 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i32.const 0xFFFE) (i32.const 1) (i32.const 3))))`);

// ./test/core/memory_init.wast:267
assert_trap(() => invoke($10, `test`, []), `out of bounds memory access`);

// ./test/core/memory_init.wast:269
let $11 = instantiate(`(module
  (memory 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i32.const 1234) (i32.const 4) (i32.const 0))))`);

// ./test/core/memory_init.wast:274
assert_trap(() => invoke($11, `test`, []), `out of bounds memory access`);

// ./test/core/memory_init.wast:276
let $12 = instantiate(`(module
  (memory 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i32.const 1234) (i32.const 1) (i32.const 0))))`);

// ./test/core/memory_init.wast:281
invoke($12, `test`, []);

// ./test/core/memory_init.wast:283
let $13 = instantiate(`(module
  (memory 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i32.const 0x10001) (i32.const 0) (i32.const 0))))`);

// ./test/core/memory_init.wast:288
assert_trap(() => invoke($13, `test`, []), `out of bounds memory access`);

// ./test/core/memory_init.wast:290
let $14 = instantiate(`(module
  (memory 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i32.const 0x10000) (i32.const 0) (i32.const 0))))`);

// ./test/core/memory_init.wast:295
invoke($14, `test`, []);

// ./test/core/memory_init.wast:297
let $15 = instantiate(`(module
  (memory 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i32.const 0x10000) (i32.const 1) (i32.const 0))))`);

// ./test/core/memory_init.wast:302
invoke($15, `test`, []);

// ./test/core/memory_init.wast:304
let $16 = instantiate(`(module
  (memory 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i32.const 0x10001) (i32.const 4) (i32.const 0))))`);

// ./test/core/memory_init.wast:309
assert_trap(() => invoke($16, `test`, []), `out of bounds memory access`);

// ./test/core/memory_init.wast:311
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (i32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:319
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (i32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:327
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (i32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:335
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (f32.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:343
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (f32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:351
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (f32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:359
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (f32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:367
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (i64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:375
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (i64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:383
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (i64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:391
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (i64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:399
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (f64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:407
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (f64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:415
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (f64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:423
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (f64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:431
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (i32.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:439
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (i32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:447
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (i32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:455
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (i32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:463
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (f32.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:471
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (f32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:479
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (f32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:487
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (f32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:495
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (i64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:503
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (i64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:511
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (i64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:519
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (i64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:527
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (f64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:535
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (f64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:543
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (f64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:551
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (f64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:559
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (i32.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:567
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (i32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:575
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (i32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:583
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (i32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:591
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (f32.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:599
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (f32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:607
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (f32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:615
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (f32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:623
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (i64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:631
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (i64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:639
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (i64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:647
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (i64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:655
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (f64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:663
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (f64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:671
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (f64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:679
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (f64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:687
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (i32.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:695
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (i32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:703
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (i32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:711
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (i32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:719
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (f32.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:727
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (f32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:735
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (f32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:743
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (f32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:751
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (i64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:759
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (i64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:767
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (i64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:775
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (i64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:783
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (f64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:791
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (f64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:799
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (f64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:807
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (f64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:815
let $17 = instantiate(`(module
  (memory 1 1 )
  (data "\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42")
   
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "run") (param $$offs i32) (param $$len i32)
    (memory.init 0 (local.get $$offs) (i32.const 0) (local.get $$len))))`);

// ./test/core/memory_init.wast:833
assert_trap(
  () => invoke($17, `run`, [65528, 16]),
  `out of bounds memory access`,
);

// ./test/core/memory_init.wast:836
assert_return(() => invoke($17, `checkRange`, [0, 1, 0]), [value("i32", -1)]);

// ./test/core/memory_init.wast:838
let $18 = instantiate(`(module
  (memory 1 1 )
  (data "\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42")
   
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "run") (param $$offs i32) (param $$len i32)
    (memory.init 0 (local.get $$offs) (i32.const 0) (local.get $$len))))`);

// ./test/core/memory_init.wast:856
assert_trap(
  () => invoke($18, `run`, [65527, 16]),
  `out of bounds memory access`,
);

// ./test/core/memory_init.wast:859
assert_return(() => invoke($18, `checkRange`, [0, 1, 0]), [value("i32", -1)]);

// ./test/core/memory_init.wast:861
let $19 = instantiate(`(module
  (memory 1 1 )
  (data "\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42")
   
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "run") (param $$offs i32) (param $$len i32)
    (memory.init 0 (local.get $$offs) (i32.const 0) (local.get $$len))))`);

// ./test/core/memory_init.wast:879
assert_trap(
  () => invoke($19, `run`, [65472, 30]),
  `out of bounds memory access`,
);

// ./test/core/memory_init.wast:882
assert_return(() => invoke($19, `checkRange`, [0, 1, 0]), [value("i32", -1)]);

// ./test/core/memory_init.wast:884
let $20 = instantiate(`(module
  (memory 1 1 )
  (data "\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42")
   
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "run") (param $$offs i32) (param $$len i32)
    (memory.init 0 (local.get $$offs) (i32.const 0) (local.get $$len))))`);

// ./test/core/memory_init.wast:902
assert_trap(
  () => invoke($20, `run`, [65473, 31]),
  `out of bounds memory access`,
);

// ./test/core/memory_init.wast:905
assert_return(() => invoke($20, `checkRange`, [0, 1, 0]), [value("i32", -1)]);

// ./test/core/memory_init.wast:907
let $21 = instantiate(`(module
  (memory 1  )
  (data "\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42")
   
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "run") (param $$offs i32) (param $$len i32)
    (memory.init 0 (local.get $$offs) (i32.const 0) (local.get $$len))))`);

// ./test/core/memory_init.wast:925
assert_trap(
  () => invoke($21, `run`, [65528, -256]),
  `out of bounds memory access`,
);

// ./test/core/memory_init.wast:928
assert_return(() => invoke($21, `checkRange`, [0, 1, 0]), [value("i32", -1)]);

// ./test/core/memory_init.wast:930
let $22 = instantiate(`(module
  (memory 1  )
  (data "\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42")
   
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "run") (param $$offs i32) (param $$len i32)
    (memory.init 0 (local.get $$offs) (i32.const 0) (local.get $$len))))`);

// ./test/core/memory_init.wast:948
assert_trap(() => invoke($22, `run`, [0, -4]), `out of bounds memory access`);

// ./test/core/memory_init.wast:951
assert_return(() => invoke($22, `checkRange`, [0, 1, 0]), [value("i32", -1)]);

// ./test/core/memory_init.wast:954
let $23 = instantiate(`(module
  (memory 1)
  ;; 65 data segments. 64 is the smallest positive number that is encoded
  ;; differently as a signed LEB.
  (data "") (data "") (data "") (data "") (data "") (data "") (data "") (data "")
  (data "") (data "") (data "") (data "") (data "") (data "") (data "") (data "")
  (data "") (data "") (data "") (data "") (data "") (data "") (data "") (data "")
  (data "") (data "") (data "") (data "") (data "") (data "") (data "") (data "")
  (data "") (data "") (data "") (data "") (data "") (data "") (data "") (data "")
  (data "") (data "") (data "") (data "") (data "") (data "") (data "") (data "")
  (data "") (data "") (data "") (data "") (data "") (data "") (data "") (data "")
  (data "") (data "") (data "") (data "") (data "") (data "") (data "") (data "")
  (data "")
  (func (memory.init 64 (i32.const 0) (i32.const 0) (i32.const 0))))`);
