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

// ./test/core/local_set.wast

// ./test/core/local_set.wast:3
let $0 = instantiate(`(module
  ;; Typing

  (func (export "type-local-i32") (local i32) (local.set 0 (i32.const 0)))
  (func (export "type-local-i64") (local i64) (local.set 0 (i64.const 0)))
  (func (export "type-local-f32") (local f32) (local.set 0 (f32.const 0)))
  (func (export "type-local-f64") (local f64) (local.set 0 (f64.const 0)))

  (func (export "type-param-i32") (param i32) (local.set 0 (i32.const 10)))
  (func (export "type-param-i64") (param i64) (local.set 0 (i64.const 11)))
  (func (export "type-param-f32") (param f32) (local.set 0 (f32.const 11.1)))
  (func (export "type-param-f64") (param f64) (local.set 0 (f64.const 12.2)))

  (func (export "type-mixed") (param i64 f32 f64 i32 i32) (local f32 i64 i64 f64)
    (local.set 0 (i64.const 0))
    (local.set 1 (f32.const 0))
    (local.set 2 (f64.const 0))
    (local.set 3 (i32.const 0))
    (local.set 4 (i32.const 0))
    (local.set 5 (f32.const 0))
    (local.set 6 (i64.const 0))
    (local.set 7 (i64.const 0))
    (local.set 8 (f64.const 0))
  )

  ;; Writing

  (func (export "write") (param i64 f32 f64 i32 i32) (result i64)
    (local f32 i64 i64 f64)
    (local.set 1 (f32.const -0.3))
    (local.set 3 (i32.const 40))
    (local.set 4 (i32.const -7))
    (local.set 5 (f32.const 5.5))
    (local.set 6 (i64.const 6))
    (local.set 8 (f64.const 8))
    (i64.trunc_f64_s
      (f64.add
        (f64.convert_i64_u (local.get 0))
        (f64.add
          (f64.promote_f32 (local.get 1))
          (f64.add
            (local.get 2)
            (f64.add
              (f64.convert_i32_u (local.get 3))
              (f64.add
                (f64.convert_i32_s (local.get 4))
                (f64.add
                  (f64.promote_f32 (local.get 5))
                  (f64.add
                    (f64.convert_i64_u (local.get 6))
                    (f64.add
                      (f64.convert_i64_u (local.get 7))
                      (local.get 8)
                    )
                  )
                )
              )
            )
          )
        )
      )
    )
  )

  ;; As parameter of control constructs and instructions

  (func (export "as-block-value") (param i32)
    (block (local.set 0 (i32.const 1)))
  )
  (func (export "as-loop-value") (param i32)
    (loop (local.set 0 (i32.const 3)))
  )

  (func (export "as-br-value") (param i32)
    (block (br 0 (local.set 0 (i32.const 9))))
  )
  (func (export "as-br_if-value") (param i32)
    (block
      (br_if 0 (local.set 0 (i32.const 8)) (i32.const 1))
    )
  )
  (func (export "as-br_if-value-cond") (param i32)
    (block
      (br_if 0 (i32.const 6) (local.set 0 (i32.const 9)))
    )
  )
  (func (export "as-br_table-value") (param i32)
    (block
      (br_table 0 (local.set 0 (i32.const 10)) (i32.const 1))
    )
  )

  (func (export "as-return-value") (param i32)
    (return (local.set 0 (i32.const 7)))
  )

  (func (export "as-if-then") (param i32)
    (if (local.get 0) (then (local.set 0 (i32.const 3))))
  )
  (func (export "as-if-else") (param i32)
    (if (local.get 0) (then) (else (local.set 0 (i32.const 1))))
  )
)`);

// ./test/core/local_set.wast:107
assert_return(() => invoke($0, `type-local-i32`, []), []);

// ./test/core/local_set.wast:108
assert_return(() => invoke($0, `type-local-i64`, []), []);

// ./test/core/local_set.wast:109
assert_return(() => invoke($0, `type-local-f32`, []), []);

// ./test/core/local_set.wast:110
assert_return(() => invoke($0, `type-local-f64`, []), []);

// ./test/core/local_set.wast:112
assert_return(() => invoke($0, `type-param-i32`, [2]), []);

// ./test/core/local_set.wast:113
assert_return(() => invoke($0, `type-param-i64`, [3n]), []);

// ./test/core/local_set.wast:114
assert_return(() => invoke($0, `type-param-f32`, [value("f32", 4.4)]), []);

// ./test/core/local_set.wast:115
assert_return(() => invoke($0, `type-param-f64`, [value("f64", 5.5)]), []);

// ./test/core/local_set.wast:117
assert_return(() => invoke($0, `as-block-value`, [0]), []);

// ./test/core/local_set.wast:118
assert_return(() => invoke($0, `as-loop-value`, [0]), []);

// ./test/core/local_set.wast:120
assert_return(() => invoke($0, `as-br-value`, [0]), []);

// ./test/core/local_set.wast:121
assert_return(() => invoke($0, `as-br_if-value`, [0]), []);

// ./test/core/local_set.wast:122
assert_return(() => invoke($0, `as-br_if-value-cond`, [0]), []);

// ./test/core/local_set.wast:123
assert_return(() => invoke($0, `as-br_table-value`, [0]), []);

// ./test/core/local_set.wast:125
assert_return(() => invoke($0, `as-return-value`, [0]), []);

// ./test/core/local_set.wast:127
assert_return(() => invoke($0, `as-if-then`, [1]), []);

// ./test/core/local_set.wast:128
assert_return(() => invoke($0, `as-if-else`, [0]), []);

// ./test/core/local_set.wast:130
assert_return(
  () =>
    invoke($0, `type-mixed`, [1n, value("f32", 2.2), value("f64", 3.3), 4, 5]),
  [],
);

// ./test/core/local_set.wast:136
assert_return(
  () => invoke($0, `write`, [1n, value("f32", 2), value("f64", 3.3), 4, 5]),
  [value("i64", 56n)],
);

// ./test/core/local_set.wast:147
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-local-arg-void-vs-num (local i32) (local.set 0 (nop))))`,
    ),
  `type mismatch`,
);

// ./test/core/local_set.wast:151
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-local-arg-num-vs-num (local i32) (local.set 0 (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/local_set.wast:155
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-local-arg-num-vs-num (local f32) (local.set 0 (f64.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/local_set.wast:159
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-local-arg-num-vs-num (local f64 i64) (local.set 1 (f64.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/local_set.wast:168
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-param-arg-void-vs-num (param i32) (local.set 0 (nop))))`,
    ),
  `type mismatch`,
);

// ./test/core/local_set.wast:172
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-param-arg-num-vs-num (param i32) (local.set 0 (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/local_set.wast:176
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-param-arg-num-vs-num (param f32) (local.set 0 (f64.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/local_set.wast:180
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-param-arg-num-vs-num (param f64 i64) (local.set 1 (f64.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/local_set.wast:185
assert_invalid(() =>
  instantiate(`(module
    (func $$type-param-arg-empty-vs-num (param i32)
      (local.set 0)
    )
  )`), `type mismatch`);

// ./test/core/local_set.wast:193
assert_invalid(() =>
  instantiate(`(module
    (func $$type-param-arg-empty-vs-num-in-block (param i32)
      (i32.const 0)
      (block (local.set 0))
    )
  )`), `type mismatch`);

// ./test/core/local_set.wast:202
assert_invalid(() =>
  instantiate(`(module
    (func $$type-param-arg-empty-vs-num-in-loop (param i32)
      (i32.const 0)
      (loop (local.set 0))
    )
  )`), `type mismatch`);

// ./test/core/local_set.wast:211
assert_invalid(() =>
  instantiate(`(module
    (func $$type-param-arg-empty-vs-num-in-then (param i32)
      (i32.const 0)
      (if (i32.const 1) (then (local.set 0)))
    )
  )`), `type mismatch`);

// ./test/core/local_set.wast:220
assert_invalid(() =>
  instantiate(`(module
    (func $$type-param-arg-empty-vs-num-in-else (param i32)
      (i32.const 0)
      (if (result i32) (i32.const 0) (then (i32.const 0)) (else (local.set 0)))
    )
  )`), `type mismatch`);

// ./test/core/local_set.wast:229
assert_invalid(() =>
  instantiate(`(module
    (func $$type-param-arg-empty-vs-num-in-br (param i32)
      (i32.const 0)
      (block (br 0 (local.set 0)))
    )
  )`), `type mismatch`);

// ./test/core/local_set.wast:238
assert_invalid(() =>
  instantiate(`(module
    (func $$type-param-arg-empty-vs-num-in-br_if (param i32)
      (i32.const 0)
      (block (br_if 0 (local.set 0)))
    )
  )`), `type mismatch`);

// ./test/core/local_set.wast:247
assert_invalid(() =>
  instantiate(`(module
    (func $$type-param-arg-empty-vs-num-in-br_table (param i32)
      (i32.const 0)
      (block (br_table 0 (local.set 0)))
    )
  )`), `type mismatch`);

// ./test/core/local_set.wast:256
assert_invalid(() =>
  instantiate(`(module
    (func $$type-param-arg-empty-vs-num-in-return (param i32)
      (return (local.set 0))
    )
  )`), `type mismatch`);

// ./test/core/local_set.wast:264
assert_invalid(() =>
  instantiate(`(module
    (func $$type-param-arg-empty-vs-num-in-select (param i32)
      (select (local.set 0) (i32.const 1) (i32.const 2))
    )
  )`), `type mismatch`);

// ./test/core/local_set.wast:272
assert_invalid(() =>
  instantiate(`(module
    (func $$type-param-arg-empty-vs-num-in-call (param i32)
      (call 1 (local.set 0))
    )
    (func (param i32) (result i32) (local.get 0))
  )`), `type mismatch`);

// ./test/core/local_set.wast:281
assert_invalid(() =>
  instantiate(`(module
    (func $$f (param i32) (result i32) (local.get 0))
    (type $$sig (func (param i32) (result i32)))
    (table funcref (elem $$f))
    (func $$type-param-arg-empty-vs-num-in-call_indirect (param i32)
      (block (result i32)
        (call_indirect (type $$sig)
          (local.set 0) (i32.const 0)
        )
      )
    )
  )`), `type mismatch`);

// ./test/core/local_set.wast:300
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-mixed-arg-num-vs-num (param f32) (local i32) (local.set 1 (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/local_set.wast:304
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-mixed-arg-num-vs-num (param i64 i32) (local f32) (local.set 1 (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/local_set.wast:308
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-mixed-arg-num-vs-num (param i64) (local f64 i64) (local.set 1 (i64.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/local_set.wast:316
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-empty-vs-i32 (param i32) (result i32) (local.set 0 (i32.const 1))))`,
    ),
  `type mismatch`,
);

// ./test/core/local_set.wast:320
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-empty-vs-i64 (param i64) (result i64) (local.set 0 (i64.const 1))))`,
    ),
  `type mismatch`,
);

// ./test/core/local_set.wast:324
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-empty-vs-f32 (param f32) (result f32) (local.set 0 (f32.const 1))))`,
    ),
  `type mismatch`,
);

// ./test/core/local_set.wast:328
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-empty-vs-f64 (param f64) (result f64) (local.set 0 (f64.const 1))))`,
    ),
  `type mismatch`,
);

// ./test/core/local_set.wast:336
assert_invalid(
  () =>
    instantiate(
      `(module (func $$unbound-local (local i32 i64) (local.set 3 (i32.const 0))))`,
    ),
  `unknown local`,
);

// ./test/core/local_set.wast:340
assert_invalid(
  () =>
    instantiate(
      `(module (func $$large-local (local i32 i64) (local.set 14324343 (i32.const 0))))`,
    ),
  `unknown local`,
);

// ./test/core/local_set.wast:345
assert_invalid(
  () =>
    instantiate(
      `(module (func $$unbound-param (param i32 i64) (local.set 2 (i32.const 0))))`,
    ),
  `unknown local`,
);

// ./test/core/local_set.wast:349
assert_invalid(
  () =>
    instantiate(
      `(module (func $$large-param (param i32 i64) (local.set 714324343 (i32.const 0))))`,
    ),
  `unknown local`,
);

// ./test/core/local_set.wast:354
assert_invalid(
  () =>
    instantiate(
      `(module (func $$unbound-mixed (param i32) (local i32 i64) (local.set 3 (i32.const 0))))`,
    ),
  `unknown local`,
);

// ./test/core/local_set.wast:358
assert_invalid(
  () =>
    instantiate(
      `(module (func $$large-mixed (param i64) (local i32 i64) (local.set 214324343 (i32.const 0))))`,
    ),
  `unknown local`,
);
