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

// ./test/core/multi-memory/load2.wast

// ./test/core/multi-memory/load2.wast:1
let $0 = instantiate(`(module
  (memory 0)
  (memory 0)
  (memory 0)
  (memory $$m 1)

  (func (export "as-br-value") (result i32)
    (block (result i32) (br 0 (i32.load $$m (i32.const 0))))
  )

  (func (export "as-br_if-cond")
    (block (br_if 0 (i32.load $$m (i32.const 0))))
  )
  (func (export "as-br_if-value") (result i32)
    (block (result i32)
      (drop (br_if 0 (i32.load $$m (i32.const 0)) (i32.const 1))) (i32.const 7)
    )
  )
  (func (export "as-br_if-value-cond") (result i32)
    (block (result i32)
      (drop (br_if 0 (i32.const 6) (i32.load $$m (i32.const 0)))) (i32.const 7)
    )
  )

  (func (export "as-br_table-index")
    (block (br_table 0 0 0 (i32.load $$m (i32.const 0))))
  )
  (func (export "as-br_table-value") (result i32)
    (block (result i32)
      (br_table 0 0 0 (i32.load $$m (i32.const 0)) (i32.const 1)) (i32.const 7)
    )
  )
  (func (export "as-br_table-value-index") (result i32)
    (block (result i32)
      (br_table 0 0 (i32.const 6) (i32.load $$m (i32.const 0))) (i32.const 7)
    )
  )

  (func (export "as-return-value") (result i32)
    (return (i32.load $$m (i32.const 0)))
  )

  (func (export "as-if-cond") (result i32)
    (if (result i32) (i32.load $$m (i32.const 0))
      (then (i32.const 0)) (else (i32.const 1))
    )
  )
  (func (export "as-if-then") (result i32)
    (if (result i32) (i32.const 1)
      (then (i32.load $$m (i32.const 0))) (else (i32.const 0))
    )
  )
  (func (export "as-if-else") (result i32)
    (if (result i32) (i32.const 0)
      (then (i32.const 0)) (else (i32.load $$m (i32.const 0)))
    )
  )

  (func (export "as-select-first") (param i32 i32) (result i32)
    (select (i32.load $$m (i32.const 0)) (local.get 0) (local.get 1))
  )
  (func (export "as-select-second") (param i32 i32) (result i32)
    (select (local.get 0) (i32.load $$m (i32.const 0)) (local.get 1))
  )
  (func (export "as-select-cond") (result i32)
    (select (i32.const 0) (i32.const 1) (i32.load $$m (i32.const 0)))
  )

  (func $$f (param i32 i32 i32) (result i32) (i32.const -1))
  (func (export "as-call-first") (result i32)
    (call $$f (i32.load $$m (i32.const 0)) (i32.const 2) (i32.const 3))
  )
  (func (export "as-call-mid") (result i32)
    (call $$f (i32.const 1) (i32.load $$m (i32.const 0)) (i32.const 3))
  )
  (func (export "as-call-last") (result i32)
    (call $$f (i32.const 1) (i32.const 2) (i32.load $$m (i32.const 0)))
  )

  (type $$sig (func (param i32 i32 i32) (result i32)))
  (table funcref (elem $$f))
  (func (export "as-call_indirect-first") (result i32)
    (call_indirect (type $$sig)
      (i32.load $$m (i32.const 0)) (i32.const 2) (i32.const 3) (i32.const 0)
    )
  )
  (func (export "as-call_indirect-mid") (result i32)
    (call_indirect (type $$sig)
      (i32.const 1) (i32.load $$m (i32.const 0)) (i32.const 3) (i32.const 0)
    )
  )
  (func (export "as-call_indirect-last") (result i32)
    (call_indirect (type $$sig)
      (i32.const 1) (i32.const 2) (i32.load $$m (i32.const 0)) (i32.const 0)
    )
  )
  (func (export "as-call_indirect-index") (result i32)
    (call_indirect (type $$sig)
      (i32.const 1) (i32.const 2) (i32.const 3) (i32.load $$m (i32.const 0))
    )
  )

  (func (export "as-local.set-value") (local i32)
    (local.set 0 (i32.load $$m (i32.const 0)))
  )
  (func (export "as-local.tee-value") (result i32) (local i32)
    (local.tee 0 (i32.load $$m (i32.const 0)))
  )
  (global $$g (mut i32) (i32.const 0))
  (func (export "as-global.set-value") (local i32)
    (global.set $$g (i32.load $$m (i32.const 0)))
  )

  (func (export "as-load-address") (result i32)
    (i32.load $$m (i32.load $$m (i32.const 0)))
  )
  (func (export "as-loadN-address") (result i32)
    (i32.load8_s $$m (i32.load $$m (i32.const 0)))
  )

  (func (export "as-store-address")
    (i32.store $$m (i32.load $$m (i32.const 0)) (i32.const 7))
  )
  (func (export "as-store-value")
    (i32.store $$m (i32.const 2) (i32.load $$m (i32.const 0)))
  )

  (func (export "as-storeN-address")
    (i32.store8 $$m (i32.load8_s $$m (i32.const 0)) (i32.const 7))
  )
  (func (export "as-storeN-value")
    (i32.store16 $$m (i32.const 2) (i32.load $$m (i32.const 0)))
  )

  (func (export "as-unary-operand") (result i32)
    (i32.clz (i32.load $$m (i32.const 100)))
  )

  (func (export "as-binary-left") (result i32)
    (i32.add (i32.load $$m (i32.const 100)) (i32.const 10))
  )
  (func (export "as-binary-right") (result i32)
    (i32.sub (i32.const 10) (i32.load $$m (i32.const 100)))
  )

  (func (export "as-test-operand") (result i32)
    (i32.eqz (i32.load $$m (i32.const 100)))
  )

  (func (export "as-compare-left") (result i32)
    (i32.le_s (i32.load $$m (i32.const 100)) (i32.const 10))
  )
  (func (export "as-compare-right") (result i32)
    (i32.ne (i32.const 10) (i32.load $$m (i32.const 100)))
  )

  (func (export "as-memory.grow-size") (result i32)
    (memory.grow $$m (i32.load $$m (i32.const 100)))
  )
)`);

// ./test/core/multi-memory/load2.wast:162
assert_return(() => invoke($0, `as-br-value`, []), [value("i32", 0)]);

// ./test/core/multi-memory/load2.wast:164
assert_return(() => invoke($0, `as-br_if-cond`, []), []);

// ./test/core/multi-memory/load2.wast:165
assert_return(() => invoke($0, `as-br_if-value`, []), [value("i32", 0)]);

// ./test/core/multi-memory/load2.wast:166
assert_return(() => invoke($0, `as-br_if-value-cond`, []), [value("i32", 7)]);

// ./test/core/multi-memory/load2.wast:168
assert_return(() => invoke($0, `as-br_table-index`, []), []);

// ./test/core/multi-memory/load2.wast:169
assert_return(() => invoke($0, `as-br_table-value`, []), [value("i32", 0)]);

// ./test/core/multi-memory/load2.wast:170
assert_return(() => invoke($0, `as-br_table-value-index`, []), [value("i32", 6)]);

// ./test/core/multi-memory/load2.wast:172
assert_return(() => invoke($0, `as-return-value`, []), [value("i32", 0)]);

// ./test/core/multi-memory/load2.wast:174
assert_return(() => invoke($0, `as-if-cond`, []), [value("i32", 1)]);

// ./test/core/multi-memory/load2.wast:175
assert_return(() => invoke($0, `as-if-then`, []), [value("i32", 0)]);

// ./test/core/multi-memory/load2.wast:176
assert_return(() => invoke($0, `as-if-else`, []), [value("i32", 0)]);

// ./test/core/multi-memory/load2.wast:178
assert_return(() => invoke($0, `as-select-first`, [0, 1]), [value("i32", 0)]);

// ./test/core/multi-memory/load2.wast:179
assert_return(() => invoke($0, `as-select-second`, [0, 0]), [value("i32", 0)]);

// ./test/core/multi-memory/load2.wast:180
assert_return(() => invoke($0, `as-select-cond`, []), [value("i32", 1)]);

// ./test/core/multi-memory/load2.wast:182
assert_return(() => invoke($0, `as-call-first`, []), [value("i32", -1)]);

// ./test/core/multi-memory/load2.wast:183
assert_return(() => invoke($0, `as-call-mid`, []), [value("i32", -1)]);

// ./test/core/multi-memory/load2.wast:184
assert_return(() => invoke($0, `as-call-last`, []), [value("i32", -1)]);

// ./test/core/multi-memory/load2.wast:186
assert_return(() => invoke($0, `as-call_indirect-first`, []), [value("i32", -1)]);

// ./test/core/multi-memory/load2.wast:187
assert_return(() => invoke($0, `as-call_indirect-mid`, []), [value("i32", -1)]);

// ./test/core/multi-memory/load2.wast:188
assert_return(() => invoke($0, `as-call_indirect-last`, []), [value("i32", -1)]);

// ./test/core/multi-memory/load2.wast:189
assert_return(() => invoke($0, `as-call_indirect-index`, []), [value("i32", -1)]);

// ./test/core/multi-memory/load2.wast:191
assert_return(() => invoke($0, `as-local.set-value`, []), []);

// ./test/core/multi-memory/load2.wast:192
assert_return(() => invoke($0, `as-local.tee-value`, []), [value("i32", 0)]);

// ./test/core/multi-memory/load2.wast:193
assert_return(() => invoke($0, `as-global.set-value`, []), []);

// ./test/core/multi-memory/load2.wast:195
assert_return(() => invoke($0, `as-load-address`, []), [value("i32", 0)]);

// ./test/core/multi-memory/load2.wast:196
assert_return(() => invoke($0, `as-loadN-address`, []), [value("i32", 0)]);

// ./test/core/multi-memory/load2.wast:197
assert_return(() => invoke($0, `as-store-address`, []), []);

// ./test/core/multi-memory/load2.wast:198
assert_return(() => invoke($0, `as-store-value`, []), []);

// ./test/core/multi-memory/load2.wast:199
assert_return(() => invoke($0, `as-storeN-address`, []), []);

// ./test/core/multi-memory/load2.wast:200
assert_return(() => invoke($0, `as-storeN-value`, []), []);

// ./test/core/multi-memory/load2.wast:202
assert_return(() => invoke($0, `as-unary-operand`, []), [value("i32", 32)]);

// ./test/core/multi-memory/load2.wast:204
assert_return(() => invoke($0, `as-binary-left`, []), [value("i32", 10)]);

// ./test/core/multi-memory/load2.wast:205
assert_return(() => invoke($0, `as-binary-right`, []), [value("i32", 10)]);

// ./test/core/multi-memory/load2.wast:207
assert_return(() => invoke($0, `as-test-operand`, []), [value("i32", 1)]);

// ./test/core/multi-memory/load2.wast:209
assert_return(() => invoke($0, `as-compare-left`, []), [value("i32", 1)]);

// ./test/core/multi-memory/load2.wast:210
assert_return(() => invoke($0, `as-compare-right`, []), [value("i32", 1)]);

// ./test/core/multi-memory/load2.wast:212
assert_return(() => invoke($0, `as-memory.grow-size`, []), [value("i32", 1)]);
