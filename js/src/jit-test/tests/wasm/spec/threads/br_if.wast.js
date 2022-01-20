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

// ./test/core/br_if.wast

// ./test/core/br_if.wast:3
let $0 = instantiate(`(module
  (func $$dummy)

  (func (export "type-i32")
    (block (drop (i32.ctz (br_if 0 (i32.const 0) (i32.const 1)))))
  )
  (func (export "type-i64")
    (block (drop (i64.ctz (br_if 0 (i64.const 0) (i32.const 1)))))
  )
  (func (export "type-f32")
    (block (drop (f32.neg (br_if 0 (f32.const 0) (i32.const 1)))))
  )
  (func (export "type-f64")
    (block (drop (f64.neg (br_if 0 (f64.const 0) (i32.const 1)))))
  )

  (func (export "type-i32-value") (result i32)
    (block (result i32) (i32.ctz (br_if 0 (i32.const 1) (i32.const 1))))
  )
  (func (export "type-i64-value") (result i64)
    (block (result i64) (i64.ctz (br_if 0 (i64.const 2) (i32.const 1))))
  )
  (func (export "type-f32-value") (result f32)
    (block (result f32) (f32.neg (br_if 0 (f32.const 3) (i32.const 1))))
  )
  (func (export "type-f64-value") (result f64)
    (block (result f64) (f64.neg (br_if 0 (f64.const 4) (i32.const 1))))
  )

  (func (export "as-block-first") (param i32) (result i32)
    (block (br_if 0 (local.get 0)) (return (i32.const 2))) (i32.const 3)
  )
  (func (export "as-block-mid") (param i32) (result i32)
    (block (call $$dummy) (br_if 0 (local.get 0)) (return (i32.const 2)))
    (i32.const 3)
  )
  (func (export "as-block-last") (param i32)
    (block (call $$dummy) (call $$dummy) (br_if 0 (local.get 0)))
  )
  (func (export "as-block-first-value") (param i32) (result i32)
    (block (result i32)
      (drop (br_if 0 (i32.const 10) (local.get 0))) (return (i32.const 11))
    )
  )
  (func (export "as-block-mid-value") (param i32) (result i32)
    (block (result i32)
      (call $$dummy)
      (drop (br_if 0 (i32.const 20) (local.get 0)))
      (return (i32.const 21))
    )
  )
  (func (export "as-block-last-value") (param i32) (result i32)
    (block (result i32)
      (call $$dummy) (call $$dummy) (br_if 0 (i32.const 11) (local.get 0))
    )
  )

  (func (export "as-loop-first") (param i32) (result i32)
    (block (loop (br_if 1 (local.get 0)) (return (i32.const 2)))) (i32.const 3)
  )
  (func (export "as-loop-mid") (param i32) (result i32)
    (block (loop (call $$dummy) (br_if 1 (local.get 0)) (return (i32.const 2))))
    (i32.const 4)
  )
  (func (export "as-loop-last") (param i32)
    (loop (call $$dummy) (br_if 1 (local.get 0)))
  )

  (func (export "as-br-value") (result i32)
    (block (result i32) (br 0 (br_if 0 (i32.const 1) (i32.const 2))))
  )

  (func (export "as-br_if-cond")
    (block (br_if 0 (br_if 0 (i32.const 1) (i32.const 1))))
  )
  (func (export "as-br_if-value") (result i32)
    (block (result i32)
      (drop (br_if 0 (br_if 0 (i32.const 1) (i32.const 2)) (i32.const 3)))
      (i32.const 4)
    )
  )
  (func (export "as-br_if-value-cond") (param i32) (result i32)
    (block (result i32)
      (drop (br_if 0 (i32.const 2) (br_if 0 (i32.const 1) (local.get 0))))
      (i32.const 4)
    )
  )

  (func (export "as-br_table-index")
    (block (br_table 0 0 0 (br_if 0 (i32.const 1) (i32.const 2))))
  )
  (func (export "as-br_table-value") (result i32)
    (block (result i32)
      (br_table 0 0 0 (br_if 0 (i32.const 1) (i32.const 2)) (i32.const 3)) (i32.const 4)
    )
  )
  (func (export "as-br_table-value-index") (result i32)
    (block (result i32)
      (br_table 0 0 (i32.const 2) (br_if 0 (i32.const 1) (i32.const 3))) (i32.const 4)
    )
  )
  (func (export "as-return-value") (result i64)
    (block (result i64) (return (br_if 0 (i64.const 1) (i32.const 2))))
  )

  (func (export "as-if-cond") (param i32) (result i32)
    (block (result i32)
      (if (result i32)
        (br_if 0 (i32.const 1) (local.get 0))
        (then (i32.const 2))
        (else (i32.const 3))
      )
    )
  )
  (func (export "as-if-then") (param i32 i32)
    (block
      (if (local.get 0) (then (br_if 1 (local.get 1))) (else (call $$dummy)))
    )
  )
  (func (export "as-if-else") (param i32 i32)
    (block
      (if (local.get 0) (then (call $$dummy)) (else (br_if 1 (local.get 1))))
    )
  )

  (func (export "as-select-first") (param i32) (result i32)
    (block (result i32)
      (select (br_if 0 (i32.const 3) (i32.const 10)) (i32.const 2) (local.get 0))
    )
  )
  (func (export "as-select-second") (param i32) (result i32)
    (block (result i32)
      (select (i32.const 1) (br_if 0 (i32.const 3) (i32.const 10)) (local.get 0))
    )
  )
  (func (export "as-select-cond") (result i32)
    (block (result i32)
      (select (i32.const 1) (i32.const 2) (br_if 0 (i32.const 3) (i32.const 10)))
    )
  )

 (func $$f (param i32 i32 i32) (result i32) (i32.const -1))
  (func (export "as-call-first") (result i32)
    (block (result i32)
      (call $$f
        (br_if 0 (i32.const 12) (i32.const 1)) (i32.const 2) (i32.const 3)
      )
    )
  )
  (func (export "as-call-mid") (result i32)
    (block (result i32)
      (call $$f
        (i32.const 1) (br_if 0 (i32.const 13) (i32.const 1)) (i32.const 3)
      )
    )
  )
  (func (export "as-call-last") (result i32)
    (block (result i32)
      (call $$f
        (i32.const 1) (i32.const 2) (br_if 0 (i32.const 14) (i32.const 1))
      )
    )
  )

  (func $$func (param i32 i32 i32) (result i32) (local.get 0))
  (type $$check (func (param i32 i32 i32) (result i32)))
  (table funcref (elem $$func))
  (func (export "as-call_indirect-func") (result i32)
    (block (result i32)
      (call_indirect (type $$check)
        (br_if 0 (i32.const 4) (i32.const 10))
        (i32.const 1) (i32.const 2) (i32.const 0)
      )
    )
  )

  (func (export "as-call_indirect-first") (result i32)
    (block (result i32)
      (call_indirect (type $$check)
        (i32.const 1) (br_if 0 (i32.const 4) (i32.const 10)) (i32.const 2) (i32.const 0)
      )
    )
  )
  (func (export "as-call_indirect-mid") (result i32)
    (block (result i32)
      (call_indirect (type $$check)
        (i32.const 1) (i32.const 2) (br_if 0 (i32.const 4) (i32.const 10)) (i32.const 0)
      )
    )
  )
  (func (export "as-call_indirect-last") (result i32)
    (block (result i32)
      (call_indirect (type $$check)
        (i32.const 1) (i32.const 2) (i32.const 3) (br_if 0 (i32.const 4) (i32.const 10))
      )
    )
  )

  (func (export "as-local.set-value") (param i32) (result i32)
    (local i32)
    (block (result i32)
      (local.set 0 (br_if 0 (i32.const 17) (local.get 0)))
      (i32.const -1)
    )
  )
  (func (export "as-local.tee-value") (param i32) (result i32)
    (block (result i32)
      (local.tee 0 (br_if 0 (i32.const 1) (local.get 0)))
      (return (i32.const -1))
    )
  )
  (global $$a (mut i32) (i32.const 10))
  (func (export "as-global.set-value") (param i32) (result i32)
    (block (result i32)
      (global.set $$a (br_if 0 (i32.const 1) (local.get 0)))
      (return (i32.const -1))
    )
  )

  (memory 1)
  (func (export "as-load-address") (result i32)
    (block (result i32) (i32.load (br_if 0 (i32.const 1) (i32.const 1))))
  )
  (func (export "as-loadN-address") (result i32)
    (block (result i32) (i32.load8_s (br_if 0 (i32.const 30) (i32.const 1))))
  )

  (func (export "as-store-address") (result i32)
    (block (result i32)
      (i32.store (br_if 0 (i32.const 30) (i32.const 1)) (i32.const 7)) (i32.const -1)
    )
  )
  (func (export "as-store-value") (result i32)
    (block (result i32)
      (i32.store (i32.const 2) (br_if 0 (i32.const 31) (i32.const 1))) (i32.const -1)
    )
  )

  (func (export "as-storeN-address") (result i32)
    (block (result i32)
      (i32.store8 (br_if 0 (i32.const 32) (i32.const 1)) (i32.const 7)) (i32.const -1)
    )
  )
  (func (export "as-storeN-value") (result i32)
    (block (result i32)
      (i32.store16 (i32.const 2) (br_if 0 (i32.const 33) (i32.const 1))) (i32.const -1)
    )
  )

  (func (export "as-unary-operand") (result f64)
    (block (result f64) (f64.neg (br_if 0 (f64.const 1.0) (i32.const 1))))
  )
  (func (export "as-binary-left") (result i32)
    (block (result i32) (i32.add (br_if 0 (i32.const 1) (i32.const 1)) (i32.const 10)))
  )
  (func (export "as-binary-right") (result i32)
    (block (result i32) (i32.sub (i32.const 10) (br_if 0 (i32.const 1) (i32.const 1))))
  )
  (func (export "as-test-operand") (result i32)
    (block (result i32) (i32.eqz (br_if 0 (i32.const 0) (i32.const 1))))
  )
  (func (export "as-compare-left") (result i32)
    (block (result i32) (i32.le_u (br_if 0 (i32.const 1) (i32.const 1)) (i32.const 10)))
  )
  (func (export "as-compare-right") (result i32)
    (block (result i32) (i32.ne (i32.const 10) (br_if 0 (i32.const 1) (i32.const 42))))
  )

  (func (export "as-memory.grow-size") (result i32)
    (block (result i32) (memory.grow (br_if 0 (i32.const 1) (i32.const 1))))
  )

  (func (export "nested-block-value") (param i32) (result i32)
    (i32.add
      (i32.const 1)
      (block (result i32)
        (drop (i32.const 2))
        (i32.add
          (i32.const 4)
          (block (result i32)
            (drop (br_if 1 (i32.const 8) (local.get 0)))
            (i32.const 16)
          )
        )
      )
    )
  )

  (func (export "nested-br-value") (param i32) (result i32)
    (i32.add
      (i32.const 1)
      (block (result i32)
        (drop (i32.const 2))
        (br 0
          (block (result i32)
            (drop (br_if 1 (i32.const 8) (local.get 0))) (i32.const 4)
          )
        )
        (i32.const 16)
      )
    )
  )

  (func (export "nested-br_if-value") (param i32) (result i32)
    (i32.add
      (i32.const 1)
      (block (result i32)
        (drop (i32.const 2))
        (drop (br_if 0
          (block (result i32)
            (drop (br_if 1 (i32.const 8) (local.get 0))) (i32.const 4)
          )
          (i32.const 1)
        ))
        (i32.const 16)
      )
    )
  )

  (func (export "nested-br_if-value-cond") (param i32) (result i32)
    (i32.add
      (i32.const 1)
      (block (result i32)
        (drop (i32.const 2))
        (drop (br_if 0
          (i32.const 4)
          (block (result i32)
            (drop (br_if 1 (i32.const 8) (local.get 0))) (i32.const 1)
          )
        ))
        (i32.const 16)
      )
    )
  )

  (func (export "nested-br_table-value") (param i32) (result i32)
    (i32.add
      (i32.const 1)
      (block (result i32)
        (drop (i32.const 2))
        (br_table 0
          (block (result i32)
            (drop (br_if 1 (i32.const 8) (local.get 0))) (i32.const 4)
          )
          (i32.const 1)
        )
        (i32.const 16)
      )
    )
  )

  (func (export "nested-br_table-value-index") (param i32) (result i32)
    (i32.add
      (i32.const 1)
      (block (result i32)
        (drop (i32.const 2))
        (br_table 0
          (i32.const 4)
          (block (result i32)
            (drop (br_if 1 (i32.const 8) (local.get 0))) (i32.const 1)
          )
        )
        (i32.const 16)
      )
    )
  )

)`);

// ./test/core/br_if.wast:372
assert_return(() => invoke($0, `type-i32`, []), []);

// ./test/core/br_if.wast:373
assert_return(() => invoke($0, `type-i64`, []), []);

// ./test/core/br_if.wast:374
assert_return(() => invoke($0, `type-f32`, []), []);

// ./test/core/br_if.wast:375
assert_return(() => invoke($0, `type-f64`, []), []);

// ./test/core/br_if.wast:377
assert_return(() => invoke($0, `type-i32-value`, []), [value("i32", 1)]);

// ./test/core/br_if.wast:378
assert_return(() => invoke($0, `type-i64-value`, []), [value("i64", 2n)]);

// ./test/core/br_if.wast:379
assert_return(() => invoke($0, `type-f32-value`, []), [value("f32", 3)]);

// ./test/core/br_if.wast:380
assert_return(() => invoke($0, `type-f64-value`, []), [value("f64", 4)]);

// ./test/core/br_if.wast:382
assert_return(() => invoke($0, `as-block-first`, [0]), [value("i32", 2)]);

// ./test/core/br_if.wast:383
assert_return(() => invoke($0, `as-block-first`, [1]), [value("i32", 3)]);

// ./test/core/br_if.wast:384
assert_return(() => invoke($0, `as-block-mid`, [0]), [value("i32", 2)]);

// ./test/core/br_if.wast:385
assert_return(() => invoke($0, `as-block-mid`, [1]), [value("i32", 3)]);

// ./test/core/br_if.wast:386
assert_return(() => invoke($0, `as-block-last`, [0]), []);

// ./test/core/br_if.wast:387
assert_return(() => invoke($0, `as-block-last`, [1]), []);

// ./test/core/br_if.wast:389
assert_return(() => invoke($0, `as-block-first-value`, [0]), [
  value("i32", 11),
]);

// ./test/core/br_if.wast:390
assert_return(() => invoke($0, `as-block-first-value`, [1]), [
  value("i32", 10),
]);

// ./test/core/br_if.wast:391
assert_return(() => invoke($0, `as-block-mid-value`, [0]), [value("i32", 21)]);

// ./test/core/br_if.wast:392
assert_return(() => invoke($0, `as-block-mid-value`, [1]), [value("i32", 20)]);

// ./test/core/br_if.wast:393
assert_return(() => invoke($0, `as-block-last-value`, [0]), [value("i32", 11)]);

// ./test/core/br_if.wast:394
assert_return(() => invoke($0, `as-block-last-value`, [1]), [value("i32", 11)]);

// ./test/core/br_if.wast:396
assert_return(() => invoke($0, `as-loop-first`, [0]), [value("i32", 2)]);

// ./test/core/br_if.wast:397
assert_return(() => invoke($0, `as-loop-first`, [1]), [value("i32", 3)]);

// ./test/core/br_if.wast:398
assert_return(() => invoke($0, `as-loop-mid`, [0]), [value("i32", 2)]);

// ./test/core/br_if.wast:399
assert_return(() => invoke($0, `as-loop-mid`, [1]), [value("i32", 4)]);

// ./test/core/br_if.wast:400
assert_return(() => invoke($0, `as-loop-last`, [0]), []);

// ./test/core/br_if.wast:401
assert_return(() => invoke($0, `as-loop-last`, [1]), []);

// ./test/core/br_if.wast:403
assert_return(() => invoke($0, `as-br-value`, []), [value("i32", 1)]);

// ./test/core/br_if.wast:405
assert_return(() => invoke($0, `as-br_if-cond`, []), []);

// ./test/core/br_if.wast:406
assert_return(() => invoke($0, `as-br_if-value`, []), [value("i32", 1)]);

// ./test/core/br_if.wast:407
assert_return(() => invoke($0, `as-br_if-value-cond`, [0]), [value("i32", 2)]);

// ./test/core/br_if.wast:408
assert_return(() => invoke($0, `as-br_if-value-cond`, [1]), [value("i32", 1)]);

// ./test/core/br_if.wast:410
assert_return(() => invoke($0, `as-br_table-index`, []), []);

// ./test/core/br_if.wast:411
assert_return(() => invoke($0, `as-br_table-value`, []), [value("i32", 1)]);

// ./test/core/br_if.wast:412
assert_return(() => invoke($0, `as-br_table-value-index`, []), [
  value("i32", 1),
]);

// ./test/core/br_if.wast:414
assert_return(() => invoke($0, `as-return-value`, []), [value("i64", 1n)]);

// ./test/core/br_if.wast:416
assert_return(() => invoke($0, `as-if-cond`, [0]), [value("i32", 2)]);

// ./test/core/br_if.wast:417
assert_return(() => invoke($0, `as-if-cond`, [1]), [value("i32", 1)]);

// ./test/core/br_if.wast:418
assert_return(() => invoke($0, `as-if-then`, [0, 0]), []);

// ./test/core/br_if.wast:419
assert_return(() => invoke($0, `as-if-then`, [4, 0]), []);

// ./test/core/br_if.wast:420
assert_return(() => invoke($0, `as-if-then`, [0, 1]), []);

// ./test/core/br_if.wast:421
assert_return(() => invoke($0, `as-if-then`, [4, 1]), []);

// ./test/core/br_if.wast:422
assert_return(() => invoke($0, `as-if-else`, [0, 0]), []);

// ./test/core/br_if.wast:423
assert_return(() => invoke($0, `as-if-else`, [3, 0]), []);

// ./test/core/br_if.wast:424
assert_return(() => invoke($0, `as-if-else`, [0, 1]), []);

// ./test/core/br_if.wast:425
assert_return(() => invoke($0, `as-if-else`, [3, 1]), []);

// ./test/core/br_if.wast:427
assert_return(() => invoke($0, `as-select-first`, [0]), [value("i32", 3)]);

// ./test/core/br_if.wast:428
assert_return(() => invoke($0, `as-select-first`, [1]), [value("i32", 3)]);

// ./test/core/br_if.wast:429
assert_return(() => invoke($0, `as-select-second`, [0]), [value("i32", 3)]);

// ./test/core/br_if.wast:430
assert_return(() => invoke($0, `as-select-second`, [1]), [value("i32", 3)]);

// ./test/core/br_if.wast:431
assert_return(() => invoke($0, `as-select-cond`, []), [value("i32", 3)]);

// ./test/core/br_if.wast:433
assert_return(() => invoke($0, `as-call-first`, []), [value("i32", 12)]);

// ./test/core/br_if.wast:434
assert_return(() => invoke($0, `as-call-mid`, []), [value("i32", 13)]);

// ./test/core/br_if.wast:435
assert_return(() => invoke($0, `as-call-last`, []), [value("i32", 14)]);

// ./test/core/br_if.wast:437
assert_return(() => invoke($0, `as-call_indirect-func`, []), [value("i32", 4)]);

// ./test/core/br_if.wast:438
assert_return(() => invoke($0, `as-call_indirect-first`, []), [
  value("i32", 4),
]);

// ./test/core/br_if.wast:439
assert_return(() => invoke($0, `as-call_indirect-mid`, []), [value("i32", 4)]);

// ./test/core/br_if.wast:440
assert_return(() => invoke($0, `as-call_indirect-last`, []), [value("i32", 4)]);

// ./test/core/br_if.wast:442
assert_return(() => invoke($0, `as-local.set-value`, [0]), [value("i32", -1)]);

// ./test/core/br_if.wast:443
assert_return(() => invoke($0, `as-local.set-value`, [1]), [value("i32", 17)]);

// ./test/core/br_if.wast:445
assert_return(() => invoke($0, `as-local.tee-value`, [0]), [value("i32", -1)]);

// ./test/core/br_if.wast:446
assert_return(() => invoke($0, `as-local.tee-value`, [1]), [value("i32", 1)]);

// ./test/core/br_if.wast:448
assert_return(() => invoke($0, `as-global.set-value`, [0]), [value("i32", -1)]);

// ./test/core/br_if.wast:449
assert_return(() => invoke($0, `as-global.set-value`, [1]), [value("i32", 1)]);

// ./test/core/br_if.wast:451
assert_return(() => invoke($0, `as-load-address`, []), [value("i32", 1)]);

// ./test/core/br_if.wast:452
assert_return(() => invoke($0, `as-loadN-address`, []), [value("i32", 30)]);

// ./test/core/br_if.wast:454
assert_return(() => invoke($0, `as-store-address`, []), [value("i32", 30)]);

// ./test/core/br_if.wast:455
assert_return(() => invoke($0, `as-store-value`, []), [value("i32", 31)]);

// ./test/core/br_if.wast:456
assert_return(() => invoke($0, `as-storeN-address`, []), [value("i32", 32)]);

// ./test/core/br_if.wast:457
assert_return(() => invoke($0, `as-storeN-value`, []), [value("i32", 33)]);

// ./test/core/br_if.wast:459
assert_return(() => invoke($0, `as-unary-operand`, []), [value("f64", 1)]);

// ./test/core/br_if.wast:460
assert_return(() => invoke($0, `as-binary-left`, []), [value("i32", 1)]);

// ./test/core/br_if.wast:461
assert_return(() => invoke($0, `as-binary-right`, []), [value("i32", 1)]);

// ./test/core/br_if.wast:462
assert_return(() => invoke($0, `as-test-operand`, []), [value("i32", 0)]);

// ./test/core/br_if.wast:463
assert_return(() => invoke($0, `as-compare-left`, []), [value("i32", 1)]);

// ./test/core/br_if.wast:464
assert_return(() => invoke($0, `as-compare-right`, []), [value("i32", 1)]);

// ./test/core/br_if.wast:465
assert_return(() => invoke($0, `as-memory.grow-size`, []), [value("i32", 1)]);

// ./test/core/br_if.wast:467
assert_return(() => invoke($0, `nested-block-value`, [0]), [value("i32", 21)]);

// ./test/core/br_if.wast:468
assert_return(() => invoke($0, `nested-block-value`, [1]), [value("i32", 9)]);

// ./test/core/br_if.wast:469
assert_return(() => invoke($0, `nested-br-value`, [0]), [value("i32", 5)]);

// ./test/core/br_if.wast:470
assert_return(() => invoke($0, `nested-br-value`, [1]), [value("i32", 9)]);

// ./test/core/br_if.wast:471
assert_return(() => invoke($0, `nested-br_if-value`, [0]), [value("i32", 5)]);

// ./test/core/br_if.wast:472
assert_return(() => invoke($0, `nested-br_if-value`, [1]), [value("i32", 9)]);

// ./test/core/br_if.wast:473
assert_return(() => invoke($0, `nested-br_if-value-cond`, [0]), [
  value("i32", 5),
]);

// ./test/core/br_if.wast:474
assert_return(() => invoke($0, `nested-br_if-value-cond`, [1]), [
  value("i32", 9),
]);

// ./test/core/br_if.wast:475
assert_return(() => invoke($0, `nested-br_table-value`, [0]), [
  value("i32", 5),
]);

// ./test/core/br_if.wast:476
assert_return(() => invoke($0, `nested-br_table-value`, [1]), [
  value("i32", 9),
]);

// ./test/core/br_if.wast:477
assert_return(() => invoke($0, `nested-br_table-value-index`, [0]), [
  value("i32", 5),
]);

// ./test/core/br_if.wast:478
assert_return(() => invoke($0, `nested-br_table-value-index`, [1]), [
  value("i32", 9),
]);

// ./test/core/br_if.wast:480
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-false-i32 (block (i32.ctz (br_if 0 (i32.const 0))))))`,
    ),
  `type mismatch`,
);

// ./test/core/br_if.wast:484
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-false-i64 (block (i64.ctz (br_if 0 (i32.const 0))))))`,
    ),
  `type mismatch`,
);

// ./test/core/br_if.wast:488
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-false-f32 (block (f32.neg (br_if 0 (i32.const 0))))))`,
    ),
  `type mismatch`,
);

// ./test/core/br_if.wast:492
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-false-f64 (block (f64.neg (br_if 0 (i32.const 0))))))`,
    ),
  `type mismatch`,
);

// ./test/core/br_if.wast:497
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-true-i32 (block (i32.ctz (br_if 0 (i32.const 1))))))`,
    ),
  `type mismatch`,
);

// ./test/core/br_if.wast:501
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-true-i64 (block (i64.ctz (br_if 0 (i64.const 1))))))`,
    ),
  `type mismatch`,
);

// ./test/core/br_if.wast:505
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-true-f32 (block (f32.neg (br_if 0 (f32.const 1))))))`,
    ),
  `type mismatch`,
);

// ./test/core/br_if.wast:509
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-true-f64 (block (f64.neg (br_if 0 (i64.const 1))))))`,
    ),
  `type mismatch`,
);

// ./test/core/br_if.wast:514
assert_invalid(
  () =>
    instantiate(`(module (func $$type-false-arg-void-vs-num (result i32)
    (block (result i32) (br_if 0 (i32.const 0)) (i32.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/br_if.wast:520
assert_invalid(
  () =>
    instantiate(`(module (func $$type-true-arg-void-vs-num (result i32)
    (block (result i32) (br_if 0 (i32.const 1)) (i32.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/br_if.wast:526
assert_invalid(() =>
  instantiate(`(module (func $$type-false-arg-num-vs-void
    (block (br_if 0 (i32.const 0) (i32.const 0)))
  ))`), `type mismatch`);

// ./test/core/br_if.wast:532
assert_invalid(() =>
  instantiate(`(module (func $$type-true-arg-num-vs-void
    (block (br_if 0 (i32.const 0) (i32.const 1)))
  ))`), `type mismatch`);

// ./test/core/br_if.wast:539
assert_invalid(
  () =>
    instantiate(`(module (func $$type-false-arg-void-vs-num (result i32)
    (block (result i32) (br_if 0 (nop) (i32.const 0)) (i32.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/br_if.wast:545
assert_invalid(
  () =>
    instantiate(`(module (func $$type-true-arg-void-vs-num (result i32)
    (block (result i32) (br_if 0 (nop) (i32.const 1)) (i32.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/br_if.wast:551
assert_invalid(
  () =>
    instantiate(`(module (func $$type-false-arg-num-vs-num (result i32)
    (block (result i32)
      (drop (br_if 0 (i64.const 1) (i32.const 0))) (i32.const 1)
    )
  ))`),
  `type mismatch`,
);

// ./test/core/br_if.wast:559
assert_invalid(
  () =>
    instantiate(`(module (func $$type-true-arg-num-vs-num (result i32)
    (block (result i32)
      (drop (br_if 0 (i64.const 1) (i32.const 0))) (i32.const 1)
    )
  ))`),
  `type mismatch`,
);

// ./test/core/br_if.wast:568
assert_invalid(() =>
  instantiate(`(module (func $$type-cond-empty-vs-i32
    (block (br_if 0))
  ))`), `type mismatch`);

// ./test/core/br_if.wast:574
assert_invalid(() =>
  instantiate(`(module (func $$type-cond-void-vs-i32
    (block (br_if 0 (nop)))
  ))`), `type mismatch`);

// ./test/core/br_if.wast:580
assert_invalid(() =>
  instantiate(`(module (func $$type-cond-num-vs-i32
    (block (br_if 0 (i64.const 0)))
  ))`), `type mismatch`);

// ./test/core/br_if.wast:586
assert_invalid(
  () =>
    instantiate(`(module (func $$type-arg-cond-void-vs-i32 (result i32)
    (block (result i32) (br_if 0 (i32.const 0) (nop)) (i32.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/br_if.wast:592
assert_invalid(
  () =>
    instantiate(`(module (func $$type-arg-void-vs-num-nested (result i32)
    (block (result i32) (i32.const 0) (block (br_if 1 (i32.const 1))))
  ))`),
  `type mismatch`,
);

// ./test/core/br_if.wast:598
assert_invalid(
  () =>
    instantiate(`(module (func $$type-arg-cond-num-vs-i32 (result i32)
    (block (result i32) (br_if 0 (i32.const 0) (i64.const 0)) (i32.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/br_if.wast:605
assert_invalid(() =>
  instantiate(`(module
    (func $$type-1st-cond-empty-in-then
      (block
        (i32.const 0) (i32.const 0)
        (if (result i32) (then (br_if 0)))
      )
      (i32.eqz) (drop)
    )
  )`), `type mismatch`);

// ./test/core/br_if.wast:617
assert_invalid(() =>
  instantiate(`(module
    (func $$type-2nd-cond-empty-in-then
      (block
        (i32.const 0) (i32.const 0)
        (if (result i32) (then (br_if 0 (i32.const 1))))
      )
      (i32.eqz) (drop)
    )
  )`), `type mismatch`);

// ./test/core/br_if.wast:629
assert_invalid(() =>
  instantiate(`(module
    (func $$type-1st-cond-empty-in-return
      (block (result i32)
        (return (br_if 0))
      )
      (i32.eqz) (drop)
    )
  )`), `type mismatch`);

// ./test/core/br_if.wast:640
assert_invalid(() =>
  instantiate(`(module
    (func $$type-2nd-cond-empty-in-return
      (block (result i32)
        (return (br_if 0 (i32.const 1)))
      )
      (i32.eqz) (drop)
    )
  )`), `type mismatch`);

// ./test/core/br_if.wast:653
assert_invalid(
  () => instantiate(`(module (func $$unbound-label (br_if 1 (i32.const 1))))`),
  `unknown label`,
);

// ./test/core/br_if.wast:657
assert_invalid(
  () =>
    instantiate(
      `(module (func $$unbound-nested-label (block (block (br_if 5 (i32.const 1))))))`,
    ),
  `unknown label`,
);

// ./test/core/br_if.wast:661
assert_invalid(
  () =>
    instantiate(
      `(module (func $$large-label (br_if 0x10000001 (i32.const 1))))`,
    ),
  `unknown label`,
);
