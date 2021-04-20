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

// ./test/core/table_init.wast:5
let $0 = instantiate(`(module
  (func (export "ef0") (result i32) (i32.const 0))
  (func (export "ef1") (result i32) (i32.const 1))
  (func (export "ef2") (result i32) (i32.const 2))
  (func (export "ef3") (result i32) (i32.const 3))
  (func (export "ef4") (result i32) (i32.const 4))
)`);

// ./test/core/table_init.wast:12
register($0, `a`);

// ./test/core/table_init.wast:14
let $1 = instantiate(`(module
  (type (func (result i32)))  ;; type #0
  (import "a" "ef0" (func (result i32)))    ;; index 0
  (import "a" "ef1" (func (result i32)))
  (import "a" "ef2" (func (result i32)))
  (import "a" "ef3" (func (result i32)))
  (import "a" "ef4" (func (result i32)))    ;; index 4
  (table 30 30 funcref)
  (elem (i32.const 2) 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (i32.const 12) 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 5))  ;; index 5
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))  ;; index 9
  (func (export "test")
    (table.init 1 (i32.const 7) (i32.const 0) (i32.const 4)))
  (func (export "check") (param i32) (result i32)
    (call_indirect (type 0) (local.get 0)))
)`);

// ./test/core/table_init.wast:39
invoke($1, `test`, []);

// ./test/core/table_init.wast:40
assert_trap(() => invoke($1, `check`, [0]), `uninitialized element`);

// ./test/core/table_init.wast:41
assert_trap(() => invoke($1, `check`, [1]), `uninitialized element`);

// ./test/core/table_init.wast:42
assert_return(() => invoke($1, `check`, [2]), [value("i32", 3)]);

// ./test/core/table_init.wast:43
assert_return(() => invoke($1, `check`, [3]), [value("i32", 1)]);

// ./test/core/table_init.wast:44
assert_return(() => invoke($1, `check`, [4]), [value("i32", 4)]);

// ./test/core/table_init.wast:45
assert_return(() => invoke($1, `check`, [5]), [value("i32", 1)]);

// ./test/core/table_init.wast:46
assert_trap(() => invoke($1, `check`, [6]), `uninitialized element`);

// ./test/core/table_init.wast:47
assert_return(() => invoke($1, `check`, [7]), [value("i32", 2)]);

// ./test/core/table_init.wast:48
assert_return(() => invoke($1, `check`, [8]), [value("i32", 7)]);

// ./test/core/table_init.wast:49
assert_return(() => invoke($1, `check`, [9]), [value("i32", 1)]);

// ./test/core/table_init.wast:50
assert_return(() => invoke($1, `check`, [10]), [value("i32", 8)]);

// ./test/core/table_init.wast:51
assert_trap(() => invoke($1, `check`, [11]), `uninitialized element`);

// ./test/core/table_init.wast:52
assert_return(() => invoke($1, `check`, [12]), [value("i32", 7)]);

// ./test/core/table_init.wast:53
assert_return(() => invoke($1, `check`, [13]), [value("i32", 5)]);

// ./test/core/table_init.wast:54
assert_return(() => invoke($1, `check`, [14]), [value("i32", 2)]);

// ./test/core/table_init.wast:55
assert_return(() => invoke($1, `check`, [15]), [value("i32", 3)]);

// ./test/core/table_init.wast:56
assert_return(() => invoke($1, `check`, [16]), [value("i32", 6)]);

// ./test/core/table_init.wast:57
assert_trap(() => invoke($1, `check`, [17]), `uninitialized element`);

// ./test/core/table_init.wast:58
assert_trap(() => invoke($1, `check`, [18]), `uninitialized element`);

// ./test/core/table_init.wast:59
assert_trap(() => invoke($1, `check`, [19]), `uninitialized element`);

// ./test/core/table_init.wast:60
assert_trap(() => invoke($1, `check`, [20]), `uninitialized element`);

// ./test/core/table_init.wast:61
assert_trap(() => invoke($1, `check`, [21]), `uninitialized element`);

// ./test/core/table_init.wast:62
assert_trap(() => invoke($1, `check`, [22]), `uninitialized element`);

// ./test/core/table_init.wast:63
assert_trap(() => invoke($1, `check`, [23]), `uninitialized element`);

// ./test/core/table_init.wast:64
assert_trap(() => invoke($1, `check`, [24]), `uninitialized element`);

// ./test/core/table_init.wast:65
assert_trap(() => invoke($1, `check`, [25]), `uninitialized element`);

// ./test/core/table_init.wast:66
assert_trap(() => invoke($1, `check`, [26]), `uninitialized element`);

// ./test/core/table_init.wast:67
assert_trap(() => invoke($1, `check`, [27]), `uninitialized element`);

// ./test/core/table_init.wast:68
assert_trap(() => invoke($1, `check`, [28]), `uninitialized element`);

// ./test/core/table_init.wast:69
assert_trap(() => invoke($1, `check`, [29]), `uninitialized element`);

// ./test/core/table_init.wast:71
let $2 = instantiate(`(module
  (type (func (result i32)))  ;; type #0
  (import "a" "ef0" (func (result i32)))    ;; index 0
  (import "a" "ef1" (func (result i32)))
  (import "a" "ef2" (func (result i32)))
  (import "a" "ef3" (func (result i32)))
  (import "a" "ef4" (func (result i32)))    ;; index 4
  (table 30 30 funcref)
  (elem (i32.const 2) 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (i32.const 12) 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 5))  ;; index 5
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))  ;; index 9
  (func (export "test")
    (table.init 3 (i32.const 15) (i32.const 1) (i32.const 3)))
  (func (export "check") (param i32) (result i32)
    (call_indirect (type 0) (local.get 0)))
)`);

// ./test/core/table_init.wast:96
invoke($2, `test`, []);

// ./test/core/table_init.wast:97
assert_trap(() => invoke($2, `check`, [0]), `uninitialized element`);

// ./test/core/table_init.wast:98
assert_trap(() => invoke($2, `check`, [1]), `uninitialized element`);

// ./test/core/table_init.wast:99
assert_return(() => invoke($2, `check`, [2]), [value("i32", 3)]);

// ./test/core/table_init.wast:100
assert_return(() => invoke($2, `check`, [3]), [value("i32", 1)]);

// ./test/core/table_init.wast:101
assert_return(() => invoke($2, `check`, [4]), [value("i32", 4)]);

// ./test/core/table_init.wast:102
assert_return(() => invoke($2, `check`, [5]), [value("i32", 1)]);

// ./test/core/table_init.wast:103
assert_trap(() => invoke($2, `check`, [6]), `uninitialized element`);

// ./test/core/table_init.wast:104
assert_trap(() => invoke($2, `check`, [7]), `uninitialized element`);

// ./test/core/table_init.wast:105
assert_trap(() => invoke($2, `check`, [8]), `uninitialized element`);

// ./test/core/table_init.wast:106
assert_trap(() => invoke($2, `check`, [9]), `uninitialized element`);

// ./test/core/table_init.wast:107
assert_trap(() => invoke($2, `check`, [10]), `uninitialized element`);

// ./test/core/table_init.wast:108
assert_trap(() => invoke($2, `check`, [11]), `uninitialized element`);

// ./test/core/table_init.wast:109
assert_return(() => invoke($2, `check`, [12]), [value("i32", 7)]);

// ./test/core/table_init.wast:110
assert_return(() => invoke($2, `check`, [13]), [value("i32", 5)]);

// ./test/core/table_init.wast:111
assert_return(() => invoke($2, `check`, [14]), [value("i32", 2)]);

// ./test/core/table_init.wast:112
assert_return(() => invoke($2, `check`, [15]), [value("i32", 9)]);

// ./test/core/table_init.wast:113
assert_return(() => invoke($2, `check`, [16]), [value("i32", 2)]);

// ./test/core/table_init.wast:114
assert_return(() => invoke($2, `check`, [17]), [value("i32", 7)]);

// ./test/core/table_init.wast:115
assert_trap(() => invoke($2, `check`, [18]), `uninitialized element`);

// ./test/core/table_init.wast:116
assert_trap(() => invoke($2, `check`, [19]), `uninitialized element`);

// ./test/core/table_init.wast:117
assert_trap(() => invoke($2, `check`, [20]), `uninitialized element`);

// ./test/core/table_init.wast:118
assert_trap(() => invoke($2, `check`, [21]), `uninitialized element`);

// ./test/core/table_init.wast:119
assert_trap(() => invoke($2, `check`, [22]), `uninitialized element`);

// ./test/core/table_init.wast:120
assert_trap(() => invoke($2, `check`, [23]), `uninitialized element`);

// ./test/core/table_init.wast:121
assert_trap(() => invoke($2, `check`, [24]), `uninitialized element`);

// ./test/core/table_init.wast:122
assert_trap(() => invoke($2, `check`, [25]), `uninitialized element`);

// ./test/core/table_init.wast:123
assert_trap(() => invoke($2, `check`, [26]), `uninitialized element`);

// ./test/core/table_init.wast:124
assert_trap(() => invoke($2, `check`, [27]), `uninitialized element`);

// ./test/core/table_init.wast:125
assert_trap(() => invoke($2, `check`, [28]), `uninitialized element`);

// ./test/core/table_init.wast:126
assert_trap(() => invoke($2, `check`, [29]), `uninitialized element`);

// ./test/core/table_init.wast:128
let $3 = instantiate(`(module
  (type (func (result i32)))  ;; type #0
  (import "a" "ef0" (func (result i32)))    ;; index 0
  (import "a" "ef1" (func (result i32)))
  (import "a" "ef2" (func (result i32)))
  (import "a" "ef3" (func (result i32)))
  (import "a" "ef4" (func (result i32)))    ;; index 4
  (table 30 30 funcref)
  (elem (i32.const 2) 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (i32.const 12) 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 5))  ;; index 5
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))  ;; index 9
  (func (export "test")
    (table.init 1 (i32.const 7) (i32.const 0) (i32.const 4))
    (elem.drop 1)
    (table.init 3 (i32.const 15) (i32.const 1) (i32.const 3))
    (elem.drop 3)
    (table.copy (i32.const 20) (i32.const 15) (i32.const 5))
    (table.copy (i32.const 21) (i32.const 29) (i32.const 1))
    (table.copy (i32.const 24) (i32.const 10) (i32.const 1))
    (table.copy (i32.const 13) (i32.const 11) (i32.const 4))
    (table.copy (i32.const 19) (i32.const 20) (i32.const 5)))
  (func (export "check") (param i32) (result i32)
    (call_indirect (type 0) (local.get 0)))
)`);

// ./test/core/table_init.wast:161
invoke($3, `test`, []);

// ./test/core/table_init.wast:162
assert_trap(() => invoke($3, `check`, [0]), `uninitialized element`);

// ./test/core/table_init.wast:163
assert_trap(() => invoke($3, `check`, [1]), `uninitialized element`);

// ./test/core/table_init.wast:164
assert_return(() => invoke($3, `check`, [2]), [value("i32", 3)]);

// ./test/core/table_init.wast:165
assert_return(() => invoke($3, `check`, [3]), [value("i32", 1)]);

// ./test/core/table_init.wast:166
assert_return(() => invoke($3, `check`, [4]), [value("i32", 4)]);

// ./test/core/table_init.wast:167
assert_return(() => invoke($3, `check`, [5]), [value("i32", 1)]);

// ./test/core/table_init.wast:168
assert_trap(() => invoke($3, `check`, [6]), `uninitialized element`);

// ./test/core/table_init.wast:169
assert_return(() => invoke($3, `check`, [7]), [value("i32", 2)]);

// ./test/core/table_init.wast:170
assert_return(() => invoke($3, `check`, [8]), [value("i32", 7)]);

// ./test/core/table_init.wast:171
assert_return(() => invoke($3, `check`, [9]), [value("i32", 1)]);

// ./test/core/table_init.wast:172
assert_return(() => invoke($3, `check`, [10]), [value("i32", 8)]);

// ./test/core/table_init.wast:173
assert_trap(() => invoke($3, `check`, [11]), `uninitialized element`);

// ./test/core/table_init.wast:174
assert_return(() => invoke($3, `check`, [12]), [value("i32", 7)]);

// ./test/core/table_init.wast:175
assert_trap(() => invoke($3, `check`, [13]), `uninitialized element`);

// ./test/core/table_init.wast:176
assert_return(() => invoke($3, `check`, [14]), [value("i32", 7)]);

// ./test/core/table_init.wast:177
assert_return(() => invoke($3, `check`, [15]), [value("i32", 5)]);

// ./test/core/table_init.wast:178
assert_return(() => invoke($3, `check`, [16]), [value("i32", 2)]);

// ./test/core/table_init.wast:179
assert_return(() => invoke($3, `check`, [17]), [value("i32", 7)]);

// ./test/core/table_init.wast:180
assert_trap(() => invoke($3, `check`, [18]), `uninitialized element`);

// ./test/core/table_init.wast:181
assert_return(() => invoke($3, `check`, [19]), [value("i32", 9)]);

// ./test/core/table_init.wast:182
assert_trap(() => invoke($3, `check`, [20]), `uninitialized element`);

// ./test/core/table_init.wast:183
assert_return(() => invoke($3, `check`, [21]), [value("i32", 7)]);

// ./test/core/table_init.wast:184
assert_trap(() => invoke($3, `check`, [22]), `uninitialized element`);

// ./test/core/table_init.wast:185
assert_return(() => invoke($3, `check`, [23]), [value("i32", 8)]);

// ./test/core/table_init.wast:186
assert_return(() => invoke($3, `check`, [24]), [value("i32", 8)]);

// ./test/core/table_init.wast:187
assert_trap(() => invoke($3, `check`, [25]), `uninitialized element`);

// ./test/core/table_init.wast:188
assert_trap(() => invoke($3, `check`, [26]), `uninitialized element`);

// ./test/core/table_init.wast:189
assert_trap(() => invoke($3, `check`, [27]), `uninitialized element`);

// ./test/core/table_init.wast:190
assert_trap(() => invoke($3, `check`, [28]), `uninitialized element`);

// ./test/core/table_init.wast:191
assert_trap(() => invoke($3, `check`, [29]), `uninitialized element`);

// ./test/core/table_init.wast:192
assert_invalid(() =>
  instantiate(`(module
    (func (export "test")
      (elem.drop 0)))`), `unknown elem segment 0`);

// ./test/core/table_init.wast:198
assert_invalid(
  () =>
    instantiate(`(module
    (func (export "test")
      (table.init 0 (i32.const 12) (i32.const 1) (i32.const 1))))`),
  `unknown table 0`,
);

// ./test/core/table_init.wast:204
assert_invalid(() =>
  instantiate(`(module
    (elem funcref (ref.func 0))
    (func (result i32) (i32.const 0))
    (func (export "test")
      (elem.drop 4)))`), `unknown elem segment 4`);

// ./test/core/table_init.wast:212
assert_invalid(
  () =>
    instantiate(`(module
    (elem funcref (ref.func 0))
    (func (result i32) (i32.const 0))
    (func (export "test")
      (table.init 4 (i32.const 12) (i32.const 1) (i32.const 1))))`),
  `unknown table 0`,
);

// ./test/core/table_init.wast:221
let $4 = instantiate(`(module
  (table 30 30 funcref)
  (elem (i32.const 2) 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (i32.const 12) 7 5 2 3 6)
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

// ./test/core/table_init.wast:242
invoke($4, `test`, []);

// ./test/core/table_init.wast:244
let $5 = instantiate(`(module
  (table 30 30 funcref)
  (elem (i32.const 2) 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (i32.const 12) 7 5 2 3 6)
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

// ./test/core/table_init.wast:265
assert_trap(() => invoke($5, `test`, []), `out of bounds`);

// ./test/core/table_init.wast:267
let $6 = instantiate(`(module
  (table 30 30 funcref)
  (elem (i32.const 2) 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (i32.const 12) 7 5 2 3 6)
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

// ./test/core/table_init.wast:288
invoke($6, `test`, []);

// ./test/core/table_init.wast:290
let $7 = instantiate(`(module
  (table 30 30 funcref)
  (elem (i32.const 2) 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (i32.const 12) 7 5 2 3 6)
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

// ./test/core/table_init.wast:311
invoke($7, `test`, []);

// ./test/core/table_init.wast:313
let $8 = instantiate(`(module
  (table 30 30 funcref)
  (elem (i32.const 2) 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (i32.const 12) 7 5 2 3 6)
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

// ./test/core/table_init.wast:334
assert_trap(() => invoke($8, `test`, []), `out of bounds`);

// ./test/core/table_init.wast:336
let $9 = instantiate(`(module
  (table 30 30 funcref)
  (elem (i32.const 2) 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (i32.const 12) 7 5 2 3 6)
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

// ./test/core/table_init.wast:357
assert_trap(() => invoke($9, `test`, []), `out of bounds`);

// ./test/core/table_init.wast:359
let $10 = instantiate(`(module
  (table 30 30 funcref)
  (elem (i32.const 2) 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (i32.const 12) 7 5 2 3 6)
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

// ./test/core/table_init.wast:380
assert_trap(() => invoke($10, `test`, []), `out of bounds`);

// ./test/core/table_init.wast:382
let $11 = instantiate(`(module
  (table 30 30 funcref)
  (elem (i32.const 2) 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (i32.const 12) 7 5 2 3 6)
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
    (table.init 1 (i32.const 28) (i32.const 1) (i32.const 3))
    ))`);

// ./test/core/table_init.wast:403
assert_trap(() => invoke($11, `test`, []), `out of bounds`);

// ./test/core/table_init.wast:405
let $12 = instantiate(`(module
  (table 30 30 funcref)
  (elem (i32.const 2) 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (i32.const 12) 7 5 2 3 6)
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
    (table.init 1 (i32.const 12) (i32.const 4) (i32.const 0))
    ))`);

// ./test/core/table_init.wast:426
invoke($12, `test`, []);

// ./test/core/table_init.wast:428
let $13 = instantiate(`(module
  (table 30 30 funcref)
  (elem (i32.const 2) 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (i32.const 12) 7 5 2 3 6)
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
    (table.init 1 (i32.const 12) (i32.const 5) (i32.const 0))
    ))`);

// ./test/core/table_init.wast:449
assert_trap(() => invoke($13, `test`, []), `out of bounds`);

// ./test/core/table_init.wast:451
let $14 = instantiate(`(module
  (table 30 30 funcref)
  (elem (i32.const 2) 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (i32.const 12) 7 5 2 3 6)
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
    (table.init 1 (i32.const 30) (i32.const 2) (i32.const 0))
    ))`);

// ./test/core/table_init.wast:472
invoke($14, `test`, []);

// ./test/core/table_init.wast:474
let $15 = instantiate(`(module
  (table 30 30 funcref)
  (elem (i32.const 2) 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (i32.const 12) 7 5 2 3 6)
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
    (table.init 1 (i32.const 31) (i32.const 2) (i32.const 0))
    ))`);

// ./test/core/table_init.wast:495
assert_trap(() => invoke($15, `test`, []), `out of bounds`);

// ./test/core/table_init.wast:497
let $16 = instantiate(`(module
  (table 30 30 funcref)
  (elem (i32.const 2) 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (i32.const 12) 7 5 2 3 6)
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
    (table.init 1 (i32.const 30) (i32.const 4) (i32.const 0))
    ))`);

// ./test/core/table_init.wast:518
invoke($16, `test`, []);

// ./test/core/table_init.wast:520
let $17 = instantiate(`(module
  (table 30 30 funcref)
  (elem (i32.const 2) 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (i32.const 12) 7 5 2 3 6)
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
    (table.init 1 (i32.const 31) (i32.const 5) (i32.const 0))
    ))`);

// ./test/core/table_init.wast:541
assert_trap(() => invoke($17, `test`, []), `out of bounds`);

// ./test/core/table_init.wast:543
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

// ./test/core/table_init.wast:552
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

// ./test/core/table_init.wast:561
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

// ./test/core/table_init.wast:570
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

// ./test/core/table_init.wast:579
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

// ./test/core/table_init.wast:588
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

// ./test/core/table_init.wast:597
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

// ./test/core/table_init.wast:606
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

// ./test/core/table_init.wast:615
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

// ./test/core/table_init.wast:624
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

// ./test/core/table_init.wast:633
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

// ./test/core/table_init.wast:642
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

// ./test/core/table_init.wast:651
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

// ./test/core/table_init.wast:660
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

// ./test/core/table_init.wast:669
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

// ./test/core/table_init.wast:678
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

// ./test/core/table_init.wast:687
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

// ./test/core/table_init.wast:696
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

// ./test/core/table_init.wast:705
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

// ./test/core/table_init.wast:714
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

// ./test/core/table_init.wast:723
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

// ./test/core/table_init.wast:732
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

// ./test/core/table_init.wast:741
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

// ./test/core/table_init.wast:750
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

// ./test/core/table_init.wast:759
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

// ./test/core/table_init.wast:768
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

// ./test/core/table_init.wast:777
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

// ./test/core/table_init.wast:786
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

// ./test/core/table_init.wast:795
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

// ./test/core/table_init.wast:804
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

// ./test/core/table_init.wast:813
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

// ./test/core/table_init.wast:822
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

// ./test/core/table_init.wast:831
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

// ./test/core/table_init.wast:840
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

// ./test/core/table_init.wast:849
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

// ./test/core/table_init.wast:858
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

// ./test/core/table_init.wast:867
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

// ./test/core/table_init.wast:876
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

// ./test/core/table_init.wast:885
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

// ./test/core/table_init.wast:894
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

// ./test/core/table_init.wast:903
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

// ./test/core/table_init.wast:912
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

// ./test/core/table_init.wast:921
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

// ./test/core/table_init.wast:930
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

// ./test/core/table_init.wast:939
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

// ./test/core/table_init.wast:948
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

// ./test/core/table_init.wast:957
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

// ./test/core/table_init.wast:966
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

// ./test/core/table_init.wast:975
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

// ./test/core/table_init.wast:984
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

// ./test/core/table_init.wast:993
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

// ./test/core/table_init.wast:1002
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

// ./test/core/table_init.wast:1011
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

// ./test/core/table_init.wast:1020
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

// ./test/core/table_init.wast:1029
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

// ./test/core/table_init.wast:1038
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

// ./test/core/table_init.wast:1047
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

// ./test/core/table_init.wast:1056
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

// ./test/core/table_init.wast:1065
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

// ./test/core/table_init.wast:1074
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

// ./test/core/table_init.wast:1083
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

// ./test/core/table_init.wast:1092
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

// ./test/core/table_init.wast:1101
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

// ./test/core/table_init.wast:1110
let $18 = instantiate(`(module
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

// ./test/core/table_init.wast:1138
assert_trap(() => invoke($18, `run`, [24, 16]), `out of bounds`);

// ./test/core/table_init.wast:1139
assert_trap(() => invoke($18, `test`, [0]), `uninitialized element`);

// ./test/core/table_init.wast:1140
assert_trap(() => invoke($18, `test`, [1]), `uninitialized element`);

// ./test/core/table_init.wast:1141
assert_trap(() => invoke($18, `test`, [2]), `uninitialized element`);

// ./test/core/table_init.wast:1142
assert_trap(() => invoke($18, `test`, [3]), `uninitialized element`);

// ./test/core/table_init.wast:1143
assert_trap(() => invoke($18, `test`, [4]), `uninitialized element`);

// ./test/core/table_init.wast:1144
assert_trap(() => invoke($18, `test`, [5]), `uninitialized element`);

// ./test/core/table_init.wast:1145
assert_trap(() => invoke($18, `test`, [6]), `uninitialized element`);

// ./test/core/table_init.wast:1146
assert_trap(() => invoke($18, `test`, [7]), `uninitialized element`);

// ./test/core/table_init.wast:1147
assert_trap(() => invoke($18, `test`, [8]), `uninitialized element`);

// ./test/core/table_init.wast:1148
assert_trap(() => invoke($18, `test`, [9]), `uninitialized element`);

// ./test/core/table_init.wast:1149
assert_trap(() => invoke($18, `test`, [10]), `uninitialized element`);

// ./test/core/table_init.wast:1150
assert_trap(() => invoke($18, `test`, [11]), `uninitialized element`);

// ./test/core/table_init.wast:1151
assert_trap(() => invoke($18, `test`, [12]), `uninitialized element`);

// ./test/core/table_init.wast:1152
assert_trap(() => invoke($18, `test`, [13]), `uninitialized element`);

// ./test/core/table_init.wast:1153
assert_trap(() => invoke($18, `test`, [14]), `uninitialized element`);

// ./test/core/table_init.wast:1154
assert_trap(() => invoke($18, `test`, [15]), `uninitialized element`);

// ./test/core/table_init.wast:1155
assert_trap(() => invoke($18, `test`, [16]), `uninitialized element`);

// ./test/core/table_init.wast:1156
assert_trap(() => invoke($18, `test`, [17]), `uninitialized element`);

// ./test/core/table_init.wast:1157
assert_trap(() => invoke($18, `test`, [18]), `uninitialized element`);

// ./test/core/table_init.wast:1158
assert_trap(() => invoke($18, `test`, [19]), `uninitialized element`);

// ./test/core/table_init.wast:1159
assert_trap(() => invoke($18, `test`, [20]), `uninitialized element`);

// ./test/core/table_init.wast:1160
assert_trap(() => invoke($18, `test`, [21]), `uninitialized element`);

// ./test/core/table_init.wast:1161
assert_trap(() => invoke($18, `test`, [22]), `uninitialized element`);

// ./test/core/table_init.wast:1162
assert_trap(() => invoke($18, `test`, [23]), `uninitialized element`);

// ./test/core/table_init.wast:1163
assert_trap(() => invoke($18, `test`, [24]), `uninitialized element`);

// ./test/core/table_init.wast:1164
assert_trap(() => invoke($18, `test`, [25]), `uninitialized element`);

// ./test/core/table_init.wast:1165
assert_trap(() => invoke($18, `test`, [26]), `uninitialized element`);

// ./test/core/table_init.wast:1166
assert_trap(() => invoke($18, `test`, [27]), `uninitialized element`);

// ./test/core/table_init.wast:1167
assert_trap(() => invoke($18, `test`, [28]), `uninitialized element`);

// ./test/core/table_init.wast:1168
assert_trap(() => invoke($18, `test`, [29]), `uninitialized element`);

// ./test/core/table_init.wast:1169
assert_trap(() => invoke($18, `test`, [30]), `uninitialized element`);

// ./test/core/table_init.wast:1170
assert_trap(() => invoke($18, `test`, [31]), `uninitialized element`);

// ./test/core/table_init.wast:1172
let $19 = instantiate(`(module
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

// ./test/core/table_init.wast:1200
assert_trap(() => invoke($19, `run`, [25, 16]), `out of bounds`);

// ./test/core/table_init.wast:1201
assert_trap(() => invoke($19, `test`, [0]), `uninitialized element`);

// ./test/core/table_init.wast:1202
assert_trap(() => invoke($19, `test`, [1]), `uninitialized element`);

// ./test/core/table_init.wast:1203
assert_trap(() => invoke($19, `test`, [2]), `uninitialized element`);

// ./test/core/table_init.wast:1204
assert_trap(() => invoke($19, `test`, [3]), `uninitialized element`);

// ./test/core/table_init.wast:1205
assert_trap(() => invoke($19, `test`, [4]), `uninitialized element`);

// ./test/core/table_init.wast:1206
assert_trap(() => invoke($19, `test`, [5]), `uninitialized element`);

// ./test/core/table_init.wast:1207
assert_trap(() => invoke($19, `test`, [6]), `uninitialized element`);

// ./test/core/table_init.wast:1208
assert_trap(() => invoke($19, `test`, [7]), `uninitialized element`);

// ./test/core/table_init.wast:1209
assert_trap(() => invoke($19, `test`, [8]), `uninitialized element`);

// ./test/core/table_init.wast:1210
assert_trap(() => invoke($19, `test`, [9]), `uninitialized element`);

// ./test/core/table_init.wast:1211
assert_trap(() => invoke($19, `test`, [10]), `uninitialized element`);

// ./test/core/table_init.wast:1212
assert_trap(() => invoke($19, `test`, [11]), `uninitialized element`);

// ./test/core/table_init.wast:1213
assert_trap(() => invoke($19, `test`, [12]), `uninitialized element`);

// ./test/core/table_init.wast:1214
assert_trap(() => invoke($19, `test`, [13]), `uninitialized element`);

// ./test/core/table_init.wast:1215
assert_trap(() => invoke($19, `test`, [14]), `uninitialized element`);

// ./test/core/table_init.wast:1216
assert_trap(() => invoke($19, `test`, [15]), `uninitialized element`);

// ./test/core/table_init.wast:1217
assert_trap(() => invoke($19, `test`, [16]), `uninitialized element`);

// ./test/core/table_init.wast:1218
assert_trap(() => invoke($19, `test`, [17]), `uninitialized element`);

// ./test/core/table_init.wast:1219
assert_trap(() => invoke($19, `test`, [18]), `uninitialized element`);

// ./test/core/table_init.wast:1220
assert_trap(() => invoke($19, `test`, [19]), `uninitialized element`);

// ./test/core/table_init.wast:1221
assert_trap(() => invoke($19, `test`, [20]), `uninitialized element`);

// ./test/core/table_init.wast:1222
assert_trap(() => invoke($19, `test`, [21]), `uninitialized element`);

// ./test/core/table_init.wast:1223
assert_trap(() => invoke($19, `test`, [22]), `uninitialized element`);

// ./test/core/table_init.wast:1224
assert_trap(() => invoke($19, `test`, [23]), `uninitialized element`);

// ./test/core/table_init.wast:1225
assert_trap(() => invoke($19, `test`, [24]), `uninitialized element`);

// ./test/core/table_init.wast:1226
assert_trap(() => invoke($19, `test`, [25]), `uninitialized element`);

// ./test/core/table_init.wast:1227
assert_trap(() => invoke($19, `test`, [26]), `uninitialized element`);

// ./test/core/table_init.wast:1228
assert_trap(() => invoke($19, `test`, [27]), `uninitialized element`);

// ./test/core/table_init.wast:1229
assert_trap(() => invoke($19, `test`, [28]), `uninitialized element`);

// ./test/core/table_init.wast:1230
assert_trap(() => invoke($19, `test`, [29]), `uninitialized element`);

// ./test/core/table_init.wast:1231
assert_trap(() => invoke($19, `test`, [30]), `uninitialized element`);

// ./test/core/table_init.wast:1232
assert_trap(() => invoke($19, `test`, [31]), `uninitialized element`);

// ./test/core/table_init.wast:1234
let $20 = instantiate(`(module
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

// ./test/core/table_init.wast:1262
assert_trap(() => invoke($20, `run`, [96, 32]), `out of bounds`);

// ./test/core/table_init.wast:1263
assert_trap(() => invoke($20, `test`, [0]), `uninitialized element`);

// ./test/core/table_init.wast:1264
assert_trap(() => invoke($20, `test`, [1]), `uninitialized element`);

// ./test/core/table_init.wast:1265
assert_trap(() => invoke($20, `test`, [2]), `uninitialized element`);

// ./test/core/table_init.wast:1266
assert_trap(() => invoke($20, `test`, [3]), `uninitialized element`);

// ./test/core/table_init.wast:1267
assert_trap(() => invoke($20, `test`, [4]), `uninitialized element`);

// ./test/core/table_init.wast:1268
assert_trap(() => invoke($20, `test`, [5]), `uninitialized element`);

// ./test/core/table_init.wast:1269
assert_trap(() => invoke($20, `test`, [6]), `uninitialized element`);

// ./test/core/table_init.wast:1270
assert_trap(() => invoke($20, `test`, [7]), `uninitialized element`);

// ./test/core/table_init.wast:1271
assert_trap(() => invoke($20, `test`, [8]), `uninitialized element`);

// ./test/core/table_init.wast:1272
assert_trap(() => invoke($20, `test`, [9]), `uninitialized element`);

// ./test/core/table_init.wast:1273
assert_trap(() => invoke($20, `test`, [10]), `uninitialized element`);

// ./test/core/table_init.wast:1274
assert_trap(() => invoke($20, `test`, [11]), `uninitialized element`);

// ./test/core/table_init.wast:1275
assert_trap(() => invoke($20, `test`, [12]), `uninitialized element`);

// ./test/core/table_init.wast:1276
assert_trap(() => invoke($20, `test`, [13]), `uninitialized element`);

// ./test/core/table_init.wast:1277
assert_trap(() => invoke($20, `test`, [14]), `uninitialized element`);

// ./test/core/table_init.wast:1278
assert_trap(() => invoke($20, `test`, [15]), `uninitialized element`);

// ./test/core/table_init.wast:1279
assert_trap(() => invoke($20, `test`, [16]), `uninitialized element`);

// ./test/core/table_init.wast:1280
assert_trap(() => invoke($20, `test`, [17]), `uninitialized element`);

// ./test/core/table_init.wast:1281
assert_trap(() => invoke($20, `test`, [18]), `uninitialized element`);

// ./test/core/table_init.wast:1282
assert_trap(() => invoke($20, `test`, [19]), `uninitialized element`);

// ./test/core/table_init.wast:1283
assert_trap(() => invoke($20, `test`, [20]), `uninitialized element`);

// ./test/core/table_init.wast:1284
assert_trap(() => invoke($20, `test`, [21]), `uninitialized element`);

// ./test/core/table_init.wast:1285
assert_trap(() => invoke($20, `test`, [22]), `uninitialized element`);

// ./test/core/table_init.wast:1286
assert_trap(() => invoke($20, `test`, [23]), `uninitialized element`);

// ./test/core/table_init.wast:1287
assert_trap(() => invoke($20, `test`, [24]), `uninitialized element`);

// ./test/core/table_init.wast:1288
assert_trap(() => invoke($20, `test`, [25]), `uninitialized element`);

// ./test/core/table_init.wast:1289
assert_trap(() => invoke($20, `test`, [26]), `uninitialized element`);

// ./test/core/table_init.wast:1290
assert_trap(() => invoke($20, `test`, [27]), `uninitialized element`);

// ./test/core/table_init.wast:1291
assert_trap(() => invoke($20, `test`, [28]), `uninitialized element`);

// ./test/core/table_init.wast:1292
assert_trap(() => invoke($20, `test`, [29]), `uninitialized element`);

// ./test/core/table_init.wast:1293
assert_trap(() => invoke($20, `test`, [30]), `uninitialized element`);

// ./test/core/table_init.wast:1294
assert_trap(() => invoke($20, `test`, [31]), `uninitialized element`);

// ./test/core/table_init.wast:1295
assert_trap(() => invoke($20, `test`, [32]), `uninitialized element`);

// ./test/core/table_init.wast:1296
assert_trap(() => invoke($20, `test`, [33]), `uninitialized element`);

// ./test/core/table_init.wast:1297
assert_trap(() => invoke($20, `test`, [34]), `uninitialized element`);

// ./test/core/table_init.wast:1298
assert_trap(() => invoke($20, `test`, [35]), `uninitialized element`);

// ./test/core/table_init.wast:1299
assert_trap(() => invoke($20, `test`, [36]), `uninitialized element`);

// ./test/core/table_init.wast:1300
assert_trap(() => invoke($20, `test`, [37]), `uninitialized element`);

// ./test/core/table_init.wast:1301
assert_trap(() => invoke($20, `test`, [38]), `uninitialized element`);

// ./test/core/table_init.wast:1302
assert_trap(() => invoke($20, `test`, [39]), `uninitialized element`);

// ./test/core/table_init.wast:1303
assert_trap(() => invoke($20, `test`, [40]), `uninitialized element`);

// ./test/core/table_init.wast:1304
assert_trap(() => invoke($20, `test`, [41]), `uninitialized element`);

// ./test/core/table_init.wast:1305
assert_trap(() => invoke($20, `test`, [42]), `uninitialized element`);

// ./test/core/table_init.wast:1306
assert_trap(() => invoke($20, `test`, [43]), `uninitialized element`);

// ./test/core/table_init.wast:1307
assert_trap(() => invoke($20, `test`, [44]), `uninitialized element`);

// ./test/core/table_init.wast:1308
assert_trap(() => invoke($20, `test`, [45]), `uninitialized element`);

// ./test/core/table_init.wast:1309
assert_trap(() => invoke($20, `test`, [46]), `uninitialized element`);

// ./test/core/table_init.wast:1310
assert_trap(() => invoke($20, `test`, [47]), `uninitialized element`);

// ./test/core/table_init.wast:1311
assert_trap(() => invoke($20, `test`, [48]), `uninitialized element`);

// ./test/core/table_init.wast:1312
assert_trap(() => invoke($20, `test`, [49]), `uninitialized element`);

// ./test/core/table_init.wast:1313
assert_trap(() => invoke($20, `test`, [50]), `uninitialized element`);

// ./test/core/table_init.wast:1314
assert_trap(() => invoke($20, `test`, [51]), `uninitialized element`);

// ./test/core/table_init.wast:1315
assert_trap(() => invoke($20, `test`, [52]), `uninitialized element`);

// ./test/core/table_init.wast:1316
assert_trap(() => invoke($20, `test`, [53]), `uninitialized element`);

// ./test/core/table_init.wast:1317
assert_trap(() => invoke($20, `test`, [54]), `uninitialized element`);

// ./test/core/table_init.wast:1318
assert_trap(() => invoke($20, `test`, [55]), `uninitialized element`);

// ./test/core/table_init.wast:1319
assert_trap(() => invoke($20, `test`, [56]), `uninitialized element`);

// ./test/core/table_init.wast:1320
assert_trap(() => invoke($20, `test`, [57]), `uninitialized element`);

// ./test/core/table_init.wast:1321
assert_trap(() => invoke($20, `test`, [58]), `uninitialized element`);

// ./test/core/table_init.wast:1322
assert_trap(() => invoke($20, `test`, [59]), `uninitialized element`);

// ./test/core/table_init.wast:1323
assert_trap(() => invoke($20, `test`, [60]), `uninitialized element`);

// ./test/core/table_init.wast:1324
assert_trap(() => invoke($20, `test`, [61]), `uninitialized element`);

// ./test/core/table_init.wast:1325
assert_trap(() => invoke($20, `test`, [62]), `uninitialized element`);

// ./test/core/table_init.wast:1326
assert_trap(() => invoke($20, `test`, [63]), `uninitialized element`);

// ./test/core/table_init.wast:1327
assert_trap(() => invoke($20, `test`, [64]), `uninitialized element`);

// ./test/core/table_init.wast:1328
assert_trap(() => invoke($20, `test`, [65]), `uninitialized element`);

// ./test/core/table_init.wast:1329
assert_trap(() => invoke($20, `test`, [66]), `uninitialized element`);

// ./test/core/table_init.wast:1330
assert_trap(() => invoke($20, `test`, [67]), `uninitialized element`);

// ./test/core/table_init.wast:1331
assert_trap(() => invoke($20, `test`, [68]), `uninitialized element`);

// ./test/core/table_init.wast:1332
assert_trap(() => invoke($20, `test`, [69]), `uninitialized element`);

// ./test/core/table_init.wast:1333
assert_trap(() => invoke($20, `test`, [70]), `uninitialized element`);

// ./test/core/table_init.wast:1334
assert_trap(() => invoke($20, `test`, [71]), `uninitialized element`);

// ./test/core/table_init.wast:1335
assert_trap(() => invoke($20, `test`, [72]), `uninitialized element`);

// ./test/core/table_init.wast:1336
assert_trap(() => invoke($20, `test`, [73]), `uninitialized element`);

// ./test/core/table_init.wast:1337
assert_trap(() => invoke($20, `test`, [74]), `uninitialized element`);

// ./test/core/table_init.wast:1338
assert_trap(() => invoke($20, `test`, [75]), `uninitialized element`);

// ./test/core/table_init.wast:1339
assert_trap(() => invoke($20, `test`, [76]), `uninitialized element`);

// ./test/core/table_init.wast:1340
assert_trap(() => invoke($20, `test`, [77]), `uninitialized element`);

// ./test/core/table_init.wast:1341
assert_trap(() => invoke($20, `test`, [78]), `uninitialized element`);

// ./test/core/table_init.wast:1342
assert_trap(() => invoke($20, `test`, [79]), `uninitialized element`);

// ./test/core/table_init.wast:1343
assert_trap(() => invoke($20, `test`, [80]), `uninitialized element`);

// ./test/core/table_init.wast:1344
assert_trap(() => invoke($20, `test`, [81]), `uninitialized element`);

// ./test/core/table_init.wast:1345
assert_trap(() => invoke($20, `test`, [82]), `uninitialized element`);

// ./test/core/table_init.wast:1346
assert_trap(() => invoke($20, `test`, [83]), `uninitialized element`);

// ./test/core/table_init.wast:1347
assert_trap(() => invoke($20, `test`, [84]), `uninitialized element`);

// ./test/core/table_init.wast:1348
assert_trap(() => invoke($20, `test`, [85]), `uninitialized element`);

// ./test/core/table_init.wast:1349
assert_trap(() => invoke($20, `test`, [86]), `uninitialized element`);

// ./test/core/table_init.wast:1350
assert_trap(() => invoke($20, `test`, [87]), `uninitialized element`);

// ./test/core/table_init.wast:1351
assert_trap(() => invoke($20, `test`, [88]), `uninitialized element`);

// ./test/core/table_init.wast:1352
assert_trap(() => invoke($20, `test`, [89]), `uninitialized element`);

// ./test/core/table_init.wast:1353
assert_trap(() => invoke($20, `test`, [90]), `uninitialized element`);

// ./test/core/table_init.wast:1354
assert_trap(() => invoke($20, `test`, [91]), `uninitialized element`);

// ./test/core/table_init.wast:1355
assert_trap(() => invoke($20, `test`, [92]), `uninitialized element`);

// ./test/core/table_init.wast:1356
assert_trap(() => invoke($20, `test`, [93]), `uninitialized element`);

// ./test/core/table_init.wast:1357
assert_trap(() => invoke($20, `test`, [94]), `uninitialized element`);

// ./test/core/table_init.wast:1358
assert_trap(() => invoke($20, `test`, [95]), `uninitialized element`);

// ./test/core/table_init.wast:1359
assert_trap(() => invoke($20, `test`, [96]), `uninitialized element`);

// ./test/core/table_init.wast:1360
assert_trap(() => invoke($20, `test`, [97]), `uninitialized element`);

// ./test/core/table_init.wast:1361
assert_trap(() => invoke($20, `test`, [98]), `uninitialized element`);

// ./test/core/table_init.wast:1362
assert_trap(() => invoke($20, `test`, [99]), `uninitialized element`);

// ./test/core/table_init.wast:1363
assert_trap(() => invoke($20, `test`, [100]), `uninitialized element`);

// ./test/core/table_init.wast:1364
assert_trap(() => invoke($20, `test`, [101]), `uninitialized element`);

// ./test/core/table_init.wast:1365
assert_trap(() => invoke($20, `test`, [102]), `uninitialized element`);

// ./test/core/table_init.wast:1366
assert_trap(() => invoke($20, `test`, [103]), `uninitialized element`);

// ./test/core/table_init.wast:1367
assert_trap(() => invoke($20, `test`, [104]), `uninitialized element`);

// ./test/core/table_init.wast:1368
assert_trap(() => invoke($20, `test`, [105]), `uninitialized element`);

// ./test/core/table_init.wast:1369
assert_trap(() => invoke($20, `test`, [106]), `uninitialized element`);

// ./test/core/table_init.wast:1370
assert_trap(() => invoke($20, `test`, [107]), `uninitialized element`);

// ./test/core/table_init.wast:1371
assert_trap(() => invoke($20, `test`, [108]), `uninitialized element`);

// ./test/core/table_init.wast:1372
assert_trap(() => invoke($20, `test`, [109]), `uninitialized element`);

// ./test/core/table_init.wast:1373
assert_trap(() => invoke($20, `test`, [110]), `uninitialized element`);

// ./test/core/table_init.wast:1374
assert_trap(() => invoke($20, `test`, [111]), `uninitialized element`);

// ./test/core/table_init.wast:1375
assert_trap(() => invoke($20, `test`, [112]), `uninitialized element`);

// ./test/core/table_init.wast:1376
assert_trap(() => invoke($20, `test`, [113]), `uninitialized element`);

// ./test/core/table_init.wast:1377
assert_trap(() => invoke($20, `test`, [114]), `uninitialized element`);

// ./test/core/table_init.wast:1378
assert_trap(() => invoke($20, `test`, [115]), `uninitialized element`);

// ./test/core/table_init.wast:1379
assert_trap(() => invoke($20, `test`, [116]), `uninitialized element`);

// ./test/core/table_init.wast:1380
assert_trap(() => invoke($20, `test`, [117]), `uninitialized element`);

// ./test/core/table_init.wast:1381
assert_trap(() => invoke($20, `test`, [118]), `uninitialized element`);

// ./test/core/table_init.wast:1382
assert_trap(() => invoke($20, `test`, [119]), `uninitialized element`);

// ./test/core/table_init.wast:1383
assert_trap(() => invoke($20, `test`, [120]), `uninitialized element`);

// ./test/core/table_init.wast:1384
assert_trap(() => invoke($20, `test`, [121]), `uninitialized element`);

// ./test/core/table_init.wast:1385
assert_trap(() => invoke($20, `test`, [122]), `uninitialized element`);

// ./test/core/table_init.wast:1386
assert_trap(() => invoke($20, `test`, [123]), `uninitialized element`);

// ./test/core/table_init.wast:1387
assert_trap(() => invoke($20, `test`, [124]), `uninitialized element`);

// ./test/core/table_init.wast:1388
assert_trap(() => invoke($20, `test`, [125]), `uninitialized element`);

// ./test/core/table_init.wast:1389
assert_trap(() => invoke($20, `test`, [126]), `uninitialized element`);

// ./test/core/table_init.wast:1390
assert_trap(() => invoke($20, `test`, [127]), `uninitialized element`);

// ./test/core/table_init.wast:1391
assert_trap(() => invoke($20, `test`, [128]), `uninitialized element`);

// ./test/core/table_init.wast:1392
assert_trap(() => invoke($20, `test`, [129]), `uninitialized element`);

// ./test/core/table_init.wast:1393
assert_trap(() => invoke($20, `test`, [130]), `uninitialized element`);

// ./test/core/table_init.wast:1394
assert_trap(() => invoke($20, `test`, [131]), `uninitialized element`);

// ./test/core/table_init.wast:1395
assert_trap(() => invoke($20, `test`, [132]), `uninitialized element`);

// ./test/core/table_init.wast:1396
assert_trap(() => invoke($20, `test`, [133]), `uninitialized element`);

// ./test/core/table_init.wast:1397
assert_trap(() => invoke($20, `test`, [134]), `uninitialized element`);

// ./test/core/table_init.wast:1398
assert_trap(() => invoke($20, `test`, [135]), `uninitialized element`);

// ./test/core/table_init.wast:1399
assert_trap(() => invoke($20, `test`, [136]), `uninitialized element`);

// ./test/core/table_init.wast:1400
assert_trap(() => invoke($20, `test`, [137]), `uninitialized element`);

// ./test/core/table_init.wast:1401
assert_trap(() => invoke($20, `test`, [138]), `uninitialized element`);

// ./test/core/table_init.wast:1402
assert_trap(() => invoke($20, `test`, [139]), `uninitialized element`);

// ./test/core/table_init.wast:1403
assert_trap(() => invoke($20, `test`, [140]), `uninitialized element`);

// ./test/core/table_init.wast:1404
assert_trap(() => invoke($20, `test`, [141]), `uninitialized element`);

// ./test/core/table_init.wast:1405
assert_trap(() => invoke($20, `test`, [142]), `uninitialized element`);

// ./test/core/table_init.wast:1406
assert_trap(() => invoke($20, `test`, [143]), `uninitialized element`);

// ./test/core/table_init.wast:1407
assert_trap(() => invoke($20, `test`, [144]), `uninitialized element`);

// ./test/core/table_init.wast:1408
assert_trap(() => invoke($20, `test`, [145]), `uninitialized element`);

// ./test/core/table_init.wast:1409
assert_trap(() => invoke($20, `test`, [146]), `uninitialized element`);

// ./test/core/table_init.wast:1410
assert_trap(() => invoke($20, `test`, [147]), `uninitialized element`);

// ./test/core/table_init.wast:1411
assert_trap(() => invoke($20, `test`, [148]), `uninitialized element`);

// ./test/core/table_init.wast:1412
assert_trap(() => invoke($20, `test`, [149]), `uninitialized element`);

// ./test/core/table_init.wast:1413
assert_trap(() => invoke($20, `test`, [150]), `uninitialized element`);

// ./test/core/table_init.wast:1414
assert_trap(() => invoke($20, `test`, [151]), `uninitialized element`);

// ./test/core/table_init.wast:1415
assert_trap(() => invoke($20, `test`, [152]), `uninitialized element`);

// ./test/core/table_init.wast:1416
assert_trap(() => invoke($20, `test`, [153]), `uninitialized element`);

// ./test/core/table_init.wast:1417
assert_trap(() => invoke($20, `test`, [154]), `uninitialized element`);

// ./test/core/table_init.wast:1418
assert_trap(() => invoke($20, `test`, [155]), `uninitialized element`);

// ./test/core/table_init.wast:1419
assert_trap(() => invoke($20, `test`, [156]), `uninitialized element`);

// ./test/core/table_init.wast:1420
assert_trap(() => invoke($20, `test`, [157]), `uninitialized element`);

// ./test/core/table_init.wast:1421
assert_trap(() => invoke($20, `test`, [158]), `uninitialized element`);

// ./test/core/table_init.wast:1422
assert_trap(() => invoke($20, `test`, [159]), `uninitialized element`);

// ./test/core/table_init.wast:1424
let $21 = instantiate(`(module
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

// ./test/core/table_init.wast:1452
assert_trap(() => invoke($21, `run`, [97, 31]), `out of bounds`);

// ./test/core/table_init.wast:1453
assert_trap(() => invoke($21, `test`, [0]), `uninitialized element`);

// ./test/core/table_init.wast:1454
assert_trap(() => invoke($21, `test`, [1]), `uninitialized element`);

// ./test/core/table_init.wast:1455
assert_trap(() => invoke($21, `test`, [2]), `uninitialized element`);

// ./test/core/table_init.wast:1456
assert_trap(() => invoke($21, `test`, [3]), `uninitialized element`);

// ./test/core/table_init.wast:1457
assert_trap(() => invoke($21, `test`, [4]), `uninitialized element`);

// ./test/core/table_init.wast:1458
assert_trap(() => invoke($21, `test`, [5]), `uninitialized element`);

// ./test/core/table_init.wast:1459
assert_trap(() => invoke($21, `test`, [6]), `uninitialized element`);

// ./test/core/table_init.wast:1460
assert_trap(() => invoke($21, `test`, [7]), `uninitialized element`);

// ./test/core/table_init.wast:1461
assert_trap(() => invoke($21, `test`, [8]), `uninitialized element`);

// ./test/core/table_init.wast:1462
assert_trap(() => invoke($21, `test`, [9]), `uninitialized element`);

// ./test/core/table_init.wast:1463
assert_trap(() => invoke($21, `test`, [10]), `uninitialized element`);

// ./test/core/table_init.wast:1464
assert_trap(() => invoke($21, `test`, [11]), `uninitialized element`);

// ./test/core/table_init.wast:1465
assert_trap(() => invoke($21, `test`, [12]), `uninitialized element`);

// ./test/core/table_init.wast:1466
assert_trap(() => invoke($21, `test`, [13]), `uninitialized element`);

// ./test/core/table_init.wast:1467
assert_trap(() => invoke($21, `test`, [14]), `uninitialized element`);

// ./test/core/table_init.wast:1468
assert_trap(() => invoke($21, `test`, [15]), `uninitialized element`);

// ./test/core/table_init.wast:1469
assert_trap(() => invoke($21, `test`, [16]), `uninitialized element`);

// ./test/core/table_init.wast:1470
assert_trap(() => invoke($21, `test`, [17]), `uninitialized element`);

// ./test/core/table_init.wast:1471
assert_trap(() => invoke($21, `test`, [18]), `uninitialized element`);

// ./test/core/table_init.wast:1472
assert_trap(() => invoke($21, `test`, [19]), `uninitialized element`);

// ./test/core/table_init.wast:1473
assert_trap(() => invoke($21, `test`, [20]), `uninitialized element`);

// ./test/core/table_init.wast:1474
assert_trap(() => invoke($21, `test`, [21]), `uninitialized element`);

// ./test/core/table_init.wast:1475
assert_trap(() => invoke($21, `test`, [22]), `uninitialized element`);

// ./test/core/table_init.wast:1476
assert_trap(() => invoke($21, `test`, [23]), `uninitialized element`);

// ./test/core/table_init.wast:1477
assert_trap(() => invoke($21, `test`, [24]), `uninitialized element`);

// ./test/core/table_init.wast:1478
assert_trap(() => invoke($21, `test`, [25]), `uninitialized element`);

// ./test/core/table_init.wast:1479
assert_trap(() => invoke($21, `test`, [26]), `uninitialized element`);

// ./test/core/table_init.wast:1480
assert_trap(() => invoke($21, `test`, [27]), `uninitialized element`);

// ./test/core/table_init.wast:1481
assert_trap(() => invoke($21, `test`, [28]), `uninitialized element`);

// ./test/core/table_init.wast:1482
assert_trap(() => invoke($21, `test`, [29]), `uninitialized element`);

// ./test/core/table_init.wast:1483
assert_trap(() => invoke($21, `test`, [30]), `uninitialized element`);

// ./test/core/table_init.wast:1484
assert_trap(() => invoke($21, `test`, [31]), `uninitialized element`);

// ./test/core/table_init.wast:1485
assert_trap(() => invoke($21, `test`, [32]), `uninitialized element`);

// ./test/core/table_init.wast:1486
assert_trap(() => invoke($21, `test`, [33]), `uninitialized element`);

// ./test/core/table_init.wast:1487
assert_trap(() => invoke($21, `test`, [34]), `uninitialized element`);

// ./test/core/table_init.wast:1488
assert_trap(() => invoke($21, `test`, [35]), `uninitialized element`);

// ./test/core/table_init.wast:1489
assert_trap(() => invoke($21, `test`, [36]), `uninitialized element`);

// ./test/core/table_init.wast:1490
assert_trap(() => invoke($21, `test`, [37]), `uninitialized element`);

// ./test/core/table_init.wast:1491
assert_trap(() => invoke($21, `test`, [38]), `uninitialized element`);

// ./test/core/table_init.wast:1492
assert_trap(() => invoke($21, `test`, [39]), `uninitialized element`);

// ./test/core/table_init.wast:1493
assert_trap(() => invoke($21, `test`, [40]), `uninitialized element`);

// ./test/core/table_init.wast:1494
assert_trap(() => invoke($21, `test`, [41]), `uninitialized element`);

// ./test/core/table_init.wast:1495
assert_trap(() => invoke($21, `test`, [42]), `uninitialized element`);

// ./test/core/table_init.wast:1496
assert_trap(() => invoke($21, `test`, [43]), `uninitialized element`);

// ./test/core/table_init.wast:1497
assert_trap(() => invoke($21, `test`, [44]), `uninitialized element`);

// ./test/core/table_init.wast:1498
assert_trap(() => invoke($21, `test`, [45]), `uninitialized element`);

// ./test/core/table_init.wast:1499
assert_trap(() => invoke($21, `test`, [46]), `uninitialized element`);

// ./test/core/table_init.wast:1500
assert_trap(() => invoke($21, `test`, [47]), `uninitialized element`);

// ./test/core/table_init.wast:1501
assert_trap(() => invoke($21, `test`, [48]), `uninitialized element`);

// ./test/core/table_init.wast:1502
assert_trap(() => invoke($21, `test`, [49]), `uninitialized element`);

// ./test/core/table_init.wast:1503
assert_trap(() => invoke($21, `test`, [50]), `uninitialized element`);

// ./test/core/table_init.wast:1504
assert_trap(() => invoke($21, `test`, [51]), `uninitialized element`);

// ./test/core/table_init.wast:1505
assert_trap(() => invoke($21, `test`, [52]), `uninitialized element`);

// ./test/core/table_init.wast:1506
assert_trap(() => invoke($21, `test`, [53]), `uninitialized element`);

// ./test/core/table_init.wast:1507
assert_trap(() => invoke($21, `test`, [54]), `uninitialized element`);

// ./test/core/table_init.wast:1508
assert_trap(() => invoke($21, `test`, [55]), `uninitialized element`);

// ./test/core/table_init.wast:1509
assert_trap(() => invoke($21, `test`, [56]), `uninitialized element`);

// ./test/core/table_init.wast:1510
assert_trap(() => invoke($21, `test`, [57]), `uninitialized element`);

// ./test/core/table_init.wast:1511
assert_trap(() => invoke($21, `test`, [58]), `uninitialized element`);

// ./test/core/table_init.wast:1512
assert_trap(() => invoke($21, `test`, [59]), `uninitialized element`);

// ./test/core/table_init.wast:1513
assert_trap(() => invoke($21, `test`, [60]), `uninitialized element`);

// ./test/core/table_init.wast:1514
assert_trap(() => invoke($21, `test`, [61]), `uninitialized element`);

// ./test/core/table_init.wast:1515
assert_trap(() => invoke($21, `test`, [62]), `uninitialized element`);

// ./test/core/table_init.wast:1516
assert_trap(() => invoke($21, `test`, [63]), `uninitialized element`);

// ./test/core/table_init.wast:1517
assert_trap(() => invoke($21, `test`, [64]), `uninitialized element`);

// ./test/core/table_init.wast:1518
assert_trap(() => invoke($21, `test`, [65]), `uninitialized element`);

// ./test/core/table_init.wast:1519
assert_trap(() => invoke($21, `test`, [66]), `uninitialized element`);

// ./test/core/table_init.wast:1520
assert_trap(() => invoke($21, `test`, [67]), `uninitialized element`);

// ./test/core/table_init.wast:1521
assert_trap(() => invoke($21, `test`, [68]), `uninitialized element`);

// ./test/core/table_init.wast:1522
assert_trap(() => invoke($21, `test`, [69]), `uninitialized element`);

// ./test/core/table_init.wast:1523
assert_trap(() => invoke($21, `test`, [70]), `uninitialized element`);

// ./test/core/table_init.wast:1524
assert_trap(() => invoke($21, `test`, [71]), `uninitialized element`);

// ./test/core/table_init.wast:1525
assert_trap(() => invoke($21, `test`, [72]), `uninitialized element`);

// ./test/core/table_init.wast:1526
assert_trap(() => invoke($21, `test`, [73]), `uninitialized element`);

// ./test/core/table_init.wast:1527
assert_trap(() => invoke($21, `test`, [74]), `uninitialized element`);

// ./test/core/table_init.wast:1528
assert_trap(() => invoke($21, `test`, [75]), `uninitialized element`);

// ./test/core/table_init.wast:1529
assert_trap(() => invoke($21, `test`, [76]), `uninitialized element`);

// ./test/core/table_init.wast:1530
assert_trap(() => invoke($21, `test`, [77]), `uninitialized element`);

// ./test/core/table_init.wast:1531
assert_trap(() => invoke($21, `test`, [78]), `uninitialized element`);

// ./test/core/table_init.wast:1532
assert_trap(() => invoke($21, `test`, [79]), `uninitialized element`);

// ./test/core/table_init.wast:1533
assert_trap(() => invoke($21, `test`, [80]), `uninitialized element`);

// ./test/core/table_init.wast:1534
assert_trap(() => invoke($21, `test`, [81]), `uninitialized element`);

// ./test/core/table_init.wast:1535
assert_trap(() => invoke($21, `test`, [82]), `uninitialized element`);

// ./test/core/table_init.wast:1536
assert_trap(() => invoke($21, `test`, [83]), `uninitialized element`);

// ./test/core/table_init.wast:1537
assert_trap(() => invoke($21, `test`, [84]), `uninitialized element`);

// ./test/core/table_init.wast:1538
assert_trap(() => invoke($21, `test`, [85]), `uninitialized element`);

// ./test/core/table_init.wast:1539
assert_trap(() => invoke($21, `test`, [86]), `uninitialized element`);

// ./test/core/table_init.wast:1540
assert_trap(() => invoke($21, `test`, [87]), `uninitialized element`);

// ./test/core/table_init.wast:1541
assert_trap(() => invoke($21, `test`, [88]), `uninitialized element`);

// ./test/core/table_init.wast:1542
assert_trap(() => invoke($21, `test`, [89]), `uninitialized element`);

// ./test/core/table_init.wast:1543
assert_trap(() => invoke($21, `test`, [90]), `uninitialized element`);

// ./test/core/table_init.wast:1544
assert_trap(() => invoke($21, `test`, [91]), `uninitialized element`);

// ./test/core/table_init.wast:1545
assert_trap(() => invoke($21, `test`, [92]), `uninitialized element`);

// ./test/core/table_init.wast:1546
assert_trap(() => invoke($21, `test`, [93]), `uninitialized element`);

// ./test/core/table_init.wast:1547
assert_trap(() => invoke($21, `test`, [94]), `uninitialized element`);

// ./test/core/table_init.wast:1548
assert_trap(() => invoke($21, `test`, [95]), `uninitialized element`);

// ./test/core/table_init.wast:1549
assert_trap(() => invoke($21, `test`, [96]), `uninitialized element`);

// ./test/core/table_init.wast:1550
assert_trap(() => invoke($21, `test`, [97]), `uninitialized element`);

// ./test/core/table_init.wast:1551
assert_trap(() => invoke($21, `test`, [98]), `uninitialized element`);

// ./test/core/table_init.wast:1552
assert_trap(() => invoke($21, `test`, [99]), `uninitialized element`);

// ./test/core/table_init.wast:1553
assert_trap(() => invoke($21, `test`, [100]), `uninitialized element`);

// ./test/core/table_init.wast:1554
assert_trap(() => invoke($21, `test`, [101]), `uninitialized element`);

// ./test/core/table_init.wast:1555
assert_trap(() => invoke($21, `test`, [102]), `uninitialized element`);

// ./test/core/table_init.wast:1556
assert_trap(() => invoke($21, `test`, [103]), `uninitialized element`);

// ./test/core/table_init.wast:1557
assert_trap(() => invoke($21, `test`, [104]), `uninitialized element`);

// ./test/core/table_init.wast:1558
assert_trap(() => invoke($21, `test`, [105]), `uninitialized element`);

// ./test/core/table_init.wast:1559
assert_trap(() => invoke($21, `test`, [106]), `uninitialized element`);

// ./test/core/table_init.wast:1560
assert_trap(() => invoke($21, `test`, [107]), `uninitialized element`);

// ./test/core/table_init.wast:1561
assert_trap(() => invoke($21, `test`, [108]), `uninitialized element`);

// ./test/core/table_init.wast:1562
assert_trap(() => invoke($21, `test`, [109]), `uninitialized element`);

// ./test/core/table_init.wast:1563
assert_trap(() => invoke($21, `test`, [110]), `uninitialized element`);

// ./test/core/table_init.wast:1564
assert_trap(() => invoke($21, `test`, [111]), `uninitialized element`);

// ./test/core/table_init.wast:1565
assert_trap(() => invoke($21, `test`, [112]), `uninitialized element`);

// ./test/core/table_init.wast:1566
assert_trap(() => invoke($21, `test`, [113]), `uninitialized element`);

// ./test/core/table_init.wast:1567
assert_trap(() => invoke($21, `test`, [114]), `uninitialized element`);

// ./test/core/table_init.wast:1568
assert_trap(() => invoke($21, `test`, [115]), `uninitialized element`);

// ./test/core/table_init.wast:1569
assert_trap(() => invoke($21, `test`, [116]), `uninitialized element`);

// ./test/core/table_init.wast:1570
assert_trap(() => invoke($21, `test`, [117]), `uninitialized element`);

// ./test/core/table_init.wast:1571
assert_trap(() => invoke($21, `test`, [118]), `uninitialized element`);

// ./test/core/table_init.wast:1572
assert_trap(() => invoke($21, `test`, [119]), `uninitialized element`);

// ./test/core/table_init.wast:1573
assert_trap(() => invoke($21, `test`, [120]), `uninitialized element`);

// ./test/core/table_init.wast:1574
assert_trap(() => invoke($21, `test`, [121]), `uninitialized element`);

// ./test/core/table_init.wast:1575
assert_trap(() => invoke($21, `test`, [122]), `uninitialized element`);

// ./test/core/table_init.wast:1576
assert_trap(() => invoke($21, `test`, [123]), `uninitialized element`);

// ./test/core/table_init.wast:1577
assert_trap(() => invoke($21, `test`, [124]), `uninitialized element`);

// ./test/core/table_init.wast:1578
assert_trap(() => invoke($21, `test`, [125]), `uninitialized element`);

// ./test/core/table_init.wast:1579
assert_trap(() => invoke($21, `test`, [126]), `uninitialized element`);

// ./test/core/table_init.wast:1580
assert_trap(() => invoke($21, `test`, [127]), `uninitialized element`);

// ./test/core/table_init.wast:1581
assert_trap(() => invoke($21, `test`, [128]), `uninitialized element`);

// ./test/core/table_init.wast:1582
assert_trap(() => invoke($21, `test`, [129]), `uninitialized element`);

// ./test/core/table_init.wast:1583
assert_trap(() => invoke($21, `test`, [130]), `uninitialized element`);

// ./test/core/table_init.wast:1584
assert_trap(() => invoke($21, `test`, [131]), `uninitialized element`);

// ./test/core/table_init.wast:1585
assert_trap(() => invoke($21, `test`, [132]), `uninitialized element`);

// ./test/core/table_init.wast:1586
assert_trap(() => invoke($21, `test`, [133]), `uninitialized element`);

// ./test/core/table_init.wast:1587
assert_trap(() => invoke($21, `test`, [134]), `uninitialized element`);

// ./test/core/table_init.wast:1588
assert_trap(() => invoke($21, `test`, [135]), `uninitialized element`);

// ./test/core/table_init.wast:1589
assert_trap(() => invoke($21, `test`, [136]), `uninitialized element`);

// ./test/core/table_init.wast:1590
assert_trap(() => invoke($21, `test`, [137]), `uninitialized element`);

// ./test/core/table_init.wast:1591
assert_trap(() => invoke($21, `test`, [138]), `uninitialized element`);

// ./test/core/table_init.wast:1592
assert_trap(() => invoke($21, `test`, [139]), `uninitialized element`);

// ./test/core/table_init.wast:1593
assert_trap(() => invoke($21, `test`, [140]), `uninitialized element`);

// ./test/core/table_init.wast:1594
assert_trap(() => invoke($21, `test`, [141]), `uninitialized element`);

// ./test/core/table_init.wast:1595
assert_trap(() => invoke($21, `test`, [142]), `uninitialized element`);

// ./test/core/table_init.wast:1596
assert_trap(() => invoke($21, `test`, [143]), `uninitialized element`);

// ./test/core/table_init.wast:1597
assert_trap(() => invoke($21, `test`, [144]), `uninitialized element`);

// ./test/core/table_init.wast:1598
assert_trap(() => invoke($21, `test`, [145]), `uninitialized element`);

// ./test/core/table_init.wast:1599
assert_trap(() => invoke($21, `test`, [146]), `uninitialized element`);

// ./test/core/table_init.wast:1600
assert_trap(() => invoke($21, `test`, [147]), `uninitialized element`);

// ./test/core/table_init.wast:1601
assert_trap(() => invoke($21, `test`, [148]), `uninitialized element`);

// ./test/core/table_init.wast:1602
assert_trap(() => invoke($21, `test`, [149]), `uninitialized element`);

// ./test/core/table_init.wast:1603
assert_trap(() => invoke($21, `test`, [150]), `uninitialized element`);

// ./test/core/table_init.wast:1604
assert_trap(() => invoke($21, `test`, [151]), `uninitialized element`);

// ./test/core/table_init.wast:1605
assert_trap(() => invoke($21, `test`, [152]), `uninitialized element`);

// ./test/core/table_init.wast:1606
assert_trap(() => invoke($21, `test`, [153]), `uninitialized element`);

// ./test/core/table_init.wast:1607
assert_trap(() => invoke($21, `test`, [154]), `uninitialized element`);

// ./test/core/table_init.wast:1608
assert_trap(() => invoke($21, `test`, [155]), `uninitialized element`);

// ./test/core/table_init.wast:1609
assert_trap(() => invoke($21, `test`, [156]), `uninitialized element`);

// ./test/core/table_init.wast:1610
assert_trap(() => invoke($21, `test`, [157]), `uninitialized element`);

// ./test/core/table_init.wast:1611
assert_trap(() => invoke($21, `test`, [158]), `uninitialized element`);

// ./test/core/table_init.wast:1612
assert_trap(() => invoke($21, `test`, [159]), `uninitialized element`);

// ./test/core/table_init.wast:1614
let $22 = instantiate(`(module
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

// ./test/core/table_init.wast:1642
assert_trap(() => invoke($22, `run`, [48, -16]), `out of bounds`);

// ./test/core/table_init.wast:1643
assert_trap(() => invoke($22, `test`, [0]), `uninitialized element`);

// ./test/core/table_init.wast:1644
assert_trap(() => invoke($22, `test`, [1]), `uninitialized element`);

// ./test/core/table_init.wast:1645
assert_trap(() => invoke($22, `test`, [2]), `uninitialized element`);

// ./test/core/table_init.wast:1646
assert_trap(() => invoke($22, `test`, [3]), `uninitialized element`);

// ./test/core/table_init.wast:1647
assert_trap(() => invoke($22, `test`, [4]), `uninitialized element`);

// ./test/core/table_init.wast:1648
assert_trap(() => invoke($22, `test`, [5]), `uninitialized element`);

// ./test/core/table_init.wast:1649
assert_trap(() => invoke($22, `test`, [6]), `uninitialized element`);

// ./test/core/table_init.wast:1650
assert_trap(() => invoke($22, `test`, [7]), `uninitialized element`);

// ./test/core/table_init.wast:1651
assert_trap(() => invoke($22, `test`, [8]), `uninitialized element`);

// ./test/core/table_init.wast:1652
assert_trap(() => invoke($22, `test`, [9]), `uninitialized element`);

// ./test/core/table_init.wast:1653
assert_trap(() => invoke($22, `test`, [10]), `uninitialized element`);

// ./test/core/table_init.wast:1654
assert_trap(() => invoke($22, `test`, [11]), `uninitialized element`);

// ./test/core/table_init.wast:1655
assert_trap(() => invoke($22, `test`, [12]), `uninitialized element`);

// ./test/core/table_init.wast:1656
assert_trap(() => invoke($22, `test`, [13]), `uninitialized element`);

// ./test/core/table_init.wast:1657
assert_trap(() => invoke($22, `test`, [14]), `uninitialized element`);

// ./test/core/table_init.wast:1658
assert_trap(() => invoke($22, `test`, [15]), `uninitialized element`);

// ./test/core/table_init.wast:1659
assert_trap(() => invoke($22, `test`, [16]), `uninitialized element`);

// ./test/core/table_init.wast:1660
assert_trap(() => invoke($22, `test`, [17]), `uninitialized element`);

// ./test/core/table_init.wast:1661
assert_trap(() => invoke($22, `test`, [18]), `uninitialized element`);

// ./test/core/table_init.wast:1662
assert_trap(() => invoke($22, `test`, [19]), `uninitialized element`);

// ./test/core/table_init.wast:1663
assert_trap(() => invoke($22, `test`, [20]), `uninitialized element`);

// ./test/core/table_init.wast:1664
assert_trap(() => invoke($22, `test`, [21]), `uninitialized element`);

// ./test/core/table_init.wast:1665
assert_trap(() => invoke($22, `test`, [22]), `uninitialized element`);

// ./test/core/table_init.wast:1666
assert_trap(() => invoke($22, `test`, [23]), `uninitialized element`);

// ./test/core/table_init.wast:1667
assert_trap(() => invoke($22, `test`, [24]), `uninitialized element`);

// ./test/core/table_init.wast:1668
assert_trap(() => invoke($22, `test`, [25]), `uninitialized element`);

// ./test/core/table_init.wast:1669
assert_trap(() => invoke($22, `test`, [26]), `uninitialized element`);

// ./test/core/table_init.wast:1670
assert_trap(() => invoke($22, `test`, [27]), `uninitialized element`);

// ./test/core/table_init.wast:1671
assert_trap(() => invoke($22, `test`, [28]), `uninitialized element`);

// ./test/core/table_init.wast:1672
assert_trap(() => invoke($22, `test`, [29]), `uninitialized element`);

// ./test/core/table_init.wast:1673
assert_trap(() => invoke($22, `test`, [30]), `uninitialized element`);

// ./test/core/table_init.wast:1674
assert_trap(() => invoke($22, `test`, [31]), `uninitialized element`);

// ./test/core/table_init.wast:1675
assert_trap(() => invoke($22, `test`, [32]), `uninitialized element`);

// ./test/core/table_init.wast:1676
assert_trap(() => invoke($22, `test`, [33]), `uninitialized element`);

// ./test/core/table_init.wast:1677
assert_trap(() => invoke($22, `test`, [34]), `uninitialized element`);

// ./test/core/table_init.wast:1678
assert_trap(() => invoke($22, `test`, [35]), `uninitialized element`);

// ./test/core/table_init.wast:1679
assert_trap(() => invoke($22, `test`, [36]), `uninitialized element`);

// ./test/core/table_init.wast:1680
assert_trap(() => invoke($22, `test`, [37]), `uninitialized element`);

// ./test/core/table_init.wast:1681
assert_trap(() => invoke($22, `test`, [38]), `uninitialized element`);

// ./test/core/table_init.wast:1682
assert_trap(() => invoke($22, `test`, [39]), `uninitialized element`);

// ./test/core/table_init.wast:1683
assert_trap(() => invoke($22, `test`, [40]), `uninitialized element`);

// ./test/core/table_init.wast:1684
assert_trap(() => invoke($22, `test`, [41]), `uninitialized element`);

// ./test/core/table_init.wast:1685
assert_trap(() => invoke($22, `test`, [42]), `uninitialized element`);

// ./test/core/table_init.wast:1686
assert_trap(() => invoke($22, `test`, [43]), `uninitialized element`);

// ./test/core/table_init.wast:1687
assert_trap(() => invoke($22, `test`, [44]), `uninitialized element`);

// ./test/core/table_init.wast:1688
assert_trap(() => invoke($22, `test`, [45]), `uninitialized element`);

// ./test/core/table_init.wast:1689
assert_trap(() => invoke($22, `test`, [46]), `uninitialized element`);

// ./test/core/table_init.wast:1690
assert_trap(() => invoke($22, `test`, [47]), `uninitialized element`);

// ./test/core/table_init.wast:1691
assert_trap(() => invoke($22, `test`, [48]), `uninitialized element`);

// ./test/core/table_init.wast:1692
assert_trap(() => invoke($22, `test`, [49]), `uninitialized element`);

// ./test/core/table_init.wast:1693
assert_trap(() => invoke($22, `test`, [50]), `uninitialized element`);

// ./test/core/table_init.wast:1694
assert_trap(() => invoke($22, `test`, [51]), `uninitialized element`);

// ./test/core/table_init.wast:1695
assert_trap(() => invoke($22, `test`, [52]), `uninitialized element`);

// ./test/core/table_init.wast:1696
assert_trap(() => invoke($22, `test`, [53]), `uninitialized element`);

// ./test/core/table_init.wast:1697
assert_trap(() => invoke($22, `test`, [54]), `uninitialized element`);

// ./test/core/table_init.wast:1698
assert_trap(() => invoke($22, `test`, [55]), `uninitialized element`);

// ./test/core/table_init.wast:1699
assert_trap(() => invoke($22, `test`, [56]), `uninitialized element`);

// ./test/core/table_init.wast:1700
assert_trap(() => invoke($22, `test`, [57]), `uninitialized element`);

// ./test/core/table_init.wast:1701
assert_trap(() => invoke($22, `test`, [58]), `uninitialized element`);

// ./test/core/table_init.wast:1702
assert_trap(() => invoke($22, `test`, [59]), `uninitialized element`);

// ./test/core/table_init.wast:1703
assert_trap(() => invoke($22, `test`, [60]), `uninitialized element`);

// ./test/core/table_init.wast:1704
assert_trap(() => invoke($22, `test`, [61]), `uninitialized element`);

// ./test/core/table_init.wast:1705
assert_trap(() => invoke($22, `test`, [62]), `uninitialized element`);

// ./test/core/table_init.wast:1706
assert_trap(() => invoke($22, `test`, [63]), `uninitialized element`);

// ./test/core/table_init.wast:1708
let $23 = instantiate(`(module
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

// ./test/core/table_init.wast:1736
assert_trap(() => invoke($23, `run`, [0, -4]), `out of bounds`);

// ./test/core/table_init.wast:1737
assert_trap(() => invoke($23, `test`, [0]), `uninitialized element`);

// ./test/core/table_init.wast:1738
assert_trap(() => invoke($23, `test`, [1]), `uninitialized element`);

// ./test/core/table_init.wast:1739
assert_trap(() => invoke($23, `test`, [2]), `uninitialized element`);

// ./test/core/table_init.wast:1740
assert_trap(() => invoke($23, `test`, [3]), `uninitialized element`);

// ./test/core/table_init.wast:1741
assert_trap(() => invoke($23, `test`, [4]), `uninitialized element`);

// ./test/core/table_init.wast:1742
assert_trap(() => invoke($23, `test`, [5]), `uninitialized element`);

// ./test/core/table_init.wast:1743
assert_trap(() => invoke($23, `test`, [6]), `uninitialized element`);

// ./test/core/table_init.wast:1744
assert_trap(() => invoke($23, `test`, [7]), `uninitialized element`);

// ./test/core/table_init.wast:1745
assert_trap(() => invoke($23, `test`, [8]), `uninitialized element`);

// ./test/core/table_init.wast:1746
assert_trap(() => invoke($23, `test`, [9]), `uninitialized element`);

// ./test/core/table_init.wast:1747
assert_trap(() => invoke($23, `test`, [10]), `uninitialized element`);

// ./test/core/table_init.wast:1748
assert_trap(() => invoke($23, `test`, [11]), `uninitialized element`);

// ./test/core/table_init.wast:1749
assert_trap(() => invoke($23, `test`, [12]), `uninitialized element`);

// ./test/core/table_init.wast:1750
assert_trap(() => invoke($23, `test`, [13]), `uninitialized element`);

// ./test/core/table_init.wast:1751
assert_trap(() => invoke($23, `test`, [14]), `uninitialized element`);

// ./test/core/table_init.wast:1752
assert_trap(() => invoke($23, `test`, [15]), `uninitialized element`);

// ./test/core/table_init.wast:1754
let $24 = instantiate(`(module
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
