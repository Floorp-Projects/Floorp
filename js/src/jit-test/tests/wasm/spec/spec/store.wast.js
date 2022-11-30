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

// ./test/core/store.wast

// ./test/core/store.wast:3
let $0 = instantiate(`(module
  (memory 1)

  (func (export "as-block-value")
    (block (i32.store (i32.const 0) (i32.const 1)))
  )
  (func (export "as-loop-value")
    (loop (i32.store (i32.const 0) (i32.const 1)))
  )

  (func (export "as-br-value")
    (block (br 0 (i32.store (i32.const 0) (i32.const 1))))
  )
  (func (export "as-br_if-value")
    (block
      (br_if 0 (i32.store (i32.const 0) (i32.const 1)) (i32.const 1))
    )
  )
  (func (export "as-br_if-value-cond")
    (block
      (br_if 0 (i32.const 6) (i32.store (i32.const 0) (i32.const 1)))
    )
  )
  (func (export "as-br_table-value")
    (block
      (br_table 0 (i32.store (i32.const 0) (i32.const 1)) (i32.const 1))
    )
  )

  (func (export "as-return-value")
    (return (i32.store (i32.const 0) (i32.const 1)))
  )

  (func (export "as-if-then")
    (if (i32.const 1) (then (i32.store (i32.const 0) (i32.const 1))))
  )
  (func (export "as-if-else")
    (if (i32.const 0) (then) (else (i32.store (i32.const 0) (i32.const 1))))
  )
)`);

// ./test/core/store.wast:44
assert_return(() => invoke($0, `as-block-value`, []), []);

// ./test/core/store.wast:45
assert_return(() => invoke($0, `as-loop-value`, []), []);

// ./test/core/store.wast:47
assert_return(() => invoke($0, `as-br-value`, []), []);

// ./test/core/store.wast:48
assert_return(() => invoke($0, `as-br_if-value`, []), []);

// ./test/core/store.wast:49
assert_return(() => invoke($0, `as-br_if-value-cond`, []), []);

// ./test/core/store.wast:50
assert_return(() => invoke($0, `as-br_table-value`, []), []);

// ./test/core/store.wast:52
assert_return(() => invoke($0, `as-return-value`, []), []);

// ./test/core/store.wast:54
assert_return(() => invoke($0, `as-if-then`, []), []);

// ./test/core/store.wast:55
assert_return(() => invoke($0, `as-if-else`, []), []);

// ./test/core/store.wast:57
assert_malformed(
  () => instantiate(`(memory 1) (func (param i32) (i32.store32 (local.get 0) (i32.const 0))) `),
  `unknown operator`,
);

// ./test/core/store.wast:64
assert_malformed(
  () => instantiate(`(memory 1) (func (param i32) (i32.store64 (local.get 0) (i64.const 0))) `),
  `unknown operator`,
);

// ./test/core/store.wast:72
assert_malformed(
  () => instantiate(`(memory 1) (func (param i32) (i64.store64 (local.get 0) (i64.const 0))) `),
  `unknown operator`,
);

// ./test/core/store.wast:80
assert_malformed(
  () => instantiate(`(memory 1) (func (param i32) (f32.store32 (local.get 0) (f32.const 0))) `),
  `unknown operator`,
);

// ./test/core/store.wast:87
assert_malformed(
  () => instantiate(`(memory 1) (func (param i32) (f32.store64 (local.get 0) (f64.const 0))) `),
  `unknown operator`,
);

// ./test/core/store.wast:95
assert_malformed(
  () => instantiate(`(memory 1) (func (param i32) (f64.store32 (local.get 0) (f32.const 0))) `),
  `unknown operator`,
);

// ./test/core/store.wast:102
assert_malformed(
  () => instantiate(`(memory 1) (func (param i32) (f64.store64 (local.get 0) (f64.const 0))) `),
  `unknown operator`,
);

// ./test/core/store.wast:111
assert_invalid(
  () => instantiate(`(module (memory 1) (func (param i32) (result i32) (i32.store (i32.const 0) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/store.wast:115
assert_invalid(
  () => instantiate(`(module (memory 1) (func (param i64) (result i64) (i64.store (i32.const 0) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/store.wast:119
assert_invalid(
  () => instantiate(`(module (memory 1) (func (param f32) (result f32) (f32.store (i32.const 0) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/store.wast:123
assert_invalid(
  () => instantiate(`(module (memory 1) (func (param f64) (result f64) (f64.store (i32.const 0) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/store.wast:127
assert_invalid(
  () => instantiate(`(module (memory 1) (func (param i32) (result i32) (i32.store8 (i32.const 0) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/store.wast:131
assert_invalid(
  () => instantiate(`(module (memory 1) (func (param i32) (result i32) (i32.store16 (i32.const 0) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/store.wast:135
assert_invalid(
  () => instantiate(`(module (memory 1) (func (param i64) (result i64) (i64.store8 (i32.const 0) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/store.wast:139
assert_invalid(
  () => instantiate(`(module (memory 1) (func (param i64) (result i64) (i64.store16 (i32.const 0) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/store.wast:143
assert_invalid(
  () => instantiate(`(module (memory 1) (func (param i64) (result i64) (i64.store32 (i32.const 0) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/store.wast:149
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-address-empty
      (i32.store)
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:158
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-value-empty
     (i32.const 0) (i32.store)
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:167
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-address-empty-in-block
      (i32.const 0) (i32.const 0)
      (block (i32.store))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:177
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-value-empty-in-block
      (i32.const 0)
      (block (i32.const 0) (i32.store))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:187
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-address-empty-in-loop
      (i32.const 0) (i32.const 0)
      (loop (i32.store))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:197
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-value-empty-in-loop
      (i32.const 0)
      (loop (i32.const 0) (i32.store))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:207
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-address-empty-in-then
      (i32.const 0) (i32.const 0)
      (if (then (i32.store)))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:217
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-value-empty-in-then
      (i32.const 0)
      (if (then (i32.const 0) (i32.store)))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:227
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-address-empty-in-else
      (i32.const 0) (i32.const 0)
      (if (result i32) (then (i32.const 0)) (else (i32.store)))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:237
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-value-empty-in-else
      (i32.const 0)
      (if (result i32) (then (i32.const 0)) (else (i32.const 0) (i32.store)))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:247
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-address-empty-in-br
      (i32.const 0) (i32.const 0)
      (block (br 0 (i32.store)))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:257
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-value-empty-in-br
      (i32.const 0)
      (block (br 0 (i32.const 0) (i32.store)))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:267
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-address-empty-in-br_if
      (i32.const 0) (i32.const 0)
      (block (br_if 0 (i32.store) (i32.const 1)) )
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:277
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-value-empty-in-br_if
      (i32.const 0)
      (block (br_if 0 (i32.const 0) (i32.store) (i32.const 1)) )
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:287
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-address-empty-in-br_table
      (i32.const 0) (i32.const 0)
      (block (br_table 0 (i32.store)))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:297
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-value-empty-in-br_table
      (i32.const 0)
      (block (br_table 0 (i32.const 0) (i32.store)))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:307
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-address-empty-in-return
      (return (i32.store))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:316
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-value-empty-in-return
      (return (i32.const 0) (i32.store))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:325
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-address-empty-in-select
      (select (i32.store) (i32.const 1) (i32.const 2))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:334
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-value-empty-in-select
      (select (i32.const 0) (i32.store) (i32.const 1) (i32.const 2))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:343
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-address-empty-in-call
      (call 1 (i32.store))
    )
    (func (param i32) (result i32) (local.get 0))
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:353
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-value-empty-in-call
      (call 1 (i32.const 0) (i32.store))
    )
    (func (param i32) (result i32) (local.get 0))
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:363
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$f (param i32) (result i32) (local.get 0))
    (type $$sig (func (param i32) (result i32)))
    (table funcref (elem $$f))
    (func $$type-address-empty-in-call_indirect
      (block (result i32)
        (call_indirect (type $$sig)
          (i32.store) (i32.const 0)
        )
      )
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:379
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$f (param i32) (result i32) (local.get 0))
    (type $$sig (func (param i32) (result i32)))
    (table funcref (elem $$f))
    (func $$type-value-empty-in-call_indirect
      (block (result i32)
        (call_indirect (type $$sig)
          (i32.const 0) (i32.store) (i32.const 0)
        )
      )
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:399
assert_invalid(
  () => instantiate(`(module (memory 1) (func (i32.store (f32.const 0) (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:400
assert_invalid(
  () => instantiate(`(module (memory 1) (func (i32.store8 (f32.const 0) (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:401
assert_invalid(
  () => instantiate(`(module (memory 1) (func (i32.store16 (f32.const 0) (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:402
assert_invalid(
  () => instantiate(`(module (memory 1) (func (i64.store (f32.const 0) (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:403
assert_invalid(
  () => instantiate(`(module (memory 1) (func (i64.store8 (f32.const 0) (i64.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:404
assert_invalid(
  () => instantiate(`(module (memory 1) (func (i64.store16 (f32.const 0) (i64.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:405
assert_invalid(
  () => instantiate(`(module (memory 1) (func (i64.store32 (f32.const 0) (i64.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:406
assert_invalid(
  () => instantiate(`(module (memory 1) (func (f32.store (f32.const 0) (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:407
assert_invalid(
  () => instantiate(`(module (memory 1) (func (f64.store (f32.const 0) (f64.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:409
assert_invalid(
  () => instantiate(`(module (memory 1) (func (i32.store (i32.const 0) (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:410
assert_invalid(
  () => instantiate(`(module (memory 1) (func (i32.store8 (i32.const 0) (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:411
assert_invalid(
  () => instantiate(`(module (memory 1) (func (i32.store16 (i32.const 0) (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:412
assert_invalid(
  () => instantiate(`(module (memory 1) (func (i64.store (i32.const 0) (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:413
assert_invalid(
  () => instantiate(`(module (memory 1) (func (i64.store8 (i32.const 0) (f64.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:414
assert_invalid(
  () => instantiate(`(module (memory 1) (func (i64.store16 (i32.const 0) (f64.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:415
assert_invalid(
  () => instantiate(`(module (memory 1) (func (i64.store32 (i32.const 0) (f64.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:416
assert_invalid(
  () => instantiate(`(module (memory 1) (func (f32.store (i32.const 0) (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:417
assert_invalid(
  () => instantiate(`(module (memory 1) (func (f64.store (i32.const 0) (i64.const 0))))`),
  `type mismatch`,
);
