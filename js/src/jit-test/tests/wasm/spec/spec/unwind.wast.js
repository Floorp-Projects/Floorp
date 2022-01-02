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

// ./test/core/unwind.wast

// ./test/core/unwind.wast:3
let $0 = instantiate(`(module
  (func (export "func-unwind-by-unreachable")
    (i32.const 3) (i64.const 1) (unreachable)
  )
  (func (export "func-unwind-by-br")
    (i32.const 3) (i64.const 1) (br 0)
  )
  (func (export "func-unwind-by-br-value") (result i32)
    (i32.const 3) (i64.const 1) (br 0 (i32.const 9))
  )
  (func (export "func-unwind-by-br_if")
    (i32.const 3) (i64.const 1) (drop (drop (br_if 0 (i32.const 1))))
  )
  (func (export "func-unwind-by-br_if-value") (result i32)
    (i32.const 3) (i64.const 1) (drop (drop (br_if 0 (i32.const 9) (i32.const 1))))
  )
  (func (export "func-unwind-by-br_table")
    (i32.const 3) (i64.const 1) (br_table 0 (i32.const 0))
  )
  (func (export "func-unwind-by-br_table-value") (result i32)
    (i32.const 3) (i64.const 1) (br_table 0 (i32.const 9) (i32.const 0))
  )
  (func (export "func-unwind-by-return") (result i32)
    (i32.const 3) (i64.const 1) (return (i32.const 9))
  )

  (func (export "block-unwind-by-unreachable")
    (block (i32.const 3) (i64.const 1) (unreachable))
  )
  (func (export "block-unwind-by-br") (result i32)
    (block (i32.const 3) (i64.const 1) (br 0)) (i32.const 9)
  )
  (func (export "block-unwind-by-br-value") (result i32)
    (block (result i32) (i32.const 3) (i64.const 1) (br 0 (i32.const 9)))
  )
  (func (export "block-unwind-by-br_if") (result i32)
    (block (i32.const 3) (i64.const 1) (drop (drop (br_if 0 (i32.const 1))))) (i32.const 9)
  )
  (func (export "block-unwind-by-br_if-value") (result i32)
    (block (result i32)
      (i32.const 3) (i64.const 1) (drop (drop (br_if 0 (i32.const 9) (i32.const 1))))
    )
  )
  (func (export "block-unwind-by-br_table") (result i32)
    (block (i32.const 3) (i64.const 1) (br_table 0 (i32.const 0))) (i32.const 9)
  )
  (func (export "block-unwind-by-br_table-value") (result i32)
    (block (result i32)
      (i32.const 3) (i64.const 1) (br_table 0 (i32.const 9) (i32.const 0))
    )
  )
  (func (export "block-unwind-by-return") (result i32)
    (block (result i32) (i32.const 3) (i64.const 1) (return (i32.const 9)))
  )

  (func (export "block-nested-unwind-by-unreachable") (result i32)
    (block (result i32) (i32.const 3) (block (i64.const 1) (unreachable)))
  )
  (func (export "block-nested-unwind-by-br") (result i32)
    (block (i32.const 3) (block (i64.const 1) (br 1)) (drop)) (i32.const 9)
  )
  (func (export "block-nested-unwind-by-br-value") (result i32)
    (block (result i32)
      (i32.const 3) (block (i64.const 1) (br 1 (i32.const 9)))
    )
  )
  (func (export "block-nested-unwind-by-br_if") (result i32)
    (block (i32.const 3) (block (i64.const 1) (drop (br_if 1 (i32.const 1)))) (drop)) (i32.const 9)
  )
  (func (export "block-nested-unwind-by-br_if-value") (result i32)
    (block (result i32)
      (i32.const 3) (block (i64.const 1) (drop (drop (br_if 1 (i32.const 9) (i32.const 1)))))
    )
  )
  (func (export "block-nested-unwind-by-br_table") (result i32)
    (block
      (i32.const 3) (block (i64.const 1) (br_table 1 (i32.const 1)))
      (drop)
    )
    (i32.const 9)
  )
  (func (export "block-nested-unwind-by-br_table-value") (result i32)
    (block (result i32)
      (i32.const 3)
      (block (i64.const 1) (br_table 1 (i32.const 9) (i32.const 1)))
    )
  )
  (func (export "block-nested-unwind-by-return") (result i32)
    (block (result i32)
      (i32.const 3) (block (i64.const 1) (return (i32.const 9)))
    )
  )

  (func (export "unary-after-unreachable") (result i32)
    (f32.const 0) (unreachable) (i64.eqz)
  )
  (func (export "unary-after-br") (result i32)
    (block (result i32) (f32.const 0) (br 0 (i32.const 9)) (i64.eqz))
  )
  (func (export "unary-after-br_if") (result i32)
    (block (result i32)
      (i64.const 0) (drop (br_if 0 (i32.const 9) (i32.const 1))) (i64.eqz)
    )
  )
  (func (export "unary-after-br_table") (result i32)
    (block (result i32)
      (f32.const 0) (br_table 0 0 (i32.const 9) (i32.const 0)) (i64.eqz)
    )
  )
  (func (export "unary-after-return") (result i32)
    (f32.const 0) (return (i32.const 9)) (i64.eqz)
  )

  (func (export "binary-after-unreachable") (result i32)
    (f32.const 0) (f64.const 1) (unreachable) (i64.eq)
  )
  (func (export "binary-after-br") (result i32)
    (block (result i32)
      (f32.const 0) (f64.const 1) (br 0 (i32.const 9)) (i64.eq)
    )
  )
  (func (export "binary-after-br_if") (result i32)
    (block (result i32)
      (i64.const 0) (i64.const 1) (drop (br_if 0 (i32.const 9) (i32.const 1)))
      (i64.eq)
    )
  )
  (func (export "binary-after-br_table") (result i32)
    (block (result i32)
      (f32.const 0) (f64.const 1) (br_table 0 (i32.const 9) (i32.const 0))
      (i64.eq)
    )
  )
  (func (export "binary-after-return") (result i32)
    (f32.const 0) (f64.const 1) (return (i32.const 9)) (i64.eq)
  )

  (func (export "select-after-unreachable") (result i32)
    (f32.const 0) (f64.const 1) (i64.const 0) (unreachable) (select)
  )
  (func (export "select-after-br") (result i32)
    (block (result i32)
      (f32.const 0) (f64.const 1) (i64.const 0) (br 0 (i32.const 9)) (select)
    )
  )
  (func (export "select-after-br_if") (result i32)
    (block (result i32)
      (i32.const 0) (i32.const 1) (i32.const 0)
      (drop (br_if 0 (i32.const 9) (i32.const 1)))
      (select)
    )
  )
  (func (export "select-after-br_table") (result i32)
    (block (result i32)
      (f32.const 0) (f64.const 1) (i64.const 0)
      (br_table 0 (i32.const 9) (i32.const 0))
      (select)
    )
  )
  (func (export "select-after-return") (result i32)
    (f32.const 0) (f64.const 1) (i64.const 1) (return (i32.const 9)) (select)
  )

  (func (export "block-value-after-unreachable") (result i32)
    (block (result i32) (f32.const 0) (unreachable))
  )
  (func (export "block-value-after-br") (result i32)
    (block (result i32) (f32.const 0) (br 0 (i32.const 9)))
  )
  (func (export "block-value-after-br_if") (result i32)
    (block (result i32)
      (i32.const 0) (drop (br_if 0 (i32.const 9) (i32.const 1)))
    )
  )
  (func (export "block-value-after-br_table") (result i32)
    (block (result i32)
      (f32.const 0) (br_table 0 0 (i32.const 9) (i32.const 0))
    )
  )
  (func (export "block-value-after-return") (result i32)
    (block (result i32) (f32.const 0) (return (i32.const 9)))
  )

  (func (export "loop-value-after-unreachable") (result i32)
    (loop (result i32) (f32.const 0) (unreachable))
  )
  (func (export "loop-value-after-br") (result i32)
    (block (result i32) (loop (result i32) (f32.const 0) (br 1 (i32.const 9))))
  )
  (func (export "loop-value-after-br_if") (result i32)
    (block (result i32)
      (loop (result i32)
        (i32.const 0) (drop (br_if 1 (i32.const 9) (i32.const 1)))
      )
    )
  )

  (func (export "loop-value-after-br_table") (result i32)
    (block (result i32)
      (loop (result i32)
        (f32.const 0) (br_table 1 1 (i32.const 9) (i32.const 0))
      )
    )
  )
  (func (export "loop-value-after-return") (result i32)
    (loop (result i32) (f32.const 0) (return (i32.const 9)))
  )
)`);

// ./test/core/unwind.wast:212
assert_trap(() => invoke($0, `func-unwind-by-unreachable`, []), `unreachable`);

// ./test/core/unwind.wast:213
assert_return(() => invoke($0, `func-unwind-by-br`, []), []);

// ./test/core/unwind.wast:214
assert_return(() => invoke($0, `func-unwind-by-br-value`, []), [
  value("i32", 9),
]);

// ./test/core/unwind.wast:215
assert_return(() => invoke($0, `func-unwind-by-br_if`, []), []);

// ./test/core/unwind.wast:216
assert_return(() => invoke($0, `func-unwind-by-br_if-value`, []), [
  value("i32", 9),
]);

// ./test/core/unwind.wast:217
assert_return(() => invoke($0, `func-unwind-by-br_table`, []), []);

// ./test/core/unwind.wast:218
assert_return(() => invoke($0, `func-unwind-by-br_table-value`, []), [
  value("i32", 9),
]);

// ./test/core/unwind.wast:219
assert_return(() => invoke($0, `func-unwind-by-return`, []), [value("i32", 9)]);

// ./test/core/unwind.wast:221
assert_trap(() => invoke($0, `block-unwind-by-unreachable`, []), `unreachable`);

// ./test/core/unwind.wast:222
assert_return(() => invoke($0, `block-unwind-by-br`, []), [value("i32", 9)]);

// ./test/core/unwind.wast:223
assert_return(() => invoke($0, `block-unwind-by-br-value`, []), [
  value("i32", 9),
]);

// ./test/core/unwind.wast:224
assert_return(() => invoke($0, `block-unwind-by-br_if`, []), [value("i32", 9)]);

// ./test/core/unwind.wast:225
assert_return(() => invoke($0, `block-unwind-by-br_if-value`, []), [
  value("i32", 9),
]);

// ./test/core/unwind.wast:226
assert_return(() => invoke($0, `block-unwind-by-br_table`, []), [
  value("i32", 9),
]);

// ./test/core/unwind.wast:227
assert_return(() => invoke($0, `block-unwind-by-br_table-value`, []), [
  value("i32", 9),
]);

// ./test/core/unwind.wast:228
assert_return(() => invoke($0, `block-unwind-by-return`, []), [
  value("i32", 9),
]);

// ./test/core/unwind.wast:230
assert_trap(
  () => invoke($0, `block-nested-unwind-by-unreachable`, []),
  `unreachable`,
);

// ./test/core/unwind.wast:231
assert_return(() => invoke($0, `block-nested-unwind-by-br`, []), [
  value("i32", 9),
]);

// ./test/core/unwind.wast:232
assert_return(() => invoke($0, `block-nested-unwind-by-br-value`, []), [
  value("i32", 9),
]);

// ./test/core/unwind.wast:233
assert_return(() => invoke($0, `block-nested-unwind-by-br_if`, []), [
  value("i32", 9),
]);

// ./test/core/unwind.wast:234
assert_return(() => invoke($0, `block-nested-unwind-by-br_if-value`, []), [
  value("i32", 9),
]);

// ./test/core/unwind.wast:235
assert_return(() => invoke($0, `block-nested-unwind-by-br_table`, []), [
  value("i32", 9),
]);

// ./test/core/unwind.wast:236
assert_return(() => invoke($0, `block-nested-unwind-by-br_table-value`, []), [
  value("i32", 9),
]);

// ./test/core/unwind.wast:237
assert_return(() => invoke($0, `block-nested-unwind-by-return`, []), [
  value("i32", 9),
]);

// ./test/core/unwind.wast:239
assert_trap(() => invoke($0, `unary-after-unreachable`, []), `unreachable`);

// ./test/core/unwind.wast:240
assert_return(() => invoke($0, `unary-after-br`, []), [value("i32", 9)]);

// ./test/core/unwind.wast:241
assert_return(() => invoke($0, `unary-after-br_if`, []), [value("i32", 9)]);

// ./test/core/unwind.wast:242
assert_return(() => invoke($0, `unary-after-br_table`, []), [value("i32", 9)]);

// ./test/core/unwind.wast:243
assert_return(() => invoke($0, `unary-after-return`, []), [value("i32", 9)]);

// ./test/core/unwind.wast:245
assert_trap(() => invoke($0, `binary-after-unreachable`, []), `unreachable`);

// ./test/core/unwind.wast:246
assert_return(() => invoke($0, `binary-after-br`, []), [value("i32", 9)]);

// ./test/core/unwind.wast:247
assert_return(() => invoke($0, `binary-after-br_if`, []), [value("i32", 9)]);

// ./test/core/unwind.wast:248
assert_return(() => invoke($0, `binary-after-br_table`, []), [value("i32", 9)]);

// ./test/core/unwind.wast:249
assert_return(() => invoke($0, `binary-after-return`, []), [value("i32", 9)]);

// ./test/core/unwind.wast:251
assert_trap(() => invoke($0, `select-after-unreachable`, []), `unreachable`);

// ./test/core/unwind.wast:252
assert_return(() => invoke($0, `select-after-br`, []), [value("i32", 9)]);

// ./test/core/unwind.wast:253
assert_return(() => invoke($0, `select-after-br_if`, []), [value("i32", 9)]);

// ./test/core/unwind.wast:254
assert_return(() => invoke($0, `select-after-br_table`, []), [value("i32", 9)]);

// ./test/core/unwind.wast:255
assert_return(() => invoke($0, `select-after-return`, []), [value("i32", 9)]);

// ./test/core/unwind.wast:257
assert_trap(
  () => invoke($0, `block-value-after-unreachable`, []),
  `unreachable`,
);

// ./test/core/unwind.wast:258
assert_return(() => invoke($0, `block-value-after-br`, []), [value("i32", 9)]);

// ./test/core/unwind.wast:259
assert_return(() => invoke($0, `block-value-after-br_if`, []), [
  value("i32", 9),
]);

// ./test/core/unwind.wast:260
assert_return(() => invoke($0, `block-value-after-br_table`, []), [
  value("i32", 9),
]);

// ./test/core/unwind.wast:261
assert_return(() => invoke($0, `block-value-after-return`, []), [
  value("i32", 9),
]);

// ./test/core/unwind.wast:263
assert_trap(
  () => invoke($0, `loop-value-after-unreachable`, []),
  `unreachable`,
);

// ./test/core/unwind.wast:264
assert_return(() => invoke($0, `loop-value-after-br`, []), [value("i32", 9)]);

// ./test/core/unwind.wast:265
assert_return(() => invoke($0, `loop-value-after-br_if`, []), [
  value("i32", 9),
]);

// ./test/core/unwind.wast:266
assert_return(() => invoke($0, `loop-value-after-br_table`, []), [
  value("i32", 9),
]);

// ./test/core/unwind.wast:267
assert_return(() => invoke($0, `loop-value-after-return`, []), [
  value("i32", 9),
]);
