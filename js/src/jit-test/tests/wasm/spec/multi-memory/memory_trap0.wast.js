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

// ./test/core/multi-memory/memory_trap0.wast

// ./test/core/multi-memory/memory_trap0.wast:1
let $0 = instantiate(`(module
    (memory 0)
    (memory 0)
    (memory $$m 1)

    (func $$addr_limit (result i32)
      (i32.mul (memory.size $$m) (i32.const 0x10000))
    )

    (func (export "store") (param $$i i32) (param $$v i32)
      (i32.store $$m (i32.add (call $$addr_limit) (local.get $$i)) (local.get $$v))
    )

    (func (export "load") (param $$i i32) (result i32)
      (i32.load $$m (i32.add (call $$addr_limit) (local.get $$i)))
    )

    (func (export "memory.grow") (param i32) (result i32)
      (memory.grow $$m (local.get 0))
    )
)`);

// ./test/core/multi-memory/memory_trap0.wast:23
assert_return(() => invoke($0, `store`, [-4, 42]), []);

// ./test/core/multi-memory/memory_trap0.wast:24
assert_return(() => invoke($0, `load`, [-4]), [value("i32", 42)]);

// ./test/core/multi-memory/memory_trap0.wast:25
assert_trap(() => invoke($0, `store`, [-3, 305419896]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap0.wast:26
assert_trap(() => invoke($0, `load`, [-3]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap0.wast:27
assert_trap(() => invoke($0, `store`, [-2, 13]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap0.wast:28
assert_trap(() => invoke($0, `load`, [-2]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap0.wast:29
assert_trap(() => invoke($0, `store`, [-1, 13]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap0.wast:30
assert_trap(() => invoke($0, `load`, [-1]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap0.wast:31
assert_trap(() => invoke($0, `store`, [0, 13]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap0.wast:32
assert_trap(() => invoke($0, `load`, [0]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap0.wast:33
assert_trap(() => invoke($0, `store`, [-2147483648, 13]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap0.wast:34
assert_trap(() => invoke($0, `load`, [-2147483648]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_trap0.wast:35
assert_return(() => invoke($0, `memory.grow`, [65537]), [value("i32", -1)]);
