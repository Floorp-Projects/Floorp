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

// ./test/core/table_copy.wast

// ./test/core/table_copy.wast:5
let $0 = instantiate(`(module
  (func (export "ef0") (result i32) (i32.const 0))
  (func (export "ef1") (result i32) (i32.const 1))
  (func (export "ef2") (result i32) (i32.const 2))
  (func (export "ef3") (result i32) (i32.const 3))
  (func (export "ef4") (result i32) (i32.const 4))
)`);

// ./test/core/table_copy.wast:12
register($0, `a`);

// ./test/core/table_copy.wast:14
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
    (nop))
  (func (export "check") (param i32) (result i32)
    (call_indirect (type 0) (local.get 0)))
)`);

// ./test/core/table_copy.wast:39
invoke($1, `test`, []);

// ./test/core/table_copy.wast:40
assert_trap(() => invoke($1, `check`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:41
assert_trap(() => invoke($1, `check`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:42
assert_return(() => invoke($1, `check`, [2]), [value("i32", 3)]);

// ./test/core/table_copy.wast:43
assert_return(() => invoke($1, `check`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:44
assert_return(() => invoke($1, `check`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:45
assert_return(() => invoke($1, `check`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:46
assert_trap(() => invoke($1, `check`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:47
assert_trap(() => invoke($1, `check`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:48
assert_trap(() => invoke($1, `check`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:49
assert_trap(() => invoke($1, `check`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:50
assert_trap(() => invoke($1, `check`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:51
assert_trap(() => invoke($1, `check`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:52
assert_return(() => invoke($1, `check`, [12]), [value("i32", 7)]);

// ./test/core/table_copy.wast:53
assert_return(() => invoke($1, `check`, [13]), [value("i32", 5)]);

// ./test/core/table_copy.wast:54
assert_return(() => invoke($1, `check`, [14]), [value("i32", 2)]);

// ./test/core/table_copy.wast:55
assert_return(() => invoke($1, `check`, [15]), [value("i32", 3)]);

// ./test/core/table_copy.wast:56
assert_return(() => invoke($1, `check`, [16]), [value("i32", 6)]);

// ./test/core/table_copy.wast:57
assert_trap(() => invoke($1, `check`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:58
assert_trap(() => invoke($1, `check`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:59
assert_trap(() => invoke($1, `check`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:60
assert_trap(() => invoke($1, `check`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:61
assert_trap(() => invoke($1, `check`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:62
assert_trap(() => invoke($1, `check`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:63
assert_trap(() => invoke($1, `check`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:64
assert_trap(() => invoke($1, `check`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:65
assert_trap(() => invoke($1, `check`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:66
assert_trap(() => invoke($1, `check`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:67
assert_trap(() => invoke($1, `check`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:68
assert_trap(() => invoke($1, `check`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:69
assert_trap(() => invoke($1, `check`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:71
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
    (table.copy (i32.const 13) (i32.const 2) (i32.const 3)))
  (func (export "check") (param i32) (result i32)
    (call_indirect (type 0) (local.get 0)))
)`);

// ./test/core/table_copy.wast:96
invoke($2, `test`, []);

// ./test/core/table_copy.wast:97
assert_trap(() => invoke($2, `check`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:98
assert_trap(() => invoke($2, `check`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:99
assert_return(() => invoke($2, `check`, [2]), [value("i32", 3)]);

// ./test/core/table_copy.wast:100
assert_return(() => invoke($2, `check`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:101
assert_return(() => invoke($2, `check`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:102
assert_return(() => invoke($2, `check`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:103
assert_trap(() => invoke($2, `check`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:104
assert_trap(() => invoke($2, `check`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:105
assert_trap(() => invoke($2, `check`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:106
assert_trap(() => invoke($2, `check`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:107
assert_trap(() => invoke($2, `check`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:108
assert_trap(() => invoke($2, `check`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:109
assert_return(() => invoke($2, `check`, [12]), [value("i32", 7)]);

// ./test/core/table_copy.wast:110
assert_return(() => invoke($2, `check`, [13]), [value("i32", 3)]);

// ./test/core/table_copy.wast:111
assert_return(() => invoke($2, `check`, [14]), [value("i32", 1)]);

// ./test/core/table_copy.wast:112
assert_return(() => invoke($2, `check`, [15]), [value("i32", 4)]);

// ./test/core/table_copy.wast:113
assert_return(() => invoke($2, `check`, [16]), [value("i32", 6)]);

// ./test/core/table_copy.wast:114
assert_trap(() => invoke($2, `check`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:115
assert_trap(() => invoke($2, `check`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:116
assert_trap(() => invoke($2, `check`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:117
assert_trap(() => invoke($2, `check`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:118
assert_trap(() => invoke($2, `check`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:119
assert_trap(() => invoke($2, `check`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:120
assert_trap(() => invoke($2, `check`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:121
assert_trap(() => invoke($2, `check`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:122
assert_trap(() => invoke($2, `check`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:123
assert_trap(() => invoke($2, `check`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:124
assert_trap(() => invoke($2, `check`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:125
assert_trap(() => invoke($2, `check`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:126
assert_trap(() => invoke($2, `check`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:128
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
    (table.copy (i32.const 25) (i32.const 15) (i32.const 2)))
  (func (export "check") (param i32) (result i32)
    (call_indirect (type 0) (local.get 0)))
)`);

// ./test/core/table_copy.wast:153
invoke($3, `test`, []);

// ./test/core/table_copy.wast:154
assert_trap(() => invoke($3, `check`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:155
assert_trap(() => invoke($3, `check`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:156
assert_return(() => invoke($3, `check`, [2]), [value("i32", 3)]);

// ./test/core/table_copy.wast:157
assert_return(() => invoke($3, `check`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:158
assert_return(() => invoke($3, `check`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:159
assert_return(() => invoke($3, `check`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:160
assert_trap(() => invoke($3, `check`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:161
assert_trap(() => invoke($3, `check`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:162
assert_trap(() => invoke($3, `check`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:163
assert_trap(() => invoke($3, `check`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:164
assert_trap(() => invoke($3, `check`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:165
assert_trap(() => invoke($3, `check`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:166
assert_return(() => invoke($3, `check`, [12]), [value("i32", 7)]);

// ./test/core/table_copy.wast:167
assert_return(() => invoke($3, `check`, [13]), [value("i32", 5)]);

// ./test/core/table_copy.wast:168
assert_return(() => invoke($3, `check`, [14]), [value("i32", 2)]);

// ./test/core/table_copy.wast:169
assert_return(() => invoke($3, `check`, [15]), [value("i32", 3)]);

// ./test/core/table_copy.wast:170
assert_return(() => invoke($3, `check`, [16]), [value("i32", 6)]);

// ./test/core/table_copy.wast:171
assert_trap(() => invoke($3, `check`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:172
assert_trap(() => invoke($3, `check`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:173
assert_trap(() => invoke($3, `check`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:174
assert_trap(() => invoke($3, `check`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:175
assert_trap(() => invoke($3, `check`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:176
assert_trap(() => invoke($3, `check`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:177
assert_trap(() => invoke($3, `check`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:178
assert_trap(() => invoke($3, `check`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:179
assert_return(() => invoke($3, `check`, [25]), [value("i32", 3)]);

// ./test/core/table_copy.wast:180
assert_return(() => invoke($3, `check`, [26]), [value("i32", 6)]);

// ./test/core/table_copy.wast:181
assert_trap(() => invoke($3, `check`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:182
assert_trap(() => invoke($3, `check`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:183
assert_trap(() => invoke($3, `check`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:185
let $4 = instantiate(`(module
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
    (table.copy (i32.const 13) (i32.const 25) (i32.const 3)))
  (func (export "check") (param i32) (result i32)
    (call_indirect (type 0) (local.get 0)))
)`);

// ./test/core/table_copy.wast:210
invoke($4, `test`, []);

// ./test/core/table_copy.wast:211
assert_trap(() => invoke($4, `check`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:212
assert_trap(() => invoke($4, `check`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:213
assert_return(() => invoke($4, `check`, [2]), [value("i32", 3)]);

// ./test/core/table_copy.wast:214
assert_return(() => invoke($4, `check`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:215
assert_return(() => invoke($4, `check`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:216
assert_return(() => invoke($4, `check`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:217
assert_trap(() => invoke($4, `check`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:218
assert_trap(() => invoke($4, `check`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:219
assert_trap(() => invoke($4, `check`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:220
assert_trap(() => invoke($4, `check`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:221
assert_trap(() => invoke($4, `check`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:222
assert_trap(() => invoke($4, `check`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:223
assert_return(() => invoke($4, `check`, [12]), [value("i32", 7)]);

// ./test/core/table_copy.wast:224
assert_trap(() => invoke($4, `check`, [13]), `uninitialized element`);

// ./test/core/table_copy.wast:225
assert_trap(() => invoke($4, `check`, [14]), `uninitialized element`);

// ./test/core/table_copy.wast:226
assert_trap(() => invoke($4, `check`, [15]), `uninitialized element`);

// ./test/core/table_copy.wast:227
assert_return(() => invoke($4, `check`, [16]), [value("i32", 6)]);

// ./test/core/table_copy.wast:228
assert_trap(() => invoke($4, `check`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:229
assert_trap(() => invoke($4, `check`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:230
assert_trap(() => invoke($4, `check`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:231
assert_trap(() => invoke($4, `check`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:232
assert_trap(() => invoke($4, `check`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:233
assert_trap(() => invoke($4, `check`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:234
assert_trap(() => invoke($4, `check`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:235
assert_trap(() => invoke($4, `check`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:236
assert_trap(() => invoke($4, `check`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:237
assert_trap(() => invoke($4, `check`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:238
assert_trap(() => invoke($4, `check`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:239
assert_trap(() => invoke($4, `check`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:240
assert_trap(() => invoke($4, `check`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:242
let $5 = instantiate(`(module
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
    (table.copy (i32.const 20) (i32.const 22) (i32.const 4)))
  (func (export "check") (param i32) (result i32)
    (call_indirect (type 0) (local.get 0)))
)`);

// ./test/core/table_copy.wast:267
invoke($5, `test`, []);

// ./test/core/table_copy.wast:268
assert_trap(() => invoke($5, `check`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:269
assert_trap(() => invoke($5, `check`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:270
assert_return(() => invoke($5, `check`, [2]), [value("i32", 3)]);

// ./test/core/table_copy.wast:271
assert_return(() => invoke($5, `check`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:272
assert_return(() => invoke($5, `check`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:273
assert_return(() => invoke($5, `check`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:274
assert_trap(() => invoke($5, `check`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:275
assert_trap(() => invoke($5, `check`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:276
assert_trap(() => invoke($5, `check`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:277
assert_trap(() => invoke($5, `check`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:278
assert_trap(() => invoke($5, `check`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:279
assert_trap(() => invoke($5, `check`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:280
assert_return(() => invoke($5, `check`, [12]), [value("i32", 7)]);

// ./test/core/table_copy.wast:281
assert_return(() => invoke($5, `check`, [13]), [value("i32", 5)]);

// ./test/core/table_copy.wast:282
assert_return(() => invoke($5, `check`, [14]), [value("i32", 2)]);

// ./test/core/table_copy.wast:283
assert_return(() => invoke($5, `check`, [15]), [value("i32", 3)]);

// ./test/core/table_copy.wast:284
assert_return(() => invoke($5, `check`, [16]), [value("i32", 6)]);

// ./test/core/table_copy.wast:285
assert_trap(() => invoke($5, `check`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:286
assert_trap(() => invoke($5, `check`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:287
assert_trap(() => invoke($5, `check`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:288
assert_trap(() => invoke($5, `check`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:289
assert_trap(() => invoke($5, `check`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:290
assert_trap(() => invoke($5, `check`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:291
assert_trap(() => invoke($5, `check`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:292
assert_trap(() => invoke($5, `check`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:293
assert_trap(() => invoke($5, `check`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:294
assert_trap(() => invoke($5, `check`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:295
assert_trap(() => invoke($5, `check`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:296
assert_trap(() => invoke($5, `check`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:297
assert_trap(() => invoke($5, `check`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:299
let $6 = instantiate(`(module
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
    (table.copy (i32.const 25) (i32.const 1) (i32.const 3)))
  (func (export "check") (param i32) (result i32)
    (call_indirect (type 0) (local.get 0)))
)`);

// ./test/core/table_copy.wast:324
invoke($6, `test`, []);

// ./test/core/table_copy.wast:325
assert_trap(() => invoke($6, `check`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:326
assert_trap(() => invoke($6, `check`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:327
assert_return(() => invoke($6, `check`, [2]), [value("i32", 3)]);

// ./test/core/table_copy.wast:328
assert_return(() => invoke($6, `check`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:329
assert_return(() => invoke($6, `check`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:330
assert_return(() => invoke($6, `check`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:331
assert_trap(() => invoke($6, `check`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:332
assert_trap(() => invoke($6, `check`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:333
assert_trap(() => invoke($6, `check`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:334
assert_trap(() => invoke($6, `check`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:335
assert_trap(() => invoke($6, `check`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:336
assert_trap(() => invoke($6, `check`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:337
assert_return(() => invoke($6, `check`, [12]), [value("i32", 7)]);

// ./test/core/table_copy.wast:338
assert_return(() => invoke($6, `check`, [13]), [value("i32", 5)]);

// ./test/core/table_copy.wast:339
assert_return(() => invoke($6, `check`, [14]), [value("i32", 2)]);

// ./test/core/table_copy.wast:340
assert_return(() => invoke($6, `check`, [15]), [value("i32", 3)]);

// ./test/core/table_copy.wast:341
assert_return(() => invoke($6, `check`, [16]), [value("i32", 6)]);

// ./test/core/table_copy.wast:342
assert_trap(() => invoke($6, `check`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:343
assert_trap(() => invoke($6, `check`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:344
assert_trap(() => invoke($6, `check`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:345
assert_trap(() => invoke($6, `check`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:346
assert_trap(() => invoke($6, `check`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:347
assert_trap(() => invoke($6, `check`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:348
assert_trap(() => invoke($6, `check`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:349
assert_trap(() => invoke($6, `check`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:350
assert_trap(() => invoke($6, `check`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:351
assert_return(() => invoke($6, `check`, [26]), [value("i32", 3)]);

// ./test/core/table_copy.wast:352
assert_return(() => invoke($6, `check`, [27]), [value("i32", 1)]);

// ./test/core/table_copy.wast:353
assert_trap(() => invoke($6, `check`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:354
assert_trap(() => invoke($6, `check`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:356
let $7 = instantiate(`(module
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
    (table.copy (i32.const 10) (i32.const 12) (i32.const 7)))
  (func (export "check") (param i32) (result i32)
    (call_indirect (type 0) (local.get 0)))
)`);

// ./test/core/table_copy.wast:381
invoke($7, `test`, []);

// ./test/core/table_copy.wast:382
assert_trap(() => invoke($7, `check`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:383
assert_trap(() => invoke($7, `check`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:384
assert_return(() => invoke($7, `check`, [2]), [value("i32", 3)]);

// ./test/core/table_copy.wast:385
assert_return(() => invoke($7, `check`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:386
assert_return(() => invoke($7, `check`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:387
assert_return(() => invoke($7, `check`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:388
assert_trap(() => invoke($7, `check`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:389
assert_trap(() => invoke($7, `check`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:390
assert_trap(() => invoke($7, `check`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:391
assert_trap(() => invoke($7, `check`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:392
assert_return(() => invoke($7, `check`, [10]), [value("i32", 7)]);

// ./test/core/table_copy.wast:393
assert_return(() => invoke($7, `check`, [11]), [value("i32", 5)]);

// ./test/core/table_copy.wast:394
assert_return(() => invoke($7, `check`, [12]), [value("i32", 2)]);

// ./test/core/table_copy.wast:395
assert_return(() => invoke($7, `check`, [13]), [value("i32", 3)]);

// ./test/core/table_copy.wast:396
assert_return(() => invoke($7, `check`, [14]), [value("i32", 6)]);

// ./test/core/table_copy.wast:397
assert_trap(() => invoke($7, `check`, [15]), `uninitialized element`);

// ./test/core/table_copy.wast:398
assert_trap(() => invoke($7, `check`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:399
assert_trap(() => invoke($7, `check`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:400
assert_trap(() => invoke($7, `check`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:401
assert_trap(() => invoke($7, `check`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:402
assert_trap(() => invoke($7, `check`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:403
assert_trap(() => invoke($7, `check`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:404
assert_trap(() => invoke($7, `check`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:405
assert_trap(() => invoke($7, `check`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:406
assert_trap(() => invoke($7, `check`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:407
assert_trap(() => invoke($7, `check`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:408
assert_trap(() => invoke($7, `check`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:409
assert_trap(() => invoke($7, `check`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:410
assert_trap(() => invoke($7, `check`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:411
assert_trap(() => invoke($7, `check`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:413
let $8 = instantiate(`(module
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
    (table.copy (i32.const 12) (i32.const 10) (i32.const 7)))
  (func (export "check") (param i32) (result i32)
    (call_indirect (type 0) (local.get 0)))
)`);

// ./test/core/table_copy.wast:438
invoke($8, `test`, []);

// ./test/core/table_copy.wast:439
assert_trap(() => invoke($8, `check`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:440
assert_trap(() => invoke($8, `check`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:441
assert_return(() => invoke($8, `check`, [2]), [value("i32", 3)]);

// ./test/core/table_copy.wast:442
assert_return(() => invoke($8, `check`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:443
assert_return(() => invoke($8, `check`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:444
assert_return(() => invoke($8, `check`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:445
assert_trap(() => invoke($8, `check`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:446
assert_trap(() => invoke($8, `check`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:447
assert_trap(() => invoke($8, `check`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:448
assert_trap(() => invoke($8, `check`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:449
assert_trap(() => invoke($8, `check`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:450
assert_trap(() => invoke($8, `check`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:451
assert_trap(() => invoke($8, `check`, [12]), `uninitialized element`);

// ./test/core/table_copy.wast:452
assert_trap(() => invoke($8, `check`, [13]), `uninitialized element`);

// ./test/core/table_copy.wast:453
assert_return(() => invoke($8, `check`, [14]), [value("i32", 7)]);

// ./test/core/table_copy.wast:454
assert_return(() => invoke($8, `check`, [15]), [value("i32", 5)]);

// ./test/core/table_copy.wast:455
assert_return(() => invoke($8, `check`, [16]), [value("i32", 2)]);

// ./test/core/table_copy.wast:456
assert_return(() => invoke($8, `check`, [17]), [value("i32", 3)]);

// ./test/core/table_copy.wast:457
assert_return(() => invoke($8, `check`, [18]), [value("i32", 6)]);

// ./test/core/table_copy.wast:458
assert_trap(() => invoke($8, `check`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:459
assert_trap(() => invoke($8, `check`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:460
assert_trap(() => invoke($8, `check`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:461
assert_trap(() => invoke($8, `check`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:462
assert_trap(() => invoke($8, `check`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:463
assert_trap(() => invoke($8, `check`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:464
assert_trap(() => invoke($8, `check`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:465
assert_trap(() => invoke($8, `check`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:466
assert_trap(() => invoke($8, `check`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:467
assert_trap(() => invoke($8, `check`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:468
assert_trap(() => invoke($8, `check`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:470
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
    (table.copy (i32.const 28) (i32.const 1) (i32.const 3))
    ))`);

// ./test/core/table_copy.wast:492
assert_trap(() => invoke($9, `test`, []), `out of bounds`);

// ./test/core/table_copy.wast:494
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
    (table.copy (i32.const 0xFFFFFFFE) (i32.const 1) (i32.const 2))
    ))`);

// ./test/core/table_copy.wast:516
assert_trap(() => invoke($10, `test`, []), `out of bounds`);

// ./test/core/table_copy.wast:518
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
    (table.copy (i32.const 15) (i32.const 25) (i32.const 6))
    ))`);

// ./test/core/table_copy.wast:540
assert_trap(() => invoke($11, `test`, []), `out of bounds`);

// ./test/core/table_copy.wast:542
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
    (table.copy (i32.const 15) (i32.const 0xFFFFFFFE) (i32.const 2))
    ))`);

// ./test/core/table_copy.wast:564
assert_trap(() => invoke($12, `test`, []), `out of bounds`);

// ./test/core/table_copy.wast:566
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
    (table.copy (i32.const 15) (i32.const 25) (i32.const 0))
    ))`);

// ./test/core/table_copy.wast:588
invoke($13, `test`, []);

// ./test/core/table_copy.wast:590
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
    (table.copy (i32.const 30) (i32.const 15) (i32.const 0))
    ))`);

// ./test/core/table_copy.wast:612
invoke($14, `test`, []);

// ./test/core/table_copy.wast:614
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
    (table.copy (i32.const 31) (i32.const 15) (i32.const 0))
    ))`);

// ./test/core/table_copy.wast:636
assert_trap(() => invoke($15, `test`, []), `out of bounds`);

// ./test/core/table_copy.wast:638
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
    (table.copy (i32.const 15) (i32.const 30) (i32.const 0))
    ))`);

// ./test/core/table_copy.wast:660
invoke($16, `test`, []);

// ./test/core/table_copy.wast:662
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
    (table.copy (i32.const 15) (i32.const 31) (i32.const 0))
    ))`);

// ./test/core/table_copy.wast:684
assert_trap(() => invoke($17, `test`, []), `out of bounds`);

// ./test/core/table_copy.wast:686
let $18 = instantiate(`(module
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
    (table.copy (i32.const 30) (i32.const 30) (i32.const 0))
    ))`);

// ./test/core/table_copy.wast:708
invoke($18, `test`, []);

// ./test/core/table_copy.wast:710
let $19 = instantiate(`(module
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
    (table.copy (i32.const 31) (i32.const 31) (i32.const 0))
    ))`);

// ./test/core/table_copy.wast:732
assert_trap(() => invoke($19, `test`, []), `out of bounds`);

// ./test/core/table_copy.wast:734
let $20 = instantiate(`(module
  (type (func (result i32)))
  (table 32 64 funcref)
  (elem (i32.const 0)
         $$f0 $$f1 $$f2 $$f3 $$f4 $$f5 $$f6 $$f7)
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
  (func (export "run") (param $$targetOffs i32) (param $$srcOffs i32) (param $$len i32)
    (table.copy (local.get $$targetOffs) (local.get $$srcOffs) (local.get $$len))))`);

// ./test/core/table_copy.wast:760
assert_trap(() => invoke($20, `run`, [24, 0, 16]), `out of bounds`);

// ./test/core/table_copy.wast:762
assert_return(() => invoke($20, `test`, [0]), [value("i32", 0)]);

// ./test/core/table_copy.wast:763
assert_return(() => invoke($20, `test`, [1]), [value("i32", 1)]);

// ./test/core/table_copy.wast:764
assert_return(() => invoke($20, `test`, [2]), [value("i32", 2)]);

// ./test/core/table_copy.wast:765
assert_return(() => invoke($20, `test`, [3]), [value("i32", 3)]);

// ./test/core/table_copy.wast:766
assert_return(() => invoke($20, `test`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:767
assert_return(() => invoke($20, `test`, [5]), [value("i32", 5)]);

// ./test/core/table_copy.wast:768
assert_return(() => invoke($20, `test`, [6]), [value("i32", 6)]);

// ./test/core/table_copy.wast:769
assert_return(() => invoke($20, `test`, [7]), [value("i32", 7)]);

// ./test/core/table_copy.wast:770
assert_trap(() => invoke($20, `test`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:771
assert_trap(() => invoke($20, `test`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:772
assert_trap(() => invoke($20, `test`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:773
assert_trap(() => invoke($20, `test`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:774
assert_trap(() => invoke($20, `test`, [12]), `uninitialized element`);

// ./test/core/table_copy.wast:775
assert_trap(() => invoke($20, `test`, [13]), `uninitialized element`);

// ./test/core/table_copy.wast:776
assert_trap(() => invoke($20, `test`, [14]), `uninitialized element`);

// ./test/core/table_copy.wast:777
assert_trap(() => invoke($20, `test`, [15]), `uninitialized element`);

// ./test/core/table_copy.wast:778
assert_trap(() => invoke($20, `test`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:779
assert_trap(() => invoke($20, `test`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:780
assert_trap(() => invoke($20, `test`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:781
assert_trap(() => invoke($20, `test`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:782
assert_trap(() => invoke($20, `test`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:783
assert_trap(() => invoke($20, `test`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:784
assert_trap(() => invoke($20, `test`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:785
assert_trap(() => invoke($20, `test`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:786
assert_trap(() => invoke($20, `test`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:787
assert_trap(() => invoke($20, `test`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:788
assert_trap(() => invoke($20, `test`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:789
assert_trap(() => invoke($20, `test`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:790
assert_trap(() => invoke($20, `test`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:791
assert_trap(() => invoke($20, `test`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:792
assert_trap(() => invoke($20, `test`, [30]), `uninitialized element`);

// ./test/core/table_copy.wast:793
assert_trap(() => invoke($20, `test`, [31]), `uninitialized element`);

// ./test/core/table_copy.wast:795
let $21 = instantiate(`(module
  (type (func (result i32)))
  (table 32 64 funcref)
  (elem (i32.const 0)
         $$f0 $$f1 $$f2 $$f3 $$f4 $$f5 $$f6 $$f7 $$f8)
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
  (func (export "run") (param $$targetOffs i32) (param $$srcOffs i32) (param $$len i32)
    (table.copy (local.get $$targetOffs) (local.get $$srcOffs) (local.get $$len))))`);

// ./test/core/table_copy.wast:821
assert_trap(() => invoke($21, `run`, [23, 0, 15]), `out of bounds`);

// ./test/core/table_copy.wast:823
assert_return(() => invoke($21, `test`, [0]), [value("i32", 0)]);

// ./test/core/table_copy.wast:824
assert_return(() => invoke($21, `test`, [1]), [value("i32", 1)]);

// ./test/core/table_copy.wast:825
assert_return(() => invoke($21, `test`, [2]), [value("i32", 2)]);

// ./test/core/table_copy.wast:826
assert_return(() => invoke($21, `test`, [3]), [value("i32", 3)]);

// ./test/core/table_copy.wast:827
assert_return(() => invoke($21, `test`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:828
assert_return(() => invoke($21, `test`, [5]), [value("i32", 5)]);

// ./test/core/table_copy.wast:829
assert_return(() => invoke($21, `test`, [6]), [value("i32", 6)]);

// ./test/core/table_copy.wast:830
assert_return(() => invoke($21, `test`, [7]), [value("i32", 7)]);

// ./test/core/table_copy.wast:831
assert_return(() => invoke($21, `test`, [8]), [value("i32", 8)]);

// ./test/core/table_copy.wast:832
assert_trap(() => invoke($21, `test`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:833
assert_trap(() => invoke($21, `test`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:834
assert_trap(() => invoke($21, `test`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:835
assert_trap(() => invoke($21, `test`, [12]), `uninitialized element`);

// ./test/core/table_copy.wast:836
assert_trap(() => invoke($21, `test`, [13]), `uninitialized element`);

// ./test/core/table_copy.wast:837
assert_trap(() => invoke($21, `test`, [14]), `uninitialized element`);

// ./test/core/table_copy.wast:838
assert_trap(() => invoke($21, `test`, [15]), `uninitialized element`);

// ./test/core/table_copy.wast:839
assert_trap(() => invoke($21, `test`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:840
assert_trap(() => invoke($21, `test`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:841
assert_trap(() => invoke($21, `test`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:842
assert_trap(() => invoke($21, `test`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:843
assert_trap(() => invoke($21, `test`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:844
assert_trap(() => invoke($21, `test`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:845
assert_trap(() => invoke($21, `test`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:846
assert_trap(() => invoke($21, `test`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:847
assert_trap(() => invoke($21, `test`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:848
assert_trap(() => invoke($21, `test`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:849
assert_trap(() => invoke($21, `test`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:850
assert_trap(() => invoke($21, `test`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:851
assert_trap(() => invoke($21, `test`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:852
assert_trap(() => invoke($21, `test`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:853
assert_trap(() => invoke($21, `test`, [30]), `uninitialized element`);

// ./test/core/table_copy.wast:854
assert_trap(() => invoke($21, `test`, [31]), `uninitialized element`);

// ./test/core/table_copy.wast:856
let $22 = instantiate(`(module
  (type (func (result i32)))
  (table 32 64 funcref)
  (elem (i32.const 24)
         $$f0 $$f1 $$f2 $$f3 $$f4 $$f5 $$f6 $$f7)
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
  (func (export "run") (param $$targetOffs i32) (param $$srcOffs i32) (param $$len i32)
    (table.copy (local.get $$targetOffs) (local.get $$srcOffs) (local.get $$len))))`);

// ./test/core/table_copy.wast:882
assert_trap(() => invoke($22, `run`, [0, 24, 16]), `out of bounds`);

// ./test/core/table_copy.wast:884
assert_trap(() => invoke($22, `test`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:885
assert_trap(() => invoke($22, `test`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:886
assert_trap(() => invoke($22, `test`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:887
assert_trap(() => invoke($22, `test`, [3]), `uninitialized element`);

// ./test/core/table_copy.wast:888
assert_trap(() => invoke($22, `test`, [4]), `uninitialized element`);

// ./test/core/table_copy.wast:889
assert_trap(() => invoke($22, `test`, [5]), `uninitialized element`);

// ./test/core/table_copy.wast:890
assert_trap(() => invoke($22, `test`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:891
assert_trap(() => invoke($22, `test`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:892
assert_trap(() => invoke($22, `test`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:893
assert_trap(() => invoke($22, `test`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:894
assert_trap(() => invoke($22, `test`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:895
assert_trap(() => invoke($22, `test`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:896
assert_trap(() => invoke($22, `test`, [12]), `uninitialized element`);

// ./test/core/table_copy.wast:897
assert_trap(() => invoke($22, `test`, [13]), `uninitialized element`);

// ./test/core/table_copy.wast:898
assert_trap(() => invoke($22, `test`, [14]), `uninitialized element`);

// ./test/core/table_copy.wast:899
assert_trap(() => invoke($22, `test`, [15]), `uninitialized element`);

// ./test/core/table_copy.wast:900
assert_trap(() => invoke($22, `test`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:901
assert_trap(() => invoke($22, `test`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:902
assert_trap(() => invoke($22, `test`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:903
assert_trap(() => invoke($22, `test`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:904
assert_trap(() => invoke($22, `test`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:905
assert_trap(() => invoke($22, `test`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:906
assert_trap(() => invoke($22, `test`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:907
assert_trap(() => invoke($22, `test`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:908
assert_return(() => invoke($22, `test`, [24]), [value("i32", 0)]);

// ./test/core/table_copy.wast:909
assert_return(() => invoke($22, `test`, [25]), [value("i32", 1)]);

// ./test/core/table_copy.wast:910
assert_return(() => invoke($22, `test`, [26]), [value("i32", 2)]);

// ./test/core/table_copy.wast:911
assert_return(() => invoke($22, `test`, [27]), [value("i32", 3)]);

// ./test/core/table_copy.wast:912
assert_return(() => invoke($22, `test`, [28]), [value("i32", 4)]);

// ./test/core/table_copy.wast:913
assert_return(() => invoke($22, `test`, [29]), [value("i32", 5)]);

// ./test/core/table_copy.wast:914
assert_return(() => invoke($22, `test`, [30]), [value("i32", 6)]);

// ./test/core/table_copy.wast:915
assert_return(() => invoke($22, `test`, [31]), [value("i32", 7)]);

// ./test/core/table_copy.wast:917
let $23 = instantiate(`(module
  (type (func (result i32)))
  (table 32 64 funcref)
  (elem (i32.const 23)
         $$f0 $$f1 $$f2 $$f3 $$f4 $$f5 $$f6 $$f7 $$f8)
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
  (func (export "run") (param $$targetOffs i32) (param $$srcOffs i32) (param $$len i32)
    (table.copy (local.get $$targetOffs) (local.get $$srcOffs) (local.get $$len))))`);

// ./test/core/table_copy.wast:943
assert_trap(() => invoke($23, `run`, [0, 23, 15]), `out of bounds`);

// ./test/core/table_copy.wast:945
assert_trap(() => invoke($23, `test`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:946
assert_trap(() => invoke($23, `test`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:947
assert_trap(() => invoke($23, `test`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:948
assert_trap(() => invoke($23, `test`, [3]), `uninitialized element`);

// ./test/core/table_copy.wast:949
assert_trap(() => invoke($23, `test`, [4]), `uninitialized element`);

// ./test/core/table_copy.wast:950
assert_trap(() => invoke($23, `test`, [5]), `uninitialized element`);

// ./test/core/table_copy.wast:951
assert_trap(() => invoke($23, `test`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:952
assert_trap(() => invoke($23, `test`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:953
assert_trap(() => invoke($23, `test`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:954
assert_trap(() => invoke($23, `test`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:955
assert_trap(() => invoke($23, `test`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:956
assert_trap(() => invoke($23, `test`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:957
assert_trap(() => invoke($23, `test`, [12]), `uninitialized element`);

// ./test/core/table_copy.wast:958
assert_trap(() => invoke($23, `test`, [13]), `uninitialized element`);

// ./test/core/table_copy.wast:959
assert_trap(() => invoke($23, `test`, [14]), `uninitialized element`);

// ./test/core/table_copy.wast:960
assert_trap(() => invoke($23, `test`, [15]), `uninitialized element`);

// ./test/core/table_copy.wast:961
assert_trap(() => invoke($23, `test`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:962
assert_trap(() => invoke($23, `test`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:963
assert_trap(() => invoke($23, `test`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:964
assert_trap(() => invoke($23, `test`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:965
assert_trap(() => invoke($23, `test`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:966
assert_trap(() => invoke($23, `test`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:967
assert_trap(() => invoke($23, `test`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:968
assert_return(() => invoke($23, `test`, [23]), [value("i32", 0)]);

// ./test/core/table_copy.wast:969
assert_return(() => invoke($23, `test`, [24]), [value("i32", 1)]);

// ./test/core/table_copy.wast:970
assert_return(() => invoke($23, `test`, [25]), [value("i32", 2)]);

// ./test/core/table_copy.wast:971
assert_return(() => invoke($23, `test`, [26]), [value("i32", 3)]);

// ./test/core/table_copy.wast:972
assert_return(() => invoke($23, `test`, [27]), [value("i32", 4)]);

// ./test/core/table_copy.wast:973
assert_return(() => invoke($23, `test`, [28]), [value("i32", 5)]);

// ./test/core/table_copy.wast:974
assert_return(() => invoke($23, `test`, [29]), [value("i32", 6)]);

// ./test/core/table_copy.wast:975
assert_return(() => invoke($23, `test`, [30]), [value("i32", 7)]);

// ./test/core/table_copy.wast:976
assert_return(() => invoke($23, `test`, [31]), [value("i32", 8)]);

// ./test/core/table_copy.wast:978
let $24 = instantiate(`(module
  (type (func (result i32)))
  (table 32 64 funcref)
  (elem (i32.const 11)
         $$f0 $$f1 $$f2 $$f3 $$f4 $$f5 $$f6 $$f7)
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
  (func (export "run") (param $$targetOffs i32) (param $$srcOffs i32) (param $$len i32)
    (table.copy (local.get $$targetOffs) (local.get $$srcOffs) (local.get $$len))))`);

// ./test/core/table_copy.wast:1004
assert_trap(() => invoke($24, `run`, [24, 11, 16]), `out of bounds`);

// ./test/core/table_copy.wast:1006
assert_trap(() => invoke($24, `test`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:1007
assert_trap(() => invoke($24, `test`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:1008
assert_trap(() => invoke($24, `test`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:1009
assert_trap(() => invoke($24, `test`, [3]), `uninitialized element`);

// ./test/core/table_copy.wast:1010
assert_trap(() => invoke($24, `test`, [4]), `uninitialized element`);

// ./test/core/table_copy.wast:1011
assert_trap(() => invoke($24, `test`, [5]), `uninitialized element`);

// ./test/core/table_copy.wast:1012
assert_trap(() => invoke($24, `test`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:1013
assert_trap(() => invoke($24, `test`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:1014
assert_trap(() => invoke($24, `test`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:1015
assert_trap(() => invoke($24, `test`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:1016
assert_trap(() => invoke($24, `test`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:1017
assert_return(() => invoke($24, `test`, [11]), [value("i32", 0)]);

// ./test/core/table_copy.wast:1018
assert_return(() => invoke($24, `test`, [12]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1019
assert_return(() => invoke($24, `test`, [13]), [value("i32", 2)]);

// ./test/core/table_copy.wast:1020
assert_return(() => invoke($24, `test`, [14]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1021
assert_return(() => invoke($24, `test`, [15]), [value("i32", 4)]);

// ./test/core/table_copy.wast:1022
assert_return(() => invoke($24, `test`, [16]), [value("i32", 5)]);

// ./test/core/table_copy.wast:1023
assert_return(() => invoke($24, `test`, [17]), [value("i32", 6)]);

// ./test/core/table_copy.wast:1024
assert_return(() => invoke($24, `test`, [18]), [value("i32", 7)]);

// ./test/core/table_copy.wast:1025
assert_trap(() => invoke($24, `test`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:1026
assert_trap(() => invoke($24, `test`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:1027
assert_trap(() => invoke($24, `test`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:1028
assert_trap(() => invoke($24, `test`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:1029
assert_trap(() => invoke($24, `test`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:1030
assert_trap(() => invoke($24, `test`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:1031
assert_trap(() => invoke($24, `test`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:1032
assert_trap(() => invoke($24, `test`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:1033
assert_trap(() => invoke($24, `test`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:1034
assert_trap(() => invoke($24, `test`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:1035
assert_trap(() => invoke($24, `test`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:1036
assert_trap(() => invoke($24, `test`, [30]), `uninitialized element`);

// ./test/core/table_copy.wast:1037
assert_trap(() => invoke($24, `test`, [31]), `uninitialized element`);

// ./test/core/table_copy.wast:1039
let $25 = instantiate(`(module
  (type (func (result i32)))
  (table 32 64 funcref)
  (elem (i32.const 24)
         $$f0 $$f1 $$f2 $$f3 $$f4 $$f5 $$f6 $$f7)
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
  (func (export "run") (param $$targetOffs i32) (param $$srcOffs i32) (param $$len i32)
    (table.copy (local.get $$targetOffs) (local.get $$srcOffs) (local.get $$len))))`);

// ./test/core/table_copy.wast:1065
assert_trap(() => invoke($25, `run`, [11, 24, 16]), `out of bounds`);

// ./test/core/table_copy.wast:1067
assert_trap(() => invoke($25, `test`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:1068
assert_trap(() => invoke($25, `test`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:1069
assert_trap(() => invoke($25, `test`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:1070
assert_trap(() => invoke($25, `test`, [3]), `uninitialized element`);

// ./test/core/table_copy.wast:1071
assert_trap(() => invoke($25, `test`, [4]), `uninitialized element`);

// ./test/core/table_copy.wast:1072
assert_trap(() => invoke($25, `test`, [5]), `uninitialized element`);

// ./test/core/table_copy.wast:1073
assert_trap(() => invoke($25, `test`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:1074
assert_trap(() => invoke($25, `test`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:1075
assert_trap(() => invoke($25, `test`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:1076
assert_trap(() => invoke($25, `test`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:1077
assert_trap(() => invoke($25, `test`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:1078
assert_trap(() => invoke($25, `test`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:1079
assert_trap(() => invoke($25, `test`, [12]), `uninitialized element`);

// ./test/core/table_copy.wast:1080
assert_trap(() => invoke($25, `test`, [13]), `uninitialized element`);

// ./test/core/table_copy.wast:1081
assert_trap(() => invoke($25, `test`, [14]), `uninitialized element`);

// ./test/core/table_copy.wast:1082
assert_trap(() => invoke($25, `test`, [15]), `uninitialized element`);

// ./test/core/table_copy.wast:1083
assert_trap(() => invoke($25, `test`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:1084
assert_trap(() => invoke($25, `test`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:1085
assert_trap(() => invoke($25, `test`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:1086
assert_trap(() => invoke($25, `test`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:1087
assert_trap(() => invoke($25, `test`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:1088
assert_trap(() => invoke($25, `test`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:1089
assert_trap(() => invoke($25, `test`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:1090
assert_trap(() => invoke($25, `test`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:1091
assert_return(() => invoke($25, `test`, [24]), [value("i32", 0)]);

// ./test/core/table_copy.wast:1092
assert_return(() => invoke($25, `test`, [25]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1093
assert_return(() => invoke($25, `test`, [26]), [value("i32", 2)]);

// ./test/core/table_copy.wast:1094
assert_return(() => invoke($25, `test`, [27]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1095
assert_return(() => invoke($25, `test`, [28]), [value("i32", 4)]);

// ./test/core/table_copy.wast:1096
assert_return(() => invoke($25, `test`, [29]), [value("i32", 5)]);

// ./test/core/table_copy.wast:1097
assert_return(() => invoke($25, `test`, [30]), [value("i32", 6)]);

// ./test/core/table_copy.wast:1098
assert_return(() => invoke($25, `test`, [31]), [value("i32", 7)]);

// ./test/core/table_copy.wast:1100
let $26 = instantiate(`(module
  (type (func (result i32)))
  (table 32 64 funcref)
  (elem (i32.const 21)
         $$f0 $$f1 $$f2 $$f3 $$f4 $$f5 $$f6 $$f7)
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
  (func (export "run") (param $$targetOffs i32) (param $$srcOffs i32) (param $$len i32)
    (table.copy (local.get $$targetOffs) (local.get $$srcOffs) (local.get $$len))))`);

// ./test/core/table_copy.wast:1126
assert_trap(() => invoke($26, `run`, [24, 21, 16]), `out of bounds`);

// ./test/core/table_copy.wast:1128
assert_trap(() => invoke($26, `test`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:1129
assert_trap(() => invoke($26, `test`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:1130
assert_trap(() => invoke($26, `test`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:1131
assert_trap(() => invoke($26, `test`, [3]), `uninitialized element`);

// ./test/core/table_copy.wast:1132
assert_trap(() => invoke($26, `test`, [4]), `uninitialized element`);

// ./test/core/table_copy.wast:1133
assert_trap(() => invoke($26, `test`, [5]), `uninitialized element`);

// ./test/core/table_copy.wast:1134
assert_trap(() => invoke($26, `test`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:1135
assert_trap(() => invoke($26, `test`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:1136
assert_trap(() => invoke($26, `test`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:1137
assert_trap(() => invoke($26, `test`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:1138
assert_trap(() => invoke($26, `test`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:1139
assert_trap(() => invoke($26, `test`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:1140
assert_trap(() => invoke($26, `test`, [12]), `uninitialized element`);

// ./test/core/table_copy.wast:1141
assert_trap(() => invoke($26, `test`, [13]), `uninitialized element`);

// ./test/core/table_copy.wast:1142
assert_trap(() => invoke($26, `test`, [14]), `uninitialized element`);

// ./test/core/table_copy.wast:1143
assert_trap(() => invoke($26, `test`, [15]), `uninitialized element`);

// ./test/core/table_copy.wast:1144
assert_trap(() => invoke($26, `test`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:1145
assert_trap(() => invoke($26, `test`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:1146
assert_trap(() => invoke($26, `test`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:1147
assert_trap(() => invoke($26, `test`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:1148
assert_trap(() => invoke($26, `test`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:1149
assert_return(() => invoke($26, `test`, [21]), [value("i32", 0)]);

// ./test/core/table_copy.wast:1150
assert_return(() => invoke($26, `test`, [22]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1151
assert_return(() => invoke($26, `test`, [23]), [value("i32", 2)]);

// ./test/core/table_copy.wast:1152
assert_return(() => invoke($26, `test`, [24]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1153
assert_return(() => invoke($26, `test`, [25]), [value("i32", 4)]);

// ./test/core/table_copy.wast:1154
assert_return(() => invoke($26, `test`, [26]), [value("i32", 5)]);

// ./test/core/table_copy.wast:1155
assert_return(() => invoke($26, `test`, [27]), [value("i32", 6)]);

// ./test/core/table_copy.wast:1156
assert_return(() => invoke($26, `test`, [28]), [value("i32", 7)]);

// ./test/core/table_copy.wast:1157
assert_trap(() => invoke($26, `test`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:1158
assert_trap(() => invoke($26, `test`, [30]), `uninitialized element`);

// ./test/core/table_copy.wast:1159
assert_trap(() => invoke($26, `test`, [31]), `uninitialized element`);

// ./test/core/table_copy.wast:1161
let $27 = instantiate(`(module
  (type (func (result i32)))
  (table 32 64 funcref)
  (elem (i32.const 24)
         $$f0 $$f1 $$f2 $$f3 $$f4 $$f5 $$f6 $$f7)
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
  (func (export "run") (param $$targetOffs i32) (param $$srcOffs i32) (param $$len i32)
    (table.copy (local.get $$targetOffs) (local.get $$srcOffs) (local.get $$len))))`);

// ./test/core/table_copy.wast:1187
assert_trap(() => invoke($27, `run`, [21, 24, 16]), `out of bounds`);

// ./test/core/table_copy.wast:1189
assert_trap(() => invoke($27, `test`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:1190
assert_trap(() => invoke($27, `test`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:1191
assert_trap(() => invoke($27, `test`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:1192
assert_trap(() => invoke($27, `test`, [3]), `uninitialized element`);

// ./test/core/table_copy.wast:1193
assert_trap(() => invoke($27, `test`, [4]), `uninitialized element`);

// ./test/core/table_copy.wast:1194
assert_trap(() => invoke($27, `test`, [5]), `uninitialized element`);

// ./test/core/table_copy.wast:1195
assert_trap(() => invoke($27, `test`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:1196
assert_trap(() => invoke($27, `test`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:1197
assert_trap(() => invoke($27, `test`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:1198
assert_trap(() => invoke($27, `test`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:1199
assert_trap(() => invoke($27, `test`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:1200
assert_trap(() => invoke($27, `test`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:1201
assert_trap(() => invoke($27, `test`, [12]), `uninitialized element`);

// ./test/core/table_copy.wast:1202
assert_trap(() => invoke($27, `test`, [13]), `uninitialized element`);

// ./test/core/table_copy.wast:1203
assert_trap(() => invoke($27, `test`, [14]), `uninitialized element`);

// ./test/core/table_copy.wast:1204
assert_trap(() => invoke($27, `test`, [15]), `uninitialized element`);

// ./test/core/table_copy.wast:1205
assert_trap(() => invoke($27, `test`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:1206
assert_trap(() => invoke($27, `test`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:1207
assert_trap(() => invoke($27, `test`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:1208
assert_trap(() => invoke($27, `test`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:1209
assert_trap(() => invoke($27, `test`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:1210
assert_trap(() => invoke($27, `test`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:1211
assert_trap(() => invoke($27, `test`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:1212
assert_trap(() => invoke($27, `test`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:1213
assert_return(() => invoke($27, `test`, [24]), [value("i32", 0)]);

// ./test/core/table_copy.wast:1214
assert_return(() => invoke($27, `test`, [25]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1215
assert_return(() => invoke($27, `test`, [26]), [value("i32", 2)]);

// ./test/core/table_copy.wast:1216
assert_return(() => invoke($27, `test`, [27]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1217
assert_return(() => invoke($27, `test`, [28]), [value("i32", 4)]);

// ./test/core/table_copy.wast:1218
assert_return(() => invoke($27, `test`, [29]), [value("i32", 5)]);

// ./test/core/table_copy.wast:1219
assert_return(() => invoke($27, `test`, [30]), [value("i32", 6)]);

// ./test/core/table_copy.wast:1220
assert_return(() => invoke($27, `test`, [31]), [value("i32", 7)]);

// ./test/core/table_copy.wast:1222
let $28 = instantiate(`(module
  (type (func (result i32)))
  (table 32 64 funcref)
  (elem (i32.const 21)
         $$f0 $$f1 $$f2 $$f3 $$f4 $$f5 $$f6 $$f7 $$f8 $$f9 $$f10)
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
  (func (export "run") (param $$targetOffs i32) (param $$srcOffs i32) (param $$len i32)
    (table.copy (local.get $$targetOffs) (local.get $$srcOffs) (local.get $$len))))`);

// ./test/core/table_copy.wast:1248
assert_trap(() => invoke($28, `run`, [21, 21, 16]), `out of bounds`);

// ./test/core/table_copy.wast:1250
assert_trap(() => invoke($28, `test`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:1251
assert_trap(() => invoke($28, `test`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:1252
assert_trap(() => invoke($28, `test`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:1253
assert_trap(() => invoke($28, `test`, [3]), `uninitialized element`);

// ./test/core/table_copy.wast:1254
assert_trap(() => invoke($28, `test`, [4]), `uninitialized element`);

// ./test/core/table_copy.wast:1255
assert_trap(() => invoke($28, `test`, [5]), `uninitialized element`);

// ./test/core/table_copy.wast:1256
assert_trap(() => invoke($28, `test`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:1257
assert_trap(() => invoke($28, `test`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:1258
assert_trap(() => invoke($28, `test`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:1259
assert_trap(() => invoke($28, `test`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:1260
assert_trap(() => invoke($28, `test`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:1261
assert_trap(() => invoke($28, `test`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:1262
assert_trap(() => invoke($28, `test`, [12]), `uninitialized element`);

// ./test/core/table_copy.wast:1263
assert_trap(() => invoke($28, `test`, [13]), `uninitialized element`);

// ./test/core/table_copy.wast:1264
assert_trap(() => invoke($28, `test`, [14]), `uninitialized element`);

// ./test/core/table_copy.wast:1265
assert_trap(() => invoke($28, `test`, [15]), `uninitialized element`);

// ./test/core/table_copy.wast:1266
assert_trap(() => invoke($28, `test`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:1267
assert_trap(() => invoke($28, `test`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:1268
assert_trap(() => invoke($28, `test`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:1269
assert_trap(() => invoke($28, `test`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:1270
assert_trap(() => invoke($28, `test`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:1271
assert_return(() => invoke($28, `test`, [21]), [value("i32", 0)]);

// ./test/core/table_copy.wast:1272
assert_return(() => invoke($28, `test`, [22]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1273
assert_return(() => invoke($28, `test`, [23]), [value("i32", 2)]);

// ./test/core/table_copy.wast:1274
assert_return(() => invoke($28, `test`, [24]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1275
assert_return(() => invoke($28, `test`, [25]), [value("i32", 4)]);

// ./test/core/table_copy.wast:1276
assert_return(() => invoke($28, `test`, [26]), [value("i32", 5)]);

// ./test/core/table_copy.wast:1277
assert_return(() => invoke($28, `test`, [27]), [value("i32", 6)]);

// ./test/core/table_copy.wast:1278
assert_return(() => invoke($28, `test`, [28]), [value("i32", 7)]);

// ./test/core/table_copy.wast:1279
assert_return(() => invoke($28, `test`, [29]), [value("i32", 8)]);

// ./test/core/table_copy.wast:1280
assert_return(() => invoke($28, `test`, [30]), [value("i32", 9)]);

// ./test/core/table_copy.wast:1281
assert_return(() => invoke($28, `test`, [31]), [value("i32", 10)]);

// ./test/core/table_copy.wast:1283
let $29 = instantiate(`(module
  (type (func (result i32)))
  (table 128 128 funcref)
  (elem (i32.const 112)
         $$f0 $$f1 $$f2 $$f3 $$f4 $$f5 $$f6 $$f7 $$f8 $$f9 $$f10 $$f11 $$f12 $$f13 $$f14 $$f15)
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
  (func (export "run") (param $$targetOffs i32) (param $$srcOffs i32) (param $$len i32)
    (table.copy (local.get $$targetOffs) (local.get $$srcOffs) (local.get $$len))))`);

// ./test/core/table_copy.wast:1309
assert_trap(() => invoke($29, `run`, [0, 112, -32]), `out of bounds`);

// ./test/core/table_copy.wast:1311
assert_trap(() => invoke($29, `test`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:1312
assert_trap(() => invoke($29, `test`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:1313
assert_trap(() => invoke($29, `test`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:1314
assert_trap(() => invoke($29, `test`, [3]), `uninitialized element`);

// ./test/core/table_copy.wast:1315
assert_trap(() => invoke($29, `test`, [4]), `uninitialized element`);

// ./test/core/table_copy.wast:1316
assert_trap(() => invoke($29, `test`, [5]), `uninitialized element`);

// ./test/core/table_copy.wast:1317
assert_trap(() => invoke($29, `test`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:1318
assert_trap(() => invoke($29, `test`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:1319
assert_trap(() => invoke($29, `test`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:1320
assert_trap(() => invoke($29, `test`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:1321
assert_trap(() => invoke($29, `test`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:1322
assert_trap(() => invoke($29, `test`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:1323
assert_trap(() => invoke($29, `test`, [12]), `uninitialized element`);

// ./test/core/table_copy.wast:1324
assert_trap(() => invoke($29, `test`, [13]), `uninitialized element`);

// ./test/core/table_copy.wast:1325
assert_trap(() => invoke($29, `test`, [14]), `uninitialized element`);

// ./test/core/table_copy.wast:1326
assert_trap(() => invoke($29, `test`, [15]), `uninitialized element`);

// ./test/core/table_copy.wast:1327
assert_trap(() => invoke($29, `test`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:1328
assert_trap(() => invoke($29, `test`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:1329
assert_trap(() => invoke($29, `test`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:1330
assert_trap(() => invoke($29, `test`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:1331
assert_trap(() => invoke($29, `test`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:1332
assert_trap(() => invoke($29, `test`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:1333
assert_trap(() => invoke($29, `test`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:1334
assert_trap(() => invoke($29, `test`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:1335
assert_trap(() => invoke($29, `test`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:1336
assert_trap(() => invoke($29, `test`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:1337
assert_trap(() => invoke($29, `test`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:1338
assert_trap(() => invoke($29, `test`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:1339
assert_trap(() => invoke($29, `test`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:1340
assert_trap(() => invoke($29, `test`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:1341
assert_trap(() => invoke($29, `test`, [30]), `uninitialized element`);

// ./test/core/table_copy.wast:1342
assert_trap(() => invoke($29, `test`, [31]), `uninitialized element`);

// ./test/core/table_copy.wast:1343
assert_trap(() => invoke($29, `test`, [32]), `uninitialized element`);

// ./test/core/table_copy.wast:1344
assert_trap(() => invoke($29, `test`, [33]), `uninitialized element`);

// ./test/core/table_copy.wast:1345
assert_trap(() => invoke($29, `test`, [34]), `uninitialized element`);

// ./test/core/table_copy.wast:1346
assert_trap(() => invoke($29, `test`, [35]), `uninitialized element`);

// ./test/core/table_copy.wast:1347
assert_trap(() => invoke($29, `test`, [36]), `uninitialized element`);

// ./test/core/table_copy.wast:1348
assert_trap(() => invoke($29, `test`, [37]), `uninitialized element`);

// ./test/core/table_copy.wast:1349
assert_trap(() => invoke($29, `test`, [38]), `uninitialized element`);

// ./test/core/table_copy.wast:1350
assert_trap(() => invoke($29, `test`, [39]), `uninitialized element`);

// ./test/core/table_copy.wast:1351
assert_trap(() => invoke($29, `test`, [40]), `uninitialized element`);

// ./test/core/table_copy.wast:1352
assert_trap(() => invoke($29, `test`, [41]), `uninitialized element`);

// ./test/core/table_copy.wast:1353
assert_trap(() => invoke($29, `test`, [42]), `uninitialized element`);

// ./test/core/table_copy.wast:1354
assert_trap(() => invoke($29, `test`, [43]), `uninitialized element`);

// ./test/core/table_copy.wast:1355
assert_trap(() => invoke($29, `test`, [44]), `uninitialized element`);

// ./test/core/table_copy.wast:1356
assert_trap(() => invoke($29, `test`, [45]), `uninitialized element`);

// ./test/core/table_copy.wast:1357
assert_trap(() => invoke($29, `test`, [46]), `uninitialized element`);

// ./test/core/table_copy.wast:1358
assert_trap(() => invoke($29, `test`, [47]), `uninitialized element`);

// ./test/core/table_copy.wast:1359
assert_trap(() => invoke($29, `test`, [48]), `uninitialized element`);

// ./test/core/table_copy.wast:1360
assert_trap(() => invoke($29, `test`, [49]), `uninitialized element`);

// ./test/core/table_copy.wast:1361
assert_trap(() => invoke($29, `test`, [50]), `uninitialized element`);

// ./test/core/table_copy.wast:1362
assert_trap(() => invoke($29, `test`, [51]), `uninitialized element`);

// ./test/core/table_copy.wast:1363
assert_trap(() => invoke($29, `test`, [52]), `uninitialized element`);

// ./test/core/table_copy.wast:1364
assert_trap(() => invoke($29, `test`, [53]), `uninitialized element`);

// ./test/core/table_copy.wast:1365
assert_trap(() => invoke($29, `test`, [54]), `uninitialized element`);

// ./test/core/table_copy.wast:1366
assert_trap(() => invoke($29, `test`, [55]), `uninitialized element`);

// ./test/core/table_copy.wast:1367
assert_trap(() => invoke($29, `test`, [56]), `uninitialized element`);

// ./test/core/table_copy.wast:1368
assert_trap(() => invoke($29, `test`, [57]), `uninitialized element`);

// ./test/core/table_copy.wast:1369
assert_trap(() => invoke($29, `test`, [58]), `uninitialized element`);

// ./test/core/table_copy.wast:1370
assert_trap(() => invoke($29, `test`, [59]), `uninitialized element`);

// ./test/core/table_copy.wast:1371
assert_trap(() => invoke($29, `test`, [60]), `uninitialized element`);

// ./test/core/table_copy.wast:1372
assert_trap(() => invoke($29, `test`, [61]), `uninitialized element`);

// ./test/core/table_copy.wast:1373
assert_trap(() => invoke($29, `test`, [62]), `uninitialized element`);

// ./test/core/table_copy.wast:1374
assert_trap(() => invoke($29, `test`, [63]), `uninitialized element`);

// ./test/core/table_copy.wast:1375
assert_trap(() => invoke($29, `test`, [64]), `uninitialized element`);

// ./test/core/table_copy.wast:1376
assert_trap(() => invoke($29, `test`, [65]), `uninitialized element`);

// ./test/core/table_copy.wast:1377
assert_trap(() => invoke($29, `test`, [66]), `uninitialized element`);

// ./test/core/table_copy.wast:1378
assert_trap(() => invoke($29, `test`, [67]), `uninitialized element`);

// ./test/core/table_copy.wast:1379
assert_trap(() => invoke($29, `test`, [68]), `uninitialized element`);

// ./test/core/table_copy.wast:1380
assert_trap(() => invoke($29, `test`, [69]), `uninitialized element`);

// ./test/core/table_copy.wast:1381
assert_trap(() => invoke($29, `test`, [70]), `uninitialized element`);

// ./test/core/table_copy.wast:1382
assert_trap(() => invoke($29, `test`, [71]), `uninitialized element`);

// ./test/core/table_copy.wast:1383
assert_trap(() => invoke($29, `test`, [72]), `uninitialized element`);

// ./test/core/table_copy.wast:1384
assert_trap(() => invoke($29, `test`, [73]), `uninitialized element`);

// ./test/core/table_copy.wast:1385
assert_trap(() => invoke($29, `test`, [74]), `uninitialized element`);

// ./test/core/table_copy.wast:1386
assert_trap(() => invoke($29, `test`, [75]), `uninitialized element`);

// ./test/core/table_copy.wast:1387
assert_trap(() => invoke($29, `test`, [76]), `uninitialized element`);

// ./test/core/table_copy.wast:1388
assert_trap(() => invoke($29, `test`, [77]), `uninitialized element`);

// ./test/core/table_copy.wast:1389
assert_trap(() => invoke($29, `test`, [78]), `uninitialized element`);

// ./test/core/table_copy.wast:1390
assert_trap(() => invoke($29, `test`, [79]), `uninitialized element`);

// ./test/core/table_copy.wast:1391
assert_trap(() => invoke($29, `test`, [80]), `uninitialized element`);

// ./test/core/table_copy.wast:1392
assert_trap(() => invoke($29, `test`, [81]), `uninitialized element`);

// ./test/core/table_copy.wast:1393
assert_trap(() => invoke($29, `test`, [82]), `uninitialized element`);

// ./test/core/table_copy.wast:1394
assert_trap(() => invoke($29, `test`, [83]), `uninitialized element`);

// ./test/core/table_copy.wast:1395
assert_trap(() => invoke($29, `test`, [84]), `uninitialized element`);

// ./test/core/table_copy.wast:1396
assert_trap(() => invoke($29, `test`, [85]), `uninitialized element`);

// ./test/core/table_copy.wast:1397
assert_trap(() => invoke($29, `test`, [86]), `uninitialized element`);

// ./test/core/table_copy.wast:1398
assert_trap(() => invoke($29, `test`, [87]), `uninitialized element`);

// ./test/core/table_copy.wast:1399
assert_trap(() => invoke($29, `test`, [88]), `uninitialized element`);

// ./test/core/table_copy.wast:1400
assert_trap(() => invoke($29, `test`, [89]), `uninitialized element`);

// ./test/core/table_copy.wast:1401
assert_trap(() => invoke($29, `test`, [90]), `uninitialized element`);

// ./test/core/table_copy.wast:1402
assert_trap(() => invoke($29, `test`, [91]), `uninitialized element`);

// ./test/core/table_copy.wast:1403
assert_trap(() => invoke($29, `test`, [92]), `uninitialized element`);

// ./test/core/table_copy.wast:1404
assert_trap(() => invoke($29, `test`, [93]), `uninitialized element`);

// ./test/core/table_copy.wast:1405
assert_trap(() => invoke($29, `test`, [94]), `uninitialized element`);

// ./test/core/table_copy.wast:1406
assert_trap(() => invoke($29, `test`, [95]), `uninitialized element`);

// ./test/core/table_copy.wast:1407
assert_trap(() => invoke($29, `test`, [96]), `uninitialized element`);

// ./test/core/table_copy.wast:1408
assert_trap(() => invoke($29, `test`, [97]), `uninitialized element`);

// ./test/core/table_copy.wast:1409
assert_trap(() => invoke($29, `test`, [98]), `uninitialized element`);

// ./test/core/table_copy.wast:1410
assert_trap(() => invoke($29, `test`, [99]), `uninitialized element`);

// ./test/core/table_copy.wast:1411
assert_trap(() => invoke($29, `test`, [100]), `uninitialized element`);

// ./test/core/table_copy.wast:1412
assert_trap(() => invoke($29, `test`, [101]), `uninitialized element`);

// ./test/core/table_copy.wast:1413
assert_trap(() => invoke($29, `test`, [102]), `uninitialized element`);

// ./test/core/table_copy.wast:1414
assert_trap(() => invoke($29, `test`, [103]), `uninitialized element`);

// ./test/core/table_copy.wast:1415
assert_trap(() => invoke($29, `test`, [104]), `uninitialized element`);

// ./test/core/table_copy.wast:1416
assert_trap(() => invoke($29, `test`, [105]), `uninitialized element`);

// ./test/core/table_copy.wast:1417
assert_trap(() => invoke($29, `test`, [106]), `uninitialized element`);

// ./test/core/table_copy.wast:1418
assert_trap(() => invoke($29, `test`, [107]), `uninitialized element`);

// ./test/core/table_copy.wast:1419
assert_trap(() => invoke($29, `test`, [108]), `uninitialized element`);

// ./test/core/table_copy.wast:1420
assert_trap(() => invoke($29, `test`, [109]), `uninitialized element`);

// ./test/core/table_copy.wast:1421
assert_trap(() => invoke($29, `test`, [110]), `uninitialized element`);

// ./test/core/table_copy.wast:1422
assert_trap(() => invoke($29, `test`, [111]), `uninitialized element`);

// ./test/core/table_copy.wast:1423
assert_return(() => invoke($29, `test`, [112]), [value("i32", 0)]);

// ./test/core/table_copy.wast:1424
assert_return(() => invoke($29, `test`, [113]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1425
assert_return(() => invoke($29, `test`, [114]), [value("i32", 2)]);

// ./test/core/table_copy.wast:1426
assert_return(() => invoke($29, `test`, [115]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1427
assert_return(() => invoke($29, `test`, [116]), [value("i32", 4)]);

// ./test/core/table_copy.wast:1428
assert_return(() => invoke($29, `test`, [117]), [value("i32", 5)]);

// ./test/core/table_copy.wast:1429
assert_return(() => invoke($29, `test`, [118]), [value("i32", 6)]);

// ./test/core/table_copy.wast:1430
assert_return(() => invoke($29, `test`, [119]), [value("i32", 7)]);

// ./test/core/table_copy.wast:1431
assert_return(() => invoke($29, `test`, [120]), [value("i32", 8)]);

// ./test/core/table_copy.wast:1432
assert_return(() => invoke($29, `test`, [121]), [value("i32", 9)]);

// ./test/core/table_copy.wast:1433
assert_return(() => invoke($29, `test`, [122]), [value("i32", 10)]);

// ./test/core/table_copy.wast:1434
assert_return(() => invoke($29, `test`, [123]), [value("i32", 11)]);

// ./test/core/table_copy.wast:1435
assert_return(() => invoke($29, `test`, [124]), [value("i32", 12)]);

// ./test/core/table_copy.wast:1436
assert_return(() => invoke($29, `test`, [125]), [value("i32", 13)]);

// ./test/core/table_copy.wast:1437
assert_return(() => invoke($29, `test`, [126]), [value("i32", 14)]);

// ./test/core/table_copy.wast:1438
assert_return(() => invoke($29, `test`, [127]), [value("i32", 15)]);

// ./test/core/table_copy.wast:1440
let $30 = instantiate(`(module
  (type (func (result i32)))
  (table 128 128 funcref)
  (elem (i32.const 0)
         $$f0 $$f1 $$f2 $$f3 $$f4 $$f5 $$f6 $$f7 $$f8 $$f9 $$f10 $$f11 $$f12 $$f13 $$f14 $$f15)
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
  (func (export "run") (param $$targetOffs i32) (param $$srcOffs i32) (param $$len i32)
    (table.copy (local.get $$targetOffs) (local.get $$srcOffs) (local.get $$len))))`);

// ./test/core/table_copy.wast:1466
assert_trap(() => invoke($30, `run`, [112, 0, -32]), `out of bounds`);

// ./test/core/table_copy.wast:1468
assert_return(() => invoke($30, `test`, [0]), [value("i32", 0)]);

// ./test/core/table_copy.wast:1469
assert_return(() => invoke($30, `test`, [1]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1470
assert_return(() => invoke($30, `test`, [2]), [value("i32", 2)]);

// ./test/core/table_copy.wast:1471
assert_return(() => invoke($30, `test`, [3]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1472
assert_return(() => invoke($30, `test`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:1473
assert_return(() => invoke($30, `test`, [5]), [value("i32", 5)]);

// ./test/core/table_copy.wast:1474
assert_return(() => invoke($30, `test`, [6]), [value("i32", 6)]);

// ./test/core/table_copy.wast:1475
assert_return(() => invoke($30, `test`, [7]), [value("i32", 7)]);

// ./test/core/table_copy.wast:1476
assert_return(() => invoke($30, `test`, [8]), [value("i32", 8)]);

// ./test/core/table_copy.wast:1477
assert_return(() => invoke($30, `test`, [9]), [value("i32", 9)]);

// ./test/core/table_copy.wast:1478
assert_return(() => invoke($30, `test`, [10]), [value("i32", 10)]);

// ./test/core/table_copy.wast:1479
assert_return(() => invoke($30, `test`, [11]), [value("i32", 11)]);

// ./test/core/table_copy.wast:1480
assert_return(() => invoke($30, `test`, [12]), [value("i32", 12)]);

// ./test/core/table_copy.wast:1481
assert_return(() => invoke($30, `test`, [13]), [value("i32", 13)]);

// ./test/core/table_copy.wast:1482
assert_return(() => invoke($30, `test`, [14]), [value("i32", 14)]);

// ./test/core/table_copy.wast:1483
assert_return(() => invoke($30, `test`, [15]), [value("i32", 15)]);

// ./test/core/table_copy.wast:1484
assert_trap(() => invoke($30, `test`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:1485
assert_trap(() => invoke($30, `test`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:1486
assert_trap(() => invoke($30, `test`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:1487
assert_trap(() => invoke($30, `test`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:1488
assert_trap(() => invoke($30, `test`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:1489
assert_trap(() => invoke($30, `test`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:1490
assert_trap(() => invoke($30, `test`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:1491
assert_trap(() => invoke($30, `test`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:1492
assert_trap(() => invoke($30, `test`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:1493
assert_trap(() => invoke($30, `test`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:1494
assert_trap(() => invoke($30, `test`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:1495
assert_trap(() => invoke($30, `test`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:1496
assert_trap(() => invoke($30, `test`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:1497
assert_trap(() => invoke($30, `test`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:1498
assert_trap(() => invoke($30, `test`, [30]), `uninitialized element`);

// ./test/core/table_copy.wast:1499
assert_trap(() => invoke($30, `test`, [31]), `uninitialized element`);

// ./test/core/table_copy.wast:1500
assert_trap(() => invoke($30, `test`, [32]), `uninitialized element`);

// ./test/core/table_copy.wast:1501
assert_trap(() => invoke($30, `test`, [33]), `uninitialized element`);

// ./test/core/table_copy.wast:1502
assert_trap(() => invoke($30, `test`, [34]), `uninitialized element`);

// ./test/core/table_copy.wast:1503
assert_trap(() => invoke($30, `test`, [35]), `uninitialized element`);

// ./test/core/table_copy.wast:1504
assert_trap(() => invoke($30, `test`, [36]), `uninitialized element`);

// ./test/core/table_copy.wast:1505
assert_trap(() => invoke($30, `test`, [37]), `uninitialized element`);

// ./test/core/table_copy.wast:1506
assert_trap(() => invoke($30, `test`, [38]), `uninitialized element`);

// ./test/core/table_copy.wast:1507
assert_trap(() => invoke($30, `test`, [39]), `uninitialized element`);

// ./test/core/table_copy.wast:1508
assert_trap(() => invoke($30, `test`, [40]), `uninitialized element`);

// ./test/core/table_copy.wast:1509
assert_trap(() => invoke($30, `test`, [41]), `uninitialized element`);

// ./test/core/table_copy.wast:1510
assert_trap(() => invoke($30, `test`, [42]), `uninitialized element`);

// ./test/core/table_copy.wast:1511
assert_trap(() => invoke($30, `test`, [43]), `uninitialized element`);

// ./test/core/table_copy.wast:1512
assert_trap(() => invoke($30, `test`, [44]), `uninitialized element`);

// ./test/core/table_copy.wast:1513
assert_trap(() => invoke($30, `test`, [45]), `uninitialized element`);

// ./test/core/table_copy.wast:1514
assert_trap(() => invoke($30, `test`, [46]), `uninitialized element`);

// ./test/core/table_copy.wast:1515
assert_trap(() => invoke($30, `test`, [47]), `uninitialized element`);

// ./test/core/table_copy.wast:1516
assert_trap(() => invoke($30, `test`, [48]), `uninitialized element`);

// ./test/core/table_copy.wast:1517
assert_trap(() => invoke($30, `test`, [49]), `uninitialized element`);

// ./test/core/table_copy.wast:1518
assert_trap(() => invoke($30, `test`, [50]), `uninitialized element`);

// ./test/core/table_copy.wast:1519
assert_trap(() => invoke($30, `test`, [51]), `uninitialized element`);

// ./test/core/table_copy.wast:1520
assert_trap(() => invoke($30, `test`, [52]), `uninitialized element`);

// ./test/core/table_copy.wast:1521
assert_trap(() => invoke($30, `test`, [53]), `uninitialized element`);

// ./test/core/table_copy.wast:1522
assert_trap(() => invoke($30, `test`, [54]), `uninitialized element`);

// ./test/core/table_copy.wast:1523
assert_trap(() => invoke($30, `test`, [55]), `uninitialized element`);

// ./test/core/table_copy.wast:1524
assert_trap(() => invoke($30, `test`, [56]), `uninitialized element`);

// ./test/core/table_copy.wast:1525
assert_trap(() => invoke($30, `test`, [57]), `uninitialized element`);

// ./test/core/table_copy.wast:1526
assert_trap(() => invoke($30, `test`, [58]), `uninitialized element`);

// ./test/core/table_copy.wast:1527
assert_trap(() => invoke($30, `test`, [59]), `uninitialized element`);

// ./test/core/table_copy.wast:1528
assert_trap(() => invoke($30, `test`, [60]), `uninitialized element`);

// ./test/core/table_copy.wast:1529
assert_trap(() => invoke($30, `test`, [61]), `uninitialized element`);

// ./test/core/table_copy.wast:1530
assert_trap(() => invoke($30, `test`, [62]), `uninitialized element`);

// ./test/core/table_copy.wast:1531
assert_trap(() => invoke($30, `test`, [63]), `uninitialized element`);

// ./test/core/table_copy.wast:1532
assert_trap(() => invoke($30, `test`, [64]), `uninitialized element`);

// ./test/core/table_copy.wast:1533
assert_trap(() => invoke($30, `test`, [65]), `uninitialized element`);

// ./test/core/table_copy.wast:1534
assert_trap(() => invoke($30, `test`, [66]), `uninitialized element`);

// ./test/core/table_copy.wast:1535
assert_trap(() => invoke($30, `test`, [67]), `uninitialized element`);

// ./test/core/table_copy.wast:1536
assert_trap(() => invoke($30, `test`, [68]), `uninitialized element`);

// ./test/core/table_copy.wast:1537
assert_trap(() => invoke($30, `test`, [69]), `uninitialized element`);

// ./test/core/table_copy.wast:1538
assert_trap(() => invoke($30, `test`, [70]), `uninitialized element`);

// ./test/core/table_copy.wast:1539
assert_trap(() => invoke($30, `test`, [71]), `uninitialized element`);

// ./test/core/table_copy.wast:1540
assert_trap(() => invoke($30, `test`, [72]), `uninitialized element`);

// ./test/core/table_copy.wast:1541
assert_trap(() => invoke($30, `test`, [73]), `uninitialized element`);

// ./test/core/table_copy.wast:1542
assert_trap(() => invoke($30, `test`, [74]), `uninitialized element`);

// ./test/core/table_copy.wast:1543
assert_trap(() => invoke($30, `test`, [75]), `uninitialized element`);

// ./test/core/table_copy.wast:1544
assert_trap(() => invoke($30, `test`, [76]), `uninitialized element`);

// ./test/core/table_copy.wast:1545
assert_trap(() => invoke($30, `test`, [77]), `uninitialized element`);

// ./test/core/table_copy.wast:1546
assert_trap(() => invoke($30, `test`, [78]), `uninitialized element`);

// ./test/core/table_copy.wast:1547
assert_trap(() => invoke($30, `test`, [79]), `uninitialized element`);

// ./test/core/table_copy.wast:1548
assert_trap(() => invoke($30, `test`, [80]), `uninitialized element`);

// ./test/core/table_copy.wast:1549
assert_trap(() => invoke($30, `test`, [81]), `uninitialized element`);

// ./test/core/table_copy.wast:1550
assert_trap(() => invoke($30, `test`, [82]), `uninitialized element`);

// ./test/core/table_copy.wast:1551
assert_trap(() => invoke($30, `test`, [83]), `uninitialized element`);

// ./test/core/table_copy.wast:1552
assert_trap(() => invoke($30, `test`, [84]), `uninitialized element`);

// ./test/core/table_copy.wast:1553
assert_trap(() => invoke($30, `test`, [85]), `uninitialized element`);

// ./test/core/table_copy.wast:1554
assert_trap(() => invoke($30, `test`, [86]), `uninitialized element`);

// ./test/core/table_copy.wast:1555
assert_trap(() => invoke($30, `test`, [87]), `uninitialized element`);

// ./test/core/table_copy.wast:1556
assert_trap(() => invoke($30, `test`, [88]), `uninitialized element`);

// ./test/core/table_copy.wast:1557
assert_trap(() => invoke($30, `test`, [89]), `uninitialized element`);

// ./test/core/table_copy.wast:1558
assert_trap(() => invoke($30, `test`, [90]), `uninitialized element`);

// ./test/core/table_copy.wast:1559
assert_trap(() => invoke($30, `test`, [91]), `uninitialized element`);

// ./test/core/table_copy.wast:1560
assert_trap(() => invoke($30, `test`, [92]), `uninitialized element`);

// ./test/core/table_copy.wast:1561
assert_trap(() => invoke($30, `test`, [93]), `uninitialized element`);

// ./test/core/table_copy.wast:1562
assert_trap(() => invoke($30, `test`, [94]), `uninitialized element`);

// ./test/core/table_copy.wast:1563
assert_trap(() => invoke($30, `test`, [95]), `uninitialized element`);

// ./test/core/table_copy.wast:1564
assert_trap(() => invoke($30, `test`, [96]), `uninitialized element`);

// ./test/core/table_copy.wast:1565
assert_trap(() => invoke($30, `test`, [97]), `uninitialized element`);

// ./test/core/table_copy.wast:1566
assert_trap(() => invoke($30, `test`, [98]), `uninitialized element`);

// ./test/core/table_copy.wast:1567
assert_trap(() => invoke($30, `test`, [99]), `uninitialized element`);

// ./test/core/table_copy.wast:1568
assert_trap(() => invoke($30, `test`, [100]), `uninitialized element`);

// ./test/core/table_copy.wast:1569
assert_trap(() => invoke($30, `test`, [101]), `uninitialized element`);

// ./test/core/table_copy.wast:1570
assert_trap(() => invoke($30, `test`, [102]), `uninitialized element`);

// ./test/core/table_copy.wast:1571
assert_trap(() => invoke($30, `test`, [103]), `uninitialized element`);

// ./test/core/table_copy.wast:1572
assert_trap(() => invoke($30, `test`, [104]), `uninitialized element`);

// ./test/core/table_copy.wast:1573
assert_trap(() => invoke($30, `test`, [105]), `uninitialized element`);

// ./test/core/table_copy.wast:1574
assert_trap(() => invoke($30, `test`, [106]), `uninitialized element`);

// ./test/core/table_copy.wast:1575
assert_trap(() => invoke($30, `test`, [107]), `uninitialized element`);

// ./test/core/table_copy.wast:1576
assert_trap(() => invoke($30, `test`, [108]), `uninitialized element`);

// ./test/core/table_copy.wast:1577
assert_trap(() => invoke($30, `test`, [109]), `uninitialized element`);

// ./test/core/table_copy.wast:1578
assert_trap(() => invoke($30, `test`, [110]), `uninitialized element`);

// ./test/core/table_copy.wast:1579
assert_trap(() => invoke($30, `test`, [111]), `uninitialized element`);

// ./test/core/table_copy.wast:1580
assert_trap(() => invoke($30, `test`, [112]), `uninitialized element`);

// ./test/core/table_copy.wast:1581
assert_trap(() => invoke($30, `test`, [113]), `uninitialized element`);

// ./test/core/table_copy.wast:1582
assert_trap(() => invoke($30, `test`, [114]), `uninitialized element`);

// ./test/core/table_copy.wast:1583
assert_trap(() => invoke($30, `test`, [115]), `uninitialized element`);

// ./test/core/table_copy.wast:1584
assert_trap(() => invoke($30, `test`, [116]), `uninitialized element`);

// ./test/core/table_copy.wast:1585
assert_trap(() => invoke($30, `test`, [117]), `uninitialized element`);

// ./test/core/table_copy.wast:1586
assert_trap(() => invoke($30, `test`, [118]), `uninitialized element`);

// ./test/core/table_copy.wast:1587
assert_trap(() => invoke($30, `test`, [119]), `uninitialized element`);

// ./test/core/table_copy.wast:1588
assert_trap(() => invoke($30, `test`, [120]), `uninitialized element`);

// ./test/core/table_copy.wast:1589
assert_trap(() => invoke($30, `test`, [121]), `uninitialized element`);

// ./test/core/table_copy.wast:1590
assert_trap(() => invoke($30, `test`, [122]), `uninitialized element`);

// ./test/core/table_copy.wast:1591
assert_trap(() => invoke($30, `test`, [123]), `uninitialized element`);

// ./test/core/table_copy.wast:1592
assert_trap(() => invoke($30, `test`, [124]), `uninitialized element`);

// ./test/core/table_copy.wast:1593
assert_trap(() => invoke($30, `test`, [125]), `uninitialized element`);

// ./test/core/table_copy.wast:1594
assert_trap(() => invoke($30, `test`, [126]), `uninitialized element`);

// ./test/core/table_copy.wast:1595
assert_trap(() => invoke($30, `test`, [127]), `uninitialized element`);
