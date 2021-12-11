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
    (block (result i32)
      (call $$dummy) (call $$dummy) (call $$dummy) (i32.const 7) (call $$dummy)
    )
    (drop)
    (block (result i32 i64 i32)
      (call $$dummy) (call $$dummy) (call $$dummy) (i32.const 8) (call $$dummy)
      (call $$dummy) (call $$dummy) (call $$dummy) (i64.const 7) (call $$dummy)
      (call $$dummy) (call $$dummy) (call $$dummy) (i32.const 9) (call $$dummy)
    )
    (drop) (drop)
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
  (func (export "as-binary-operands") (result i32)
    (i32.mul
      (block (result i32 i32)
        (call $$dummy) (i32.const 3) (call $$dummy) (i32.const 4)
      )
    )
  )
  (func (export "as-compare-operands") (result i32)
    (f32.gt
      (block (result f32 f32)
        (call $$dummy) (f32.const 3) (call $$dummy) (f32.const 3)
      )
    )
  )
  (func (export "as-mixed-operands") (result i32)
    (block (result i32 i32)
      (call $$dummy) (i32.const 3) (call $$dummy) (i32.const 4)
    )
    (i32.const 5)
    (i32.add)
    (i32.mul)
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
  (func (export "break-multi-value") (result i32 i32 i64)
    (block (result i32 i32 i64)
      (br 0 (i32.const 18) (i32.const -18) (i64.const 18))
      (i32.const 19) (i32.const -19) (i64.const 19)
    )
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

  (func (export "param") (result i32)
    (i32.const 1)
    (block (param i32) (result i32)
      (i32.const 2)
      (i32.add)
    )
  )
  (func (export "params") (result i32)
    (i32.const 1)
    (i32.const 2)
    (block (param i32 i32) (result i32)
      (i32.add)
    )
  )
  (func (export "params-id") (result i32)
    (i32.const 1)
    (i32.const 2)
    (block (param i32 i32) (result i32 i32))
    (i32.add)
  )
  (func (export "param-break") (result i32)
    (i32.const 1)
    (block (param i32) (result i32)
      (i32.const 2)
      (i32.add)
      (br 0)
    )
  )
  (func (export "params-break") (result i32)
    (i32.const 1)
    (i32.const 2)
    (block (param i32 i32) (result i32)
      (i32.add)
      (br 0)
    )
  )
  (func (export "params-id-break") (result i32)
    (i32.const 1)
    (i32.const 2)
    (block (param i32 i32) (result i32 i32) (br 0))
    (i32.add)
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

  (type $$block-sig-1 (func))
  (type $$block-sig-2 (func (result i32)))
  (type $$block-sig-3 (func (param $$x i32)))
  (type $$block-sig-4 (func (param i32 f64 i32) (result i32 f64 i32)))

  (func (export "type-use")
    (block (type $$block-sig-1))
    (block (type $$block-sig-2) (i32.const 0))
    (block (type $$block-sig-3) (drop))
    (i32.const 0) (f64.const 0) (i32.const 0)
    (block (type $$block-sig-4))
    (drop) (drop) (drop)
    (block (type $$block-sig-2) (result i32) (i32.const 0))
    (block (type $$block-sig-3) (param i32) (drop))
    (i32.const 0) (f64.const 0) (i32.const 0)
    (block (type $$block-sig-4)
      (param i32) (param f64 i32) (result i32 f64) (result i32)
    )
    (drop) (drop) (drop)
  )
)`);

// ./test/core/block.wast:353
assert_return(() => invoke($0, `empty`, []), []);

// ./test/core/block.wast:354
assert_return(() => invoke($0, `singular`, []), [value("i32", 7)]);

// ./test/core/block.wast:355
assert_return(() => invoke($0, `multi`, []), [value("i32", 8)]);

// ./test/core/block.wast:356
assert_return(() => invoke($0, `nested`, []), [value("i32", 9)]);

// ./test/core/block.wast:357
assert_return(() => invoke($0, `deep`, []), [value("i32", 150)]);

// ./test/core/block.wast:359
assert_return(() => invoke($0, `as-select-first`, []), [value("i32", 1)]);

// ./test/core/block.wast:360
assert_return(() => invoke($0, `as-select-mid`, []), [value("i32", 2)]);

// ./test/core/block.wast:361
assert_return(() => invoke($0, `as-select-last`, []), [value("i32", 2)]);

// ./test/core/block.wast:363
assert_return(() => invoke($0, `as-loop-first`, []), [value("i32", 1)]);

// ./test/core/block.wast:364
assert_return(() => invoke($0, `as-loop-mid`, []), [value("i32", 1)]);

// ./test/core/block.wast:365
assert_return(() => invoke($0, `as-loop-last`, []), [value("i32", 1)]);

// ./test/core/block.wast:367
assert_return(() => invoke($0, `as-if-condition`, []), []);

// ./test/core/block.wast:368
assert_return(() => invoke($0, `as-if-then`, []), [value("i32", 1)]);

// ./test/core/block.wast:369
assert_return(() => invoke($0, `as-if-else`, []), [value("i32", 2)]);

// ./test/core/block.wast:371
assert_return(() => invoke($0, `as-br_if-first`, []), [value("i32", 1)]);

// ./test/core/block.wast:372
assert_return(() => invoke($0, `as-br_if-last`, []), [value("i32", 2)]);

// ./test/core/block.wast:374
assert_return(() => invoke($0, `as-br_table-first`, []), [value("i32", 1)]);

// ./test/core/block.wast:375
assert_return(() => invoke($0, `as-br_table-last`, []), [value("i32", 2)]);

// ./test/core/block.wast:377
assert_return(() => invoke($0, `as-call_indirect-first`, []), [
  value("i32", 1),
]);

// ./test/core/block.wast:378
assert_return(() => invoke($0, `as-call_indirect-mid`, []), [value("i32", 2)]);

// ./test/core/block.wast:379
assert_return(() => invoke($0, `as-call_indirect-last`, []), [value("i32", 1)]);

// ./test/core/block.wast:381
assert_return(() => invoke($0, `as-store-first`, []), []);

// ./test/core/block.wast:382
assert_return(() => invoke($0, `as-store-last`, []), []);

// ./test/core/block.wast:384
assert_return(() => invoke($0, `as-memory.grow-value`, []), [value("i32", 1)]);

// ./test/core/block.wast:385
assert_return(() => invoke($0, `as-call-value`, []), [value("i32", 1)]);

// ./test/core/block.wast:386
assert_return(() => invoke($0, `as-return-value`, []), [value("i32", 1)]);

// ./test/core/block.wast:387
assert_return(() => invoke($0, `as-drop-operand`, []), []);

// ./test/core/block.wast:388
assert_return(() => invoke($0, `as-br-value`, []), [value("i32", 1)]);

// ./test/core/block.wast:389
assert_return(() => invoke($0, `as-local.set-value`, []), [value("i32", 1)]);

// ./test/core/block.wast:390
assert_return(() => invoke($0, `as-local.tee-value`, []), [value("i32", 1)]);

// ./test/core/block.wast:391
assert_return(() => invoke($0, `as-global.set-value`, []), [value("i32", 1)]);

// ./test/core/block.wast:392
assert_return(() => invoke($0, `as-load-operand`, []), [value("i32", 1)]);

// ./test/core/block.wast:394
assert_return(() => invoke($0, `as-unary-operand`, []), [value("i32", 0)]);

// ./test/core/block.wast:395
assert_return(() => invoke($0, `as-binary-operand`, []), [value("i32", 12)]);

// ./test/core/block.wast:396
assert_return(() => invoke($0, `as-test-operand`, []), [value("i32", 0)]);

// ./test/core/block.wast:397
assert_return(() => invoke($0, `as-compare-operand`, []), [value("i32", 0)]);

// ./test/core/block.wast:398
assert_return(() => invoke($0, `as-binary-operands`, []), [value("i32", 12)]);

// ./test/core/block.wast:399
assert_return(() => invoke($0, `as-compare-operands`, []), [value("i32", 0)]);

// ./test/core/block.wast:400
assert_return(() => invoke($0, `as-mixed-operands`, []), [value("i32", 27)]);

// ./test/core/block.wast:402
assert_return(() => invoke($0, `break-bare`, []), [value("i32", 19)]);

// ./test/core/block.wast:403
assert_return(() => invoke($0, `break-value`, []), [value("i32", 18)]);

// ./test/core/block.wast:404
assert_return(() => invoke($0, `break-multi-value`, []), [
  value("i32", 18),
  value("i32", -18),
  value("i64", 18n),
]);

// ./test/core/block.wast:407
assert_return(() => invoke($0, `break-repeated`, []), [value("i32", 18)]);

// ./test/core/block.wast:408
assert_return(() => invoke($0, `break-inner`, []), [value("i32", 15)]);

// ./test/core/block.wast:410
assert_return(() => invoke($0, `param`, []), [value("i32", 3)]);

// ./test/core/block.wast:411
assert_return(() => invoke($0, `params`, []), [value("i32", 3)]);

// ./test/core/block.wast:412
assert_return(() => invoke($0, `params-id`, []), [value("i32", 3)]);

// ./test/core/block.wast:413
assert_return(() => invoke($0, `param-break`, []), [value("i32", 3)]);

// ./test/core/block.wast:414
assert_return(() => invoke($0, `params-break`, []), [value("i32", 3)]);

// ./test/core/block.wast:415
assert_return(() => invoke($0, `params-id-break`, []), [value("i32", 3)]);

// ./test/core/block.wast:417
assert_return(() => invoke($0, `effects`, []), [value("i32", 1)]);

// ./test/core/block.wast:419
assert_return(() => invoke($0, `type-use`, []), []);

// ./test/core/block.wast:421
assert_malformed(
  () =>
    instantiate(
      `(type $$sig (func (param i32) (result i32))) (func (i32.const 0) (block (type $$sig) (result i32) (param i32))) `,
    ),
  `unexpected token`,
);

// ./test/core/block.wast:428
assert_malformed(
  () =>
    instantiate(
      `(type $$sig (func (param i32) (result i32))) (func (i32.const 0) (block (param i32) (type $$sig) (result i32))) `,
    ),
  `unexpected token`,
);

// ./test/core/block.wast:435
assert_malformed(
  () =>
    instantiate(
      `(type $$sig (func (param i32) (result i32))) (func (i32.const 0) (block (param i32) (result i32) (type $$sig))) `,
    ),
  `unexpected token`,
);

// ./test/core/block.wast:442
assert_malformed(
  () =>
    instantiate(
      `(type $$sig (func (param i32) (result i32))) (func (i32.const 0) (block (result i32) (type $$sig) (param i32))) `,
    ),
  `unexpected token`,
);

// ./test/core/block.wast:449
assert_malformed(
  () =>
    instantiate(
      `(type $$sig (func (param i32) (result i32))) (func (i32.const 0) (block (result i32) (param i32) (type $$sig))) `,
    ),
  `unexpected token`,
);

// ./test/core/block.wast:456
assert_malformed(
  () => instantiate(`(func (i32.const 0) (block (result i32) (param i32))) `),
  `unexpected token`,
);

// ./test/core/block.wast:463
assert_malformed(
  () => instantiate(`(func (i32.const 0) (block (param $$x i32) (drop))) `),
  `unexpected token`,
);

// ./test/core/block.wast:467
assert_malformed(
  () =>
    instantiate(
      `(type $$sig (func)) (func (block (type $$sig) (result i32) (i32.const 0)) (unreachable)) `,
    ),
  `inline function type`,
);

// ./test/core/block.wast:474
assert_malformed(
  () =>
    instantiate(
      `(type $$sig (func (param i32) (result i32))) (func (block (type $$sig) (result i32) (i32.const 0)) (unreachable)) `,
    ),
  `inline function type`,
);

// ./test/core/block.wast:481
assert_malformed(
  () =>
    instantiate(
      `(type $$sig (func (param i32) (result i32))) (func (i32.const 0) (block (type $$sig) (param i32) (drop)) (unreachable)) `,
    ),
  `inline function type`,
);

// ./test/core/block.wast:488
assert_malformed(
  () =>
    instantiate(
      `(type $$sig (func (param i32 i32) (result i32))) (func (i32.const 0) (block (type $$sig) (param i32) (result i32)) (unreachable)) `,
    ),
  `inline function type`,
);

// ./test/core/block.wast:496
assert_invalid(() =>
  instantiate(`(module
    (type $$sig (func))
    (func (block (type $$sig) (i32.const 0)))
  )`), `type mismatch`);

// ./test/core/block.wast:504
assert_invalid(
  () => instantiate(`(module (func $$type-empty-i32 (result i32) (block)))`),
  `type mismatch`,
);

// ./test/core/block.wast:508
assert_invalid(
  () => instantiate(`(module (func $$type-empty-i64 (result i64) (block)))`),
  `type mismatch`,
);

// ./test/core/block.wast:512
assert_invalid(
  () => instantiate(`(module (func $$type-empty-f32 (result f32) (block)))`),
  `type mismatch`,
);

// ./test/core/block.wast:516
assert_invalid(
  () => instantiate(`(module (func $$type-empty-f64 (result f64) (block)))`),
  `type mismatch`,
);

// ./test/core/block.wast:521
assert_invalid(() =>
  instantiate(`(module (func $$type-value-i32-vs-void
    (block (i32.const 1))
  ))`), `type mismatch`);

// ./test/core/block.wast:527
assert_invalid(() =>
  instantiate(`(module (func $$type-value-i64-vs-void
    (block (i64.const 1))
  ))`), `type mismatch`);

// ./test/core/block.wast:533
assert_invalid(() =>
  instantiate(`(module (func $$type-value-f32-vs-void
    (block (f32.const 1.0))
  ))`), `type mismatch`);

// ./test/core/block.wast:539
assert_invalid(() =>
  instantiate(`(module (func $$type-value-f64-vs-void
    (block (f64.const 1.0))
  ))`), `type mismatch`);

// ./test/core/block.wast:545
assert_invalid(() =>
  instantiate(`(module (func $$type-value-nums-vs-void
    (block (i32.const 1) (i32.const 2))
  ))`), `type mismatch`);

// ./test/core/block.wast:551
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-empty-vs-i32 (result i32)
    (block (result i32))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:557
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-empty-vs-i64 (result i64)
    (block (result i64))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:563
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-empty-vs-f32 (result f32)
    (block (result f32))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:569
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-empty-vs-f64 (result f64)
    (block (result f64))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:575
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-empty-vs-nums (result i32 i32)
    (block (result i32 i32))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:582
assert_invalid(() =>
  instantiate(`(module
    (func $$type-value-empty-in-block
      (i32.const 0)
      (block (block (result i32)) (drop))
    )
  )`), `type mismatch`);

// ./test/core/block.wast:591
assert_invalid(() =>
  instantiate(`(module
    (func $$type-value-empty-in-loop
      (i32.const 0)
      (loop (block (result i32)) (drop))
    )
  )`), `type mismatch`);

// ./test/core/block.wast:600
assert_invalid(() =>
  instantiate(`(module
    (func $$type-value-empty-in-then
      (i32.const 0) (i32.const 0)
      (if (then (block (result i32)) (drop)))
    )
  )`), `type mismatch`);

// ./test/core/block.wast:610
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-void-vs-i32 (result i32)
    (block (result i32) (nop))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:616
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-void-vs-i64 (result i64)
    (block (result i64) (nop))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:622
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-void-vs-f32 (result f32)
    (block (result f32) (nop))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:628
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-void-vs-f64 (result f64)
    (block (result f64) (nop))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:634
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-void-vs-nums (result i32 i32)
    (block (result i32 i32) (nop))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:640
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-i32-vs-i64 (result i32)
    (block (result i32) (i64.const 0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:646
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-i32-vs-f32 (result i32)
    (block (result i32) (f32.const 0.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:652
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-i32-vs-f64 (result i32)
    (block (result i32) (f64.const 0.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:658
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-i64-vs-i32 (result i64)
    (block (result i64) (i32.const 0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:664
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-i64-vs-f32 (result i64)
    (block (result i64) (f32.const 0.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:670
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-i64-vs-f64 (result i64)
    (block (result i64) (f64.const 0.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:676
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-f32-vs-i32 (result f32)
    (block (result f32) (i32.const 0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:682
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-f32-vs-i64 (result f32)
    (block (result f32) (i64.const 0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:688
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-f32-vs-f64 (result f32)
    (block (result f32) (f64.const 0.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:694
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-f64-vs-i32 (result f64)
    (block (result f64) (i32.const 0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:700
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-f64-vs-i64 (result f64)
    (block (result f64) (i64.const 0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:706
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-f64-vs-f32 (result f32)
    (block (result f64) (f32.const 0.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:712
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-num-vs-nums (result i32 i32)
    (block (result i32 i32) (i32.const 0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:718
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-partial-vs-nums (result i32 i32)
    (i32.const 1) (block (result i32 i32) (i32.const 2))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:724
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-nums-vs-num (result i32)
    (block (result i32) (i32.const 1) (i32.const 2))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:731
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-value-unreached-select-i32-i64 (result i32)
    (block (result i64) (select (unreachable) (unreachable) (unreachable)))
  ))`,
    ),
  `type mismatch`,
);

// ./test/core/block.wast:737
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-value-unreached-select-i32-f32 (result i32)
    (block (result f32) (select (unreachable) (unreachable) (unreachable)))
  ))`,
    ),
  `type mismatch`,
);

// ./test/core/block.wast:743
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-value-unreached-select-i32-f64 (result i32)
    (block (result f64) (select (unreachable) (unreachable) (unreachable)))
  ))`,
    ),
  `type mismatch`,
);

// ./test/core/block.wast:749
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-value-unreached-select-i64-i32 (result i64)
    (block (result i32) (select (unreachable) (unreachable) (unreachable)))
  ))`,
    ),
  `type mismatch`,
);

// ./test/core/block.wast:755
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-value-unreached-select-i64-f32 (result i64)
    (block (result f32) (select (unreachable) (unreachable) (unreachable)))
  ))`,
    ),
  `type mismatch`,
);

// ./test/core/block.wast:761
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-value-unreached-select-i64-f64 (result i64)
    (block (result f64) (select (unreachable) (unreachable) (unreachable)))
  ))`,
    ),
  `type mismatch`,
);

// ./test/core/block.wast:767
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-value-unreached-select-f32-i32 (result f32)
    (block (result i32) (select (unreachable) (unreachable) (unreachable)))
  ))`,
    ),
  `type mismatch`,
);

// ./test/core/block.wast:773
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-value-unreached-select-f32-i64 (result f32)
    (block (result i64) (select (unreachable) (unreachable) (unreachable)))
  ))`,
    ),
  `type mismatch`,
);

// ./test/core/block.wast:779
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-value-unreached-select-f32-f64 (result f32)
    (block (result f64) (select (unreachable) (unreachable) (unreachable)))
  ))`,
    ),
  `type mismatch`,
);

// ./test/core/block.wast:785
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-value-unreached-select-f64-i32 (result f64)
    (block (result i32) (select (unreachable) (unreachable) (unreachable)))
  ))`,
    ),
  `type mismatch`,
);

// ./test/core/block.wast:791
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-value-unreached-select-f64-i64 (result f64)
    (block (result i64) (select (unreachable) (unreachable) (unreachable)))
  ))`,
    ),
  `type mismatch`,
);

// ./test/core/block.wast:797
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-value-unreached-select-f64-f32 (result f64)
    (block (result f32) (select (unreachable) (unreachable) (unreachable)))
  ))`,
    ),
  `type mismatch`,
);

// ./test/core/block.wast:804
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-last-void-vs-i32 (result i32)
    (block (result i32) (br 0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:810
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-last-void-vs-i64 (result i64)
    (block (result i64) (br 0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:816
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-last-void-vs-f32 (result f32)
    (block (result f32) (br 0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:822
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-last-void-vs-f64 (result f64)
    (block (result f64) (br 0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:828
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-last-void-vs-nums (result i32 i32)
    (block (result i32 i32) (br 0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:835
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-empty-vs-i32 (result i32)
    (block (result i32) (br 0) (i32.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:841
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-empty-vs-i64 (result i64)
    (block (result i64) (br 0) (i64.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:847
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-empty-vs-f32 (result f32)
    (block (result f32) (br 0) (f32.const 1.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:853
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-empty-vs-f64 (result f64)
    (block (result f64) (br 0) (f64.const 1.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:859
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-empty-vs-nums (result i32 i32)
    (block (result i32 i32) (br 0) (i32.const 1) (i32.const 2))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:866
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-void-vs-i32 (result i32)
    (block (result i32) (br 0 (nop)) (i32.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:872
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-void-vs-i64 (result i64)
    (block (result i64) (br 0 (nop)) (i64.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:878
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-void-vs-f32 (result f32)
    (block (result f32) (br 0 (nop)) (f32.const 1.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:884
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-void-vs-f64 (result f64)
    (block (result f64) (br 0 (nop)) (f64.const 1.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:891
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-i32-vs-i64 (result i32)
    (block (result i32) (br 0 (i64.const 1)) (i32.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:897
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-i32-vs-f32 (result i32)
    (block (result i32) (br 0 (f32.const 1.0)) (i32.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:903
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-i32-vs-f64 (result i32)
    (block (result i32) (br 0 (f64.const 1.0)) (i32.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:909
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-i64-vs-i32 (result i64)
    (block (result i64) (br 0 (i32.const 1)) (i64.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:915
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-i64-vs-f32 (result i64)
    (block (result i64) (br 0 (f32.const 1.0)) (i64.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:921
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-i64-vs-f64 (result i64)
    (block (result i64) (br 0 (f64.const 1.0)) (i64.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:927
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-f32-vs-i32 (result f32)
    (block (result f32) (br 0 (i32.const 1)) (f32.const 1.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:933
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-f32-vs-i64 (result f32)
    (block (result f32) (br 0 (i64.const 1)) (f32.const 1.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:939
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-f32-vs-f64 (result f32)
    (block (result f32) (br 0 (f64.const 1.0)) (f32.const 1.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:945
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-f64-vs-i32 (result f64)
    (block (result i64) (br 0 (i32.const 1)) (f64.const 1.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:951
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-f64-vs-i64 (result f64)
    (block (result f64) (br 0 (i64.const 1)) (f64.const 1.0))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:957
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-f64-vs-f32 (result f64)
    (block (result f64) (br 0 (f32.const 1.0)) (f64.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:963
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-num-vs-nums (result i32 i32)
    (block (result i32 i32) (br 0 (i32.const 0)) (i32.const 1) (i32.const 2))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:969
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-partial-vs-nums (result i32 i32)
    (i32.const 1) (block (result i32 i32) (br 0 (i32.const 0)) (i32.const 2))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:976
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-void-vs-i32 (result i32)
    (block (result i32) (br 0 (nop)) (br 0 (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:982
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-void-vs-i64 (result i64)
    (block (result i64) (br 0 (nop)) (br 0 (i64.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:988
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-void-vs-f32 (result f32)
    (block (result f32) (br 0 (nop)) (br 0 (f32.const 1.0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:994
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-void-vs-f64 (result f64)
    (block (result f64) (br 0 (nop)) (br 0 (f64.const 1.0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1000
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-void-vs-nums (result i32 i32)
    (block (result i32 i32) (br 0 (nop)) (br 0 (i32.const 1) (i32.const 2)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1007
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-i32-vs-i64 (result i32)
    (block (result i32) (br 0 (i64.const 1)) (br 0 (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1013
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-i32-vs-f32 (result i32)
    (block (result i32) (br 0 (f32.const 1.0)) (br 0 (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1019
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-i32-vs-f64 (result i32)
    (block (result i32) (br 0 (f64.const 1.0)) (br 0 (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1025
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-i64-vs-i32 (result i64)
    (block (result i64) (br 0 (i32.const 1)) (br 0 (i64.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1031
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-i64-vs-f32 (result i64)
    (block (result i64) (br 0 (f32.const 1.0)) (br 0 (i64.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1037
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-i64-vs-f64 (result i64)
    (block (result i64) (br 0 (f64.const 1.0)) (br 0 (i64.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1043
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-f32-vs-i32 (result f32)
    (block (result f32) (br 0 (i32.const 1)) (br 0 (f32.const 1.0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1049
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-f32-vs-i64 (result f32)
    (block (result f32) (br 0 (i64.const 1)) (br 0 (f32.const 1.0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1055
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-f32-vs-f64 (result f32)
    (block (result f32) (br 0 (f64.const 1.0)) (br 0 (f32.const 1.0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1061
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-f64-vs-i32 (result f64)
    (block (result f64) (br 0 (i32.const 1)) (br 0 (f64.const 1.0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1067
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-f64-vs-i64 (result f64)
    (block (result f64) (br 0 (i64.const 1)) (br 0 (f64.const 1.0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1073
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-f64-vs-f32 (result f64)
    (block (result f64) (br 0 (f32.const 1.0)) (br 0 (f64.const 1.0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1079
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-num-vs-nums (result i32 i32)
    (block (result i32 i32) (br 0 (i32.const 0)) (br 0 (i32.const 1) (i32.const 2)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1086
assert_invalid(() =>
  instantiate(`(module (func $$type-break-nested-i32-vs-void
    (block (result i32) (block (result i32) (br 1 (i32.const 1))) (br 0))
  ))`), `type mismatch`);

// ./test/core/block.wast:1092
assert_invalid(() =>
  instantiate(`(module (func $$type-break-nested-i64-vs-void
    (block (result i64) (block (result i64) (br 1 (i64.const 1))) (br 0))
  ))`), `type mismatch`);

// ./test/core/block.wast:1098
assert_invalid(() =>
  instantiate(`(module (func $$type-break-nested-f32-vs-void
    (block (result f32) (block (result f32) (br 1 (f32.const 1.0))) (br 0))
  ))`), `type mismatch`);

// ./test/core/block.wast:1104
assert_invalid(() =>
  instantiate(`(module (func $$type-break-nested-f64-vs-void
    (block (result f64) (block (result f64) (br 1 (f64.const 1.0))) (br 0))
  ))`), `type mismatch`);

// ./test/core/block.wast:1110
assert_invalid(() =>
  instantiate(`(module (func $$type-break-nested-nums-vs-void
    (block (result i32 i32) (block (result i32 i32) (br 1 (i32.const 1) (i32.const 2))) (br 0))
  ))`), `type mismatch`);

// ./test/core/block.wast:1117
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-empty-vs-i32 (result i32)
    (block (result i32) (block (br 1)) (br 0 (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1123
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-empty-vs-i64 (result i64)
    (block (result i64) (block (br 1)) (br 0 (i64.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1129
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-empty-vs-f32 (result f32)
    (block (result f32) (block (br 1)) (br 0 (f32.const 1.0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1135
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-empty-vs-f64 (result f64)
    (block (result f64) (block (br 1)) (br 0 (f64.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1141
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-break-nested-empty-vs-nums (result i32 i32)
    (block (result i32 i32) (block (br 1)) (br 0 (i32.const 1) (i32.const 2)))
  ))`,
    ),
  `type mismatch`,
);

// ./test/core/block.wast:1148
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-void-vs-i32 (result i32)
    (block (result i32) (block (result i32) (br 1 (nop))) (br 0 (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1154
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-void-vs-i64 (result i64)
    (block (result i64) (block (result i64) (br 1 (nop))) (br 0 (i64.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1160
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-void-vs-f32 (result f32)
    (block (result f32) (block (result f32) (br 1 (nop))) (br 0 (f32.const 1.0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1166
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-void-vs-f64 (result f64)
    (block (result f64) (block (result f64) (br 1 (nop))) (br 0 (f64.const 1.0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1172
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-void-vs-nums (result i32 i32)
    (block (result i32 i32) (block (result i32 i32) (br 1 (nop))) (br 0 (i32.const 1) (i32.const 2)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1179
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-i32-vs-i64 (result i32)
    (block (result i32)
      (block (result i32) (br 1 (i64.const 1))) (br 0 (i32.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1187
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-i32-vs-f32 (result i32)
    (block (result i32)
      (block (result i32) (br 1 (f32.const 1.0))) (br 0 (i32.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1195
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-i32-vs-f64 (result i32)
    (block (result i32)
      (block (result i32) (br 1 (f64.const 1.0))) (br 0 (i32.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1203
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-i64-vs-i32 (result i64)
    (block (result i64)
      (block (result i64) (br 1 (i32.const 1))) (br 0 (i64.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1211
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-i64-vs-f32 (result i64)
    (block (result i64)
      (block (result i64) (br 1 (f32.const 1.0))) (br 0 (i64.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1219
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-i64-vs-f64 (result i64)
    (block (result i64)
      (block (result i64) (br 1 (f64.const 1.0))) (br 0 (i64.const 1))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1227
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-f32-vs-i32 (result f32)
    (block (result f32)
      (block (result f32) (br 1 (i32.const 1))) (br 0 (f32.const 1.0))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1235
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-f32-vs-i64 (result f32)
    (block (result f32)
      (block (result f32) (br 1 (i64.const 1))) (br 0 (f32.const 1.0))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1243
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-f32-vs-f64 (result f32)
    (block (result f32)
      (block (result f32) (br 1 (f64.const 1.0))) (br 0 (f32.const 1.0))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1251
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-f64-vs-i32 (result f64)
    (block (result f64)
      (block (result f64) (br 1 (i32.const 1))) (br 0 (f64.const 1.0))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1259
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-f64-vs-i64 (result f64)
    (block (result f64)
      (block (result f64) (br 1 (i64.const 1))) (br 0 (f64.const 1.0))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1267
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-f64-vs-f32 (result f64)
    (block (result f64)
      (block (result f64) (br 1 (f32.const 1.0))) (br 0 (f64.const 1.0))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1275
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-num-vs-nums (result i32 i32)
    (block (result i32 i32)
      (block (result i32 i32) (br 1 (i32.const 0))) (br 0 (i32.const 1) (i32.const 2))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1284
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-empty-vs-i32 (result i32)
    (i32.ctz (block (br 0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1290
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-empty-vs-i64 (result i64)
    (i64.ctz (block (br 0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1296
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-empty-vs-f32 (result f32)
    (f32.floor (block (br 0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1302
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-empty-vs-f64 (result f64)
    (f64.floor (block (br 0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1308
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-empty-vs-nums (result i32)
    (i32.add (block (br 0)))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1315
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-void-vs-i32 (result i32)
    (i32.ctz (block (br 0 (nop))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1321
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-void-vs-i64 (result i64)
    (i64.ctz (block (br 0 (nop))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1327
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-void-vs-f32 (result f32)
    (f32.floor (block (br 0 (nop))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1333
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-void-vs-f64 (result f64)
    (f64.floor (block (br 0 (nop))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1339
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-void-vs-nums (result i32)
    (i32.add (block (br 0 (nop))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1346
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-i32-vs-i64 (result i32)
    (i64.ctz (block (br 0 (i64.const 9))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1352
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-i32-vs-f32 (result i32)
    (f32.floor (block (br 0 (f32.const 9.0))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1358
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-i32-vs-f64 (result i32)
    (f64.floor (block (br 0 (f64.const 9.0))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1364
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-i64-vs-i32 (result i64)
    (i32.ctz (block (br 0 (i32.const 9))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1370
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-i64-vs-f32 (result i64)
    (f32.floor (block (br 0 (f32.const 9.0))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1376
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-i64-vs-f64 (result i64)
    (f64.floor (block (br 0 (f64.const 9.0))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1382
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-f32-vs-i32 (result f32)
    (i32.ctz (block (br 0 (i32.const 9))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1388
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-f32-vs-i64 (result f32)
    (i64.ctz (block (br 0 (i64.const 9))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1394
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-f32-vs-f64 (result f32)
    (f64.floor (block (br 0 (f64.const 9.0))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1400
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-f64-vs-i32 (result f64)
    (i32.ctz (block (br 0 (i32.const 9))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1406
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-f64-vs-i64 (result f64)
    (i64.ctz (block (br 0 (i64.const 9))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1412
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-f64-vs-f32 (result f64)
    (f32.floor (block (br 0 (f32.const 9.0))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1418
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-operand-num-vs-nums (result i32)
    (i32.add (block (br 0 (i64.const 9) (i32.const 10))))
  ))`),
  `type mismatch`,
);

// ./test/core/block.wast:1425
assert_invalid(() =>
  instantiate(`(module (func $$type-param-void-vs-num
    (block (param i32) (drop))
  ))`), `type mismatch`);

// ./test/core/block.wast:1431
assert_invalid(() =>
  instantiate(`(module (func $$type-param-void-vs-nums
    (block (param i32 f64) (drop) (drop))
  ))`), `type mismatch`);

// ./test/core/block.wast:1437
assert_invalid(() =>
  instantiate(`(module (func $$type-param-num-vs-num
    (f32.const 0) (block (param i32) (drop))
  ))`), `type mismatch`);

// ./test/core/block.wast:1443
assert_invalid(() =>
  instantiate(`(module (func $$type-param-num-vs-nums
    (f32.const 0) (block (param f32 i32) (drop) (drop))
  ))`), `type mismatch`);

// ./test/core/block.wast:1449
assert_invalid(() =>
  instantiate(`(module (func $$type-param-nested-void-vs-num
    (block (block (param i32) (drop)))
  ))`), `type mismatch`);

// ./test/core/block.wast:1455
assert_invalid(() =>
  instantiate(`(module (func $$type-param-void-vs-nums
    (block (block (param i32 f64) (drop) (drop)))
  ))`), `type mismatch`);

// ./test/core/block.wast:1461
assert_invalid(() =>
  instantiate(`(module (func $$type-param-num-vs-num
    (block (f32.const 0) (block (param i32) (drop)))
  ))`), `type mismatch`);

// ./test/core/block.wast:1467
assert_invalid(() =>
  instantiate(`(module (func $$type-param-num-vs-nums
    (block (f32.const 0) (block (param f32 i32) (drop) (drop)))
  ))`), `type mismatch`);

// ./test/core/block.wast:1474
assert_malformed(
  () =>
    instantiate(`(func (param i32) (result i32) block (param $$x i32) end) `),
  `unexpected token`,
);

// ./test/core/block.wast:1478
assert_malformed(
  () => instantiate(`(func (param i32) (result i32) (block (param $$x i32))) `),
  `unexpected token`,
);

// ./test/core/block.wast:1484
assert_malformed(
  () => instantiate(`(func block end $$l) `),
  `mismatching label`,
);

// ./test/core/block.wast:1488
assert_malformed(
  () => instantiate(`(func block $$a end $$l) `),
  `mismatching label`,
);
