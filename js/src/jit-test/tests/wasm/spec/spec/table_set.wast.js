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

// ./test/core/table_set.wast

// ./test/core/table_set.wast:1
let $0 = instantiate(`(module
  (table $$t2 1 externref)
  (table $$t3 2 funcref)
  (elem (table $$t3) (i32.const 1) func $$dummy)
  (func $$dummy)

  (func (export "get-externref") (param $$i i32) (result externref)
    (table.get $$t2 (local.get $$i))
  )
  (func $$f3 (export "get-funcref") (param $$i i32) (result funcref)
    (table.get $$t3 (local.get $$i))
  )

  (func (export "set-externref") (param $$i i32) (param $$r externref)
    (table.set $$t2 (local.get $$i) (local.get $$r))
  )
  (func (export "set-funcref") (param $$i i32) (param $$r funcref)
    (table.set $$t3 (local.get $$i) (local.get $$r))
  )
  (func (export "set-funcref-from") (param $$i i32) (param $$j i32)
    (table.set $$t3 (local.get $$i) (table.get $$t3 (local.get $$j)))
  )

  (func (export "is_null-funcref") (param $$i i32) (result i32)
    (ref.is_null (call $$f3 (local.get $$i)))
  )
)`);

// ./test/core/table_set.wast:29
assert_return(() => invoke($0, `get-externref`, [0]), [
  value("externref", null),
]);

// ./test/core/table_set.wast:30
assert_return(() => invoke($0, `set-externref`, [0, externref(1)]), []);

// ./test/core/table_set.wast:31
assert_return(() => invoke($0, `get-externref`, [0]), [
  value("externref", externref(1)),
]);

// ./test/core/table_set.wast:32
assert_return(() => invoke($0, `set-externref`, [0, null]), []);

// ./test/core/table_set.wast:33
assert_return(() => invoke($0, `get-externref`, [0]), [
  value("externref", null),
]);

// ./test/core/table_set.wast:35
assert_return(() => invoke($0, `get-funcref`, [0]), [value("funcref", null)]);

// ./test/core/table_set.wast:36
assert_return(() => invoke($0, `set-funcref-from`, [0, 1]), []);

// ./test/core/table_set.wast:37
assert_return(() => invoke($0, `is_null-funcref`, [0]), [value("i32", 0)]);

// ./test/core/table_set.wast:38
assert_return(() => invoke($0, `set-funcref`, [0, null]), []);

// ./test/core/table_set.wast:39
assert_return(() => invoke($0, `get-funcref`, [0]), [value("funcref", null)]);

// ./test/core/table_set.wast:41
assert_trap(
  () => invoke($0, `set-externref`, [2, null]),
  `out of bounds table access`,
);

// ./test/core/table_set.wast:42
assert_trap(
  () => invoke($0, `set-funcref`, [3, null]),
  `out of bounds table access`,
);

// ./test/core/table_set.wast:43
assert_trap(
  () => invoke($0, `set-externref`, [-1, null]),
  `out of bounds table access`,
);

// ./test/core/table_set.wast:44
assert_trap(
  () => invoke($0, `set-funcref`, [-1, null]),
  `out of bounds table access`,
);

// ./test/core/table_set.wast:46
assert_trap(
  () => invoke($0, `set-externref`, [2, externref(0)]),
  `out of bounds table access`,
);

// ./test/core/table_set.wast:47
assert_trap(
  () => invoke($0, `set-funcref-from`, [3, 1]),
  `out of bounds table access`,
);

// ./test/core/table_set.wast:48
assert_trap(
  () => invoke($0, `set-externref`, [-1, externref(0)]),
  `out of bounds table access`,
);

// ./test/core/table_set.wast:49
assert_trap(
  () => invoke($0, `set-funcref-from`, [-1, 1]),
  `out of bounds table access`,
);

// ./test/core/table_set.wast:54
assert_invalid(() =>
  instantiate(`(module
    (table $$t 10 externref)
    (func $$type-index-value-empty-vs-i32-externref
      (table.set $$t)
    )
  )`), `type mismatch`);

// ./test/core/table_set.wast:63
assert_invalid(() =>
  instantiate(`(module
    (table $$t 10 externref)
    (func $$type-index-empty-vs-i32
      (table.set $$t (ref.null extern))
    )
  )`), `type mismatch`);

// ./test/core/table_set.wast:72
assert_invalid(() =>
  instantiate(`(module
    (table $$t 10 externref)
    (func $$type-value-empty-vs-externref
      (table.set $$t (i32.const 1))
    )
  )`), `type mismatch`);

// ./test/core/table_set.wast:81
assert_invalid(() =>
  instantiate(`(module
    (table $$t 10 externref)
    (func $$type-size-f32-vs-i32
      (table.set $$t (f32.const 1) (ref.null extern))
    )
  )`), `type mismatch`);

// ./test/core/table_set.wast:90
assert_invalid(() =>
  instantiate(`(module
    (table $$t 10 funcref)
    (func $$type-value-externref-vs-funcref (param $$r externref)
      (table.set $$t (i32.const 1) (local.get $$r))
    )
  )`), `type mismatch`);

// ./test/core/table_set.wast:100
assert_invalid(() =>
  instantiate(`(module
    (table $$t1 1 externref)
    (table $$t2 1 funcref)
    (func $$type-value-externref-vs-funcref-multi (param $$r externref)
      (table.set $$t2 (i32.const 0) (local.get $$r))
    )
  )`), `type mismatch`);

// ./test/core/table_set.wast:111
assert_invalid(() =>
  instantiate(`(module
    (table $$t 10 externref)
    (func $$type-result-empty-vs-num (result i32)
      (table.set $$t (i32.const 0) (ref.null extern))
    )
  )`), `type mismatch`);
