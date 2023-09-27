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

// ./test/core/table_fill.wast

// ./test/core/table_fill.wast:1
let $0 = instantiate(`(module
  (table $$t 10 externref)

  (func (export "fill") (param $$i i32) (param $$r externref) (param $$n i32)
    (table.fill $$t (local.get $$i) (local.get $$r) (local.get $$n))
  )

  (func (export "fill-abbrev") (param $$i i32) (param $$r externref) (param $$n i32)
    (table.fill (local.get $$i) (local.get $$r) (local.get $$n))
  )

  (func (export "get") (param $$i i32) (result externref)
    (table.get $$t (local.get $$i))
  )
)`);

// ./test/core/table_fill.wast:17
assert_return(() => invoke($0, `get`, [1]), [value('externref', null)]);

// ./test/core/table_fill.wast:18
assert_return(() => invoke($0, `get`, [2]), [value('externref', null)]);

// ./test/core/table_fill.wast:19
assert_return(() => invoke($0, `get`, [3]), [value('externref', null)]);

// ./test/core/table_fill.wast:20
assert_return(() => invoke($0, `get`, [4]), [value('externref', null)]);

// ./test/core/table_fill.wast:21
assert_return(() => invoke($0, `get`, [5]), [value('externref', null)]);

// ./test/core/table_fill.wast:23
assert_return(() => invoke($0, `fill`, [2, externref(1), 3]), []);

// ./test/core/table_fill.wast:24
assert_return(() => invoke($0, `get`, [1]), [value('externref', null)]);

// ./test/core/table_fill.wast:25
assert_return(() => invoke($0, `get`, [2]), [new ExternRefResult(1)]);

// ./test/core/table_fill.wast:26
assert_return(() => invoke($0, `get`, [3]), [new ExternRefResult(1)]);

// ./test/core/table_fill.wast:27
assert_return(() => invoke($0, `get`, [4]), [new ExternRefResult(1)]);

// ./test/core/table_fill.wast:28
assert_return(() => invoke($0, `get`, [5]), [value('externref', null)]);

// ./test/core/table_fill.wast:30
assert_return(() => invoke($0, `fill`, [4, externref(2), 2]), []);

// ./test/core/table_fill.wast:31
assert_return(() => invoke($0, `get`, [3]), [new ExternRefResult(1)]);

// ./test/core/table_fill.wast:32
assert_return(() => invoke($0, `get`, [4]), [new ExternRefResult(2)]);

// ./test/core/table_fill.wast:33
assert_return(() => invoke($0, `get`, [5]), [new ExternRefResult(2)]);

// ./test/core/table_fill.wast:34
assert_return(() => invoke($0, `get`, [6]), [value('externref', null)]);

// ./test/core/table_fill.wast:36
assert_return(() => invoke($0, `fill`, [4, externref(3), 0]), []);

// ./test/core/table_fill.wast:37
assert_return(() => invoke($0, `get`, [3]), [new ExternRefResult(1)]);

// ./test/core/table_fill.wast:38
assert_return(() => invoke($0, `get`, [4]), [new ExternRefResult(2)]);

// ./test/core/table_fill.wast:39
assert_return(() => invoke($0, `get`, [5]), [new ExternRefResult(2)]);

// ./test/core/table_fill.wast:41
assert_return(() => invoke($0, `fill`, [8, externref(4), 2]), []);

// ./test/core/table_fill.wast:42
assert_return(() => invoke($0, `get`, [7]), [value('externref', null)]);

// ./test/core/table_fill.wast:43
assert_return(() => invoke($0, `get`, [8]), [new ExternRefResult(4)]);

// ./test/core/table_fill.wast:44
assert_return(() => invoke($0, `get`, [9]), [new ExternRefResult(4)]);

// ./test/core/table_fill.wast:46
assert_return(() => invoke($0, `fill-abbrev`, [9, null, 1]), []);

// ./test/core/table_fill.wast:47
assert_return(() => invoke($0, `get`, [8]), [new ExternRefResult(4)]);

// ./test/core/table_fill.wast:48
assert_return(() => invoke($0, `get`, [9]), [value('externref', null)]);

// ./test/core/table_fill.wast:50
assert_return(() => invoke($0, `fill`, [10, externref(5), 0]), []);

// ./test/core/table_fill.wast:51
assert_return(() => invoke($0, `get`, [9]), [value('externref', null)]);

// ./test/core/table_fill.wast:53
assert_trap(() => invoke($0, `fill`, [8, externref(6), 3]), `out of bounds table access`);

// ./test/core/table_fill.wast:57
assert_return(() => invoke($0, `get`, [7]), [value('externref', null)]);

// ./test/core/table_fill.wast:58
assert_return(() => invoke($0, `get`, [8]), [new ExternRefResult(4)]);

// ./test/core/table_fill.wast:59
assert_return(() => invoke($0, `get`, [9]), [value('externref', null)]);

// ./test/core/table_fill.wast:61
assert_trap(() => invoke($0, `fill`, [11, null, 0]), `out of bounds table access`);

// ./test/core/table_fill.wast:66
assert_trap(() => invoke($0, `fill`, [11, null, 10]), `out of bounds table access`);

// ./test/core/table_fill.wast:74
assert_invalid(
  () => instantiate(`(module
    (table $$t 10 externref)
    (func $$type-index-value-length-empty-vs-i32-i32
      (table.fill $$t)
    )
  )`),
  `type mismatch`,
);

// ./test/core/table_fill.wast:83
assert_invalid(
  () => instantiate(`(module
    (table $$t 10 externref)
    (func $$type-index-empty-vs-i32
      (table.fill $$t (ref.null extern) (i32.const 1))
    )
  )`),
  `type mismatch`,
);

// ./test/core/table_fill.wast:92
assert_invalid(
  () => instantiate(`(module
    (table $$t 10 externref)
    (func $$type-value-empty-vs
      (table.fill $$t (i32.const 1) (i32.const 1))
    )
  )`),
  `type mismatch`,
);

// ./test/core/table_fill.wast:101
assert_invalid(
  () => instantiate(`(module
    (table $$t 10 externref)
    (func $$type-length-empty-vs-i32
      (table.fill $$t (i32.const 1) (ref.null extern))
    )
  )`),
  `type mismatch`,
);

// ./test/core/table_fill.wast:110
assert_invalid(
  () => instantiate(`(module
    (table $$t 0 externref)
    (func $$type-index-f32-vs-i32
      (table.fill $$t (f32.const 1) (ref.null extern) (i32.const 1))
    )
  )`),
  `type mismatch`,
);

// ./test/core/table_fill.wast:119
assert_invalid(
  () => instantiate(`(module
    (table $$t 0 funcref)
    (func $$type-value-vs-funcref (param $$r externref)
      (table.fill $$t (i32.const 1) (local.get $$r) (i32.const 1))
    )
  )`),
  `type mismatch`,
);

// ./test/core/table_fill.wast:128
assert_invalid(
  () => instantiate(`(module
    (table $$t 0 externref)
    (func $$type-length-f32-vs-i32
      (table.fill $$t (i32.const 1) (ref.null extern) (f32.const 1))
    )
  )`),
  `type mismatch`,
);

// ./test/core/table_fill.wast:138
assert_invalid(
  () => instantiate(`(module
    (table $$t1 1 externref)
    (table $$t2 1 funcref)
    (func $$type-value-externref-vs-funcref-multi (param $$r externref)
      (table.fill $$t2 (i32.const 0) (local.get $$r) (i32.const 1))
    )
  )`),
  `type mismatch`,
);

// ./test/core/table_fill.wast:149
assert_invalid(
  () => instantiate(`(module
    (table $$t 1 externref)
    (func $$type-result-empty-vs-num (result i32)
      (table.fill $$t (i32.const 0) (ref.null extern) (i32.const 1))
    )
  )`),
  `type mismatch`,
);
