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

// ./test/core/rethrow.wast

// ./test/core/rethrow.wast:3
let $0 = instantiate(`(module
  (tag $$e0)
  (tag $$e1)

  (func (export "catch-rethrow-0")
    (try
      (do (throw $$e0))
      (catch $$e0 (rethrow 0))
    )
  )

  (func (export "catch-rethrow-1") (param i32) (result i32)
    (try (result i32)
      (do (throw $$e0))
      (catch $$e0
        (if (i32.eqz (local.get 0)) (then (rethrow 1))) (i32.const 23)
      )
    )
  )

  (func (export "catchall-rethrow-0")
    (try
      (do (throw $$e0))
      (catch_all (rethrow 0))
    )
  )

  (func (export "catchall-rethrow-1") (param i32) (result i32)
    (try (result i32)
      (do (throw $$e0))
      (catch_all
        (if (i32.eqz (local.get 0)) (then (rethrow 1))) (i32.const 23)
      )
    )
  )

  (func (export "rethrow-nested") (param i32) (result i32)
    (try (result i32)
      (do (throw $$e1))
      (catch $$e1
        (try (result i32)
          (do (throw $$e0))
          (catch $$e0
            (if (i32.eq (local.get 0) (i32.const 0)) (then (rethrow 1)))
            (if (i32.eq (local.get 0) (i32.const 1)) (then (rethrow 2)))
            (i32.const 23)
          )
        )
      )
    )
  )

  (func (export "rethrow-recatch") (param i32) (result i32)
    (try (result i32)
      (do (throw $$e0))
      (catch $$e0
        (try (result i32)
         (do (if (i32.eqz (local.get 0)) (then (rethrow 2))) (i32.const 42))
         (catch $$e0 (i32.const 23))
        )
      )
    )
  )

  (func (export "rethrow-stack-polymorphism")
    (try
      (do (throw $$e0))
      (catch $$e0 (i32.const 1) (rethrow 0))
    )
  )
)`);

// ./test/core/rethrow.wast:75
assert_exception(() => invoke($0, `catch-rethrow-0`, []));

// ./test/core/rethrow.wast:77
assert_exception(() => invoke($0, `catch-rethrow-1`, [0]));

// ./test/core/rethrow.wast:78
assert_return(() => invoke($0, `catch-rethrow-1`, [1]), [value("i32", 23)]);

// ./test/core/rethrow.wast:80
assert_exception(() => invoke($0, `catchall-rethrow-0`, []));

// ./test/core/rethrow.wast:82
assert_exception(() => invoke($0, `catchall-rethrow-1`, [0]));

// ./test/core/rethrow.wast:83
assert_return(() => invoke($0, `catchall-rethrow-1`, [1]), [value("i32", 23)]);

// ./test/core/rethrow.wast:84
assert_exception(() => invoke($0, `rethrow-nested`, [0]));

// ./test/core/rethrow.wast:85
assert_exception(() => invoke($0, `rethrow-nested`, [1]));

// ./test/core/rethrow.wast:86
assert_return(() => invoke($0, `rethrow-nested`, [2]), [value("i32", 23)]);

// ./test/core/rethrow.wast:88
assert_return(() => invoke($0, `rethrow-recatch`, [0]), [value("i32", 23)]);

// ./test/core/rethrow.wast:89
assert_return(() => invoke($0, `rethrow-recatch`, [1]), [value("i32", 42)]);

// ./test/core/rethrow.wast:91
assert_exception(() => invoke($0, `rethrow-stack-polymorphism`, []));

// ./test/core/rethrow.wast:93
assert_invalid(() => instantiate(`(module (func (rethrow 0)))`), `invalid rethrow label`);

// ./test/core/rethrow.wast:94
assert_invalid(
  () => instantiate(`(module (func (block (rethrow 0))))`),
  `invalid rethrow label`,
);

// ./test/core/rethrow.wast:95
assert_invalid(
  () => instantiate(`(module (func (try (do (rethrow 0)) (delegate 0))))`),
  `invalid rethrow label`,
);
