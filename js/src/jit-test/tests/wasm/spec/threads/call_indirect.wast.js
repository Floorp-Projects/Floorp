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

// ./test/core/call_indirect.wast

// ./test/core/call_indirect.wast:3
let $0 = instantiate(`(module
  ;; Auxiliary definitions
  (type $$proc (func))
  (type $$out-i32 (func (result i32)))
  (type $$out-i64 (func (result i64)))
  (type $$out-f32 (func (result f32)))
  (type $$out-f64 (func (result f64)))
  (type $$over-i32 (func (param i32) (result i32)))
  (type $$over-i64 (func (param i64) (result i64)))
  (type $$over-f32 (func (param f32) (result f32)))
  (type $$over-f64 (func (param f64) (result f64)))
  (type $$f32-i32 (func (param f32 i32) (result i32)))
  (type $$i32-i64 (func (param i32 i64) (result i64)))
  (type $$f64-f32 (func (param f64 f32) (result f32)))
  (type $$i64-f64 (func (param i64 f64) (result f64)))
  (type $$over-i32-duplicate (func (param i32) (result i32)))
  (type $$over-i64-duplicate (func (param i64) (result i64)))
  (type $$over-f32-duplicate (func (param f32) (result f32)))
  (type $$over-f64-duplicate (func (param f64) (result f64)))

  (func $$const-i32 (type $$out-i32) (i32.const 0x132))
  (func $$const-i64 (type $$out-i64) (i64.const 0x164))
  (func $$const-f32 (type $$out-f32) (f32.const 0xf32))
  (func $$const-f64 (type $$out-f64) (f64.const 0xf64))

  (func $$id-i32 (type $$over-i32) (local.get 0))
  (func $$id-i64 (type $$over-i64) (local.get 0))
  (func $$id-f32 (type $$over-f32) (local.get 0))
  (func $$id-f64 (type $$over-f64) (local.get 0))

  (func $$i32-i64 (type $$i32-i64) (local.get 1))
  (func $$i64-f64 (type $$i64-f64) (local.get 1))
  (func $$f32-i32 (type $$f32-i32) (local.get 1))
  (func $$f64-f32 (type $$f64-f32) (local.get 1))

  (func $$over-i32-duplicate (type $$over-i32-duplicate) (local.get 0))
  (func $$over-i64-duplicate (type $$over-i64-duplicate) (local.get 0))
  (func $$over-f32-duplicate (type $$over-f32-duplicate) (local.get 0))
  (func $$over-f64-duplicate (type $$over-f64-duplicate) (local.get 0))

  (table funcref
    (elem
      $$const-i32 $$const-i64 $$const-f32 $$const-f64
      $$id-i32 $$id-i64 $$id-f32 $$id-f64
      $$f32-i32 $$i32-i64 $$f64-f32 $$i64-f64
      $$fac-i64 $$fib-i64 $$even $$odd
      $$runaway $$mutual-runaway1 $$mutual-runaway2
      $$over-i32-duplicate $$over-i64-duplicate
      $$over-f32-duplicate $$over-f64-duplicate
      $$fac-i32 $$fac-f32 $$fac-f64
      $$fib-i32 $$fib-f32 $$fib-f64
    )
  )

  ;; Syntax

  (func
    (call_indirect (i32.const 0))
    (call_indirect (param i64) (i64.const 0) (i32.const 0))
    (call_indirect (param i64) (param) (param f64 i32 i64)
      (i64.const 0) (f64.const 0) (i32.const 0) (i64.const 0) (i32.const 0)
    )
    (call_indirect (result) (i32.const 0))
    (drop (i32.eqz (call_indirect (result i32) (i32.const 0))))
    (drop (i32.eqz (call_indirect (result i32) (result) (i32.const 0))))
    (drop (i32.eqz
      (call_indirect (param i64) (result i32) (i64.const 0) (i32.const 0))
    ))
    (drop (i32.eqz
      (call_indirect
        (param) (param i64) (param) (param f64 i32 i64) (param) (param)
        (result) (result i32) (result) (result)
        (i64.const 0) (f64.const 0) (i32.const 0) (i64.const 0) (i32.const 0)
      )
    ))
    (drop (i64.eqz
      (call_indirect (type $$over-i64) (param i64) (result i64)
        (i64.const 0) (i32.const 0)
      )
    ))
  )

  ;; Typing

  (func (export "type-i32") (result i32)
    (call_indirect (type $$out-i32) (i32.const 0))
  )
  (func (export "type-i64") (result i64)
    (call_indirect (type $$out-i64) (i32.const 1))
  )
  (func (export "type-f32") (result f32)
    (call_indirect (type $$out-f32) (i32.const 2))
  )
  (func (export "type-f64") (result f64)
    (call_indirect (type $$out-f64) (i32.const 3))
  )

  (func (export "type-index") (result i64)
    (call_indirect (type $$over-i64) (i64.const 100) (i32.const 5))
  )

  (func (export "type-first-i32") (result i32)
    (call_indirect (type $$over-i32) (i32.const 32) (i32.const 4))
  )
  (func (export "type-first-i64") (result i64)
    (call_indirect (type $$over-i64) (i64.const 64) (i32.const 5))
  )
  (func (export "type-first-f32") (result f32)
    (call_indirect (type $$over-f32) (f32.const 1.32) (i32.const 6))
  )
  (func (export "type-first-f64") (result f64)
    (call_indirect (type $$over-f64) (f64.const 1.64) (i32.const 7))
  )

  (func (export "type-second-i32") (result i32)
    (call_indirect (type $$f32-i32) (f32.const 32.1) (i32.const 32) (i32.const 8))
  )
  (func (export "type-second-i64") (result i64)
    (call_indirect (type $$i32-i64) (i32.const 32) (i64.const 64) (i32.const 9))
  )
  (func (export "type-second-f32") (result f32)
    (call_indirect (type $$f64-f32) (f64.const 64) (f32.const 32) (i32.const 10))
  )
  (func (export "type-second-f64") (result f64)
    (call_indirect (type $$i64-f64) (i64.const 64) (f64.const 64.1) (i32.const 11))
  )

  ;; Dispatch

  (func (export "dispatch") (param i32 i64) (result i64)
    (call_indirect (type $$over-i64) (local.get 1) (local.get 0))
  )

  (func (export "dispatch-structural-i64") (param i32) (result i64)
    (call_indirect (type $$over-i64-duplicate) (i64.const 9) (local.get 0))
  )
  (func (export "dispatch-structural-i32") (param i32) (result i32)
    (call_indirect (type $$over-i32-duplicate) (i32.const 9) (local.get 0))
  )
  (func (export "dispatch-structural-f32") (param i32) (result f32)
    (call_indirect (type $$over-f32-duplicate) (f32.const 9.0) (local.get 0))
  )
  (func (export "dispatch-structural-f64") (param i32) (result f64)
    (call_indirect (type $$over-f64-duplicate) (f64.const 9.0) (local.get 0))
  )

  ;; Recursion

  (func $$fac-i64 (export "fac-i64") (type $$over-i64)
    (if (result i64) (i64.eqz (local.get 0))
      (then (i64.const 1))
      (else
        (i64.mul
          (local.get 0)
          (call_indirect (type $$over-i64)
            (i64.sub (local.get 0) (i64.const 1))
            (i32.const 12)
          )
        )
      )
    )
  )

  (func $$fib-i64 (export "fib-i64") (type $$over-i64)
    (if (result i64) (i64.le_u (local.get 0) (i64.const 1))
      (then (i64.const 1))
      (else
        (i64.add
          (call_indirect (type $$over-i64)
            (i64.sub (local.get 0) (i64.const 2))
            (i32.const 13)
          )
          (call_indirect (type $$over-i64)
            (i64.sub (local.get 0) (i64.const 1))
            (i32.const 13)
          )
        )
      )
    )
  )

  (func $$fac-i32 (export "fac-i32") (type $$over-i32)
    (if (result i32) (i32.eqz (local.get 0))
      (then (i32.const 1))
      (else
        (i32.mul
          (local.get 0)
          (call_indirect (type $$over-i32)
            (i32.sub (local.get 0) (i32.const 1))
            (i32.const 23)
          )
        )
      )
    )
  )

  (func $$fac-f32 (export "fac-f32") (type $$over-f32)
    (if (result f32) (f32.eq (local.get 0) (f32.const 0.0))
      (then (f32.const 1.0))
      (else
        (f32.mul
          (local.get 0)
          (call_indirect (type $$over-f32)
            (f32.sub (local.get 0) (f32.const 1.0))
            (i32.const 24)
          )
        )
      )
    )
  )

  (func $$fac-f64 (export "fac-f64") (type $$over-f64)
    (if (result f64) (f64.eq (local.get 0) (f64.const 0.0))
      (then (f64.const 1.0))
      (else
        (f64.mul
          (local.get 0)
          (call_indirect (type $$over-f64)
            (f64.sub (local.get 0) (f64.const 1.0))
            (i32.const 25)
          )
        )
      )
    )
  )

  (func $$fib-i32 (export "fib-i32") (type $$over-i32)
    (if (result i32) (i32.le_u (local.get 0) (i32.const 1))
      (then (i32.const 1))
      (else
        (i32.add
          (call_indirect (type $$over-i32)
            (i32.sub (local.get 0) (i32.const 2))
            (i32.const 26)
          )
          (call_indirect (type $$over-i32)
            (i32.sub (local.get 0) (i32.const 1))
            (i32.const 26)
          )
        )
      )
    )
  )

  (func $$fib-f32 (export "fib-f32") (type $$over-f32)
    (if (result f32) (f32.le (local.get 0) (f32.const 1.0))
      (then (f32.const 1.0))
      (else
        (f32.add
          (call_indirect (type $$over-f32)
            (f32.sub (local.get 0) (f32.const 2.0))
            (i32.const 27)
          )
          (call_indirect (type $$over-f32)
            (f32.sub (local.get 0) (f32.const 1.0))
            (i32.const 27)
          )
        )
      )
    )
  )

  (func $$fib-f64 (export "fib-f64") (type $$over-f64)
    (if (result f64) (f64.le (local.get 0) (f64.const 1.0))
      (then (f64.const 1.0))
      (else
        (f64.add
          (call_indirect (type $$over-f64)
            (f64.sub (local.get 0) (f64.const 2.0))
            (i32.const 28)
          )
          (call_indirect (type $$over-f64)
            (f64.sub (local.get 0) (f64.const 1.0))
            (i32.const 28)
          )
        )
      )
    )
  )

  (func $$even (export "even") (param i32) (result i32)
    (if (result i32) (i32.eqz (local.get 0))
      (then (i32.const 44))
      (else
        (call_indirect (type $$over-i32)
          (i32.sub (local.get 0) (i32.const 1))
          (i32.const 15)
        )
      )
    )
  )
  (func $$odd (export "odd") (param i32) (result i32)
    (if (result i32) (i32.eqz (local.get 0))
      (then (i32.const 99))
      (else
        (call_indirect (type $$over-i32)
          (i32.sub (local.get 0) (i32.const 1))
          (i32.const 14)
        )
      )
    )
  )

  ;; Stack exhaustion

  ;; Implementations are required to have every call consume some abstract
  ;; resource towards exhausting some abstract finite limit, such that
  ;; infinitely recursive test cases reliably trap in finite time. This is
  ;; because otherwise applications could come to depend on it on those
  ;; implementations and be incompatible with implementations that don't do
  ;; it (or don't do it under the same circumstances).

  (func $$runaway (export "runaway") (call_indirect (type $$proc) (i32.const 16)))

  (func $$mutual-runaway1 (export "mutual-runaway") (call_indirect (type $$proc) (i32.const 18)))
  (func $$mutual-runaway2 (call_indirect (type $$proc) (i32.const 17)))

  ;; As parameter of control constructs and instructions

  (memory 1)

  (func (export "as-select-first") (result i32)
    (select (call_indirect (type $$out-i32) (i32.const 0)) (i32.const 2) (i32.const 3))
  )
  (func (export "as-select-mid") (result i32)
    (select (i32.const 2) (call_indirect (type $$out-i32) (i32.const 0)) (i32.const 3))
  )
  (func (export "as-select-last") (result i32)
    (select (i32.const 2) (i32.const 3) (call_indirect (type $$out-i32) (i32.const 0)))
  )

  (func (export "as-if-condition") (result i32)
    (if (result i32) (call_indirect (type $$out-i32) (i32.const 0)) (then (i32.const 1)) (else (i32.const 2)))
  )

  (func (export "as-br_if-first") (result i64)
    (block (result i64) (br_if 0 (call_indirect (type $$out-i64) (i32.const 1)) (i32.const 2)))
  )
  (func (export "as-br_if-last") (result i32)
    (block (result i32) (br_if 0 (i32.const 2) (call_indirect (type $$out-i32) (i32.const 0))))
  )

  (func (export "as-br_table-first") (result f32)
    (block (result f32) (call_indirect (type $$out-f32) (i32.const 2)) (i32.const 2) (br_table 0 0))
  )
  (func (export "as-br_table-last") (result i32)
    (block (result i32) (i32.const 2) (call_indirect (type $$out-i32) (i32.const 0)) (br_table 0 0))
  )

  (func (export "as-store-first")
    (call_indirect (type $$out-i32) (i32.const 0)) (i32.const 1) (i32.store)
  )
  (func (export "as-store-last")
    (i32.const 10) (call_indirect (type $$out-f64) (i32.const 3)) (f64.store)
  )

  (func (export "as-memory.grow-value") (result i32)
    (memory.grow (call_indirect (type $$out-i32) (i32.const 0)))
  )
  (func (export "as-return-value") (result i32)
    (call_indirect (type $$over-i32) (i32.const 1) (i32.const 4)) (return)
  )
  (func (export "as-drop-operand")
    (call_indirect (type $$over-i64) (i64.const 1) (i32.const 5)) (drop)
  )
  (func (export "as-br-value") (result f32)
    (block (result f32) (br 0 (call_indirect (type $$over-f32) (f32.const 1) (i32.const 6))))
  )
  (func (export "as-local.set-value") (result f64)
    (local f64) (local.set 0 (call_indirect (type $$over-f64) (f64.const 1) (i32.const 7))) (local.get 0)
  )
  (func (export "as-local.tee-value") (result f64)
    (local f64) (local.tee 0 (call_indirect (type $$over-f64) (f64.const 1) (i32.const 7)))
  )
  (global $$a (mut f64) (f64.const 10.0))
  (func (export "as-global.set-value") (result f64)
    (global.set $$a (call_indirect (type $$over-f64) (f64.const 1.0) (i32.const 7)))
    (global.get $$a)
  )

  (func (export "as-load-operand") (result i32)
    (i32.load (call_indirect (type $$out-i32) (i32.const 0)))
  )

  (func (export "as-unary-operand") (result f32)
    (block (result f32)
      (f32.sqrt
        (call_indirect (type $$over-f32) (f32.const 0x0p+0) (i32.const 6))
      )
    )
  )

  (func (export "as-binary-left") (result i32)
    (block (result i32)
      (i32.add
        (call_indirect (type $$over-i32) (i32.const 1) (i32.const 4))
        (i32.const 10)
      )
    )
  )
  (func (export "as-binary-right") (result i32)
    (block (result i32)
      (i32.sub
        (i32.const 10)
        (call_indirect (type $$over-i32) (i32.const 1) (i32.const 4))
      )
    )
  )

  (func (export "as-test-operand") (result i32)
    (block (result i32)
      (i32.eqz
        (call_indirect (type $$over-i32) (i32.const 1) (i32.const 4))
      )
    )
  )

  (func (export "as-compare-left") (result i32)
    (block (result i32)
      (i32.le_u
        (call_indirect (type $$over-i32) (i32.const 1) (i32.const 4))
        (i32.const 10)
      )
    )
  )
  (func (export "as-compare-right") (result i32)
    (block (result i32)
      (i32.ne
        (i32.const 10)
        (call_indirect (type $$over-i32) (i32.const 1) (i32.const 4))
      )
    )
  )

  (func (export "as-convert-operand") (result i64)
    (block (result i64)
      (i64.extend_i32_s
        (call_indirect (type $$over-i32) (i32.const 1) (i32.const 4))
      )
    )
  )

)`);

// ./test/core/call_indirect.wast:447
assert_return(() => invoke($0, `type-i32`, []), [value("i32", 306)]);

// ./test/core/call_indirect.wast:448
assert_return(() => invoke($0, `type-i64`, []), [value("i64", 356n)]);

// ./test/core/call_indirect.wast:449
assert_return(() => invoke($0, `type-f32`, []), [value("f32", 3890)]);

// ./test/core/call_indirect.wast:450
assert_return(() => invoke($0, `type-f64`, []), [value("f64", 3940)]);

// ./test/core/call_indirect.wast:452
assert_return(() => invoke($0, `type-index`, []), [value("i64", 100n)]);

// ./test/core/call_indirect.wast:454
assert_return(() => invoke($0, `type-first-i32`, []), [value("i32", 32)]);

// ./test/core/call_indirect.wast:455
assert_return(() => invoke($0, `type-first-i64`, []), [value("i64", 64n)]);

// ./test/core/call_indirect.wast:456
assert_return(() => invoke($0, `type-first-f32`, []), [value("f32", 1.32)]);

// ./test/core/call_indirect.wast:457
assert_return(() => invoke($0, `type-first-f64`, []), [value("f64", 1.64)]);

// ./test/core/call_indirect.wast:459
assert_return(() => invoke($0, `type-second-i32`, []), [value("i32", 32)]);

// ./test/core/call_indirect.wast:460
assert_return(() => invoke($0, `type-second-i64`, []), [value("i64", 64n)]);

// ./test/core/call_indirect.wast:461
assert_return(() => invoke($0, `type-second-f32`, []), [value("f32", 32)]);

// ./test/core/call_indirect.wast:462
assert_return(() => invoke($0, `type-second-f64`, []), [value("f64", 64.1)]);

// ./test/core/call_indirect.wast:464
assert_return(() => invoke($0, `dispatch`, [5, 2n]), [value("i64", 2n)]);

// ./test/core/call_indirect.wast:465
assert_return(() => invoke($0, `dispatch`, [5, 5n]), [value("i64", 5n)]);

// ./test/core/call_indirect.wast:466
assert_return(() => invoke($0, `dispatch`, [12, 5n]), [value("i64", 120n)]);

// ./test/core/call_indirect.wast:467
assert_return(() => invoke($0, `dispatch`, [13, 5n]), [value("i64", 8n)]);

// ./test/core/call_indirect.wast:468
assert_return(() => invoke($0, `dispatch`, [20, 2n]), [value("i64", 2n)]);

// ./test/core/call_indirect.wast:469
assert_trap(
  () => invoke($0, `dispatch`, [0, 2n]),
  `indirect call type mismatch`,
);

// ./test/core/call_indirect.wast:470
assert_trap(
  () => invoke($0, `dispatch`, [15, 2n]),
  `indirect call type mismatch`,
);

// ./test/core/call_indirect.wast:471
assert_trap(() => invoke($0, `dispatch`, [29, 2n]), `undefined element`);

// ./test/core/call_indirect.wast:472
assert_trap(() => invoke($0, `dispatch`, [-1, 2n]), `undefined element`);

// ./test/core/call_indirect.wast:473
assert_trap(
  () => invoke($0, `dispatch`, [1213432423, 2n]),
  `undefined element`,
);

// ./test/core/call_indirect.wast:475
assert_return(() => invoke($0, `dispatch-structural-i64`, [5]), [
  value("i64", 9n),
]);

// ./test/core/call_indirect.wast:476
assert_return(() => invoke($0, `dispatch-structural-i64`, [12]), [
  value("i64", 362880n),
]);

// ./test/core/call_indirect.wast:477
assert_return(() => invoke($0, `dispatch-structural-i64`, [13]), [
  value("i64", 55n),
]);

// ./test/core/call_indirect.wast:478
assert_return(() => invoke($0, `dispatch-structural-i64`, [20]), [
  value("i64", 9n),
]);

// ./test/core/call_indirect.wast:479
assert_trap(
  () => invoke($0, `dispatch-structural-i64`, [11]),
  `indirect call type mismatch`,
);

// ./test/core/call_indirect.wast:480
assert_trap(
  () => invoke($0, `dispatch-structural-i64`, [22]),
  `indirect call type mismatch`,
);

// ./test/core/call_indirect.wast:482
assert_return(() => invoke($0, `dispatch-structural-i32`, [4]), [
  value("i32", 9),
]);

// ./test/core/call_indirect.wast:483
assert_return(() => invoke($0, `dispatch-structural-i32`, [23]), [
  value("i32", 362880),
]);

// ./test/core/call_indirect.wast:484
assert_return(() => invoke($0, `dispatch-structural-i32`, [26]), [
  value("i32", 55),
]);

// ./test/core/call_indirect.wast:485
assert_return(() => invoke($0, `dispatch-structural-i32`, [19]), [
  value("i32", 9),
]);

// ./test/core/call_indirect.wast:486
assert_trap(
  () => invoke($0, `dispatch-structural-i32`, [9]),
  `indirect call type mismatch`,
);

// ./test/core/call_indirect.wast:487
assert_trap(
  () => invoke($0, `dispatch-structural-i32`, [21]),
  `indirect call type mismatch`,
);

// ./test/core/call_indirect.wast:489
assert_return(() => invoke($0, `dispatch-structural-f32`, [6]), [
  value("f32", 9),
]);

// ./test/core/call_indirect.wast:490
assert_return(() => invoke($0, `dispatch-structural-f32`, [24]), [
  value("f32", 362880),
]);

// ./test/core/call_indirect.wast:491
assert_return(() => invoke($0, `dispatch-structural-f32`, [27]), [
  value("f32", 55),
]);

// ./test/core/call_indirect.wast:492
assert_return(() => invoke($0, `dispatch-structural-f32`, [21]), [
  value("f32", 9),
]);

// ./test/core/call_indirect.wast:493
assert_trap(
  () => invoke($0, `dispatch-structural-f32`, [8]),
  `indirect call type mismatch`,
);

// ./test/core/call_indirect.wast:494
assert_trap(
  () => invoke($0, `dispatch-structural-f32`, [19]),
  `indirect call type mismatch`,
);

// ./test/core/call_indirect.wast:496
assert_return(() => invoke($0, `dispatch-structural-f64`, [7]), [
  value("f64", 9),
]);

// ./test/core/call_indirect.wast:497
assert_return(() => invoke($0, `dispatch-structural-f64`, [25]), [
  value("f64", 362880),
]);

// ./test/core/call_indirect.wast:498
assert_return(() => invoke($0, `dispatch-structural-f64`, [28]), [
  value("f64", 55),
]);

// ./test/core/call_indirect.wast:499
assert_return(() => invoke($0, `dispatch-structural-f64`, [22]), [
  value("f64", 9),
]);

// ./test/core/call_indirect.wast:500
assert_trap(
  () => invoke($0, `dispatch-structural-f64`, [10]),
  `indirect call type mismatch`,
);

// ./test/core/call_indirect.wast:501
assert_trap(
  () => invoke($0, `dispatch-structural-f64`, [18]),
  `indirect call type mismatch`,
);

// ./test/core/call_indirect.wast:503
assert_return(() => invoke($0, `fac-i64`, [0n]), [value("i64", 1n)]);

// ./test/core/call_indirect.wast:504
assert_return(() => invoke($0, `fac-i64`, [1n]), [value("i64", 1n)]);

// ./test/core/call_indirect.wast:505
assert_return(() => invoke($0, `fac-i64`, [5n]), [value("i64", 120n)]);

// ./test/core/call_indirect.wast:506
assert_return(() => invoke($0, `fac-i64`, [25n]), [
  value("i64", 7034535277573963776n),
]);

// ./test/core/call_indirect.wast:508
assert_return(() => invoke($0, `fac-i32`, [0]), [value("i32", 1)]);

// ./test/core/call_indirect.wast:509
assert_return(() => invoke($0, `fac-i32`, [1]), [value("i32", 1)]);

// ./test/core/call_indirect.wast:510
assert_return(() => invoke($0, `fac-i32`, [5]), [value("i32", 120)]);

// ./test/core/call_indirect.wast:511
assert_return(() => invoke($0, `fac-i32`, [10]), [value("i32", 3628800)]);

// ./test/core/call_indirect.wast:513
assert_return(() => invoke($0, `fac-f32`, [value("f32", 0)]), [
  value("f32", 1),
]);

// ./test/core/call_indirect.wast:514
assert_return(() => invoke($0, `fac-f32`, [value("f32", 1)]), [
  value("f32", 1),
]);

// ./test/core/call_indirect.wast:515
assert_return(() => invoke($0, `fac-f32`, [value("f32", 5)]), [
  value("f32", 120),
]);

// ./test/core/call_indirect.wast:516
assert_return(() => invoke($0, `fac-f32`, [value("f32", 10)]), [
  value("f32", 3628800),
]);

// ./test/core/call_indirect.wast:518
assert_return(() => invoke($0, `fac-f64`, [value("f64", 0)]), [
  value("f64", 1),
]);

// ./test/core/call_indirect.wast:519
assert_return(() => invoke($0, `fac-f64`, [value("f64", 1)]), [
  value("f64", 1),
]);

// ./test/core/call_indirect.wast:520
assert_return(() => invoke($0, `fac-f64`, [value("f64", 5)]), [
  value("f64", 120),
]);

// ./test/core/call_indirect.wast:521
assert_return(() => invoke($0, `fac-f64`, [value("f64", 10)]), [
  value("f64", 3628800),
]);

// ./test/core/call_indirect.wast:523
assert_return(() => invoke($0, `fib-i64`, [0n]), [value("i64", 1n)]);

// ./test/core/call_indirect.wast:524
assert_return(() => invoke($0, `fib-i64`, [1n]), [value("i64", 1n)]);

// ./test/core/call_indirect.wast:525
assert_return(() => invoke($0, `fib-i64`, [2n]), [value("i64", 2n)]);

// ./test/core/call_indirect.wast:526
assert_return(() => invoke($0, `fib-i64`, [5n]), [value("i64", 8n)]);

// ./test/core/call_indirect.wast:527
assert_return(() => invoke($0, `fib-i64`, [20n]), [value("i64", 10946n)]);

// ./test/core/call_indirect.wast:529
assert_return(() => invoke($0, `fib-i32`, [0]), [value("i32", 1)]);

// ./test/core/call_indirect.wast:530
assert_return(() => invoke($0, `fib-i32`, [1]), [value("i32", 1)]);

// ./test/core/call_indirect.wast:531
assert_return(() => invoke($0, `fib-i32`, [2]), [value("i32", 2)]);

// ./test/core/call_indirect.wast:532
assert_return(() => invoke($0, `fib-i32`, [5]), [value("i32", 8)]);

// ./test/core/call_indirect.wast:533
assert_return(() => invoke($0, `fib-i32`, [20]), [value("i32", 10946)]);

// ./test/core/call_indirect.wast:535
assert_return(() => invoke($0, `fib-f32`, [value("f32", 0)]), [
  value("f32", 1),
]);

// ./test/core/call_indirect.wast:536
assert_return(() => invoke($0, `fib-f32`, [value("f32", 1)]), [
  value("f32", 1),
]);

// ./test/core/call_indirect.wast:537
assert_return(() => invoke($0, `fib-f32`, [value("f32", 2)]), [
  value("f32", 2),
]);

// ./test/core/call_indirect.wast:538
assert_return(() => invoke($0, `fib-f32`, [value("f32", 5)]), [
  value("f32", 8),
]);

// ./test/core/call_indirect.wast:539
assert_return(() => invoke($0, `fib-f32`, [value("f32", 20)]), [
  value("f32", 10946),
]);

// ./test/core/call_indirect.wast:541
assert_return(() => invoke($0, `fib-f64`, [value("f64", 0)]), [
  value("f64", 1),
]);

// ./test/core/call_indirect.wast:542
assert_return(() => invoke($0, `fib-f64`, [value("f64", 1)]), [
  value("f64", 1),
]);

// ./test/core/call_indirect.wast:543
assert_return(() => invoke($0, `fib-f64`, [value("f64", 2)]), [
  value("f64", 2),
]);

// ./test/core/call_indirect.wast:544
assert_return(() => invoke($0, `fib-f64`, [value("f64", 5)]), [
  value("f64", 8),
]);

// ./test/core/call_indirect.wast:545
assert_return(() => invoke($0, `fib-f64`, [value("f64", 20)]), [
  value("f64", 10946),
]);

// ./test/core/call_indirect.wast:547
assert_return(() => invoke($0, `even`, [0]), [value("i32", 44)]);

// ./test/core/call_indirect.wast:548
assert_return(() => invoke($0, `even`, [1]), [value("i32", 99)]);

// ./test/core/call_indirect.wast:549
assert_return(() => invoke($0, `even`, [100]), [value("i32", 44)]);

// ./test/core/call_indirect.wast:550
assert_return(() => invoke($0, `even`, [77]), [value("i32", 99)]);

// ./test/core/call_indirect.wast:551
assert_return(() => invoke($0, `odd`, [0]), [value("i32", 99)]);

// ./test/core/call_indirect.wast:552
assert_return(() => invoke($0, `odd`, [1]), [value("i32", 44)]);

// ./test/core/call_indirect.wast:553
assert_return(() => invoke($0, `odd`, [200]), [value("i32", 99)]);

// ./test/core/call_indirect.wast:554
assert_return(() => invoke($0, `odd`, [77]), [value("i32", 44)]);

// ./test/core/call_indirect.wast:556
assert_exhaustion(() => invoke($0, `runaway`, []), `call stack exhausted`);

// ./test/core/call_indirect.wast:557
assert_exhaustion(
  () => invoke($0, `mutual-runaway`, []),
  `call stack exhausted`,
);

// ./test/core/call_indirect.wast:559
assert_return(() => invoke($0, `as-select-first`, []), [value("i32", 306)]);

// ./test/core/call_indirect.wast:560
assert_return(() => invoke($0, `as-select-mid`, []), [value("i32", 2)]);

// ./test/core/call_indirect.wast:561
assert_return(() => invoke($0, `as-select-last`, []), [value("i32", 2)]);

// ./test/core/call_indirect.wast:563
assert_return(() => invoke($0, `as-if-condition`, []), [value("i32", 1)]);

// ./test/core/call_indirect.wast:565
assert_return(() => invoke($0, `as-br_if-first`, []), [value("i64", 356n)]);

// ./test/core/call_indirect.wast:566
assert_return(() => invoke($0, `as-br_if-last`, []), [value("i32", 2)]);

// ./test/core/call_indirect.wast:568
assert_return(() => invoke($0, `as-br_table-first`, []), [value("f32", 3890)]);

// ./test/core/call_indirect.wast:569
assert_return(() => invoke($0, `as-br_table-last`, []), [value("i32", 2)]);

// ./test/core/call_indirect.wast:571
assert_return(() => invoke($0, `as-store-first`, []), []);

// ./test/core/call_indirect.wast:572
assert_return(() => invoke($0, `as-store-last`, []), []);

// ./test/core/call_indirect.wast:574
assert_return(() => invoke($0, `as-memory.grow-value`, []), [value("i32", 1)]);

// ./test/core/call_indirect.wast:575
assert_return(() => invoke($0, `as-return-value`, []), [value("i32", 1)]);

// ./test/core/call_indirect.wast:576
assert_return(() => invoke($0, `as-drop-operand`, []), []);

// ./test/core/call_indirect.wast:577
assert_return(() => invoke($0, `as-br-value`, []), [value("f32", 1)]);

// ./test/core/call_indirect.wast:578
assert_return(() => invoke($0, `as-local.set-value`, []), [value("f64", 1)]);

// ./test/core/call_indirect.wast:579
assert_return(() => invoke($0, `as-local.tee-value`, []), [value("f64", 1)]);

// ./test/core/call_indirect.wast:580
assert_return(() => invoke($0, `as-global.set-value`, []), [value("f64", 1)]);

// ./test/core/call_indirect.wast:581
assert_return(() => invoke($0, `as-load-operand`, []), [value("i32", 1)]);

// ./test/core/call_indirect.wast:583
assert_return(() => invoke($0, `as-unary-operand`, []), [value("f32", 0)]);

// ./test/core/call_indirect.wast:584
assert_return(() => invoke($0, `as-binary-left`, []), [value("i32", 11)]);

// ./test/core/call_indirect.wast:585
assert_return(() => invoke($0, `as-binary-right`, []), [value("i32", 9)]);

// ./test/core/call_indirect.wast:586
assert_return(() => invoke($0, `as-test-operand`, []), [value("i32", 0)]);

// ./test/core/call_indirect.wast:587
assert_return(() => invoke($0, `as-compare-left`, []), [value("i32", 1)]);

// ./test/core/call_indirect.wast:588
assert_return(() => invoke($0, `as-compare-right`, []), [value("i32", 1)]);

// ./test/core/call_indirect.wast:589
assert_return(() => invoke($0, `as-convert-operand`, []), [value("i64", 1n)]);

// ./test/core/call_indirect.wast:593
assert_malformed(
  () =>
    instantiate(
      `(type $$sig (func (param i32) (result i32))) (table 0 funcref) (func (result i32)   (call_indirect (type $$sig) (result i32) (param i32)     (i32.const 0) (i32.const 0)   ) ) `,
    ),
  `unexpected token`,
);

// ./test/core/call_indirect.wast:605
assert_malformed(
  () =>
    instantiate(
      `(type $$sig (func (param i32) (result i32))) (table 0 funcref) (func (result i32)   (call_indirect (param i32) (type $$sig) (result i32)     (i32.const 0) (i32.const 0)   ) ) `,
    ),
  `unexpected token`,
);

// ./test/core/call_indirect.wast:617
assert_malformed(
  () =>
    instantiate(
      `(type $$sig (func (param i32) (result i32))) (table 0 funcref) (func (result i32)   (call_indirect (param i32) (result i32) (type $$sig)     (i32.const 0) (i32.const 0)   ) ) `,
    ),
  `unexpected token`,
);

// ./test/core/call_indirect.wast:629
assert_malformed(
  () =>
    instantiate(
      `(type $$sig (func (param i32) (result i32))) (table 0 funcref) (func (result i32)   (call_indirect (result i32) (type $$sig) (param i32)     (i32.const 0) (i32.const 0)   ) ) `,
    ),
  `unexpected token`,
);

// ./test/core/call_indirect.wast:641
assert_malformed(
  () =>
    instantiate(
      `(type $$sig (func (param i32) (result i32))) (table 0 funcref) (func (result i32)   (call_indirect (result i32) (param i32) (type $$sig)     (i32.const 0) (i32.const 0)   ) ) `,
    ),
  `unexpected token`,
);

// ./test/core/call_indirect.wast:653
assert_malformed(
  () =>
    instantiate(
      `(table 0 funcref) (func (result i32)   (call_indirect (result i32) (param i32) (i32.const 0) (i32.const 0)) ) `,
    ),
  `unexpected token`,
);

// ./test/core/call_indirect.wast:663
assert_malformed(
  () =>
    instantiate(
      `(table 0 funcref) (func (call_indirect (param $$x i32) (i32.const 0) (i32.const 0))) `,
    ),
  `unexpected token`,
);

// ./test/core/call_indirect.wast:670
assert_malformed(
  () =>
    instantiate(
      `(type $$sig (func)) (table 0 funcref) (func (result i32)   (call_indirect (type $$sig) (result i32) (i32.const 0)) ) `,
    ),
  `inline function type`,
);

// ./test/core/call_indirect.wast:680
assert_malformed(
  () =>
    instantiate(
      `(type $$sig (func (param i32) (result i32))) (table 0 funcref) (func (result i32)   (call_indirect (type $$sig) (result i32) (i32.const 0)) ) `,
    ),
  `inline function type`,
);

// ./test/core/call_indirect.wast:690
assert_malformed(
  () =>
    instantiate(
      `(type $$sig (func (param i32) (result i32))) (table 0 funcref) (func   (call_indirect (type $$sig) (param i32) (i32.const 0) (i32.const 0)) ) `,
    ),
  `inline function type`,
);

// ./test/core/call_indirect.wast:700
assert_malformed(
  () =>
    instantiate(
      `(type $$sig (func (param i32 i32) (result i32))) (table 0 funcref) (func (result i32)   (call_indirect (type $$sig) (param i32) (result i32)     (i32.const 0) (i32.const 0)   ) ) `,
    ),
  `inline function type`,
);

// ./test/core/call_indirect.wast:715
assert_invalid(() =>
  instantiate(`(module
    (type (func))
    (func $$no-table (call_indirect (type 0) (i32.const 0)))
  )`), `unknown table`);

// ./test/core/call_indirect.wast:723
assert_invalid(() =>
  instantiate(`(module
    (type (func))
    (table 0 funcref)
    (func $$type-void-vs-num (i32.eqz (call_indirect (type 0) (i32.const 0))))
  )`), `type mismatch`);

// ./test/core/call_indirect.wast:731
assert_invalid(() =>
  instantiate(`(module
    (type (func (result i64)))
    (table 0 funcref)
    (func $$type-num-vs-num (i32.eqz (call_indirect (type 0) (i32.const 0))))
  )`), `type mismatch`);

// ./test/core/call_indirect.wast:740
assert_invalid(() =>
  instantiate(`(module
    (type (func (param i32)))
    (table 0 funcref)
    (func $$arity-0-vs-1 (call_indirect (type 0) (i32.const 0)))
  )`), `type mismatch`);

// ./test/core/call_indirect.wast:748
assert_invalid(() =>
  instantiate(`(module
    (type (func (param f64 i32)))
    (table 0 funcref)
    (func $$arity-0-vs-2 (call_indirect (type 0) (i32.const 0)))
  )`), `type mismatch`);

// ./test/core/call_indirect.wast:756
assert_invalid(() =>
  instantiate(`(module
    (type (func))
    (table 0 funcref)
    (func $$arity-1-vs-0 (call_indirect (type 0) (i32.const 1) (i32.const 0)))
  )`), `type mismatch`);

// ./test/core/call_indirect.wast:764
assert_invalid(() =>
  instantiate(`(module
    (type (func))
    (table 0 funcref)
    (func $$arity-2-vs-0
      (call_indirect (type 0) (f64.const 2) (i32.const 1) (i32.const 0))
    )
  )`), `type mismatch`);

// ./test/core/call_indirect.wast:775
assert_invalid(() =>
  instantiate(`(module
    (type (func (param i32)))
    (table 0 funcref)
    (func $$type-func-void-vs-i32 (call_indirect (type 0) (i32.const 1) (nop)))
  )`), `type mismatch`);

// ./test/core/call_indirect.wast:783
assert_invalid(() =>
  instantiate(`(module
    (type (func (param i32)))
    (table 0 funcref)
    (func $$type-func-num-vs-i32 (call_indirect (type 0) (i32.const 0) (i64.const 1)))
  )`), `type mismatch`);

// ./test/core/call_indirect.wast:792
assert_invalid(() =>
  instantiate(`(module
    (type (func (param i32 i32)))
    (table 0 funcref)
    (func $$type-first-void-vs-num
      (call_indirect (type 0) (nop) (i32.const 1) (i32.const 0))
    )
  )`), `type mismatch`);

// ./test/core/call_indirect.wast:802
assert_invalid(() =>
  instantiate(`(module
    (type (func (param i32 i32)))
    (table 0 funcref)
    (func $$type-second-void-vs-num
      (call_indirect (type 0) (i32.const 1) (nop) (i32.const 0))
    )
  )`), `type mismatch`);

// ./test/core/call_indirect.wast:812
assert_invalid(() =>
  instantiate(`(module
    (type (func (param i32 f64)))
    (table 0 funcref)
    (func $$type-first-num-vs-num
      (call_indirect (type 0) (f64.const 1) (i32.const 1) (i32.const 0))
    )
  )`), `type mismatch`);

// ./test/core/call_indirect.wast:822
assert_invalid(() =>
  instantiate(`(module
    (type (func (param f64 i32)))
    (table 0 funcref)
    (func $$type-second-num-vs-num
      (call_indirect (type 0) (i32.const 1) (f64.const 1) (i32.const 0))
    )
  )`), `type mismatch`);

// ./test/core/call_indirect.wast:833
assert_invalid(() =>
  instantiate(`(module
    (func $$f (param i32))
    (type $$sig (func (param i32)))
    (table funcref (elem $$f))
    (func $$type-first-empty-in-block
      (block
        (call_indirect (type $$sig) (i32.const 0))
      )
    )
  )`), `type mismatch`);

// ./test/core/call_indirect.wast:846
assert_invalid(() =>
  instantiate(`(module
    (func $$f (param i32 i32))
    (type $$sig (func (param i32 i32)))
    (table funcref (elem $$f))
    (func $$type-second-empty-in-block
      (block
        (call_indirect (type $$sig) (i32.const 0) (i32.const 0))
      )
    )
  )`), `type mismatch`);

// ./test/core/call_indirect.wast:859
assert_invalid(() =>
  instantiate(`(module
    (func $$f (param i32))
    (type $$sig (func (param i32)))
    (table funcref (elem $$f))
    (func $$type-first-empty-in-loop
      (loop
        (call_indirect (type $$sig) (i32.const 0))
      )
    )
  )`), `type mismatch`);

// ./test/core/call_indirect.wast:872
assert_invalid(() =>
  instantiate(`(module
    (func $$f (param i32 i32))
    (type $$sig (func (param i32 i32)))
    (table funcref (elem $$f))
    (func $$type-second-empty-in-loop
      (loop
        (call_indirect (type $$sig) (i32.const 0) (i32.const 0))
      )
    )
  )`), `type mismatch`);

// ./test/core/call_indirect.wast:885
assert_invalid(() =>
  instantiate(`(module
    (func $$f (param i32))
    (type $$sig (func (param i32)))
    (table funcref (elem $$f))
    (func $$type-first-empty-in-then
      (i32.const 0) (i32.const 0)
      (if
        (then
          (call_indirect (type $$sig) (i32.const 0))
        )
      )
    )
  )`), `type mismatch`);

// ./test/core/call_indirect.wast:901
assert_invalid(() =>
  instantiate(`(module
    (func $$f (param i32 i32))
    (type $$sig (func (param i32 i32)))
    (table funcref (elem $$f))
    (func $$type-second-empty-in-then
      (i32.const 0) (i32.const 0)
      (if
        (then
          (call_indirect (type $$sig) (i32.const 0) (i32.const 0))
        )
      )
    )
  )`), `type mismatch`);

// ./test/core/call_indirect.wast:921
assert_invalid(() =>
  instantiate(`(module
    (table 0 funcref)
    (func $$unbound-type (call_indirect (type 1) (i32.const 0)))
  )`), `unknown type`);

// ./test/core/call_indirect.wast:928
assert_invalid(() =>
  instantiate(`(module
    (table 0 funcref)
    (func $$large-type (call_indirect (type 1012321300) (i32.const 0)))
  )`), `unknown type`);

// ./test/core/call_indirect.wast:939
assert_invalid(
  () => instantiate(`(module (table funcref (elem 0 0)))`),
  `unknown function`,
);
