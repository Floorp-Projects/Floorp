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

// ./test/core/multi-memory/memory_init0.wast

// ./test/core/multi-memory/memory_init0.wast:2
let $0 = instantiate(`(module
  (memory $$mem0 0)
  (memory $$mem1 0)
  (memory $$mem2 1)
  (memory $$mem3 0)
  (data $$mem2 "\\aa\\bb\\cc\\dd")

  (func (export "init") (param i32 i32 i32)
    (memory.init $$mem2 0
      (local.get 0)
      (local.get 1)
      (local.get 2)))

  (func (export "load8_u") (param i32) (result i32)
    (i32.load8_u $$mem2 (local.get 0)))
)`);

// ./test/core/multi-memory/memory_init0.wast:19
invoke($0, `init`, [0, 1, 2]);

// ./test/core/multi-memory/memory_init0.wast:20
assert_return(() => invoke($0, `load8_u`, [0]), [value("i32", 187)]);

// ./test/core/multi-memory/memory_init0.wast:21
assert_return(() => invoke($0, `load8_u`, [1]), [value("i32", 204)]);

// ./test/core/multi-memory/memory_init0.wast:22
assert_return(() => invoke($0, `load8_u`, [2]), [value("i32", 0)]);

// ./test/core/multi-memory/memory_init0.wast:25
invoke($0, `init`, [65532, 0, 4]);

// ./test/core/multi-memory/memory_init0.wast:28
assert_trap(() => invoke($0, `init`, [65534, 0, 3]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_init0.wast:30
assert_return(() => invoke($0, `load8_u`, [65534]), [value("i32", 204)]);

// ./test/core/multi-memory/memory_init0.wast:31
assert_return(() => invoke($0, `load8_u`, [65535]), [value("i32", 221)]);

// ./test/core/multi-memory/memory_init0.wast:34
invoke($0, `init`, [65536, 0, 0]);

// ./test/core/multi-memory/memory_init0.wast:35
invoke($0, `init`, [0, 4, 0]);

// ./test/core/multi-memory/memory_init0.wast:38
assert_trap(() => invoke($0, `init`, [65537, 0, 0]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_init0.wast:40
assert_trap(() => invoke($0, `init`, [0, 5, 0]), `out of bounds memory access`);
