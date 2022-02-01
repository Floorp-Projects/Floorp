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
  ;; Auxiliary
  (func $$dummy)
  (table $$tab funcref (elem $$dummy))
  (memory 1)

  (func (export "select-i32") (param i32 i32 i32) (result i32)
    (select (local.get 0) (local.get 1) (local.get 2))
  )
  (func (export "select-i64") (param i64 i64 i32) (result i64)
    (select (local.get 0) (local.get 1) (local.get 2))
  )
  (func (export "select-f32") (param f32 f32 i32) (result f32)
    (select (local.get 0) (local.get 1) (local.get 2))
  )
  (func (export "select-f64") (param f64 f64 i32) (result f64)
    (select (local.get 0) (local.get 1) (local.get 2))
  )

  (func (export "select-i32-t") (param i32 i32 i32) (result i32)
    (select (result i32) (local.get 0) (local.get 1) (local.get 2))
  )
  (func (export "select-i64-t") (param i64 i64 i32) (result i64)
    (select (result i64) (local.get 0) (local.get 1) (local.get 2))
  )
  (func (export "select-f32-t") (param f32 f32 i32) (result f32)
    (select (result f32) (local.get 0) (local.get 1) (local.get 2))
  )
  (func (export "select-f64-t") (param f64 f64 i32) (result f64)
    (select (result f64) (local.get 0) (local.get 1) (local.get 2))
  )
  (func (export "select-funcref") (param funcref funcref i32) (result funcref)
    (select (result funcref) (local.get 0) (local.get 1) (local.get 2))
  )
  (func (export "select-externref") (param externref externref i32) (result externref)
    (select (result externref) (local.get 0) (local.get 1) (local.get 2))
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
  (table $$t funcref (elem $$func))
  (func (export "as-call_indirect-first") (param i32) (result i32)
    (block (result i32)
      (call_indirect $$t (type $$check)
        (select (i32.const 2) (i32.const 3) (local.get 0)) (i32.const 1) (i32.const 0)
      )
    )
  )
  (func (export "as-call_indirect-mid") (param i32) (result i32)
    (block (result i32)
      (call_indirect $$t (type $$check)
        (i32.const 1) (select (i32.const 2) (i32.const 3) (local.get 0)) (i32.const 0)
      )
    )
  )
  (func (export "as-call_indirect-last") (param i32) (result i32)
    (block (result i32)
      (call_indirect $$t (type $$check)
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

// ./test/core/select.wast:183
assert_return(() => invoke($0, `select-i32`, [1, 2, 1]), [value("i32", 1)]);

// ./test/core/select.wast:184
assert_return(() => invoke($0, `select-i64`, [2n, 1n, 1]), [value("i64", 2n)]);

// ./test/core/select.wast:185
assert_return(
  () => invoke($0, `select-f32`, [value("f32", 1), value("f32", 2), 1]),
  [value("f32", 1)],
);

// ./test/core/select.wast:186
assert_return(
  () => invoke($0, `select-f64`, [value("f64", 1), value("f64", 2), 1]),
  [value("f64", 1)],
);

// ./test/core/select.wast:188
assert_return(() => invoke($0, `select-i32`, [1, 2, 0]), [value("i32", 2)]);

// ./test/core/select.wast:189
assert_return(() => invoke($0, `select-i32`, [2, 1, 0]), [value("i32", 1)]);

// ./test/core/select.wast:190
assert_return(() => invoke($0, `select-i64`, [2n, 1n, -1]), [value("i64", 2n)]);

// ./test/core/select.wast:191
assert_return(() => invoke($0, `select-i64`, [2n, 1n, -252645136]), [
  value("i64", 2n),
]);

// ./test/core/select.wast:193
assert_return(
  () =>
    invoke($0, `select-f32`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 1),
      1,
    ]),
  [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])],
);

// ./test/core/select.wast:194
assert_return(
  () =>
    invoke($0, `select-f32`, [
      bytes("f32", [0x4, 0x3, 0x82, 0x7f]),
      value("f32", 1),
      1,
    ]),
  [bytes("f32", [0x4, 0x3, 0x82, 0x7f])],
);

// ./test/core/select.wast:195
assert_return(
  () =>
    invoke($0, `select-f32`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 1),
      0,
    ]),
  [value("f32", 1)],
);

// ./test/core/select.wast:196
assert_return(
  () =>
    invoke($0, `select-f32`, [
      bytes("f32", [0x4, 0x3, 0x82, 0x7f]),
      value("f32", 1),
      0,
    ]),
  [value("f32", 1)],
);

// ./test/core/select.wast:197
assert_return(
  () =>
    invoke($0, `select-f32`, [
      value("f32", 2),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      1,
    ]),
  [value("f32", 2)],
);

// ./test/core/select.wast:198
assert_return(
  () =>
    invoke($0, `select-f32`, [
      value("f32", 2),
      bytes("f32", [0x4, 0x3, 0x82, 0x7f]),
      1,
    ]),
  [value("f32", 2)],
);

// ./test/core/select.wast:199
assert_return(
  () =>
    invoke($0, `select-f32`, [
      value("f32", 2),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      0,
    ]),
  [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])],
);

// ./test/core/select.wast:200
assert_return(
  () =>
    invoke($0, `select-f32`, [
      value("f32", 2),
      bytes("f32", [0x4, 0x3, 0x82, 0x7f]),
      0,
    ]),
  [bytes("f32", [0x4, 0x3, 0x82, 0x7f])],
);

// ./test/core/select.wast:202
assert_return(
  () =>
    invoke($0, `select-f64`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
      value("f64", 1),
      1,
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])],
);

// ./test/core/select.wast:203
assert_return(
  () =>
    invoke($0, `select-f64`, [
      bytes("f64", [0x4, 0x3, 0x2, 0x0, 0x0, 0x0, 0xf0, 0x7f]),
      value("f64", 1),
      1,
    ]),
  [bytes("f64", [0x4, 0x3, 0x2, 0x0, 0x0, 0x0, 0xf0, 0x7f])],
);

// ./test/core/select.wast:204
assert_return(
  () =>
    invoke($0, `select-f64`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
      value("f64", 1),
      0,
    ]),
  [value("f64", 1)],
);

// ./test/core/select.wast:205
assert_return(
  () =>
    invoke($0, `select-f64`, [
      bytes("f64", [0x4, 0x3, 0x2, 0x0, 0x0, 0x0, 0xf0, 0x7f]),
      value("f64", 1),
      0,
    ]),
  [value("f64", 1)],
);

// ./test/core/select.wast:206
assert_return(
  () =>
    invoke($0, `select-f64`, [
      value("f64", 2),
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
      1,
    ]),
  [value("f64", 2)],
);

// ./test/core/select.wast:207
assert_return(
  () =>
    invoke($0, `select-f64`, [
      value("f64", 2),
      bytes("f64", [0x4, 0x3, 0x2, 0x0, 0x0, 0x0, 0xf0, 0x7f]),
      1,
    ]),
  [value("f64", 2)],
);

// ./test/core/select.wast:208
assert_return(
  () =>
    invoke($0, `select-f64`, [
      value("f64", 2),
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
      0,
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])],
);

// ./test/core/select.wast:209
assert_return(
  () =>
    invoke($0, `select-f64`, [
      value("f64", 2),
      bytes("f64", [0x4, 0x3, 0x2, 0x0, 0x0, 0x0, 0xf0, 0x7f]),
      0,
    ]),
  [bytes("f64", [0x4, 0x3, 0x2, 0x0, 0x0, 0x0, 0xf0, 0x7f])],
);

// ./test/core/select.wast:211
assert_return(() => invoke($0, `select-i32-t`, [1, 2, 1]), [value("i32", 1)]);

// ./test/core/select.wast:212
assert_return(() => invoke($0, `select-i64-t`, [2n, 1n, 1]), [
  value("i64", 2n),
]);

// ./test/core/select.wast:213
assert_return(
  () => invoke($0, `select-f32-t`, [value("f32", 1), value("f32", 2), 1]),
  [value("f32", 1)],
);

// ./test/core/select.wast:214
assert_return(
  () => invoke($0, `select-f64-t`, [value("f64", 1), value("f64", 2), 1]),
  [value("f64", 1)],
);

// ./test/core/select.wast:215
assert_return(() => invoke($0, `select-funcref`, [null, null, 1]), [
  value("anyfunc", null),
]);

// ./test/core/select.wast:216
assert_return(
  () => invoke($0, `select-externref`, [externref(1), externref(2), 1]),
  [value("externref", externref(1))],
);

// ./test/core/select.wast:218
assert_return(() => invoke($0, `select-i32-t`, [1, 2, 0]), [value("i32", 2)]);

// ./test/core/select.wast:219
assert_return(() => invoke($0, `select-i32-t`, [2, 1, 0]), [value("i32", 1)]);

// ./test/core/select.wast:220
assert_return(() => invoke($0, `select-i64-t`, [2n, 1n, -1]), [
  value("i64", 2n),
]);

// ./test/core/select.wast:221
assert_return(() => invoke($0, `select-i64-t`, [2n, 1n, -252645136]), [
  value("i64", 2n),
]);

// ./test/core/select.wast:222
assert_return(
  () => invoke($0, `select-externref`, [externref(1), externref(2), 0]),
  [value("externref", externref(2))],
);

// ./test/core/select.wast:223
assert_return(
  () => invoke($0, `select-externref`, [externref(2), externref(1), 0]),
  [value("externref", externref(1))],
);

// ./test/core/select.wast:225
assert_return(
  () =>
    invoke($0, `select-f32-t`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 1),
      1,
    ]),
  [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])],
);

// ./test/core/select.wast:226
assert_return(
  () =>
    invoke($0, `select-f32-t`, [
      bytes("f32", [0x4, 0x3, 0x82, 0x7f]),
      value("f32", 1),
      1,
    ]),
  [bytes("f32", [0x4, 0x3, 0x82, 0x7f])],
);

// ./test/core/select.wast:227
assert_return(
  () =>
    invoke($0, `select-f32-t`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 1),
      0,
    ]),
  [value("f32", 1)],
);

// ./test/core/select.wast:228
assert_return(
  () =>
    invoke($0, `select-f32-t`, [
      bytes("f32", [0x4, 0x3, 0x82, 0x7f]),
      value("f32", 1),
      0,
    ]),
  [value("f32", 1)],
);

// ./test/core/select.wast:229
assert_return(
  () =>
    invoke($0, `select-f32-t`, [
      value("f32", 2),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      1,
    ]),
  [value("f32", 2)],
);

// ./test/core/select.wast:230
assert_return(
  () =>
    invoke($0, `select-f32-t`, [
      value("f32", 2),
      bytes("f32", [0x4, 0x3, 0x82, 0x7f]),
      1,
    ]),
  [value("f32", 2)],
);

// ./test/core/select.wast:231
assert_return(
  () =>
    invoke($0, `select-f32-t`, [
      value("f32", 2),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      0,
    ]),
  [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])],
);

// ./test/core/select.wast:232
assert_return(
  () =>
    invoke($0, `select-f32-t`, [
      value("f32", 2),
      bytes("f32", [0x4, 0x3, 0x82, 0x7f]),
      0,
    ]),
  [bytes("f32", [0x4, 0x3, 0x82, 0x7f])],
);

// ./test/core/select.wast:234
assert_return(
  () =>
    invoke($0, `select-f64-t`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
      value("f64", 1),
      1,
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])],
);

// ./test/core/select.wast:235
assert_return(
  () =>
    invoke($0, `select-f64-t`, [
      bytes("f64", [0x4, 0x3, 0x2, 0x0, 0x0, 0x0, 0xf0, 0x7f]),
      value("f64", 1),
      1,
    ]),
  [bytes("f64", [0x4, 0x3, 0x2, 0x0, 0x0, 0x0, 0xf0, 0x7f])],
);

// ./test/core/select.wast:236
assert_return(
  () =>
    invoke($0, `select-f64-t`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
      value("f64", 1),
      0,
    ]),
  [value("f64", 1)],
);

// ./test/core/select.wast:237
assert_return(
  () =>
    invoke($0, `select-f64-t`, [
      bytes("f64", [0x4, 0x3, 0x2, 0x0, 0x0, 0x0, 0xf0, 0x7f]),
      value("f64", 1),
      0,
    ]),
  [value("f64", 1)],
);

// ./test/core/select.wast:238
assert_return(
  () =>
    invoke($0, `select-f64-t`, [
      value("f64", 2),
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
      1,
    ]),
  [value("f64", 2)],
);

// ./test/core/select.wast:239
assert_return(
  () =>
    invoke($0, `select-f64-t`, [
      value("f64", 2),
      bytes("f64", [0x4, 0x3, 0x2, 0x0, 0x0, 0x0, 0xf0, 0x7f]),
      1,
    ]),
  [value("f64", 2)],
);

// ./test/core/select.wast:240
assert_return(
  () =>
    invoke($0, `select-f64-t`, [
      value("f64", 2),
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
      0,
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])],
);

// ./test/core/select.wast:241
assert_return(
  () =>
    invoke($0, `select-f64-t`, [
      value("f64", 2),
      bytes("f64", [0x4, 0x3, 0x2, 0x0, 0x0, 0x0, 0xf0, 0x7f]),
      0,
    ]),
  [bytes("f64", [0x4, 0x3, 0x2, 0x0, 0x0, 0x0, 0xf0, 0x7f])],
);

// ./test/core/select.wast:243
assert_return(() => invoke($0, `as-select-first`, [0]), [value("i32", 1)]);

// ./test/core/select.wast:244
assert_return(() => invoke($0, `as-select-first`, [1]), [value("i32", 0)]);

// ./test/core/select.wast:245
assert_return(() => invoke($0, `as-select-mid`, [0]), [value("i32", 2)]);

// ./test/core/select.wast:246
assert_return(() => invoke($0, `as-select-mid`, [1]), [value("i32", 2)]);

// ./test/core/select.wast:247
assert_return(() => invoke($0, `as-select-last`, [0]), [value("i32", 2)]);

// ./test/core/select.wast:248
assert_return(() => invoke($0, `as-select-last`, [1]), [value("i32", 3)]);

// ./test/core/select.wast:250
assert_return(() => invoke($0, `as-loop-first`, [0]), [value("i32", 3)]);

// ./test/core/select.wast:251
assert_return(() => invoke($0, `as-loop-first`, [1]), [value("i32", 2)]);

// ./test/core/select.wast:252
assert_return(() => invoke($0, `as-loop-mid`, [0]), [value("i32", 3)]);

// ./test/core/select.wast:253
assert_return(() => invoke($0, `as-loop-mid`, [1]), [value("i32", 2)]);

// ./test/core/select.wast:254
assert_return(() => invoke($0, `as-loop-last`, [0]), [value("i32", 3)]);

// ./test/core/select.wast:255
assert_return(() => invoke($0, `as-loop-last`, [1]), [value("i32", 2)]);

// ./test/core/select.wast:257
assert_return(() => invoke($0, `as-if-condition`, [0]), []);

// ./test/core/select.wast:258
assert_return(() => invoke($0, `as-if-condition`, [1]), []);

// ./test/core/select.wast:259
assert_return(() => invoke($0, `as-if-then`, [0]), [value("i32", 3)]);

// ./test/core/select.wast:260
assert_return(() => invoke($0, `as-if-then`, [1]), [value("i32", 2)]);

// ./test/core/select.wast:261
assert_return(() => invoke($0, `as-if-else`, [0]), [value("i32", 3)]);

// ./test/core/select.wast:262
assert_return(() => invoke($0, `as-if-else`, [1]), [value("i32", 2)]);

// ./test/core/select.wast:264
assert_return(() => invoke($0, `as-br_if-first`, [0]), [value("i32", 3)]);

// ./test/core/select.wast:265
assert_return(() => invoke($0, `as-br_if-first`, [1]), [value("i32", 2)]);

// ./test/core/select.wast:266
assert_return(() => invoke($0, `as-br_if-last`, [0]), [value("i32", 2)]);

// ./test/core/select.wast:267
assert_return(() => invoke($0, `as-br_if-last`, [1]), [value("i32", 2)]);

// ./test/core/select.wast:269
assert_return(() => invoke($0, `as-br_table-first`, [0]), [value("i32", 3)]);

// ./test/core/select.wast:270
assert_return(() => invoke($0, `as-br_table-first`, [1]), [value("i32", 2)]);

// ./test/core/select.wast:271
assert_return(() => invoke($0, `as-br_table-last`, [0]), [value("i32", 2)]);

// ./test/core/select.wast:272
assert_return(() => invoke($0, `as-br_table-last`, [1]), [value("i32", 2)]);

// ./test/core/select.wast:274
assert_return(() => invoke($0, `as-call_indirect-first`, [0]), [
  value("i32", 3),
]);

// ./test/core/select.wast:275
assert_return(() => invoke($0, `as-call_indirect-first`, [1]), [
  value("i32", 2),
]);

// ./test/core/select.wast:276
assert_return(() => invoke($0, `as-call_indirect-mid`, [0]), [value("i32", 1)]);

// ./test/core/select.wast:277
assert_return(() => invoke($0, `as-call_indirect-mid`, [1]), [value("i32", 1)]);

// ./test/core/select.wast:278
assert_trap(
  () => invoke($0, `as-call_indirect-last`, [0]),
  `undefined element`,
);

// ./test/core/select.wast:279
assert_trap(
  () => invoke($0, `as-call_indirect-last`, [1]),
  `undefined element`,
);

// ./test/core/select.wast:281
assert_return(() => invoke($0, `as-store-first`, [0]), []);

// ./test/core/select.wast:282
assert_return(() => invoke($0, `as-store-first`, [1]), []);

// ./test/core/select.wast:283
assert_return(() => invoke($0, `as-store-last`, [0]), []);

// ./test/core/select.wast:284
assert_return(() => invoke($0, `as-store-last`, [1]), []);

// ./test/core/select.wast:286
assert_return(() => invoke($0, `as-memory.grow-value`, [0]), [value("i32", 1)]);

// ./test/core/select.wast:287
assert_return(() => invoke($0, `as-memory.grow-value`, [1]), [value("i32", 3)]);

// ./test/core/select.wast:289
assert_return(() => invoke($0, `as-call-value`, [0]), [value("i32", 2)]);

// ./test/core/select.wast:290
assert_return(() => invoke($0, `as-call-value`, [1]), [value("i32", 1)]);

// ./test/core/select.wast:291
assert_return(() => invoke($0, `as-return-value`, [0]), [value("i32", 2)]);

// ./test/core/select.wast:292
assert_return(() => invoke($0, `as-return-value`, [1]), [value("i32", 1)]);

// ./test/core/select.wast:293
assert_return(() => invoke($0, `as-drop-operand`, [0]), []);

// ./test/core/select.wast:294
assert_return(() => invoke($0, `as-drop-operand`, [1]), []);

// ./test/core/select.wast:295
assert_return(() => invoke($0, `as-br-value`, [0]), [value("i32", 2)]);

// ./test/core/select.wast:296
assert_return(() => invoke($0, `as-br-value`, [1]), [value("i32", 1)]);

// ./test/core/select.wast:297
assert_return(() => invoke($0, `as-local.set-value`, [0]), [value("i32", 2)]);

// ./test/core/select.wast:298
assert_return(() => invoke($0, `as-local.set-value`, [1]), [value("i32", 1)]);

// ./test/core/select.wast:299
assert_return(() => invoke($0, `as-local.tee-value`, [0]), [value("i32", 2)]);

// ./test/core/select.wast:300
assert_return(() => invoke($0, `as-local.tee-value`, [1]), [value("i32", 1)]);

// ./test/core/select.wast:301
assert_return(() => invoke($0, `as-global.set-value`, [0]), [value("i32", 2)]);

// ./test/core/select.wast:302
assert_return(() => invoke($0, `as-global.set-value`, [1]), [value("i32", 1)]);

// ./test/core/select.wast:303
assert_return(() => invoke($0, `as-load-operand`, [0]), [value("i32", 1)]);

// ./test/core/select.wast:304
assert_return(() => invoke($0, `as-load-operand`, [1]), [value("i32", 1)]);

// ./test/core/select.wast:306
assert_return(() => invoke($0, `as-unary-operand`, [0]), [value("i32", 0)]);

// ./test/core/select.wast:307
assert_return(() => invoke($0, `as-unary-operand`, [1]), [value("i32", 1)]);

// ./test/core/select.wast:308
assert_return(() => invoke($0, `as-binary-operand`, [0]), [value("i32", 4)]);

// ./test/core/select.wast:309
assert_return(() => invoke($0, `as-binary-operand`, [1]), [value("i32", 1)]);

// ./test/core/select.wast:310
assert_return(() => invoke($0, `as-test-operand`, [0]), [value("i32", 0)]);

// ./test/core/select.wast:311
assert_return(() => invoke($0, `as-test-operand`, [1]), [value("i32", 1)]);

// ./test/core/select.wast:312
assert_return(() => invoke($0, `as-compare-left`, [0]), [value("i32", 0)]);

// ./test/core/select.wast:313
assert_return(() => invoke($0, `as-compare-left`, [1]), [value("i32", 1)]);

// ./test/core/select.wast:314
assert_return(() => invoke($0, `as-compare-right`, [0]), [value("i32", 0)]);

// ./test/core/select.wast:315
assert_return(() => invoke($0, `as-compare-right`, [1]), [value("i32", 1)]);

// ./test/core/select.wast:316
assert_return(() => invoke($0, `as-convert-operand`, [0]), [value("i32", 0)]);

// ./test/core/select.wast:317
assert_return(() => invoke($0, `as-convert-operand`, [1]), [value("i32", 1)]);

// ./test/core/select.wast:319
assert_invalid(
  () =>
    instantiate(
      `(module (func $$arity-0-implicit (select (nop) (nop) (i32.const 1))))`,
    ),
  `type mismatch`,
);

// ./test/core/select.wast:323
assert_invalid(
  () =>
    instantiate(
      `(module (func $$arity-0 (select (result) (nop) (nop) (i32.const 1))))`,
    ),
  `invalid result arity`,
);

// ./test/core/select.wast:327
assert_invalid(() =>
  instantiate(`(module (func $$arity-2 (result i32 i32)
    (select (result i32 i32)
      (i32.const 0) (i32.const 0)
      (i32.const 0) (i32.const 0)
      (i32.const 1)
    )
  ))`), `invalid result arity`);

// ./test/core/select.wast:339
assert_invalid(
  () =>
    instantiate(`(module (func $$type-externref-implicit (param $$r externref)
    (drop (select (local.get $$r) (local.get $$r) (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/select.wast:346
assert_invalid(() =>
  instantiate(`(module (func $$type-num-vs-num
    (drop (select (i32.const 1) (i64.const 1) (i32.const 1)))
  ))`), `type mismatch`);

// ./test/core/select.wast:352
assert_invalid(() =>
  instantiate(`(module (func $$type-num-vs-num
    (drop (select (i32.const 1) (f32.const 1.0) (i32.const 1)))
  ))`), `type mismatch`);

// ./test/core/select.wast:358
assert_invalid(() =>
  instantiate(`(module (func $$type-num-vs-num
    (drop (select (i32.const 1) (f64.const 1.0) (i32.const 1)))
  ))`), `type mismatch`);

// ./test/core/select.wast:365
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-num-vs-num (select (i32.const 1) (i64.const 1) (i32.const 1)) (drop)))`,
    ),
  `type mismatch`,
);

// ./test/core/select.wast:369
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-num-vs-num (select (i32.const 1) (f32.const 1.0) (i32.const 1)) (drop)))`,
    ),
  `type mismatch`,
);

// ./test/core/select.wast:373
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-num-vs-num (select (i32.const 1) (i64.const 1) (i32.const 1)) (drop)))`,
    ),
  `type mismatch`,
);

// ./test/core/select.wast:377
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-num-vs-num (select (i32.const 1) (f32.const 1.0) (i32.const 1)) (drop)))`,
    ),
  `type mismatch`,
);

// ./test/core/select.wast:381
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-num-vs-num (select (i32.const 1) (f64.const 1.0) (i32.const 1)) (drop)))`,
    ),
  `type mismatch`,
);

// ./test/core/select.wast:387
assert_invalid(() =>
  instantiate(`(module
    (func $$type-1st-operand-empty
      (select) (drop)
    )
  )`), `type mismatch`);

// ./test/core/select.wast:395
assert_invalid(() =>
  instantiate(`(module
    (func $$type-2nd-operand-empty
      (i32.const 0) (select) (drop)
    )
  )`), `type mismatch`);

// ./test/core/select.wast:403
assert_invalid(() =>
  instantiate(`(module
    (func $$type-3rd-operand-empty
      (i32.const 0) (i32.const 0) (select) (drop)
    )
  )`), `type mismatch`);

// ./test/core/select.wast:411
assert_invalid(() =>
  instantiate(`(module
    (func $$type-1st-operand-empty-in-block
      (i32.const 0) (i32.const 0) (i32.const 0)
      (block (select) (drop))
    )
  )`), `type mismatch`);

// ./test/core/select.wast:420
assert_invalid(() =>
  instantiate(`(module
    (func $$type-2nd-operand-empty-in-block
      (i32.const 0) (i32.const 0)
      (block (i32.const 0) (select) (drop))
    )
  )`), `type mismatch`);

// ./test/core/select.wast:429
assert_invalid(() =>
  instantiate(`(module
    (func $$type-3rd-operand-empty-in-block
      (i32.const 0)
      (block (i32.const 0) (i32.const 0) (select) (drop))
    )
  )`), `type mismatch`);

// ./test/core/select.wast:438
assert_invalid(() =>
  instantiate(`(module
    (func $$type-1st-operand-empty-in-loop
      (i32.const 0) (i32.const 0) (i32.const 0)
      (loop (select) (drop))
    )
  )`), `type mismatch`);

// ./test/core/select.wast:447
assert_invalid(() =>
  instantiate(`(module
    (func $$type-2nd-operand-empty-in-loop
      (i32.const 0) (i32.const 0)
      (loop (i32.const 0) (select) (drop))
    )
  )`), `type mismatch`);

// ./test/core/select.wast:456
assert_invalid(() =>
  instantiate(`(module
    (func $$type-3rd-operand-empty-in-loop
      (i32.const 0)
      (loop (i32.const 0) (i32.const 0) (select) (drop))
    )
  )`), `type mismatch`);

// ./test/core/select.wast:465
assert_invalid(() =>
  instantiate(`(module
    (func $$type-1st-operand-empty-in-then
      (i32.const 0) (i32.const 0) (i32.const 0)
      (if (then (select) (drop)))
    )
  )`), `type mismatch`);

// ./test/core/select.wast:474
assert_invalid(() =>
  instantiate(`(module
    (func $$type-2nd-operand-empty-in-then
      (i32.const 0) (i32.const 0)
      (if (then (i32.const 0) (select) (drop)))
    )
  )`), `type mismatch`);

// ./test/core/select.wast:483
assert_invalid(() =>
  instantiate(`(module
    (func $$type-3rd-operand-empty-in-then
      (i32.const 0)
      (if (then (i32.const 0) (i32.const 0) (select) (drop)))
    )
  )`), `type mismatch`);

// ./test/core/select.wast:495
assert_invalid(
  () =>
    instantiate(
      `(module (func (select (i32.const 1) (i32.const 1) (i64.const 1)) (drop)))`,
    ),
  `type mismatch`,
);

// ./test/core/select.wast:499
assert_invalid(
  () =>
    instantiate(
      `(module (func (select (i32.const 1) (i32.const 1) (f32.const 1)) (drop)))`,
    ),
  `type mismatch`,
);

// ./test/core/select.wast:503
assert_invalid(
  () =>
    instantiate(
      `(module (func (select (i32.const 1) (i32.const 1) (f64.const 1)) (drop)))`,
    ),
  `type mismatch`,
);

// ./test/core/select.wast:510
assert_invalid(
  () =>
    instantiate(
      `(module (func (result i32) (select (i64.const 1) (i64.const 1) (i32.const 1))))`,
    ),
  `type mismatch`,
);
