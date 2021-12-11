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

// ./test/core/br.wast

// ./test/core/br.wast:3
let $0 = instantiate(`(module
  ;; Auxiliary definition
  (func $$dummy)

  (func (export "type-i32") (block (drop (i32.ctz (br 0)))))
  (func (export "type-i64") (block (drop (i64.ctz (br 0)))))
  (func (export "type-f32") (block (drop (f32.neg (br 0)))))
  (func (export "type-f64") (block (drop (f64.neg (br 0)))))
  (func (export "type-i32-i32") (block (drop (i32.add (br 0)))))
  (func (export "type-i64-i64") (block (drop (i64.add (br 0)))))
  (func (export "type-f32-f32") (block (drop (f32.add (br 0)))))
  (func (export "type-f64-f64") (block (drop (f64.add (br 0)))))

  (func (export "type-i32-value") (result i32)
    (block (result i32) (i32.ctz (br 0 (i32.const 1))))
  )
  (func (export "type-i64-value") (result i64)
    (block (result i64) (i64.ctz (br 0 (i64.const 2))))
  )
  (func (export "type-f32-value") (result f32)
    (block (result f32) (f32.neg (br 0 (f32.const 3))))
  )
  (func (export "type-f64-value") (result f64)
    (block (result f64) (f64.neg (br 0 (f64.const 4))))
  )
  (func (export "type-f64-f64-value") (result f64 f64)
    (block (result f64 f64)
      (f64.add (br 0 (f64.const 4) (f64.const 5))) (f64.const 6)
    )
  )

  (func (export "as-block-first")
    (block (br 0) (call $$dummy))
  )
  (func (export "as-block-mid")
    (block (call $$dummy) (br 0) (call $$dummy))
  )
  (func (export "as-block-last")
    (block (nop) (call $$dummy) (br 0))
  )
  (func (export "as-block-value") (result i32)
    (block (result i32) (nop) (call $$dummy) (br 0 (i32.const 2)))
  )

  (func (export "as-loop-first") (result i32)
    (block (result i32) (loop (result i32) (br 1 (i32.const 3)) (i32.const 2)))
  )
  (func (export "as-loop-mid") (result i32)
    (block (result i32)
      (loop (result i32) (call $$dummy) (br 1 (i32.const 4)) (i32.const 2))
    )
  )
  (func (export "as-loop-last") (result i32)
    (block (result i32)
      (loop (result i32) (nop) (call $$dummy) (br 1 (i32.const 5)))
    )
  )

  (func (export "as-br-value") (result i32)
    (block (result i32) (br 0 (br 0 (i32.const 9))))
  )

  (func (export "as-br_if-cond")
    (block (br_if 0 (br 0)))
  )
  (func (export "as-br_if-value") (result i32)
    (block (result i32)
      (drop (br_if 0 (br 0 (i32.const 8)) (i32.const 1))) (i32.const 7)
    )
  )
  (func (export "as-br_if-value-cond") (result i32)
    (block (result i32)
      (drop (br_if 0 (i32.const 6) (br 0 (i32.const 9)))) (i32.const 7)
    )
  )

  (func (export "as-br_table-index")
    (block (br_table 0 0 0 (br 0)))
  )
  (func (export "as-br_table-value") (result i32)
    (block (result i32)
      (br_table 0 0 0 (br 0 (i32.const 10)) (i32.const 1)) (i32.const 7)
    )
  )
  (func (export "as-br_table-value-index") (result i32)
    (block (result i32)
      (br_table 0 0 (i32.const 6) (br 0 (i32.const 11))) (i32.const 7)
    )
  )

  (func (export "as-return-value") (result i64)
    (block (result i64) (return (br 0 (i64.const 7))))
  )
  (func (export "as-return-values") (result i32 i64)
    (i32.const 2)
    (block (result i64) (return (br 0 (i32.const 1) (i64.const 7))))
  )

  (func (export "as-if-cond") (result i32)
    (block (result i32)
      (if (result i32) (br 0 (i32.const 2))
        (then (i32.const 0))
        (else (i32.const 1))
      )
    )
  )
  (func (export "as-if-then") (param i32 i32) (result i32)
    (block (result i32)
      (if (result i32) (local.get 0)
        (then (br 1 (i32.const 3)))
        (else (local.get 1))
      )
    )
  )
  (func (export "as-if-else") (param i32 i32) (result i32)
    (block (result i32)
      (if (result i32) (local.get 0)
        (then (local.get 1))
        (else (br 1 (i32.const 4)))
      )
    )
  )

  (func (export "as-select-first") (param i32 i32) (result i32)
    (block (result i32)
      (select (br 0 (i32.const 5)) (local.get 0) (local.get 1))
    )
  )
  (func (export "as-select-second") (param i32 i32) (result i32)
    (block (result i32)
      (select (local.get 0) (br 0 (i32.const 6)) (local.get 1))
    )
  )
  (func (export "as-select-cond") (result i32)
    (block (result i32)
      (select (i32.const 0) (i32.const 1) (br 0 (i32.const 7)))
    )
  )
  (func (export "as-select-all") (result i32)
    (block (result i32) (select (br 0 (i32.const 8))))
  )

  (func $$f (param i32 i32 i32) (result i32) (i32.const -1))
  (func (export "as-call-first") (result i32)
    (block (result i32)
      (call $$f (br 0 (i32.const 12)) (i32.const 2) (i32.const 3))
    )
  )
  (func (export "as-call-mid") (result i32)
    (block (result i32)
      (call $$f (i32.const 1) (br 0 (i32.const 13)) (i32.const 3))
    )
  )
  (func (export "as-call-last") (result i32)
    (block (result i32)
      (call $$f (i32.const 1) (i32.const 2) (br 0 (i32.const 14)))
    )
  )
  (func (export "as-call-all") (result i32)
    (block (result i32) (call $$f (br 0 (i32.const 15))))
  )

  (type $$sig (func (param i32 i32 i32) (result i32)))
  (table funcref (elem $$f))
  (func (export "as-call_indirect-func") (result i32)
    (block (result i32)
      (call_indirect (type $$sig)
        (br 0 (i32.const 20))
        (i32.const 1) (i32.const 2) (i32.const 3)
      )
    )
  )
  (func (export "as-call_indirect-first") (result i32)
    (block (result i32)
      (call_indirect (type $$sig)
        (i32.const 0)
        (br 0 (i32.const 21)) (i32.const 2) (i32.const 3)
      )
    )
  )
  (func (export "as-call_indirect-mid") (result i32)
    (block (result i32)
      (call_indirect (type $$sig)
        (i32.const 0)
        (i32.const 1) (br 0 (i32.const 22)) (i32.const 3)
      )
    )
  )
  (func (export "as-call_indirect-last") (result i32)
    (block (result i32)
      (call_indirect (type $$sig)
        (i32.const 0)
        (i32.const 1) (i32.const 2) (br 0 (i32.const 23))
      )
    )
  )
  (func (export "as-call_indirect-all") (result i32)
    (block (result i32) (call_indirect (type $$sig) (br 0 (i32.const 24))))
  )

  (func (export "as-local.set-value") (result i32) (local f32)
    (block (result i32) (local.set 0 (br 0 (i32.const 17))) (i32.const -1))
  )
  (func (export "as-local.tee-value") (result i32) (local i32)
    (block (result i32) (local.tee 0 (br 0 (i32.const 1))))
  )
  (global $$a (mut i32) (i32.const 10))
  (func (export "as-global.set-value") (result i32)
    (block (result i32) (global.set $$a (br 0 (i32.const 1))))
  )

  (memory 1)
  (func (export "as-load-address") (result f32)
    (block (result f32) (f32.load (br 0 (f32.const 1.7))))
  )
  (func (export "as-loadN-address") (result i64)
    (block (result i64) (i64.load8_s (br 0 (i64.const 30))))
  )

  (func (export "as-store-address") (result i32)
    (block (result i32)
      (f64.store (br 0 (i32.const 30)) (f64.const 7)) (i32.const -1)
    )
  )
  (func (export "as-store-value") (result i32)
    (block (result i32)
      (i64.store (i32.const 2) (br 0 (i32.const 31))) (i32.const -1)
    )
  )
  (func (export "as-store-both") (result i32)
    (block (result i32)
      (i64.store (br 0 (i32.const 32))) (i32.const -1)
    )
  )

  (func (export "as-storeN-address") (result i32)
    (block (result i32)
      (i32.store8 (br 0 (i32.const 32)) (i32.const 7)) (i32.const -1)
    )
  )
  (func (export "as-storeN-value") (result i32)
    (block (result i32)
      (i64.store16 (i32.const 2) (br 0 (i32.const 33))) (i32.const -1)
    )
  )
  (func (export "as-storeN-both") (result i32)
    (block (result i32)
      (i64.store16 (br 0 (i32.const 34))) (i32.const -1)
    )
  )

  (func (export "as-unary-operand") (result f32)
    (block (result f32) (f32.neg (br 0 (f32.const 3.4))))
  )

  (func (export "as-binary-left") (result i32)
    (block (result i32) (i32.add (br 0 (i32.const 3)) (i32.const 10)))
  )
  (func (export "as-binary-right") (result i64)
    (block (result i64) (i64.sub (i64.const 10) (br 0 (i64.const 45))))
  )
  (func (export "as-binary-both") (result i32)
    (block (result i32) (i32.add (br 0 (i32.const 46))))
  )

  (func (export "as-test-operand") (result i32)
    (block (result i32) (i32.eqz (br 0 (i32.const 44))))
  )

  (func (export "as-compare-left") (result i32)
    (block (result i32) (f64.le (br 0 (i32.const 43)) (f64.const 10)))
  )
  (func (export "as-compare-right") (result i32)
    (block (result i32) (f32.ne (f32.const 10) (br 0 (i32.const 42))))
  )
  (func (export "as-compare-both") (result i32)
    (block (result i32) (f64.le (br 0 (i32.const 44))))
  )

  (func (export "as-convert-operand") (result i32)
    (block (result i32) (i32.wrap_i64 (br 0 (i32.const 41))))
  )

  (func (export "as-memory.grow-size") (result i32)
    (block (result i32) (memory.grow (br 0 (i32.const 40))))
  )

  (func (export "nested-block-value") (result i32)
    (i32.add
      (i32.const 1)
      (block (result i32)
        (call $$dummy)
        (i32.add (i32.const 4) (br 0 (i32.const 8)))
      )
    )
  )

  (func (export "nested-br-value") (result i32)
    (i32.add
      (i32.const 1)
      (block (result i32)
        (drop (i32.const 2))
        (drop
          (block (result i32)
            (drop (i32.const 4))
            (br 0 (br 1 (i32.const 8)))
          )
        )
        (i32.const 16)
      )
    )
  )

  (func (export "nested-br_if-value") (result i32)
    (i32.add
      (i32.const 1)
      (block (result i32)
        (drop (i32.const 2))
        (drop
          (block (result i32)
            (drop (i32.const 4))
            (drop (br_if 0 (br 1 (i32.const 8)) (i32.const 1)))
            (i32.const 32)
          )
        )
        (i32.const 16)
      )
    )
  )

  (func (export "nested-br_if-value-cond") (result i32)
    (i32.add
      (i32.const 1)
      (block (result i32)
        (drop (i32.const 2))
        (drop (br_if 0 (i32.const 4) (br 0 (i32.const 8))))
        (i32.const 16)
      )
    )
  )

  (func (export "nested-br_table-value") (result i32)
    (i32.add
      (i32.const 1)
      (block (result i32)
        (drop (i32.const 2))
        (drop
          (block (result i32)
            (drop (i32.const 4))
            (br_table 0 (br 1 (i32.const 8)) (i32.const 1))
          )
        )
        (i32.const 16)
      )
    )
  )

  (func (export "nested-br_table-value-index") (result i32)
    (i32.add
      (i32.const 1)
      (block (result i32)
        (drop (i32.const 2))
        (br_table 0 (i32.const 4) (br 0 (i32.const 8)))
        (i32.const 16)
      )
    )
  )
)`);

// ./test/core/br.wast:372
assert_return(() => invoke($0, `type-i32`, []), []);

// ./test/core/br.wast:373
assert_return(() => invoke($0, `type-i64`, []), []);

// ./test/core/br.wast:374
assert_return(() => invoke($0, `type-f32`, []), []);

// ./test/core/br.wast:375
assert_return(() => invoke($0, `type-f64`, []), []);

// ./test/core/br.wast:376
assert_return(() => invoke($0, `type-i32-i32`, []), []);

// ./test/core/br.wast:377
assert_return(() => invoke($0, `type-i64-i64`, []), []);

// ./test/core/br.wast:378
assert_return(() => invoke($0, `type-f32-f32`, []), []);

// ./test/core/br.wast:379
assert_return(() => invoke($0, `type-f64-f64`, []), []);

// ./test/core/br.wast:381
assert_return(() => invoke($0, `type-i32-value`, []), [value("i32", 1)]);

// ./test/core/br.wast:382
assert_return(() => invoke($0, `type-i64-value`, []), [value("i64", 2n)]);

// ./test/core/br.wast:383
assert_return(() => invoke($0, `type-f32-value`, []), [value("f32", 3)]);

// ./test/core/br.wast:384
assert_return(() => invoke($0, `type-f64-value`, []), [value("f64", 4)]);

// ./test/core/br.wast:385
assert_return(() => invoke($0, `type-f64-f64-value`, []), [
  value("f64", 4),
  value("f64", 5),
]);

// ./test/core/br.wast:387
assert_return(() => invoke($0, `as-block-first`, []), []);

// ./test/core/br.wast:388
assert_return(() => invoke($0, `as-block-mid`, []), []);

// ./test/core/br.wast:389
assert_return(() => invoke($0, `as-block-last`, []), []);

// ./test/core/br.wast:390
assert_return(() => invoke($0, `as-block-value`, []), [value("i32", 2)]);

// ./test/core/br.wast:392
assert_return(() => invoke($0, `as-loop-first`, []), [value("i32", 3)]);

// ./test/core/br.wast:393
assert_return(() => invoke($0, `as-loop-mid`, []), [value("i32", 4)]);

// ./test/core/br.wast:394
assert_return(() => invoke($0, `as-loop-last`, []), [value("i32", 5)]);

// ./test/core/br.wast:396
assert_return(() => invoke($0, `as-br-value`, []), [value("i32", 9)]);

// ./test/core/br.wast:398
assert_return(() => invoke($0, `as-br_if-cond`, []), []);

// ./test/core/br.wast:399
assert_return(() => invoke($0, `as-br_if-value`, []), [value("i32", 8)]);

// ./test/core/br.wast:400
assert_return(() => invoke($0, `as-br_if-value-cond`, []), [value("i32", 9)]);

// ./test/core/br.wast:402
assert_return(() => invoke($0, `as-br_table-index`, []), []);

// ./test/core/br.wast:403
assert_return(() => invoke($0, `as-br_table-value`, []), [value("i32", 10)]);

// ./test/core/br.wast:404
assert_return(() => invoke($0, `as-br_table-value-index`, []), [
  value("i32", 11),
]);

// ./test/core/br.wast:406
assert_return(() => invoke($0, `as-return-value`, []), [value("i64", 7n)]);

// ./test/core/br.wast:407
assert_return(() => invoke($0, `as-return-values`, []), [
  value("i32", 2),
  value("i64", 7n),
]);

// ./test/core/br.wast:409
assert_return(() => invoke($0, `as-if-cond`, []), [value("i32", 2)]);

// ./test/core/br.wast:410
assert_return(() => invoke($0, `as-if-then`, [1, 6]), [value("i32", 3)]);

// ./test/core/br.wast:411
assert_return(() => invoke($0, `as-if-then`, [0, 6]), [value("i32", 6)]);

// ./test/core/br.wast:412
assert_return(() => invoke($0, `as-if-else`, [0, 6]), [value("i32", 4)]);

// ./test/core/br.wast:413
assert_return(() => invoke($0, `as-if-else`, [1, 6]), [value("i32", 6)]);

// ./test/core/br.wast:415
assert_return(() => invoke($0, `as-select-first`, [0, 6]), [value("i32", 5)]);

// ./test/core/br.wast:416
assert_return(() => invoke($0, `as-select-first`, [1, 6]), [value("i32", 5)]);

// ./test/core/br.wast:417
assert_return(() => invoke($0, `as-select-second`, [0, 6]), [value("i32", 6)]);

// ./test/core/br.wast:418
assert_return(() => invoke($0, `as-select-second`, [1, 6]), [value("i32", 6)]);

// ./test/core/br.wast:419
assert_return(() => invoke($0, `as-select-cond`, []), [value("i32", 7)]);

// ./test/core/br.wast:420
assert_return(() => invoke($0, `as-select-all`, []), [value("i32", 8)]);

// ./test/core/br.wast:422
assert_return(() => invoke($0, `as-call-first`, []), [value("i32", 12)]);

// ./test/core/br.wast:423
assert_return(() => invoke($0, `as-call-mid`, []), [value("i32", 13)]);

// ./test/core/br.wast:424
assert_return(() => invoke($0, `as-call-last`, []), [value("i32", 14)]);

// ./test/core/br.wast:425
assert_return(() => invoke($0, `as-call-all`, []), [value("i32", 15)]);

// ./test/core/br.wast:427
assert_return(() => invoke($0, `as-call_indirect-func`, []), [
  value("i32", 20),
]);

// ./test/core/br.wast:428
assert_return(() => invoke($0, `as-call_indirect-first`, []), [
  value("i32", 21),
]);

// ./test/core/br.wast:429
assert_return(() => invoke($0, `as-call_indirect-mid`, []), [value("i32", 22)]);

// ./test/core/br.wast:430
assert_return(() => invoke($0, `as-call_indirect-last`, []), [
  value("i32", 23),
]);

// ./test/core/br.wast:431
assert_return(() => invoke($0, `as-call_indirect-all`, []), [value("i32", 24)]);

// ./test/core/br.wast:433
assert_return(() => invoke($0, `as-local.set-value`, []), [value("i32", 17)]);

// ./test/core/br.wast:434
assert_return(() => invoke($0, `as-local.tee-value`, []), [value("i32", 1)]);

// ./test/core/br.wast:435
assert_return(() => invoke($0, `as-global.set-value`, []), [value("i32", 1)]);

// ./test/core/br.wast:437
assert_return(() => invoke($0, `as-load-address`, []), [value("f32", 1.7)]);

// ./test/core/br.wast:438
assert_return(() => invoke($0, `as-loadN-address`, []), [value("i64", 30n)]);

// ./test/core/br.wast:440
assert_return(() => invoke($0, `as-store-address`, []), [value("i32", 30)]);

// ./test/core/br.wast:441
assert_return(() => invoke($0, `as-store-value`, []), [value("i32", 31)]);

// ./test/core/br.wast:442
assert_return(() => invoke($0, `as-store-both`, []), [value("i32", 32)]);

// ./test/core/br.wast:443
assert_return(() => invoke($0, `as-storeN-address`, []), [value("i32", 32)]);

// ./test/core/br.wast:444
assert_return(() => invoke($0, `as-storeN-value`, []), [value("i32", 33)]);

// ./test/core/br.wast:445
assert_return(() => invoke($0, `as-storeN-both`, []), [value("i32", 34)]);

// ./test/core/br.wast:447
assert_return(() => invoke($0, `as-unary-operand`, []), [value("f32", 3.4)]);

// ./test/core/br.wast:449
assert_return(() => invoke($0, `as-binary-left`, []), [value("i32", 3)]);

// ./test/core/br.wast:450
assert_return(() => invoke($0, `as-binary-right`, []), [value("i64", 45n)]);

// ./test/core/br.wast:451
assert_return(() => invoke($0, `as-binary-both`, []), [value("i32", 46)]);

// ./test/core/br.wast:453
assert_return(() => invoke($0, `as-test-operand`, []), [value("i32", 44)]);

// ./test/core/br.wast:455
assert_return(() => invoke($0, `as-compare-left`, []), [value("i32", 43)]);

// ./test/core/br.wast:456
assert_return(() => invoke($0, `as-compare-right`, []), [value("i32", 42)]);

// ./test/core/br.wast:457
assert_return(() => invoke($0, `as-compare-both`, []), [value("i32", 44)]);

// ./test/core/br.wast:459
assert_return(() => invoke($0, `as-convert-operand`, []), [value("i32", 41)]);

// ./test/core/br.wast:461
assert_return(() => invoke($0, `as-memory.grow-size`, []), [value("i32", 40)]);

// ./test/core/br.wast:463
assert_return(() => invoke($0, `nested-block-value`, []), [value("i32", 9)]);

// ./test/core/br.wast:464
assert_return(() => invoke($0, `nested-br-value`, []), [value("i32", 9)]);

// ./test/core/br.wast:465
assert_return(() => invoke($0, `nested-br_if-value`, []), [value("i32", 9)]);

// ./test/core/br.wast:466
assert_return(() => invoke($0, `nested-br_if-value-cond`, []), [
  value("i32", 9),
]);

// ./test/core/br.wast:467
assert_return(() => invoke($0, `nested-br_table-value`, []), [value("i32", 9)]);

// ./test/core/br.wast:468
assert_return(() => invoke($0, `nested-br_table-value-index`, []), [
  value("i32", 9),
]);

// ./test/core/br.wast:470
assert_invalid(
  () =>
    instantiate(`(module (func $$type-arg-empty-vs-num (result i32)
    (block (result i32) (br 0) (i32.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/br.wast:477
assert_invalid(
  () =>
    instantiate(`(module (func $$type-arg-void-vs-num (result i32)
    (block (result i32) (br 0 (nop)) (i32.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/br.wast:483
assert_invalid(
  () =>
    instantiate(`(module (func $$type-arg-void-vs-num-nested (result i32)
    (block (result i32) (i32.const 0) (block (br 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/br.wast:489
assert_invalid(
  () =>
    instantiate(`(module (func $$type-arg-num-vs-num (result i32)
    (block (result i32) (br 0 (i64.const 1)) (i32.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/br.wast:496
assert_invalid(() =>
  instantiate(`(module
    (func $$type-arg-empty-in-br
      (i32.const 0)
      (block (result i32) (br 0 (br 0))) (i32.eqz) (drop)
    )
  )`), `type mismatch`);

// ./test/core/br.wast:505
assert_invalid(() =>
  instantiate(`(module
    (func $$type-arg-empty-in-br_if
      (i32.const 0)
      (block (result i32) (br_if 0 (br 0) (i32.const 1))) (i32.eqz) (drop)
    )
  )`), `type mismatch`);

// ./test/core/br.wast:514
assert_invalid(() =>
  instantiate(`(module
    (func $$type-arg-empty-in-br_table
      (i32.const 0)
      (block (result i32) (br_table 0 (br 0))) (i32.eqz) (drop)
    )
  )`), `type mismatch`);

// ./test/core/br.wast:523
assert_invalid(() =>
  instantiate(`(module
    (func $$type-arg-empty-in-return
      (block (result i32)
        (return (br 0))
      )
      (i32.eqz) (drop)
    )
  )`), `type mismatch`);

// ./test/core/br.wast:534
assert_invalid(() =>
  instantiate(`(module
    (func $$type-arg-empty-in-select
      (block (result i32)
        (select (br 0) (i32.const 1) (i32.const 2))
      )
      (i32.eqz) (drop)
    )
  )`), `type mismatch`);

// ./test/core/br.wast:545
assert_invalid(() =>
  instantiate(`(module
    (func $$type-arg-empty-in-call
      (block (result i32)
        (call 1 (br 0))
      )
      (i32.eqz) (drop)
    )
    (func (param i32) (result i32) (local.get 0))
  )`), `type mismatch`);

// ./test/core/br.wast:557
assert_invalid(() =>
  instantiate(`(module
    (func $$f (param i32) (result i32) (local.get 0))
    (type $$sig (func (param i32) (result i32)))
    (table funcref (elem $$f))
    (func $$type-arg-empty-in-call_indirect
      (block (result i32)
        (call_indirect (type $$sig)
          (br 0) (i32.const 0)
        )
      )
      (i32.eqz) (drop)
    )
  )`), `type mismatch`);

// ./test/core/br.wast:573
assert_invalid(() =>
  instantiate(`(module
    (func $$type-arg-empty-in-local.set
      (local i32)
      (block (result i32)
        (local.set 0 (br 0)) (local.get 0)
      )
      (i32.eqz) (drop)
    )
  )`), `type mismatch`);

// ./test/core/br.wast:585
assert_invalid(() =>
  instantiate(`(module
    (func $$type-arg-empty-in-local.tee
      (local i32)
      (block (result i32)
        (local.tee 0 (br 0))
      )
      (i32.eqz) (drop)
    )
  )`), `type mismatch`);

// ./test/core/br.wast:597
assert_invalid(() =>
  instantiate(`(module
    (global $$x (mut i32) (i32.const 0))
    (func $$type-arg-empty-in-global.set
      (block (result i32)
        (global.set $$x (br 0)) (global.get $$x)
      )
      (i32.eqz) (drop)
    )
  )`), `type mismatch`);

// ./test/core/br.wast:609
assert_invalid(() =>
  instantiate(`(module
    (memory 0)
    (func $$type-arg-empty-in-memory.grow
      (block (result i32)
        (memory.grow (br 0))
      )
      (i32.eqz) (drop)
    )
  )`), `type mismatch`);

// ./test/core/br.wast:621
assert_invalid(() =>
  instantiate(`(module
    (memory 1)
    (func $$type-arg-empty-in-load
      (block (result i32)
        (i32.load (br 0))
      )
      (i32.eqz) (drop)
    )
  )`), `type mismatch`);

// ./test/core/br.wast:633
assert_invalid(() =>
  instantiate(`(module
    (memory 1)
    (func $$type-arg-empty-in-store
      (block (result i32)
        (i32.store (br 0) (i32.const 0))
      )
      (i32.eqz) (drop)
    )
  )`), `type mismatch`);

// ./test/core/br.wast:646
assert_invalid(
  () => instantiate(`(module (func $$unbound-label (br 1)))`),
  `unknown label`,
);

// ./test/core/br.wast:650
assert_invalid(
  () =>
    instantiate(
      `(module (func $$unbound-nested-label (block (block (br 5)))))`,
    ),
  `unknown label`,
);

// ./test/core/br.wast:654
assert_invalid(
  () => instantiate(`(module (func $$large-label (br 0x10000001)))`),
  `unknown label`,
);
