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

// ./test/core/bulk.wast

// ./test/core/bulk.wast:2
let $0 = instantiate(`(module
  (memory 1)
  (data "foo"))`);

// ./test/core/bulk.wast:6
let $1 = instantiate(`(module
  (table 3 funcref)
  (elem funcref (ref.func 0) (ref.null func) (ref.func 1))
  (func)
  (func))`);

// ./test/core/bulk.wast:13
let $2 = instantiate(`(module
  (memory 1)

  (func (export "fill") (param i32 i32 i32)
    (memory.fill
      (local.get 0)
      (local.get 1)
      (local.get 2)))

  (func (export "load8_u") (param i32) (result i32)
    (i32.load8_u (local.get 0)))
)`);

// ./test/core/bulk.wast:27
invoke($2, `fill`, [1, 255, 3]);

// ./test/core/bulk.wast:28
assert_return(() => invoke($2, `load8_u`, [0]), [value("i32", 0)]);

// ./test/core/bulk.wast:29
assert_return(() => invoke($2, `load8_u`, [1]), [value("i32", 255)]);

// ./test/core/bulk.wast:30
assert_return(() => invoke($2, `load8_u`, [2]), [value("i32", 255)]);

// ./test/core/bulk.wast:31
assert_return(() => invoke($2, `load8_u`, [3]), [value("i32", 255)]);

// ./test/core/bulk.wast:32
assert_return(() => invoke($2, `load8_u`, [4]), [value("i32", 0)]);

// ./test/core/bulk.wast:35
invoke($2, `fill`, [0, 48042, 2]);

// ./test/core/bulk.wast:36
assert_return(() => invoke($2, `load8_u`, [0]), [value("i32", 170)]);

// ./test/core/bulk.wast:37
assert_return(() => invoke($2, `load8_u`, [1]), [value("i32", 170)]);

// ./test/core/bulk.wast:40
invoke($2, `fill`, [0, 0, 65536]);

// ./test/core/bulk.wast:43
assert_trap(
  () => invoke($2, `fill`, [65280, 1, 257]),
  `out of bounds memory access`,
);

// ./test/core/bulk.wast:45
assert_return(() => invoke($2, `load8_u`, [65280]), [value("i32", 0)]);

// ./test/core/bulk.wast:46
assert_return(() => invoke($2, `load8_u`, [65535]), [value("i32", 0)]);

// ./test/core/bulk.wast:49
invoke($2, `fill`, [65536, 0, 0]);

// ./test/core/bulk.wast:52
assert_trap(
  () => invoke($2, `fill`, [65537, 0, 0]),
  `out of bounds memory access`,
);

// ./test/core/bulk.wast:57
let $3 = instantiate(`(module
  (memory (data "\\aa\\bb\\cc\\dd"))

  (func (export "copy") (param i32 i32 i32)
    (memory.copy
      (local.get 0)
      (local.get 1)
      (local.get 2)))

  (func (export "load8_u") (param i32) (result i32)
    (i32.load8_u (local.get 0)))
)`);

// ./test/core/bulk.wast:71
invoke($3, `copy`, [10, 0, 4]);

// ./test/core/bulk.wast:73
assert_return(() => invoke($3, `load8_u`, [9]), [value("i32", 0)]);

// ./test/core/bulk.wast:74
assert_return(() => invoke($3, `load8_u`, [10]), [value("i32", 170)]);

// ./test/core/bulk.wast:75
assert_return(() => invoke($3, `load8_u`, [11]), [value("i32", 187)]);

// ./test/core/bulk.wast:76
assert_return(() => invoke($3, `load8_u`, [12]), [value("i32", 204)]);

// ./test/core/bulk.wast:77
assert_return(() => invoke($3, `load8_u`, [13]), [value("i32", 221)]);

// ./test/core/bulk.wast:78
assert_return(() => invoke($3, `load8_u`, [14]), [value("i32", 0)]);

// ./test/core/bulk.wast:81
invoke($3, `copy`, [8, 10, 4]);

// ./test/core/bulk.wast:82
assert_return(() => invoke($3, `load8_u`, [8]), [value("i32", 170)]);

// ./test/core/bulk.wast:83
assert_return(() => invoke($3, `load8_u`, [9]), [value("i32", 187)]);

// ./test/core/bulk.wast:84
assert_return(() => invoke($3, `load8_u`, [10]), [value("i32", 204)]);

// ./test/core/bulk.wast:85
assert_return(() => invoke($3, `load8_u`, [11]), [value("i32", 221)]);

// ./test/core/bulk.wast:86
assert_return(() => invoke($3, `load8_u`, [12]), [value("i32", 204)]);

// ./test/core/bulk.wast:87
assert_return(() => invoke($3, `load8_u`, [13]), [value("i32", 221)]);

// ./test/core/bulk.wast:90
invoke($3, `copy`, [10, 7, 6]);

// ./test/core/bulk.wast:91
assert_return(() => invoke($3, `load8_u`, [10]), [value("i32", 0)]);

// ./test/core/bulk.wast:92
assert_return(() => invoke($3, `load8_u`, [11]), [value("i32", 170)]);

// ./test/core/bulk.wast:93
assert_return(() => invoke($3, `load8_u`, [12]), [value("i32", 187)]);

// ./test/core/bulk.wast:94
assert_return(() => invoke($3, `load8_u`, [13]), [value("i32", 204)]);

// ./test/core/bulk.wast:95
assert_return(() => invoke($3, `load8_u`, [14]), [value("i32", 221)]);

// ./test/core/bulk.wast:96
assert_return(() => invoke($3, `load8_u`, [15]), [value("i32", 204)]);

// ./test/core/bulk.wast:97
assert_return(() => invoke($3, `load8_u`, [16]), [value("i32", 0)]);

// ./test/core/bulk.wast:100
invoke($3, `copy`, [65280, 0, 256]);

// ./test/core/bulk.wast:101
invoke($3, `copy`, [65024, 65280, 256]);

// ./test/core/bulk.wast:104
invoke($3, `copy`, [65536, 0, 0]);

// ./test/core/bulk.wast:105
invoke($3, `copy`, [0, 65536, 0]);

// ./test/core/bulk.wast:108
assert_trap(
  () => invoke($3, `copy`, [65537, 0, 0]),
  `out of bounds memory access`,
);

// ./test/core/bulk.wast:110
assert_trap(
  () => invoke($3, `copy`, [0, 65537, 0]),
  `out of bounds memory access`,
);

// ./test/core/bulk.wast:115
let $4 = instantiate(`(module
  (memory 1)
  (data "\\aa\\bb\\cc\\dd")

  (func (export "init") (param i32 i32 i32)
    (memory.init 0
      (local.get 0)
      (local.get 1)
      (local.get 2)))

  (func (export "load8_u") (param i32) (result i32)
    (i32.load8_u (local.get 0)))
)`);

// ./test/core/bulk.wast:129
invoke($4, `init`, [0, 1, 2]);

// ./test/core/bulk.wast:130
assert_return(() => invoke($4, `load8_u`, [0]), [value("i32", 187)]);

// ./test/core/bulk.wast:131
assert_return(() => invoke($4, `load8_u`, [1]), [value("i32", 204)]);

// ./test/core/bulk.wast:132
assert_return(() => invoke($4, `load8_u`, [2]), [value("i32", 0)]);

// ./test/core/bulk.wast:135
invoke($4, `init`, [65532, 0, 4]);

// ./test/core/bulk.wast:138
assert_trap(
  () => invoke($4, `init`, [65534, 0, 3]),
  `out of bounds memory access`,
);

// ./test/core/bulk.wast:140
assert_return(() => invoke($4, `load8_u`, [65534]), [value("i32", 204)]);

// ./test/core/bulk.wast:141
assert_return(() => invoke($4, `load8_u`, [65535]), [value("i32", 221)]);

// ./test/core/bulk.wast:144
invoke($4, `init`, [65536, 0, 0]);

// ./test/core/bulk.wast:145
invoke($4, `init`, [0, 4, 0]);

// ./test/core/bulk.wast:148
assert_trap(
  () => invoke($4, `init`, [65537, 0, 0]),
  `out of bounds memory access`,
);

// ./test/core/bulk.wast:150
assert_trap(() => invoke($4, `init`, [0, 5, 0]), `out of bounds memory access`);

// ./test/core/bulk.wast:154
let $5 = instantiate(`(module
  (memory 1)
  (data $$p "x")
  (data $$a (memory 0) (i32.const 0) "x")

  (func (export "drop_passive") (data.drop $$p))
  (func (export "init_passive") (param $$len i32)
    (memory.init $$p (i32.const 0) (i32.const 0) (local.get $$len)))

  (func (export "drop_active") (data.drop $$a))
  (func (export "init_active") (param $$len i32)
    (memory.init $$a (i32.const 0) (i32.const 0) (local.get $$len)))
)`);

// ./test/core/bulk.wast:168
invoke($5, `init_passive`, [1]);

// ./test/core/bulk.wast:169
invoke($5, `drop_passive`, []);

// ./test/core/bulk.wast:170
invoke($5, `drop_passive`, []);

// ./test/core/bulk.wast:171
assert_return(() => invoke($5, `init_passive`, [0]), []);

// ./test/core/bulk.wast:172
assert_trap(
  () => invoke($5, `init_passive`, [1]),
  `out of bounds memory access`,
);

// ./test/core/bulk.wast:173
invoke($5, `init_passive`, [0]);

// ./test/core/bulk.wast:174
invoke($5, `drop_active`, []);

// ./test/core/bulk.wast:175
assert_return(() => invoke($5, `init_active`, [0]), []);

// ./test/core/bulk.wast:176
assert_trap(
  () => invoke($5, `init_active`, [1]),
  `out of bounds memory access`,
);

// ./test/core/bulk.wast:177
invoke($5, `init_active`, [0]);

// ./test/core/bulk.wast:181
let $6 = instantiate(`(module
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
  (func (data.drop 64)))`);

// ./test/core/bulk.wast:196
let $7 = instantiate(`(module (data "goodbye") (func (data.drop 0)))`);

// ./test/core/bulk.wast:199
let $8 = instantiate(`(module
  (table 3 funcref)
  (elem funcref
    (ref.func $$zero) (ref.func $$one) (ref.func $$zero) (ref.func $$one))

  (func $$zero (result i32) (i32.const 0))
  (func $$one (result i32) (i32.const 1))

  (func (export "init") (param i32 i32 i32)
    (table.init 0
      (local.get 0)
      (local.get 1)
      (local.get 2)))

  (func (export "call") (param i32) (result i32)
    (call_indirect (result i32)
      (local.get 0)))
)`);

// ./test/core/bulk.wast:219
assert_trap(() => invoke($8, `init`, [2, 0, 2]), `out of bounds table access`);

// ./test/core/bulk.wast:221
assert_trap(() => invoke($8, `call`, [2]), `uninitialized element 2`);

// ./test/core/bulk.wast:224
invoke($8, `init`, [0, 1, 2]);

// ./test/core/bulk.wast:225
assert_return(() => invoke($8, `call`, [0]), [value("i32", 1)]);

// ./test/core/bulk.wast:226
assert_return(() => invoke($8, `call`, [1]), [value("i32", 0)]);

// ./test/core/bulk.wast:227
assert_trap(() => invoke($8, `call`, [2]), `uninitialized element`);

// ./test/core/bulk.wast:230
invoke($8, `init`, [1, 2, 2]);

// ./test/core/bulk.wast:233
invoke($8, `init`, [3, 0, 0]);

// ./test/core/bulk.wast:234
invoke($8, `init`, [0, 4, 0]);

// ./test/core/bulk.wast:237
assert_trap(() => invoke($8, `init`, [4, 0, 0]), `out of bounds table access`);

// ./test/core/bulk.wast:239
assert_trap(() => invoke($8, `init`, [0, 5, 0]), `out of bounds table access`);

// ./test/core/bulk.wast:244
let $9 = instantiate(`(module
  (table 1 funcref)
  (func $$f)
  (elem $$p funcref (ref.func $$f))
  (elem $$a (table 0) (i32.const 0) func $$f)

  (func (export "drop_passive") (elem.drop $$p))
  (func (export "init_passive") (param $$len i32)
    (table.init $$p (i32.const 0) (i32.const 0) (local.get $$len))
  )

  (func (export "drop_active") (elem.drop $$a))
  (func (export "init_active") (param $$len i32)
    (table.init $$a (i32.const 0) (i32.const 0) (local.get $$len))
  )
)`);

// ./test/core/bulk.wast:261
invoke($9, `init_passive`, [1]);

// ./test/core/bulk.wast:262
invoke($9, `drop_passive`, []);

// ./test/core/bulk.wast:263
invoke($9, `drop_passive`, []);

// ./test/core/bulk.wast:264
assert_return(() => invoke($9, `init_passive`, [0]), []);

// ./test/core/bulk.wast:265
assert_trap(
  () => invoke($9, `init_passive`, [1]),
  `out of bounds table access`,
);

// ./test/core/bulk.wast:266
invoke($9, `init_passive`, [0]);

// ./test/core/bulk.wast:267
invoke($9, `drop_active`, []);

// ./test/core/bulk.wast:268
assert_return(() => invoke($9, `init_active`, [0]), []);

// ./test/core/bulk.wast:269
assert_trap(() => invoke($9, `init_active`, [1]), `out of bounds table access`);

// ./test/core/bulk.wast:270
invoke($9, `init_active`, [0]);

// ./test/core/bulk.wast:274
let $10 = instantiate(`(module
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
  (func (elem.drop 64)))`);

// ./test/core/bulk.wast:297
let $11 = instantiate(
  `(module (elem funcref (ref.func 0)) (func (elem.drop 0)))`,
);

// ./test/core/bulk.wast:300
let $12 = instantiate(`(module
  (table 10 funcref)
  (elem (i32.const 0) $$zero $$one $$two)
  (func $$zero (result i32) (i32.const 0))
  (func $$one (result i32) (i32.const 1))
  (func $$two (result i32) (i32.const 2))

  (func (export "copy") (param i32 i32 i32)
    (table.copy
      (local.get 0)
      (local.get 1)
      (local.get 2)))

  (func (export "call") (param i32) (result i32)
    (call_indirect (result i32)
      (local.get 0)))
)`);

// ./test/core/bulk.wast:319
invoke($12, `copy`, [3, 0, 3]);

// ./test/core/bulk.wast:321
assert_return(() => invoke($12, `call`, [3]), [value("i32", 0)]);

// ./test/core/bulk.wast:322
assert_return(() => invoke($12, `call`, [4]), [value("i32", 1)]);

// ./test/core/bulk.wast:323
assert_return(() => invoke($12, `call`, [5]), [value("i32", 2)]);

// ./test/core/bulk.wast:326
invoke($12, `copy`, [0, 1, 3]);

// ./test/core/bulk.wast:328
assert_return(() => invoke($12, `call`, [0]), [value("i32", 1)]);

// ./test/core/bulk.wast:329
assert_return(() => invoke($12, `call`, [1]), [value("i32", 2)]);

// ./test/core/bulk.wast:330
assert_return(() => invoke($12, `call`, [2]), [value("i32", 0)]);

// ./test/core/bulk.wast:333
invoke($12, `copy`, [2, 0, 3]);

// ./test/core/bulk.wast:335
assert_return(() => invoke($12, `call`, [2]), [value("i32", 1)]);

// ./test/core/bulk.wast:336
assert_return(() => invoke($12, `call`, [3]), [value("i32", 2)]);

// ./test/core/bulk.wast:337
assert_return(() => invoke($12, `call`, [4]), [value("i32", 0)]);

// ./test/core/bulk.wast:340
invoke($12, `copy`, [6, 8, 2]);

// ./test/core/bulk.wast:341
invoke($12, `copy`, [8, 6, 2]);

// ./test/core/bulk.wast:344
invoke($12, `copy`, [10, 0, 0]);

// ./test/core/bulk.wast:345
invoke($12, `copy`, [0, 10, 0]);

// ./test/core/bulk.wast:348
assert_trap(
  () => invoke($12, `copy`, [11, 0, 0]),
  `out of bounds table access`,
);

// ./test/core/bulk.wast:350
assert_trap(
  () => invoke($12, `copy`, [0, 11, 0]),
  `out of bounds table access`,
);
