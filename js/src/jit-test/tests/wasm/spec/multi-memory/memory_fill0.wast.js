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

// ./test/core/multi-memory/memory_fill0.wast

// ./test/core/multi-memory/memory_fill0.wast:2
let $0 = instantiate(`(module
  (memory $$mem0 0)
  (memory $$mem1 0)
  (memory $$mem2 1)

  (func (export "fill") (param i32 i32 i32)
    (memory.fill $$mem2
      (local.get 0)
      (local.get 1)
      (local.get 2)))

  (func (export "load8_u") (param i32) (result i32)
    (i32.load8_u $$mem2 (local.get 0)))
)`);

// ./test/core/multi-memory/memory_fill0.wast:18
invoke($0, `fill`, [1, 255, 3]);

// ./test/core/multi-memory/memory_fill0.wast:19
assert_return(() => invoke($0, `load8_u`, [0]), [value("i32", 0)]);

// ./test/core/multi-memory/memory_fill0.wast:20
assert_return(() => invoke($0, `load8_u`, [1]), [value("i32", 255)]);

// ./test/core/multi-memory/memory_fill0.wast:21
assert_return(() => invoke($0, `load8_u`, [2]), [value("i32", 255)]);

// ./test/core/multi-memory/memory_fill0.wast:22
assert_return(() => invoke($0, `load8_u`, [3]), [value("i32", 255)]);

// ./test/core/multi-memory/memory_fill0.wast:23
assert_return(() => invoke($0, `load8_u`, [4]), [value("i32", 0)]);

// ./test/core/multi-memory/memory_fill0.wast:26
invoke($0, `fill`, [0, 48042, 2]);

// ./test/core/multi-memory/memory_fill0.wast:27
assert_return(() => invoke($0, `load8_u`, [0]), [value("i32", 170)]);

// ./test/core/multi-memory/memory_fill0.wast:28
assert_return(() => invoke($0, `load8_u`, [1]), [value("i32", 170)]);

// ./test/core/multi-memory/memory_fill0.wast:31
invoke($0, `fill`, [0, 0, 65536]);

// ./test/core/multi-memory/memory_fill0.wast:34
assert_trap(() => invoke($0, `fill`, [65280, 1, 257]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_fill0.wast:36
assert_return(() => invoke($0, `load8_u`, [65280]), [value("i32", 0)]);

// ./test/core/multi-memory/memory_fill0.wast:37
assert_return(() => invoke($0, `load8_u`, [65535]), [value("i32", 0)]);

// ./test/core/multi-memory/memory_fill0.wast:40
invoke($0, `fill`, [65536, 0, 0]);

// ./test/core/multi-memory/memory_fill0.wast:43
assert_trap(() => invoke($0, `fill`, [65537, 0, 0]), `out of bounds memory access`);
