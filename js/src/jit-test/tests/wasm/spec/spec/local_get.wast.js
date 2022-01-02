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

// ./test/core/local_get.wast

// ./test/core/local_get.wast:3
let $0 = instantiate(`(module
  ;; Typing

  (func (export "type-local-i32") (result i32) (local i32) (local.get 0))
  (func (export "type-local-i64") (result i64) (local i64) (local.get 0))
  (func (export "type-local-f32") (result f32) (local f32) (local.get 0))
  (func (export "type-local-f64") (result f64) (local f64) (local.get 0))

  (func (export "type-param-i32") (param i32) (result i32) (local.get 0))
  (func (export "type-param-i64") (param i64) (result i64) (local.get 0))
  (func (export "type-param-f32") (param f32) (result f32) (local.get 0))
  (func (export "type-param-f64") (param f64) (result f64) (local.get 0))

  (func (export "type-mixed") (param i64 f32 f64 i32 i32)
    (local f32 i64 i64 f64)
    (drop (i64.eqz (local.get 0)))
    (drop (f32.neg (local.get 1)))
    (drop (f64.neg (local.get 2)))
    (drop (i32.eqz (local.get 3)))
    (drop (i32.eqz (local.get 4)))
    (drop (f32.neg (local.get 5)))
    (drop (i64.eqz (local.get 6)))
    (drop (i64.eqz (local.get 7)))
    (drop (f64.neg (local.get 8)))
  )

  ;; Reading

  (func (export "read") (param i64 f32 f64 i32 i32) (result f64)
    (local f32 i64 i64 f64)
    (local.set 5 (f32.const 5.5))
    (local.set 6 (i64.const 6))
    (local.set 8 (f64.const 8))
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

  ;; As parameter of control constructs and instructions

  (func (export "as-block-value") (param i32) (result i32)
    (block (result i32) (local.get 0))
  )
  (func (export "as-loop-value") (param i32) (result i32)
    (loop (result i32) (local.get 0))
  )
  (func (export "as-br-value") (param i32) (result i32)
    (block (result i32) (br 0 (local.get 0)))
  )
  (func (export "as-br_if-value") (param i32) (result i32)
    (block $$l0 (result i32) (br_if $$l0 (local.get 0) (i32.const 1)))
  )

  (func (export "as-br_if-value-cond") (param i32) (result i32)
    (block (result i32)
      (br_if 0 (local.get 0) (local.get 0))
    )
  )
  (func (export "as-br_table-value") (param i32) (result i32)
    (block
      (block
        (block
          (br_table 0 1 2 (local.get 0))
          (return (i32.const 0))
        )
        (return (i32.const 1))
      )
      (return (i32.const 2))
    )
    (i32.const 3)
  )

  (func (export "as-return-value") (param i32) (result i32)
    (return (local.get 0))
  )

  (func (export "as-if-then") (param i32) (result i32)
    (if (result i32) (local.get 0) (then (local.get 0)) (else (i32.const 0)))
  )
  (func (export "as-if-else") (param i32) (result i32)
    (if (result i32) (local.get 0) (then (i32.const 1)) (else (local.get 0)))
  )
)`);

// ./test/core/local_get.wast:109
assert_return(() => invoke($0, `type-local-i32`, []), [value("i32", 0)]);

// ./test/core/local_get.wast:110
assert_return(() => invoke($0, `type-local-i64`, []), [value("i64", 0n)]);

// ./test/core/local_get.wast:111
assert_return(() => invoke($0, `type-local-f32`, []), [value("f32", 0)]);

// ./test/core/local_get.wast:112
assert_return(() => invoke($0, `type-local-f64`, []), [value("f64", 0)]);

// ./test/core/local_get.wast:114
assert_return(() => invoke($0, `type-param-i32`, [2]), [value("i32", 2)]);

// ./test/core/local_get.wast:115
assert_return(() => invoke($0, `type-param-i64`, [3n]), [value("i64", 3n)]);

// ./test/core/local_get.wast:116
assert_return(() => invoke($0, `type-param-f32`, [value("f32", 4.4)]), [
  value("f32", 4.4),
]);

// ./test/core/local_get.wast:117
assert_return(() => invoke($0, `type-param-f64`, [value("f64", 5.5)]), [
  value("f64", 5.5),
]);

// ./test/core/local_get.wast:119
assert_return(() => invoke($0, `as-block-value`, [6]), [value("i32", 6)]);

// ./test/core/local_get.wast:120
assert_return(() => invoke($0, `as-loop-value`, [7]), [value("i32", 7)]);

// ./test/core/local_get.wast:122
assert_return(() => invoke($0, `as-br-value`, [8]), [value("i32", 8)]);

// ./test/core/local_get.wast:123
assert_return(() => invoke($0, `as-br_if-value`, [9]), [value("i32", 9)]);

// ./test/core/local_get.wast:124
assert_return(() => invoke($0, `as-br_if-value-cond`, [10]), [
  value("i32", 10),
]);

// ./test/core/local_get.wast:125
assert_return(() => invoke($0, `as-br_table-value`, [1]), [value("i32", 2)]);

// ./test/core/local_get.wast:127
assert_return(() => invoke($0, `as-return-value`, [0]), [value("i32", 0)]);

// ./test/core/local_get.wast:129
assert_return(() => invoke($0, `as-if-then`, [1]), [value("i32", 1)]);

// ./test/core/local_get.wast:130
assert_return(() => invoke($0, `as-if-else`, [0]), [value("i32", 0)]);

// ./test/core/local_get.wast:132
assert_return(
  () =>
    invoke($0, `type-mixed`, [1n, value("f32", 2.2), value("f64", 3.3), 4, 5]),
  [],
);

// ./test/core/local_get.wast:138
assert_return(
  () => invoke($0, `read`, [1n, value("f32", 2), value("f64", 3.3), 4, 5]),
  [value("f64", 34.8)],
);

// ./test/core/local_get.wast:148
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-local-num-vs-num (result i64) (local i32) (local.get 0)))`,
    ),
  `type mismatch`,
);

// ./test/core/local_get.wast:152
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-local-num-vs-num (result i32) (local f32) (i32.eqz (local.get 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/local_get.wast:156
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-local-num-vs-num (result f64) (local f64 i64) (f64.neg (local.get 1))))`,
    ),
  `type mismatch`,
);

// ./test/core/local_get.wast:164
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-param-num-vs-num (param i32) (result i64) (local.get 0)))`,
    ),
  `type mismatch`,
);

// ./test/core/local_get.wast:168
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-param-num-vs-num (param f32) (result i32) (i32.eqz (local.get 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/local_get.wast:172
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-param-num-vs-num (param f64 i64) (result f64) (f64.neg (local.get 1))))`,
    ),
  `type mismatch`,
);

// ./test/core/local_get.wast:180
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-empty-vs-i32 (local i32) (local.get 0)))`,
    ),
  `type mismatch`,
);

// ./test/core/local_get.wast:184
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-empty-vs-i64 (local i64) (local.get 0)))`,
    ),
  `type mismatch`,
);

// ./test/core/local_get.wast:188
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-empty-vs-f32 (local f32) (local.get 0)))`,
    ),
  `type mismatch`,
);

// ./test/core/local_get.wast:192
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-empty-vs-f64 (local f64) (local.get 0)))`,
    ),
  `type mismatch`,
);

// ./test/core/local_get.wast:200
assert_invalid(
  () =>
    instantiate(
      `(module (func $$unbound-local (local i32 i64) (local.get 3) drop))`,
    ),
  `unknown local`,
);

// ./test/core/local_get.wast:204
assert_invalid(
  () =>
    instantiate(
      `(module (func $$large-local (local i32 i64) (local.get 14324343) drop))`,
    ),
  `unknown local`,
);

// ./test/core/local_get.wast:209
assert_invalid(
  () =>
    instantiate(
      `(module (func $$unbound-param (param i32 i64) (local.get 2) drop))`,
    ),
  `unknown local`,
);

// ./test/core/local_get.wast:213
assert_invalid(
  () =>
    instantiate(
      `(module (func $$large-param (param i32 i64) (local.get 714324343) drop))`,
    ),
  `unknown local`,
);

// ./test/core/local_get.wast:218
assert_invalid(
  () =>
    instantiate(
      `(module (func $$unbound-mixed (param i32) (local i32 i64) (local.get 3) drop))`,
    ),
  `unknown local`,
);

// ./test/core/local_get.wast:222
assert_invalid(
  () =>
    instantiate(
      `(module (func $$large-mixed (param i64) (local i32 i64) (local.get 214324343) drop))`,
    ),
  `unknown local`,
);
