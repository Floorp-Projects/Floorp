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

// ./test/core/throw.wast

// ./test/core/throw.wast:3
let $0 = instantiate(`(module
  (tag $$e0)
  (tag $$e-i32 (param i32))
  (tag $$e-f32 (param f32))
  (tag $$e-i64 (param i64))
  (tag $$e-f64 (param f64))
  (tag $$e-i32-i32 (param i32 i32))

  (func $$throw-if (export "throw-if") (param i32) (result i32)
    (local.get 0)
    (i32.const 0) (if (i32.ne) (then (throw $$e0)))
    (i32.const 0)
  )

  (func (export "throw-param-f32") (param f32) (local.get 0) (throw $$e-f32))

  (func (export "throw-param-i64") (param i64) (local.get 0) (throw $$e-i64))

  (func (export "throw-param-f64") (param f64) (local.get 0) (throw $$e-f64))

  (func (export "throw-polymorphic") (throw $$e0) (throw $$e-i32))

  (func (export "throw-polymorphic-block") (block (result i32) (throw $$e0)) (throw $$e-i32))

  (func $$throw-1-2 (i32.const 1) (i32.const 2) (throw $$e-i32-i32))
  (func (export "test-throw-1-2")
    (try
      (do (call $$throw-1-2))
      (catch $$e-i32-i32
        (i32.const 2)
        (if (i32.ne) (then (unreachable)))
        (i32.const 1)
        (if (i32.ne) (then (unreachable)))
      )
    )
  )
)`);

// ./test/core/throw.wast:41
assert_return(() => invoke($0, `throw-if`, [0]), [value("i32", 0)]);

// ./test/core/throw.wast:42
assert_exception(() => invoke($0, `throw-if`, [10]));

// ./test/core/throw.wast:43
assert_exception(() => invoke($0, `throw-if`, [-1]));

// ./test/core/throw.wast:45
assert_exception(() => invoke($0, `throw-param-f32`, [value("f32", 5)]));

// ./test/core/throw.wast:46
assert_exception(() => invoke($0, `throw-param-i64`, [5n]));

// ./test/core/throw.wast:47
assert_exception(() => invoke($0, `throw-param-f64`, [value("f64", 5)]));

// ./test/core/throw.wast:49
assert_exception(() => invoke($0, `throw-polymorphic`, []));

// ./test/core/throw.wast:50
assert_exception(() => invoke($0, `throw-polymorphic-block`, []));

// ./test/core/throw.wast:52
assert_return(() => invoke($0, `test-throw-1-2`, []), []);

// ./test/core/throw.wast:54
assert_invalid(() => instantiate(`(module (func (throw 0)))`), `unknown tag 0`);

// ./test/core/throw.wast:55
assert_invalid(
  () => instantiate(`(module (tag (param i32)) (func (throw 0)))`),
  `type mismatch: instruction requires [i32] but stack has []`,
);

// ./test/core/throw.wast:57
assert_invalid(
  () => instantiate(`(module (tag (param i32)) (func (i64.const 5) (throw 0)))`),
  `type mismatch: instruction requires [i32] but stack has [i64]`,
);
