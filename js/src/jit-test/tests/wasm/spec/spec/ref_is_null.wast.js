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

// ./test/core/ref_is_null.wast

// ./test/core/ref_is_null.wast:1
let $0 = instantiate(`(module
  (func $$f1 (export "funcref") (param $$x funcref) (result i32)
    (ref.is_null (local.get $$x))
  )
  (func $$f2 (export "externref") (param $$x externref) (result i32)
    (ref.is_null (local.get $$x))
  )

  (table $$t1 2 funcref)
  (table $$t2 2 externref)
  (elem (table $$t1) (i32.const 1) func $$dummy)
  (func $$dummy)

  (func (export "init") (param $$r externref)
    (table.set $$t2 (i32.const 1) (local.get $$r))
  )
  (func (export "deinit")
    (table.set $$t1 (i32.const 1) (ref.null func))
    (table.set $$t2 (i32.const 1) (ref.null extern))
  )

  (func (export "funcref-elem") (param $$x i32) (result i32)
    (call $$f1 (table.get $$t1 (local.get $$x)))
  )
  (func (export "externref-elem") (param $$x i32) (result i32)
    (call $$f2 (table.get $$t2 (local.get $$x)))
  )
)`);

// ./test/core/ref_is_null.wast:30
assert_return(() => invoke($0, `funcref`, [null]), [value("i32", 1)]);

// ./test/core/ref_is_null.wast:31
assert_return(() => invoke($0, `externref`, [null]), [value("i32", 1)]);

// ./test/core/ref_is_null.wast:33
assert_return(() => invoke($0, `externref`, [externref(1)]), [value("i32", 0)]);

// ./test/core/ref_is_null.wast:35
invoke($0, `init`, [externref(0)]);

// ./test/core/ref_is_null.wast:37
assert_return(() => invoke($0, `funcref-elem`, [0]), [value("i32", 1)]);

// ./test/core/ref_is_null.wast:38
assert_return(() => invoke($0, `externref-elem`, [0]), [value("i32", 1)]);

// ./test/core/ref_is_null.wast:40
assert_return(() => invoke($0, `funcref-elem`, [1]), [value("i32", 0)]);

// ./test/core/ref_is_null.wast:41
assert_return(() => invoke($0, `externref-elem`, [1]), [value("i32", 0)]);

// ./test/core/ref_is_null.wast:43
invoke($0, `deinit`, []);

// ./test/core/ref_is_null.wast:45
assert_return(() => invoke($0, `funcref-elem`, [0]), [value("i32", 1)]);

// ./test/core/ref_is_null.wast:46
assert_return(() => invoke($0, `externref-elem`, [0]), [value("i32", 1)]);

// ./test/core/ref_is_null.wast:48
assert_return(() => invoke($0, `funcref-elem`, [1]), [value("i32", 1)]);

// ./test/core/ref_is_null.wast:49
assert_return(() => invoke($0, `externref-elem`, [1]), [value("i32", 1)]);

// ./test/core/ref_is_null.wast:51
assert_invalid(
  () => instantiate(`(module (func $$ref-vs-num (param i32) (ref.is_null (local.get 0))))`),
  `type mismatch`,
);

// ./test/core/ref_is_null.wast:55
assert_invalid(
  () => instantiate(`(module (func $$ref-vs-empty (ref.is_null)))`),
  `type mismatch`,
);
