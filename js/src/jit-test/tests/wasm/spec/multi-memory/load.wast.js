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

// ./test/core/load.wast

// ./test/core/load.wast:3
let $0 = instantiate(`(module
  (memory $$mem1 1)
  (memory $$mem2 1)

  (func (export "load1") (param i32) (result i64)
    (i64.load $$mem1 (local.get 0))
  )
  (func (export "load2") (param i32) (result i64)
    (i64.load $$mem2 (local.get 0))
  )

  (data (memory $$mem1) (i32.const 0) "\\01")
  (data (memory $$mem2) (i32.const 0) "\\02")
)`);

// ./test/core/load.wast:18
assert_return(() => invoke($0, `load1`, [0]), [value("i64", 1n)]);

// ./test/core/load.wast:19
assert_return(() => invoke($0, `load2`, [0]), [value("i64", 2n)]);

// ./test/core/load.wast:22
let $1 = instantiate(`(module $$M
  (memory (export "mem") 2)

  (func (export "read") (param i32) (result i32)
    (i32.load8_u (local.get 0))
  )
)`);
register($1, `M`);

// ./test/core/load.wast:29
register($1, `M`);

// ./test/core/load.wast:31
let $2 = instantiate(`(module
  (memory $$mem1 (import "M" "mem") 2)
  (memory $$mem2 3)

  (data (memory $$mem1) (i32.const 20) "\\01\\02\\03\\04\\05")
  (data (memory $$mem2) (i32.const 50) "\\0A\\0B\\0C\\0D\\0E")

  (func (export "read1") (param i32) (result i32)
    (i32.load8_u $$mem1 (local.get 0))
  )
  (func (export "read2") (param i32) (result i32)
    (i32.load8_u $$mem2 (local.get 0))
  )
)`);

// ./test/core/load.wast:46
assert_return(() => invoke(`M`, `read`, [20]), [value("i32", 1)]);

// ./test/core/load.wast:47
assert_return(() => invoke(`M`, `read`, [21]), [value("i32", 2)]);

// ./test/core/load.wast:48
assert_return(() => invoke(`M`, `read`, [22]), [value("i32", 3)]);

// ./test/core/load.wast:49
assert_return(() => invoke(`M`, `read`, [23]), [value("i32", 4)]);

// ./test/core/load.wast:50
assert_return(() => invoke(`M`, `read`, [24]), [value("i32", 5)]);

// ./test/core/load.wast:52
assert_return(() => invoke($2, `read1`, [20]), [value("i32", 1)]);

// ./test/core/load.wast:53
assert_return(() => invoke($2, `read1`, [21]), [value("i32", 2)]);

// ./test/core/load.wast:54
assert_return(() => invoke($2, `read1`, [22]), [value("i32", 3)]);

// ./test/core/load.wast:55
assert_return(() => invoke($2, `read1`, [23]), [value("i32", 4)]);

// ./test/core/load.wast:56
assert_return(() => invoke($2, `read1`, [24]), [value("i32", 5)]);

// ./test/core/load.wast:58
assert_return(() => invoke($2, `read2`, [50]), [value("i32", 10)]);

// ./test/core/load.wast:59
assert_return(() => invoke($2, `read2`, [51]), [value("i32", 11)]);

// ./test/core/load.wast:60
assert_return(() => invoke($2, `read2`, [52]), [value("i32", 12)]);

// ./test/core/load.wast:61
assert_return(() => invoke($2, `read2`, [53]), [value("i32", 13)]);

// ./test/core/load.wast:62
assert_return(() => invoke($2, `read2`, [54]), [value("i32", 14)]);

// ./test/core/load.wast:67
let $3 = instantiate(`(module
  (memory 1)

  (func (export "as-br-value") (result i32)
    (block (result i32) (br 0 (i32.load (i32.const 0))))
  )

  (func (export "as-br_if-cond")
    (block (br_if 0 (i32.load (i32.const 0))))
  )
  (func (export "as-br_if-value") (result i32)
    (block (result i32)
      (drop (br_if 0 (i32.load (i32.const 0)) (i32.const 1))) (i32.const 7)
    )
  )
  (func (export "as-br_if-value-cond") (result i32)
    (block (result i32)
      (drop (br_if 0 (i32.const 6) (i32.load (i32.const 0)))) (i32.const 7)
    )
  )

  (func (export "as-br_table-index")
    (block (br_table 0 0 0 (i32.load (i32.const 0))))
  )
  (func (export "as-br_table-value") (result i32)
    (block (result i32)
      (br_table 0 0 0 (i32.load (i32.const 0)) (i32.const 1)) (i32.const 7)
    )
  )
  (func (export "as-br_table-value-index") (result i32)
    (block (result i32)
      (br_table 0 0 (i32.const 6) (i32.load (i32.const 0))) (i32.const 7)
    )
  )

  (func (export "as-return-value") (result i32)
    (return (i32.load (i32.const 0)))
  )

  (func (export "as-if-cond") (result i32)
    (if (result i32) (i32.load (i32.const 0))
      (then (i32.const 0)) (else (i32.const 1))
    )
  )
  (func (export "as-if-then") (result i32)
    (if (result i32) (i32.const 1)
      (then (i32.load (i32.const 0))) (else (i32.const 0))
    )
  )
  (func (export "as-if-else") (result i32)
    (if (result i32) (i32.const 0)
      (then (i32.const 0)) (else (i32.load (i32.const 0)))
    )
  )

  (func (export "as-select-first") (param i32 i32) (result i32)
    (select (i32.load (i32.const 0)) (local.get 0) (local.get 1))
  )
  (func (export "as-select-second") (param i32 i32) (result i32)
    (select (local.get 0) (i32.load (i32.const 0)) (local.get 1))
  )
  (func (export "as-select-cond") (result i32)
    (select (i32.const 0) (i32.const 1) (i32.load (i32.const 0)))
  )

  (func $$f (param i32 i32 i32) (result i32) (i32.const -1))
  (func (export "as-call-first") (result i32)
    (call $$f (i32.load (i32.const 0)) (i32.const 2) (i32.const 3))
  )
  (func (export "as-call-mid") (result i32)
    (call $$f (i32.const 1) (i32.load (i32.const 0)) (i32.const 3))
  )
  (func (export "as-call-last") (result i32)
    (call $$f (i32.const 1) (i32.const 2) (i32.load (i32.const 0)))
  )

  (type $$sig (func (param i32 i32 i32) (result i32)))
  (table funcref (elem $$f))
  (func (export "as-call_indirect-first") (result i32)
    (call_indirect (type $$sig)
      (i32.load (i32.const 0)) (i32.const 2) (i32.const 3) (i32.const 0)
    )
  )
  (func (export "as-call_indirect-mid") (result i32)
    (call_indirect (type $$sig)
      (i32.const 1) (i32.load (i32.const 0)) (i32.const 3) (i32.const 0)
    )
  )
  (func (export "as-call_indirect-last") (result i32)
    (call_indirect (type $$sig)
      (i32.const 1) (i32.const 2) (i32.load (i32.const 0)) (i32.const 0)
    )
  )
  (func (export "as-call_indirect-index") (result i32)
    (call_indirect (type $$sig)
      (i32.const 1) (i32.const 2) (i32.const 3) (i32.load (i32.const 0))
    )
  )

  (func (export "as-local.set-value") (local i32)
    (local.set 0 (i32.load (i32.const 0)))
  )
  (func (export "as-local.tee-value") (result i32) (local i32)
    (local.tee 0 (i32.load (i32.const 0)))
  )
  (global $$g (mut i32) (i32.const 0))
  (func (export "as-global.set-value") (local i32)
    (global.set $$g (i32.load (i32.const 0)))
  )

  (func (export "as-load-address") (result i32)
    (i32.load (i32.load (i32.const 0)))
  )
  (func (export "as-loadN-address") (result i32)
    (i32.load8_s (i32.load (i32.const 0)))
  )

  (func (export "as-store-address")
    (i32.store (i32.load (i32.const 0)) (i32.const 7))
  )
  (func (export "as-store-value")
    (i32.store (i32.const 2) (i32.load (i32.const 0)))
  )

  (func (export "as-storeN-address")
    (i32.store8 (i32.load8_s (i32.const 0)) (i32.const 7))
  )
  (func (export "as-storeN-value")
    (i32.store16 (i32.const 2) (i32.load (i32.const 0)))
  )

  (func (export "as-unary-operand") (result i32)
    (i32.clz (i32.load (i32.const 100)))
  )

  (func (export "as-binary-left") (result i32)
    (i32.add (i32.load (i32.const 100)) (i32.const 10))
  )
  (func (export "as-binary-right") (result i32)
    (i32.sub (i32.const 10) (i32.load (i32.const 100)))
  )

  (func (export "as-test-operand") (result i32)
    (i32.eqz (i32.load (i32.const 100)))
  )

  (func (export "as-compare-left") (result i32)
    (i32.le_s (i32.load (i32.const 100)) (i32.const 10))
  )
  (func (export "as-compare-right") (result i32)
    (i32.ne (i32.const 10) (i32.load (i32.const 100)))
  )

  (func (export "as-memory.grow-size") (result i32)
    (memory.grow (i32.load (i32.const 100)))
  )
)`);

// ./test/core/load.wast:225
assert_return(() => invoke($3, `as-br-value`, []), [value("i32", 0)]);

// ./test/core/load.wast:227
assert_return(() => invoke($3, `as-br_if-cond`, []), []);

// ./test/core/load.wast:228
assert_return(() => invoke($3, `as-br_if-value`, []), [value("i32", 0)]);

// ./test/core/load.wast:229
assert_return(() => invoke($3, `as-br_if-value-cond`, []), [value("i32", 7)]);

// ./test/core/load.wast:231
assert_return(() => invoke($3, `as-br_table-index`, []), []);

// ./test/core/load.wast:232
assert_return(() => invoke($3, `as-br_table-value`, []), [value("i32", 0)]);

// ./test/core/load.wast:233
assert_return(() => invoke($3, `as-br_table-value-index`, []), [value("i32", 6)]);

// ./test/core/load.wast:235
assert_return(() => invoke($3, `as-return-value`, []), [value("i32", 0)]);

// ./test/core/load.wast:237
assert_return(() => invoke($3, `as-if-cond`, []), [value("i32", 1)]);

// ./test/core/load.wast:238
assert_return(() => invoke($3, `as-if-then`, []), [value("i32", 0)]);

// ./test/core/load.wast:239
assert_return(() => invoke($3, `as-if-else`, []), [value("i32", 0)]);

// ./test/core/load.wast:241
assert_return(() => invoke($3, `as-select-first`, [0, 1]), [value("i32", 0)]);

// ./test/core/load.wast:242
assert_return(() => invoke($3, `as-select-second`, [0, 0]), [value("i32", 0)]);

// ./test/core/load.wast:243
assert_return(() => invoke($3, `as-select-cond`, []), [value("i32", 1)]);

// ./test/core/load.wast:245
assert_return(() => invoke($3, `as-call-first`, []), [value("i32", -1)]);

// ./test/core/load.wast:246
assert_return(() => invoke($3, `as-call-mid`, []), [value("i32", -1)]);

// ./test/core/load.wast:247
assert_return(() => invoke($3, `as-call-last`, []), [value("i32", -1)]);

// ./test/core/load.wast:249
assert_return(() => invoke($3, `as-call_indirect-first`, []), [value("i32", -1)]);

// ./test/core/load.wast:250
assert_return(() => invoke($3, `as-call_indirect-mid`, []), [value("i32", -1)]);

// ./test/core/load.wast:251
assert_return(() => invoke($3, `as-call_indirect-last`, []), [value("i32", -1)]);

// ./test/core/load.wast:252
assert_return(() => invoke($3, `as-call_indirect-index`, []), [value("i32", -1)]);

// ./test/core/load.wast:254
assert_return(() => invoke($3, `as-local.set-value`, []), []);

// ./test/core/load.wast:255
assert_return(() => invoke($3, `as-local.tee-value`, []), [value("i32", 0)]);

// ./test/core/load.wast:256
assert_return(() => invoke($3, `as-global.set-value`, []), []);

// ./test/core/load.wast:258
assert_return(() => invoke($3, `as-load-address`, []), [value("i32", 0)]);

// ./test/core/load.wast:259
assert_return(() => invoke($3, `as-loadN-address`, []), [value("i32", 0)]);

// ./test/core/load.wast:260
assert_return(() => invoke($3, `as-store-address`, []), []);

// ./test/core/load.wast:261
assert_return(() => invoke($3, `as-store-value`, []), []);

// ./test/core/load.wast:262
assert_return(() => invoke($3, `as-storeN-address`, []), []);

// ./test/core/load.wast:263
assert_return(() => invoke($3, `as-storeN-value`, []), []);

// ./test/core/load.wast:265
assert_return(() => invoke($3, `as-unary-operand`, []), [value("i32", 32)]);

// ./test/core/load.wast:267
assert_return(() => invoke($3, `as-binary-left`, []), [value("i32", 10)]);

// ./test/core/load.wast:268
assert_return(() => invoke($3, `as-binary-right`, []), [value("i32", 10)]);

// ./test/core/load.wast:270
assert_return(() => invoke($3, `as-test-operand`, []), [value("i32", 1)]);

// ./test/core/load.wast:272
assert_return(() => invoke($3, `as-compare-left`, []), [value("i32", 1)]);

// ./test/core/load.wast:273
assert_return(() => invoke($3, `as-compare-right`, []), [value("i32", 1)]);

// ./test/core/load.wast:275
assert_return(() => invoke($3, `as-memory.grow-size`, []), [value("i32", 1)]);

// ./test/core/load.wast:277
assert_malformed(
  () => instantiate(`(memory 1) (func (param i32) (result i32) (i32.load32 (local.get 0))) `),
  `unknown operator`,
);

// ./test/core/load.wast:284
assert_malformed(
  () => instantiate(`(memory 1) (func (param i32) (result i32) (i32.load32_u (local.get 0))) `),
  `unknown operator`,
);

// ./test/core/load.wast:291
assert_malformed(
  () => instantiate(`(memory 1) (func (param i32) (result i32) (i32.load32_s (local.get 0))) `),
  `unknown operator`,
);

// ./test/core/load.wast:298
assert_malformed(
  () => instantiate(`(memory 1) (func (param i32) (result i32) (i32.load64 (local.get 0))) `),
  `unknown operator`,
);

// ./test/core/load.wast:305
assert_malformed(
  () => instantiate(`(memory 1) (func (param i32) (result i32) (i32.load64_u (local.get 0))) `),
  `unknown operator`,
);

// ./test/core/load.wast:312
assert_malformed(
  () => instantiate(`(memory 1) (func (param i32) (result i32) (i32.load64_s (local.get 0))) `),
  `unknown operator`,
);

// ./test/core/load.wast:320
assert_malformed(
  () => instantiate(`(memory 1) (func (param i32) (result i64) (i64.load64 (local.get 0))) `),
  `unknown operator`,
);

// ./test/core/load.wast:327
assert_malformed(
  () => instantiate(`(memory 1) (func (param i32) (result i64) (i64.load64_u (local.get 0))) `),
  `unknown operator`,
);

// ./test/core/load.wast:334
assert_malformed(
  () => instantiate(`(memory 1) (func (param i32) (result i64) (i64.load64_s (local.get 0))) `),
  `unknown operator`,
);

// ./test/core/load.wast:342
assert_malformed(
  () => instantiate(`(memory 1) (func (param i32) (result f32) (f32.load32 (local.get 0))) `),
  `unknown operator`,
);

// ./test/core/load.wast:349
assert_malformed(
  () => instantiate(`(memory 1) (func (param i32) (result f32) (f32.load64 (local.get 0))) `),
  `unknown operator`,
);

// ./test/core/load.wast:357
assert_malformed(
  () => instantiate(`(memory 1) (func (param i32) (result f64) (f64.load32 (local.get 0))) `),
  `unknown operator`,
);

// ./test/core/load.wast:364
assert_malformed(
  () => instantiate(`(memory 1) (func (param i32) (result f64) (f64.load64 (local.get 0))) `),
  `unknown operator`,
);

// ./test/core/load.wast:375
assert_invalid(
  () => instantiate(`(module (memory 1) (func $$load_i32 (i32.load (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load.wast:379
assert_invalid(
  () => instantiate(`(module (memory 1) (func $$load8_s_i32 (i32.load8_s (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load.wast:383
assert_invalid(
  () => instantiate(`(module (memory 1) (func $$load8_u_i32 (i32.load8_u (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load.wast:387
assert_invalid(
  () => instantiate(`(module (memory 1) (func $$load16_s_i32 (i32.load16_s (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load.wast:391
assert_invalid(
  () => instantiate(`(module (memory 1) (func $$load16_u_i32 (i32.load16_u (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load.wast:395
assert_invalid(
  () => instantiate(`(module (memory 1) (func $$load_i64 (i64.load (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load.wast:399
assert_invalid(
  () => instantiate(`(module (memory 1) (func $$load8_s_i64 (i64.load8_s (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load.wast:403
assert_invalid(
  () => instantiate(`(module (memory 1) (func $$load8_u_i64 (i64.load8_u (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load.wast:407
assert_invalid(
  () => instantiate(`(module (memory 1) (func $$load16_s_i64 (i64.load16_s (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load.wast:411
assert_invalid(
  () => instantiate(`(module (memory 1) (func $$load16_u_i64 (i64.load16_u (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load.wast:415
assert_invalid(
  () => instantiate(`(module (memory 1) (func $$load32_s_i64 (i64.load32_s (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load.wast:419
assert_invalid(
  () => instantiate(`(module (memory 1) (func $$load32_u_i64 (i64.load32_u (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load.wast:423
assert_invalid(
  () => instantiate(`(module (memory 1) (func $$load_f32 (f32.load (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load.wast:427
assert_invalid(
  () => instantiate(`(module (memory 1) (func $$load_f64 (f64.load (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load.wast:435
assert_invalid(
  () => instantiate(`(module (memory 1) (func (result i32) (i32.load (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load.wast:436
assert_invalid(
  () => instantiate(`(module (memory 1) (func (result i32) (i32.load8_s (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load.wast:437
assert_invalid(
  () => instantiate(`(module (memory 1) (func (result i32) (i32.load8_u (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load.wast:438
assert_invalid(
  () => instantiate(`(module (memory 1) (func (result i32) (i32.load16_s (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load.wast:439
assert_invalid(
  () => instantiate(`(module (memory 1) (func (result i32) (i32.load16_u (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load.wast:440
assert_invalid(
  () => instantiate(`(module (memory 1) (func (result i64) (i64.load (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load.wast:441
assert_invalid(
  () => instantiate(`(module (memory 1) (func (result i64) (i64.load8_s (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load.wast:442
assert_invalid(
  () => instantiate(`(module (memory 1) (func (result i64) (i64.load8_u (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load.wast:443
assert_invalid(
  () => instantiate(`(module (memory 1) (func (result i64) (i64.load16_s (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load.wast:444
assert_invalid(
  () => instantiate(`(module (memory 1) (func (result i64) (i64.load16_u (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load.wast:445
assert_invalid(
  () => instantiate(`(module (memory 1) (func (result i64) (i64.load32_s (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load.wast:446
assert_invalid(
  () => instantiate(`(module (memory 1) (func (result i64) (i64.load32_u (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load.wast:447
assert_invalid(
  () => instantiate(`(module (memory 1) (func (result f32) (f32.load (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load.wast:448
assert_invalid(
  () => instantiate(`(module (memory 1) (func (result f64) (f64.load (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/load.wast:451
assert_invalid(
  () => instantiate(`(module
    (memory 0)
    (func $$type-address-empty
      (i32.load) (drop)
    )
  )`),
  `type mismatch`,
);

// ./test/core/load.wast:460
assert_invalid(
  () => instantiate(`(module
    (memory 0)
    (func $$type-address-empty-in-block
      (i32.const 0)
      (block (i32.load) (drop))
    )
  )`),
  `type mismatch`,
);

// ./test/core/load.wast:470
assert_invalid(
  () => instantiate(`(module
    (memory 0)
    (func $$type-address-empty-in-loop
      (i32.const 0)
      (loop (i32.load) (drop))
    )
  )`),
  `type mismatch`,
);

// ./test/core/load.wast:480
assert_invalid(
  () => instantiate(`(module
    (memory 0)
    (func $$type-address-empty-in-then
      (i32.const 0) (i32.const 0)
      (if (then (i32.load) (drop)))
    )
  )`),
  `type mismatch`,
);

// ./test/core/load.wast:490
assert_invalid(
  () => instantiate(`(module
    (memory 0)
    (func $$type-address-empty-in-else
      (i32.const 0) (i32.const 0)
      (if (result i32) (then (i32.const 0)) (else (i32.load))) (drop)
    )
  )`),
  `type mismatch`,
);

// ./test/core/load.wast:500
assert_invalid(
  () => instantiate(`(module
    (memory 0)
    (func $$type-address-empty-in-br
      (i32.const 0)
      (block (br 0 (i32.load)) (drop))
    )
  )`),
  `type mismatch`,
);

// ./test/core/load.wast:510
assert_invalid(
  () => instantiate(`(module
    (memory 0)
    (func $$type-address-empty-in-br_if
      (i32.const 0)
      (block (br_if 0 (i32.load) (i32.const 1)) (drop))
    )
  )`),
  `type mismatch`,
);

// ./test/core/load.wast:520
assert_invalid(
  () => instantiate(`(module
    (memory 0)
    (func $$type-address-empty-in-br_table
      (i32.const 0)
      (block (br_table 0 (i32.load)) (drop))
    )
  )`),
  `type mismatch`,
);

// ./test/core/load.wast:530
assert_invalid(
  () => instantiate(`(module
    (memory 0)
    (func $$type-address-empty-in-return
      (return (i32.load)) (drop)
    )
  )`),
  `type mismatch`,
);

// ./test/core/load.wast:539
assert_invalid(
  () => instantiate(`(module
    (memory 0)
    (func $$type-address-empty-in-select
      (select (i32.load) (i32.const 1) (i32.const 2)) (drop)
    )
  )`),
  `type mismatch`,
);

// ./test/core/load.wast:548
assert_invalid(
  () => instantiate(`(module
    (memory 0)
    (func $$type-address-empty-in-call
      (call 1 (i32.load)) (drop)
    )
    (func (param i32) (result i32) (local.get 0))
  )`),
  `type mismatch`,
);

// ./test/core/load.wast:558
assert_invalid(
  () => instantiate(`(module
    (memory 0)
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

// ./test/core/load.wast:575
assert_invalid(
  () => instantiate(`(module
    (memory 0)
    (func $$type-address-empty-in-local.set
      (local i32)
      (local.set 0 (i32.load)) (local.get 0) (drop)
    )
  )`),
  `type mismatch`,
);

// ./test/core/load.wast:585
assert_invalid(
  () => instantiate(`(module
    (memory 0)
    (func $$type-address-empty-in-local.tee
      (local i32)
      (local.tee 0 (i32.load)) (drop)
    )
  )`),
  `type mismatch`,
);

// ./test/core/load.wast:595
assert_invalid(
  () => instantiate(`(module
    (memory 0)
    (global $$x (mut i32) (i32.const 0))
    (func $$type-address-empty-in-global.set
      (global.set $$x (i32.load)) (global.get $$x) (drop)
    )
  )`),
  `type mismatch`,
);

// ./test/core/load.wast:605
assert_invalid(
  () => instantiate(`(module
    (memory 0)
    (func $$type-address-empty-in-memory.grow
      (memory.grow (i32.load)) (drop)
    )
  )`),
  `type mismatch`,
);

// ./test/core/load.wast:614
assert_invalid(
  () => instantiate(`(module
    (memory 0)
    (func $$type-address-empty-in-load
      (i32.load (i32.load)) (drop)
    )
  )`),
  `type mismatch`,
);

// ./test/core/load.wast:623
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-address-empty-in-store
      (i32.store (i32.load) (i32.const 1))
    )
  )`),
  `type mismatch`,
);
