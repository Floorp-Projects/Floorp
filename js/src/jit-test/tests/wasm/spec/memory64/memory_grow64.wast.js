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

// ./test/core/memory_grow64.wast

// ./test/core/memory_grow64.wast:1
let $0 = instantiate(`(module
    (memory i64 0)

    (func (export "load_at_zero") (result i32) (i32.load (i64.const 0)))
    (func (export "store_at_zero") (i32.store (i64.const 0) (i32.const 2)))

    (func (export "load_at_page_size") (result i32) (i32.load (i64.const 0x10000)))
    (func (export "store_at_page_size") (i32.store (i64.const 0x10000) (i32.const 3)))

    (func (export "grow") (param $$sz i64) (result i64) (memory.grow (local.get $$sz)))
    (func (export "size") (result i64) (memory.size))
)`);

// ./test/core/memory_grow64.wast:14
assert_return(() => invoke($0, `size`, []), [value("i64", 0n)]);

// ./test/core/memory_grow64.wast:15
assert_trap(() => invoke($0, `store_at_zero`, []), `out of bounds memory access`);

// ./test/core/memory_grow64.wast:16
assert_trap(() => invoke($0, `load_at_zero`, []), `out of bounds memory access`);

// ./test/core/memory_grow64.wast:17
assert_trap(() => invoke($0, `store_at_page_size`, []), `out of bounds memory access`);

// ./test/core/memory_grow64.wast:18
assert_trap(() => invoke($0, `load_at_page_size`, []), `out of bounds memory access`);

// ./test/core/memory_grow64.wast:19
assert_return(() => invoke($0, `grow`, [1n]), [value("i64", 0n)]);

// ./test/core/memory_grow64.wast:20
assert_return(() => invoke($0, `size`, []), [value("i64", 1n)]);

// ./test/core/memory_grow64.wast:21
assert_return(() => invoke($0, `load_at_zero`, []), [value("i32", 0)]);

// ./test/core/memory_grow64.wast:22
assert_return(() => invoke($0, `store_at_zero`, []), []);

// ./test/core/memory_grow64.wast:23
assert_return(() => invoke($0, `load_at_zero`, []), [value("i32", 2)]);

// ./test/core/memory_grow64.wast:24
assert_trap(() => invoke($0, `store_at_page_size`, []), `out of bounds memory access`);

// ./test/core/memory_grow64.wast:25
assert_trap(() => invoke($0, `load_at_page_size`, []), `out of bounds memory access`);

// ./test/core/memory_grow64.wast:26
assert_return(() => invoke($0, `grow`, [4n]), [value("i64", 1n)]);

// ./test/core/memory_grow64.wast:27
assert_return(() => invoke($0, `size`, []), [value("i64", 5n)]);

// ./test/core/memory_grow64.wast:28
assert_return(() => invoke($0, `load_at_zero`, []), [value("i32", 2)]);

// ./test/core/memory_grow64.wast:29
assert_return(() => invoke($0, `store_at_zero`, []), []);

// ./test/core/memory_grow64.wast:30
assert_return(() => invoke($0, `load_at_zero`, []), [value("i32", 2)]);

// ./test/core/memory_grow64.wast:31
assert_return(() => invoke($0, `load_at_page_size`, []), [value("i32", 0)]);

// ./test/core/memory_grow64.wast:32
assert_return(() => invoke($0, `store_at_page_size`, []), []);

// ./test/core/memory_grow64.wast:33
assert_return(() => invoke($0, `load_at_page_size`, []), [value("i32", 3)]);

// ./test/core/memory_grow64.wast:36
let $1 = instantiate(`(module
  (memory i64 0)
  (func (export "grow") (param i64) (result i64) (memory.grow (local.get 0)))
)`);

// ./test/core/memory_grow64.wast:41
assert_return(() => invoke($1, `grow`, [0n]), [value("i64", 0n)]);

// ./test/core/memory_grow64.wast:42
assert_return(() => invoke($1, `grow`, [1n]), [value("i64", 0n)]);

// ./test/core/memory_grow64.wast:43
assert_return(() => invoke($1, `grow`, [0n]), [value("i64", 1n)]);

// ./test/core/memory_grow64.wast:44
assert_return(() => invoke($1, `grow`, [2n]), [value("i64", 1n)]);

// ./test/core/memory_grow64.wast:45
assert_return(() => invoke($1, `grow`, [800n]), [value("i64", 3n)]);

// ./test/core/memory_grow64.wast:46
assert_return(() => invoke($1, `grow`, [1n]), [value("i64", 803n)]);

// ./test/core/memory_grow64.wast:48
let $2 = instantiate(`(module
  (memory i64 0 10)
  (func (export "grow") (param i64) (result i64) (memory.grow (local.get 0)))
)`);

// ./test/core/memory_grow64.wast:53
assert_return(() => invoke($2, `grow`, [0n]), [value("i64", 0n)]);

// ./test/core/memory_grow64.wast:54
assert_return(() => invoke($2, `grow`, [1n]), [value("i64", 0n)]);

// ./test/core/memory_grow64.wast:55
assert_return(() => invoke($2, `grow`, [1n]), [value("i64", 1n)]);

// ./test/core/memory_grow64.wast:56
assert_return(() => invoke($2, `grow`, [2n]), [value("i64", 2n)]);

// ./test/core/memory_grow64.wast:57
assert_return(() => invoke($2, `grow`, [6n]), [value("i64", 4n)]);

// ./test/core/memory_grow64.wast:58
assert_return(() => invoke($2, `grow`, [0n]), [value("i64", 10n)]);

// ./test/core/memory_grow64.wast:59
assert_return(() => invoke($2, `grow`, [1n]), [value("i64", -1n)]);

// ./test/core/memory_grow64.wast:60
assert_return(() => invoke($2, `grow`, [65536n]), [value("i64", -1n)]);

// ./test/core/memory_grow64.wast:64
let $3 = instantiate(`(module
  (memory i64 1)
  (func (export "grow") (param i64) (result i64)
    (memory.grow (local.get 0))
  )
  (func (export "check-memory-zero") (param i64 i64) (result i32)
    (local i32)
    (local.set 2 (i32.const 1))
    (block
      (loop
        (local.set 2 (i32.load8_u (local.get 0)))
        (br_if 1 (i32.ne (local.get 2) (i32.const 0)))
        (br_if 1 (i64.ge_u (local.get 0) (local.get 1)))
        (local.set 0 (i64.add (local.get 0) (i64.const 1)))
        (br_if 0 (i64.le_u (local.get 0) (local.get 1)))
      )
    )
    (local.get 2)
  )
)`);

// ./test/core/memory_grow64.wast:85
assert_return(() => invoke($3, `check-memory-zero`, [0n, 65535n]), [value("i32", 0)]);

// ./test/core/memory_grow64.wast:86
assert_return(() => invoke($3, `grow`, [1n]), [value("i64", 1n)]);

// ./test/core/memory_grow64.wast:87
assert_return(() => invoke($3, `check-memory-zero`, [65536n, 131071n]), [value("i32", 0)]);

// ./test/core/memory_grow64.wast:88
assert_return(() => invoke($3, `grow`, [1n]), [value("i64", 2n)]);

// ./test/core/memory_grow64.wast:89
assert_return(() => invoke($3, `check-memory-zero`, [131072n, 196607n]), [value("i32", 0)]);

// ./test/core/memory_grow64.wast:90
assert_return(() => invoke($3, `grow`, [1n]), [value("i64", 3n)]);

// ./test/core/memory_grow64.wast:91
assert_return(() => invoke($3, `check-memory-zero`, [196608n, 262143n]), [value("i32", 0)]);

// ./test/core/memory_grow64.wast:92
assert_return(() => invoke($3, `grow`, [1n]), [value("i64", 4n)]);

// ./test/core/memory_grow64.wast:93
assert_return(() => invoke($3, `check-memory-zero`, [262144n, 327679n]), [value("i32", 0)]);

// ./test/core/memory_grow64.wast:94
assert_return(() => invoke($3, `grow`, [1n]), [value("i64", 5n)]);

// ./test/core/memory_grow64.wast:95
assert_return(() => invoke($3, `check-memory-zero`, [327680n, 393215n]), [value("i32", 0)]);
