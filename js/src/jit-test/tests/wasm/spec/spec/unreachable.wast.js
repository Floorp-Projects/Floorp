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

// ./test/core/unreachable.wast

// ./test/core/unreachable.wast:3
let $0 = instantiate(`(module
  ;; Auxiliary definitions
  (func $$dummy)
  (func $$dummy3 (param i32 i32 i32))

  (func (export "type-i32") (result i32) (unreachable))
  (func (export "type-i64") (result i32) (unreachable))
  (func (export "type-f32") (result f64) (unreachable))
  (func (export "type-f64") (result f64) (unreachable))

  (func (export "as-func-first") (result i32)
    (unreachable) (i32.const -1)
  )
  (func (export "as-func-mid") (result i32)
    (call $$dummy) (unreachable) (i32.const -1)
  )
  (func (export "as-func-last")
    (call $$dummy) (unreachable)
  )
  (func (export "as-func-value") (result i32)
    (call $$dummy) (unreachable)
  )

  (func (export "as-block-first") (result i32)
    (block (result i32) (unreachable) (i32.const 2))
  )
  (func (export "as-block-mid") (result i32)
    (block (result i32) (call $$dummy) (unreachable) (i32.const 2))
  )
  (func (export "as-block-last")
    (block (nop) (call $$dummy) (unreachable))
  )
  (func (export "as-block-value") (result i32)
    (block (result i32) (nop) (call $$dummy) (unreachable))
  )
  (func (export "as-block-broke") (result i32)
    (block (result i32) (call $$dummy) (br 0 (i32.const 1)) (unreachable))
  )

  (func (export "as-loop-first") (result i32)
    (loop (result i32) (unreachable) (i32.const 2))
  )
  (func (export "as-loop-mid") (result i32)
    (loop (result i32) (call $$dummy) (unreachable) (i32.const 2))
  )
  (func (export "as-loop-last")
    (loop (nop) (call $$dummy) (unreachable))
  )
  (func (export "as-loop-broke") (result i32)
    (block (result i32)
      (loop (result i32) (call $$dummy) (br 1 (i32.const 1)) (unreachable))
    )
  )

  (func (export "as-br-value") (result i32)
    (block (result i32) (br 0 (unreachable)))
  )

  (func (export "as-br_if-cond")
    (block (br_if 0 (unreachable)))
  )
  (func (export "as-br_if-value") (result i32)
    (block (result i32)
      (drop (br_if 0 (unreachable) (i32.const 1))) (i32.const 7)
    )
  )
  (func (export "as-br_if-value-cond") (result i32)
    (block (result i32)
      (drop (br_if 0 (i32.const 6) (unreachable))) (i32.const 7)
    )
  )

  (func (export "as-br_table-index")
    (block (br_table 0 0 0 (unreachable)))
  )
  (func (export "as-br_table-value") (result i32)
    (block (result i32)
      (br_table 0 0 0 (unreachable) (i32.const 1)) (i32.const 7)
    )
  )
  (func (export "as-br_table-value-2") (result i32)
    (block (result i32)
      (block (result i32) (br_table 0 1 (unreachable) (i32.const 1)))
    )
  )
  (func (export "as-br_table-value-index") (result i32)
    (block (result i32)
      (br_table 0 0 (i32.const 6) (unreachable)) (i32.const 7)
    )
  )
  (func (export "as-br_table-value-and-index") (result i32)
    (block (result i32) (br_table 0 0 (unreachable)) (i32.const 8))
  )

  (func (export "as-return-value") (result i64)
    (return (unreachable))
  )

  (func (export "as-if-cond") (result i32)
    (if (result i32) (unreachable) (then (i32.const 0)) (else (i32.const 1)))
  )
  (func (export "as-if-then") (param i32 i32) (result i32)
    (if (result i32) (local.get 0) (then (unreachable)) (else (local.get 1)))
  )
  (func (export "as-if-else") (param i32 i32) (result i32)
    (if (result i32) (local.get 0) (then (local.get 1)) (else (unreachable)))
  )
  (func (export "as-if-then-no-else") (param i32 i32) (result i32)
    (if (local.get 0) (then (unreachable))) (local.get 1)
  )

  (func (export "as-select-first") (param i32 i32) (result i32)
    (select (unreachable) (local.get 0) (local.get 1))
  )
  (func (export "as-select-second") (param i32 i32) (result i32)
    (select (local.get 0) (unreachable) (local.get 1))
  )
  (func (export "as-select-cond") (result i32)
    (select (i32.const 0) (i32.const 1) (unreachable))
  )

  (func (export "as-call-first")
    (call $$dummy3 (unreachable) (i32.const 2) (i32.const 3))
  )
  (func (export "as-call-mid")
    (call $$dummy3 (i32.const 1) (unreachable) (i32.const 3))
  )
  (func (export "as-call-last")
    (call $$dummy3 (i32.const 1) (i32.const 2) (unreachable))
  )

  (type $$sig (func (param i32 i32 i32)))
  (table funcref (elem $$dummy3))
  (func (export "as-call_indirect-func")
    (call_indirect (type $$sig)
      (unreachable) (i32.const 1) (i32.const 2) (i32.const 3)
    )
  )
  (func (export "as-call_indirect-first")
    (call_indirect (type $$sig)
      (i32.const 0) (unreachable) (i32.const 2) (i32.const 3)
    )
  )
  (func (export "as-call_indirect-mid")
    (call_indirect (type $$sig)
      (i32.const 0) (i32.const 1) (unreachable) (i32.const 3)
    )
  )
  (func (export "as-call_indirect-last")
    (call_indirect (type $$sig)
      (i32.const 0) (i32.const 1) (i32.const 2) (unreachable)
    )
  )

  (func (export "as-local.set-value") (local f32)
    (local.set 0 (unreachable))
  )
  (func (export "as-local.tee-value") (result f32) (local f32)
    (local.tee 0 (unreachable))
  )
  (global $$a (mut f32) (f32.const 0))
  (func (export "as-global.set-value") (result f32)
    (global.set $$a (unreachable))
  )

  (memory 1)
  (func (export "as-load-address") (result f32)
    (f32.load (unreachable))
  )
  (func (export "as-loadN-address") (result i64)
    (i64.load8_s (unreachable))
  )

  (func (export "as-store-address")
    (f64.store (unreachable) (f64.const 7))
  )
  (func (export "as-store-value")
    (i64.store (i32.const 2) (unreachable))
  )

  (func (export "as-storeN-address")
    (i32.store8 (unreachable) (i32.const 7))
  )
  (func (export "as-storeN-value")
    (i64.store16 (i32.const 2) (unreachable))
  )

  (func (export "as-unary-operand") (result f32)
    (f32.neg (unreachable))
  )

  (func (export "as-binary-left") (result i32)
    (i32.add (unreachable) (i32.const 10))
  )
  (func (export "as-binary-right") (result i64)
    (i64.sub (i64.const 10) (unreachable))
  )

  (func (export "as-test-operand") (result i32)
    (i32.eqz (unreachable))
  )

  (func (export "as-compare-left") (result i32)
    (f64.le (unreachable) (f64.const 10))
  )
  (func (export "as-compare-right") (result i32)
    (f32.ne (f32.const 10) (unreachable))
  )

  (func (export "as-convert-operand") (result i32)
    (i32.wrap_i64 (unreachable))
  )

  (func (export "as-memory.grow-size") (result i32)
    (memory.grow (unreachable))
  )
)`);

// ./test/core/unreachable.wast:221
assert_trap(() => invoke($0, `type-i32`, []), `unreachable`);

// ./test/core/unreachable.wast:222
assert_trap(() => invoke($0, `type-i64`, []), `unreachable`);

// ./test/core/unreachable.wast:223
assert_trap(() => invoke($0, `type-f32`, []), `unreachable`);

// ./test/core/unreachable.wast:224
assert_trap(() => invoke($0, `type-f64`, []), `unreachable`);

// ./test/core/unreachable.wast:226
assert_trap(() => invoke($0, `as-func-first`, []), `unreachable`);

// ./test/core/unreachable.wast:227
assert_trap(() => invoke($0, `as-func-mid`, []), `unreachable`);

// ./test/core/unreachable.wast:228
assert_trap(() => invoke($0, `as-func-last`, []), `unreachable`);

// ./test/core/unreachable.wast:229
assert_trap(() => invoke($0, `as-func-value`, []), `unreachable`);

// ./test/core/unreachable.wast:231
assert_trap(() => invoke($0, `as-block-first`, []), `unreachable`);

// ./test/core/unreachable.wast:232
assert_trap(() => invoke($0, `as-block-mid`, []), `unreachable`);

// ./test/core/unreachable.wast:233
assert_trap(() => invoke($0, `as-block-last`, []), `unreachable`);

// ./test/core/unreachable.wast:234
assert_trap(() => invoke($0, `as-block-value`, []), `unreachable`);

// ./test/core/unreachable.wast:235
assert_return(() => invoke($0, `as-block-broke`, []), [value("i32", 1)]);

// ./test/core/unreachable.wast:237
assert_trap(() => invoke($0, `as-loop-first`, []), `unreachable`);

// ./test/core/unreachable.wast:238
assert_trap(() => invoke($0, `as-loop-mid`, []), `unreachable`);

// ./test/core/unreachable.wast:239
assert_trap(() => invoke($0, `as-loop-last`, []), `unreachable`);

// ./test/core/unreachable.wast:240
assert_return(() => invoke($0, `as-loop-broke`, []), [value("i32", 1)]);

// ./test/core/unreachable.wast:242
assert_trap(() => invoke($0, `as-br-value`, []), `unreachable`);

// ./test/core/unreachable.wast:244
assert_trap(() => invoke($0, `as-br_if-cond`, []), `unreachable`);

// ./test/core/unreachable.wast:245
assert_trap(() => invoke($0, `as-br_if-value`, []), `unreachable`);

// ./test/core/unreachable.wast:246
assert_trap(() => invoke($0, `as-br_if-value-cond`, []), `unreachable`);

// ./test/core/unreachable.wast:248
assert_trap(() => invoke($0, `as-br_table-index`, []), `unreachable`);

// ./test/core/unreachable.wast:249
assert_trap(() => invoke($0, `as-br_table-value`, []), `unreachable`);

// ./test/core/unreachable.wast:250
assert_trap(() => invoke($0, `as-br_table-value-2`, []), `unreachable`);

// ./test/core/unreachable.wast:251
assert_trap(() => invoke($0, `as-br_table-value-index`, []), `unreachable`);

// ./test/core/unreachable.wast:252
assert_trap(() => invoke($0, `as-br_table-value-and-index`, []), `unreachable`);

// ./test/core/unreachable.wast:254
assert_trap(() => invoke($0, `as-return-value`, []), `unreachable`);

// ./test/core/unreachable.wast:256
assert_trap(() => invoke($0, `as-if-cond`, []), `unreachable`);

// ./test/core/unreachable.wast:257
assert_trap(() => invoke($0, `as-if-then`, [1, 6]), `unreachable`);

// ./test/core/unreachable.wast:258
assert_return(() => invoke($0, `as-if-then`, [0, 6]), [value("i32", 6)]);

// ./test/core/unreachable.wast:259
assert_trap(() => invoke($0, `as-if-else`, [0, 6]), `unreachable`);

// ./test/core/unreachable.wast:260
assert_return(() => invoke($0, `as-if-else`, [1, 6]), [value("i32", 6)]);

// ./test/core/unreachable.wast:261
assert_trap(() => invoke($0, `as-if-then-no-else`, [1, 6]), `unreachable`);

// ./test/core/unreachable.wast:262
assert_return(() => invoke($0, `as-if-then-no-else`, [0, 6]), [value("i32", 6)]);

// ./test/core/unreachable.wast:264
assert_trap(() => invoke($0, `as-select-first`, [0, 6]), `unreachable`);

// ./test/core/unreachable.wast:265
assert_trap(() => invoke($0, `as-select-first`, [1, 6]), `unreachable`);

// ./test/core/unreachable.wast:266
assert_trap(() => invoke($0, `as-select-second`, [0, 6]), `unreachable`);

// ./test/core/unreachable.wast:267
assert_trap(() => invoke($0, `as-select-second`, [1, 6]), `unreachable`);

// ./test/core/unreachable.wast:268
assert_trap(() => invoke($0, `as-select-cond`, []), `unreachable`);

// ./test/core/unreachable.wast:270
assert_trap(() => invoke($0, `as-call-first`, []), `unreachable`);

// ./test/core/unreachable.wast:271
assert_trap(() => invoke($0, `as-call-mid`, []), `unreachable`);

// ./test/core/unreachable.wast:272
assert_trap(() => invoke($0, `as-call-last`, []), `unreachable`);

// ./test/core/unreachable.wast:274
assert_trap(() => invoke($0, `as-call_indirect-func`, []), `unreachable`);

// ./test/core/unreachable.wast:275
assert_trap(() => invoke($0, `as-call_indirect-first`, []), `unreachable`);

// ./test/core/unreachable.wast:276
assert_trap(() => invoke($0, `as-call_indirect-mid`, []), `unreachable`);

// ./test/core/unreachable.wast:277
assert_trap(() => invoke($0, `as-call_indirect-last`, []), `unreachable`);

// ./test/core/unreachable.wast:279
assert_trap(() => invoke($0, `as-local.set-value`, []), `unreachable`);

// ./test/core/unreachable.wast:280
assert_trap(() => invoke($0, `as-local.tee-value`, []), `unreachable`);

// ./test/core/unreachable.wast:281
assert_trap(() => invoke($0, `as-global.set-value`, []), `unreachable`);

// ./test/core/unreachable.wast:283
assert_trap(() => invoke($0, `as-load-address`, []), `unreachable`);

// ./test/core/unreachable.wast:284
assert_trap(() => invoke($0, `as-loadN-address`, []), `unreachable`);

// ./test/core/unreachable.wast:286
assert_trap(() => invoke($0, `as-store-address`, []), `unreachable`);

// ./test/core/unreachable.wast:287
assert_trap(() => invoke($0, `as-store-value`, []), `unreachable`);

// ./test/core/unreachable.wast:288
assert_trap(() => invoke($0, `as-storeN-address`, []), `unreachable`);

// ./test/core/unreachable.wast:289
assert_trap(() => invoke($0, `as-storeN-value`, []), `unreachable`);

// ./test/core/unreachable.wast:291
assert_trap(() => invoke($0, `as-unary-operand`, []), `unreachable`);

// ./test/core/unreachable.wast:293
assert_trap(() => invoke($0, `as-binary-left`, []), `unreachable`);

// ./test/core/unreachable.wast:294
assert_trap(() => invoke($0, `as-binary-right`, []), `unreachable`);

// ./test/core/unreachable.wast:296
assert_trap(() => invoke($0, `as-test-operand`, []), `unreachable`);

// ./test/core/unreachable.wast:298
assert_trap(() => invoke($0, `as-compare-left`, []), `unreachable`);

// ./test/core/unreachable.wast:299
assert_trap(() => invoke($0, `as-compare-right`, []), `unreachable`);

// ./test/core/unreachable.wast:301
assert_trap(() => invoke($0, `as-convert-operand`, []), `unreachable`);

// ./test/core/unreachable.wast:303
assert_trap(() => invoke($0, `as-memory.grow-size`, []), `unreachable`);
