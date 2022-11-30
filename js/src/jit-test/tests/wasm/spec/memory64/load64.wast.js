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

// ./test/core/load64.wast

// ./test/core/load64.wast:3
let $0 = instantiate(`(module
  (memory i64 1)

  (func (export "as-br-value") (result i32)
    (block (result i32) (br 0 (i32.load (i64.const 0))))
  )

  (func (export "as-br_if-cond")
    (block (br_if 0 (i32.load (i64.const 0))))
  )
  (func (export "as-br_if-value") (result i32)
    (block (result i32)
      (drop (br_if 0 (i32.load (i64.const 0)) (i32.const 1))) (i32.const 7)
    )
  )
  (func (export "as-br_if-value-cond") (result i32)
    (block (result i32)
      (drop (br_if 0 (i32.const 6) (i32.load (i64.const 0)))) (i32.const 7)
    )
  )

  (func (export "as-br_table-index")
    (block (br_table 0 0 0 (i32.load (i64.const 0))))
  )
  (func (export "as-br_table-value") (result i32)
    (block (result i32)
      (br_table 0 0 0 (i32.load (i64.const 0)) (i32.const 1)) (i32.const 7)
    )
  )
  (func (export "as-br_table-value-index") (result i32)
    (block (result i32)
      (br_table 0 0 (i32.const 6) (i32.load (i64.const 0))) (i32.const 7)
    )
  )

  (func (export "as-return-value") (result i32)
    (return (i32.load (i64.const 0)))
  )

  (func (export "as-if-cond") (result i32)
    (if (result i32) (i32.load (i64.const 0))
      (then (i32.const 0)) (else (i32.const 1))
    )
  )
  (func (export "as-if-then") (result i32)
    (if (result i32) (i32.const 1)
      (then (i32.load (i64.const 0))) (else (i32.const 0))
    )
  )
  (func (export "as-if-else") (result i32)
    (if (result i32) (i32.const 0)
      (then (i32.const 0)) (else (i32.load (i64.const 0)))
    )
  )

  (func (export "as-select-first") (param i32 i32) (result i32)
    (select (i32.load (i64.const 0)) (local.get 0) (local.get 1))
  )
  (func (export "as-select-second") (param i32 i32) (result i32)
    (select (local.get 0) (i32.load (i64.const 0)) (local.get 1))
  )
  (func (export "as-select-cond") (result i32)
    (select (i32.const 0) (i32.const 1) (i32.load (i64.const 0)))
  )

  (func $$f (param i32 i32 i32) (result i32) (i32.const -1))
  (func (export "as-call-first") (result i32)
    (call $$f (i32.load (i64.const 0)) (i32.const 2) (i32.const 3))
  )
  (func (export "as-call-mid") (result i32)
    (call $$f (i32.const 1) (i32.load (i64.const 0)) (i32.const 3))
  )
  (func (export "as-call-last") (result i32)
    (call $$f (i32.const 1) (i32.const 2) (i32.load (i64.const 0)))
  )

  (type $$sig (func (param i32 i32 i32) (result i32)))
  (table funcref (elem $$f))
  (func (export "as-call_indirect-first") (result i32)
    (call_indirect (type $$sig)
      (i32.load (i64.const 0)) (i32.const 2) (i32.const 3) (i32.const 0)
    )
  )
  (func (export "as-call_indirect-mid") (result i32)
    (call_indirect (type $$sig)
      (i32.const 1) (i32.load (i64.const 0)) (i32.const 3) (i32.const 0)
    )
  )
  (func (export "as-call_indirect-last") (result i32)
    (call_indirect (type $$sig)
      (i32.const 1) (i32.const 2) (i32.load (i64.const 0)) (i32.const 0)
    )
  )
  (func (export "as-call_indirect-index") (result i32)
    (call_indirect (type $$sig)
      (i32.const 1) (i32.const 2) (i32.const 3) (i32.load (i64.const 0))
    )
  )

  (func (export "as-local.set-value") (local i32)
    (local.set 0 (i32.load (i64.const 0)))
  )
  (func (export "as-local.tee-value") (result i32) (local i32)
    (local.tee 0 (i32.load (i64.const 0)))
  )
  (global $$g (mut i32) (i32.const 0))
  (func (export "as-global.set-value") (local i32)
    (global.set $$g (i32.load (i64.const 0)))
  )

  (func (export "as-load-address") (result i32)
    (i32.load (i64.load (i64.const 0)))
  )
  (func (export "as-loadN-address") (result i32)
    (i32.load8_s (i64.load (i64.const 0)))
  )

  (func (export "as-store-address")
    (i32.store (i64.load (i64.const 0)) (i32.const 7))
  )
  (func (export "as-store-value")
    (i32.store (i64.const 2) (i32.load (i64.const 0)))
  )

  (func (export "as-storeN-address")
    (i32.store8 (i64.load8_s (i64.const 0)) (i32.const 7))
  )
  (func (export "as-storeN-value")
    (i32.store16 (i64.const 2) (i32.load (i64.const 0)))
  )

  (func (export "as-unary-operand") (result i32)
    (i32.clz (i32.load (i64.const 100)))
  )

  (func (export "as-binary-left") (result i32)
    (i32.add (i32.load (i64.const 100)) (i32.const 10))
  )
  (func (export "as-binary-right") (result i32)
    (i32.sub (i32.const 10) (i32.load (i64.const 100)))
  )

  (func (export "as-test-operand") (result i32)
    (i32.eqz (i32.load (i64.const 100)))
  )

  (func (export "as-compare-left") (result i32)
    (i32.le_s (i32.load (i64.const 100)) (i32.const 10))
  )
  (func (export "as-compare-right") (result i32)
    (i32.ne (i32.const 10) (i32.load (i64.const 100)))
  )

  (func (export "as-memory.grow-size") (result i64)
    (memory.grow (i64.load (i64.const 100)))
  )
)`);

// ./test/core/load64.wast:161
assert_return(() => invoke($0, `as-br-value`, []), [value("i32", 0)]);

// ./test/core/load64.wast:163
assert_return(() => invoke($0, `as-br_if-cond`, []), []);

// ./test/core/load64.wast:164
assert_return(() => invoke($0, `as-br_if-value`, []), [value("i32", 0)]);

// ./test/core/load64.wast:165
assert_return(() => invoke($0, `as-br_if-value-cond`, []), [value("i32", 7)]);

// ./test/core/load64.wast:167
assert_return(() => invoke($0, `as-br_table-index`, []), []);

// ./test/core/load64.wast:168
assert_return(() => invoke($0, `as-br_table-value`, []), [value("i32", 0)]);

// ./test/core/load64.wast:169
assert_return(() => invoke($0, `as-br_table-value-index`, []), [value("i32", 6)]);

// ./test/core/load64.wast:171
assert_return(() => invoke($0, `as-return-value`, []), [value("i32", 0)]);

// ./test/core/load64.wast:173
assert_return(() => invoke($0, `as-if-cond`, []), [value("i32", 1)]);

// ./test/core/load64.wast:174
assert_return(() => invoke($0, `as-if-then`, []), [value("i32", 0)]);

// ./test/core/load64.wast:175
assert_return(() => invoke($0, `as-if-else`, []), [value("i32", 0)]);

// ./test/core/load64.wast:177
assert_return(() => invoke($0, `as-select-first`, [0, 1]), [value("i32", 0)]);

// ./test/core/load64.wast:178
assert_return(() => invoke($0, `as-select-second`, [0, 0]), [value("i32", 0)]);

// ./test/core/load64.wast:179
assert_return(() => invoke($0, `as-select-cond`, []), [value("i32", 1)]);

// ./test/core/load64.wast:181
assert_return(() => invoke($0, `as-call-first`, []), [value("i32", -1)]);

// ./test/core/load64.wast:182
assert_return(() => invoke($0, `as-call-mid`, []), [value("i32", -1)]);

// ./test/core/load64.wast:183
assert_return(() => invoke($0, `as-call-last`, []), [value("i32", -1)]);

// ./test/core/load64.wast:185
assert_return(() => invoke($0, `as-call_indirect-first`, []), [value("i32", -1)]);

// ./test/core/load64.wast:186
assert_return(() => invoke($0, `as-call_indirect-mid`, []), [value("i32", -1)]);

// ./test/core/load64.wast:187
assert_return(() => invoke($0, `as-call_indirect-last`, []), [value("i32", -1)]);

// ./test/core/load64.wast:188
assert_return(() => invoke($0, `as-call_indirect-index`, []), [value("i32", -1)]);

// ./test/core/load64.wast:190
assert_return(() => invoke($0, `as-local.set-value`, []), []);

// ./test/core/load64.wast:191
assert_return(() => invoke($0, `as-local.tee-value`, []), [value("i32", 0)]);

// ./test/core/load64.wast:192
assert_return(() => invoke($0, `as-global.set-value`, []), []);

// ./test/core/load64.wast:194
assert_return(() => invoke($0, `as-load-address`, []), [value("i32", 0)]);

// ./test/core/load64.wast:195
assert_return(() => invoke($0, `as-loadN-address`, []), [value("i32", 0)]);

// ./test/core/load64.wast:196
assert_return(() => invoke($0, `as-store-address`, []), []);

// ./test/core/load64.wast:197
assert_return(() => invoke($0, `as-store-value`, []), []);

// ./test/core/load64.wast:198
assert_return(() => invoke($0, `as-storeN-address`, []), []);

// ./test/core/load64.wast:199
assert_return(() => invoke($0, `as-storeN-value`, []), []);

// ./test/core/load64.wast:201
assert_return(() => invoke($0, `as-unary-operand`, []), [value("i32", 32)]);

// ./test/core/load64.wast:203
assert_return(() => invoke($0, `as-binary-left`, []), [value("i32", 10)]);

// ./test/core/load64.wast:204
assert_return(() => invoke($0, `as-binary-right`, []), [value("i32", 10)]);

// ./test/core/load64.wast:206
assert_return(() => invoke($0, `as-test-operand`, []), [value("i32", 1)]);

// ./test/core/load64.wast:208
assert_return(() => invoke($0, `as-compare-left`, []), [value("i32", 1)]);

// ./test/core/load64.wast:209
assert_return(() => invoke($0, `as-compare-right`, []), [value("i32", 1)]);

// ./test/core/load64.wast:211
assert_return(() => invoke($0, `as-memory.grow-size`, []), [value("i64", 1n)]);

// ./test/core/load64.wast:213
assert_malformed(
  () => instantiate(`(memory i64 1) (func (param i64) (result i32) (i32.load32 (local.get 0))) `),
  `unknown operator`,
);

// ./test/core/load64.wast:220
assert_malformed(
  () => instantiate(`(memory i64 1) (func (param i64) (result i32) (i32.load32_u (local.get 0))) `),
  `unknown operator`,
);

// ./test/core/load64.wast:227
assert_malformed(
  () => instantiate(`(memory i64 1) (func (param i64) (result i32) (i32.load32_s (local.get 0))) `),
  `unknown operator`,
);

// ./test/core/load64.wast:234
assert_malformed(
  () => instantiate(`(memory i64 1) (func (param i64) (result i32) (i32.load64 (local.get 0))) `),
  `unknown operator`,
);

// ./test/core/load64.wast:241
assert_malformed(
  () => instantiate(`(memory i64 1) (func (param i64) (result i32) (i32.load64_u (local.get 0))) `),
  `unknown operator`,
);

// ./test/core/load64.wast:248
assert_malformed(
  () => instantiate(`(memory i64 1) (func (param i64) (result i32) (i32.load64_s (local.get 0))) `),
  `unknown operator`,
);

// ./test/core/load64.wast:256
assert_malformed(
  () => instantiate(`(memory i64 1) (func (param i64) (result i64) (i64.load64 (local.get 0))) `),
  `unknown operator`,
);

// ./test/core/load64.wast:263
assert_malformed(
  () => instantiate(`(memory i64 1) (func (param i64) (result i64) (i64.load64_u (local.get 0))) `),
  `unknown operator`,
);

// ./test/core/load64.wast:270
assert_malformed(
  () => instantiate(`(memory i64 1) (func (param i64) (result i64) (i64.load64_s (local.get 0))) `),
  `unknown operator`,
);

// ./test/core/load64.wast:278
assert_malformed(
  () => instantiate(`(memory i64 1) (func (param i64) (result f32) (f32.load32 (local.get 0))) `),
  `unknown operator`,
);

// ./test/core/load64.wast:285
assert_malformed(
  () => instantiate(`(memory i64 1) (func (param i64) (result f32) (f32.load64 (local.get 0))) `),
  `unknown operator`,
);

// ./test/core/load64.wast:293
assert_malformed(
  () => instantiate(`(memory i64 1) (func (param i64) (result f64) (f64.load32 (local.get 0))) `),
  `unknown operator`,
);

// ./test/core/load64.wast:300
assert_malformed(
  () => instantiate(`(memory i64 1) (func (param i64) (result f64) (f64.load64 (local.get 0))) `),
  `unknown operator`,
);

// ./test/core/load64.wast:311
assert_invalid(
  () => instantiate(`(module (memory i64 1) (func $$load_i32 (i32.load (i64.const 0))))`),
  `type mismatch`,
);

// ./test/core/load64.wast:315
assert_invalid(
  () => instantiate(`(module (memory i64 1) (func $$load8_s_i32 (i32.load8_s (i64.const 0))))`),
  `type mismatch`,
);

// ./test/core/load64.wast:319
assert_invalid(
  () => instantiate(`(module (memory i64 1) (func $$load8_u_i32 (i32.load8_u (i64.const 0))))`),
  `type mismatch`,
);

// ./test/core/load64.wast:323
assert_invalid(
  () => instantiate(`(module (memory i64 1) (func $$load16_s_i32 (i32.load16_s (i64.const 0))))`),
  `type mismatch`,
);

// ./test/core/load64.wast:327
assert_invalid(
  () => instantiate(`(module (memory i64 1) (func $$load16_u_i32 (i32.load16_u (i64.const 0))))`),
  `type mismatch`,
);

// ./test/core/load64.wast:331
assert_invalid(
  () => instantiate(`(module (memory i64 1) (func $$load_i64 (i64.load (i64.const 0))))`),
  `type mismatch`,
);

// ./test/core/load64.wast:335
assert_invalid(
  () => instantiate(`(module (memory i64 1) (func $$load8_s_i64 (i64.load8_s (i64.const 0))))`),
  `type mismatch`,
);

// ./test/core/load64.wast:339
assert_invalid(
  () => instantiate(`(module (memory i64 1) (func $$load8_u_i64 (i64.load8_u (i64.const 0))))`),
  `type mismatch`,
);

// ./test/core/load64.wast:343
assert_invalid(
  () => instantiate(`(module (memory i64 1) (func $$load16_s_i64 (i64.load16_s (i64.const 0))))`),
  `type mismatch`,
);

// ./test/core/load64.wast:347
assert_invalid(
  () => instantiate(`(module (memory i64 1) (func $$load16_u_i64 (i64.load16_u (i64.const 0))))`),
  `type mismatch`,
);

// ./test/core/load64.wast:351
assert_invalid(
  () => instantiate(`(module (memory i64 1) (func $$load32_s_i64 (i64.load32_s (i64.const 0))))`),
  `type mismatch`,
);

// ./test/core/load64.wast:355
assert_invalid(
  () => instantiate(`(module (memory i64 1) (func $$load32_u_i64 (i64.load32_u (i64.const 0))))`),
  `type mismatch`,
);

// ./test/core/load64.wast:359
assert_invalid(
  () => instantiate(`(module (memory i64 1) (func $$load_f32 (f32.load (i64.const 0))))`),
  `type mismatch`,
);

// ./test/core/load64.wast:363
assert_invalid(
  () => instantiate(`(module (memory i64 1) (func $$load_f64 (f64.load (i64.const 0))))`),
  `type mismatch`,
);

// ./test/core/load64.wast:371
assert_invalid(
  () => instantiate(`(module (memory i64 1) (func (result i32) (i32.load (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load64.wast:372
assert_invalid(
  () => instantiate(`(module (memory i64 1) (func (result i32) (i32.load8_s (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load64.wast:373
assert_invalid(
  () => instantiate(`(module (memory i64 1) (func (result i32) (i32.load8_u (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load64.wast:374
assert_invalid(
  () => instantiate(`(module (memory i64 1) (func (result i32) (i32.load16_s (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load64.wast:375
assert_invalid(
  () => instantiate(`(module (memory i64 1) (func (result i32) (i32.load16_u (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load64.wast:376
assert_invalid(
  () => instantiate(`(module (memory i64 1) (func (result i64) (i64.load (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load64.wast:377
assert_invalid(
  () => instantiate(`(module (memory i64 1) (func (result i64) (i64.load8_s (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load64.wast:378
assert_invalid(
  () => instantiate(`(module (memory i64 1) (func (result i64) (i64.load8_u (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load64.wast:379
assert_invalid(
  () => instantiate(`(module (memory i64 1) (func (result i64) (i64.load16_s (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load64.wast:380
assert_invalid(
  () => instantiate(`(module (memory i64 1) (func (result i64) (i64.load16_u (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load64.wast:381
assert_invalid(
  () => instantiate(`(module (memory i64 1) (func (result i64) (i64.load32_s (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load64.wast:382
assert_invalid(
  () => instantiate(`(module (memory i64 1) (func (result i64) (i64.load32_u (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load64.wast:383
assert_invalid(
  () => instantiate(`(module (memory i64 1) (func (result f32) (f32.load (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load64.wast:384
assert_invalid(
  () => instantiate(`(module (memory i64 1) (func (result f64) (f64.load (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load64.wast:387
assert_invalid(
  () => instantiate(`(module
    (memory i64 0)
    (func $$type-address-empty
      (i32.load) (drop)
    )
  )`),
  `type mismatch`,
);

// ./test/core/load64.wast:396
assert_invalid(
  () => instantiate(`(module
    (memory i64 0)
    (func $$type-address-empty-in-block
      (i32.const 0)
      (block (i32.load) (drop))
    )
  )`),
  `type mismatch`,
);

// ./test/core/load64.wast:406
assert_invalid(
  () => instantiate(`(module
    (memory i64 0)
    (func $$type-address-empty-in-loop
      (i32.const 0)
      (loop (i32.load) (drop))
    )
  )`),
  `type mismatch`,
);

// ./test/core/load64.wast:416
assert_invalid(
  () => instantiate(`(module
    (memory i64 0)
    (func $$type-address-empty-in-then
      (i32.const 0) (i32.const 0)
      (if (then (i32.load) (drop)))
    )
  )`),
  `type mismatch`,
);

// ./test/core/load64.wast:426
assert_invalid(
  () => instantiate(`(module
    (memory i64 0)
    (func $$type-address-empty-in-else
      (i32.const 0) (i32.const 0)
      (if (result i32) (then (i32.const 0)) (else (i32.load))) (drop)
    )
  )`),
  `type mismatch`,
);

// ./test/core/load64.wast:436
assert_invalid(
  () => instantiate(`(module
    (memory i64 0)
    (func $$type-address-empty-in-br
      (i32.const 0)
      (block (br 0 (i32.load)) (drop))
    )
  )`),
  `type mismatch`,
);

// ./test/core/load64.wast:446
assert_invalid(
  () => instantiate(`(module
    (memory i64 0)
    (func $$type-address-empty-in-br_if
      (i32.const 0)
      (block (br_if 0 (i32.load) (i32.const 1)) (drop))
    )
  )`),
  `type mismatch`,
);

// ./test/core/load64.wast:456
assert_invalid(
  () => instantiate(`(module
    (memory i64 0)
    (func $$type-address-empty-in-br_table
      (i32.const 0)
      (block (br_table 0 (i32.load)) (drop))
    )
  )`),
  `type mismatch`,
);

// ./test/core/load64.wast:466
assert_invalid(
  () => instantiate(`(module
    (memory i64 0)
    (func $$type-address-empty-in-return
      (return (i32.load)) (drop)
    )
  )`),
  `type mismatch`,
);

// ./test/core/load64.wast:475
assert_invalid(
  () => instantiate(`(module
    (memory i64 0)
    (func $$type-address-empty-in-select
      (select (i32.load) (i32.const 1) (i32.const 2)) (drop)
    )
  )`),
  `type mismatch`,
);

// ./test/core/load64.wast:484
assert_invalid(
  () => instantiate(`(module
    (memory i64 0)
    (func $$type-address-empty-in-call
      (call 1 (i32.load)) (drop)
    )
    (func (param i32) (result i32) (local.get 0))
  )`),
  `type mismatch`,
);

// ./test/core/load64.wast:494
assert_invalid(
  () => instantiate(`(module
    (memory i64 0)
    (func $$f (param i32) (result i32) (local.get 0))
    (type $$sig (func (param i32) (result i32)))
    (table funcref (elem $$f))
    (func $$type-address-empty-in-call_indirect
      (block (result i32)
        (call_indirect (type $$sig)
          (i32.load) (i32.const 0)
        )
        (drop)
      )
    )
  )`),
  `type mismatch`,
);

// ./test/core/load64.wast:511
assert_invalid(
  () => instantiate(`(module
    (memory i64 0)
    (func $$type-address-empty-in-local.set
      (local i32)
      (local.set 0 (i32.load)) (local.get 0) (drop)
    )
  )`),
  `type mismatch`,
);

// ./test/core/load64.wast:521
assert_invalid(
  () => instantiate(`(module
    (memory i64 0)
    (func $$type-address-empty-in-local.tee
      (local i32)
      (local.tee 0 (i32.load)) (drop)
    )
  )`),
  `type mismatch`,
);

// ./test/core/load64.wast:531
assert_invalid(
  () => instantiate(`(module
    (memory i64 0)
    (global $$x (mut i32) (i32.const 0))
    (func $$type-address-empty-in-global.set
      (global.set $$x (i32.load)) (global.get $$x) (drop)
    )
  )`),
  `type mismatch`,
);

// ./test/core/load64.wast:541
assert_invalid(
  () => instantiate(`(module
    (memory i64 0)
    (func $$type-address-empty-in-memory.grow
      (memory.grow (i64.load)) (drop)
    )
  )`),
  `type mismatch`,
);

// ./test/core/load64.wast:550
assert_invalid(
  () => instantiate(`(module
    (memory i64 0)
    (func $$type-address-empty-in-load
      (i32.load (i32.load)) (drop)
    )
  )`),
  `type mismatch`,
);

// ./test/core/load64.wast:559
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (func $$type-address-empty-in-store
      (i32.store (i32.load) (i32.const 1))
    )
  )`),
  `type mismatch`,
);
