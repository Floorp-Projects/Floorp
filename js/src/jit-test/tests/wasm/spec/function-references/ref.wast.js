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

// ./test/core/ref.wast

// ./test/core/ref.wast:3
let $0 = instantiate(`(module
  (type $$t (func))

  (func
    (param
      funcref
      externref
      (ref func)
      (ref extern)
      (ref 0)
      (ref $$t)
      (ref 0)
      (ref $$t)
      (ref null func)
      (ref null extern)
      (ref null 0)
      (ref null $$t)
    )
  )
)`);

// ./test/core/ref.wast:27
assert_invalid(
  () =>
    instantiate(
      `(module (type $$type-func-param-invalid (func (param (ref 1)))))`,
    ),
  `unknown type`,
);

// ./test/core/ref.wast:31
assert_invalid(
  () =>
    instantiate(
      `(module (type $$type-func-result-invalid (func (result (ref 1)))))`,
    ),
  `unknown type`,
);

// ./test/core/ref.wast:36
assert_invalid(
  () =>
    instantiate(`(module (global $$global-invalid (ref null 1) (ref.null 1)))`),
  `unknown type`,
);

// ./test/core/ref.wast:41
assert_invalid(
  () => instantiate(`(module (table $$table-invalid 10 (ref null 1)))`),
  `unknown type`,
);

// ./test/core/ref.wast:46
assert_invalid(
  () => instantiate(`(module (elem $$elem-invalid (ref 1)))`),
  `unknown type`,
);

// ./test/core/ref.wast:51
assert_invalid(
  () => instantiate(`(module (func $$func-param-invalid (param (ref 1))))`),
  `unknown type`,
);

// ./test/core/ref.wast:55
assert_invalid(
  () => instantiate(`(module (func $$func-result-invalid (result (ref 1))))`),
  `unknown type`,
);

// ./test/core/ref.wast:59
assert_invalid(
  () =>
    instantiate(`(module (func $$func-local-invalid (local (ref null 1))))`),
  `unknown type`,
);

// ./test/core/ref.wast:64
assert_invalid(
  () =>
    instantiate(
      `(module (func $$block-result-invalid (drop (block (result (ref 1)) (unreachable)))))`,
    ),
  `unknown type`,
);

// ./test/core/ref.wast:68
assert_invalid(
  () =>
    instantiate(
      `(module (func $$loop-result-invalid (drop (loop (result (ref 1)) (unreachable)))))`,
    ),
  `unknown type`,
);

// ./test/core/ref.wast:72
assert_invalid(
  () =>
    instantiate(
      `(module (func $$if-invalid (drop (if (result (ref 1)) (then) (else)))))`,
    ),
  `unknown type`,
);

// ./test/core/ref.wast:77
assert_invalid(
  () =>
    instantiate(
      `(module (func $$select-result-invalid (drop (select (result (ref 1)) (unreachable)))))`,
    ),
  `unknown type`,
);
