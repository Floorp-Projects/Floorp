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

// ./test/core/if.wast

// ./test/core/if.wast:3
let $0 = instantiate(`(module
  ;; Auxiliary definition
  (memory 1)

  (func $$dummy)

  (func (export "empty") (param i32)
    (if (local.get 0) (then))
    (if (local.get 0) (then) (else))
    (if $$l (local.get 0) (then))
    (if $$l (local.get 0) (then) (else))
  )

  (func (export "singular") (param i32) (result i32)
    (if (local.get 0) (then (nop)))
    (if (local.get 0) (then (nop)) (else (nop)))
    (if (result i32) (local.get 0) (then (i32.const 7)) (else (i32.const 8)))
  )

  (func (export "multi") (param i32) (result i32)
    (if (local.get 0) (then (call $$dummy) (call $$dummy) (call $$dummy)))
    (if (local.get 0) (then) (else (call $$dummy) (call $$dummy) (call $$dummy)))
    (if (result i32) (local.get 0)
      (then (call $$dummy) (call $$dummy) (i32.const 8))
      (else (call $$dummy) (call $$dummy) (i32.const 9))
    )
  )

  (func (export "nested") (param i32 i32) (result i32)
    (if (result i32) (local.get 0)
      (then
        (if (local.get 1) (then (call $$dummy) (block) (nop)))
        (if (local.get 1) (then) (else (call $$dummy) (block) (nop)))
        (if (result i32) (local.get 1)
          (then (call $$dummy) (i32.const 9))
          (else (call $$dummy) (i32.const 10))
        )
      )
      (else
        (if (local.get 1) (then (call $$dummy) (block) (nop)))
        (if (local.get 1) (then) (else (call $$dummy) (block) (nop)))
        (if (result i32) (local.get 1)
          (then (call $$dummy) (i32.const 10))
          (else (call $$dummy) (i32.const 11))
        )
      )
    )
  )

  (func (export "as-select-first") (param i32) (result i32)
    (select
      (if (result i32) (local.get 0)
        (then (call $$dummy) (i32.const 1))
        (else (call $$dummy) (i32.const 0))
      )
      (i32.const 2) (i32.const 3)
    )
  )
  (func (export "as-select-mid") (param i32) (result i32)
    (select
      (i32.const 2)
      (if (result i32) (local.get 0)
        (then (call $$dummy) (i32.const 1))
        (else (call $$dummy) (i32.const 0))
      )
      (i32.const 3)
    )
  )
  (func (export "as-select-last") (param i32) (result i32)
    (select
      (i32.const 2) (i32.const 3)
      (if (result i32) (local.get 0)
        (then (call $$dummy) (i32.const 1))
        (else (call $$dummy) (i32.const 0))
      )
    )
  )

  (func (export "as-loop-first") (param i32) (result i32)
    (loop (result i32)
      (if (result i32) (local.get 0)
        (then (call $$dummy) (i32.const 1))
        (else (call $$dummy) (i32.const 0))
      )
      (call $$dummy) (call $$dummy)
    )
  )
  (func (export "as-loop-mid") (param i32) (result i32)
    (loop (result i32)
      (call $$dummy)
      (if (result i32) (local.get 0)
        (then (call $$dummy) (i32.const 1))
        (else (call $$dummy) (i32.const 0))
      )
      (call $$dummy)
    )
  )
  (func (export "as-loop-last") (param i32) (result i32)
    (loop (result i32)
      (call $$dummy) (call $$dummy)
      (if (result i32) (local.get 0)
        (then (call $$dummy) (i32.const 1))
        (else (call $$dummy) (i32.const 0))
      )
    )
  )

  (func (export "as-if-condition") (param i32) (result i32)
    (if (result i32)
      (if (result i32) (local.get 0)
        (then (i32.const 1)) (else (i32.const 0))
      )
      (then (call $$dummy) (i32.const 2))
      (else (call $$dummy) (i32.const 3))
    )
  )

  (func (export "as-br_if-first") (param i32) (result i32)
    (block (result i32)
      (br_if 0
        (if (result i32) (local.get 0)
          (then (call $$dummy) (i32.const 1))
          (else (call $$dummy) (i32.const 0))
        )
        (i32.const 2)
      )
      (return (i32.const 3))
    )
  )
  (func (export "as-br_if-last") (param i32) (result i32)
    (block (result i32)
      (br_if 0
        (i32.const 2)
        (if (result i32) (local.get 0)
          (then (call $$dummy) (i32.const 1))
          (else (call $$dummy) (i32.const 0))
        )
      )
      (return (i32.const 3))
    )
  )

  (func (export "as-br_table-first") (param i32) (result i32)
    (block (result i32)
      (if (result i32) (local.get 0)
        (then (call $$dummy) (i32.const 1))
        (else (call $$dummy) (i32.const 0))
      )
      (i32.const 2)
      (br_table 0 0)
    )
  )
  (func (export "as-br_table-last") (param i32) (result i32)
    (block (result i32)
      (i32.const 2)
      (if (result i32) (local.get 0)
        (then (call $$dummy) (i32.const 1))
        (else (call $$dummy) (i32.const 0))
      )
      (br_table 0 0)
    )
  )

  (func $$func (param i32 i32) (result i32) (local.get 0))
  (type $$check (func (param i32 i32) (result i32)))
  (table funcref (elem $$func))
  (func (export "as-call_indirect-first") (param i32) (result i32)
    (block (result i32)
      (call_indirect (type $$check)
        (if (result i32) (local.get 0)
          (then (call $$dummy) (i32.const 1))
          (else (call $$dummy) (i32.const 0))
        )
        (i32.const 2) (i32.const 0)
      )
    )
  )
  (func (export "as-call_indirect-mid") (param i32) (result i32)
    (block (result i32)
      (call_indirect (type $$check)
        (i32.const 2)
        (if (result i32) (local.get 0)
          (then (call $$dummy) (i32.const 1))
          (else (call $$dummy) (i32.const 0))
        )
        (i32.const 0)
      )
    )
  )
  (func (export "as-call_indirect-last") (param i32) (result i32)
    (block (result i32)
      (call_indirect (type $$check)
        (i32.const 2) (i32.const 0)
        (if (result i32) (local.get 0)
          (then (call $$dummy) (i32.const 1))
          (else (call $$dummy) (i32.const 0))
        )
      )
    )
  )

  (func (export "as-store-first") (param i32)
    (if (result i32) (local.get 0)
      (then (call $$dummy) (i32.const 1))
      (else (call $$dummy) (i32.const 0))
    )
    (i32.const 2)
    (i32.store)
  )
  (func (export "as-store-last") (param i32)
    (i32.const 2)
    (if (result i32) (local.get 0)
      (then (call $$dummy) (i32.const 1))
      (else (call $$dummy) (i32.const 0))
    )
    (i32.store)
  )

  (func (export "as-memory.grow-value") (param i32) (result i32)
    (memory.grow
      (if (result i32) (local.get 0)
        (then (i32.const 1))
        (else (i32.const 0))
      )
    )
  )

  (func $$f (param i32) (result i32) (local.get 0))

  (func (export "as-call-value") (param i32) (result i32)
    (call $$f
      (if (result i32) (local.get 0)
        (then (i32.const 1))
        (else (i32.const 0))
      )
    )
  )
  (func (export "as-return-value") (param i32) (result i32)
    (if (result i32) (local.get 0)
      (then (i32.const 1))
      (else (i32.const 0)))
    (return)
  )
  (func (export "as-drop-operand") (param i32)
    (drop
      (if (result i32) (local.get 0)
        (then (i32.const 1))
        (else (i32.const 0))
      )
    )
  )
  (func (export "as-br-value") (param i32) (result i32)
    (block (result i32)
      (br 0
        (if (result i32) (local.get 0)
          (then (i32.const 1))
          (else (i32.const 0))
        )
      )
    )
  )
  (func (export "as-local.set-value") (param i32) (result i32)
    (local i32)
    (local.set 0
      (if (result i32) (local.get 0)
        (then (i32.const 1))
        (else (i32.const 0))
      )
    )
    (local.get 0)
  )
  (func (export "as-local.tee-value") (param i32) (result i32)
    (local.tee 0
      (if (result i32) (local.get 0)
        (then (i32.const 1))
        (else (i32.const 0))
      )
    )
  )
  (global $$a (mut i32) (i32.const 10))
  (func (export "as-global.set-value") (param i32) (result i32)
    (global.set $$a
      (if (result i32) (local.get 0)
        (then (i32.const 1))
        (else (i32.const 0))
      )
    ) (global.get $$a)
  )
  (func (export "as-load-operand") (param i32) (result i32)
    (i32.load
      (if (result i32) (local.get 0)
        (then (i32.const 11))
        (else (i32.const 10))
      )
    )
  )

  (func (export "as-unary-operand") (param i32) (result i32)
    (i32.ctz
      (if (result i32) (local.get 0)
        (then (call $$dummy) (i32.const 13))
        (else (call $$dummy) (i32.const -13))
      )
    )
  )
  (func (export "as-binary-operand") (param i32 i32) (result i32)
    (i32.mul
      (if (result i32) (local.get 0)
        (then (call $$dummy) (i32.const 3))
        (else (call $$dummy) (i32.const -3))
      )
      (if (result i32) (local.get 1)
        (then (call $$dummy) (i32.const 4))
        (else (call $$dummy) (i32.const -5))
      )
    )
  )
  (func (export "as-test-operand") (param i32) (result i32)
    (i32.eqz
      (if (result i32) (local.get 0)
        (then (call $$dummy) (i32.const 13))
        (else (call $$dummy) (i32.const 0))
      )
    )
  )
  (func (export "as-compare-operand") (param i32 i32) (result i32)
    (f32.gt
      (if (result f32) (local.get 0)
        (then (call $$dummy) (f32.const 3))
        (else (call $$dummy) (f32.const -3))
      )
      (if (result f32) (local.get 1)
        (then (call $$dummy) (f32.const 4))
        (else (call $$dummy) (f32.const -4))
      )
    )
  )

  (func (export "break-bare") (result i32)
    (if (i32.const 1) (then (br 0) (unreachable)))
    (if (i32.const 1) (then (br 0) (unreachable)) (else (unreachable)))
    (if (i32.const 0) (then (unreachable)) (else (br 0) (unreachable)))
    (if (i32.const 1) (then (br_if 0 (i32.const 1)) (unreachable)))
    (if (i32.const 1) (then (br_if 0 (i32.const 1)) (unreachable)) (else (unreachable)))
    (if (i32.const 0) (then (unreachable)) (else (br_if 0 (i32.const 1)) (unreachable)))
    (if (i32.const 1) (then (br_table 0 (i32.const 0)) (unreachable)))
    (if (i32.const 1) (then (br_table 0 (i32.const 0)) (unreachable)) (else (unreachable)))
    (if (i32.const 0) (then (unreachable)) (else (br_table 0 (i32.const 0)) (unreachable)))
    (i32.const 19)
  )

  (func (export "break-value") (param i32) (result i32)
    (if (result i32) (local.get 0)
      (then (br 0 (i32.const 18)) (i32.const 19))
      (else (br 0 (i32.const 21)) (i32.const 20))
    )
  )

  (func (export "effects") (param i32) (result i32)
    (local i32)
    (if
      (block (result i32) (local.set 1 (i32.const 1)) (local.get 0))
      (then
        (local.set 1 (i32.mul (local.get 1) (i32.const 3)))
        (local.set 1 (i32.sub (local.get 1) (i32.const 5)))
        (local.set 1 (i32.mul (local.get 1) (i32.const 7)))
        (br 0)
        (local.set 1 (i32.mul (local.get 1) (i32.const 100)))
      )
      (else
        (local.set 1 (i32.mul (local.get 1) (i32.const 5)))
        (local.set 1 (i32.sub (local.get 1) (i32.const 7)))
        (local.set 1 (i32.mul (local.get 1) (i32.const 3)))
        (br 0)
        (local.set 1 (i32.mul (local.get 1) (i32.const 1000)))
      )
    )
    (local.get 1)
  )
)`);

// ./test/core/if.wast:384
assert_return(() => invoke($0, `empty`, [0]), []);

// ./test/core/if.wast:385
assert_return(() => invoke($0, `empty`, [1]), []);

// ./test/core/if.wast:386
assert_return(() => invoke($0, `empty`, [100]), []);

// ./test/core/if.wast:387
assert_return(() => invoke($0, `empty`, [-2]), []);

// ./test/core/if.wast:389
assert_return(() => invoke($0, `singular`, [0]), [value("i32", 8)]);

// ./test/core/if.wast:390
assert_return(() => invoke($0, `singular`, [1]), [value("i32", 7)]);

// ./test/core/if.wast:391
assert_return(() => invoke($0, `singular`, [10]), [value("i32", 7)]);

// ./test/core/if.wast:392
assert_return(() => invoke($0, `singular`, [-10]), [value("i32", 7)]);

// ./test/core/if.wast:394
assert_return(() => invoke($0, `multi`, [0]), [value("i32", 9)]);

// ./test/core/if.wast:395
assert_return(() => invoke($0, `multi`, [1]), [value("i32", 8)]);

// ./test/core/if.wast:396
assert_return(() => invoke($0, `multi`, [13]), [value("i32", 8)]);

// ./test/core/if.wast:397
assert_return(() => invoke($0, `multi`, [-5]), [value("i32", 8)]);

// ./test/core/if.wast:399
assert_return(() => invoke($0, `nested`, [0, 0]), [value("i32", 11)]);

// ./test/core/if.wast:400
assert_return(() => invoke($0, `nested`, [1, 0]), [value("i32", 10)]);

// ./test/core/if.wast:401
assert_return(() => invoke($0, `nested`, [0, 1]), [value("i32", 10)]);

// ./test/core/if.wast:402
assert_return(() => invoke($0, `nested`, [3, 2]), [value("i32", 9)]);

// ./test/core/if.wast:403
assert_return(() => invoke($0, `nested`, [0, -100]), [value("i32", 10)]);

// ./test/core/if.wast:404
assert_return(() => invoke($0, `nested`, [10, 10]), [value("i32", 9)]);

// ./test/core/if.wast:405
assert_return(() => invoke($0, `nested`, [0, -1]), [value("i32", 10)]);

// ./test/core/if.wast:406
assert_return(() => invoke($0, `nested`, [-111, -2]), [value("i32", 9)]);

// ./test/core/if.wast:408
assert_return(() => invoke($0, `as-select-first`, [0]), [value("i32", 0)]);

// ./test/core/if.wast:409
assert_return(() => invoke($0, `as-select-first`, [1]), [value("i32", 1)]);

// ./test/core/if.wast:410
assert_return(() => invoke($0, `as-select-mid`, [0]), [value("i32", 2)]);

// ./test/core/if.wast:411
assert_return(() => invoke($0, `as-select-mid`, [1]), [value("i32", 2)]);

// ./test/core/if.wast:412
assert_return(() => invoke($0, `as-select-last`, [0]), [value("i32", 3)]);

// ./test/core/if.wast:413
assert_return(() => invoke($0, `as-select-last`, [1]), [value("i32", 2)]);

// ./test/core/if.wast:415
assert_return(() => invoke($0, `as-loop-first`, [0]), [value("i32", 0)]);

// ./test/core/if.wast:416
assert_return(() => invoke($0, `as-loop-first`, [1]), [value("i32", 1)]);

// ./test/core/if.wast:417
assert_return(() => invoke($0, `as-loop-mid`, [0]), [value("i32", 0)]);

// ./test/core/if.wast:418
assert_return(() => invoke($0, `as-loop-mid`, [1]), [value("i32", 1)]);

// ./test/core/if.wast:419
assert_return(() => invoke($0, `as-loop-last`, [0]), [value("i32", 0)]);

// ./test/core/if.wast:420
assert_return(() => invoke($0, `as-loop-last`, [1]), [value("i32", 1)]);

// ./test/core/if.wast:422
assert_return(() => invoke($0, `as-if-condition`, [0]), [value("i32", 3)]);

// ./test/core/if.wast:423
assert_return(() => invoke($0, `as-if-condition`, [1]), [value("i32", 2)]);

// ./test/core/if.wast:425
assert_return(() => invoke($0, `as-br_if-first`, [0]), [value("i32", 0)]);

// ./test/core/if.wast:426
assert_return(() => invoke($0, `as-br_if-first`, [1]), [value("i32", 1)]);

// ./test/core/if.wast:427
assert_return(() => invoke($0, `as-br_if-last`, [0]), [value("i32", 3)]);

// ./test/core/if.wast:428
assert_return(() => invoke($0, `as-br_if-last`, [1]), [value("i32", 2)]);

// ./test/core/if.wast:430
assert_return(() => invoke($0, `as-br_table-first`, [0]), [value("i32", 0)]);

// ./test/core/if.wast:431
assert_return(() => invoke($0, `as-br_table-first`, [1]), [value("i32", 1)]);

// ./test/core/if.wast:432
assert_return(() => invoke($0, `as-br_table-last`, [0]), [value("i32", 2)]);

// ./test/core/if.wast:433
assert_return(() => invoke($0, `as-br_table-last`, [1]), [value("i32", 2)]);

// ./test/core/if.wast:435
assert_return(() => invoke($0, `as-call_indirect-first`, [0]), [
  value("i32", 0),
]);

// ./test/core/if.wast:436
assert_return(() => invoke($0, `as-call_indirect-first`, [1]), [
  value("i32", 1),
]);

// ./test/core/if.wast:437
assert_return(() => invoke($0, `as-call_indirect-mid`, [0]), [value("i32", 2)]);

// ./test/core/if.wast:438
assert_return(() => invoke($0, `as-call_indirect-mid`, [1]), [value("i32", 2)]);

// ./test/core/if.wast:439
assert_return(() => invoke($0, `as-call_indirect-last`, [0]), [
  value("i32", 2),
]);

// ./test/core/if.wast:440
assert_trap(
  () => invoke($0, `as-call_indirect-last`, [1]),
  `undefined element`,
);

// ./test/core/if.wast:442
assert_return(() => invoke($0, `as-store-first`, [0]), []);

// ./test/core/if.wast:443
assert_return(() => invoke($0, `as-store-first`, [1]), []);

// ./test/core/if.wast:444
assert_return(() => invoke($0, `as-store-last`, [0]), []);

// ./test/core/if.wast:445
assert_return(() => invoke($0, `as-store-last`, [1]), []);

// ./test/core/if.wast:447
assert_return(() => invoke($0, `as-memory.grow-value`, [0]), [value("i32", 1)]);

// ./test/core/if.wast:448
assert_return(() => invoke($0, `as-memory.grow-value`, [1]), [value("i32", 1)]);

// ./test/core/if.wast:450
assert_return(() => invoke($0, `as-call-value`, [0]), [value("i32", 0)]);

// ./test/core/if.wast:451
assert_return(() => invoke($0, `as-call-value`, [1]), [value("i32", 1)]);

// ./test/core/if.wast:453
assert_return(() => invoke($0, `as-return-value`, [0]), [value("i32", 0)]);

// ./test/core/if.wast:454
assert_return(() => invoke($0, `as-return-value`, [1]), [value("i32", 1)]);

// ./test/core/if.wast:456
assert_return(() => invoke($0, `as-drop-operand`, [0]), []);

// ./test/core/if.wast:457
assert_return(() => invoke($0, `as-drop-operand`, [1]), []);

// ./test/core/if.wast:459
assert_return(() => invoke($0, `as-br-value`, [0]), [value("i32", 0)]);

// ./test/core/if.wast:460
assert_return(() => invoke($0, `as-br-value`, [1]), [value("i32", 1)]);

// ./test/core/if.wast:462
assert_return(() => invoke($0, `as-local.set-value`, [0]), [value("i32", 0)]);

// ./test/core/if.wast:463
assert_return(() => invoke($0, `as-local.set-value`, [1]), [value("i32", 1)]);

// ./test/core/if.wast:465
assert_return(() => invoke($0, `as-local.tee-value`, [0]), [value("i32", 0)]);

// ./test/core/if.wast:466
assert_return(() => invoke($0, `as-local.tee-value`, [1]), [value("i32", 1)]);

// ./test/core/if.wast:468
assert_return(() => invoke($0, `as-global.set-value`, [0]), [value("i32", 0)]);

// ./test/core/if.wast:469
assert_return(() => invoke($0, `as-global.set-value`, [1]), [value("i32", 1)]);

// ./test/core/if.wast:471
assert_return(() => invoke($0, `as-load-operand`, [0]), [value("i32", 0)]);

// ./test/core/if.wast:472
assert_return(() => invoke($0, `as-load-operand`, [1]), [value("i32", 0)]);

// ./test/core/if.wast:474
assert_return(() => invoke($0, `as-unary-operand`, [0]), [value("i32", 0)]);

// ./test/core/if.wast:475
assert_return(() => invoke($0, `as-unary-operand`, [1]), [value("i32", 0)]);

// ./test/core/if.wast:476
assert_return(() => invoke($0, `as-unary-operand`, [-1]), [value("i32", 0)]);

// ./test/core/if.wast:478
assert_return(() => invoke($0, `as-binary-operand`, [0, 0]), [
  value("i32", 15),
]);

// ./test/core/if.wast:479
assert_return(() => invoke($0, `as-binary-operand`, [0, 1]), [
  value("i32", -12),
]);

// ./test/core/if.wast:480
assert_return(() => invoke($0, `as-binary-operand`, [1, 0]), [
  value("i32", -15),
]);

// ./test/core/if.wast:481
assert_return(() => invoke($0, `as-binary-operand`, [1, 1]), [
  value("i32", 12),
]);

// ./test/core/if.wast:483
assert_return(() => invoke($0, `as-test-operand`, [0]), [value("i32", 1)]);

// ./test/core/if.wast:484
assert_return(() => invoke($0, `as-test-operand`, [1]), [value("i32", 0)]);

// ./test/core/if.wast:486
assert_return(() => invoke($0, `as-compare-operand`, [0, 0]), [
  value("i32", 1),
]);

// ./test/core/if.wast:487
assert_return(() => invoke($0, `as-compare-operand`, [0, 1]), [
  value("i32", 0),
]);

// ./test/core/if.wast:488
assert_return(() => invoke($0, `as-compare-operand`, [1, 0]), [
  value("i32", 1),
]);

// ./test/core/if.wast:489
assert_return(() => invoke($0, `as-compare-operand`, [1, 1]), [
  value("i32", 0),
]);

// ./test/core/if.wast:491
assert_return(() => invoke($0, `break-bare`, []), [value("i32", 19)]);

// ./test/core/if.wast:492
assert_return(() => invoke($0, `break-value`, [1]), [value("i32", 18)]);

// ./test/core/if.wast:493
assert_return(() => invoke($0, `break-value`, [0]), [value("i32", 21)]);

// ./test/core/if.wast:495
assert_return(() => invoke($0, `effects`, [1]), [value("i32", -14)]);

// ./test/core/if.wast:496
assert_return(() => invoke($0, `effects`, [0]), [value("i32", -6)]);

// ./test/core/if.wast:498
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-empty-i32 (result i32) (if (i32.const 0) (then))))`,
    ),
  `type mismatch`,
);

// ./test/core/if.wast:502
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-empty-i64 (result i64) (if (i32.const 0) (then))))`,
    ),
  `type mismatch`,
);

// ./test/core/if.wast:506
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-empty-f32 (result f32) (if (i32.const 0) (then))))`,
    ),
  `type mismatch`,
);

// ./test/core/if.wast:510
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-empty-f64 (result f64) (if (i32.const 0) (then))))`,
    ),
  `type mismatch`,
);

// ./test/core/if.wast:515
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-empty-i32 (result i32) (if (i32.const 0) (then) (else))))`,
    ),
  `type mismatch`,
);

// ./test/core/if.wast:519
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-empty-i64 (result i64) (if (i32.const 0) (then) (else))))`,
    ),
  `type mismatch`,
);

// ./test/core/if.wast:523
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-empty-f32 (result f32) (if (i32.const 0) (then) (else))))`,
    ),
  `type mismatch`,
);

// ./test/core/if.wast:527
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-empty-f64 (result f64) (if (i32.const 0) (then) (else))))`,
    ),
  `type mismatch`,
);

// ./test/core/if.wast:532
assert_invalid(() =>
  instantiate(`(module (func $$type-then-value-num-vs-void
    (if (i32.const 1) (then (i32.const 1)))
  ))`), `type mismatch`);

// ./test/core/if.wast:538
assert_invalid(() =>
  instantiate(`(module (func $$type-then-value-num-vs-void
    (if (i32.const 1) (then (i32.const 1)) (else))
  ))`), `type mismatch`);

// ./test/core/if.wast:544
assert_invalid(() =>
  instantiate(`(module (func $$type-else-value-num-vs-void
    (if (i32.const 1) (then) (else (i32.const 1)))
  ))`), `type mismatch`);

// ./test/core/if.wast:550
assert_invalid(() =>
  instantiate(`(module (func $$type-both-value-num-vs-void
    (if (i32.const 1) (then (i32.const 1)) (else (i32.const 1)))
  ))`), `type mismatch`);

// ./test/core/if.wast:557
assert_invalid(
  () =>
    instantiate(`(module (func $$type-then-value-empty-vs-num (result i32)
    (if (result i32) (i32.const 1) (then) (else (i32.const 0)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:563
assert_invalid(
  () =>
    instantiate(`(module (func $$type-then-value-empty-vs-num (result i32)
    (if (result i32) (i32.const 1) (then (i32.const 0)) (else))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:569
assert_invalid(
  () =>
    instantiate(`(module (func $$type-both-value-empty-vs-num (result i32)
    (if (result i32) (i32.const 1) (then) (else))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:575
assert_invalid(
  () =>
    instantiate(`(module (func $$type-no-else-vs-num (result i32)
    (if (result i32) (i32.const 1) (then (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:582
assert_invalid(
  () =>
    instantiate(`(module (func $$type-then-value-void-vs-num (result i32)
    (if (result i32) (i32.const 1) (then (nop)) (else (i32.const 0)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:588
assert_invalid(
  () =>
    instantiate(`(module (func $$type-then-value-void-vs-num (result i32)
    (if (result i32) (i32.const 1) (then (i32.const 0)) (else (nop)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:594
assert_invalid(
  () =>
    instantiate(`(module (func $$type-both-value-void-vs-num (result i32)
    (if (result i32) (i32.const 1) (then (nop)) (else (nop)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:601
assert_invalid(
  () =>
    instantiate(`(module (func $$type-then-value-num-vs-num (result i32)
    (if (result i32) (i32.const 1) (then (i64.const 1)) (else (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:607
assert_invalid(
  () =>
    instantiate(`(module (func $$type-then-value-num-vs-num (result i32)
    (if (result i32) (i32.const 1) (then (i32.const 1)) (else (i64.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:613
assert_invalid(
  () =>
    instantiate(`(module (func $$type-both-value-num-vs-num (result i32)
    (if (result i32) (i32.const 1) (then (i64.const 1)) (else (i64.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:619
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-both-different-value-num-vs-num (result i32)
    (if (result i32) (i32.const 1) (then (i64.const 1)) (else (f64.const 1)))
  ))`,
    ),
  `type mismatch`,
);

// ./test/core/if.wast:626
assert_invalid(
  () =>
    instantiate(`(module (func $$type-then-value-unreached-select (result i32)
    (if (result i64)
      (i32.const 0)
      (then (select (unreachable) (unreachable) (unreachable)))
      (else (i64.const 0))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:636
assert_invalid(
  () =>
    instantiate(`(module (func $$type-else-value-unreached-select (result i32)
    (if (result i64)
      (i32.const 1)
      (then (i64.const 0))
      (else (select (unreachable) (unreachable) (unreachable)))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:646
assert_invalid(
  () =>
    instantiate(`(module (func $$type-else-value-unreached-select (result i32)
    (if (result i64)
      (i32.const 1)
      (then (select (unreachable) (unreachable) (unreachable)))
      (else (select (unreachable) (unreachable) (unreachable)))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:657
assert_invalid(
  () =>
    instantiate(`(module (func $$type-then-break-last-void-vs-num (result i32)
    (if (result i32) (i32.const 1) (then (br 0)) (else (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:663
assert_invalid(
  () =>
    instantiate(`(module (func $$type-else-break-last-void-vs-num (result i32)
    (if (result i32) (i32.const 1) (then (i32.const 1)) (else (br 0)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:669
assert_invalid(
  () =>
    instantiate(`(module (func $$type-then-break-empty-vs-num (result i32)
    (if (result i32) (i32.const 1)
      (then (br 0) (i32.const 1))
      (else (i32.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:678
assert_invalid(
  () =>
    instantiate(`(module (func $$type-else-break-empty-vs-num (result i32)
    (if (result i32) (i32.const 1)
      (then (i32.const 1))
      (else (br 0) (i32.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:687
assert_invalid(
  () =>
    instantiate(`(module (func $$type-then-break-void-vs-num (result i32)
    (if (result i32) (i32.const 1)
      (then (br 0 (nop)) (i32.const 1))
      (else (i32.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:696
assert_invalid(
  () =>
    instantiate(`(module (func $$type-else-break-void-vs-num (result i32)
    (if (result i32) (i32.const 1)
      (then (i32.const 1))
      (else (br 0 (nop)) (i32.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:706
assert_invalid(
  () =>
    instantiate(`(module (func $$type-then-break-num-vs-num (result i32)
    (if (result i32) (i32.const 1)
      (then (br 0 (i64.const 1)) (i32.const 1))
      (else (i32.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:715
assert_invalid(
  () =>
    instantiate(`(module (func $$type-else-break-num-vs-num (result i32)
    (if (result i32) (i32.const 1)
      (then (i32.const 1))
      (else (br 0 (i64.const 1)) (i32.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:725
assert_invalid(() =>
  instantiate(`(module
    (func $$type-condition-empty
      (if (then))
    )
  )`), `type mismatch`);

// ./test/core/if.wast:733
assert_invalid(() =>
  instantiate(`(module
    (func $$type-condition-empty-in-block
      (i32.const 0)
      (block (if (then)))
    )
  )`), `type mismatch`);

// ./test/core/if.wast:742
assert_invalid(() =>
  instantiate(`(module
    (func $$type-condition-empty-in-loop
      (i32.const 0)
      (loop (if (then)))
    )
  )`), `type mismatch`);

// ./test/core/if.wast:751
assert_invalid(() =>
  instantiate(`(module
    (func $$type-condition-empty-in-then
      (i32.const 0) (i32.const 0)
      (if (then (if (then))))
    )
  )`), `type mismatch`);

// ./test/core/if.wast:760
assert_invalid(() =>
  instantiate(`(module
    (func $$type-condition-empty-in-else
      (i32.const 0) (i32.const 0)
      (if (result i32) (then (i32.const 0)) (else (if (then)) (i32.const 0)))
      (drop)
    )
  )`), `type mismatch`);

// ./test/core/if.wast:770
assert_invalid(() =>
  instantiate(`(module
    (func $$type-condition-empty-in-br
      (i32.const 0)
      (block (br 0 (if(then))) (drop))
    )
  )`), `type mismatch`);

// ./test/core/if.wast:779
assert_invalid(() =>
  instantiate(`(module
    (func $$type-condition-empty-in-br_if
      (i32.const 0)
      (block (br_if 0 (if(then)) (i32.const 1)) (drop))
    )
  )`), `type mismatch`);

// ./test/core/if.wast:788
assert_invalid(() =>
  instantiate(`(module
    (func $$type-condition-empty-in-br_table
      (i32.const 0)
      (block (br_table 0 (if(then))) (drop))
    )
  )`), `type mismatch`);

// ./test/core/if.wast:797
assert_invalid(() =>
  instantiate(`(module
    (func $$type-condition-empty-in-return
      (return (if(then))) (drop)
    )
  )`), `type mismatch`);

// ./test/core/if.wast:805
assert_invalid(() =>
  instantiate(`(module
    (func $$type-condition-empty-in-select
      (select (if(then)) (i32.const 1) (i32.const 2)) (drop)
    )
  )`), `type mismatch`);

// ./test/core/if.wast:813
assert_invalid(() =>
  instantiate(`(module
    (func $$type-condition-empty-in-call
      (call 1 (if(then))) (drop)
    )
    (func (param i32) (result i32) (local.get 0))
  )`), `type mismatch`);

// ./test/core/if.wast:822
assert_invalid(() =>
  instantiate(`(module
    (func $$f (param i32) (result i32) (local.get 0))
    (type $$sig (func (param i32) (result i32)))
    (table funcref (elem $$f))
    (func $$type-condition-empty-in-call_indirect
      (block (result i32)
        (call_indirect (type $$sig)
          (if(then)) (i32.const 0)
        )
        (drop)
      )
    )
  )`), `type mismatch`);

// ./test/core/if.wast:838
assert_invalid(() =>
  instantiate(`(module
    (func $$type-condition-empty-in-local.set
      (local i32)
      (local.set 0 (if(then))) (local.get 0) (drop)
    )
  )`), `type mismatch`);

// ./test/core/if.wast:847
assert_invalid(() =>
  instantiate(`(module
    (func $$type-condition-empty-in-local.tee
      (local i32)
      (local.tee 0 (if(then))) (drop)
    )
  )`), `type mismatch`);

// ./test/core/if.wast:856
assert_invalid(() =>
  instantiate(`(module
    (global $$x (mut i32) (i32.const 0))
    (func $$type-condition-empty-in-global.set
      (global.set $$x (if(then))) (global.get $$x) (drop)
    )
  )`), `type mismatch`);

// ./test/core/if.wast:865
assert_invalid(() =>
  instantiate(`(module
    (memory 0)
    (func $$type-condition-empty-in-memory.grow
      (memory.grow (if(then))) (drop)
    )
  )`), `type mismatch`);

// ./test/core/if.wast:874
assert_invalid(() =>
  instantiate(`(module
    (memory 0)
    (func $$type-condition-empty-in-load
      (i32.load (if(then))) (drop)
    )
  )`), `type mismatch`);

// ./test/core/if.wast:883
assert_invalid(() =>
  instantiate(`(module
    (memory 1)
    (func $$type-condition-empty-in-store
      (i32.store (if(then)) (i32.const 1))
    )
  )`), `type mismatch`);

// ./test/core/if.wast:894
assert_malformed(
  () => instantiate(`(func i32.const 0 if end $$l) `),
  `mismatching label`,
);

// ./test/core/if.wast:898
assert_malformed(
  () => instantiate(`(func i32.const 0 if $$a end $$l) `),
  `mismatching label`,
);

// ./test/core/if.wast:902
assert_malformed(
  () => instantiate(`(func i32.const 0 if else $$l end) `),
  `mismatching label`,
);

// ./test/core/if.wast:906
assert_malformed(
  () => instantiate(`(func i32.const 0 if $$a else $$l end) `),
  `mismatching label`,
);

// ./test/core/if.wast:910
assert_malformed(
  () => instantiate(`(func i32.const 0 if else end $$l) `),
  `mismatching label`,
);

// ./test/core/if.wast:914
assert_malformed(
  () => instantiate(`(func i32.const 0 if else $$l end $$l) `),
  `mismatching label`,
);

// ./test/core/if.wast:918
assert_malformed(
  () => instantiate(`(func i32.const 0 if else $$l1 end $$l2) `),
  `mismatching label`,
);

// ./test/core/if.wast:922
assert_malformed(
  () => instantiate(`(func i32.const 0 if $$a else end $$l) `),
  `mismatching label`,
);

// ./test/core/if.wast:926
assert_malformed(
  () => instantiate(`(func i32.const 0 if $$a else $$a end $$l) `),
  `mismatching label`,
);

// ./test/core/if.wast:930
assert_malformed(
  () => instantiate(`(func i32.const 0 if $$a else $$l end $$l) `),
  `mismatching label`,
);
