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

// ./test/core/throw_ref.wast

// ./test/core/throw_ref.wast:3
let $0 = instantiate(`(module
  (tag $$e0)
  (tag $$e1)

  (func (export "catch-throw_ref-0")
    (block $$h (result exnref)
      (try_table (catch_ref $$e0 $$h) (throw $$e0))
      (unreachable)
    )
    (throw_ref)
  )

  (func (export "catch-throw_ref-1") (param i32) (result i32)
    (block $$h (result exnref)
      (try_table (result i32) (catch_ref $$e0 $$h) (throw $$e0))
      (return)
    )
    (if (param exnref) (i32.eqz (local.get 0))
      (then (throw_ref))
      (else (drop))
    )
    (i32.const 23)
  )

  (func (export "catchall-throw_ref-0")
    (block $$h (result exnref)
      (try_table (result exnref) (catch_all_ref $$h) (throw $$e0))
    )
    (throw_ref)
  )

  (func (export "catchall-throw_ref-1") (param i32) (result i32)
    (block $$h (result exnref)
      (try_table (result i32) (catch_all_ref $$h) (throw $$e0))
      (return)
    )
    (if (param exnref) (i32.eqz (local.get 0))
      (then (throw_ref))
      (else (drop))
    )
    (i32.const 23)
  )

  (func (export "throw_ref-nested") (param i32) (result i32)
    (local $$exn1 exnref)
    (local $$exn2 exnref)
    (block $$h1 (result exnref)
      (try_table (result i32) (catch_ref $$e1 $$h1) (throw $$e1))
      (return)
    )
    (local.set $$exn1)
    (block $$h2 (result exnref)
      (try_table (result i32) (catch_ref $$e0 $$h2) (throw $$e0))
      (return)
    )
    (local.set $$exn2)
    (if (i32.eq (local.get 0) (i32.const 0))
      (then (throw_ref (local.get $$exn1)))
    )
    (if (i32.eq (local.get 0) (i32.const 1))
      (then (throw_ref (local.get $$exn2)))
    )
    (i32.const 23)
  )

  (func (export "throw_ref-recatch") (param i32) (result i32)
    (local $$e exnref)
    (block $$h1 (result exnref)
      (try_table (result i32) (catch_ref $$e0 $$h1) (throw $$e0))
      (return)
    )
    (local.set $$e)
    (block $$h2 (result exnref)
      (try_table (result i32) (catch_ref $$e0 $$h2)
        (if (i32.eqz (local.get 0))
          (then (throw_ref (local.get $$e)))
        )
        (i32.const 42)
      )
      (return)
    )
    (drop) (i32.const 23)
  )

  (func (export "throw_ref-stack-polymorphism")
    (local $$e exnref)
    (block $$h (result exnref)
      (try_table (result f64) (catch_ref $$e0 $$h) (throw $$e0))
      (unreachable)
    )
    (local.set $$e)
    (i32.const 1)
    (throw_ref (local.get $$e))
  )
)`);

// ./test/core/throw_ref.wast:99
assert_exception(() => invoke($0, `catch-throw_ref-0`, []));

// ./test/core/throw_ref.wast:101
assert_exception(() => invoke($0, `catch-throw_ref-1`, [0]));

// ./test/core/throw_ref.wast:102
assert_return(() => invoke($0, `catch-throw_ref-1`, [1]), [value("i32", 23)]);

// ./test/core/throw_ref.wast:104
assert_exception(() => invoke($0, `catchall-throw_ref-0`, []));

// ./test/core/throw_ref.wast:106
assert_exception(() => invoke($0, `catchall-throw_ref-1`, [0]));

// ./test/core/throw_ref.wast:107
assert_return(() => invoke($0, `catchall-throw_ref-1`, [1]), [value("i32", 23)]);

// ./test/core/throw_ref.wast:108
assert_exception(() => invoke($0, `throw_ref-nested`, [0]));

// ./test/core/throw_ref.wast:109
assert_exception(() => invoke($0, `throw_ref-nested`, [1]));

// ./test/core/throw_ref.wast:110
assert_return(() => invoke($0, `throw_ref-nested`, [2]), [value("i32", 23)]);

// ./test/core/throw_ref.wast:112
assert_return(() => invoke($0, `throw_ref-recatch`, [0]), [value("i32", 23)]);

// ./test/core/throw_ref.wast:113
assert_return(() => invoke($0, `throw_ref-recatch`, [1]), [value("i32", 42)]);

// ./test/core/throw_ref.wast:115
assert_exception(() => invoke($0, `throw_ref-stack-polymorphism`, []));

// ./test/core/throw_ref.wast:117
assert_invalid(() => instantiate(`(module (func (throw_ref)))`), `type mismatch`);

// ./test/core/throw_ref.wast:118
assert_invalid(() => instantiate(`(module (func (block (throw_ref))))`), `type mismatch`);
