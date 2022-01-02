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

// ./test/core/block.wast

// ./test/core/block.wast:3
let $0 = instantiate(`(module
  ;; Auxiliary definition
  (memory 1)

  (func $$dummy)

  (func (export "empty")
    (block)
    (block $$l)
  )

  (func (export "singular") (result i32)
    (block (nop))
    (block (result i32) (i32.const 7))
  )

  (func (export "multi") (result i32)
    (block (call $$dummy) (call $$dummy) (call $$dummy) (call $$dummy))
    (block (result i32) (call $$dummy) (call $$dummy) (call $$dummy) (i32.const 8))
  )

  (func (export "nested") (result i32)
    (block (result i32)
      (block (call $$dummy) (block) (nop))
      (block (result i32) (call $$dummy) (i32.const 9))
    )
  )

  (func (export "deep") (result i32)
    (block (result i32) (block (result i32)
      (block (result i32) (block (result i32)
        (block (result i32) (block (result i32)
          (block (result i32) (block (result i32)
            (block (result i32) (block (result i32)
              (block (result i32) (block (result i32)
                (block (result i32) (block (result i32)
                  (block (result i32) (block (result i32)
                    (block (result i32) (block (result i32)
                      (block (result i32) (block (result i32)
                        (block (result i32) (block (result i32)
                          (block (result i32) (block (result i32)
                            (block (result i32) (block (result i32)
                              (block (result i32) (block (result i32)
                                (block (result i32) (block (result i32)
                                  (block (result i32) (block (result i32)
                                    (block (result i32) (block (result i32)
                                      (block (result i32) (block (result i32)
                                        (block (result i32) (block (result i32)
                                          (call $$dummy) (i32.const 150)
                                        ))
                                      ))
                                    ))
                                  ))
                                ))
                              ))
                            ))
                          ))
                        ))
                      ))
                    ))
                  ))
                ))
              ))
            ))
          ))
        ))
      ))
    ))
  )

  (func (export "as-select-first") (result i32)
    (select (block (result i32) (i32.const 1)) (i32.const 2) (i32.const 3))
  )
  (func (export "as-select-mid") (result i32)
    (select (i32.const 2) (block (result i32) (i32.const 1)) (i32.const 3))
  )
  (func (export "as-select-last") (result i32)
    (select (i32.const 2) (i32.const 3) (block (result i32) (i32.const 1)))
  )

  (func (export "as-loop-first") (result i32)
    (loop (result i32) (block (result i32) (i32.const 1)) (call $$dummy) (call $$dummy))
  )
  (func (export "as-loop-mid") (result i32)
    (loop (result i32) (call $$dummy) (block (result i32) (i32.const 1)) (call $$dummy))
  )
  (func (export "as-loop-last") (result i32)
    (loop (result i32) (call $$dummy) (call $$dummy) (block (result i32) (i32.const 1)))
  )

  (func (export "as-if-condition")
    (block (result i32) (i32.const 1)) (if (then (call $$dummy)))
  )
  (func (export "as-if-then") (result i32)
    (if (result i32) (i32.const 1) (then (block (result i32) (i32.const 1))) (else (i32.const 2)))
  )
  (func (export "as-if-else") (result i32)
    (if (result i32) (i32.const 1) (then (i32.const 2)) (else (block (result i32) (i32.const 1))))
  )

  (func (export "as-br_if-first") (result i32)
    (block (result i32) (br_if 0 (block (result i32) (i32.const 1)) (i32.const 2)))
  )
  (func (export "as-br_if-last") (result i32)
    (block (result i32) (br_if 0 (i32.const 2) (block (result i32) (i32.const 1))))
  )

  (func (export "as-br_table-first") (result i32)
    (block (result i32) (block (result i32) (i32.const 1)) (i32.const 2) (br_table 0 0))
  )
  (func (export "as-br_table-last") (result i32)
    (block (result i32) (i32.const 2) (block (result i32) (i32.const 1)) (br_table 0 0))
  )

  (func $$func (param i32 i32) (result i32) (local.get 0))
  (type $$check (func (param i32 i32) (result i32)))
  (table funcref (elem $$func))
  (func (export "as-call_indirect-first") (result i32)
    (block (result i32)
      (call_indirect (type $$check)
        (block (result i32) (i32.const 1)) (i32.const 2) (i32.const 0)
      )
    )
  )
  (func (export "as-call_indirect-mid") (result i32)
    (block (result i32)
      (call_indirect (type $$check)
        (i32.const 2) (block (result i32) (i32.const 1)) (i32.const 0)
      )
    )
  )
  (func (export "as-call_indirect-last") (result i32)
    (block (result i32)
      (call_indirect (type $$check)
        (i32.const 1) (i32.const 2) (block (result i32) (i32.const 0))
      )
    )
  )

  (func (export "as-store-first")
    (block (result i32) (i32.const 1)) (i32.const 1) (i32.store)
  )
  (func (export "as-store-last")
    (i32.const 10) (block (result i32) (i32.const 1)) (i32.store)
  )

  (func (export "as-memory.grow-value") (result i32)
    (memory.grow (block (result i32) (i32.const 1)))
  )

  (func $$f (param i32) (result i32) (local.get 0))

  (func (export "as-call-value") (result i32)
    (call $$f (block (result i32) (i32.const 1)))
  )
  (func (export "as-return-value") (result i32)
    (block (result i32) (i32.const 1)) (return)
  )
  (func (export "as-drop-operand")
    (drop (block (result i32) (i32.const 1)))
  )
  (func (export "as-br-value") (result i32)
    (block (result i32) (br 0 (block (result i32) (i32.const 1))))
  )
  (func (export "as-local.set-value") (result i32)
    (local i32) (local.set 0 (block (result i32) (i32.const 1))) (local.get 0)
  )
  (func (export "as-local.tee-value") (result i32)
    (local i32) (local.tee 0 (block (result i32) (i32.const 1)))
  )
  (global $$a (mut i32) (i32.const 10))
  (func (export "as-global.set-value") (result i32)
    (global.set $$a (block (result i32) (i32.const 1)))
    (global.get $$a)
  )

  (func (export "as-load-operand") (result i32)
    (i32.load (block (result i32) (i32.const 1)))
  )

  (func (export "as-unary-operand") (result i32)
    (i32.ctz (block (result i32) (call $$dummy) (i32.const 13)))
  )
  (func (export "as-binary-operand") (result i32)
    (i32.mul
      (block (result i32) (call $$dummy) (i32.const 3))
      (block (result i32) (call $$dummy) (i32.const 4))
    )
  )
  (func (export "as-test-operand") (result i32)
    (i32.eqz (block (result i32) (call $$dummy) (i32.const 13)))
  )
  (func (export "as-compare-operand") (result i32)
    (f32.gt
      (block (result f32) (call $$dummy) (f32.const 3))
      (block (result f32) (call $$dummy) (f32.const 3))
    )
  )

  (func (export "break-bare") (result i32)
    (block (br 0) (unreachable))
    (block (br_if 0 (i32.const 1)) (unreachable))
    (block (br_table 0 (i32.const 0)) (unreachable))
    (block (br_table 0 0 0 (i32.const 1)) (unreachable))
    (i32.const 19)
  )
  (func (export "break-value") (result i32)
    (block (result i32) (br 0 (i32.const 18)) (i32.const 19))
  )
  (func (export "break-repeated") (result i32)
    (block (result i32)
      (br 0 (i32.const 18))
      (br 0 (i32.const 19))
      (drop (br_if 0 (i32.const 20) (i32.const 0)))
      (drop (br_if 0 (i32.const 20) (i32.const 1)))
      (br 0 (i32.const 21))
      (br_table 0 (i32.const 22) (i32.const 4))
      (br_table 0 0 0 (i32.const 23) (i32.const 1))
      (i32.const 21)
    )
  )
  (func (export "break-inner") (result i32)
    (local i32)
    (local.set 0 (i32.const 0))
    (local.set 0 (i32.add (local.get 0) (block (result i32) (block (result i32) (br 1 (i32.const 0x1))))))
    (local.set 0 (i32.add (local.get 0) (block (result i32) (block (br 0)) (i32.const 0x2))))
    (local.set 0
      (i32.add (local.get 0) (block (result i32) (i32.ctz (br 0 (i32.const 0x4)))))
    )
    (local.set 0
      (i32.add (local.get 0) (block (result i32) (i32.ctz (block (result i32) (br 1 (i32.const 0x8))))))
    )
    (local.get 0)
  )

  (func (export "effects") (result i32)
    (local i32)
    (block
      (local.set 0 (i32.const 1))
      (local.set 0 (i32.mul (local.get 0) (i32.const 3)))
      (local.set 0 (i32.sub (local.get 0) (i32.const 5)))
      (local.set 0 (i32.mul (local.get 0) (i32.const 7)))
      (br 0)
      (local.set 0 (i32.mul (local.get 0) (i32.const 100)))
    )
    (i32.eq (local.get 0) (i32.const -14))
  )
)`);

// ./test/core/block.wast:252
assert_return(() => invoke($0, `empty`, []), []);

// ./test/core/block.wast:253
assert_return(() => invoke($0, `singular`, []), [value("i32", 7)]);

// ./test/core/block.wast:254
assert_return(() => invoke($0, `multi`, []), [value("i32", 8)]);

// ./test/core/block.wast:255
assert_return(() => invoke($0, `nested`, []), [value("i32", 9)]);

// ./test/core/block.wast:256
assert_return(() => invoke($0, `deep`, []), [value("i32", 150)]);

// ./test/core/block.wast:258
assert_return(() => invoke($0, `as-select-first`, []), [value("i32", 1)]);

// ./test/core/block.wast:259
assert_return(() => invoke($0, `as-select-mid`, []), [value("i32", 2)]);

// ./test/core/block.wast:260
assert_return(() => invoke($0, `as-select-last`, []), [value("i32", 2)]);

// ./test/core/block.wast:262
assert_return(() => invoke($0, `as-loop-first`, []), [value("i32", 1)]);

// ./test/core/block.wast:263
assert_return(() => invoke($0, `as-loop-mid`, []), [value("i32", 1)]);

// ./test/core/block.wast:264
assert_return(() => invoke($0, `as-loop-last`, []), [value("i32", 1)]);

// ./test/core/block.wast:266
assert_return(() => invoke($0, `as-if-condition`, []), []);

// ./test/core/block.wast:267
assert_return(() => invoke($0, `as-if-then`, []), [value("i32", 1)]);

// ./test/core/block.wast:268
assert_return(() => invoke($0, `as-if-else`, []), [value("i32", 2)]);

// ./test/core/block.wast:270
assert_return(() => invoke($0, `as-br_if-first`, []), [value("i32", 1)]);

// ./test/core/block.wast:271
assert_return(() => invoke($0, `as-br_if-last`, []), [value("i32", 2)]);

// ./test/core/block.wast:273
assert_return(() => invoke($0, `as-br_table-first`, []), [value("i32", 1)]);

// ./test/core/block.wast:274
assert_return(() => invoke($0, `as-br_table-last`, []), [value("i32", 2)]);

// ./test/core/block.wast:276
assert_return(() => invoke($0, `as-call_indirect-first`, []), [
  value("i32", 1),
]);

// ./test/core/block.wast:277
assert_return(() => invoke($0, `as-call_indirect-mid`, []), [value("i32", 2)]);

// ./test/core/block.wast:278
assert_return(() => invoke($0, `as-call_indirect-last`, []), [value("i32", 1)]);

// ./test/core/block.wast:280
assert_return(() => invoke($0, `as-store-first`, []), []);

// ./test/core/block.wast:281
assert_return(() => invoke($0, `as-store-last`, []), []);

// ./test/core/block.wast:283
assert_return(() => invoke($0, `as-memory.grow-value`, []), [value("i32", 1)]);

// ./test/core/block.wast:284
assert_return(() => invoke($0, `as-call-value`, []), [value("i32", 1)]);

// ./test/core/block.wast:285
assert_return(() => invoke($0, `as-return-value`, []), [value("i32", 1)]);

// ./test/core/block.wast:286
assert_return(() => invoke($0, `as-drop-operand`, []), []);

// ./test/core/block.wast:287
assert_return(() => invoke($0, `as-br-value`, []), [value("i32", 1)]);

// ./test/core/block.wast:288
assert_return(() => invoke($0, `as-local.set-value`, []), [value("i32", 1)]);

// ./test/core/block.wast:289
assert_return(() => invoke($0, `as-local.tee-value`, []), [value("i32", 1)]);

// ./test/core/block.wast:290
assert_return(() => invoke($0, `as-global.set-value`, []), [value("i32", 1)]);

// ./test/core/block.wast:291
assert_return(() => invoke($0, `as-load-operand`, []), [value("i32", 1)]);

// ./test/core/block.wast:293
assert_return(() => invoke($0, `as-unary-operand`, []), [value("i32", 0)]);

// ./test/core/block.wast:294
assert_return(() => invoke($0, `as-binary-operand`, []), [value("i32", 12)]);

// ./test/core/block.wast:295
assert_return(() => invoke($0, `as-test-operand`, []), [value("i32", 0)]);

// ./test/core/block.wast:296
assert_return(() => invoke($0, `as-compare-operand`, []), [value("i32", 0)]);

// ./test/core/block.wast:298
assert_return(() => invoke($0, `break-bare`, []), [value("i32", 19)]);

// ./test/core/block.wast:299
assert_return(() => invoke($0, `break-value`, []), [value("i32", 18)]);

// ./test/core/block.wast:300
assert_return(() => invoke($0, `break-repeated`, []), [value("i32", 18)]);

// ./test/core/block.wast:301
assert_return(() => invoke($0, `break-inner`, []), [value("i32", 15)]);

// ./test/core/block.wast:303
assert_return(() => invoke($0, `effects`, []), [value("i32", 1)]);

// ./test/core/block.wast:305
assert_invalid(
  () => instantiate(`(module (func $$type-empty-i32 (result i32) (block)))`),
  `type mismatch`,
);

// ./test/core/block.wast:309
assert_invalid(
  () => instantiate(`(module (func $$type-empty-i64 (result i64) (block)))`),
  `type mismatch`,
);

// ./test/core/block.wast:313
assert_invalid(
  () => instantiate(`(module (func $$type-empty-f32 (result f32) (block)))`),
  `type mismatch`,
);

// ./test/core/block.wast:317
assert_invalid(
  () => instantiate(`(module (func $$type-empty-f64 (result f64) (block)))`),
  `type mismatch`,
);

// ./test/core/block.wast:322
assert_invalid(() =>
  instantiate(`(module (func $$type-value-i32-vs-void
    (block (i32.const 1))
  ))`), `type mismatch`);

// ./test/core/block.wast:328
assert_invalid(() =>
  instantiate(`(module (func $$type-value-i64-vs-void
    (block (i64.const 1))
  ))`), `type mismatch`);

// ./test/core/block.wast:334
assert_invalid(() =>
  instantiate(`(module (func $$type-value-f32-vs-void
    (block (f32.const 1.0))
  ))`), `type mismatch`);

// ./test/core/block.wast:340
assert_invalid(() =>
  instantiate(`(module (func $$type-value-f64-vs-void
    (block (f64.const 1.0))
  ))`), `type mismatch`);

// ./test/core/block.wast:347
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-empty-vs-i32 (result i32)
    (block (result i32))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:353
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-empty-vs-i64 (result i64)
    (block (result i64))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:359
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-empty-vs-f32 (result f32)
    (block (result f32))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:365
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-empty-vs-f64 (result f64)
    (block (result f64))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:372
assert_invalid(() =>
  instantiate(`(module
    (func $$type-value-empty-in-block
      (i32.const 0)
      (block (block (result i32)) (drop))
    )
  )`), `type mismatch`);

// ./test/core/block.wast:381
assert_invalid(() =>
  instantiate(`(module
    (func $$type-value-empty-in-loop
      (i32.const 0)
      (loop (block (result i32)) (drop))
    )
  )`), `type mismatch`);

// ./test/core/block.wast:390
assert_invalid(() =>
  instantiate(`(module
    (func $$type-value-empty-in-then
      (i32.const 0) (i32.const 0)
      (if (then (block (result i32)) (drop)))
    )
  )`), `type mismatch`);

// ./test/core/block.wast:400
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-void-vs-i32 (result i32)
    (block (result i32) (nop))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:406
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-void-vs-i64 (result i64)
    (block (result i64) (nop))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:412
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-void-vs-f32 (result f32)
    (block (result f32) (nop))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:418
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-void-vs-f64 (result f64)
    (block (result f64) (nop))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:425
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-i32-vs-i64 (result i32)
    (block (result i32) (i64.const 0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:431
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-i32-vs-f32 (result i32)
    (block (result i32) (f32.const 0.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:437
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-i32-vs-f64 (result i32)
    (block (result i32) (f64.const 0.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:443
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-i64-vs-i32 (result i64)
    (block (result i64) (i32.const 0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:449
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-i64-vs-f32 (result i64)
    (block (result i64) (f32.const 0.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:455
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-i64-vs-f64 (result i64)
    (block (result i64) (f64.const 0.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:461
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-f32-vs-i32 (result f32)
    (block (result f32) (i32.const 0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:467
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-f32-vs-i64 (result f32)
    (block (result f32) (i64.const 0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:473
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-f32-vs-f64 (result f32)
    (block (result f32) (f64.const 0.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:479
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-f64-vs-i32 (result f64)
    (block (result f64) (i32.const 0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:485
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-f64-vs-i64 (result f64)
    (block (result f64) (i64.const 0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:491
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-f64-vs-f32 (result f32)
    (block (result f64) (f32.const 0.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:498
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-value-unreached-select-i32-i64 (result i32)
    (block (result i64) (select (unreachable) (unreachable) (unreachable)))
  ))`,
    ),
  `type mismatch`,
);

// ./test/core/block.wast:504
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-value-unreached-select-i32-f32 (result i32)
    (block (result f32) (select (unreachable) (unreachable) (unreachable)))
  ))`,
    ),
  `type mismatch`,
);

// ./test/core/block.wast:510
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-value-unreached-select-i32-f64 (result i32)
    (block (result f64) (select (unreachable) (unreachable) (unreachable)))
  ))`,
    ),
  `type mismatch`,
);

// ./test/core/block.wast:516
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-value-unreached-select-i64-i32 (result i64)
    (block (result i32) (select (unreachable) (unreachable) (unreachable)))
  ))`,
    ),
  `type mismatch`,
);

// ./test/core/block.wast:522
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-value-unreached-select-i64-f32 (result i64)
    (block (result f32) (select (unreachable) (unreachable) (unreachable)))
  ))`,
    ),
  `type mismatch`,
);

// ./test/core/block.wast:528
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-value-unreached-select-i64-f64 (result i64)
    (block (result f64) (select (unreachable) (unreachable) (unreachable)))
  ))`,
    ),
  `type mismatch`,
);

// ./test/core/block.wast:534
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-value-unreached-select-f32-i32 (result f32)
    (block (result i32) (select (unreachable) (unreachable) (unreachable)))
  ))`,
    ),
  `type mismatch`,
);

// ./test/core/block.wast:540
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-value-unreached-select-f32-i64 (result f32)
    (block (result i64) (select (unreachable) (unreachable) (unreachable)))
  ))`,
    ),
  `type mismatch`,
);

// ./test/core/block.wast:546
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-value-unreached-select-f32-f64 (result f32)
    (block (result f64) (select (unreachable) (unreachable) (unreachable)))
  ))`,
    ),
  `type mismatch`,
);

// ./test/core/block.wast:552
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-value-unreached-select-f64-i32 (result f64)
    (block (result i32) (select (unreachable) (unreachable) (unreachable)))
  ))`,
    ),
  `type mismatch`,
);

// ./test/core/block.wast:558
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-value-unreached-select-f64-i64 (result f64)
    (block (result i64) (select (unreachable) (unreachable) (unreachable)))
  ))`,
    ),
  `type mismatch`,
);

// ./test/core/block.wast:564
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-value-unreached-select-f64-f32 (result f64)
    (block (result f32) (select (unreachable) (unreachable) (unreachable)))
  ))`,
    ),
  `type mismatch`,
);

// ./test/core/block.wast:571
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-last-void-vs-i32 (result i32)
    (block (result i32) (br 0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:577
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-last-void-vs-i64 (result i64)
    (block (result i64) (br 0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:583
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-last-void-vs-f32 (result f32)
    (block (result f32) (br 0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:589
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-last-void-vs-f64 (result f64)
    (block (result f64) (br 0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:596
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-empty-vs-i32 (result i32)
    (block (result i32) (br 0) (i32.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:602
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-empty-vs-i64 (result i64)
    (block (result i64) (br 0) (i64.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:608
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-empty-vs-f32 (result f32)
    (block (result f32) (br 0) (f32.const 1.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:614
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-empty-vs-f64 (result f64)
    (block (result f64) (br 0) (f64.const 1.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:621
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-void-vs-i32 (result i32)
    (block (result i32) (br 0 (nop)) (i32.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:627
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-void-vs-i64 (result i64)
    (block (result i64) (br 0 (nop)) (i64.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:633
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-void-vs-f32 (result f32)
    (block (result f32) (br 0 (nop)) (f32.const 1.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:639
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-void-vs-f64 (result f64)
    (block (result f64) (br 0 (nop)) (f64.const 1.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:646
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-i32-vs-i64 (result i32)
    (block (result i32) (br 0 (i64.const 1)) (i32.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:652
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-i32-vs-f32 (result i32)
    (block (result i32) (br 0 (f32.const 1.0)) (i32.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:658
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-i32-vs-f64 (result i32)
    (block (result i32) (br 0 (f64.const 1.0)) (i32.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:664
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-i64-vs-i32 (result i64)
    (block (result i64) (br 0 (i32.const 1)) (i64.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:670
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-i64-vs-f32 (result i64)
    (block (result i64) (br 0 (f32.const 1.0)) (i64.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:676
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-i64-vs-f64 (result i64)
    (block (result i64) (br 0 (f64.const 1.0)) (i64.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:682
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-f32-vs-i32 (result f32)
    (block (result f32) (br 0 (i32.const 1)) (f32.const 1.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:688
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-f32-vs-i64 (result f32)
    (block (result f32) (br 0 (i64.const 1)) (f32.const 1.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:694
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-f32-vs-f64 (result f32)
    (block (result f32) (br 0 (f64.const 1.0)) (f32.const 1.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:700
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-f64-vs-i32 (result f64)
    (block (result i64) (br 0 (i32.const 1)) (f64.const 1.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:706
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-f64-vs-i64 (result f64)
    (block (result f64) (br 0 (i64.const 1)) (f64.const 1.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:712
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-f64-vs-f32 (result f64)
    (block (result f64) (br 0 (f32.const 1.0)) (f64.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:719
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-void-vs-i32 (result i32)
    (block (result i32) (br 0 (nop)) (br 0 (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:725
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-void-vs-i64 (result i64)
    (block (result i64) (br 0 (nop)) (br 0 (i64.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:731
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-void-vs-f32 (result f32)
    (block (result f32) (br 0 (nop)) (br 0 (f32.const 1.0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:737
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-void-vs-f64 (result f64)
    (block (result f64) (br 0 (nop)) (br 0 (f64.const 1.0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:744
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-i32-vs-i64 (result i32)
    (block (result i32) (br 0 (i64.const 1)) (br 0 (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:750
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-i32-vs-f32 (result i32)
    (block (result i32) (br 0 (f32.const 1.0)) (br 0 (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:756
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-i32-vs-f64 (result i32)
    (block (result i32) (br 0 (f64.const 1.0)) (br 0 (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:762
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-i64-vs-i32 (result i64)
    (block (result i64) (br 0 (i32.const 1)) (br 0 (i64.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:768
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-i64-vs-f32 (result i64)
    (block (result i64) (br 0 (f32.const 1.0)) (br 0 (i64.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:774
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-i64-vs-f64 (result i64)
    (block (result i64) (br 0 (f64.const 1.0)) (br 0 (i64.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:780
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-f32-vs-i32 (result f32)
    (block (result f32) (br 0 (i32.const 1)) (br 0 (f32.const 1.0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:786
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-f32-vs-i64 (result f32)
    (block (result f32) (br 0 (i64.const 1)) (br 0 (f32.const 1.0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:792
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-f32-vs-f64 (result f32)
    (block (result f32) (br 0 (f64.const 1.0)) (br 0 (f32.const 1.0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:798
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-f64-vs-i32 (result f64)
    (block (result f64) (br 0 (i32.const 1)) (br 0 (f64.const 1.0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:804
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-f64-vs-i64 (result f64)
    (block (result f64) (br 0 (i64.const 1)) (br 0 (f64.const 1.0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:810
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-f64-vs-f32 (result f64)
    (block (result f64) (br 0 (f32.const 1.0)) (br 0 (f64.const 1.0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:817
assert_invalid(() =>
  instantiate(`(module (func $$type-break-nested-i32-vs-void
    (block (result i32) (block (result i32) (br 1 (i32.const 1))) (br 0))
  ))`), `type mismatch`);

// ./test/core/block.wast:823
assert_invalid(() =>
  instantiate(`(module (func $$type-break-nested-i64-vs-void
    (block (result i64) (block (result i64) (br 1 (i64.const 1))) (br 0))
  ))`), `type mismatch`);

// ./test/core/block.wast:829
assert_invalid(() =>
  instantiate(`(module (func $$type-break-nested-f32-vs-void
    (block (result f32) (block (result f32) (br 1 (f32.const 1.0))) (br 0))
  ))`), `type mismatch`);

// ./test/core/block.wast:835
assert_invalid(() =>
  instantiate(`(module (func $$type-break-nested-f64-vs-void
    (block (result f64) (block (result f64) (br 1 (f64.const 1.0))) (br 0))
  ))`), `type mismatch`);

// ./test/core/block.wast:842
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-empty-vs-i32 (result i32)
    (block (result i32) (block (br 1)) (br 0 (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:848
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-empty-vs-i64 (result i64)
    (block (result i64) (block (br 1)) (br 0 (i64.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:854
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-empty-vs-f32 (result f32)
    (block (result f32) (block (br 1)) (br 0 (f32.const 1.0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:860
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-empty-vs-f64 (result f64)
    (block (result f64) (block (br 1)) (br 0 (f64.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:867
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-void-vs-i32 (result i32)
    (block (result i32) (block (result i32) (br 1 (nop))) (br 0 (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:873
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-void-vs-i64 (result i64)
    (block (result i64) (block (result i64) (br 1 (nop))) (br 0 (i64.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:879
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-void-vs-f32 (result f32)
    (block (result f32) (block (result f32) (br 1 (nop))) (br 0 (f32.const 1.0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:885
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-void-vs-f64 (result f64)
    (block (result f64) (block (result f64) (br 1 (nop))) (br 0 (f64.const 1.0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:892
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-i32-vs-i64 (result i32)
    (block (result i32)
      (block (result i32) (br 1 (i64.const 1))) (br 0 (i32.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:900
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-i32-vs-f32 (result i32)
    (block (result i32)
      (block (result i32) (br 1 (f32.const 1.0))) (br 0 (i32.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:908
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-i32-vs-f64 (result i32)
    (block (result i32)
      (block (result i32) (br 1 (f64.const 1.0))) (br 0 (i32.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:916
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-i64-vs-i32 (result i64)
    (block (result i64)
      (block (result i64) (br 1 (i32.const 1))) (br 0 (i64.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:924
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-i64-vs-f32 (result i64)
    (block (result i64)
      (block (result i64) (br 1 (f32.const 1.0))) (br 0 (i64.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:932
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-i64-vs-f64 (result i64)
    (block (result i64)
      (block (result i64) (br 1 (f64.const 1.0))) (br 0 (i64.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:940
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-f32-vs-i32 (result f32)
    (block (result f32)
      (block (result f32) (br 1 (i32.const 1))) (br 0 (f32.const 1.0))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:948
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-f32-vs-i64 (result f32)
    (block (result f32)
      (block (result f32) (br 1 (i64.const 1))) (br 0 (f32.const 1.0))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:956
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-f32-vs-f64 (result f32)
    (block (result f32)
      (block (result f32) (br 1 (f64.const 1.0))) (br 0 (f32.const 1.0))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:964
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-f64-vs-i32 (result f64)
    (block (result f64)
      (block (result f64) (br 1 (i32.const 1))) (br 0 (f64.const 1.0))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:972
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-f64-vs-i64 (result f64)
    (block (result f64)
      (block (result f64) (br 1 (i64.const 1))) (br 0 (f64.const 1.0))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:980
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-f64-vs-f32 (result f64)
    (block (result f64)
      (block (result f64) (br 1 (f32.const 1.0))) (br 0 (f64.const 1.0))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:989
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-empty-vs-i32 (result i32)
    (i32.ctz (block (br 0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:995
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-empty-vs-i64 (result i64)
    (i64.ctz (block (br 0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1001
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-empty-vs-f32 (result f32)
    (f32.floor (block (br 0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1007
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-empty-vs-f64 (result f64)
    (f64.floor (block (br 0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1014
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-void-vs-i32 (result i32)
    (i32.ctz (block (br 0 (nop))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1020
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-void-vs-i64 (result i64)
    (i64.ctz (block (br 0 (nop))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1026
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-void-vs-f32 (result f32)
    (f32.floor (block (br 0 (nop))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1032
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-void-vs-f64 (result f64)
    (f64.floor (block (br 0 (nop))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1039
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-i32-vs-i64 (result i32)
    (i64.ctz (block (br 0 (i64.const 9))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1045
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-i32-vs-f32 (result i32)
    (f32.floor (block (br 0 (f32.const 9.0))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1051
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-i32-vs-f64 (result i32)
    (f64.floor (block (br 0 (f64.const 9.0))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1057
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-i64-vs-i32 (result i64)
    (i32.ctz (block (br 0 (i32.const 9))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1063
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-i64-vs-f32 (result i64)
    (f32.floor (block (br 0 (f32.const 9.0))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1069
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-i64-vs-f64 (result i64)
    (f64.floor (block (br 0 (f64.const 9.0))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1075
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-f32-vs-i32 (result f32)
    (i32.ctz (block (br 0 (i32.const 9))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1081
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-f32-vs-i64 (result f32)
    (i64.ctz (block (br 0 (i64.const 9))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1087
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-f32-vs-f64 (result f32)
    (f64.floor (block (br 0 (f64.const 9.0))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1093
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-f64-vs-i32 (result f64)
    (i32.ctz (block (br 0 (i32.const 9))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1099
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-f64-vs-i64 (result f64)
    (i64.ctz (block (br 0 (i64.const 9))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1105
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-f64-vs-f32 (result f64)
    (f32.floor (block (br 0 (f32.const 9.0))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1113
assert_malformed(
  () => instantiate(`(func block end $$l) `),
  `mismatching label`,
);

// ./test/core/block.wast:1117
assert_malformed(
  () => instantiate(`(func block $$a end $$l) `),
  `mismatching label`,
);
