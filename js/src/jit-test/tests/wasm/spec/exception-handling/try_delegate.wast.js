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

// ./test/core/try_delegate.wast

// ./test/core/try_delegate.wast:3
let $0 = instantiate(`(module
  (tag $$e0)
  (tag $$e1)

  (func (export "delegate-no-throw") (result i32)
    (try $$t (result i32)
      (do (try (result i32) (do (i32.const 1)) (delegate $$t)))
      (catch $$e0 (i32.const 2))
    )
  )

  (func $$throw-if (param i32)
    (local.get 0)
    (if (then (throw $$e0)) (else))
  )

  (func (export "delegate-throw") (param i32) (result i32)
    (try $$t (result i32)
      (do
        (try (result i32)
          (do (local.get 0) (call $$throw-if) (i32.const 1))
          (delegate $$t)
        )
      )
      (catch $$e0 (i32.const 2))
    )
  )

  (func (export "delegate-skip") (result i32)
    (try $$t (result i32)
      (do
        (try (result i32)
          (do
            (try (result i32)
              (do (throw $$e0) (i32.const 1))
              (delegate $$t)
            )
          )
          (catch $$e0 (i32.const 2))
        )
      )
      (catch $$e0 (i32.const 3))
    )
  )

  (func (export "delegate-to-block") (result i32)
    (try (result i32)
      (do (block (try (do (throw $$e0)) (delegate 0)))
          (i32.const 0))
      (catch_all (i32.const 1)))
  )

  (func (export "delegate-to-catch") (result i32)
    (try (result i32)
      (do (try
            (do (throw $$e0))
            (catch $$e0
              (try (do (rethrow 1)) (delegate 0))))
          (i32.const 0))
      (catch_all (i32.const 1)))
  )

  (func (export "delegate-to-caller-trivial")
    (try
      (do (throw $$e0))
      (delegate 0)))

  (func (export "delegate-to-caller-skipping")
    (try (do (try (do (throw $$e0)) (delegate 1))) (catch_all))
  )

  (func $$select-tag (param i32)
    (block (block (block (local.get 0) (br_table 0 1 2)) (return)) (throw $$e0))
    (throw $$e1)
  )

  (func (export "delegate-merge") (param i32 i32) (result i32)
    (try $$t (result i32)
      (do
        (local.get 0)
        (call $$select-tag)
        (try
          (result i32)
          (do (local.get 1) (call $$select-tag) (i32.const 1))
          (delegate $$t)
        )
      )
      (catch $$e0 (i32.const 2))
    )
  )

  (func (export "delegate-throw-no-catch") (result i32)
    (try (result i32)
      (do (try (result i32) (do (throw $$e0) (i32.const 1)) (delegate 0)))
      (catch $$e1 (i32.const 2))
    )
  )

  (func (export "delegate-correct-targets") (result i32)
    (try (result i32)
      (do (try $$l3
            (do (try $$l2
                  (do (try $$l1
                        (do (try $$l0
                              (do (try
                                    (do (throw $$e0))
                                    (delegate $$l1)))
                              (catch_all unreachable)))
                        (delegate $$l3)))
                  (catch_all unreachable)))
            (catch_all (try
                         (do (throw $$e0))
                         (delegate $$l3))))
          unreachable)
      (catch_all (i32.const 1))))
)`);

// ./test/core/try_delegate.wast:150
assert_return(() => invoke($0, `delegate-no-throw`, []), [value("i32", 1)]);

// ./test/core/try_delegate.wast:152
assert_return(() => invoke($0, `delegate-throw`, [0]), [value("i32", 1)]);

// ./test/core/try_delegate.wast:153
assert_return(() => invoke($0, `delegate-throw`, [1]), [value("i32", 2)]);

// ./test/core/try_delegate.wast:155
assert_exception(() => invoke($0, `delegate-throw-no-catch`, []));

// ./test/core/try_delegate.wast:157
assert_return(() => invoke($0, `delegate-merge`, [1, 0]), [value("i32", 2)]);

// ./test/core/try_delegate.wast:158
assert_exception(() => invoke($0, `delegate-merge`, [2, 0]));

// ./test/core/try_delegate.wast:159
assert_return(() => invoke($0, `delegate-merge`, [0, 1]), [value("i32", 2)]);

// ./test/core/try_delegate.wast:160
assert_exception(() => invoke($0, `delegate-merge`, [0, 2]));

// ./test/core/try_delegate.wast:161
assert_return(() => invoke($0, `delegate-merge`, [0, 0]), [value("i32", 1)]);

// ./test/core/try_delegate.wast:163
assert_return(() => invoke($0, `delegate-skip`, []), [value("i32", 3)]);

// ./test/core/try_delegate.wast:165
assert_return(() => invoke($0, `delegate-to-block`, []), [value("i32", 1)]);

// ./test/core/try_delegate.wast:166
assert_return(() => invoke($0, `delegate-to-catch`, []), [value("i32", 1)]);

// ./test/core/try_delegate.wast:168
assert_exception(() => invoke($0, `delegate-to-caller-trivial`, []));

// ./test/core/try_delegate.wast:169
assert_exception(() => invoke($0, `delegate-to-caller-skipping`, []));

// ./test/core/try_delegate.wast:171
assert_return(() => invoke($0, `delegate-correct-targets`, []), [value("i32", 1)]);

// ./test/core/try_delegate.wast:176
assert_malformed(() => instantiate(`(module (func (delegate 0))) `), `unexpected token`);

// ./test/core/try_delegate.wast:181
assert_malformed(
  () => instantiate(`(module (tag $$e) (func (try (do) (catch $$e) (delegate 0)))) `),
  `unexpected token`,
);

// ./test/core/try_delegate.wast:186
assert_malformed(
  () => instantiate(`(module (func (try (do) (catch_all) (delegate 0)))) `),
  `unexpected token`,
);

// ./test/core/try_delegate.wast:191
assert_malformed(
  () => instantiate(`(module (func (try (do) (delegate) (delegate 0)))) `),
  `unexpected token`,
);

// ./test/core/try_delegate.wast:196
assert_invalid(
  () => instantiate(`(module (func (try (do) (delegate 1))))`),
  `unknown label`,
);
