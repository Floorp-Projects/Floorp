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

  (func (export "multi") (param i32) (result i32 i32)
    (if (local.get 0) (then (call $$dummy) (call $$dummy) (call $$dummy)))
    (if (local.get 0) (then) (else (call $$dummy) (call $$dummy) (call $$dummy)))
    (if (result i32) (local.get 0)
      (then (call $$dummy) (call $$dummy) (i32.const 8) (call $$dummy))
      (else (call $$dummy) (call $$dummy) (i32.const 9) (call $$dummy))
    )
    (if (result i32 i64 i32) (local.get 0)
      (then
        (call $$dummy) (call $$dummy) (i32.const 1) (call $$dummy)
        (call $$dummy) (call $$dummy) (i64.const 2) (call $$dummy)
        (call $$dummy) (call $$dummy) (i32.const 3) (call $$dummy)
      )
      (else
        (call $$dummy) (call $$dummy) (i32.const -1) (call $$dummy)
        (call $$dummy) (call $$dummy) (i64.const -2) (call $$dummy)
        (call $$dummy) (call $$dummy) (i32.const -3) (call $$dummy)
      )
    )
    (drop) (drop)
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
  (func (export "as-binary-operands") (param i32) (result i32)
    (i32.mul
      (if (result i32 i32) (local.get 0)
        (then (call $$dummy) (i32.const 3) (call $$dummy) (i32.const 4))
        (else (call $$dummy) (i32.const 3) (call $$dummy) (i32.const -4))
      )
    )
  )
  (func (export "as-compare-operands") (param i32) (result i32)
    (f32.gt
      (if (result f32 f32) (local.get 0)
        (then (call $$dummy) (f32.const 3) (call $$dummy) (f32.const 3))
        (else (call $$dummy) (f32.const -2) (call $$dummy) (f32.const -3))
      )
    )
  )
  (func (export "as-mixed-operands") (param i32) (result i32)
    (if (result i32 i32) (local.get 0)
      (then (call $$dummy) (i32.const 3) (call $$dummy) (i32.const 4))
      (else (call $$dummy) (i32.const -3) (call $$dummy) (i32.const -4))
    )
    (i32.const 5)
    (i32.add)
    (i32.mul)
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
  (func (export "break-multi-value") (param i32) (result i32 i32 i64)
    (if (result i32 i32 i64) (local.get 0)
      (then
        (br 0 (i32.const 18) (i32.const -18) (i64.const 18))
        (i32.const 19) (i32.const -19) (i64.const 19)
      )
      (else
        (br 0 (i32.const -18) (i32.const 18) (i64.const -18))
        (i32.const -19) (i32.const 19) (i64.const -19)
      )
    )
  )

  (func (export "param") (param i32) (result i32)
    (i32.const 1)
    (if (param i32) (result i32) (local.get 0)
      (then (i32.const 2) (i32.add))
      (else (i32.const -2) (i32.add))
    )
  )
  (func (export "params") (param i32) (result i32)
    (i32.const 1)
    (i32.const 2)
    (if (param i32 i32) (result i32) (local.get 0)
      (then (i32.add))
      (else (i32.sub))
    )
  )
  (func (export "params-id") (param i32) (result i32)
    (i32.const 1)
    (i32.const 2)
    (if (param i32 i32) (result i32 i32) (local.get 0) (then))
    (i32.add)
  )
  (func (export "param-break") (param i32) (result i32)
    (i32.const 1)
    (if (param i32) (result i32) (local.get 0)
      (then (i32.const 2) (i32.add) (br 0))
      (else (i32.const -2) (i32.add) (br 0))
    )
  )
  (func (export "params-break") (param i32) (result i32)
    (i32.const 1)
    (i32.const 2)
    (if (param i32 i32) (result i32) (local.get 0)
      (then (i32.add) (br 0))
      (else (i32.sub) (br 0))
    )
  )
  (func (export "params-id-break") (param i32) (result i32)
    (i32.const 1)
    (i32.const 2)
    (if (param i32 i32) (result i32 i32) (local.get 0) (then (br 0)))
    (i32.add)
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

  ;; Examples

  (func $$add64_u_with_carry (export "add64_u_with_carry")
    (param $$i i64) (param $$j i64) (param $$c i32) (result i64 i32)
    (local $$k i64)
    (local.set $$k
      (i64.add
        (i64.add (local.get $$i) (local.get $$j))
        (i64.extend_i32_u (local.get $$c))
      )
    )
    (return (local.get $$k) (i64.lt_u (local.get $$k) (local.get $$i)))
  )

  (func $$add64_u_saturated (export "add64_u_saturated")
    (param i64 i64) (result i64)
    (call $$add64_u_with_carry (local.get 0) (local.get 1) (i32.const 0))
    (if (param i64) (result i64)
      (then (drop) (i64.const -1))
    )
  )

  ;; Block signature syntax

  (type $$block-sig-1 (func))
  (type $$block-sig-2 (func (result i32)))
  (type $$block-sig-3 (func (param $$x i32)))
  (type $$block-sig-4 (func (param i32 f64 i32) (result i32 f64 i32)))

  (func (export "type-use")
    (if (type $$block-sig-1) (i32.const 1) (then))
    (if (type $$block-sig-2) (i32.const 1)
      (then (i32.const 0)) (else (i32.const 2))
    )
    (if (type $$block-sig-3) (i32.const 1) (then (drop)) (else (drop)))
    (i32.const 0) (f64.const 0) (i32.const 0)
    (if (type $$block-sig-4) (i32.const 1) (then))
    (drop) (drop) (drop)
    (if (type $$block-sig-2) (result i32) (i32.const 1)
      (then (i32.const 0)) (else (i32.const 2))
    )
    (if (type $$block-sig-3) (param i32) (i32.const 1)
      (then (drop)) (else (drop))
    )
    (i32.const 0) (f64.const 0) (i32.const 0)
    (if (type $$block-sig-4)
      (param i32) (param f64 i32) (result i32 f64) (result i32)
      (i32.const 1) (then)
    )
    (drop) (drop) (drop)
  )

  ;; Atypical folded condition syntax

  (func (export "atypical-condition")
    i32.const 0
    (if (then) (else))
    (if (i32.const 1) (i32.eqz) (then) (else))
  )
)`);

// ./test/core/if.wast:537
assert_return(() => invoke($0, `empty`, [0]), []);

// ./test/core/if.wast:538
assert_return(() => invoke($0, `empty`, [1]), []);

// ./test/core/if.wast:539
assert_return(() => invoke($0, `empty`, [100]), []);

// ./test/core/if.wast:540
assert_return(() => invoke($0, `empty`, [-2]), []);

// ./test/core/if.wast:542
assert_return(() => invoke($0, `singular`, [0]), [value("i32", 8)]);

// ./test/core/if.wast:543
assert_return(() => invoke($0, `singular`, [1]), [value("i32", 7)]);

// ./test/core/if.wast:544
assert_return(() => invoke($0, `singular`, [10]), [value("i32", 7)]);

// ./test/core/if.wast:545
assert_return(() => invoke($0, `singular`, [-10]), [value("i32", 7)]);

// ./test/core/if.wast:547
assert_return(() => invoke($0, `multi`, [0]), [value("i32", 9), value("i32", -1)]);

// ./test/core/if.wast:548
assert_return(() => invoke($0, `multi`, [1]), [value("i32", 8), value("i32", 1)]);

// ./test/core/if.wast:549
assert_return(() => invoke($0, `multi`, [13]), [value("i32", 8), value("i32", 1)]);

// ./test/core/if.wast:550
assert_return(() => invoke($0, `multi`, [-5]), [value("i32", 8), value("i32", 1)]);

// ./test/core/if.wast:552
assert_return(() => invoke($0, `nested`, [0, 0]), [value("i32", 11)]);

// ./test/core/if.wast:553
assert_return(() => invoke($0, `nested`, [1, 0]), [value("i32", 10)]);

// ./test/core/if.wast:554
assert_return(() => invoke($0, `nested`, [0, 1]), [value("i32", 10)]);

// ./test/core/if.wast:555
assert_return(() => invoke($0, `nested`, [3, 2]), [value("i32", 9)]);

// ./test/core/if.wast:556
assert_return(() => invoke($0, `nested`, [0, -100]), [value("i32", 10)]);

// ./test/core/if.wast:557
assert_return(() => invoke($0, `nested`, [10, 10]), [value("i32", 9)]);

// ./test/core/if.wast:558
assert_return(() => invoke($0, `nested`, [0, -1]), [value("i32", 10)]);

// ./test/core/if.wast:559
assert_return(() => invoke($0, `nested`, [-111, -2]), [value("i32", 9)]);

// ./test/core/if.wast:561
assert_return(() => invoke($0, `as-select-first`, [0]), [value("i32", 0)]);

// ./test/core/if.wast:562
assert_return(() => invoke($0, `as-select-first`, [1]), [value("i32", 1)]);

// ./test/core/if.wast:563
assert_return(() => invoke($0, `as-select-mid`, [0]), [value("i32", 2)]);

// ./test/core/if.wast:564
assert_return(() => invoke($0, `as-select-mid`, [1]), [value("i32", 2)]);

// ./test/core/if.wast:565
assert_return(() => invoke($0, `as-select-last`, [0]), [value("i32", 3)]);

// ./test/core/if.wast:566
assert_return(() => invoke($0, `as-select-last`, [1]), [value("i32", 2)]);

// ./test/core/if.wast:568
assert_return(() => invoke($0, `as-loop-first`, [0]), [value("i32", 0)]);

// ./test/core/if.wast:569
assert_return(() => invoke($0, `as-loop-first`, [1]), [value("i32", 1)]);

// ./test/core/if.wast:570
assert_return(() => invoke($0, `as-loop-mid`, [0]), [value("i32", 0)]);

// ./test/core/if.wast:571
assert_return(() => invoke($0, `as-loop-mid`, [1]), [value("i32", 1)]);

// ./test/core/if.wast:572
assert_return(() => invoke($0, `as-loop-last`, [0]), [value("i32", 0)]);

// ./test/core/if.wast:573
assert_return(() => invoke($0, `as-loop-last`, [1]), [value("i32", 1)]);

// ./test/core/if.wast:575
assert_return(() => invoke($0, `as-if-condition`, [0]), [value("i32", 3)]);

// ./test/core/if.wast:576
assert_return(() => invoke($0, `as-if-condition`, [1]), [value("i32", 2)]);

// ./test/core/if.wast:578
assert_return(() => invoke($0, `as-br_if-first`, [0]), [value("i32", 0)]);

// ./test/core/if.wast:579
assert_return(() => invoke($0, `as-br_if-first`, [1]), [value("i32", 1)]);

// ./test/core/if.wast:580
assert_return(() => invoke($0, `as-br_if-last`, [0]), [value("i32", 3)]);

// ./test/core/if.wast:581
assert_return(() => invoke($0, `as-br_if-last`, [1]), [value("i32", 2)]);

// ./test/core/if.wast:583
assert_return(() => invoke($0, `as-br_table-first`, [0]), [value("i32", 0)]);

// ./test/core/if.wast:584
assert_return(() => invoke($0, `as-br_table-first`, [1]), [value("i32", 1)]);

// ./test/core/if.wast:585
assert_return(() => invoke($0, `as-br_table-last`, [0]), [value("i32", 2)]);

// ./test/core/if.wast:586
assert_return(() => invoke($0, `as-br_table-last`, [1]), [value("i32", 2)]);

// ./test/core/if.wast:588
assert_return(() => invoke($0, `as-call_indirect-first`, [0]), [value("i32", 0)]);

// ./test/core/if.wast:589
assert_return(() => invoke($0, `as-call_indirect-first`, [1]), [value("i32", 1)]);

// ./test/core/if.wast:590
assert_return(() => invoke($0, `as-call_indirect-mid`, [0]), [value("i32", 2)]);

// ./test/core/if.wast:591
assert_return(() => invoke($0, `as-call_indirect-mid`, [1]), [value("i32", 2)]);

// ./test/core/if.wast:592
assert_return(() => invoke($0, `as-call_indirect-last`, [0]), [value("i32", 2)]);

// ./test/core/if.wast:593
assert_trap(() => invoke($0, `as-call_indirect-last`, [1]), `undefined element`);

// ./test/core/if.wast:595
assert_return(() => invoke($0, `as-store-first`, [0]), []);

// ./test/core/if.wast:596
assert_return(() => invoke($0, `as-store-first`, [1]), []);

// ./test/core/if.wast:597
assert_return(() => invoke($0, `as-store-last`, [0]), []);

// ./test/core/if.wast:598
assert_return(() => invoke($0, `as-store-last`, [1]), []);

// ./test/core/if.wast:600
assert_return(() => invoke($0, `as-memory.grow-value`, [0]), [value("i32", 1)]);

// ./test/core/if.wast:601
assert_return(() => invoke($0, `as-memory.grow-value`, [1]), [value("i32", 1)]);

// ./test/core/if.wast:603
assert_return(() => invoke($0, `as-call-value`, [0]), [value("i32", 0)]);

// ./test/core/if.wast:604
assert_return(() => invoke($0, `as-call-value`, [1]), [value("i32", 1)]);

// ./test/core/if.wast:606
assert_return(() => invoke($0, `as-return-value`, [0]), [value("i32", 0)]);

// ./test/core/if.wast:607
assert_return(() => invoke($0, `as-return-value`, [1]), [value("i32", 1)]);

// ./test/core/if.wast:609
assert_return(() => invoke($0, `as-drop-operand`, [0]), []);

// ./test/core/if.wast:610
assert_return(() => invoke($0, `as-drop-operand`, [1]), []);

// ./test/core/if.wast:612
assert_return(() => invoke($0, `as-br-value`, [0]), [value("i32", 0)]);

// ./test/core/if.wast:613
assert_return(() => invoke($0, `as-br-value`, [1]), [value("i32", 1)]);

// ./test/core/if.wast:615
assert_return(() => invoke($0, `as-local.set-value`, [0]), [value("i32", 0)]);

// ./test/core/if.wast:616
assert_return(() => invoke($0, `as-local.set-value`, [1]), [value("i32", 1)]);

// ./test/core/if.wast:618
assert_return(() => invoke($0, `as-local.tee-value`, [0]), [value("i32", 0)]);

// ./test/core/if.wast:619
assert_return(() => invoke($0, `as-local.tee-value`, [1]), [value("i32", 1)]);

// ./test/core/if.wast:621
assert_return(() => invoke($0, `as-global.set-value`, [0]), [value("i32", 0)]);

// ./test/core/if.wast:622
assert_return(() => invoke($0, `as-global.set-value`, [1]), [value("i32", 1)]);

// ./test/core/if.wast:624
assert_return(() => invoke($0, `as-load-operand`, [0]), [value("i32", 0)]);

// ./test/core/if.wast:625
assert_return(() => invoke($0, `as-load-operand`, [1]), [value("i32", 0)]);

// ./test/core/if.wast:627
assert_return(() => invoke($0, `as-unary-operand`, [0]), [value("i32", 0)]);

// ./test/core/if.wast:628
assert_return(() => invoke($0, `as-unary-operand`, [1]), [value("i32", 0)]);

// ./test/core/if.wast:629
assert_return(() => invoke($0, `as-unary-operand`, [-1]), [value("i32", 0)]);

// ./test/core/if.wast:631
assert_return(() => invoke($0, `as-binary-operand`, [0, 0]), [value("i32", 15)]);

// ./test/core/if.wast:632
assert_return(() => invoke($0, `as-binary-operand`, [0, 1]), [value("i32", -12)]);

// ./test/core/if.wast:633
assert_return(() => invoke($0, `as-binary-operand`, [1, 0]), [value("i32", -15)]);

// ./test/core/if.wast:634
assert_return(() => invoke($0, `as-binary-operand`, [1, 1]), [value("i32", 12)]);

// ./test/core/if.wast:636
assert_return(() => invoke($0, `as-test-operand`, [0]), [value("i32", 1)]);

// ./test/core/if.wast:637
assert_return(() => invoke($0, `as-test-operand`, [1]), [value("i32", 0)]);

// ./test/core/if.wast:639
assert_return(() => invoke($0, `as-compare-operand`, [0, 0]), [value("i32", 1)]);

// ./test/core/if.wast:640
assert_return(() => invoke($0, `as-compare-operand`, [0, 1]), [value("i32", 0)]);

// ./test/core/if.wast:641
assert_return(() => invoke($0, `as-compare-operand`, [1, 0]), [value("i32", 1)]);

// ./test/core/if.wast:642
assert_return(() => invoke($0, `as-compare-operand`, [1, 1]), [value("i32", 0)]);

// ./test/core/if.wast:644
assert_return(() => invoke($0, `as-binary-operands`, [0]), [value("i32", -12)]);

// ./test/core/if.wast:645
assert_return(() => invoke($0, `as-binary-operands`, [1]), [value("i32", 12)]);

// ./test/core/if.wast:647
assert_return(() => invoke($0, `as-compare-operands`, [0]), [value("i32", 1)]);

// ./test/core/if.wast:648
assert_return(() => invoke($0, `as-compare-operands`, [1]), [value("i32", 0)]);

// ./test/core/if.wast:650
assert_return(() => invoke($0, `as-mixed-operands`, [0]), [value("i32", -3)]);

// ./test/core/if.wast:651
assert_return(() => invoke($0, `as-mixed-operands`, [1]), [value("i32", 27)]);

// ./test/core/if.wast:653
assert_return(() => invoke($0, `break-bare`, []), [value("i32", 19)]);

// ./test/core/if.wast:654
assert_return(() => invoke($0, `break-value`, [1]), [value("i32", 18)]);

// ./test/core/if.wast:655
assert_return(() => invoke($0, `break-value`, [0]), [value("i32", 21)]);

// ./test/core/if.wast:656
assert_return(
  () => invoke($0, `break-multi-value`, [0]),
  [value("i32", -18), value("i32", 18), value("i64", -18n)],
);

// ./test/core/if.wast:659
assert_return(
  () => invoke($0, `break-multi-value`, [1]),
  [value("i32", 18), value("i32", -18), value("i64", 18n)],
);

// ./test/core/if.wast:663
assert_return(() => invoke($0, `param`, [0]), [value("i32", -1)]);

// ./test/core/if.wast:664
assert_return(() => invoke($0, `param`, [1]), [value("i32", 3)]);

// ./test/core/if.wast:665
assert_return(() => invoke($0, `params`, [0]), [value("i32", -1)]);

// ./test/core/if.wast:666
assert_return(() => invoke($0, `params`, [1]), [value("i32", 3)]);

// ./test/core/if.wast:667
assert_return(() => invoke($0, `params-id`, [0]), [value("i32", 3)]);

// ./test/core/if.wast:668
assert_return(() => invoke($0, `params-id`, [1]), [value("i32", 3)]);

// ./test/core/if.wast:669
assert_return(() => invoke($0, `param-break`, [0]), [value("i32", -1)]);

// ./test/core/if.wast:670
assert_return(() => invoke($0, `param-break`, [1]), [value("i32", 3)]);

// ./test/core/if.wast:671
assert_return(() => invoke($0, `params-break`, [0]), [value("i32", -1)]);

// ./test/core/if.wast:672
assert_return(() => invoke($0, `params-break`, [1]), [value("i32", 3)]);

// ./test/core/if.wast:673
assert_return(() => invoke($0, `params-id-break`, [0]), [value("i32", 3)]);

// ./test/core/if.wast:674
assert_return(() => invoke($0, `params-id-break`, [1]), [value("i32", 3)]);

// ./test/core/if.wast:676
assert_return(() => invoke($0, `effects`, [1]), [value("i32", -14)]);

// ./test/core/if.wast:677
assert_return(() => invoke($0, `effects`, [0]), [value("i32", -6)]);

// ./test/core/if.wast:679
assert_return(() => invoke($0, `add64_u_with_carry`, [0n, 0n, 0]), [value("i64", 0n), value("i32", 0)]);

// ./test/core/if.wast:683
assert_return(
  () => invoke($0, `add64_u_with_carry`, [100n, 124n, 0]),
  [value("i64", 224n), value("i32", 0)],
);

// ./test/core/if.wast:687
assert_return(
  () => invoke($0, `add64_u_with_carry`, [-1n, 0n, 0]),
  [value("i64", -1n), value("i32", 0)],
);

// ./test/core/if.wast:691
assert_return(() => invoke($0, `add64_u_with_carry`, [-1n, 1n, 0]), [value("i64", 0n), value("i32", 1)]);

// ./test/core/if.wast:695
assert_return(
  () => invoke($0, `add64_u_with_carry`, [-1n, -1n, 0]),
  [value("i64", -2n), value("i32", 1)],
);

// ./test/core/if.wast:699
assert_return(() => invoke($0, `add64_u_with_carry`, [-1n, 0n, 1]), [value("i64", 0n), value("i32", 1)]);

// ./test/core/if.wast:703
assert_return(() => invoke($0, `add64_u_with_carry`, [-1n, 1n, 1]), [value("i64", 1n), value("i32", 1)]);

// ./test/core/if.wast:707
assert_return(
  () => invoke($0, `add64_u_with_carry`, [
    -9223372036854775808n,
    -9223372036854775808n,
    0,
  ]),
  [value("i64", 0n), value("i32", 1)],
);

// ./test/core/if.wast:712
assert_return(() => invoke($0, `add64_u_saturated`, [0n, 0n]), [value("i64", 0n)]);

// ./test/core/if.wast:715
assert_return(() => invoke($0, `add64_u_saturated`, [1230n, 23n]), [value("i64", 1253n)]);

// ./test/core/if.wast:718
assert_return(() => invoke($0, `add64_u_saturated`, [-1n, 0n]), [value("i64", -1n)]);

// ./test/core/if.wast:721
assert_return(() => invoke($0, `add64_u_saturated`, [-1n, 1n]), [value("i64", -1n)]);

// ./test/core/if.wast:724
assert_return(() => invoke($0, `add64_u_saturated`, [-1n, -1n]), [value("i64", -1n)]);

// ./test/core/if.wast:727
assert_return(
  () => invoke($0, `add64_u_saturated`, [-9223372036854775808n, -9223372036854775808n]),
  [value("i64", -1n)],
);

// ./test/core/if.wast:731
assert_return(() => invoke($0, `type-use`, []), []);

// ./test/core/if.wast:733
assert_return(() => invoke($0, `atypical-condition`, []), []);

// ./test/core/if.wast:735
assert_malformed(
  () => instantiate(`(type $$sig (func (param i32) (result i32))) (func (i32.const 0)   (if (type $$sig) (result i32) (param i32) (i32.const 1) (then)) ) `),
  `unexpected token`,
);

// ./test/core/if.wast:744
assert_malformed(
  () => instantiate(`(type $$sig (func (param i32) (result i32))) (func (i32.const 0)   (if (param i32) (type $$sig) (result i32) (i32.const 1) (then)) ) `),
  `unexpected token`,
);

// ./test/core/if.wast:753
assert_malformed(
  () => instantiate(`(type $$sig (func (param i32) (result i32))) (func (i32.const 0)   (if (param i32) (result i32) (type $$sig) (i32.const 1) (then)) ) `),
  `unexpected token`,
);

// ./test/core/if.wast:762
assert_malformed(
  () => instantiate(`(type $$sig (func (param i32) (result i32))) (func (i32.const 0)   (if (result i32) (type $$sig) (param i32) (i32.const 1) (then)) ) `),
  `unexpected token`,
);

// ./test/core/if.wast:771
assert_malformed(
  () => instantiate(`(type $$sig (func (param i32) (result i32))) (func (i32.const 0)   (if (result i32) (param i32) (type $$sig) (i32.const 1) (then)) ) `),
  `unexpected token`,
);

// ./test/core/if.wast:780
assert_malformed(
  () => instantiate(`(func (i32.const 0) (if (result i32) (param i32) (i32.const 1) (then))) `),
  `unexpected token`,
);

// ./test/core/if.wast:787
assert_malformed(
  () => instantiate(`(func (i32.const 0) (i32.const 1)   (if (param $$x i32) (then (drop)) (else (drop))) ) `),
  `unexpected token`,
);

// ./test/core/if.wast:795
assert_malformed(
  () => instantiate(`(type $$sig (func)) (func (i32.const 1)   (if (type $$sig) (result i32) (then (i32.const 0)) (else (i32.const 2)))   (unreachable) ) `),
  `inline function type`,
);

// ./test/core/if.wast:805
assert_malformed(
  () => instantiate(`(type $$sig (func (param i32) (result i32))) (func (i32.const 1)   (if (type $$sig) (result i32) (then (i32.const 0)) (else (i32.const 2)))   (unreachable) ) `),
  `inline function type`,
);

// ./test/core/if.wast:815
assert_malformed(
  () => instantiate(`(type $$sig (func (param i32) (result i32))) (func (i32.const 0) (i32.const 1)   (if (type $$sig) (param i32) (then (drop)) (else (drop)))   (unreachable) ) `),
  `inline function type`,
);

// ./test/core/if.wast:825
assert_malformed(
  () => instantiate(`(type $$sig (func (param i32 i32) (result i32))) (func (i32.const 0) (i32.const 1)   (if (type $$sig) (param i32) (result i32) (then)) (unreachable) ) `),
  `inline function type`,
);

// ./test/core/if.wast:835
assert_invalid(
  () => instantiate(`(module
    (type $$sig (func))
    (func (i32.const 1) (if (type $$sig) (i32.const 0) (then)))
  )`),
  `type mismatch`,
);

// ./test/core/if.wast:843
assert_invalid(
  () => instantiate(`(module (func $$type-empty-i32 (result i32) (if (i32.const 0) (then))))`),
  `type mismatch`,
);

// ./test/core/if.wast:847
assert_invalid(
  () => instantiate(`(module (func $$type-empty-i64 (result i64) (if (i32.const 0) (then))))`),
  `type mismatch`,
);

// ./test/core/if.wast:851
assert_invalid(
  () => instantiate(`(module (func $$type-empty-f32 (result f32) (if (i32.const 0) (then))))`),
  `type mismatch`,
);

// ./test/core/if.wast:855
assert_invalid(
  () => instantiate(`(module (func $$type-empty-f64 (result f64) (if (i32.const 0) (then))))`),
  `type mismatch`,
);

// ./test/core/if.wast:860
assert_invalid(
  () => instantiate(`(module (func $$type-empty-i32 (result i32) (if (i32.const 0) (then) (else))))`),
  `type mismatch`,
);

// ./test/core/if.wast:864
assert_invalid(
  () => instantiate(`(module (func $$type-empty-i64 (result i64) (if (i32.const 0) (then) (else))))`),
  `type mismatch`,
);

// ./test/core/if.wast:868
assert_invalid(
  () => instantiate(`(module (func $$type-empty-f32 (result f32) (if (i32.const 0) (then) (else))))`),
  `type mismatch`,
);

// ./test/core/if.wast:872
assert_invalid(
  () => instantiate(`(module (func $$type-empty-f64 (result f64) (if (i32.const 0) (then) (else))))`),
  `type mismatch`,
);

// ./test/core/if.wast:877
assert_invalid(
  () => instantiate(`(module (func $$type-then-value-num-vs-void
    (if (i32.const 1) (then (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:883
assert_invalid(
  () => instantiate(`(module (func $$type-then-value-num-vs-void-else
    (if (i32.const 1) (then (i32.const 1)) (else))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:889
assert_invalid(
  () => instantiate(`(module (func $$type-else-value-num-vs-void
    (if (i32.const 1) (then) (else (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:895
assert_invalid(
  () => instantiate(`(module (func $$type-both-value-num-vs-void
    (if (i32.const 1) (then (i32.const 1)) (else (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:902
assert_invalid(
  () => instantiate(`(module (func $$type-then-value-nums-vs-void
    (if (i32.const 1) (then (i32.const 1) (i32.const 2)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:908
assert_invalid(
  () => instantiate(`(module (func $$type-then-value-nums-vs-void-else
    (if (i32.const 1) (then (i32.const 1) (i32.const 2)) (else))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:914
assert_invalid(
  () => instantiate(`(module (func $$type-else-value-nums-vs-void
    (if (i32.const 1) (then) (else (i32.const 1) (i32.const 2)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:920
assert_invalid(
  () => instantiate(`(module (func $$type-both-value-nums-vs-void
    (if (i32.const 1) (then (i32.const 1) (i32.const 2)) (else (i32.const 2) (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:927
assert_invalid(
  () => instantiate(`(module (func $$type-then-value-empty-vs-num (result i32)
    (if (result i32) (i32.const 1) (then) (else (i32.const 0)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:933
assert_invalid(
  () => instantiate(`(module (func $$type-else-value-empty-vs-num (result i32)
    (if (result i32) (i32.const 1) (then (i32.const 0)) (else))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:939
assert_invalid(
  () => instantiate(`(module (func $$type-both-value-empty-vs-num (result i32)
    (if (result i32) (i32.const 1) (then) (else))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:946
assert_invalid(
  () => instantiate(`(module (func $$type-then-value-empty-vs-nums (result i32 i32)
    (if (result i32 i32) (i32.const 1) (then) (else (i32.const 0) (i32.const 2)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:952
assert_invalid(
  () => instantiate(`(module (func $$type-else-value-empty-vs-nums (result i32 i32)
    (if (result i32 i32) (i32.const 1) (then (i32.const 0) (i32.const 1)) (else))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:958
assert_invalid(
  () => instantiate(`(module (func $$type-both-value-empty-vs-nums (result i32 i32)
    (if (result i32 i32) (i32.const 1) (then) (else))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:965
assert_invalid(
  () => instantiate(`(module (func $$type-no-else-vs-num (result i32)
    (if (result i32) (i32.const 1) (then (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:971
assert_invalid(
  () => instantiate(`(module (func $$type-no-else-vs-nums (result i32 i32)
    (if (result i32 i32) (i32.const 1) (then (i32.const 1) (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:978
assert_invalid(
  () => instantiate(`(module (func $$type-then-value-void-vs-num (result i32)
    (if (result i32) (i32.const 1) (then (nop)) (else (i32.const 0)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:984
assert_invalid(
  () => instantiate(`(module (func $$type-else-value-void-vs-num (result i32)
    (if (result i32) (i32.const 1) (then (i32.const 0)) (else (nop)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:990
assert_invalid(
  () => instantiate(`(module (func $$type-both-value-void-vs-num (result i32)
    (if (result i32) (i32.const 1) (then (nop)) (else (nop)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:997
assert_invalid(
  () => instantiate(`(module (func $$type-then-value-void-vs-nums (result i32 i32)
    (if (result i32 i32) (i32.const 1) (then (nop)) (else (i32.const 0) (i32.const 0)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1003
assert_invalid(
  () => instantiate(`(module (func $$type-else-value-void-vs-nums (result i32 i32)
    (if (result i32 i32) (i32.const 1) (then (i32.const 0) (i32.const 0)) (else (nop)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1009
assert_invalid(
  () => instantiate(`(module (func $$type-both-value-void-vs-nums (result i32 i32)
    (if (result i32 i32) (i32.const 1) (then (nop)) (else (nop)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1016
assert_invalid(
  () => instantiate(`(module (func $$type-then-value-num-vs-num (result i32)
    (if (result i32) (i32.const 1) (then (i64.const 1)) (else (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1022
assert_invalid(
  () => instantiate(`(module (func $$type-else-value-num-vs-num (result i32)
    (if (result i32) (i32.const 1) (then (i32.const 1)) (else (i64.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1028
assert_invalid(
  () => instantiate(`(module (func $$type-both-value-num-vs-num (result i32)
    (if (result i32) (i32.const 1) (then (i64.const 1)) (else (i64.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1035
assert_invalid(
  () => instantiate(`(module (func $$type-then-value-num-vs-nums (result i32 i32)
    (if (result i32 i32) (i32.const 1) (then (i32.const 1)) (else (i32.const 1) (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1041
assert_invalid(
  () => instantiate(`(module (func $$type-else-value-num-vs-nums (result i32 i32)
    (if (result i32 i32) (i32.const 1) (then (i32.const 1) (i32.const 1)) (else (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1047
assert_invalid(
  () => instantiate(`(module (func $$type-both-value-num-vs-nums (result i32 i32)
    (if (result i32 i32) (i32.const 1) (then (i32.const 1)) (else (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1054
assert_invalid(
  () => instantiate(`(module (func $$type-then-value-partial-vs-nums (result i32 i32)
    (i32.const 0)
    (if (result i32 i32) (i32.const 1) (then (i32.const 1)) (else (i32.const 1) (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1061
assert_invalid(
  () => instantiate(`(module (func $$type-else-value-partial-vs-nums (result i32 i32)
    (i32.const 0)
    (if (result i32 i32) (i32.const 1) (then (i32.const 1) (i32.const 1)) (else (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1068
assert_invalid(
  () => instantiate(`(module (func $$type-both-value-partial-vs-nums (result i32 i32)
    (i32.const 0)
    (if (result i32 i32) (i32.const 1) (then (i32.const 1)) (else (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1076
assert_invalid(
  () => instantiate(`(module (func $$type-then-value-nums-vs-num (result i32)
    (if (result i32) (i32.const 1) (then (i32.const 1) (i32.const 1)) (else (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1082
assert_invalid(
  () => instantiate(`(module (func $$type-else-value-nums-vs-num (result i32)
    (if (result i32) (i32.const 1) (then (i32.const 1)) (else (i32.const 1) (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1088
assert_invalid(
  () => instantiate(`(module (func $$type-both-value-nums-vs-num (result i32)
    (if (result i32) (i32.const 1) (then (i32.const 1) (i32.const 1)) (else (i32.const 1) (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1095
assert_invalid(
  () => instantiate(`(module (func $$type-both-different-value-num-vs-num (result i32)
    (if (result i32) (i32.const 1) (then (i64.const 1)) (else (f64.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1101
assert_invalid(
  () => instantiate(`(module (func $$type-both-different-value-nums-vs-nums (result i32 i32)
    (if (result i32 i32) (i32.const 1) (then (i32.const 1) (i32.const 1) (i32.const 1)) (else (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1108
assert_invalid(
  () => instantiate(`(module (func $$type-then-value-unreached-select (result i32)
    (if (result i64)
      (i32.const 0)
      (then (select (unreachable) (unreachable) (unreachable)))
      (else (i64.const 0))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1118
assert_invalid(
  () => instantiate(`(module (func $$type-else-value-unreached-select (result i32)
    (if (result i64)
      (i32.const 1)
      (then (i64.const 0))
      (else (select (unreachable) (unreachable) (unreachable)))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1128
assert_invalid(
  () => instantiate(`(module (func $$type-else-value-unreached-select (result i32)
    (if (result i64)
      (i32.const 1)
      (then (select (unreachable) (unreachable) (unreachable)))
      (else (select (unreachable) (unreachable) (unreachable)))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1139
assert_invalid(
  () => instantiate(`(module (func $$type-then-break-last-void-vs-num (result i32)
    (if (result i32) (i32.const 1) (then (br 0)) (else (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1145
assert_invalid(
  () => instantiate(`(module (func $$type-else-break-last-void-vs-num (result i32)
    (if (result i32) (i32.const 1) (then (i32.const 1)) (else (br 0)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1151
assert_invalid(
  () => instantiate(`(module (func $$type-then-break-last-void-vs-nums (result i32 i32)
    (if (result i32 i32) (i32.const 1) (then (br 0)) (else (i32.const 1) (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1157
assert_invalid(
  () => instantiate(`(module (func $$type-else-break-last-void-vs-nums (result i32 i32)
    (if (result i32 i32) (i32.const 1) (then (i32.const 1) (i32.const 1)) (else (br 0)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1164
assert_invalid(
  () => instantiate(`(module (func $$type-then-break-empty-vs-num (result i32)
    (if (result i32) (i32.const 1)
      (then (br 0) (i32.const 1))
      (else (i32.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1173
assert_invalid(
  () => instantiate(`(module (func $$type-else-break-empty-vs-num (result i32)
    (if (result i32) (i32.const 1)
      (then (i32.const 1))
      (else (br 0) (i32.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1182
assert_invalid(
  () => instantiate(`(module (func $$type-then-break-empty-vs-nums (result i32 i32)
    (if (result i32 i32) (i32.const 1)
      (then (br 0) (i32.const 1) (i32.const 1))
      (else (i32.const 1) (i32.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1191
assert_invalid(
  () => instantiate(`(module (func $$type-else-break-empty-vs-nums (result i32 i32)
    (if (result i32 i32) (i32.const 1)
      (then (i32.const 1) (i32.const 1))
      (else (br 0) (i32.const 1) (i32.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1201
assert_invalid(
  () => instantiate(`(module (func $$type-then-break-void-vs-num (result i32)
    (if (result i32) (i32.const 1)
      (then (br 0 (nop)) (i32.const 1))
      (else (i32.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1210
assert_invalid(
  () => instantiate(`(module (func $$type-else-break-void-vs-num (result i32)
    (if (result i32) (i32.const 1)
      (then (i32.const 1))
      (else (br 0 (nop)) (i32.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1219
assert_invalid(
  () => instantiate(`(module (func $$type-then-break-void-vs-nums (result i32 i32)
    (if (result i32 i32) (i32.const 1)
      (then (br 0 (nop)) (i32.const 1) (i32.const 1))
      (else (i32.const 1) (i32.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1228
assert_invalid(
  () => instantiate(`(module (func $$type-else-break-void-vs-nums (result i32 i32)
    (if (result i32 i32) (i32.const 1)
      (then (i32.const 1) (i32.const 1))
      (else (br 0 (nop)) (i32.const 1) (i32.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1238
assert_invalid(
  () => instantiate(`(module (func $$type-then-break-num-vs-num (result i32)
    (if (result i32) (i32.const 1)
      (then (br 0 (i64.const 1)) (i32.const 1))
      (else (i32.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1247
assert_invalid(
  () => instantiate(`(module (func $$type-else-break-num-vs-num (result i32)
    (if (result i32) (i32.const 1)
      (then (i32.const 1))
      (else (br 0 (i64.const 1)) (i32.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1256
assert_invalid(
  () => instantiate(`(module (func $$type-then-break-num-vs-nums (result i32 i32)
    (if (result i32 i32) (i32.const 1)
      (then (br 0 (i64.const 1)) (i32.const 1) (i32.const 1))
      (else (i32.const 1) (i32.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1265
assert_invalid(
  () => instantiate(`(module (func $$type-else-break-num-vs-nums (result i32 i32)
    (if (result i32 i32) (i32.const 1)
      (then (i32.const 1) (i32.const 1))
      (else (br 0 (i64.const 1)) (i32.const 1) (i32.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1274
assert_invalid(
  () => instantiate(`(module (func $$type-then-break-partial-vs-nums (result i32 i32)
    (i32.const 1)
    (if (result i32 i32) (i32.const 1)
      (then (br 0 (i64.const 1)) (i32.const 1))
      (else (i32.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1284
assert_invalid(
  () => instantiate(`(module (func $$type-else-break-partial-vs-nums (result i32 i32)
    (i32.const 1)
    (if (result i32 i32) (i32.const 1)
      (then (i32.const 1))
      (else (br 0 (i64.const 1)) (i32.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1295
assert_invalid(
  () => instantiate(`(module
    (func $$type-condition-empty
      (if (then))
    )
  )`),
  `type mismatch`,
);

// ./test/core/if.wast:1303
assert_invalid(
  () => instantiate(`(module
    (func $$type-condition-empty-in-block
      (i32.const 0)
      (block (if (then)))
    )
  )`),
  `type mismatch`,
);

// ./test/core/if.wast:1312
assert_invalid(
  () => instantiate(`(module
    (func $$type-condition-empty-in-loop
      (i32.const 0)
      (loop (if (then)))
    )
  )`),
  `type mismatch`,
);

// ./test/core/if.wast:1321
assert_invalid(
  () => instantiate(`(module
    (func $$type-condition-empty-in-then
      (i32.const 0) (i32.const 0)
      (if (then (if (then))))
    )
  )`),
  `type mismatch`,
);

// ./test/core/if.wast:1330
assert_invalid(
  () => instantiate(`(module
    (func $$type-condition-empty-in-else
      (i32.const 0) (i32.const 0)
      (if (result i32) (then (i32.const 0)) (else (if (then)) (i32.const 0)))
      (drop)
    )
  )`),
  `type mismatch`,
);

// ./test/core/if.wast:1340
assert_invalid(
  () => instantiate(`(module
    (func $$type-condition-empty-in-br
      (i32.const 0)
      (block (br 0 (if(then))) (drop))
    )
  )`),
  `type mismatch`,
);

// ./test/core/if.wast:1349
assert_invalid(
  () => instantiate(`(module
    (func $$type-condition-empty-in-br_if
      (i32.const 0)
      (block (br_if 0 (if(then)) (i32.const 1)) (drop))
    )
  )`),
  `type mismatch`,
);

// ./test/core/if.wast:1358
assert_invalid(
  () => instantiate(`(module
    (func $$type-condition-empty-in-br_table
      (i32.const 0)
      (block (br_table 0 (if(then))) (drop))
    )
  )`),
  `type mismatch`,
);

// ./test/core/if.wast:1367
assert_invalid(
  () => instantiate(`(module
    (func $$type-condition-empty-in-return
      (return (if(then))) (drop)
    )
  )`),
  `type mismatch`,
);

// ./test/core/if.wast:1375
assert_invalid(
  () => instantiate(`(module
    (func $$type-condition-empty-in-select
      (select (if(then)) (i32.const 1) (i32.const 2)) (drop)
    )
  )`),
  `type mismatch`,
);

// ./test/core/if.wast:1383
assert_invalid(
  () => instantiate(`(module
    (func $$type-condition-empty-in-call
      (call 1 (if(then))) (drop)
    )
    (func (param i32) (result i32) (local.get 0))
  )`),
  `type mismatch`,
);

// ./test/core/if.wast:1392
assert_invalid(
  () => instantiate(`(module
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
  )`),
  `type mismatch`,
);

// ./test/core/if.wast:1408
assert_invalid(
  () => instantiate(`(module
    (func $$type-condition-empty-in-local.set
      (local i32)
      (local.set 0 (if(then))) (local.get 0) (drop)
    )
  )`),
  `type mismatch`,
);

// ./test/core/if.wast:1417
assert_invalid(
  () => instantiate(`(module
    (func $$type-condition-empty-in-local.tee
      (local i32)
      (local.tee 0 (if(then))) (drop)
    )
  )`),
  `type mismatch`,
);

// ./test/core/if.wast:1426
assert_invalid(
  () => instantiate(`(module
    (global $$x (mut i32) (i32.const 0))
    (func $$type-condition-empty-in-global.set
      (global.set $$x (if(then))) (global.get $$x) (drop)
    )
  )`),
  `type mismatch`,
);

// ./test/core/if.wast:1435
assert_invalid(
  () => instantiate(`(module
    (memory 0)
    (func $$type-condition-empty-in-memory.grow
      (memory.grow (if(then))) (drop)
    )
  )`),
  `type mismatch`,
);

// ./test/core/if.wast:1444
assert_invalid(
  () => instantiate(`(module
    (memory 0)
    (func $$type-condition-empty-in-load
      (i32.load (if(then))) (drop)
    )
  )`),
  `type mismatch`,
);

// ./test/core/if.wast:1453
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-condition-empty-in-store
      (i32.store (if(then)) (i32.const 1))
    )
  )`),
  `type mismatch`,
);

// ./test/core/if.wast:1463
assert_invalid(
  () => instantiate(`(module (func $$type-param-void-vs-num
    (if (param i32) (i32.const 1) (then (drop)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1469
assert_invalid(
  () => instantiate(`(module (func $$type-param-void-vs-nums
    (if (param i32 f64) (i32.const 1) (then (drop) (drop)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1475
assert_invalid(
  () => instantiate(`(module (func $$type-param-num-vs-num
    (f32.const 0) (if (param i32) (i32.const 1) (then (drop)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1481
assert_invalid(
  () => instantiate(`(module (func $$type-param-num-vs-nums
    (f32.const 0) (if (param f32 i32) (i32.const 1) (then (drop) (drop)))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1487
assert_invalid(
  () => instantiate(`(module (func $$type-param-nested-void-vs-num
    (block (if (param i32) (i32.const 1) (then (drop))))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1493
assert_invalid(
  () => instantiate(`(module (func $$type-param-void-vs-nums
    (block (if (param i32 f64) (i32.const 1) (then (drop) (drop))))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1499
assert_invalid(
  () => instantiate(`(module (func $$type-param-num-vs-num
    (block (f32.const 0) (if (param i32) (i32.const 1) (then (drop))))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1505
assert_invalid(
  () => instantiate(`(module (func $$type-param-num-vs-nums
    (block (f32.const 0) (if (param f32 i32) (i32.const 1) (then (drop) (drop))))
  ))`),
  `type mismatch`,
);

// ./test/core/if.wast:1512
assert_malformed(
  () => instantiate(`(func (param i32) (result i32) if (param $$x i32) end) `),
  `unexpected token`,
);

// ./test/core/if.wast:1516
assert_malformed(
  () => instantiate(`(func (param i32) (result i32) (if (param $$x i32) (then))) `),
  `unexpected token`,
);

// ./test/core/if.wast:1521
assert_malformed(() => instantiate(`(func i32.const 0 if end $$l) `), `mismatching label`);

// ./test/core/if.wast:1525
assert_malformed(
  () => instantiate(`(func i32.const 0 if $$a end $$l) `),
  `mismatching label`,
);

// ./test/core/if.wast:1529
assert_malformed(
  () => instantiate(`(func i32.const 0 if else $$l end) `),
  `mismatching label`,
);

// ./test/core/if.wast:1533
assert_malformed(
  () => instantiate(`(func i32.const 0 if $$a else $$l end) `),
  `mismatching label`,
);

// ./test/core/if.wast:1537
assert_malformed(
  () => instantiate(`(func i32.const 0 if else end $$l) `),
  `mismatching label`,
);

// ./test/core/if.wast:1541
assert_malformed(
  () => instantiate(`(func i32.const 0 if else $$l end $$l) `),
  `mismatching label`,
);

// ./test/core/if.wast:1545
assert_malformed(
  () => instantiate(`(func i32.const 0 if else $$l1 end $$l2) `),
  `mismatching label`,
);

// ./test/core/if.wast:1549
assert_malformed(
  () => instantiate(`(func i32.const 0 if $$a else end $$l) `),
  `mismatching label`,
);

// ./test/core/if.wast:1553
assert_malformed(
  () => instantiate(`(func i32.const 0 if $$a else $$a end $$l) `),
  `mismatching label`,
);

// ./test/core/if.wast:1557
assert_malformed(
  () => instantiate(`(func i32.const 0 if $$a else $$l end $$l) `),
  `mismatching label`,
);

// ./test/core/if.wast:1561
assert_malformed(
  () => instantiate(`(func (if i32.const 0 (then) (else))) `),
  `unexpected token`,
);
