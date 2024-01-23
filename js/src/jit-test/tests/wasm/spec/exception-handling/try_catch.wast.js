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

// ./test/core/try_catch.wast

// ./test/core/try_catch.wast:3
let $0 = instantiate(`(module
  (tag $$e0 (export "e0"))
  (func (export "throw") (throw $$e0))
)`);

// ./test/core/try_catch.wast:8
register($0, `test`);

// ./test/core/try_catch.wast:10
let $1 = instantiate(`(module
  (tag $$imported-e0 (import "test" "e0"))
  (func $$imported-throw (import "test" "throw"))
  (tag $$e0)
  (tag $$e1)
  (tag $$e2)
  (tag $$e-i32 (param i32))
  (tag $$e-f32 (param f32))
  (tag $$e-i64 (param i64))
  (tag $$e-f64 (param f64))

  (func $$throw-if (param i32) (result i32)
    (local.get 0)
    (i32.const 0) (if (i32.ne) (then (throw $$e0)))
    (i32.const 0)
  )

  (func (export "empty-catch") (try (do) (catch $$e0)))

  (func (export "simple-throw-catch") (param i32) (result i32)
    (try (result i32)
      (do (local.get 0) (i32.eqz) (if (then (throw $$e0)) (else)) (i32.const 42))
      (catch $$e0 (i32.const 23))
    )
  )

  (func (export "unreachable-not-caught") (try (do (unreachable)) (catch_all)))

  (func $$div (param i32 i32) (result i32)
    (local.get 0) (local.get 1) (i32.div_u)
  )
  (func (export "trap-in-callee") (param i32 i32) (result i32)
    (try (result i32)
      (do (local.get 0) (local.get 1) (call $$div))
      (catch_all (i32.const 11))
    )
  )

  (func (export "catch-complex-1") (param i32) (result i32)
    (try (result i32)
      (do
        (try (result i32)
          (do
            (local.get 0)
            (i32.eqz)
            (if
              (then (throw $$e0))
              (else
                (local.get 0)
                (i32.const 1)
                (i32.eq)
                (if (then (throw $$e1)) (else (throw $$e2)))
              )
            )
            (i32.const 2)
          )
          (catch $$e0 (i32.const 3))
        )
      )
      (catch $$e1 (i32.const 4))
    )
  )

  (func (export "catch-complex-2") (param i32) (result i32)
    (try (result i32)
      (do
        (local.get 0)
        (i32.eqz)
        (if
          (then (throw $$e0))
          (else
            (local.get 0)
            (i32.const 1)
            (i32.eq)
            (if (then (throw $$e1)) (else (throw $$e2)))
          )
        )
        (i32.const 2)
      )
      (catch $$e0 (i32.const 3))
      (catch $$e1 (i32.const 4))
    )
  )

  (func (export "throw-catch-param-i32") (param i32) (result i32)
    (try (result i32)
      (do (local.get 0) (throw $$e-i32) (i32.const 2))
      (catch $$e-i32 (return))
    )
  )

  (func (export "throw-catch-param-f32") (param f32) (result f32)
    (try (result f32)
      (do (local.get 0) (throw $$e-f32) (f32.const 0))
      (catch $$e-f32 (return))
    )
  )

  (func (export "throw-catch-param-i64") (param i64) (result i64)
    (try (result i64)
      (do (local.get 0) (throw $$e-i64) (i64.const 2))
      (catch $$e-i64 (return))
    )
  )

  (func (export "throw-catch-param-f64") (param f64) (result f64)
    (try (result f64)
      (do (local.get 0) (throw $$e-f64) (f64.const 0))
      (catch $$e-f64 (return))
    )
  )

  (func $$throw-param-i32 (param i32) (local.get 0) (throw $$e-i32))
  (func (export "catch-param-i32") (param i32) (result i32)
    (try (result i32)
      (do (i32.const 0) (local.get 0) (call $$throw-param-i32))
      (catch $$e-i32)
    )
  )

  (func (export "catch-imported") (result i32)
    (try (result i32)
      (do
        (i32.const 1)
        (call $$imported-throw)
      )
      (catch $$imported-e0 (i32.const 2))
    )
  )

  (func (export "catchless-try") (param i32) (result i32)
    (try (result i32)
      (do
        (try (result i32)
          (do (local.get 0) (call $$throw-if))
        )
      )
      (catch $$e0 (i32.const 1))
    )
  )
)`);

// ./test/core/try_catch.wast:172
assert_return(() => invoke($1, `empty-catch`, []), []);

// ./test/core/try_catch.wast:174
assert_return(() => invoke($1, `simple-throw-catch`, [0]), [value("i32", 23)]);

// ./test/core/try_catch.wast:175
assert_return(() => invoke($1, `simple-throw-catch`, [1]), [value("i32", 42)]);

// ./test/core/try_catch.wast:177
assert_trap(() => invoke($1, `unreachable-not-caught`, []), `unreachable`);

// ./test/core/try_catch.wast:179
assert_return(() => invoke($1, `trap-in-callee`, [7, 2]), [value("i32", 3)]);

// ./test/core/try_catch.wast:180
assert_trap(() => invoke($1, `trap-in-callee`, [1, 0]), `integer divide by zero`);

// ./test/core/try_catch.wast:182
assert_return(() => invoke($1, `catch-complex-1`, [0]), [value("i32", 3)]);

// ./test/core/try_catch.wast:183
assert_return(() => invoke($1, `catch-complex-1`, [1]), [value("i32", 4)]);

// ./test/core/try_catch.wast:184
assert_exception(() => invoke($1, `catch-complex-1`, [2]));

// ./test/core/try_catch.wast:186
assert_return(() => invoke($1, `catch-complex-2`, [0]), [value("i32", 3)]);

// ./test/core/try_catch.wast:187
assert_return(() => invoke($1, `catch-complex-2`, [1]), [value("i32", 4)]);

// ./test/core/try_catch.wast:188
assert_exception(() => invoke($1, `catch-complex-2`, [2]));

// ./test/core/try_catch.wast:190
assert_return(() => invoke($1, `throw-catch-param-i32`, [0]), [value("i32", 0)]);

// ./test/core/try_catch.wast:191
assert_return(() => invoke($1, `throw-catch-param-i32`, [1]), [value("i32", 1)]);

// ./test/core/try_catch.wast:192
assert_return(() => invoke($1, `throw-catch-param-i32`, [10]), [value("i32", 10)]);

// ./test/core/try_catch.wast:194
assert_return(() => invoke($1, `throw-catch-param-f32`, [value("f32", 5)]), [value("f32", 5)]);

// ./test/core/try_catch.wast:195
assert_return(() => invoke($1, `throw-catch-param-f32`, [value("f32", 10.5)]), [value("f32", 10.5)]);

// ./test/core/try_catch.wast:197
assert_return(() => invoke($1, `throw-catch-param-i64`, [5n]), [value("i64", 5n)]);

// ./test/core/try_catch.wast:198
assert_return(() => invoke($1, `throw-catch-param-i64`, [0n]), [value("i64", 0n)]);

// ./test/core/try_catch.wast:199
assert_return(() => invoke($1, `throw-catch-param-i64`, [-1n]), [value("i64", -1n)]);

// ./test/core/try_catch.wast:201
assert_return(() => invoke($1, `throw-catch-param-f64`, [value("f64", 5)]), [value("f64", 5)]);

// ./test/core/try_catch.wast:202
assert_return(() => invoke($1, `throw-catch-param-f64`, [value("f64", 10.5)]), [value("f64", 10.5)]);

// ./test/core/try_catch.wast:204
assert_return(() => invoke($1, `catch-param-i32`, [5]), [value("i32", 5)]);

// ./test/core/try_catch.wast:206
assert_return(() => invoke($1, `catch-imported`, []), [value("i32", 2)]);

// ./test/core/try_catch.wast:208
assert_return(() => invoke($1, `catchless-try`, [0]), [value("i32", 0)]);

// ./test/core/try_catch.wast:209
assert_return(() => invoke($1, `catchless-try`, [1]), [value("i32", 1)]);

// ./test/core/try_catch.wast:214
let $2 = instantiate(`(module
  (func $$imported-throw (import "test" "throw"))
  (tag $$e0)

  (func (export "imported-mismatch") (result i32)
    (try (result i32)
      (do
        (try (result i32)
          (do
            (i32.const 1)
            (call $$imported-throw)
          )
          (catch $$e0 (i32.const 2))
        )
      )
      (catch_all (i32.const 3))
    )
  )
)`);

// ./test/core/try_catch.wast:234
assert_return(() => invoke($2, `imported-mismatch`, []), [value("i32", 3)]);

// ./test/core/try_catch.wast:236
assert_malformed(() => instantiate(`(module (func (catch_all))) `), `unexpected token`);

// ./test/core/try_catch.wast:241
assert_malformed(
  () => instantiate(`(module (tag $$e) (func (catch $$e))) `),
  `unexpected token`,
);

// ./test/core/try_catch.wast:246
assert_malformed(
  () => instantiate(`(module (func (try (do) (catch_all) (catch_all)))) `),
  `unexpected token`,
);

// ./test/core/try_catch.wast:253
assert_invalid(
  () => instantiate(`(module (func (result i32) (try (result i32) (do))))`),
  `type mismatch: instruction requires [i32] but stack has []`,
);

// ./test/core/try_catch.wast:255
assert_invalid(
  () => instantiate(`(module (func (result i32) (try (result i32) (do (i64.const 42)))))`),
  `type mismatch: instruction requires [i32] but stack has [i64]`,
);

// ./test/core/try_catch.wast:257
assert_invalid(
  () => instantiate(`(module (tag) (func (try (do) (catch 0 (i32.const 42)))))`),
  `type mismatch: block requires [] but stack has [i32]`,
);

// ./test/core/try_catch.wast:259
assert_invalid(
  () => instantiate(`(module
                  (tag (param i64))
                  (func (result i32)
                    (try (result i32) (do (i32.const 42)) (catch 0))))`),
  `type mismatch: instruction requires [i32] but stack has [i64]`,
);

// ./test/core/try_catch.wast:264
assert_invalid(
  () => instantiate(`(module (func (try (do) (catch_all (i32.const 42)))))`),
  `type mismatch: block requires [] but stack has [i32]`,
);
