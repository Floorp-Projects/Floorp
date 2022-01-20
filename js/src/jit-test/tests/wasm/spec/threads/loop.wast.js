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

// ./test/core/loop.wast

// ./test/core/loop.wast:3
let $0 = instantiate(`(module
  (memory 1)

  (func $$dummy)

  (func (export "empty")
    (loop)
    (loop $$l)
  )

  (func (export "singular") (result i32)
    (loop (nop))
    (loop (result i32) (i32.const 7))
  )

  (func (export "multi") (result i32)
    (loop (call $$dummy) (call $$dummy) (call $$dummy) (call $$dummy))
    (loop (result i32) (call $$dummy) (call $$dummy) (call $$dummy) (i32.const 8))
  )

  (func (export "nested") (result i32)
    (loop (result i32)
      (loop (call $$dummy) (block) (nop))
      (loop (result i32) (call $$dummy) (i32.const 9))
    )
  )

  (func (export "deep") (result i32)
    (loop (result i32) (block (result i32)
      (loop (result i32) (block (result i32)
        (loop (result i32) (block (result i32)
          (loop (result i32) (block (result i32)
            (loop (result i32) (block (result i32)
              (loop (result i32) (block (result i32)
                (loop (result i32) (block (result i32)
                  (loop (result i32) (block (result i32)
                    (loop (result i32) (block (result i32)
                      (loop (result i32) (block (result i32)
                        (loop (result i32) (block (result i32)
                          (loop (result i32) (block (result i32)
                            (loop (result i32) (block (result i32)
                              (loop (result i32) (block (result i32)
                                (loop (result i32) (block (result i32)
                                  (loop (result i32) (block (result i32)
                                    (loop (result i32) (block (result i32)
                                      (loop (result i32) (block (result i32)
                                        (loop (result i32) (block (result i32)
                                          (loop (result i32) (block (result i32)
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
    ))
  )

  (func (export "as-select-first") (result i32)
    (select (loop (result i32) (i32.const 1)) (i32.const 2) (i32.const 3))
  )
  (func (export "as-select-mid") (result i32)
    (select (i32.const 2) (loop (result i32) (i32.const 1)) (i32.const 3))
  )
  (func (export "as-select-last") (result i32)
    (select (i32.const 2) (i32.const 3) (loop (result i32) (i32.const 1)))
  )

  (func (export "as-if-condition")
    (loop (result i32) (i32.const 1)) (if (then (call $$dummy)))
  )
  (func (export "as-if-then") (result i32)
    (if (result i32) (i32.const 1) (then (loop (result i32) (i32.const 1))) (else (i32.const 2)))
  )
  (func (export "as-if-else") (result i32)
    (if (result i32) (i32.const 1) (then (i32.const 2)) (else (loop (result i32) (i32.const 1))))
  )

  (func (export "as-br_if-first") (result i32)
    (block (result i32) (br_if 0 (loop (result i32) (i32.const 1)) (i32.const 2)))
  )
  (func (export "as-br_if-last") (result i32)
    (block (result i32) (br_if 0 (i32.const 2) (loop (result i32) (i32.const 1))))
  )

  (func (export "as-br_table-first") (result i32)
    (block (result i32) (loop (result i32) (i32.const 1)) (i32.const 2) (br_table 0 0))
  )
  (func (export "as-br_table-last") (result i32)
    (block (result i32) (i32.const 2) (loop (result i32) (i32.const 1)) (br_table 0 0))
  )

  (func $$func (param i32 i32) (result i32) (local.get 0))
  (type $$check (func (param i32 i32) (result i32)))
  (table funcref (elem $$func))
  (func (export "as-call_indirect-first") (result i32)
    (block (result i32)
      (call_indirect (type $$check)
        (loop (result i32) (i32.const 1)) (i32.const 2) (i32.const 0)
      )
    )
  )
  (func (export "as-call_indirect-mid") (result i32)
    (block (result i32)
      (call_indirect (type $$check)
        (i32.const 2) (loop (result i32) (i32.const 1)) (i32.const 0)
      )
    )
  )
  (func (export "as-call_indirect-last") (result i32)
    (block (result i32)
      (call_indirect (type $$check)
        (i32.const 1) (i32.const 2) (loop (result i32) (i32.const 0))
      )
    )
  )

  (func (export "as-store-first")
    (loop (result i32) (i32.const 1)) (i32.const 1) (i32.store)
  )
  (func (export "as-store-last")
    (i32.const 10) (loop (result i32) (i32.const 1)) (i32.store)
  )

  (func (export "as-memory.grow-value") (result i32)
    (memory.grow (loop (result i32) (i32.const 1)))
  )

  (func $$f (param i32) (result i32) (local.get 0))

  (func (export "as-call-value") (result i32)
    (call $$f (loop (result i32) (i32.const 1)))
  )
  (func (export "as-return-value") (result i32)
    (loop (result i32) (i32.const 1)) (return)
  )
  (func (export "as-drop-operand")
    (drop (loop (result i32) (i32.const 1)))
  )
  (func (export "as-br-value") (result i32)
    (block (result i32) (br 0 (loop (result i32) (i32.const 1))))
  )
  (func (export "as-local.set-value") (result i32)
    (local i32) (local.set 0 (loop (result i32) (i32.const 1))) (local.get 0)
  )
  (func (export "as-local.tee-value") (result i32)
    (local i32) (local.tee 0 (loop (result i32) (i32.const 1)))
  )
  (global $$a (mut i32) (i32.const 0))
  (func (export "as-global.set-value") (result i32)
    (global.set $$a (loop (result i32) (i32.const 1)))
    (global.get $$a)
  )
  (func (export "as-load-operand") (result i32)
    (i32.load (loop (result i32) (i32.const 1)))
  )

  (func (export "as-unary-operand") (result i32)
    (i32.ctz (loop (result i32) (call $$dummy) (i32.const 13)))
  )
  (func (export "as-binary-operand") (result i32)
    (i32.mul
      (loop (result i32) (call $$dummy) (i32.const 3))
      (loop (result i32) (call $$dummy) (i32.const 4))
    )
  )
  (func (export "as-test-operand") (result i32)
    (i32.eqz (loop (result i32) (call $$dummy) (i32.const 13)))
  )
  (func (export "as-compare-operand") (result i32)
    (f32.gt
      (loop (result f32) (call $$dummy) (f32.const 3))
      (loop (result f32) (call $$dummy) (f32.const 3))
    )
  )

  (func (export "break-bare") (result i32)
    (block (loop (br 1) (br 0) (unreachable)))
    (block (loop (br_if 1 (i32.const 1)) (unreachable)))
    (block (loop (br_table 1 (i32.const 0)) (unreachable)))
    (block (loop (br_table 1 1 1 (i32.const 1)) (unreachable)))
    (i32.const 19)
  )
  (func (export "break-value") (result i32)
    (block (result i32)
      (loop (result i32) (br 1 (i32.const 18)) (br 0) (i32.const 19))
    )
  )
  (func (export "break-repeated") (result i32)
    (block (result i32)
      (loop (result i32)
        (br 1 (i32.const 18))
        (br 1 (i32.const 19))
        (drop (br_if 1 (i32.const 20) (i32.const 0)))
        (drop (br_if 1 (i32.const 20) (i32.const 1)))
        (br 1 (i32.const 21))
        (br_table 1 (i32.const 22) (i32.const 0))
        (br_table 1 1 1 (i32.const 23) (i32.const 1))
        (i32.const 21)
      )
    )
  )
  (func (export "break-inner") (result i32)
    (local i32)
    (local.set 0 (i32.const 0))
    (local.set 0 (i32.add (local.get 0) (block (result i32) (loop (result i32) (block (result i32) (br 2 (i32.const 0x1)))))))
    (local.set 0 (i32.add (local.get 0) (block (result i32) (loop (result i32) (loop (result i32) (br 2 (i32.const 0x2)))))))
    (local.set 0 (i32.add (local.get 0) (block (result i32) (loop (result i32) (block (result i32) (loop (result i32) (br 1 (i32.const 0x4))))))))
    (local.set 0 (i32.add (local.get 0) (block (result i32) (loop (result i32) (i32.ctz (br 1 (i32.const 0x8)))))))
    (local.set 0 (i32.add (local.get 0) (block (result i32) (loop (result i32) (i32.ctz (loop (result i32) (br 2 (i32.const 0x10))))))))
    (local.get 0)
  )
  (func (export "cont-inner") (result i32)
    (local i32)
    (local.set 0 (i32.const 0))
    (local.set 0 (i32.add (local.get 0) (loop (result i32) (loop (result i32) (br 1)))))
    (local.set 0 (i32.add (local.get 0) (loop (result i32) (i32.ctz (br 0)))))
    (local.set 0 (i32.add (local.get 0) (loop (result i32) (i32.ctz (loop (result i32) (br 1))))))
    (local.get 0)
  )

  (func $$fx (export "effects") (result i32)
    (local i32)
    (block
      (loop
        (local.set 0 (i32.const 1))
        (local.set 0 (i32.mul (local.get 0) (i32.const 3)))
        (local.set 0 (i32.sub (local.get 0) (i32.const 5)))
        (local.set 0 (i32.mul (local.get 0) (i32.const 7)))
        (br 1)
        (local.set 0 (i32.mul (local.get 0) (i32.const 100)))
      )
    )
    (i32.eq (local.get 0) (i32.const -14))
  )

  (func (export "while") (param i64) (result i64)
    (local i64)
    (local.set 1 (i64.const 1))
    (block
      (loop
        (br_if 1 (i64.eqz (local.get 0)))
        (local.set 1 (i64.mul (local.get 0) (local.get 1)))
        (local.set 0 (i64.sub (local.get 0) (i64.const 1)))
        (br 0)
      )
    )
    (local.get 1)
  )

  (func (export "for") (param i64) (result i64)
    (local i64 i64)
    (local.set 1 (i64.const 1))
    (local.set 2 (i64.const 2))
    (block
      (loop
        (br_if 1 (i64.gt_u (local.get 2) (local.get 0)))
        (local.set 1 (i64.mul (local.get 1) (local.get 2)))
        (local.set 2 (i64.add (local.get 2) (i64.const 1)))
        (br 0)
      )
    )
    (local.get 1)
  )

  (func (export "nesting") (param f32 f32) (result f32)
    (local f32 f32)
    (block
      (loop
        (br_if 1 (f32.eq (local.get 0) (f32.const 0)))
        (local.set 2 (local.get 1))
        (block
          (loop
            (br_if 1 (f32.eq (local.get 2) (f32.const 0)))
            (br_if 3 (f32.lt (local.get 2) (f32.const 0)))
            (local.set 3 (f32.add (local.get 3) (local.get 2)))
            (local.set 2 (f32.sub (local.get 2) (f32.const 2)))
            (br 0)
          )
        )
        (local.set 3 (f32.div (local.get 3) (local.get 0)))
        (local.set 0 (f32.sub (local.get 0) (f32.const 1)))
        (br 0)
      )
    )
    (local.get 3)
  )
)`);

// ./test/core/loop.wast:305
assert_return(() => invoke($0, `empty`, []), []);

// ./test/core/loop.wast:306
assert_return(() => invoke($0, `singular`, []), [value("i32", 7)]);

// ./test/core/loop.wast:307
assert_return(() => invoke($0, `multi`, []), [value("i32", 8)]);

// ./test/core/loop.wast:308
assert_return(() => invoke($0, `nested`, []), [value("i32", 9)]);

// ./test/core/loop.wast:309
assert_return(() => invoke($0, `deep`, []), [value("i32", 150)]);

// ./test/core/loop.wast:311
assert_return(() => invoke($0, `as-select-first`, []), [value("i32", 1)]);

// ./test/core/loop.wast:312
assert_return(() => invoke($0, `as-select-mid`, []), [value("i32", 2)]);

// ./test/core/loop.wast:313
assert_return(() => invoke($0, `as-select-last`, []), [value("i32", 2)]);

// ./test/core/loop.wast:315
assert_return(() => invoke($0, `as-if-condition`, []), []);

// ./test/core/loop.wast:316
assert_return(() => invoke($0, `as-if-then`, []), [value("i32", 1)]);

// ./test/core/loop.wast:317
assert_return(() => invoke($0, `as-if-else`, []), [value("i32", 2)]);

// ./test/core/loop.wast:319
assert_return(() => invoke($0, `as-br_if-first`, []), [value("i32", 1)]);

// ./test/core/loop.wast:320
assert_return(() => invoke($0, `as-br_if-last`, []), [value("i32", 2)]);

// ./test/core/loop.wast:322
assert_return(() => invoke($0, `as-br_table-first`, []), [value("i32", 1)]);

// ./test/core/loop.wast:323
assert_return(() => invoke($0, `as-br_table-last`, []), [value("i32", 2)]);

// ./test/core/loop.wast:325
assert_return(() => invoke($0, `as-call_indirect-first`, []), [
  value("i32", 1),
]);

// ./test/core/loop.wast:326
assert_return(() => invoke($0, `as-call_indirect-mid`, []), [value("i32", 2)]);

// ./test/core/loop.wast:327
assert_return(() => invoke($0, `as-call_indirect-last`, []), [value("i32", 1)]);

// ./test/core/loop.wast:329
assert_return(() => invoke($0, `as-store-first`, []), []);

// ./test/core/loop.wast:330
assert_return(() => invoke($0, `as-store-last`, []), []);

// ./test/core/loop.wast:332
assert_return(() => invoke($0, `as-memory.grow-value`, []), [value("i32", 1)]);

// ./test/core/loop.wast:333
assert_return(() => invoke($0, `as-call-value`, []), [value("i32", 1)]);

// ./test/core/loop.wast:334
assert_return(() => invoke($0, `as-return-value`, []), [value("i32", 1)]);

// ./test/core/loop.wast:335
assert_return(() => invoke($0, `as-drop-operand`, []), []);

// ./test/core/loop.wast:336
assert_return(() => invoke($0, `as-br-value`, []), [value("i32", 1)]);

// ./test/core/loop.wast:337
assert_return(() => invoke($0, `as-local.set-value`, []), [value("i32", 1)]);

// ./test/core/loop.wast:338
assert_return(() => invoke($0, `as-local.tee-value`, []), [value("i32", 1)]);

// ./test/core/loop.wast:339
assert_return(() => invoke($0, `as-global.set-value`, []), [value("i32", 1)]);

// ./test/core/loop.wast:340
assert_return(() => invoke($0, `as-load-operand`, []), [value("i32", 1)]);

// ./test/core/loop.wast:342
assert_return(() => invoke($0, `as-unary-operand`, []), [value("i32", 0)]);

// ./test/core/loop.wast:343
assert_return(() => invoke($0, `as-binary-operand`, []), [value("i32", 12)]);

// ./test/core/loop.wast:344
assert_return(() => invoke($0, `as-test-operand`, []), [value("i32", 0)]);

// ./test/core/loop.wast:345
assert_return(() => invoke($0, `as-compare-operand`, []), [value("i32", 0)]);

// ./test/core/loop.wast:347
assert_return(() => invoke($0, `break-bare`, []), [value("i32", 19)]);

// ./test/core/loop.wast:348
assert_return(() => invoke($0, `break-value`, []), [value("i32", 18)]);

// ./test/core/loop.wast:349
assert_return(() => invoke($0, `break-repeated`, []), [value("i32", 18)]);

// ./test/core/loop.wast:350
assert_return(() => invoke($0, `break-inner`, []), [value("i32", 31)]);

// ./test/core/loop.wast:352
assert_return(() => invoke($0, `effects`, []), [value("i32", 1)]);

// ./test/core/loop.wast:354
assert_return(() => invoke($0, `while`, [0n]), [value("i64", 1n)]);

// ./test/core/loop.wast:355
assert_return(() => invoke($0, `while`, [1n]), [value("i64", 1n)]);

// ./test/core/loop.wast:356
assert_return(() => invoke($0, `while`, [2n]), [value("i64", 2n)]);

// ./test/core/loop.wast:357
assert_return(() => invoke($0, `while`, [3n]), [value("i64", 6n)]);

// ./test/core/loop.wast:358
assert_return(() => invoke($0, `while`, [5n]), [value("i64", 120n)]);

// ./test/core/loop.wast:359
assert_return(() => invoke($0, `while`, [20n]), [
  value("i64", 2432902008176640000n),
]);

// ./test/core/loop.wast:361
assert_return(() => invoke($0, `for`, [0n]), [value("i64", 1n)]);

// ./test/core/loop.wast:362
assert_return(() => invoke($0, `for`, [1n]), [value("i64", 1n)]);

// ./test/core/loop.wast:363
assert_return(() => invoke($0, `for`, [2n]), [value("i64", 2n)]);

// ./test/core/loop.wast:364
assert_return(() => invoke($0, `for`, [3n]), [value("i64", 6n)]);

// ./test/core/loop.wast:365
assert_return(() => invoke($0, `for`, [5n]), [value("i64", 120n)]);

// ./test/core/loop.wast:366
assert_return(() => invoke($0, `for`, [20n]), [
  value("i64", 2432902008176640000n),
]);

// ./test/core/loop.wast:368
assert_return(() => invoke($0, `nesting`, [value("f32", 0), value("f32", 7)]), [
  value("f32", 0),
]);

// ./test/core/loop.wast:369
assert_return(() => invoke($0, `nesting`, [value("f32", 7), value("f32", 0)]), [
  value("f32", 0),
]);

// ./test/core/loop.wast:370
assert_return(() => invoke($0, `nesting`, [value("f32", 1), value("f32", 1)]), [
  value("f32", 1),
]);

// ./test/core/loop.wast:371
assert_return(() => invoke($0, `nesting`, [value("f32", 1), value("f32", 2)]), [
  value("f32", 2),
]);

// ./test/core/loop.wast:372
assert_return(() => invoke($0, `nesting`, [value("f32", 1), value("f32", 3)]), [
  value("f32", 4),
]);

// ./test/core/loop.wast:373
assert_return(() => invoke($0, `nesting`, [value("f32", 1), value("f32", 4)]), [
  value("f32", 6),
]);

// ./test/core/loop.wast:374
assert_return(
  () => invoke($0, `nesting`, [value("f32", 1), value("f32", 100)]),
  [value("f32", 2550)],
);

// ./test/core/loop.wast:375
assert_return(
  () => invoke($0, `nesting`, [value("f32", 1), value("f32", 101)]),
  [value("f32", 2601)],
);

// ./test/core/loop.wast:376
assert_return(() => invoke($0, `nesting`, [value("f32", 2), value("f32", 1)]), [
  value("f32", 1),
]);

// ./test/core/loop.wast:377
assert_return(() => invoke($0, `nesting`, [value("f32", 3), value("f32", 1)]), [
  value("f32", 1),
]);

// ./test/core/loop.wast:378
assert_return(
  () => invoke($0, `nesting`, [value("f32", 10), value("f32", 1)]),
  [value("f32", 1)],
);

// ./test/core/loop.wast:379
assert_return(() => invoke($0, `nesting`, [value("f32", 2), value("f32", 2)]), [
  value("f32", 3),
]);

// ./test/core/loop.wast:380
assert_return(() => invoke($0, `nesting`, [value("f32", 2), value("f32", 3)]), [
  value("f32", 4),
]);

// ./test/core/loop.wast:381
assert_return(() => invoke($0, `nesting`, [value("f32", 7), value("f32", 4)]), [
  value("f32", 10.309524),
]);

// ./test/core/loop.wast:382
assert_return(
  () => invoke($0, `nesting`, [value("f32", 7), value("f32", 100)]),
  [value("f32", 4381.548)],
);

// ./test/core/loop.wast:383
assert_return(
  () => invoke($0, `nesting`, [value("f32", 7), value("f32", 101)]),
  [value("f32", 2601)],
);

// ./test/core/loop.wast:385
assert_invalid(
  () => instantiate(`(module (func $$type-empty-i32 (result i32) (loop)))`),
  `type mismatch`,
);

// ./test/core/loop.wast:389
assert_invalid(
  () => instantiate(`(module (func $$type-empty-i64 (result i64) (loop)))`),
  `type mismatch`,
);

// ./test/core/loop.wast:393
assert_invalid(
  () => instantiate(`(module (func $$type-empty-f32 (result f32) (loop)))`),
  `type mismatch`,
);

// ./test/core/loop.wast:397
assert_invalid(
  () => instantiate(`(module (func $$type-empty-f64 (result f64) (loop)))`),
  `type mismatch`,
);

// ./test/core/loop.wast:402
assert_invalid(() =>
  instantiate(`(module (func $$type-value-num-vs-void
    (loop (i32.const 1))
  ))`), `type mismatch`);

// ./test/core/loop.wast:408
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-empty-vs-num (result i32)
    (loop (result i32))
  ))`),
  `type mismatch`,
);

// ./test/core/loop.wast:414
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-void-vs-num (result i32)
    (loop (result i32) (nop))
  ))`),
  `type mismatch`,
);

// ./test/core/loop.wast:420
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-num-vs-num (result i32)
    (loop (result i32) (f32.const 0))
  ))`),
  `type mismatch`,
);

// ./test/core/loop.wast:426
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-unreached-select (result i32)
    (loop (result i64) (select (unreachable) (unreachable) (unreachable)))
  ))`),
  `type mismatch`,
);

// ./test/core/loop.wast:433
assert_invalid(() =>
  instantiate(`(module
    (func $$type-value-empty-in-block
      (i32.const 0)
      (block (loop (result i32)) (drop))
    )
  )`), `type mismatch`);

// ./test/core/loop.wast:442
assert_invalid(() =>
  instantiate(`(module
    (func $$type-value-empty-in-loop
      (i32.const 0)
      (loop (loop (result i32)) (drop))
    )
  )`), `type mismatch`);

// ./test/core/loop.wast:451
assert_invalid(() =>
  instantiate(`(module
    (func $$type-value-empty-in-then
      (i32.const 0) (i32.const 0)
      (if (then (loop (result i32)) (drop)))
    )
  )`), `type mismatch`);

// ./test/core/loop.wast:462
assert_malformed(
  () => instantiate(`(func loop end $$l) `),
  `mismatching label`,
);

// ./test/core/loop.wast:466
assert_malformed(
  () => instantiate(`(func loop $$a end $$l) `),
  `mismatching label`,
);
