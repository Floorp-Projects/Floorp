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

// ./test/core/return.wast

// ./test/core/return.wast:3
let $0 = instantiate(`(module
  ;; Auxiliary definition
  (func $$dummy)

  (func (export "type-i32") (drop (i32.ctz (return))))
  (func (export "type-i64") (drop (i64.ctz (return))))
  (func (export "type-f32") (drop (f32.neg (return))))
  (func (export "type-f64") (drop (f64.neg (return))))

  (func (export "type-i32-value") (result i32)
    (block (result i32) (i32.ctz (return (i32.const 1))))
  )
  (func (export "type-i64-value") (result i64)
    (block (result i64) (i64.ctz (return (i64.const 2))))
  )
  (func (export "type-f32-value") (result f32)
    (block (result f32) (f32.neg (return (f32.const 3))))
  )
  (func (export "type-f64-value") (result f64)
    (block (result f64) (f64.neg (return (f64.const 4))))
  )

  (func (export "nullary") (return))
  (func (export "unary") (result f64) (return (f64.const 3)))

  (func (export "as-func-first") (result i32)
    (return (i32.const 1)) (i32.const 2)
  )
  (func (export "as-func-mid") (result i32)
    (call $$dummy) (return (i32.const 2)) (i32.const 3)
  )
  (func (export "as-func-last")
    (nop) (call $$dummy) (return)
  )
  (func (export "as-func-value") (result i32)
    (nop) (call $$dummy) (return (i32.const 3))
  )

  (func (export "as-block-first")
    (block (return) (call $$dummy))
  )
  (func (export "as-block-mid")
    (block (call $$dummy) (return) (call $$dummy))
  )
  (func (export "as-block-last")
    (block (nop) (call $$dummy) (return))
  )
  (func (export "as-block-value") (result i32)
    (block (result i32) (nop) (call $$dummy) (return (i32.const 2)))
  )

  (func (export "as-loop-first") (result i32)
    (loop (result i32) (return (i32.const 3)) (i32.const 2))
  )
  (func (export "as-loop-mid") (result i32)
    (loop (result i32) (call $$dummy) (return (i32.const 4)) (i32.const 2))
  )
  (func (export "as-loop-last") (result i32)
    (loop (result i32) (nop) (call $$dummy) (return (i32.const 5)))
  )

  (func (export "as-br-value") (result i32)
    (block (result i32) (br 0 (return (i32.const 9))))
  )

  (func (export "as-br_if-cond")
    (block (br_if 0 (return)))
  )
  (func (export "as-br_if-value") (result i32)
    (block (result i32)
      (drop (br_if 0 (return (i32.const 8)) (i32.const 1))) (i32.const 7)
    )
  )
  (func (export "as-br_if-value-cond") (result i32)
    (block (result i32)
      (drop (br_if 0 (i32.const 6) (return (i32.const 9)))) (i32.const 7)
    )
  )

  (func (export "as-br_table-index") (result i64)
    (block (br_table 0 0 0 (return (i64.const 9)))) (i64.const -1)
  )
  (func (export "as-br_table-value") (result i32)
    (block (result i32)
      (br_table 0 0 0 (return (i32.const 10)) (i32.const 1)) (i32.const 7)
    )
  )
  (func (export "as-br_table-value-index") (result i32)
    (block (result i32)
      (br_table 0 0 (i32.const 6) (return (i32.const 11))) (i32.const 7)
    )
  )

  (func (export "as-return-value") (result i64)
    (return (return (i64.const 7)))
  )

  (func (export "as-if-cond") (result i32)
    (if (result i32)
      (return (i32.const 2)) (then (i32.const 0)) (else (i32.const 1))
    )
  )
  (func (export "as-if-then") (param i32 i32) (result i32)
    (if (result i32)
      (local.get 0) (then (return (i32.const 3))) (else (local.get 1))
    )
  )
  (func (export "as-if-else") (param i32 i32) (result i32)
    (if (result i32)
      (local.get 0) (then (local.get 1)) (else (return (i32.const 4)))
    )
  )

  (func (export "as-select-first") (param i32 i32) (result i32)
    (select (return (i32.const 5)) (local.get 0) (local.get 1))
  )
  (func (export "as-select-second") (param i32 i32) (result i32)
    (select (local.get 0) (return (i32.const 6)) (local.get 1))
  )
  (func (export "as-select-cond") (result i32)
    (select (i32.const 0) (i32.const 1) (return (i32.const 7)))
  )

  (func $$f (param i32 i32 i32) (result i32) (i32.const -1))
  (func (export "as-call-first") (result i32)
    (call $$f (return (i32.const 12)) (i32.const 2) (i32.const 3))
  )
  (func (export "as-call-mid") (result i32)
    (call $$f (i32.const 1) (return (i32.const 13)) (i32.const 3))
  )
  (func (export "as-call-last") (result i32)
    (call $$f (i32.const 1) (i32.const 2) (return (i32.const 14)))
  )

  (type $$sig (func (param i32 i32 i32) (result i32)))
  (table funcref (elem $$f))
  (func (export "as-call_indirect-func") (result i32)
    (call_indirect (type $$sig)
      (return (i32.const 20)) (i32.const 1) (i32.const 2) (i32.const 3)
    )
  )
  (func (export "as-call_indirect-first") (result i32)
    (call_indirect (type $$sig)
      (i32.const 0) (return (i32.const 21)) (i32.const 2) (i32.const 3)
    )
  )
  (func (export "as-call_indirect-mid") (result i32)
    (call_indirect (type $$sig)
      (i32.const 0) (i32.const 1) (return (i32.const 22)) (i32.const 3)
    )
  )
  (func (export "as-call_indirect-last") (result i32)
    (call_indirect (type $$sig)
      (i32.const 0) (i32.const 1) (i32.const 2) (return (i32.const 23))
    )
  )

  (func (export "as-local.set-value") (result i32) (local f32)
    (local.set 0 (return (i32.const 17))) (i32.const -1)
  )
  (func (export "as-local.tee-value") (result i32) (local i32)
    (local.tee 0 (return (i32.const 1)))
  )
  (global $$a (mut i32) (i32.const 0))
  (func (export "as-global.set-value") (result i32)
    (global.set $$a (return (i32.const 1)))
  )

  (memory 1)
  (func (export "as-load-address") (result f32)
    (f32.load (return (f32.const 1.7)))
  )
  (func (export "as-loadN-address") (result i64)
    (i64.load8_s (return (i64.const 30)))
  )

  (func (export "as-store-address") (result i32)
    (f64.store (return (i32.const 30)) (f64.const 7)) (i32.const -1)
  )
  (func (export "as-store-value") (result i32)
    (i64.store (i32.const 2) (return (i32.const 31))) (i32.const -1)
  )

  (func (export "as-storeN-address") (result i32)
    (i32.store8 (return (i32.const 32)) (i32.const 7)) (i32.const -1)
  )
  (func (export "as-storeN-value") (result i32)
    (i64.store16 (i32.const 2) (return (i32.const 33))) (i32.const -1)
  )

  (func (export "as-unary-operand") (result f32)
    (f32.neg (return (f32.const 3.4)))
  )

  (func (export "as-binary-left") (result i32)
    (i32.add (return (i32.const 3)) (i32.const 10))
  )
  (func (export "as-binary-right") (result i64)
    (i64.sub (i64.const 10) (return (i64.const 45)))
  )

  (func (export "as-test-operand") (result i32)
    (i32.eqz (return (i32.const 44)))
  )

  (func (export "as-compare-left") (result i32)
    (f64.le (return (i32.const 43)) (f64.const 10))
  )
  (func (export "as-compare-right") (result i32)
    (f32.ne (f32.const 10) (return (i32.const 42)))
  )

  (func (export "as-convert-operand") (result i32)
    (i32.wrap_i64 (return (i32.const 41)))
  )

  (func (export "as-memory.grow-size") (result i32)
    (memory.grow (return (i32.const 40)))
  )
)`);

// ./test/core/return.wast:224
assert_return(() => invoke($0, `type-i32`, []), []);

// ./test/core/return.wast:225
assert_return(() => invoke($0, `type-i64`, []), []);

// ./test/core/return.wast:226
assert_return(() => invoke($0, `type-f32`, []), []);

// ./test/core/return.wast:227
assert_return(() => invoke($0, `type-f64`, []), []);

// ./test/core/return.wast:229
assert_return(() => invoke($0, `type-i32-value`, []), [value("i32", 1)]);

// ./test/core/return.wast:230
assert_return(() => invoke($0, `type-i64-value`, []), [value("i64", 2n)]);

// ./test/core/return.wast:231
assert_return(() => invoke($0, `type-f32-value`, []), [value("f32", 3)]);

// ./test/core/return.wast:232
assert_return(() => invoke($0, `type-f64-value`, []), [value("f64", 4)]);

// ./test/core/return.wast:234
assert_return(() => invoke($0, `nullary`, []), []);

// ./test/core/return.wast:235
assert_return(() => invoke($0, `unary`, []), [value("f64", 3)]);

// ./test/core/return.wast:237
assert_return(() => invoke($0, `as-func-first`, []), [value("i32", 1)]);

// ./test/core/return.wast:238
assert_return(() => invoke($0, `as-func-mid`, []), [value("i32", 2)]);

// ./test/core/return.wast:239
assert_return(() => invoke($0, `as-func-last`, []), []);

// ./test/core/return.wast:240
assert_return(() => invoke($0, `as-func-value`, []), [value("i32", 3)]);

// ./test/core/return.wast:242
assert_return(() => invoke($0, `as-block-first`, []), []);

// ./test/core/return.wast:243
assert_return(() => invoke($0, `as-block-mid`, []), []);

// ./test/core/return.wast:244
assert_return(() => invoke($0, `as-block-last`, []), []);

// ./test/core/return.wast:245
assert_return(() => invoke($0, `as-block-value`, []), [value("i32", 2)]);

// ./test/core/return.wast:247
assert_return(() => invoke($0, `as-loop-first`, []), [value("i32", 3)]);

// ./test/core/return.wast:248
assert_return(() => invoke($0, `as-loop-mid`, []), [value("i32", 4)]);

// ./test/core/return.wast:249
assert_return(() => invoke($0, `as-loop-last`, []), [value("i32", 5)]);

// ./test/core/return.wast:251
assert_return(() => invoke($0, `as-br-value`, []), [value("i32", 9)]);

// ./test/core/return.wast:253
assert_return(() => invoke($0, `as-br_if-cond`, []), []);

// ./test/core/return.wast:254
assert_return(() => invoke($0, `as-br_if-value`, []), [value("i32", 8)]);

// ./test/core/return.wast:255
assert_return(() => invoke($0, `as-br_if-value-cond`, []), [value("i32", 9)]);

// ./test/core/return.wast:257
assert_return(() => invoke($0, `as-br_table-index`, []), [value("i64", 9n)]);

// ./test/core/return.wast:258
assert_return(() => invoke($0, `as-br_table-value`, []), [value("i32", 10)]);

// ./test/core/return.wast:259
assert_return(() => invoke($0, `as-br_table-value-index`, []), [
  value("i32", 11),
]);

// ./test/core/return.wast:261
assert_return(() => invoke($0, `as-return-value`, []), [value("i64", 7n)]);

// ./test/core/return.wast:263
assert_return(() => invoke($0, `as-if-cond`, []), [value("i32", 2)]);

// ./test/core/return.wast:264
assert_return(() => invoke($0, `as-if-then`, [1, 6]), [value("i32", 3)]);

// ./test/core/return.wast:265
assert_return(() => invoke($0, `as-if-then`, [0, 6]), [value("i32", 6)]);

// ./test/core/return.wast:266
assert_return(() => invoke($0, `as-if-else`, [0, 6]), [value("i32", 4)]);

// ./test/core/return.wast:267
assert_return(() => invoke($0, `as-if-else`, [1, 6]), [value("i32", 6)]);

// ./test/core/return.wast:269
assert_return(() => invoke($0, `as-select-first`, [0, 6]), [value("i32", 5)]);

// ./test/core/return.wast:270
assert_return(() => invoke($0, `as-select-first`, [1, 6]), [value("i32", 5)]);

// ./test/core/return.wast:271
assert_return(() => invoke($0, `as-select-second`, [0, 6]), [value("i32", 6)]);

// ./test/core/return.wast:272
assert_return(() => invoke($0, `as-select-second`, [1, 6]), [value("i32", 6)]);

// ./test/core/return.wast:273
assert_return(() => invoke($0, `as-select-cond`, []), [value("i32", 7)]);

// ./test/core/return.wast:275
assert_return(() => invoke($0, `as-call-first`, []), [value("i32", 12)]);

// ./test/core/return.wast:276
assert_return(() => invoke($0, `as-call-mid`, []), [value("i32", 13)]);

// ./test/core/return.wast:277
assert_return(() => invoke($0, `as-call-last`, []), [value("i32", 14)]);

// ./test/core/return.wast:279
assert_return(() => invoke($0, `as-call_indirect-func`, []), [
  value("i32", 20),
]);

// ./test/core/return.wast:280
assert_return(() => invoke($0, `as-call_indirect-first`, []), [
  value("i32", 21),
]);

// ./test/core/return.wast:281
assert_return(() => invoke($0, `as-call_indirect-mid`, []), [value("i32", 22)]);

// ./test/core/return.wast:282
assert_return(() => invoke($0, `as-call_indirect-last`, []), [
  value("i32", 23),
]);

// ./test/core/return.wast:284
assert_return(() => invoke($0, `as-local.set-value`, []), [value("i32", 17)]);

// ./test/core/return.wast:285
assert_return(() => invoke($0, `as-local.tee-value`, []), [value("i32", 1)]);

// ./test/core/return.wast:286
assert_return(() => invoke($0, `as-global.set-value`, []), [value("i32", 1)]);

// ./test/core/return.wast:288
assert_return(() => invoke($0, `as-load-address`, []), [value("f32", 1.7)]);

// ./test/core/return.wast:289
assert_return(() => invoke($0, `as-loadN-address`, []), [value("i64", 30n)]);

// ./test/core/return.wast:291
assert_return(() => invoke($0, `as-store-address`, []), [value("i32", 30)]);

// ./test/core/return.wast:292
assert_return(() => invoke($0, `as-store-value`, []), [value("i32", 31)]);

// ./test/core/return.wast:293
assert_return(() => invoke($0, `as-storeN-address`, []), [value("i32", 32)]);

// ./test/core/return.wast:294
assert_return(() => invoke($0, `as-storeN-value`, []), [value("i32", 33)]);

// ./test/core/return.wast:296
assert_return(() => invoke($0, `as-unary-operand`, []), [value("f32", 3.4)]);

// ./test/core/return.wast:298
assert_return(() => invoke($0, `as-binary-left`, []), [value("i32", 3)]);

// ./test/core/return.wast:299
assert_return(() => invoke($0, `as-binary-right`, []), [value("i64", 45n)]);

// ./test/core/return.wast:301
assert_return(() => invoke($0, `as-test-operand`, []), [value("i32", 44)]);

// ./test/core/return.wast:303
assert_return(() => invoke($0, `as-compare-left`, []), [value("i32", 43)]);

// ./test/core/return.wast:304
assert_return(() => invoke($0, `as-compare-right`, []), [value("i32", 42)]);

// ./test/core/return.wast:306
assert_return(() => invoke($0, `as-convert-operand`, []), [value("i32", 41)]);

// ./test/core/return.wast:308
assert_return(() => invoke($0, `as-memory.grow-size`, []), [value("i32", 40)]);

// ./test/core/return.wast:310
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-value-empty-vs-num (result i32) (return)))`,
    ),
  `type mismatch`,
);

// ./test/core/return.wast:314
assert_invalid(() =>
  instantiate(`(module
    (func $$type-value-empty-vs-num-in-block (result i32)
      (i32.const 0)
      (block (return))
    )
  )`), `type mismatch`);

// ./test/core/return.wast:323
assert_invalid(() =>
  instantiate(`(module
    (func $$type-value-empty-vs-num-in-loop (result i32)
      (i32.const 0)
      (loop (return))
    )
  )`), `type mismatch`);

// ./test/core/return.wast:332
assert_invalid(() =>
  instantiate(`(module
    (func $$type-value-empty-vs-num-in-then (result i32)
      (i32.const 0) (i32.const 0)
      (if (then (return)))
    )
  )`), `type mismatch`);

// ./test/core/return.wast:341
assert_invalid(() =>
  instantiate(`(module
    (func $$type-value-empty-vs-num-in-else (result i32)
      (i32.const 0) (i32.const 0)
      (if (result i32) (then (i32.const 0)) (else (return))) (drop)
    )
  )`), `type mismatch`);

// ./test/core/return.wast:350
assert_invalid(() =>
  instantiate(`(module
    (func $$type-value-empty-vs-num-in-br (result i32)
      (i32.const 0)
      (block (br 0 (return)))
    )
  )`), `type mismatch`);

// ./test/core/return.wast:359
assert_invalid(() =>
  instantiate(`(module
    (func $$type-value-empty-vs-num-in-br_if (result i32)
      (i32.const 0)
      (block (br_if 0 (return) (i32.const 1)))
    )
  )`), `type mismatch`);

// ./test/core/return.wast:368
assert_invalid(() =>
  instantiate(`(module
    (func $$type-value-empty-vs-num-in-br_table (result i32)
      (i32.const 0)
      (block (br_table 0 (return)))
    )
  )`), `type mismatch`);

// ./test/core/return.wast:377
assert_invalid(() =>
  instantiate(`(module
    (func $$type-value-empty-vs-num-in-return (result i32)
      (return (return))
    )
  )`), `type mismatch`);

// ./test/core/return.wast:385
assert_invalid(() =>
  instantiate(`(module
    (func $$type-value-empty-vs-num-in-select (result i32)
      (select (return) (i32.const 1) (i32.const 2))
    )
  )`), `type mismatch`);

// ./test/core/return.wast:393
assert_invalid(() =>
  instantiate(`(module
    (func $$type-value-empty-vs-num-in-call (result i32)
      (call 1 (return))
    )
    (func (param i32) (result i32) (local.get 0))
  )`), `type mismatch`);

// ./test/core/return.wast:402
assert_invalid(() =>
  instantiate(`(module
    (func $$f (param i32) (result i32) (local.get 0))
    (type $$sig (func (param i32) (result i32)))
    (table funcref (elem $$f))
    (func $$type-value-empty-vs-num-in-call_indirect (result i32)
      (block (result i32)
        (call_indirect (type $$sig)
          (return) (i32.const 0)
        )
      )
    )
  )`), `type mismatch`);

// ./test/core/return.wast:417
assert_invalid(() =>
  instantiate(`(module
    (func $$type-value-empty-vs-num-in-local.set (result i32)
      (local i32)
      (local.set 0 (return)) (local.get 0)
    )
  )`), `type mismatch`);

// ./test/core/return.wast:426
assert_invalid(() =>
  instantiate(`(module
    (func $$type-value-empty-vs-num-in-local.tee (result i32)
      (local i32)
      (local.tee 0 (return))
    )
  )`), `type mismatch`);

// ./test/core/return.wast:435
assert_invalid(() =>
  instantiate(`(module
    (global $$x (mut i32) (i32.const 0))
    (func $$type-value-empty-vs-num-in-global.set (result i32)
      (global.set $$x (return)) (global.get $$x)
    )
  )`), `type mismatch`);

// ./test/core/return.wast:444
assert_invalid(() =>
  instantiate(`(module
    (memory 0)
    (func $$type-value-empty-vs-num-in-memory.grow (result i32)
      (memory.grow (return))
    )
  )`), `type mismatch`);

// ./test/core/return.wast:453
assert_invalid(() =>
  instantiate(`(module
    (memory 0)
    (func $$type-value-empty-vs-num-in-load (result i32)
      (i32.load (return))
    )
  )`), `type mismatch`);

// ./test/core/return.wast:462
assert_invalid(() =>
  instantiate(`(module
    (memory 1)
    (func $$type-value-empty-vs-num-in-store (result i32)
      (i32.store (return) (i32.const 1))
    )
  )`), `type mismatch`);

// ./test/core/return.wast:471
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-value-void-vs-num (result f64) (return (nop))))`,
    ),
  `type mismatch`,
);

// ./test/core/return.wast:475
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-value-num-vs-num (result f64) (return (i64.const 1))))`,
    ),
  `type mismatch`,
);
