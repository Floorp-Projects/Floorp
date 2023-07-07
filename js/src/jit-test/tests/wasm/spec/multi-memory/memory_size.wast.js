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

// ./test/core/memory_size.wast

// ./test/core/memory_size.wast:1
let $0 = instantiate(`(module
  (memory 0)
  (func (export "size") (result i32) (memory.size))
  (func (export "grow") (param $$sz i32) (drop (memory.grow (local.get $$sz))))
)`);

// ./test/core/memory_size.wast:7
assert_return(() => invoke($0, `size`, []), [value("i32", 0)]);

// ./test/core/memory_size.wast:8
assert_return(() => invoke($0, `grow`, [1]), []);

// ./test/core/memory_size.wast:9
assert_return(() => invoke($0, `size`, []), [value("i32", 1)]);

// ./test/core/memory_size.wast:10
assert_return(() => invoke($0, `grow`, [4]), []);

// ./test/core/memory_size.wast:11
assert_return(() => invoke($0, `size`, []), [value("i32", 5)]);

// ./test/core/memory_size.wast:12
assert_return(() => invoke($0, `grow`, [0]), []);

// ./test/core/memory_size.wast:13
assert_return(() => invoke($0, `size`, []), [value("i32", 5)]);

// ./test/core/memory_size.wast:15
let $1 = instantiate(`(module
  (memory 1)
  (func (export "size") (result i32) (memory.size))
  (func (export "grow") (param $$sz i32) (drop (memory.grow (local.get $$sz))))
)`);

// ./test/core/memory_size.wast:21
assert_return(() => invoke($1, `size`, []), [value("i32", 1)]);

// ./test/core/memory_size.wast:22
assert_return(() => invoke($1, `grow`, [1]), []);

// ./test/core/memory_size.wast:23
assert_return(() => invoke($1, `size`, []), [value("i32", 2)]);

// ./test/core/memory_size.wast:24
assert_return(() => invoke($1, `grow`, [4]), []);

// ./test/core/memory_size.wast:25
assert_return(() => invoke($1, `size`, []), [value("i32", 6)]);

// ./test/core/memory_size.wast:26
assert_return(() => invoke($1, `grow`, [0]), []);

// ./test/core/memory_size.wast:27
assert_return(() => invoke($1, `size`, []), [value("i32", 6)]);

// ./test/core/memory_size.wast:29
let $2 = instantiate(`(module
  (memory 0 2)
  (func (export "size") (result i32) (memory.size))
  (func (export "grow") (param $$sz i32) (drop (memory.grow (local.get $$sz))))
)`);

// ./test/core/memory_size.wast:35
assert_return(() => invoke($2, `size`, []), [value("i32", 0)]);

// ./test/core/memory_size.wast:36
assert_return(() => invoke($2, `grow`, [3]), []);

// ./test/core/memory_size.wast:37
assert_return(() => invoke($2, `size`, []), [value("i32", 0)]);

// ./test/core/memory_size.wast:38
assert_return(() => invoke($2, `grow`, [1]), []);

// ./test/core/memory_size.wast:39
assert_return(() => invoke($2, `size`, []), [value("i32", 1)]);

// ./test/core/memory_size.wast:40
assert_return(() => invoke($2, `grow`, [0]), []);

// ./test/core/memory_size.wast:41
assert_return(() => invoke($2, `size`, []), [value("i32", 1)]);

// ./test/core/memory_size.wast:42
assert_return(() => invoke($2, `grow`, [4]), []);

// ./test/core/memory_size.wast:43
assert_return(() => invoke($2, `size`, []), [value("i32", 1)]);

// ./test/core/memory_size.wast:44
assert_return(() => invoke($2, `grow`, [1]), []);

// ./test/core/memory_size.wast:45
assert_return(() => invoke($2, `size`, []), [value("i32", 2)]);

// ./test/core/memory_size.wast:47
let $3 = instantiate(`(module
  (memory 3 8)
  (func (export "size") (result i32) (memory.size))
  (func (export "grow") (param $$sz i32) (drop (memory.grow (local.get $$sz))))
)`);

// ./test/core/memory_size.wast:53
assert_return(() => invoke($3, `size`, []), [value("i32", 3)]);

// ./test/core/memory_size.wast:54
assert_return(() => invoke($3, `grow`, [1]), []);

// ./test/core/memory_size.wast:55
assert_return(() => invoke($3, `size`, []), [value("i32", 4)]);

// ./test/core/memory_size.wast:56
assert_return(() => invoke($3, `grow`, [3]), []);

// ./test/core/memory_size.wast:57
assert_return(() => invoke($3, `size`, []), [value("i32", 7)]);

// ./test/core/memory_size.wast:58
assert_return(() => invoke($3, `grow`, [0]), []);

// ./test/core/memory_size.wast:59
assert_return(() => invoke($3, `size`, []), [value("i32", 7)]);

// ./test/core/memory_size.wast:60
assert_return(() => invoke($3, `grow`, [2]), []);

// ./test/core/memory_size.wast:61
assert_return(() => invoke($3, `size`, []), [value("i32", 7)]);

// ./test/core/memory_size.wast:62
assert_return(() => invoke($3, `grow`, [1]), []);

// ./test/core/memory_size.wast:63
assert_return(() => invoke($3, `size`, []), [value("i32", 8)]);

// ./test/core/memory_size.wast:68
let $4 = instantiate(`(module
  (memory (export "mem1") 2 4)
  (memory (export "mem2") 0)
)`);

// ./test/core/memory_size.wast:72
register($4, `M`);

// ./test/core/memory_size.wast:74
let $5 = instantiate(`(module
  (memory $$mem1 (import "M" "mem1") 1 5)
  (memory $$mem2 (import "M" "mem2") 0)
  (memory $$mem3 3)
  (memory $$mem4 4 5)

  (func (export "size1") (result i32) (memory.size $$mem1))
  (func (export "size2") (result i32) (memory.size $$mem2))
  (func (export "size3") (result i32) (memory.size $$mem3))
  (func (export "size4") (result i32) (memory.size $$mem4))
)`);

// ./test/core/memory_size.wast:86
assert_return(() => invoke($5, `size1`, []), [value("i32", 2)]);

// ./test/core/memory_size.wast:87
assert_return(() => invoke($5, `size2`, []), [value("i32", 0)]);

// ./test/core/memory_size.wast:88
assert_return(() => invoke($5, `size3`, []), [value("i32", 3)]);

// ./test/core/memory_size.wast:89
assert_return(() => invoke($5, `size4`, []), [value("i32", 4)]);

// ./test/core/memory_size.wast:94
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-result-i32-vs-empty
      (memory.size)
    )
  )`),
  `type mismatch`,
);

// ./test/core/memory_size.wast:103
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-result-i32-vs-f32 (result f32)
      (memory.size)
    )
  )`),
  `type mismatch`,
);
