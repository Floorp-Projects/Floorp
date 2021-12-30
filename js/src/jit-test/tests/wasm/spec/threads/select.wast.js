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

// ./test/core/select.wast

// ./test/core/select.wast:1
let $0 = instantiate(`(module

  (memory 1)

  (func $$dummy)

  (func (export "select_i32") (param $$lhs i32) (param $$rhs i32) (param $$cond i32) (result i32)
   (select (local.get $$lhs) (local.get $$rhs) (local.get $$cond)))

  (func (export "select_i64") (param $$lhs i64) (param $$rhs i64) (param $$cond i32) (result i64)
   (select (local.get $$lhs) (local.get $$rhs) (local.get $$cond)))

  (func (export "select_f32") (param $$lhs f32) (param $$rhs f32) (param $$cond i32) (result f32)
   (select (local.get $$lhs) (local.get $$rhs) (local.get $$cond)))

  (func (export "select_f64") (param $$lhs f64) (param $$rhs f64) (param $$cond i32) (result f64)
   (select (local.get $$lhs) (local.get $$rhs) (local.get $$cond)))

  ;; Check that both sides of the select are evaluated
  (func (export "select_trap_l") (param $$cond i32) (result i32)
    (select (unreachable) (i32.const 0) (local.get $$cond))
  )
  (func (export "select_trap_r") (param $$cond i32) (result i32)
    (select (i32.const 0) (unreachable) (local.get $$cond))
  )

  (func (export "select_unreached")
    (unreachable) (select)
    (unreachable) (i32.const 0) (select)
    (unreachable) (i32.const 0) (i32.const 0) (select)
    (unreachable) (f32.const 0) (i32.const 0) (select)
    (unreachable)
  )

  ;; As the argument of control constructs and instructions

  (func (export "as-select-first") (param i32) (result i32)
    (select (select (i32.const 0) (i32.const 1) (local.get 0)) (i32.const 2) (i32.const 3))
  )
  (func (export "as-select-mid") (param i32) (result i32)
    (select (i32.const 2) (select (i32.const 0) (i32.const 1) (local.get 0)) (i32.const 3))
  )
  (func (export "as-select-last") (param i32) (result i32)
    (select (i32.const 2) (i32.const 3) (select (i32.const 0) (i32.const 1) (local.get 0)))
  )

  (func (export "as-loop-first") (param i32) (result i32)
    (loop (result i32) (select (i32.const 2) (i32.const 3) (local.get 0)) (call $$dummy) (call $$dummy))
  )
  (func (export "as-loop-mid") (param i32) (result i32)
    (loop (result i32) (call $$dummy) (select (i32.const 2) (i32.const 3) (local.get 0)) (call $$dummy))
  )
  (func (export "as-loop-last") (param i32) (result i32)
    (loop (result i32) (call $$dummy) (call $$dummy) (select (i32.const 2) (i32.const 3) (local.get 0)))
  )

  (func (export "as-if-condition") (param i32)
    (select (i32.const 2) (i32.const 3) (local.get 0)) (if (then (call $$dummy)))
  )
  (func (export "as-if-then") (param i32) (result i32)
    (if (result i32) (i32.const 1) (then (select (i32.const 2) (i32.const 3) (local.get 0))) (else (i32.const 4)))
  )
  (func (export "as-if-else") (param i32) (result i32)
    (if (result i32) (i32.const 0) (then (i32.const 2)) (else (select (i32.const 2) (i32.const 3) (local.get 0))))
  )

  (func (export "as-br_if-first") (param i32) (result i32)
    (block (result i32) (br_if 0 (select (i32.const 2) (i32.const 3) (local.get 0)) (i32.const 4)))
  )
  (func (export "as-br_if-last") (param i32) (result i32)
    (block (result i32) (br_if 0 (i32.const 2) (select (i32.const 2) (i32.const 3) (local.get 0))))
  )

  (func (export "as-br_table-first") (param i32) (result i32)
    (block (result i32) (select (i32.const 2) (i32.const 3) (local.get 0)) (i32.const 2) (br_table 0 0))
  )
  (func (export "as-br_table-last") (param i32) (result i32)
    (block (result i32) (i32.const 2) (select (i32.const 2) (i32.const 3) (local.get 0)) (br_table 0 0))
  )

  (func $$func (param i32 i32) (result i32) (local.get 0))
  (type $$check (func (param i32 i32) (result i32)))
  (table funcref (elem $$func))
  (func (export "as-call_indirect-first") (param i32) (result i32)
    (block (result i32)
      (call_indirect (type $$check)
        (select (i32.const 2) (i32.const 3) (local.get 0)) (i32.const 1) (i32.const 0)
      )
    )
  )
  (func (export "as-call_indirect-mid") (param i32) (result i32)
    (block (result i32)
      (call_indirect (type $$check)
        (i32.const 1) (select (i32.const 2) (i32.const 3) (local.get 0)) (i32.const 0)
      )
    )
  )
  (func (export "as-call_indirect-last") (param i32) (result i32)
    (block (result i32)
      (call_indirect (type $$check)
        (i32.const 1) (i32.const 4) (select (i32.const 2) (i32.const 3) (local.get 0))
      )
    )
  )

  (func (export "as-store-first") (param i32)
    (select (i32.const 0) (i32.const 4) (local.get 0)) (i32.const 1) (i32.store)
  )
  (func (export "as-store-last") (param i32)
    (i32.const 8) (select (i32.const 1) (i32.const 2) (local.get 0)) (i32.store)
  )

  (func (export "as-memory.grow-value") (param i32) (result i32)
    (memory.grow (select (i32.const 1) (i32.const 2) (local.get 0)))
  )

  (func $$f (param i32) (result i32) (local.get 0))

  (func (export "as-call-value") (param i32) (result i32)
    (call $$f (select (i32.const 1) (i32.const 2) (local.get 0)))
  )
  (func (export "as-return-value") (param i32) (result i32)
    (select (i32.const 1) (i32.const 2) (local.get 0)) (return)
  )
  (func (export "as-drop-operand") (param i32)
    (drop (select (i32.const 1) (i32.const 2) (local.get 0)))
  )
  (func (export "as-br-value") (param i32) (result i32)
    (block (result i32) (br 0 (select (i32.const 1) (i32.const 2) (local.get 0))))
  )
  (func (export "as-local.set-value") (param i32) (result i32)
    (local i32) (local.set 0 (select (i32.const 1) (i32.const 2) (local.get 0))) (local.get 0)
  )
  (func (export "as-local.tee-value") (param i32) (result i32)
    (local.tee 0 (select (i32.const 1) (i32.const 2) (local.get 0)))
  )
  (global $$a (mut i32) (i32.const 10))
  (func (export "as-global.set-value") (param i32) (result i32)
    (global.set $$a (select (i32.const 1) (i32.const 2) (local.get 0)))
    (global.get $$a)
  )
  (func (export "as-load-operand") (param i32) (result i32)
    (i32.load (select (i32.const 0) (i32.const 4) (local.get 0)))
  )

  (func (export "as-unary-operand") (param i32) (result i32)
    (i32.eqz (select (i32.const 0) (i32.const 1) (local.get 0)))
  )
  (func (export "as-binary-operand") (param i32) (result i32)
    (i32.mul
      (select (i32.const 1) (i32.const 2) (local.get 0))
      (select (i32.const 1) (i32.const 2) (local.get 0))
    )
  )
  (func (export "as-test-operand") (param i32) (result i32)
    (block (result i32)
      (i32.eqz (select (i32.const 0) (i32.const 1) (local.get 0)))
    )
  )

  (func (export "as-compare-left") (param i32) (result i32)
    (block (result i32)
      (i32.le_s (select (i32.const 1) (i32.const 2) (local.get 0)) (i32.const 1))
    )
  )
  (func (export "as-compare-right") (param i32) (result i32)
    (block (result i32)
      (i32.ne (i32.const 1) (select (i32.const 0) (i32.const 1) (local.get 0)))
    )
  )

  (func (export "as-convert-operand") (param i32) (result i32)
    (block (result i32)
      (i32.wrap_i64 (select (i64.const 1) (i64.const 0) (local.get 0)))
    )
  )

)`);

// ./test/core/select.wast:180
assert_return(() => invoke($0, `select_i32`, [1, 2, 1]), [value("i32", 1)]);

// ./test/core/select.wast:181
assert_return(() => invoke($0, `select_i64`, [2n, 1n, 1]), [value("i64", 2n)]);

// ./test/core/select.wast:182
assert_return(
  () => invoke($0, `select_f32`, [value("f32", 1), value("f32", 2), 1]),
  [value("f32", 1)],
);

// ./test/core/select.wast:183
assert_return(
  () => invoke($0, `select_f64`, [value("f64", 1), value("f64", 2), 1]),
  [value("f64", 1)],
);

// ./test/core/select.wast:185
assert_return(() => invoke($0, `select_i32`, [1, 2, 0]), [value("i32", 2)]);

// ./test/core/select.wast:186
assert_return(() => invoke($0, `select_i32`, [2, 1, 0]), [value("i32", 1)]);

// ./test/core/select.wast:187
assert_return(() => invoke($0, `select_i64`, [2n, 1n, -1]), [value("i64", 2n)]);

// ./test/core/select.wast:188
assert_return(() => invoke($0, `select_i64`, [2n, 1n, -252645136]), [
  value("i64", 2n),
]);

// ./test/core/select.wast:190
assert_return(
  () =>
    invoke($0, `select_f32`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 1),
      1,
    ]),
  [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])],
);

// ./test/core/select.wast:191
assert_return(
  () =>
    invoke($0, `select_f32`, [
      bytes("f32", [0x4, 0x3, 0x82, 0x7f]),
      value("f32", 1),
      1,
    ]),
  [bytes("f32", [0x4, 0x3, 0x82, 0x7f])],
);

// ./test/core/select.wast:192
assert_return(
  () =>
    invoke($0, `select_f32`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 1),
      0,
    ]),
  [value("f32", 1)],
);

// ./test/core/select.wast:193
assert_return(
  () =>
    invoke($0, `select_f32`, [
      bytes("f32", [0x4, 0x3, 0x82, 0x7f]),
      value("f32", 1),
      0,
    ]),
  [value("f32", 1)],
);

// ./test/core/select.wast:194
assert_return(
  () =>
    invoke($0, `select_f32`, [
      value("f32", 2),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      1,
    ]),
  [value("f32", 2)],
);

// ./test/core/select.wast:195
assert_return(
  () =>
    invoke($0, `select_f32`, [
      value("f32", 2),
      bytes("f32", [0x4, 0x3, 0x82, 0x7f]),
      1,
    ]),
  [value("f32", 2)],
);

// ./test/core/select.wast:196
assert_return(
  () =>
    invoke($0, `select_f32`, [
      value("f32", 2),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      0,
    ]),
  [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])],
);

// ./test/core/select.wast:197
assert_return(
  () =>
    invoke($0, `select_f32`, [
      value("f32", 2),
      bytes("f32", [0x4, 0x3, 0x82, 0x7f]),
      0,
    ]),
  [bytes("f32", [0x4, 0x3, 0x82, 0x7f])],
);

// ./test/core/select.wast:199
assert_return(
  () =>
    invoke($0, `select_f64`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
      value("f64", 1),
      1,
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])],
);

// ./test/core/select.wast:200
assert_return(
  () =>
    invoke($0, `select_f64`, [
      bytes("f64", [0x4, 0x3, 0x2, 0x0, 0x0, 0x0, 0xf0, 0x7f]),
      value("f64", 1),
      1,
    ]),
  [bytes("f64", [0x4, 0x3, 0x2, 0x0, 0x0, 0x0, 0xf0, 0x7f])],
);

// ./test/core/select.wast:201
assert_return(
  () =>
    invoke($0, `select_f64`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
      value("f64", 1),
      0,
    ]),
  [value("f64", 1)],
);

// ./test/core/select.wast:202
assert_return(
  () =>
    invoke($0, `select_f64`, [
      bytes("f64", [0x4, 0x3, 0x2, 0x0, 0x0, 0x0, 0xf0, 0x7f]),
      value("f64", 1),
      0,
    ]),
  [value("f64", 1)],
);

// ./test/core/select.wast:203
assert_return(
  () =>
    invoke($0, `select_f64`, [
      value("f64", 2),
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
      1,
    ]),
  [value("f64", 2)],
);

// ./test/core/select.wast:204
assert_return(
  () =>
    invoke($0, `select_f64`, [
      value("f64", 2),
      bytes("f64", [0x4, 0x3, 0x2, 0x0, 0x0, 0x0, 0xf0, 0x7f]),
      1,
    ]),
  [value("f64", 2)],
);

// ./test/core/select.wast:205
assert_return(
  () =>
    invoke($0, `select_f64`, [
      value("f64", 2),
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
      0,
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])],
);

// ./test/core/select.wast:206
assert_return(
  () =>
    invoke($0, `select_f64`, [
      value("f64", 2),
      bytes("f64", [0x4, 0x3, 0x2, 0x0, 0x0, 0x0, 0xf0, 0x7f]),
      0,
    ]),
  [bytes("f64", [0x4, 0x3, 0x2, 0x0, 0x0, 0x0, 0xf0, 0x7f])],
);

// ./test/core/select.wast:208
assert_trap(() => invoke($0, `select_trap_l`, [1]), `unreachable`);

// ./test/core/select.wast:209
assert_trap(() => invoke($0, `select_trap_l`, [0]), `unreachable`);

// ./test/core/select.wast:210
assert_trap(() => invoke($0, `select_trap_r`, [1]), `unreachable`);

// ./test/core/select.wast:211
assert_trap(() => invoke($0, `select_trap_r`, [0]), `unreachable`);

// ./test/core/select.wast:213
assert_return(() => invoke($0, `as-select-first`, [0]), [value("i32", 1)]);

// ./test/core/select.wast:214
assert_return(() => invoke($0, `as-select-first`, [1]), [value("i32", 0)]);

// ./test/core/select.wast:215
assert_return(() => invoke($0, `as-select-mid`, [0]), [value("i32", 2)]);

// ./test/core/select.wast:216
assert_return(() => invoke($0, `as-select-mid`, [1]), [value("i32", 2)]);

// ./test/core/select.wast:217
assert_return(() => invoke($0, `as-select-last`, [0]), [value("i32", 2)]);

// ./test/core/select.wast:218
assert_return(() => invoke($0, `as-select-last`, [1]), [value("i32", 3)]);

// ./test/core/select.wast:220
assert_return(() => invoke($0, `as-loop-first`, [0]), [value("i32", 3)]);

// ./test/core/select.wast:221
assert_return(() => invoke($0, `as-loop-first`, [1]), [value("i32", 2)]);

// ./test/core/select.wast:222
assert_return(() => invoke($0, `as-loop-mid`, [0]), [value("i32", 3)]);

// ./test/core/select.wast:223
assert_return(() => invoke($0, `as-loop-mid`, [1]), [value("i32", 2)]);

// ./test/core/select.wast:224
assert_return(() => invoke($0, `as-loop-last`, [0]), [value("i32", 3)]);

// ./test/core/select.wast:225
assert_return(() => invoke($0, `as-loop-last`, [1]), [value("i32", 2)]);

// ./test/core/select.wast:227
assert_return(() => invoke($0, `as-if-condition`, [0]), []);

// ./test/core/select.wast:228
assert_return(() => invoke($0, `as-if-condition`, [1]), []);

// ./test/core/select.wast:229
assert_return(() => invoke($0, `as-if-then`, [0]), [value("i32", 3)]);

// ./test/core/select.wast:230
assert_return(() => invoke($0, `as-if-then`, [1]), [value("i32", 2)]);

// ./test/core/select.wast:231
assert_return(() => invoke($0, `as-if-else`, [0]), [value("i32", 3)]);

// ./test/core/select.wast:232
assert_return(() => invoke($0, `as-if-else`, [1]), [value("i32", 2)]);

// ./test/core/select.wast:234
assert_return(() => invoke($0, `as-br_if-first`, [0]), [value("i32", 3)]);

// ./test/core/select.wast:235
assert_return(() => invoke($0, `as-br_if-first`, [1]), [value("i32", 2)]);

// ./test/core/select.wast:236
assert_return(() => invoke($0, `as-br_if-last`, [0]), [value("i32", 2)]);

// ./test/core/select.wast:237
assert_return(() => invoke($0, `as-br_if-last`, [1]), [value("i32", 2)]);

// ./test/core/select.wast:239
assert_return(() => invoke($0, `as-br_table-first`, [0]), [value("i32", 3)]);

// ./test/core/select.wast:240
assert_return(() => invoke($0, `as-br_table-first`, [1]), [value("i32", 2)]);

// ./test/core/select.wast:241
assert_return(() => invoke($0, `as-br_table-last`, [0]), [value("i32", 2)]);

// ./test/core/select.wast:242
assert_return(() => invoke($0, `as-br_table-last`, [1]), [value("i32", 2)]);

// ./test/core/select.wast:244
assert_return(() => invoke($0, `as-call_indirect-first`, [0]), [
  value("i32", 3),
]);

// ./test/core/select.wast:245
assert_return(() => invoke($0, `as-call_indirect-first`, [1]), [
  value("i32", 2),
]);

// ./test/core/select.wast:246
assert_return(() => invoke($0, `as-call_indirect-mid`, [0]), [value("i32", 1)]);

// ./test/core/select.wast:247
assert_return(() => invoke($0, `as-call_indirect-mid`, [1]), [value("i32", 1)]);

// ./test/core/select.wast:248
assert_trap(
  () => invoke($0, `as-call_indirect-last`, [0]),
  `undefined element`,
);

// ./test/core/select.wast:249
assert_trap(
  () => invoke($0, `as-call_indirect-last`, [1]),
  `undefined element`,
);

// ./test/core/select.wast:251
assert_return(() => invoke($0, `as-store-first`, [0]), []);

// ./test/core/select.wast:252
assert_return(() => invoke($0, `as-store-first`, [1]), []);

// ./test/core/select.wast:253
assert_return(() => invoke($0, `as-store-last`, [0]), []);

// ./test/core/select.wast:254
assert_return(() => invoke($0, `as-store-last`, [1]), []);

// ./test/core/select.wast:256
assert_return(() => invoke($0, `as-memory.grow-value`, [0]), [value("i32", 1)]);

// ./test/core/select.wast:257
assert_return(() => invoke($0, `as-memory.grow-value`, [1]), [value("i32", 3)]);

// ./test/core/select.wast:259
assert_return(() => invoke($0, `as-call-value`, [0]), [value("i32", 2)]);

// ./test/core/select.wast:260
assert_return(() => invoke($0, `as-call-value`, [1]), [value("i32", 1)]);

// ./test/core/select.wast:261
assert_return(() => invoke($0, `as-return-value`, [0]), [value("i32", 2)]);

// ./test/core/select.wast:262
assert_return(() => invoke($0, `as-return-value`, [1]), [value("i32", 1)]);

// ./test/core/select.wast:263
assert_return(() => invoke($0, `as-drop-operand`, [0]), []);

// ./test/core/select.wast:264
assert_return(() => invoke($0, `as-drop-operand`, [1]), []);

// ./test/core/select.wast:265
assert_return(() => invoke($0, `as-br-value`, [0]), [value("i32", 2)]);

// ./test/core/select.wast:266
assert_return(() => invoke($0, `as-br-value`, [1]), [value("i32", 1)]);

// ./test/core/select.wast:267
assert_return(() => invoke($0, `as-local.set-value`, [0]), [value("i32", 2)]);

// ./test/core/select.wast:268
assert_return(() => invoke($0, `as-local.set-value`, [1]), [value("i32", 1)]);

// ./test/core/select.wast:269
assert_return(() => invoke($0, `as-local.tee-value`, [0]), [value("i32", 2)]);

// ./test/core/select.wast:270
assert_return(() => invoke($0, `as-local.tee-value`, [1]), [value("i32", 1)]);

// ./test/core/select.wast:271
assert_return(() => invoke($0, `as-global.set-value`, [0]), [value("i32", 2)]);

// ./test/core/select.wast:272
assert_return(() => invoke($0, `as-global.set-value`, [1]), [value("i32", 1)]);

// ./test/core/select.wast:273
assert_return(() => invoke($0, `as-load-operand`, [0]), [value("i32", 1)]);

// ./test/core/select.wast:274
assert_return(() => invoke($0, `as-load-operand`, [1]), [value("i32", 1)]);

// ./test/core/select.wast:276
assert_return(() => invoke($0, `as-unary-operand`, [0]), [value("i32", 0)]);

// ./test/core/select.wast:277
assert_return(() => invoke($0, `as-unary-operand`, [1]), [value("i32", 1)]);

// ./test/core/select.wast:278
assert_return(() => invoke($0, `as-binary-operand`, [0]), [value("i32", 4)]);

// ./test/core/select.wast:279
assert_return(() => invoke($0, `as-binary-operand`, [1]), [value("i32", 1)]);

// ./test/core/select.wast:280
assert_return(() => invoke($0, `as-test-operand`, [0]), [value("i32", 0)]);

// ./test/core/select.wast:281
assert_return(() => invoke($0, `as-test-operand`, [1]), [value("i32", 1)]);

// ./test/core/select.wast:282
assert_return(() => invoke($0, `as-compare-left`, [0]), [value("i32", 0)]);

// ./test/core/select.wast:283
assert_return(() => invoke($0, `as-compare-left`, [1]), [value("i32", 1)]);

// ./test/core/select.wast:284
assert_return(() => invoke($0, `as-compare-right`, [0]), [value("i32", 0)]);

// ./test/core/select.wast:285
assert_return(() => invoke($0, `as-compare-right`, [1]), [value("i32", 1)]);

// ./test/core/select.wast:286
assert_return(() => invoke($0, `as-convert-operand`, [0]), [value("i32", 0)]);

// ./test/core/select.wast:287
assert_return(() => invoke($0, `as-convert-operand`, [1]), [value("i32", 1)]);

// ./test/core/select.wast:289
assert_invalid(
  () =>
    instantiate(`(module (func $$arity-0 (select (nop) (nop) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/select.wast:296
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-num-vs-num (select (i32.const 1) (i64.const 1) (i32.const 1))))`,
    ),
  `type mismatch`,
);

// ./test/core/select.wast:300
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-num-vs-num (select (i32.const 1) (f32.const 1.0) (i32.const 1))))`,
    ),
  `type mismatch`,
);

// ./test/core/select.wast:304
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-num-vs-num (select (i32.const 1) (f64.const 1.0) (i32.const 1))))`,
    ),
  `type mismatch`,
);

// ./test/core/select.wast:310
assert_invalid(() =>
  instantiate(`(module
    (func $$type-1st-operand-empty
      (select) (drop)
    )
  )`), `type mismatch`);

// ./test/core/select.wast:318
assert_invalid(() =>
  instantiate(`(module
    (func $$type-2nd-operand-empty
      (i32.const 0) (select) (drop)
    )
  )`), `type mismatch`);

// ./test/core/select.wast:326
assert_invalid(() =>
  instantiate(`(module
    (func $$type-3rd-operand-empty
      (i32.const 0) (i32.const 0) (select) (drop)
    )
  )`), `type mismatch`);

// ./test/core/select.wast:334
assert_invalid(() =>
  instantiate(`(module
    (func $$type-1st-operand-empty-in-block
      (i32.const 0) (i32.const 0) (i32.const 0)
      (block (select) (drop))
    )
  )`), `type mismatch`);

// ./test/core/select.wast:343
assert_invalid(() =>
  instantiate(`(module
    (func $$type-2nd-operand-empty-in-block
      (i32.const 0) (i32.const 0)
      (block (i32.const 0) (select) (drop))
    )
  )`), `type mismatch`);

// ./test/core/select.wast:352
assert_invalid(() =>
  instantiate(`(module
    (func $$type-3rd-operand-empty-in-block
      (i32.const 0)
      (block (i32.const 0) (i32.const 0) (select) (drop))
    )
  )`), `type mismatch`);

// ./test/core/select.wast:361
assert_invalid(() =>
  instantiate(`(module
    (func $$type-1st-operand-empty-in-loop
      (i32.const 0) (i32.const 0) (i32.const 0)
      (loop (select) (drop))
    )
  )`), `type mismatch`);

// ./test/core/select.wast:370
assert_invalid(() =>
  instantiate(`(module
    (func $$type-2nd-operand-empty-in-loop
      (i32.const 0) (i32.const 0)
      (loop (i32.const 0) (select) (drop))
    )
  )`), `type mismatch`);

// ./test/core/select.wast:379
assert_invalid(() =>
  instantiate(`(module
    (func $$type-3rd-operand-empty-in-loop
      (i32.const 0)
      (loop (i32.const 0) (i32.const 0) (select) (drop))
    )
  )`), `type mismatch`);

// ./test/core/select.wast:388
assert_invalid(() =>
  instantiate(`(module
    (func $$type-1st-operand-empty-in-then
      (i32.const 0) (i32.const 0) (i32.const 0)
      (if (then (select) (drop)))
    )
  )`), `type mismatch`);

// ./test/core/select.wast:397
assert_invalid(() =>
  instantiate(`(module
    (func $$type-2nd-operand-empty-in-then
      (i32.const 0) (i32.const 0)
      (if (then (i32.const 0) (select) (drop)))
    )
  )`), `type mismatch`);

// ./test/core/select.wast:406
assert_invalid(() =>
  instantiate(`(module
    (func $$type-3rd-operand-empty-in-then
      (i32.const 0)
      (if (then (i32.const 0) (i32.const 0) (select) (drop)))
    )
  )`), `type mismatch`);
