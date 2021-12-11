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

// ./test/core/table_init.wast

// ./test/core/table_init.wast:6
let $0 = instantiate(`(module
  (func (export "ef0") (result i32) (i32.const 0))
  (func (export "ef1") (result i32) (i32.const 1))
  (func (export "ef2") (result i32) (i32.const 2))
  (func (export "ef3") (result i32) (i32.const 3))
  (func (export "ef4") (result i32) (i32.const 4))
)`);

// ./test/core/table_init.wast:13
register($0, `a`);

// ./test/core/table_init.wast:15
let $1 = instantiate(`(module
  (type (func (result i32)))  ;; type #0
  (import "a" "ef0" (func (result i32)))    ;; index 0
  (import "a" "ef1" (func (result i32)))
  (import "a" "ef2" (func (result i32)))
  (import "a" "ef3" (func (result i32)))
  (import "a" "ef4" (func (result i32)))    ;; index 4
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 5))  ;; index 5
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))  ;; index 9
  (func (export "test")
    (table.init $$t0 1 (i32.const 7) (i32.const 0) (i32.const 4)))
  (func (export "check") (param i32) (result i32)
    (call_indirect $$t0 (type 0) (local.get 0)))
)`);

// ./test/core/table_init.wast:41
invoke($1, `test`, []);

// ./test/core/table_init.wast:42
assert_trap(() => invoke($1, `check`, [0]), `uninitialized element`);

// ./test/core/table_init.wast:43
assert_trap(() => invoke($1, `check`, [1]), `uninitialized element`);

// ./test/core/table_init.wast:44
assert_return(() => invoke($1, `check`, [2]), [value("i32", 3)]);

// ./test/core/table_init.wast:45
assert_return(() => invoke($1, `check`, [3]), [value("i32", 1)]);

// ./test/core/table_init.wast:46
assert_return(() => invoke($1, `check`, [4]), [value("i32", 4)]);

// ./test/core/table_init.wast:47
assert_return(() => invoke($1, `check`, [5]), [value("i32", 1)]);

// ./test/core/table_init.wast:48
assert_trap(() => invoke($1, `check`, [6]), `uninitialized element`);

// ./test/core/table_init.wast:49
assert_return(() => invoke($1, `check`, [7]), [value("i32", 2)]);

// ./test/core/table_init.wast:50
assert_return(() => invoke($1, `check`, [8]), [value("i32", 7)]);

// ./test/core/table_init.wast:51
assert_return(() => invoke($1, `check`, [9]), [value("i32", 1)]);

// ./test/core/table_init.wast:52
assert_return(() => invoke($1, `check`, [10]), [value("i32", 8)]);

// ./test/core/table_init.wast:53
assert_trap(() => invoke($1, `check`, [11]), `uninitialized element`);

// ./test/core/table_init.wast:54
assert_return(() => invoke($1, `check`, [12]), [value("i32", 7)]);

// ./test/core/table_init.wast:55
assert_return(() => invoke($1, `check`, [13]), [value("i32", 5)]);

// ./test/core/table_init.wast:56
assert_return(() => invoke($1, `check`, [14]), [value("i32", 2)]);

// ./test/core/table_init.wast:57
assert_return(() => invoke($1, `check`, [15]), [value("i32", 3)]);

// ./test/core/table_init.wast:58
assert_return(() => invoke($1, `check`, [16]), [value("i32", 6)]);

// ./test/core/table_init.wast:59
assert_trap(() => invoke($1, `check`, [17]), `uninitialized element`);

// ./test/core/table_init.wast:60
assert_trap(() => invoke($1, `check`, [18]), `uninitialized element`);

// ./test/core/table_init.wast:61
assert_trap(() => invoke($1, `check`, [19]), `uninitialized element`);

// ./test/core/table_init.wast:62
assert_trap(() => invoke($1, `check`, [20]), `uninitialized element`);

// ./test/core/table_init.wast:63
assert_trap(() => invoke($1, `check`, [21]), `uninitialized element`);

// ./test/core/table_init.wast:64
assert_trap(() => invoke($1, `check`, [22]), `uninitialized element`);

// ./test/core/table_init.wast:65
assert_trap(() => invoke($1, `check`, [23]), `uninitialized element`);

// ./test/core/table_init.wast:66
assert_trap(() => invoke($1, `check`, [24]), `uninitialized element`);

// ./test/core/table_init.wast:67
assert_trap(() => invoke($1, `check`, [25]), `uninitialized element`);

// ./test/core/table_init.wast:68
assert_trap(() => invoke($1, `check`, [26]), `uninitialized element`);

// ./test/core/table_init.wast:69
assert_trap(() => invoke($1, `check`, [27]), `uninitialized element`);

// ./test/core/table_init.wast:70
assert_trap(() => invoke($1, `check`, [28]), `uninitialized element`);

// ./test/core/table_init.wast:71
assert_trap(() => invoke($1, `check`, [29]), `uninitialized element`);

// ./test/core/table_init.wast:73
let $2 = instantiate(`(module
  (type (func (result i32)))  ;; type #0
  (import "a" "ef0" (func (result i32)))    ;; index 0
  (import "a" "ef1" (func (result i32)))
  (import "a" "ef2" (func (result i32)))
  (import "a" "ef3" (func (result i32)))
  (import "a" "ef4" (func (result i32)))    ;; index 4
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 5))  ;; index 5
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))  ;; index 9
  (func (export "test")
    (table.init $$t0 3 (i32.const 15) (i32.const 1) (i32.const 3)))
  (func (export "check") (param i32) (result i32)
    (call_indirect $$t0 (type 0) (local.get 0)))
)`);

// ./test/core/table_init.wast:99
invoke($2, `test`, []);

// ./test/core/table_init.wast:100
assert_trap(() => invoke($2, `check`, [0]), `uninitialized element`);

// ./test/core/table_init.wast:101
assert_trap(() => invoke($2, `check`, [1]), `uninitialized element`);

// ./test/core/table_init.wast:102
assert_return(() => invoke($2, `check`, [2]), [value("i32", 3)]);

// ./test/core/table_init.wast:103
assert_return(() => invoke($2, `check`, [3]), [value("i32", 1)]);

// ./test/core/table_init.wast:104
assert_return(() => invoke($2, `check`, [4]), [value("i32", 4)]);

// ./test/core/table_init.wast:105
assert_return(() => invoke($2, `check`, [5]), [value("i32", 1)]);

// ./test/core/table_init.wast:106
assert_trap(() => invoke($2, `check`, [6]), `uninitialized element`);

// ./test/core/table_init.wast:107
assert_trap(() => invoke($2, `check`, [7]), `uninitialized element`);

// ./test/core/table_init.wast:108
assert_trap(() => invoke($2, `check`, [8]), `uninitialized element`);

// ./test/core/table_init.wast:109
assert_trap(() => invoke($2, `check`, [9]), `uninitialized element`);

// ./test/core/table_init.wast:110
assert_trap(() => invoke($2, `check`, [10]), `uninitialized element`);

// ./test/core/table_init.wast:111
assert_trap(() => invoke($2, `check`, [11]), `uninitialized element`);

// ./test/core/table_init.wast:112
assert_return(() => invoke($2, `check`, [12]), [value("i32", 7)]);

// ./test/core/table_init.wast:113
assert_return(() => invoke($2, `check`, [13]), [value("i32", 5)]);

// ./test/core/table_init.wast:114
assert_return(() => invoke($2, `check`, [14]), [value("i32", 2)]);

// ./test/core/table_init.wast:115
assert_return(() => invoke($2, `check`, [15]), [value("i32", 9)]);

// ./test/core/table_init.wast:116
assert_return(() => invoke($2, `check`, [16]), [value("i32", 2)]);

// ./test/core/table_init.wast:117
assert_return(() => invoke($2, `check`, [17]), [value("i32", 7)]);

// ./test/core/table_init.wast:118
assert_trap(() => invoke($2, `check`, [18]), `uninitialized element`);

// ./test/core/table_init.wast:119
assert_trap(() => invoke($2, `check`, [19]), `uninitialized element`);

// ./test/core/table_init.wast:120
assert_trap(() => invoke($2, `check`, [20]), `uninitialized element`);

// ./test/core/table_init.wast:121
assert_trap(() => invoke($2, `check`, [21]), `uninitialized element`);

// ./test/core/table_init.wast:122
assert_trap(() => invoke($2, `check`, [22]), `uninitialized element`);

// ./test/core/table_init.wast:123
assert_trap(() => invoke($2, `check`, [23]), `uninitialized element`);

// ./test/core/table_init.wast:124
assert_trap(() => invoke($2, `check`, [24]), `uninitialized element`);

// ./test/core/table_init.wast:125
assert_trap(() => invoke($2, `check`, [25]), `uninitialized element`);

// ./test/core/table_init.wast:126
assert_trap(() => invoke($2, `check`, [26]), `uninitialized element`);

// ./test/core/table_init.wast:127
assert_trap(() => invoke($2, `check`, [27]), `uninitialized element`);

// ./test/core/table_init.wast:128
assert_trap(() => invoke($2, `check`, [28]), `uninitialized element`);

// ./test/core/table_init.wast:129
assert_trap(() => invoke($2, `check`, [29]), `uninitialized element`);

// ./test/core/table_init.wast:131
let $3 = instantiate(`(module
  (type (func (result i32)))  ;; type #0
  (import "a" "ef0" (func (result i32)))    ;; index 0
  (import "a" "ef1" (func (result i32)))
  (import "a" "ef2" (func (result i32)))
  (import "a" "ef3" (func (result i32)))
  (import "a" "ef4" (func (result i32)))    ;; index 4
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 5))  ;; index 5
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))  ;; index 9
  (func (export "test")
    (table.init $$t0 1 (i32.const 7) (i32.const 0) (i32.const 4))
         (elem.drop 1)
         (table.init $$t0 3 (i32.const 15) (i32.const 1) (i32.const 3))
         (elem.drop 3)
         (table.copy $$t0 0 (i32.const 20) (i32.const 15) (i32.const 5))
         (table.copy $$t0 0 (i32.const 21) (i32.const 29) (i32.const 1))
         (table.copy $$t0 0 (i32.const 24) (i32.const 10) (i32.const 1))
         (table.copy $$t0 0 (i32.const 13) (i32.const 11) (i32.const 4))
         (table.copy $$t0 0 (i32.const 19) (i32.const 20) (i32.const 5)))
  (func (export "check") (param i32) (result i32)
    (call_indirect $$t0 (type 0) (local.get 0)))
)`);

// ./test/core/table_init.wast:165
invoke($3, `test`, []);

// ./test/core/table_init.wast:166
assert_trap(() => invoke($3, `check`, [0]), `uninitialized element`);

// ./test/core/table_init.wast:167
assert_trap(() => invoke($3, `check`, [1]), `uninitialized element`);

// ./test/core/table_init.wast:168
assert_return(() => invoke($3, `check`, [2]), [value("i32", 3)]);

// ./test/core/table_init.wast:169
assert_return(() => invoke($3, `check`, [3]), [value("i32", 1)]);

// ./test/core/table_init.wast:170
assert_return(() => invoke($3, `check`, [4]), [value("i32", 4)]);

// ./test/core/table_init.wast:171
assert_return(() => invoke($3, `check`, [5]), [value("i32", 1)]);

// ./test/core/table_init.wast:172
assert_trap(() => invoke($3, `check`, [6]), `uninitialized element`);

// ./test/core/table_init.wast:173
assert_return(() => invoke($3, `check`, [7]), [value("i32", 2)]);

// ./test/core/table_init.wast:174
assert_return(() => invoke($3, `check`, [8]), [value("i32", 7)]);

// ./test/core/table_init.wast:175
assert_return(() => invoke($3, `check`, [9]), [value("i32", 1)]);

// ./test/core/table_init.wast:176
assert_return(() => invoke($3, `check`, [10]), [value("i32", 8)]);

// ./test/core/table_init.wast:177
assert_trap(() => invoke($3, `check`, [11]), `uninitialized element`);

// ./test/core/table_init.wast:178
assert_return(() => invoke($3, `check`, [12]), [value("i32", 7)]);

// ./test/core/table_init.wast:179
assert_trap(() => invoke($3, `check`, [13]), `uninitialized element`);

// ./test/core/table_init.wast:180
assert_return(() => invoke($3, `check`, [14]), [value("i32", 7)]);

// ./test/core/table_init.wast:181
assert_return(() => invoke($3, `check`, [15]), [value("i32", 5)]);

// ./test/core/table_init.wast:182
assert_return(() => invoke($3, `check`, [16]), [value("i32", 2)]);

// ./test/core/table_init.wast:183
assert_return(() => invoke($3, `check`, [17]), [value("i32", 7)]);

// ./test/core/table_init.wast:184
assert_trap(() => invoke($3, `check`, [18]), `uninitialized element`);

// ./test/core/table_init.wast:185
assert_return(() => invoke($3, `check`, [19]), [value("i32", 9)]);

// ./test/core/table_init.wast:186
assert_trap(() => invoke($3, `check`, [20]), `uninitialized element`);

// ./test/core/table_init.wast:187
assert_return(() => invoke($3, `check`, [21]), [value("i32", 7)]);

// ./test/core/table_init.wast:188
assert_trap(() => invoke($3, `check`, [22]), `uninitialized element`);

// ./test/core/table_init.wast:189
assert_return(() => invoke($3, `check`, [23]), [value("i32", 8)]);

// ./test/core/table_init.wast:190
assert_return(() => invoke($3, `check`, [24]), [value("i32", 8)]);

// ./test/core/table_init.wast:191
assert_trap(() => invoke($3, `check`, [25]), `uninitialized element`);

// ./test/core/table_init.wast:192
assert_trap(() => invoke($3, `check`, [26]), `uninitialized element`);

// ./test/core/table_init.wast:193
assert_trap(() => invoke($3, `check`, [27]), `uninitialized element`);

// ./test/core/table_init.wast:194
assert_trap(() => invoke($3, `check`, [28]), `uninitialized element`);

// ./test/core/table_init.wast:195
assert_trap(() => invoke($3, `check`, [29]), `uninitialized element`);

// ./test/core/table_init.wast:197
let $4 = instantiate(`(module
  (type (func (result i32)))  ;; type #0
  (import "a" "ef0" (func (result i32)))    ;; index 0
  (import "a" "ef1" (func (result i32)))
  (import "a" "ef2" (func (result i32)))
  (import "a" "ef3" (func (result i32)))
  (import "a" "ef4" (func (result i32)))    ;; index 4
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t1) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t1) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 5))  ;; index 5
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))  ;; index 9
  (func (export "test")
    (table.init $$t1 1 (i32.const 7) (i32.const 0) (i32.const 4)))
  (func (export "check") (param i32) (result i32)
    (call_indirect $$t1 (type 0) (local.get 0)))
)`);

// ./test/core/table_init.wast:223
invoke($4, `test`, []);

// ./test/core/table_init.wast:224
assert_trap(() => invoke($4, `check`, [0]), `uninitialized element`);

// ./test/core/table_init.wast:225
assert_trap(() => invoke($4, `check`, [1]), `uninitialized element`);

// ./test/core/table_init.wast:226
assert_return(() => invoke($4, `check`, [2]), [value("i32", 3)]);

// ./test/core/table_init.wast:227
assert_return(() => invoke($4, `check`, [3]), [value("i32", 1)]);

// ./test/core/table_init.wast:228
assert_return(() => invoke($4, `check`, [4]), [value("i32", 4)]);

// ./test/core/table_init.wast:229
assert_return(() => invoke($4, `check`, [5]), [value("i32", 1)]);

// ./test/core/table_init.wast:230
assert_trap(() => invoke($4, `check`, [6]), `uninitialized element`);

// ./test/core/table_init.wast:231
assert_return(() => invoke($4, `check`, [7]), [value("i32", 2)]);

// ./test/core/table_init.wast:232
assert_return(() => invoke($4, `check`, [8]), [value("i32", 7)]);

// ./test/core/table_init.wast:233
assert_return(() => invoke($4, `check`, [9]), [value("i32", 1)]);

// ./test/core/table_init.wast:234
assert_return(() => invoke($4, `check`, [10]), [value("i32", 8)]);

// ./test/core/table_init.wast:235
assert_trap(() => invoke($4, `check`, [11]), `uninitialized element`);

// ./test/core/table_init.wast:236
assert_return(() => invoke($4, `check`, [12]), [value("i32", 7)]);

// ./test/core/table_init.wast:237
assert_return(() => invoke($4, `check`, [13]), [value("i32", 5)]);

// ./test/core/table_init.wast:238
assert_return(() => invoke($4, `check`, [14]), [value("i32", 2)]);

// ./test/core/table_init.wast:239
assert_return(() => invoke($4, `check`, [15]), [value("i32", 3)]);

// ./test/core/table_init.wast:240
assert_return(() => invoke($4, `check`, [16]), [value("i32", 6)]);

// ./test/core/table_init.wast:241
assert_trap(() => invoke($4, `check`, [17]), `uninitialized element`);

// ./test/core/table_init.wast:242
assert_trap(() => invoke($4, `check`, [18]), `uninitialized element`);

// ./test/core/table_init.wast:243
assert_trap(() => invoke($4, `check`, [19]), `uninitialized element`);

// ./test/core/table_init.wast:244
assert_trap(() => invoke($4, `check`, [20]), `uninitialized element`);

// ./test/core/table_init.wast:245
assert_trap(() => invoke($4, `check`, [21]), `uninitialized element`);

// ./test/core/table_init.wast:246
assert_trap(() => invoke($4, `check`, [22]), `uninitialized element`);

// ./test/core/table_init.wast:247
assert_trap(() => invoke($4, `check`, [23]), `uninitialized element`);

// ./test/core/table_init.wast:248
assert_trap(() => invoke($4, `check`, [24]), `uninitialized element`);

// ./test/core/table_init.wast:249
assert_trap(() => invoke($4, `check`, [25]), `uninitialized element`);

// ./test/core/table_init.wast:250
assert_trap(() => invoke($4, `check`, [26]), `uninitialized element`);

// ./test/core/table_init.wast:251
assert_trap(() => invoke($4, `check`, [27]), `uninitialized element`);

// ./test/core/table_init.wast:252
assert_trap(() => invoke($4, `check`, [28]), `uninitialized element`);

// ./test/core/table_init.wast:253
assert_trap(() => invoke($4, `check`, [29]), `uninitialized element`);

// ./test/core/table_init.wast:255
let $5 = instantiate(`(module
  (type (func (result i32)))  ;; type #0
  (import "a" "ef0" (func (result i32)))    ;; index 0
  (import "a" "ef1" (func (result i32)))
  (import "a" "ef2" (func (result i32)))
  (import "a" "ef3" (func (result i32)))
  (import "a" "ef4" (func (result i32)))    ;; index 4
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t1) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t1) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 5))  ;; index 5
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))  ;; index 9
  (func (export "test")
    (table.init $$t1 3 (i32.const 15) (i32.const 1) (i32.const 3)))
  (func (export "check") (param i32) (result i32)
    (call_indirect $$t1 (type 0) (local.get 0)))
)`);

// ./test/core/table_init.wast:281
invoke($5, `test`, []);

// ./test/core/table_init.wast:282
assert_trap(() => invoke($5, `check`, [0]), `uninitialized element`);

// ./test/core/table_init.wast:283
assert_trap(() => invoke($5, `check`, [1]), `uninitialized element`);

// ./test/core/table_init.wast:284
assert_return(() => invoke($5, `check`, [2]), [value("i32", 3)]);

// ./test/core/table_init.wast:285
assert_return(() => invoke($5, `check`, [3]), [value("i32", 1)]);

// ./test/core/table_init.wast:286
assert_return(() => invoke($5, `check`, [4]), [value("i32", 4)]);

// ./test/core/table_init.wast:287
assert_return(() => invoke($5, `check`, [5]), [value("i32", 1)]);

// ./test/core/table_init.wast:288
assert_trap(() => invoke($5, `check`, [6]), `uninitialized element`);

// ./test/core/table_init.wast:289
assert_trap(() => invoke($5, `check`, [7]), `uninitialized element`);

// ./test/core/table_init.wast:290
assert_trap(() => invoke($5, `check`, [8]), `uninitialized element`);

// ./test/core/table_init.wast:291
assert_trap(() => invoke($5, `check`, [9]), `uninitialized element`);

// ./test/core/table_init.wast:292
assert_trap(() => invoke($5, `check`, [10]), `uninitialized element`);

// ./test/core/table_init.wast:293
assert_trap(() => invoke($5, `check`, [11]), `uninitialized element`);

// ./test/core/table_init.wast:294
assert_return(() => invoke($5, `check`, [12]), [value("i32", 7)]);

// ./test/core/table_init.wast:295
assert_return(() => invoke($5, `check`, [13]), [value("i32", 5)]);

// ./test/core/table_init.wast:296
assert_return(() => invoke($5, `check`, [14]), [value("i32", 2)]);

// ./test/core/table_init.wast:297
assert_return(() => invoke($5, `check`, [15]), [value("i32", 9)]);

// ./test/core/table_init.wast:298
assert_return(() => invoke($5, `check`, [16]), [value("i32", 2)]);

// ./test/core/table_init.wast:299
assert_return(() => invoke($5, `check`, [17]), [value("i32", 7)]);

// ./test/core/table_init.wast:300
assert_trap(() => invoke($5, `check`, [18]), `uninitialized element`);

// ./test/core/table_init.wast:301
assert_trap(() => invoke($5, `check`, [19]), `uninitialized element`);

// ./test/core/table_init.wast:302
assert_trap(() => invoke($5, `check`, [20]), `uninitialized element`);

// ./test/core/table_init.wast:303
assert_trap(() => invoke($5, `check`, [21]), `uninitialized element`);

// ./test/core/table_init.wast:304
assert_trap(() => invoke($5, `check`, [22]), `uninitialized element`);

// ./test/core/table_init.wast:305
assert_trap(() => invoke($5, `check`, [23]), `uninitialized element`);

// ./test/core/table_init.wast:306
assert_trap(() => invoke($5, `check`, [24]), `uninitialized element`);

// ./test/core/table_init.wast:307
assert_trap(() => invoke($5, `check`, [25]), `uninitialized element`);

// ./test/core/table_init.wast:308
assert_trap(() => invoke($5, `check`, [26]), `uninitialized element`);

// ./test/core/table_init.wast:309
assert_trap(() => invoke($5, `check`, [27]), `uninitialized element`);

// ./test/core/table_init.wast:310
assert_trap(() => invoke($5, `check`, [28]), `uninitialized element`);

// ./test/core/table_init.wast:311
assert_trap(() => invoke($5, `check`, [29]), `uninitialized element`);

// ./test/core/table_init.wast:313
let $6 = instantiate(`(module
  (type (func (result i32)))  ;; type #0
  (import "a" "ef0" (func (result i32)))    ;; index 0
  (import "a" "ef1" (func (result i32)))
  (import "a" "ef2" (func (result i32)))
  (import "a" "ef3" (func (result i32)))
  (import "a" "ef4" (func (result i32)))    ;; index 4
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t1) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t1) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 5))  ;; index 5
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))  ;; index 9
  (func (export "test")
    (table.init $$t1 1 (i32.const 7) (i32.const 0) (i32.const 4))
         (elem.drop 1)
         (table.init $$t1 3 (i32.const 15) (i32.const 1) (i32.const 3))
         (elem.drop 3)
         (table.copy $$t1 1 (i32.const 20) (i32.const 15) (i32.const 5))
         (table.copy $$t1 1 (i32.const 21) (i32.const 29) (i32.const 1))
         (table.copy $$t1 1 (i32.const 24) (i32.const 10) (i32.const 1))
         (table.copy $$t1 1 (i32.const 13) (i32.const 11) (i32.const 4))
         (table.copy $$t1 1 (i32.const 19) (i32.const 20) (i32.const 5)))
  (func (export "check") (param i32) (result i32)
    (call_indirect $$t1 (type 0) (local.get 0)))
)`);

// ./test/core/table_init.wast:347
invoke($6, `test`, []);

// ./test/core/table_init.wast:348
assert_trap(() => invoke($6, `check`, [0]), `uninitialized element`);

// ./test/core/table_init.wast:349
assert_trap(() => invoke($6, `check`, [1]), `uninitialized element`);

// ./test/core/table_init.wast:350
assert_return(() => invoke($6, `check`, [2]), [value("i32", 3)]);

// ./test/core/table_init.wast:351
assert_return(() => invoke($6, `check`, [3]), [value("i32", 1)]);

// ./test/core/table_init.wast:352
assert_return(() => invoke($6, `check`, [4]), [value("i32", 4)]);

// ./test/core/table_init.wast:353
assert_return(() => invoke($6, `check`, [5]), [value("i32", 1)]);

// ./test/core/table_init.wast:354
assert_trap(() => invoke($6, `check`, [6]), `uninitialized element`);

// ./test/core/table_init.wast:355
assert_return(() => invoke($6, `check`, [7]), [value("i32", 2)]);

// ./test/core/table_init.wast:356
assert_return(() => invoke($6, `check`, [8]), [value("i32", 7)]);

// ./test/core/table_init.wast:357
assert_return(() => invoke($6, `check`, [9]), [value("i32", 1)]);

// ./test/core/table_init.wast:358
assert_return(() => invoke($6, `check`, [10]), [value("i32", 8)]);

// ./test/core/table_init.wast:359
assert_trap(() => invoke($6, `check`, [11]), `uninitialized element`);

// ./test/core/table_init.wast:360
assert_return(() => invoke($6, `check`, [12]), [value("i32", 7)]);

// ./test/core/table_init.wast:361
assert_trap(() => invoke($6, `check`, [13]), `uninitialized element`);

// ./test/core/table_init.wast:362
assert_return(() => invoke($6, `check`, [14]), [value("i32", 7)]);

// ./test/core/table_init.wast:363
assert_return(() => invoke($6, `check`, [15]), [value("i32", 5)]);

// ./test/core/table_init.wast:364
assert_return(() => invoke($6, `check`, [16]), [value("i32", 2)]);

// ./test/core/table_init.wast:365
assert_return(() => invoke($6, `check`, [17]), [value("i32", 7)]);

// ./test/core/table_init.wast:366
assert_trap(() => invoke($6, `check`, [18]), `uninitialized element`);

// ./test/core/table_init.wast:367
assert_return(() => invoke($6, `check`, [19]), [value("i32", 9)]);

// ./test/core/table_init.wast:368
assert_trap(() => invoke($6, `check`, [20]), `uninitialized element`);

// ./test/core/table_init.wast:369
assert_return(() => invoke($6, `check`, [21]), [value("i32", 7)]);

// ./test/core/table_init.wast:370
assert_trap(() => invoke($6, `check`, [22]), `uninitialized element`);

// ./test/core/table_init.wast:371
assert_return(() => invoke($6, `check`, [23]), [value("i32", 8)]);

// ./test/core/table_init.wast:372
assert_return(() => invoke($6, `check`, [24]), [value("i32", 8)]);

// ./test/core/table_init.wast:373
assert_trap(() => invoke($6, `check`, [25]), `uninitialized element`);

// ./test/core/table_init.wast:374
assert_trap(() => invoke($6, `check`, [26]), `uninitialized element`);

// ./test/core/table_init.wast:375
assert_trap(() => invoke($6, `check`, [27]), `uninitialized element`);

// ./test/core/table_init.wast:376
assert_trap(() => invoke($6, `check`, [28]), `uninitialized element`);

// ./test/core/table_init.wast:377
assert_trap(() => invoke($6, `check`, [29]), `uninitialized element`);

// ./test/core/table_init.wast:378
assert_invalid(() =>
  instantiate(`(module
    (func (export "test")
      (elem.drop 0)))`), `unknown elem segment 0`);

// ./test/core/table_init.wast:384
assert_invalid(
  () =>
    instantiate(`(module
    (func (export "test")
      (table.init 0 (i32.const 12) (i32.const 1) (i32.const 1))))`),
  `unknown table 0`,
);

// ./test/core/table_init.wast:390
assert_invalid(() =>
  instantiate(`(module
    (elem funcref (ref.func 0))
    (func (result i32) (i32.const 0))
    (func (export "test")
      (elem.drop 4)))`), `unknown elem segment 4`);

// ./test/core/table_init.wast:398
assert_invalid(
  () =>
    instantiate(`(module
    (elem funcref (ref.func 0))
    (func (result i32) (i32.const 0))
    (func (export "test")
      (table.init 4 (i32.const 12) (i32.const 1) (i32.const 1))))`),
  `unknown table 0`,
);

// ./test/core/table_init.wast:407
let $7 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 28 28 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (elem.drop 2)
    ))`);

// ./test/core/table_init.wast:429
invoke($7, `test`, []);

// ./test/core/table_init.wast:431
let $8 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 28 28 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.init 2 (i32.const 12) (i32.const 1) (i32.const 1))
    ))`);

// ./test/core/table_init.wast:453
assert_trap(() => invoke($8, `test`, []), `out of bounds table access`);

// ./test/core/table_init.wast:455
let $9 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 28 28 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.init 1 (i32.const 12) (i32.const 1) (i32.const 1))
    (table.init 1 (i32.const 21) (i32.const 1) (i32.const 1))))`);

// ./test/core/table_init.wast:477
invoke($9, `test`, []);

// ./test/core/table_init.wast:479
let $10 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 28 28 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (elem.drop 1)
    (elem.drop 1)))`);

// ./test/core/table_init.wast:501
invoke($10, `test`, []);

// ./test/core/table_init.wast:503
let $11 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 28 28 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (elem.drop 1)
    (table.init 1 (i32.const 12) (i32.const 1) (i32.const 1))))`);

// ./test/core/table_init.wast:525
assert_trap(() => invoke($11, `test`, []), `out of bounds table access`);

// ./test/core/table_init.wast:527
let $12 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 28 28 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.init 1 (i32.const 12) (i32.const 0) (i32.const 5))
    ))`);

// ./test/core/table_init.wast:549
assert_trap(() => invoke($12, `test`, []), `out of bounds table access`);

// ./test/core/table_init.wast:551
let $13 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 28 28 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.init 1 (i32.const 12) (i32.const 2) (i32.const 3))
    ))`);

// ./test/core/table_init.wast:573
assert_trap(() => invoke($13, `test`, []), `out of bounds table access`);

// ./test/core/table_init.wast:575
let $14 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 28 28 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.init $$t0 1 (i32.const 28) (i32.const 1) (i32.const 3))
    ))`);

// ./test/core/table_init.wast:597
assert_trap(() => invoke($14, `test`, []), `out of bounds table access`);

// ./test/core/table_init.wast:599
let $15 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 28 28 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.init $$t0 1 (i32.const 12) (i32.const 4) (i32.const 0))
    ))`);

// ./test/core/table_init.wast:621
invoke($15, `test`, []);

// ./test/core/table_init.wast:623
let $16 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 28 28 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.init $$t0 1 (i32.const 12) (i32.const 5) (i32.const 0))
    ))`);

// ./test/core/table_init.wast:645
assert_trap(() => invoke($16, `test`, []), `out of bounds table access`);

// ./test/core/table_init.wast:647
let $17 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 28 28 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.init $$t0 1 (i32.const 30) (i32.const 2) (i32.const 0))
    ))`);

// ./test/core/table_init.wast:669
invoke($17, `test`, []);

// ./test/core/table_init.wast:671
let $18 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 28 28 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.init $$t0 1 (i32.const 31) (i32.const 2) (i32.const 0))
    ))`);

// ./test/core/table_init.wast:693
assert_trap(() => invoke($18, `test`, []), `out of bounds table access`);

// ./test/core/table_init.wast:695
let $19 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 28 28 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.init $$t0 1 (i32.const 30) (i32.const 4) (i32.const 0))
    ))`);

// ./test/core/table_init.wast:717
invoke($19, `test`, []);

// ./test/core/table_init.wast:719
let $20 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 28 28 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.init $$t0 1 (i32.const 31) (i32.const 5) (i32.const 0))
    ))`);

// ./test/core/table_init.wast:741
assert_trap(() => invoke($20, `test`, []), `out of bounds table access`);

// ./test/core/table_init.wast:743
let $21 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 28 28 funcref)
  (elem (table $$t1) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t1) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.init $$t1 1 (i32.const 26) (i32.const 1) (i32.const 3))
    ))`);

// ./test/core/table_init.wast:765
assert_trap(() => invoke($21, `test`, []), `out of bounds table access`);

// ./test/core/table_init.wast:767
let $22 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 28 28 funcref)
  (elem (table $$t1) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t1) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.init $$t1 1 (i32.const 12) (i32.const 4) (i32.const 0))
    ))`);

// ./test/core/table_init.wast:789
invoke($22, `test`, []);

// ./test/core/table_init.wast:791
let $23 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 28 28 funcref)
  (elem (table $$t1) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t1) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.init $$t1 1 (i32.const 12) (i32.const 5) (i32.const 0))
    ))`);

// ./test/core/table_init.wast:813
assert_trap(() => invoke($23, `test`, []), `out of bounds table access`);

// ./test/core/table_init.wast:815
let $24 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 28 28 funcref)
  (elem (table $$t1) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t1) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.init $$t1 1 (i32.const 28) (i32.const 2) (i32.const 0))
    ))`);

// ./test/core/table_init.wast:837
invoke($24, `test`, []);

// ./test/core/table_init.wast:839
let $25 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 28 28 funcref)
  (elem (table $$t1) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t1) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.init $$t1 1 (i32.const 29) (i32.const 2) (i32.const 0))
    ))`);

// ./test/core/table_init.wast:861
assert_trap(() => invoke($25, `test`, []), `out of bounds table access`);

// ./test/core/table_init.wast:863
let $26 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 28 28 funcref)
  (elem (table $$t1) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t1) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.init $$t1 1 (i32.const 28) (i32.const 4) (i32.const 0))
    ))`);

// ./test/core/table_init.wast:885
invoke($26, `test`, []);

// ./test/core/table_init.wast:887
let $27 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 28 28 funcref)
  (elem (table $$t1) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t1) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.init $$t1 1 (i32.const 29) (i32.const 5) (i32.const 0))
    ))`);

// ./test/core/table_init.wast:909
assert_trap(() => invoke($27, `test`, []), `out of bounds table access`);

// ./test/core/table_init.wast:911
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i32.const 1) (i32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:920
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i32.const 1) (i32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:929
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i32.const 1) (i32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:938
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i32.const 1) (f32.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:947
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i32.const 1) (f32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:956
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i32.const 1) (f32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:965
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i32.const 1) (f32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:974
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i32.const 1) (i64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:983
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i32.const 1) (i64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:992
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i32.const 1) (i64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1001
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i32.const 1) (i64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1010
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i32.const 1) (f64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1019
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i32.const 1) (f64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1028
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i32.const 1) (f64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1037
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i32.const 1) (f64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1046
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f32.const 1) (i32.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1055
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f32.const 1) (i32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1064
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f32.const 1) (i32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1073
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f32.const 1) (i32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1082
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f32.const 1) (f32.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1091
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f32.const 1) (f32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1100
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f32.const 1) (f32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1109
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f32.const 1) (f32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1118
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f32.const 1) (i64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1127
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f32.const 1) (i64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1136
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f32.const 1) (i64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1145
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f32.const 1) (i64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1154
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f32.const 1) (f64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1163
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f32.const 1) (f64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1172
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f32.const 1) (f64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1181
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f32.const 1) (f64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1190
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i64.const 1) (i32.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1199
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i64.const 1) (i32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1208
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i64.const 1) (i32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1217
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i64.const 1) (i32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1226
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i64.const 1) (f32.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1235
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i64.const 1) (f32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1244
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i64.const 1) (f32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1253
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i64.const 1) (f32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1262
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i64.const 1) (i64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1271
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i64.const 1) (i64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1280
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i64.const 1) (i64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1289
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i64.const 1) (i64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1298
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i64.const 1) (f64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1307
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i64.const 1) (f64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1316
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i64.const 1) (f64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1325
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (i64.const 1) (f64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1334
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f64.const 1) (i32.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1343
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f64.const 1) (i32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1352
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f64.const 1) (i32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1361
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f64.const 1) (i32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1370
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f64.const 1) (f32.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1379
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f64.const 1) (f32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1388
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f64.const 1) (f32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1397
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f64.const 1) (f32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1406
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f64.const 1) (i64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1415
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f64.const 1) (i64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1424
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f64.const 1) (i64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1433
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f64.const 1) (i64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1442
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f64.const 1) (f64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1451
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f64.const 1) (f64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1460
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f64.const 1) (f64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1469
assert_invalid(
  () =>
    instantiate(`(module
    (table 10 funcref)
    (elem funcref (ref.func $$f0) (ref.func $$f0) (ref.func $$f0))
    (func $$f0)
    (func (export "test")
      (table.init 0 (f64.const 1) (f64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/table_init.wast:1478
let $28 = instantiate(`(module
  (type (func (result i32)))
  (table 32 64 funcref)
  (elem funcref
    (ref.func $$f0) (ref.func $$f1) (ref.func $$f2) (ref.func $$f3)
    (ref.func $$f4) (ref.func $$f5) (ref.func $$f6) (ref.func $$f7)
    (ref.func $$f8) (ref.func $$f9) (ref.func $$f10) (ref.func $$f11)
    (ref.func $$f12) (ref.func $$f13) (ref.func $$f14) (ref.func $$f15))
  (func $$f0 (export "f0") (result i32) (i32.const 0))
  (func $$f1 (export "f1") (result i32) (i32.const 1))
  (func $$f2 (export "f2") (result i32) (i32.const 2))
  (func $$f3 (export "f3") (result i32) (i32.const 3))
  (func $$f4 (export "f4") (result i32) (i32.const 4))
  (func $$f5 (export "f5") (result i32) (i32.const 5))
  (func $$f6 (export "f6") (result i32) (i32.const 6))
  (func $$f7 (export "f7") (result i32) (i32.const 7))
  (func $$f8 (export "f8") (result i32) (i32.const 8))
  (func $$f9 (export "f9") (result i32) (i32.const 9))
  (func $$f10 (export "f10") (result i32) (i32.const 10))
  (func $$f11 (export "f11") (result i32) (i32.const 11))
  (func $$f12 (export "f12") (result i32) (i32.const 12))
  (func $$f13 (export "f13") (result i32) (i32.const 13))
  (func $$f14 (export "f14") (result i32) (i32.const 14))
  (func $$f15 (export "f15") (result i32) (i32.const 15))
  (func (export "test") (param $$n i32) (result i32)
    (call_indirect (type 0) (local.get $$n)))
  (func (export "run") (param $$offs i32) (param $$len i32)
    (table.init 0 (local.get $$offs) (i32.const 0) (local.get $$len))))`);

// ./test/core/table_init.wast:1506
assert_trap(() => invoke($28, `run`, [24, 16]), `out of bounds table access`);

// ./test/core/table_init.wast:1507
assert_trap(() => invoke($28, `test`, [0]), `uninitialized element`);

// ./test/core/table_init.wast:1508
assert_trap(() => invoke($28, `test`, [1]), `uninitialized element`);

// ./test/core/table_init.wast:1509
assert_trap(() => invoke($28, `test`, [2]), `uninitialized element`);

// ./test/core/table_init.wast:1510
assert_trap(() => invoke($28, `test`, [3]), `uninitialized element`);

// ./test/core/table_init.wast:1511
assert_trap(() => invoke($28, `test`, [4]), `uninitialized element`);

// ./test/core/table_init.wast:1512
assert_trap(() => invoke($28, `test`, [5]), `uninitialized element`);

// ./test/core/table_init.wast:1513
assert_trap(() => invoke($28, `test`, [6]), `uninitialized element`);

// ./test/core/table_init.wast:1514
assert_trap(() => invoke($28, `test`, [7]), `uninitialized element`);

// ./test/core/table_init.wast:1515
assert_trap(() => invoke($28, `test`, [8]), `uninitialized element`);

// ./test/core/table_init.wast:1516
assert_trap(() => invoke($28, `test`, [9]), `uninitialized element`);

// ./test/core/table_init.wast:1517
assert_trap(() => invoke($28, `test`, [10]), `uninitialized element`);

// ./test/core/table_init.wast:1518
assert_trap(() => invoke($28, `test`, [11]), `uninitialized element`);

// ./test/core/table_init.wast:1519
assert_trap(() => invoke($28, `test`, [12]), `uninitialized element`);

// ./test/core/table_init.wast:1520
assert_trap(() => invoke($28, `test`, [13]), `uninitialized element`);

// ./test/core/table_init.wast:1521
assert_trap(() => invoke($28, `test`, [14]), `uninitialized element`);

// ./test/core/table_init.wast:1522
assert_trap(() => invoke($28, `test`, [15]), `uninitialized element`);

// ./test/core/table_init.wast:1523
assert_trap(() => invoke($28, `test`, [16]), `uninitialized element`);

// ./test/core/table_init.wast:1524
assert_trap(() => invoke($28, `test`, [17]), `uninitialized element`);

// ./test/core/table_init.wast:1525
assert_trap(() => invoke($28, `test`, [18]), `uninitialized element`);

// ./test/core/table_init.wast:1526
assert_trap(() => invoke($28, `test`, [19]), `uninitialized element`);

// ./test/core/table_init.wast:1527
assert_trap(() => invoke($28, `test`, [20]), `uninitialized element`);

// ./test/core/table_init.wast:1528
assert_trap(() => invoke($28, `test`, [21]), `uninitialized element`);

// ./test/core/table_init.wast:1529
assert_trap(() => invoke($28, `test`, [22]), `uninitialized element`);

// ./test/core/table_init.wast:1530
assert_trap(() => invoke($28, `test`, [23]), `uninitialized element`);

// ./test/core/table_init.wast:1531
assert_trap(() => invoke($28, `test`, [24]), `uninitialized element`);

// ./test/core/table_init.wast:1532
assert_trap(() => invoke($28, `test`, [25]), `uninitialized element`);

// ./test/core/table_init.wast:1533
assert_trap(() => invoke($28, `test`, [26]), `uninitialized element`);

// ./test/core/table_init.wast:1534
assert_trap(() => invoke($28, `test`, [27]), `uninitialized element`);

// ./test/core/table_init.wast:1535
assert_trap(() => invoke($28, `test`, [28]), `uninitialized element`);

// ./test/core/table_init.wast:1536
assert_trap(() => invoke($28, `test`, [29]), `uninitialized element`);

// ./test/core/table_init.wast:1537
assert_trap(() => invoke($28, `test`, [30]), `uninitialized element`);

// ./test/core/table_init.wast:1538
assert_trap(() => invoke($28, `test`, [31]), `uninitialized element`);

// ./test/core/table_init.wast:1540
let $29 = instantiate(`(module
  (type (func (result i32)))
  (table 32 64 funcref)
  (elem funcref
    (ref.func $$f0) (ref.func $$f1) (ref.func $$f2) (ref.func $$f3)
    (ref.func $$f4) (ref.func $$f5) (ref.func $$f6) (ref.func $$f7)
    (ref.func $$f8) (ref.func $$f9) (ref.func $$f10) (ref.func $$f11)
    (ref.func $$f12) (ref.func $$f13) (ref.func $$f14) (ref.func $$f15))
  (func $$f0 (export "f0") (result i32) (i32.const 0))
  (func $$f1 (export "f1") (result i32) (i32.const 1))
  (func $$f2 (export "f2") (result i32) (i32.const 2))
  (func $$f3 (export "f3") (result i32) (i32.const 3))
  (func $$f4 (export "f4") (result i32) (i32.const 4))
  (func $$f5 (export "f5") (result i32) (i32.const 5))
  (func $$f6 (export "f6") (result i32) (i32.const 6))
  (func $$f7 (export "f7") (result i32) (i32.const 7))
  (func $$f8 (export "f8") (result i32) (i32.const 8))
  (func $$f9 (export "f9") (result i32) (i32.const 9))
  (func $$f10 (export "f10") (result i32) (i32.const 10))
  (func $$f11 (export "f11") (result i32) (i32.const 11))
  (func $$f12 (export "f12") (result i32) (i32.const 12))
  (func $$f13 (export "f13") (result i32) (i32.const 13))
  (func $$f14 (export "f14") (result i32) (i32.const 14))
  (func $$f15 (export "f15") (result i32) (i32.const 15))
  (func (export "test") (param $$n i32) (result i32)
    (call_indirect (type 0) (local.get $$n)))
  (func (export "run") (param $$offs i32) (param $$len i32)
    (table.init 0 (local.get $$offs) (i32.const 0) (local.get $$len))))`);

// ./test/core/table_init.wast:1568
assert_trap(() => invoke($29, `run`, [25, 16]), `out of bounds table access`);

// ./test/core/table_init.wast:1569
assert_trap(() => invoke($29, `test`, [0]), `uninitialized element`);

// ./test/core/table_init.wast:1570
assert_trap(() => invoke($29, `test`, [1]), `uninitialized element`);

// ./test/core/table_init.wast:1571
assert_trap(() => invoke($29, `test`, [2]), `uninitialized element`);

// ./test/core/table_init.wast:1572
assert_trap(() => invoke($29, `test`, [3]), `uninitialized element`);

// ./test/core/table_init.wast:1573
assert_trap(() => invoke($29, `test`, [4]), `uninitialized element`);

// ./test/core/table_init.wast:1574
assert_trap(() => invoke($29, `test`, [5]), `uninitialized element`);

// ./test/core/table_init.wast:1575
assert_trap(() => invoke($29, `test`, [6]), `uninitialized element`);

// ./test/core/table_init.wast:1576
assert_trap(() => invoke($29, `test`, [7]), `uninitialized element`);

// ./test/core/table_init.wast:1577
assert_trap(() => invoke($29, `test`, [8]), `uninitialized element`);

// ./test/core/table_init.wast:1578
assert_trap(() => invoke($29, `test`, [9]), `uninitialized element`);

// ./test/core/table_init.wast:1579
assert_trap(() => invoke($29, `test`, [10]), `uninitialized element`);

// ./test/core/table_init.wast:1580
assert_trap(() => invoke($29, `test`, [11]), `uninitialized element`);

// ./test/core/table_init.wast:1581
assert_trap(() => invoke($29, `test`, [12]), `uninitialized element`);

// ./test/core/table_init.wast:1582
assert_trap(() => invoke($29, `test`, [13]), `uninitialized element`);

// ./test/core/table_init.wast:1583
assert_trap(() => invoke($29, `test`, [14]), `uninitialized element`);

// ./test/core/table_init.wast:1584
assert_trap(() => invoke($29, `test`, [15]), `uninitialized element`);

// ./test/core/table_init.wast:1585
assert_trap(() => invoke($29, `test`, [16]), `uninitialized element`);

// ./test/core/table_init.wast:1586
assert_trap(() => invoke($29, `test`, [17]), `uninitialized element`);

// ./test/core/table_init.wast:1587
assert_trap(() => invoke($29, `test`, [18]), `uninitialized element`);

// ./test/core/table_init.wast:1588
assert_trap(() => invoke($29, `test`, [19]), `uninitialized element`);

// ./test/core/table_init.wast:1589
assert_trap(() => invoke($29, `test`, [20]), `uninitialized element`);

// ./test/core/table_init.wast:1590
assert_trap(() => invoke($29, `test`, [21]), `uninitialized element`);

// ./test/core/table_init.wast:1591
assert_trap(() => invoke($29, `test`, [22]), `uninitialized element`);

// ./test/core/table_init.wast:1592
assert_trap(() => invoke($29, `test`, [23]), `uninitialized element`);

// ./test/core/table_init.wast:1593
assert_trap(() => invoke($29, `test`, [24]), `uninitialized element`);

// ./test/core/table_init.wast:1594
assert_trap(() => invoke($29, `test`, [25]), `uninitialized element`);

// ./test/core/table_init.wast:1595
assert_trap(() => invoke($29, `test`, [26]), `uninitialized element`);

// ./test/core/table_init.wast:1596
assert_trap(() => invoke($29, `test`, [27]), `uninitialized element`);

// ./test/core/table_init.wast:1597
assert_trap(() => invoke($29, `test`, [28]), `uninitialized element`);

// ./test/core/table_init.wast:1598
assert_trap(() => invoke($29, `test`, [29]), `uninitialized element`);

// ./test/core/table_init.wast:1599
assert_trap(() => invoke($29, `test`, [30]), `uninitialized element`);

// ./test/core/table_init.wast:1600
assert_trap(() => invoke($29, `test`, [31]), `uninitialized element`);

// ./test/core/table_init.wast:1602
let $30 = instantiate(`(module
  (type (func (result i32)))
  (table 160 320 funcref)
  (elem funcref
    (ref.func $$f0) (ref.func $$f1) (ref.func $$f2) (ref.func $$f3)
    (ref.func $$f4) (ref.func $$f5) (ref.func $$f6) (ref.func $$f7)
    (ref.func $$f8) (ref.func $$f9) (ref.func $$f10) (ref.func $$f11)
    (ref.func $$f12) (ref.func $$f13) (ref.func $$f14) (ref.func $$f15))
  (func $$f0 (export "f0") (result i32) (i32.const 0))
  (func $$f1 (export "f1") (result i32) (i32.const 1))
  (func $$f2 (export "f2") (result i32) (i32.const 2))
  (func $$f3 (export "f3") (result i32) (i32.const 3))
  (func $$f4 (export "f4") (result i32) (i32.const 4))
  (func $$f5 (export "f5") (result i32) (i32.const 5))
  (func $$f6 (export "f6") (result i32) (i32.const 6))
  (func $$f7 (export "f7") (result i32) (i32.const 7))
  (func $$f8 (export "f8") (result i32) (i32.const 8))
  (func $$f9 (export "f9") (result i32) (i32.const 9))
  (func $$f10 (export "f10") (result i32) (i32.const 10))
  (func $$f11 (export "f11") (result i32) (i32.const 11))
  (func $$f12 (export "f12") (result i32) (i32.const 12))
  (func $$f13 (export "f13") (result i32) (i32.const 13))
  (func $$f14 (export "f14") (result i32) (i32.const 14))
  (func $$f15 (export "f15") (result i32) (i32.const 15))
  (func (export "test") (param $$n i32) (result i32)
    (call_indirect (type 0) (local.get $$n)))
  (func (export "run") (param $$offs i32) (param $$len i32)
    (table.init 0 (local.get $$offs) (i32.const 0) (local.get $$len))))`);

// ./test/core/table_init.wast:1630
assert_trap(() => invoke($30, `run`, [96, 32]), `out of bounds table access`);

// ./test/core/table_init.wast:1631
assert_trap(() => invoke($30, `test`, [0]), `uninitialized element`);

// ./test/core/table_init.wast:1632
assert_trap(() => invoke($30, `test`, [1]), `uninitialized element`);

// ./test/core/table_init.wast:1633
assert_trap(() => invoke($30, `test`, [2]), `uninitialized element`);

// ./test/core/table_init.wast:1634
assert_trap(() => invoke($30, `test`, [3]), `uninitialized element`);

// ./test/core/table_init.wast:1635
assert_trap(() => invoke($30, `test`, [4]), `uninitialized element`);

// ./test/core/table_init.wast:1636
assert_trap(() => invoke($30, `test`, [5]), `uninitialized element`);

// ./test/core/table_init.wast:1637
assert_trap(() => invoke($30, `test`, [6]), `uninitialized element`);

// ./test/core/table_init.wast:1638
assert_trap(() => invoke($30, `test`, [7]), `uninitialized element`);

// ./test/core/table_init.wast:1639
assert_trap(() => invoke($30, `test`, [8]), `uninitialized element`);

// ./test/core/table_init.wast:1640
assert_trap(() => invoke($30, `test`, [9]), `uninitialized element`);

// ./test/core/table_init.wast:1641
assert_trap(() => invoke($30, `test`, [10]), `uninitialized element`);

// ./test/core/table_init.wast:1642
assert_trap(() => invoke($30, `test`, [11]), `uninitialized element`);

// ./test/core/table_init.wast:1643
assert_trap(() => invoke($30, `test`, [12]), `uninitialized element`);

// ./test/core/table_init.wast:1644
assert_trap(() => invoke($30, `test`, [13]), `uninitialized element`);

// ./test/core/table_init.wast:1645
assert_trap(() => invoke($30, `test`, [14]), `uninitialized element`);

// ./test/core/table_init.wast:1646
assert_trap(() => invoke($30, `test`, [15]), `uninitialized element`);

// ./test/core/table_init.wast:1647
assert_trap(() => invoke($30, `test`, [16]), `uninitialized element`);

// ./test/core/table_init.wast:1648
assert_trap(() => invoke($30, `test`, [17]), `uninitialized element`);

// ./test/core/table_init.wast:1649
assert_trap(() => invoke($30, `test`, [18]), `uninitialized element`);

// ./test/core/table_init.wast:1650
assert_trap(() => invoke($30, `test`, [19]), `uninitialized element`);

// ./test/core/table_init.wast:1651
assert_trap(() => invoke($30, `test`, [20]), `uninitialized element`);

// ./test/core/table_init.wast:1652
assert_trap(() => invoke($30, `test`, [21]), `uninitialized element`);

// ./test/core/table_init.wast:1653
assert_trap(() => invoke($30, `test`, [22]), `uninitialized element`);

// ./test/core/table_init.wast:1654
assert_trap(() => invoke($30, `test`, [23]), `uninitialized element`);

// ./test/core/table_init.wast:1655
assert_trap(() => invoke($30, `test`, [24]), `uninitialized element`);

// ./test/core/table_init.wast:1656
assert_trap(() => invoke($30, `test`, [25]), `uninitialized element`);

// ./test/core/table_init.wast:1657
assert_trap(() => invoke($30, `test`, [26]), `uninitialized element`);

// ./test/core/table_init.wast:1658
assert_trap(() => invoke($30, `test`, [27]), `uninitialized element`);

// ./test/core/table_init.wast:1659
assert_trap(() => invoke($30, `test`, [28]), `uninitialized element`);

// ./test/core/table_init.wast:1660
assert_trap(() => invoke($30, `test`, [29]), `uninitialized element`);

// ./test/core/table_init.wast:1661
assert_trap(() => invoke($30, `test`, [30]), `uninitialized element`);

// ./test/core/table_init.wast:1662
assert_trap(() => invoke($30, `test`, [31]), `uninitialized element`);

// ./test/core/table_init.wast:1663
assert_trap(() => invoke($30, `test`, [32]), `uninitialized element`);

// ./test/core/table_init.wast:1664
assert_trap(() => invoke($30, `test`, [33]), `uninitialized element`);

// ./test/core/table_init.wast:1665
assert_trap(() => invoke($30, `test`, [34]), `uninitialized element`);

// ./test/core/table_init.wast:1666
assert_trap(() => invoke($30, `test`, [35]), `uninitialized element`);

// ./test/core/table_init.wast:1667
assert_trap(() => invoke($30, `test`, [36]), `uninitialized element`);

// ./test/core/table_init.wast:1668
assert_trap(() => invoke($30, `test`, [37]), `uninitialized element`);

// ./test/core/table_init.wast:1669
assert_trap(() => invoke($30, `test`, [38]), `uninitialized element`);

// ./test/core/table_init.wast:1670
assert_trap(() => invoke($30, `test`, [39]), `uninitialized element`);

// ./test/core/table_init.wast:1671
assert_trap(() => invoke($30, `test`, [40]), `uninitialized element`);

// ./test/core/table_init.wast:1672
assert_trap(() => invoke($30, `test`, [41]), `uninitialized element`);

// ./test/core/table_init.wast:1673
assert_trap(() => invoke($30, `test`, [42]), `uninitialized element`);

// ./test/core/table_init.wast:1674
assert_trap(() => invoke($30, `test`, [43]), `uninitialized element`);

// ./test/core/table_init.wast:1675
assert_trap(() => invoke($30, `test`, [44]), `uninitialized element`);

// ./test/core/table_init.wast:1676
assert_trap(() => invoke($30, `test`, [45]), `uninitialized element`);

// ./test/core/table_init.wast:1677
assert_trap(() => invoke($30, `test`, [46]), `uninitialized element`);

// ./test/core/table_init.wast:1678
assert_trap(() => invoke($30, `test`, [47]), `uninitialized element`);

// ./test/core/table_init.wast:1679
assert_trap(() => invoke($30, `test`, [48]), `uninitialized element`);

// ./test/core/table_init.wast:1680
assert_trap(() => invoke($30, `test`, [49]), `uninitialized element`);

// ./test/core/table_init.wast:1681
assert_trap(() => invoke($30, `test`, [50]), `uninitialized element`);

// ./test/core/table_init.wast:1682
assert_trap(() => invoke($30, `test`, [51]), `uninitialized element`);

// ./test/core/table_init.wast:1683
assert_trap(() => invoke($30, `test`, [52]), `uninitialized element`);

// ./test/core/table_init.wast:1684
assert_trap(() => invoke($30, `test`, [53]), `uninitialized element`);

// ./test/core/table_init.wast:1685
assert_trap(() => invoke($30, `test`, [54]), `uninitialized element`);

// ./test/core/table_init.wast:1686
assert_trap(() => invoke($30, `test`, [55]), `uninitialized element`);

// ./test/core/table_init.wast:1687
assert_trap(() => invoke($30, `test`, [56]), `uninitialized element`);

// ./test/core/table_init.wast:1688
assert_trap(() => invoke($30, `test`, [57]), `uninitialized element`);

// ./test/core/table_init.wast:1689
assert_trap(() => invoke($30, `test`, [58]), `uninitialized element`);

// ./test/core/table_init.wast:1690
assert_trap(() => invoke($30, `test`, [59]), `uninitialized element`);

// ./test/core/table_init.wast:1691
assert_trap(() => invoke($30, `test`, [60]), `uninitialized element`);

// ./test/core/table_init.wast:1692
assert_trap(() => invoke($30, `test`, [61]), `uninitialized element`);

// ./test/core/table_init.wast:1693
assert_trap(() => invoke($30, `test`, [62]), `uninitialized element`);

// ./test/core/table_init.wast:1694
assert_trap(() => invoke($30, `test`, [63]), `uninitialized element`);

// ./test/core/table_init.wast:1695
assert_trap(() => invoke($30, `test`, [64]), `uninitialized element`);

// ./test/core/table_init.wast:1696
assert_trap(() => invoke($30, `test`, [65]), `uninitialized element`);

// ./test/core/table_init.wast:1697
assert_trap(() => invoke($30, `test`, [66]), `uninitialized element`);

// ./test/core/table_init.wast:1698
assert_trap(() => invoke($30, `test`, [67]), `uninitialized element`);

// ./test/core/table_init.wast:1699
assert_trap(() => invoke($30, `test`, [68]), `uninitialized element`);

// ./test/core/table_init.wast:1700
assert_trap(() => invoke($30, `test`, [69]), `uninitialized element`);

// ./test/core/table_init.wast:1701
assert_trap(() => invoke($30, `test`, [70]), `uninitialized element`);

// ./test/core/table_init.wast:1702
assert_trap(() => invoke($30, `test`, [71]), `uninitialized element`);

// ./test/core/table_init.wast:1703
assert_trap(() => invoke($30, `test`, [72]), `uninitialized element`);

// ./test/core/table_init.wast:1704
assert_trap(() => invoke($30, `test`, [73]), `uninitialized element`);

// ./test/core/table_init.wast:1705
assert_trap(() => invoke($30, `test`, [74]), `uninitialized element`);

// ./test/core/table_init.wast:1706
assert_trap(() => invoke($30, `test`, [75]), `uninitialized element`);

// ./test/core/table_init.wast:1707
assert_trap(() => invoke($30, `test`, [76]), `uninitialized element`);

// ./test/core/table_init.wast:1708
assert_trap(() => invoke($30, `test`, [77]), `uninitialized element`);

// ./test/core/table_init.wast:1709
assert_trap(() => invoke($30, `test`, [78]), `uninitialized element`);

// ./test/core/table_init.wast:1710
assert_trap(() => invoke($30, `test`, [79]), `uninitialized element`);

// ./test/core/table_init.wast:1711
assert_trap(() => invoke($30, `test`, [80]), `uninitialized element`);

// ./test/core/table_init.wast:1712
assert_trap(() => invoke($30, `test`, [81]), `uninitialized element`);

// ./test/core/table_init.wast:1713
assert_trap(() => invoke($30, `test`, [82]), `uninitialized element`);

// ./test/core/table_init.wast:1714
assert_trap(() => invoke($30, `test`, [83]), `uninitialized element`);

// ./test/core/table_init.wast:1715
assert_trap(() => invoke($30, `test`, [84]), `uninitialized element`);

// ./test/core/table_init.wast:1716
assert_trap(() => invoke($30, `test`, [85]), `uninitialized element`);

// ./test/core/table_init.wast:1717
assert_trap(() => invoke($30, `test`, [86]), `uninitialized element`);

// ./test/core/table_init.wast:1718
assert_trap(() => invoke($30, `test`, [87]), `uninitialized element`);

// ./test/core/table_init.wast:1719
assert_trap(() => invoke($30, `test`, [88]), `uninitialized element`);

// ./test/core/table_init.wast:1720
assert_trap(() => invoke($30, `test`, [89]), `uninitialized element`);

// ./test/core/table_init.wast:1721
assert_trap(() => invoke($30, `test`, [90]), `uninitialized element`);

// ./test/core/table_init.wast:1722
assert_trap(() => invoke($30, `test`, [91]), `uninitialized element`);

// ./test/core/table_init.wast:1723
assert_trap(() => invoke($30, `test`, [92]), `uninitialized element`);

// ./test/core/table_init.wast:1724
assert_trap(() => invoke($30, `test`, [93]), `uninitialized element`);

// ./test/core/table_init.wast:1725
assert_trap(() => invoke($30, `test`, [94]), `uninitialized element`);

// ./test/core/table_init.wast:1726
assert_trap(() => invoke($30, `test`, [95]), `uninitialized element`);

// ./test/core/table_init.wast:1727
assert_trap(() => invoke($30, `test`, [96]), `uninitialized element`);

// ./test/core/table_init.wast:1728
assert_trap(() => invoke($30, `test`, [97]), `uninitialized element`);

// ./test/core/table_init.wast:1729
assert_trap(() => invoke($30, `test`, [98]), `uninitialized element`);

// ./test/core/table_init.wast:1730
assert_trap(() => invoke($30, `test`, [99]), `uninitialized element`);

// ./test/core/table_init.wast:1731
assert_trap(() => invoke($30, `test`, [100]), `uninitialized element`);

// ./test/core/table_init.wast:1732
assert_trap(() => invoke($30, `test`, [101]), `uninitialized element`);

// ./test/core/table_init.wast:1733
assert_trap(() => invoke($30, `test`, [102]), `uninitialized element`);

// ./test/core/table_init.wast:1734
assert_trap(() => invoke($30, `test`, [103]), `uninitialized element`);

// ./test/core/table_init.wast:1735
assert_trap(() => invoke($30, `test`, [104]), `uninitialized element`);

// ./test/core/table_init.wast:1736
assert_trap(() => invoke($30, `test`, [105]), `uninitialized element`);

// ./test/core/table_init.wast:1737
assert_trap(() => invoke($30, `test`, [106]), `uninitialized element`);

// ./test/core/table_init.wast:1738
assert_trap(() => invoke($30, `test`, [107]), `uninitialized element`);

// ./test/core/table_init.wast:1739
assert_trap(() => invoke($30, `test`, [108]), `uninitialized element`);

// ./test/core/table_init.wast:1740
assert_trap(() => invoke($30, `test`, [109]), `uninitialized element`);

// ./test/core/table_init.wast:1741
assert_trap(() => invoke($30, `test`, [110]), `uninitialized element`);

// ./test/core/table_init.wast:1742
assert_trap(() => invoke($30, `test`, [111]), `uninitialized element`);

// ./test/core/table_init.wast:1743
assert_trap(() => invoke($30, `test`, [112]), `uninitialized element`);

// ./test/core/table_init.wast:1744
assert_trap(() => invoke($30, `test`, [113]), `uninitialized element`);

// ./test/core/table_init.wast:1745
assert_trap(() => invoke($30, `test`, [114]), `uninitialized element`);

// ./test/core/table_init.wast:1746
assert_trap(() => invoke($30, `test`, [115]), `uninitialized element`);

// ./test/core/table_init.wast:1747
assert_trap(() => invoke($30, `test`, [116]), `uninitialized element`);

// ./test/core/table_init.wast:1748
assert_trap(() => invoke($30, `test`, [117]), `uninitialized element`);

// ./test/core/table_init.wast:1749
assert_trap(() => invoke($30, `test`, [118]), `uninitialized element`);

// ./test/core/table_init.wast:1750
assert_trap(() => invoke($30, `test`, [119]), `uninitialized element`);

// ./test/core/table_init.wast:1751
assert_trap(() => invoke($30, `test`, [120]), `uninitialized element`);

// ./test/core/table_init.wast:1752
assert_trap(() => invoke($30, `test`, [121]), `uninitialized element`);

// ./test/core/table_init.wast:1753
assert_trap(() => invoke($30, `test`, [122]), `uninitialized element`);

// ./test/core/table_init.wast:1754
assert_trap(() => invoke($30, `test`, [123]), `uninitialized element`);

// ./test/core/table_init.wast:1755
assert_trap(() => invoke($30, `test`, [124]), `uninitialized element`);

// ./test/core/table_init.wast:1756
assert_trap(() => invoke($30, `test`, [125]), `uninitialized element`);

// ./test/core/table_init.wast:1757
assert_trap(() => invoke($30, `test`, [126]), `uninitialized element`);

// ./test/core/table_init.wast:1758
assert_trap(() => invoke($30, `test`, [127]), `uninitialized element`);

// ./test/core/table_init.wast:1759
assert_trap(() => invoke($30, `test`, [128]), `uninitialized element`);

// ./test/core/table_init.wast:1760
assert_trap(() => invoke($30, `test`, [129]), `uninitialized element`);

// ./test/core/table_init.wast:1761
assert_trap(() => invoke($30, `test`, [130]), `uninitialized element`);

// ./test/core/table_init.wast:1762
assert_trap(() => invoke($30, `test`, [131]), `uninitialized element`);

// ./test/core/table_init.wast:1763
assert_trap(() => invoke($30, `test`, [132]), `uninitialized element`);

// ./test/core/table_init.wast:1764
assert_trap(() => invoke($30, `test`, [133]), `uninitialized element`);

// ./test/core/table_init.wast:1765
assert_trap(() => invoke($30, `test`, [134]), `uninitialized element`);

// ./test/core/table_init.wast:1766
assert_trap(() => invoke($30, `test`, [135]), `uninitialized element`);

// ./test/core/table_init.wast:1767
assert_trap(() => invoke($30, `test`, [136]), `uninitialized element`);

// ./test/core/table_init.wast:1768
assert_trap(() => invoke($30, `test`, [137]), `uninitialized element`);

// ./test/core/table_init.wast:1769
assert_trap(() => invoke($30, `test`, [138]), `uninitialized element`);

// ./test/core/table_init.wast:1770
assert_trap(() => invoke($30, `test`, [139]), `uninitialized element`);

// ./test/core/table_init.wast:1771
assert_trap(() => invoke($30, `test`, [140]), `uninitialized element`);

// ./test/core/table_init.wast:1772
assert_trap(() => invoke($30, `test`, [141]), `uninitialized element`);

// ./test/core/table_init.wast:1773
assert_trap(() => invoke($30, `test`, [142]), `uninitialized element`);

// ./test/core/table_init.wast:1774
assert_trap(() => invoke($30, `test`, [143]), `uninitialized element`);

// ./test/core/table_init.wast:1775
assert_trap(() => invoke($30, `test`, [144]), `uninitialized element`);

// ./test/core/table_init.wast:1776
assert_trap(() => invoke($30, `test`, [145]), `uninitialized element`);

// ./test/core/table_init.wast:1777
assert_trap(() => invoke($30, `test`, [146]), `uninitialized element`);

// ./test/core/table_init.wast:1778
assert_trap(() => invoke($30, `test`, [147]), `uninitialized element`);

// ./test/core/table_init.wast:1779
assert_trap(() => invoke($30, `test`, [148]), `uninitialized element`);

// ./test/core/table_init.wast:1780
assert_trap(() => invoke($30, `test`, [149]), `uninitialized element`);

// ./test/core/table_init.wast:1781
assert_trap(() => invoke($30, `test`, [150]), `uninitialized element`);

// ./test/core/table_init.wast:1782
assert_trap(() => invoke($30, `test`, [151]), `uninitialized element`);

// ./test/core/table_init.wast:1783
assert_trap(() => invoke($30, `test`, [152]), `uninitialized element`);

// ./test/core/table_init.wast:1784
assert_trap(() => invoke($30, `test`, [153]), `uninitialized element`);

// ./test/core/table_init.wast:1785
assert_trap(() => invoke($30, `test`, [154]), `uninitialized element`);

// ./test/core/table_init.wast:1786
assert_trap(() => invoke($30, `test`, [155]), `uninitialized element`);

// ./test/core/table_init.wast:1787
assert_trap(() => invoke($30, `test`, [156]), `uninitialized element`);

// ./test/core/table_init.wast:1788
assert_trap(() => invoke($30, `test`, [157]), `uninitialized element`);

// ./test/core/table_init.wast:1789
assert_trap(() => invoke($30, `test`, [158]), `uninitialized element`);

// ./test/core/table_init.wast:1790
assert_trap(() => invoke($30, `test`, [159]), `uninitialized element`);

// ./test/core/table_init.wast:1792
let $31 = instantiate(`(module
  (type (func (result i32)))
  (table 160 320 funcref)
  (elem funcref
    (ref.func $$f0) (ref.func $$f1) (ref.func $$f2) (ref.func $$f3)
    (ref.func $$f4) (ref.func $$f5) (ref.func $$f6) (ref.func $$f7)
    (ref.func $$f8) (ref.func $$f9) (ref.func $$f10) (ref.func $$f11)
    (ref.func $$f12) (ref.func $$f13) (ref.func $$f14) (ref.func $$f15))
  (func $$f0 (export "f0") (result i32) (i32.const 0))
  (func $$f1 (export "f1") (result i32) (i32.const 1))
  (func $$f2 (export "f2") (result i32) (i32.const 2))
  (func $$f3 (export "f3") (result i32) (i32.const 3))
  (func $$f4 (export "f4") (result i32) (i32.const 4))
  (func $$f5 (export "f5") (result i32) (i32.const 5))
  (func $$f6 (export "f6") (result i32) (i32.const 6))
  (func $$f7 (export "f7") (result i32) (i32.const 7))
  (func $$f8 (export "f8") (result i32) (i32.const 8))
  (func $$f9 (export "f9") (result i32) (i32.const 9))
  (func $$f10 (export "f10") (result i32) (i32.const 10))
  (func $$f11 (export "f11") (result i32) (i32.const 11))
  (func $$f12 (export "f12") (result i32) (i32.const 12))
  (func $$f13 (export "f13") (result i32) (i32.const 13))
  (func $$f14 (export "f14") (result i32) (i32.const 14))
  (func $$f15 (export "f15") (result i32) (i32.const 15))
  (func (export "test") (param $$n i32) (result i32)
    (call_indirect (type 0) (local.get $$n)))
  (func (export "run") (param $$offs i32) (param $$len i32)
    (table.init 0 (local.get $$offs) (i32.const 0) (local.get $$len))))`);

// ./test/core/table_init.wast:1820
assert_trap(() => invoke($31, `run`, [97, 31]), `out of bounds table access`);

// ./test/core/table_init.wast:1821
assert_trap(() => invoke($31, `test`, [0]), `uninitialized element`);

// ./test/core/table_init.wast:1822
assert_trap(() => invoke($31, `test`, [1]), `uninitialized element`);

// ./test/core/table_init.wast:1823
assert_trap(() => invoke($31, `test`, [2]), `uninitialized element`);

// ./test/core/table_init.wast:1824
assert_trap(() => invoke($31, `test`, [3]), `uninitialized element`);

// ./test/core/table_init.wast:1825
assert_trap(() => invoke($31, `test`, [4]), `uninitialized element`);

// ./test/core/table_init.wast:1826
assert_trap(() => invoke($31, `test`, [5]), `uninitialized element`);

// ./test/core/table_init.wast:1827
assert_trap(() => invoke($31, `test`, [6]), `uninitialized element`);

// ./test/core/table_init.wast:1828
assert_trap(() => invoke($31, `test`, [7]), `uninitialized element`);

// ./test/core/table_init.wast:1829
assert_trap(() => invoke($31, `test`, [8]), `uninitialized element`);

// ./test/core/table_init.wast:1830
assert_trap(() => invoke($31, `test`, [9]), `uninitialized element`);

// ./test/core/table_init.wast:1831
assert_trap(() => invoke($31, `test`, [10]), `uninitialized element`);

// ./test/core/table_init.wast:1832
assert_trap(() => invoke($31, `test`, [11]), `uninitialized element`);

// ./test/core/table_init.wast:1833
assert_trap(() => invoke($31, `test`, [12]), `uninitialized element`);

// ./test/core/table_init.wast:1834
assert_trap(() => invoke($31, `test`, [13]), `uninitialized element`);

// ./test/core/table_init.wast:1835
assert_trap(() => invoke($31, `test`, [14]), `uninitialized element`);

// ./test/core/table_init.wast:1836
assert_trap(() => invoke($31, `test`, [15]), `uninitialized element`);

// ./test/core/table_init.wast:1837
assert_trap(() => invoke($31, `test`, [16]), `uninitialized element`);

// ./test/core/table_init.wast:1838
assert_trap(() => invoke($31, `test`, [17]), `uninitialized element`);

// ./test/core/table_init.wast:1839
assert_trap(() => invoke($31, `test`, [18]), `uninitialized element`);

// ./test/core/table_init.wast:1840
assert_trap(() => invoke($31, `test`, [19]), `uninitialized element`);

// ./test/core/table_init.wast:1841
assert_trap(() => invoke($31, `test`, [20]), `uninitialized element`);

// ./test/core/table_init.wast:1842
assert_trap(() => invoke($31, `test`, [21]), `uninitialized element`);

// ./test/core/table_init.wast:1843
assert_trap(() => invoke($31, `test`, [22]), `uninitialized element`);

// ./test/core/table_init.wast:1844
assert_trap(() => invoke($31, `test`, [23]), `uninitialized element`);

// ./test/core/table_init.wast:1845
assert_trap(() => invoke($31, `test`, [24]), `uninitialized element`);

// ./test/core/table_init.wast:1846
assert_trap(() => invoke($31, `test`, [25]), `uninitialized element`);

// ./test/core/table_init.wast:1847
assert_trap(() => invoke($31, `test`, [26]), `uninitialized element`);

// ./test/core/table_init.wast:1848
assert_trap(() => invoke($31, `test`, [27]), `uninitialized element`);

// ./test/core/table_init.wast:1849
assert_trap(() => invoke($31, `test`, [28]), `uninitialized element`);

// ./test/core/table_init.wast:1850
assert_trap(() => invoke($31, `test`, [29]), `uninitialized element`);

// ./test/core/table_init.wast:1851
assert_trap(() => invoke($31, `test`, [30]), `uninitialized element`);

// ./test/core/table_init.wast:1852
assert_trap(() => invoke($31, `test`, [31]), `uninitialized element`);

// ./test/core/table_init.wast:1853
assert_trap(() => invoke($31, `test`, [32]), `uninitialized element`);

// ./test/core/table_init.wast:1854
assert_trap(() => invoke($31, `test`, [33]), `uninitialized element`);

// ./test/core/table_init.wast:1855
assert_trap(() => invoke($31, `test`, [34]), `uninitialized element`);

// ./test/core/table_init.wast:1856
assert_trap(() => invoke($31, `test`, [35]), `uninitialized element`);

// ./test/core/table_init.wast:1857
assert_trap(() => invoke($31, `test`, [36]), `uninitialized element`);

// ./test/core/table_init.wast:1858
assert_trap(() => invoke($31, `test`, [37]), `uninitialized element`);

// ./test/core/table_init.wast:1859
assert_trap(() => invoke($31, `test`, [38]), `uninitialized element`);

// ./test/core/table_init.wast:1860
assert_trap(() => invoke($31, `test`, [39]), `uninitialized element`);

// ./test/core/table_init.wast:1861
assert_trap(() => invoke($31, `test`, [40]), `uninitialized element`);

// ./test/core/table_init.wast:1862
assert_trap(() => invoke($31, `test`, [41]), `uninitialized element`);

// ./test/core/table_init.wast:1863
assert_trap(() => invoke($31, `test`, [42]), `uninitialized element`);

// ./test/core/table_init.wast:1864
assert_trap(() => invoke($31, `test`, [43]), `uninitialized element`);

// ./test/core/table_init.wast:1865
assert_trap(() => invoke($31, `test`, [44]), `uninitialized element`);

// ./test/core/table_init.wast:1866
assert_trap(() => invoke($31, `test`, [45]), `uninitialized element`);

// ./test/core/table_init.wast:1867
assert_trap(() => invoke($31, `test`, [46]), `uninitialized element`);

// ./test/core/table_init.wast:1868
assert_trap(() => invoke($31, `test`, [47]), `uninitialized element`);

// ./test/core/table_init.wast:1869
assert_trap(() => invoke($31, `test`, [48]), `uninitialized element`);

// ./test/core/table_init.wast:1870
assert_trap(() => invoke($31, `test`, [49]), `uninitialized element`);

// ./test/core/table_init.wast:1871
assert_trap(() => invoke($31, `test`, [50]), `uninitialized element`);

// ./test/core/table_init.wast:1872
assert_trap(() => invoke($31, `test`, [51]), `uninitialized element`);

// ./test/core/table_init.wast:1873
assert_trap(() => invoke($31, `test`, [52]), `uninitialized element`);

// ./test/core/table_init.wast:1874
assert_trap(() => invoke($31, `test`, [53]), `uninitialized element`);

// ./test/core/table_init.wast:1875
assert_trap(() => invoke($31, `test`, [54]), `uninitialized element`);

// ./test/core/table_init.wast:1876
assert_trap(() => invoke($31, `test`, [55]), `uninitialized element`);

// ./test/core/table_init.wast:1877
assert_trap(() => invoke($31, `test`, [56]), `uninitialized element`);

// ./test/core/table_init.wast:1878
assert_trap(() => invoke($31, `test`, [57]), `uninitialized element`);

// ./test/core/table_init.wast:1879
assert_trap(() => invoke($31, `test`, [58]), `uninitialized element`);

// ./test/core/table_init.wast:1880
assert_trap(() => invoke($31, `test`, [59]), `uninitialized element`);

// ./test/core/table_init.wast:1881
assert_trap(() => invoke($31, `test`, [60]), `uninitialized element`);

// ./test/core/table_init.wast:1882
assert_trap(() => invoke($31, `test`, [61]), `uninitialized element`);

// ./test/core/table_init.wast:1883
assert_trap(() => invoke($31, `test`, [62]), `uninitialized element`);

// ./test/core/table_init.wast:1884
assert_trap(() => invoke($31, `test`, [63]), `uninitialized element`);

// ./test/core/table_init.wast:1885
assert_trap(() => invoke($31, `test`, [64]), `uninitialized element`);

// ./test/core/table_init.wast:1886
assert_trap(() => invoke($31, `test`, [65]), `uninitialized element`);

// ./test/core/table_init.wast:1887
assert_trap(() => invoke($31, `test`, [66]), `uninitialized element`);

// ./test/core/table_init.wast:1888
assert_trap(() => invoke($31, `test`, [67]), `uninitialized element`);

// ./test/core/table_init.wast:1889
assert_trap(() => invoke($31, `test`, [68]), `uninitialized element`);

// ./test/core/table_init.wast:1890
assert_trap(() => invoke($31, `test`, [69]), `uninitialized element`);

// ./test/core/table_init.wast:1891
assert_trap(() => invoke($31, `test`, [70]), `uninitialized element`);

// ./test/core/table_init.wast:1892
assert_trap(() => invoke($31, `test`, [71]), `uninitialized element`);

// ./test/core/table_init.wast:1893
assert_trap(() => invoke($31, `test`, [72]), `uninitialized element`);

// ./test/core/table_init.wast:1894
assert_trap(() => invoke($31, `test`, [73]), `uninitialized element`);

// ./test/core/table_init.wast:1895
assert_trap(() => invoke($31, `test`, [74]), `uninitialized element`);

// ./test/core/table_init.wast:1896
assert_trap(() => invoke($31, `test`, [75]), `uninitialized element`);

// ./test/core/table_init.wast:1897
assert_trap(() => invoke($31, `test`, [76]), `uninitialized element`);

// ./test/core/table_init.wast:1898
assert_trap(() => invoke($31, `test`, [77]), `uninitialized element`);

// ./test/core/table_init.wast:1899
assert_trap(() => invoke($31, `test`, [78]), `uninitialized element`);

// ./test/core/table_init.wast:1900
assert_trap(() => invoke($31, `test`, [79]), `uninitialized element`);

// ./test/core/table_init.wast:1901
assert_trap(() => invoke($31, `test`, [80]), `uninitialized element`);

// ./test/core/table_init.wast:1902
assert_trap(() => invoke($31, `test`, [81]), `uninitialized element`);

// ./test/core/table_init.wast:1903
assert_trap(() => invoke($31, `test`, [82]), `uninitialized element`);

// ./test/core/table_init.wast:1904
assert_trap(() => invoke($31, `test`, [83]), `uninitialized element`);

// ./test/core/table_init.wast:1905
assert_trap(() => invoke($31, `test`, [84]), `uninitialized element`);

// ./test/core/table_init.wast:1906
assert_trap(() => invoke($31, `test`, [85]), `uninitialized element`);

// ./test/core/table_init.wast:1907
assert_trap(() => invoke($31, `test`, [86]), `uninitialized element`);

// ./test/core/table_init.wast:1908
assert_trap(() => invoke($31, `test`, [87]), `uninitialized element`);

// ./test/core/table_init.wast:1909
assert_trap(() => invoke($31, `test`, [88]), `uninitialized element`);

// ./test/core/table_init.wast:1910
assert_trap(() => invoke($31, `test`, [89]), `uninitialized element`);

// ./test/core/table_init.wast:1911
assert_trap(() => invoke($31, `test`, [90]), `uninitialized element`);

// ./test/core/table_init.wast:1912
assert_trap(() => invoke($31, `test`, [91]), `uninitialized element`);

// ./test/core/table_init.wast:1913
assert_trap(() => invoke($31, `test`, [92]), `uninitialized element`);

// ./test/core/table_init.wast:1914
assert_trap(() => invoke($31, `test`, [93]), `uninitialized element`);

// ./test/core/table_init.wast:1915
assert_trap(() => invoke($31, `test`, [94]), `uninitialized element`);

// ./test/core/table_init.wast:1916
assert_trap(() => invoke($31, `test`, [95]), `uninitialized element`);

// ./test/core/table_init.wast:1917
assert_trap(() => invoke($31, `test`, [96]), `uninitialized element`);

// ./test/core/table_init.wast:1918
assert_trap(() => invoke($31, `test`, [97]), `uninitialized element`);

// ./test/core/table_init.wast:1919
assert_trap(() => invoke($31, `test`, [98]), `uninitialized element`);

// ./test/core/table_init.wast:1920
assert_trap(() => invoke($31, `test`, [99]), `uninitialized element`);

// ./test/core/table_init.wast:1921
assert_trap(() => invoke($31, `test`, [100]), `uninitialized element`);

// ./test/core/table_init.wast:1922
assert_trap(() => invoke($31, `test`, [101]), `uninitialized element`);

// ./test/core/table_init.wast:1923
assert_trap(() => invoke($31, `test`, [102]), `uninitialized element`);

// ./test/core/table_init.wast:1924
assert_trap(() => invoke($31, `test`, [103]), `uninitialized element`);

// ./test/core/table_init.wast:1925
assert_trap(() => invoke($31, `test`, [104]), `uninitialized element`);

// ./test/core/table_init.wast:1926
assert_trap(() => invoke($31, `test`, [105]), `uninitialized element`);

// ./test/core/table_init.wast:1927
assert_trap(() => invoke($31, `test`, [106]), `uninitialized element`);

// ./test/core/table_init.wast:1928
assert_trap(() => invoke($31, `test`, [107]), `uninitialized element`);

// ./test/core/table_init.wast:1929
assert_trap(() => invoke($31, `test`, [108]), `uninitialized element`);

// ./test/core/table_init.wast:1930
assert_trap(() => invoke($31, `test`, [109]), `uninitialized element`);

// ./test/core/table_init.wast:1931
assert_trap(() => invoke($31, `test`, [110]), `uninitialized element`);

// ./test/core/table_init.wast:1932
assert_trap(() => invoke($31, `test`, [111]), `uninitialized element`);

// ./test/core/table_init.wast:1933
assert_trap(() => invoke($31, `test`, [112]), `uninitialized element`);

// ./test/core/table_init.wast:1934
assert_trap(() => invoke($31, `test`, [113]), `uninitialized element`);

// ./test/core/table_init.wast:1935
assert_trap(() => invoke($31, `test`, [114]), `uninitialized element`);

// ./test/core/table_init.wast:1936
assert_trap(() => invoke($31, `test`, [115]), `uninitialized element`);

// ./test/core/table_init.wast:1937
assert_trap(() => invoke($31, `test`, [116]), `uninitialized element`);

// ./test/core/table_init.wast:1938
assert_trap(() => invoke($31, `test`, [117]), `uninitialized element`);

// ./test/core/table_init.wast:1939
assert_trap(() => invoke($31, `test`, [118]), `uninitialized element`);

// ./test/core/table_init.wast:1940
assert_trap(() => invoke($31, `test`, [119]), `uninitialized element`);

// ./test/core/table_init.wast:1941
assert_trap(() => invoke($31, `test`, [120]), `uninitialized element`);

// ./test/core/table_init.wast:1942
assert_trap(() => invoke($31, `test`, [121]), `uninitialized element`);

// ./test/core/table_init.wast:1943
assert_trap(() => invoke($31, `test`, [122]), `uninitialized element`);

// ./test/core/table_init.wast:1944
assert_trap(() => invoke($31, `test`, [123]), `uninitialized element`);

// ./test/core/table_init.wast:1945
assert_trap(() => invoke($31, `test`, [124]), `uninitialized element`);

// ./test/core/table_init.wast:1946
assert_trap(() => invoke($31, `test`, [125]), `uninitialized element`);

// ./test/core/table_init.wast:1947
assert_trap(() => invoke($31, `test`, [126]), `uninitialized element`);

// ./test/core/table_init.wast:1948
assert_trap(() => invoke($31, `test`, [127]), `uninitialized element`);

// ./test/core/table_init.wast:1949
assert_trap(() => invoke($31, `test`, [128]), `uninitialized element`);

// ./test/core/table_init.wast:1950
assert_trap(() => invoke($31, `test`, [129]), `uninitialized element`);

// ./test/core/table_init.wast:1951
assert_trap(() => invoke($31, `test`, [130]), `uninitialized element`);

// ./test/core/table_init.wast:1952
assert_trap(() => invoke($31, `test`, [131]), `uninitialized element`);

// ./test/core/table_init.wast:1953
assert_trap(() => invoke($31, `test`, [132]), `uninitialized element`);

// ./test/core/table_init.wast:1954
assert_trap(() => invoke($31, `test`, [133]), `uninitialized element`);

// ./test/core/table_init.wast:1955
assert_trap(() => invoke($31, `test`, [134]), `uninitialized element`);

// ./test/core/table_init.wast:1956
assert_trap(() => invoke($31, `test`, [135]), `uninitialized element`);

// ./test/core/table_init.wast:1957
assert_trap(() => invoke($31, `test`, [136]), `uninitialized element`);

// ./test/core/table_init.wast:1958
assert_trap(() => invoke($31, `test`, [137]), `uninitialized element`);

// ./test/core/table_init.wast:1959
assert_trap(() => invoke($31, `test`, [138]), `uninitialized element`);

// ./test/core/table_init.wast:1960
assert_trap(() => invoke($31, `test`, [139]), `uninitialized element`);

// ./test/core/table_init.wast:1961
assert_trap(() => invoke($31, `test`, [140]), `uninitialized element`);

// ./test/core/table_init.wast:1962
assert_trap(() => invoke($31, `test`, [141]), `uninitialized element`);

// ./test/core/table_init.wast:1963
assert_trap(() => invoke($31, `test`, [142]), `uninitialized element`);

// ./test/core/table_init.wast:1964
assert_trap(() => invoke($31, `test`, [143]), `uninitialized element`);

// ./test/core/table_init.wast:1965
assert_trap(() => invoke($31, `test`, [144]), `uninitialized element`);

// ./test/core/table_init.wast:1966
assert_trap(() => invoke($31, `test`, [145]), `uninitialized element`);

// ./test/core/table_init.wast:1967
assert_trap(() => invoke($31, `test`, [146]), `uninitialized element`);

// ./test/core/table_init.wast:1968
assert_trap(() => invoke($31, `test`, [147]), `uninitialized element`);

// ./test/core/table_init.wast:1969
assert_trap(() => invoke($31, `test`, [148]), `uninitialized element`);

// ./test/core/table_init.wast:1970
assert_trap(() => invoke($31, `test`, [149]), `uninitialized element`);

// ./test/core/table_init.wast:1971
assert_trap(() => invoke($31, `test`, [150]), `uninitialized element`);

// ./test/core/table_init.wast:1972
assert_trap(() => invoke($31, `test`, [151]), `uninitialized element`);

// ./test/core/table_init.wast:1973
assert_trap(() => invoke($31, `test`, [152]), `uninitialized element`);

// ./test/core/table_init.wast:1974
assert_trap(() => invoke($31, `test`, [153]), `uninitialized element`);

// ./test/core/table_init.wast:1975
assert_trap(() => invoke($31, `test`, [154]), `uninitialized element`);

// ./test/core/table_init.wast:1976
assert_trap(() => invoke($31, `test`, [155]), `uninitialized element`);

// ./test/core/table_init.wast:1977
assert_trap(() => invoke($31, `test`, [156]), `uninitialized element`);

// ./test/core/table_init.wast:1978
assert_trap(() => invoke($31, `test`, [157]), `uninitialized element`);

// ./test/core/table_init.wast:1979
assert_trap(() => invoke($31, `test`, [158]), `uninitialized element`);

// ./test/core/table_init.wast:1980
assert_trap(() => invoke($31, `test`, [159]), `uninitialized element`);

// ./test/core/table_init.wast:1982
let $32 = instantiate(`(module
  (type (func (result i32)))
  (table 64 64 funcref)
  (elem funcref
    (ref.func $$f0) (ref.func $$f1) (ref.func $$f2) (ref.func $$f3)
    (ref.func $$f4) (ref.func $$f5) (ref.func $$f6) (ref.func $$f7)
    (ref.func $$f8) (ref.func $$f9) (ref.func $$f10) (ref.func $$f11)
    (ref.func $$f12) (ref.func $$f13) (ref.func $$f14) (ref.func $$f15))
  (func $$f0 (export "f0") (result i32) (i32.const 0))
  (func $$f1 (export "f1") (result i32) (i32.const 1))
  (func $$f2 (export "f2") (result i32) (i32.const 2))
  (func $$f3 (export "f3") (result i32) (i32.const 3))
  (func $$f4 (export "f4") (result i32) (i32.const 4))
  (func $$f5 (export "f5") (result i32) (i32.const 5))
  (func $$f6 (export "f6") (result i32) (i32.const 6))
  (func $$f7 (export "f7") (result i32) (i32.const 7))
  (func $$f8 (export "f8") (result i32) (i32.const 8))
  (func $$f9 (export "f9") (result i32) (i32.const 9))
  (func $$f10 (export "f10") (result i32) (i32.const 10))
  (func $$f11 (export "f11") (result i32) (i32.const 11))
  (func $$f12 (export "f12") (result i32) (i32.const 12))
  (func $$f13 (export "f13") (result i32) (i32.const 13))
  (func $$f14 (export "f14") (result i32) (i32.const 14))
  (func $$f15 (export "f15") (result i32) (i32.const 15))
  (func (export "test") (param $$n i32) (result i32)
    (call_indirect (type 0) (local.get $$n)))
  (func (export "run") (param $$offs i32) (param $$len i32)
    (table.init 0 (local.get $$offs) (i32.const 0) (local.get $$len))))`);

// ./test/core/table_init.wast:2010
assert_trap(() => invoke($32, `run`, [48, -16]), `out of bounds table access`);

// ./test/core/table_init.wast:2011
assert_trap(() => invoke($32, `test`, [0]), `uninitialized element`);

// ./test/core/table_init.wast:2012
assert_trap(() => invoke($32, `test`, [1]), `uninitialized element`);

// ./test/core/table_init.wast:2013
assert_trap(() => invoke($32, `test`, [2]), `uninitialized element`);

// ./test/core/table_init.wast:2014
assert_trap(() => invoke($32, `test`, [3]), `uninitialized element`);

// ./test/core/table_init.wast:2015
assert_trap(() => invoke($32, `test`, [4]), `uninitialized element`);

// ./test/core/table_init.wast:2016
assert_trap(() => invoke($32, `test`, [5]), `uninitialized element`);

// ./test/core/table_init.wast:2017
assert_trap(() => invoke($32, `test`, [6]), `uninitialized element`);

// ./test/core/table_init.wast:2018
assert_trap(() => invoke($32, `test`, [7]), `uninitialized element`);

// ./test/core/table_init.wast:2019
assert_trap(() => invoke($32, `test`, [8]), `uninitialized element`);

// ./test/core/table_init.wast:2020
assert_trap(() => invoke($32, `test`, [9]), `uninitialized element`);

// ./test/core/table_init.wast:2021
assert_trap(() => invoke($32, `test`, [10]), `uninitialized element`);

// ./test/core/table_init.wast:2022
assert_trap(() => invoke($32, `test`, [11]), `uninitialized element`);

// ./test/core/table_init.wast:2023
assert_trap(() => invoke($32, `test`, [12]), `uninitialized element`);

// ./test/core/table_init.wast:2024
assert_trap(() => invoke($32, `test`, [13]), `uninitialized element`);

// ./test/core/table_init.wast:2025
assert_trap(() => invoke($32, `test`, [14]), `uninitialized element`);

// ./test/core/table_init.wast:2026
assert_trap(() => invoke($32, `test`, [15]), `uninitialized element`);

// ./test/core/table_init.wast:2027
assert_trap(() => invoke($32, `test`, [16]), `uninitialized element`);

// ./test/core/table_init.wast:2028
assert_trap(() => invoke($32, `test`, [17]), `uninitialized element`);

// ./test/core/table_init.wast:2029
assert_trap(() => invoke($32, `test`, [18]), `uninitialized element`);

// ./test/core/table_init.wast:2030
assert_trap(() => invoke($32, `test`, [19]), `uninitialized element`);

// ./test/core/table_init.wast:2031
assert_trap(() => invoke($32, `test`, [20]), `uninitialized element`);

// ./test/core/table_init.wast:2032
assert_trap(() => invoke($32, `test`, [21]), `uninitialized element`);

// ./test/core/table_init.wast:2033
assert_trap(() => invoke($32, `test`, [22]), `uninitialized element`);

// ./test/core/table_init.wast:2034
assert_trap(() => invoke($32, `test`, [23]), `uninitialized element`);

// ./test/core/table_init.wast:2035
assert_trap(() => invoke($32, `test`, [24]), `uninitialized element`);

// ./test/core/table_init.wast:2036
assert_trap(() => invoke($32, `test`, [25]), `uninitialized element`);

// ./test/core/table_init.wast:2037
assert_trap(() => invoke($32, `test`, [26]), `uninitialized element`);

// ./test/core/table_init.wast:2038
assert_trap(() => invoke($32, `test`, [27]), `uninitialized element`);

// ./test/core/table_init.wast:2039
assert_trap(() => invoke($32, `test`, [28]), `uninitialized element`);

// ./test/core/table_init.wast:2040
assert_trap(() => invoke($32, `test`, [29]), `uninitialized element`);

// ./test/core/table_init.wast:2041
assert_trap(() => invoke($32, `test`, [30]), `uninitialized element`);

// ./test/core/table_init.wast:2042
assert_trap(() => invoke($32, `test`, [31]), `uninitialized element`);

// ./test/core/table_init.wast:2043
assert_trap(() => invoke($32, `test`, [32]), `uninitialized element`);

// ./test/core/table_init.wast:2044
assert_trap(() => invoke($32, `test`, [33]), `uninitialized element`);

// ./test/core/table_init.wast:2045
assert_trap(() => invoke($32, `test`, [34]), `uninitialized element`);

// ./test/core/table_init.wast:2046
assert_trap(() => invoke($32, `test`, [35]), `uninitialized element`);

// ./test/core/table_init.wast:2047
assert_trap(() => invoke($32, `test`, [36]), `uninitialized element`);

// ./test/core/table_init.wast:2048
assert_trap(() => invoke($32, `test`, [37]), `uninitialized element`);

// ./test/core/table_init.wast:2049
assert_trap(() => invoke($32, `test`, [38]), `uninitialized element`);

// ./test/core/table_init.wast:2050
assert_trap(() => invoke($32, `test`, [39]), `uninitialized element`);

// ./test/core/table_init.wast:2051
assert_trap(() => invoke($32, `test`, [40]), `uninitialized element`);

// ./test/core/table_init.wast:2052
assert_trap(() => invoke($32, `test`, [41]), `uninitialized element`);

// ./test/core/table_init.wast:2053
assert_trap(() => invoke($32, `test`, [42]), `uninitialized element`);

// ./test/core/table_init.wast:2054
assert_trap(() => invoke($32, `test`, [43]), `uninitialized element`);

// ./test/core/table_init.wast:2055
assert_trap(() => invoke($32, `test`, [44]), `uninitialized element`);

// ./test/core/table_init.wast:2056
assert_trap(() => invoke($32, `test`, [45]), `uninitialized element`);

// ./test/core/table_init.wast:2057
assert_trap(() => invoke($32, `test`, [46]), `uninitialized element`);

// ./test/core/table_init.wast:2058
assert_trap(() => invoke($32, `test`, [47]), `uninitialized element`);

// ./test/core/table_init.wast:2059
assert_trap(() => invoke($32, `test`, [48]), `uninitialized element`);

// ./test/core/table_init.wast:2060
assert_trap(() => invoke($32, `test`, [49]), `uninitialized element`);

// ./test/core/table_init.wast:2061
assert_trap(() => invoke($32, `test`, [50]), `uninitialized element`);

// ./test/core/table_init.wast:2062
assert_trap(() => invoke($32, `test`, [51]), `uninitialized element`);

// ./test/core/table_init.wast:2063
assert_trap(() => invoke($32, `test`, [52]), `uninitialized element`);

// ./test/core/table_init.wast:2064
assert_trap(() => invoke($32, `test`, [53]), `uninitialized element`);

// ./test/core/table_init.wast:2065
assert_trap(() => invoke($32, `test`, [54]), `uninitialized element`);

// ./test/core/table_init.wast:2066
assert_trap(() => invoke($32, `test`, [55]), `uninitialized element`);

// ./test/core/table_init.wast:2067
assert_trap(() => invoke($32, `test`, [56]), `uninitialized element`);

// ./test/core/table_init.wast:2068
assert_trap(() => invoke($32, `test`, [57]), `uninitialized element`);

// ./test/core/table_init.wast:2069
assert_trap(() => invoke($32, `test`, [58]), `uninitialized element`);

// ./test/core/table_init.wast:2070
assert_trap(() => invoke($32, `test`, [59]), `uninitialized element`);

// ./test/core/table_init.wast:2071
assert_trap(() => invoke($32, `test`, [60]), `uninitialized element`);

// ./test/core/table_init.wast:2072
assert_trap(() => invoke($32, `test`, [61]), `uninitialized element`);

// ./test/core/table_init.wast:2073
assert_trap(() => invoke($32, `test`, [62]), `uninitialized element`);

// ./test/core/table_init.wast:2074
assert_trap(() => invoke($32, `test`, [63]), `uninitialized element`);

// ./test/core/table_init.wast:2076
let $33 = instantiate(`(module
  (type (func (result i32)))
  (table 16 16 funcref)
  (elem funcref
    (ref.func $$f0) (ref.func $$f1) (ref.func $$f2) (ref.func $$f3)
    (ref.func $$f4) (ref.func $$f5) (ref.func $$f6) (ref.func $$f7)
    (ref.func $$f8) (ref.func $$f9) (ref.func $$f10) (ref.func $$f11)
    (ref.func $$f12) (ref.func $$f13) (ref.func $$f14) (ref.func $$f15))
  (func $$f0 (export "f0") (result i32) (i32.const 0))
  (func $$f1 (export "f1") (result i32) (i32.const 1))
  (func $$f2 (export "f2") (result i32) (i32.const 2))
  (func $$f3 (export "f3") (result i32) (i32.const 3))
  (func $$f4 (export "f4") (result i32) (i32.const 4))
  (func $$f5 (export "f5") (result i32) (i32.const 5))
  (func $$f6 (export "f6") (result i32) (i32.const 6))
  (func $$f7 (export "f7") (result i32) (i32.const 7))
  (func $$f8 (export "f8") (result i32) (i32.const 8))
  (func $$f9 (export "f9") (result i32) (i32.const 9))
  (func $$f10 (export "f10") (result i32) (i32.const 10))
  (func $$f11 (export "f11") (result i32) (i32.const 11))
  (func $$f12 (export "f12") (result i32) (i32.const 12))
  (func $$f13 (export "f13") (result i32) (i32.const 13))
  (func $$f14 (export "f14") (result i32) (i32.const 14))
  (func $$f15 (export "f15") (result i32) (i32.const 15))
  (func (export "test") (param $$n i32) (result i32)
    (call_indirect (type 0) (local.get $$n)))
  (func (export "run") (param $$offs i32) (param $$len i32)
    (table.init 0 (local.get $$offs) (i32.const 8) (local.get $$len))))`);

// ./test/core/table_init.wast:2104
assert_trap(() => invoke($33, `run`, [0, -4]), `out of bounds table access`);

// ./test/core/table_init.wast:2105
assert_trap(() => invoke($33, `test`, [0]), `uninitialized element`);

// ./test/core/table_init.wast:2106
assert_trap(() => invoke($33, `test`, [1]), `uninitialized element`);

// ./test/core/table_init.wast:2107
assert_trap(() => invoke($33, `test`, [2]), `uninitialized element`);

// ./test/core/table_init.wast:2108
assert_trap(() => invoke($33, `test`, [3]), `uninitialized element`);

// ./test/core/table_init.wast:2109
assert_trap(() => invoke($33, `test`, [4]), `uninitialized element`);

// ./test/core/table_init.wast:2110
assert_trap(() => invoke($33, `test`, [5]), `uninitialized element`);

// ./test/core/table_init.wast:2111
assert_trap(() => invoke($33, `test`, [6]), `uninitialized element`);

// ./test/core/table_init.wast:2112
assert_trap(() => invoke($33, `test`, [7]), `uninitialized element`);

// ./test/core/table_init.wast:2113
assert_trap(() => invoke($33, `test`, [8]), `uninitialized element`);

// ./test/core/table_init.wast:2114
assert_trap(() => invoke($33, `test`, [9]), `uninitialized element`);

// ./test/core/table_init.wast:2115
assert_trap(() => invoke($33, `test`, [10]), `uninitialized element`);

// ./test/core/table_init.wast:2116
assert_trap(() => invoke($33, `test`, [11]), `uninitialized element`);

// ./test/core/table_init.wast:2117
assert_trap(() => invoke($33, `test`, [12]), `uninitialized element`);

// ./test/core/table_init.wast:2118
assert_trap(() => invoke($33, `test`, [13]), `uninitialized element`);

// ./test/core/table_init.wast:2119
assert_trap(() => invoke($33, `test`, [14]), `uninitialized element`);

// ./test/core/table_init.wast:2120
assert_trap(() => invoke($33, `test`, [15]), `uninitialized element`);

// ./test/core/table_init.wast:2122
let $34 = instantiate(`(module
  (table 1 funcref)
  ;; 65 elem segments. 64 is the smallest positive number that is encoded
  ;; differently as a signed LEB.
  (elem funcref) (elem funcref) (elem funcref) (elem funcref)
  (elem funcref) (elem funcref) (elem funcref) (elem funcref)
  (elem funcref) (elem funcref) (elem funcref) (elem funcref)
  (elem funcref) (elem funcref) (elem funcref) (elem funcref)
  (elem funcref) (elem funcref) (elem funcref) (elem funcref)
  (elem funcref) (elem funcref) (elem funcref) (elem funcref)
  (elem funcref) (elem funcref) (elem funcref) (elem funcref)
  (elem funcref) (elem funcref) (elem funcref) (elem funcref)
  (elem funcref) (elem funcref) (elem funcref) (elem funcref)
  (elem funcref) (elem funcref) (elem funcref) (elem funcref)
  (elem funcref) (elem funcref) (elem funcref) (elem funcref)
  (elem funcref) (elem funcref) (elem funcref) (elem funcref)
  (elem funcref) (elem funcref) (elem funcref) (elem funcref)
  (elem funcref) (elem funcref) (elem funcref) (elem funcref)
  (elem funcref) (elem funcref) (elem funcref) (elem funcref)
  (elem funcref) (elem funcref) (elem funcref) (elem funcref)
  (elem funcref)
  (func (table.init 64 (i32.const 0) (i32.const 0) (i32.const 0))))`);
