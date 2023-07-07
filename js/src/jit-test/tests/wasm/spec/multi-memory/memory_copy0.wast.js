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

// ./test/core/multi-memory/memory_copy0.wast

// ./test/core/multi-memory/memory_copy0.wast:2
let $0 = instantiate(`(module
  (memory $$mem0 (data "\\ff\\11\\44\\ee"))
  (memory $$mem1 (data "\\ee\\22\\55\\ff"))
  (memory $$mem2 (data "\\dd\\33\\66\\00"))
  (memory $$mem3 (data "\\aa\\bb\\cc\\dd"))

  (func (export "copy") (param i32 i32 i32)
    (memory.copy $$mem3 $$mem3
      (local.get 0)
      (local.get 1)
      (local.get 2)))

  (func (export "load8_u") (param i32) (result i32)
    (i32.load8_u $$mem3 (local.get 0)))
)`);

// ./test/core/multi-memory/memory_copy0.wast:19
invoke($0, `copy`, [10, 0, 4]);

// ./test/core/multi-memory/memory_copy0.wast:21
assert_return(() => invoke($0, `load8_u`, [9]), [value("i32", 0)]);

// ./test/core/multi-memory/memory_copy0.wast:22
assert_return(() => invoke($0, `load8_u`, [10]), [value("i32", 170)]);

// ./test/core/multi-memory/memory_copy0.wast:23
assert_return(() => invoke($0, `load8_u`, [11]), [value("i32", 187)]);

// ./test/core/multi-memory/memory_copy0.wast:24
assert_return(() => invoke($0, `load8_u`, [12]), [value("i32", 204)]);

// ./test/core/multi-memory/memory_copy0.wast:25
assert_return(() => invoke($0, `load8_u`, [13]), [value("i32", 221)]);

// ./test/core/multi-memory/memory_copy0.wast:26
assert_return(() => invoke($0, `load8_u`, [14]), [value("i32", 0)]);

// ./test/core/multi-memory/memory_copy0.wast:29
invoke($0, `copy`, [8, 10, 4]);

// ./test/core/multi-memory/memory_copy0.wast:30
assert_return(() => invoke($0, `load8_u`, [8]), [value("i32", 170)]);

// ./test/core/multi-memory/memory_copy0.wast:31
assert_return(() => invoke($0, `load8_u`, [9]), [value("i32", 187)]);

// ./test/core/multi-memory/memory_copy0.wast:32
assert_return(() => invoke($0, `load8_u`, [10]), [value("i32", 204)]);

// ./test/core/multi-memory/memory_copy0.wast:33
assert_return(() => invoke($0, `load8_u`, [11]), [value("i32", 221)]);

// ./test/core/multi-memory/memory_copy0.wast:34
assert_return(() => invoke($0, `load8_u`, [12]), [value("i32", 204)]);

// ./test/core/multi-memory/memory_copy0.wast:35
assert_return(() => invoke($0, `load8_u`, [13]), [value("i32", 221)]);

// ./test/core/multi-memory/memory_copy0.wast:38
invoke($0, `copy`, [10, 7, 6]);

// ./test/core/multi-memory/memory_copy0.wast:39
assert_return(() => invoke($0, `load8_u`, [10]), [value("i32", 0)]);

// ./test/core/multi-memory/memory_copy0.wast:40
assert_return(() => invoke($0, `load8_u`, [11]), [value("i32", 170)]);

// ./test/core/multi-memory/memory_copy0.wast:41
assert_return(() => invoke($0, `load8_u`, [12]), [value("i32", 187)]);

// ./test/core/multi-memory/memory_copy0.wast:42
assert_return(() => invoke($0, `load8_u`, [13]), [value("i32", 204)]);

// ./test/core/multi-memory/memory_copy0.wast:43
assert_return(() => invoke($0, `load8_u`, [14]), [value("i32", 221)]);

// ./test/core/multi-memory/memory_copy0.wast:44
assert_return(() => invoke($0, `load8_u`, [15]), [value("i32", 204)]);

// ./test/core/multi-memory/memory_copy0.wast:45
assert_return(() => invoke($0, `load8_u`, [16]), [value("i32", 0)]);

// ./test/core/multi-memory/memory_copy0.wast:48
invoke($0, `copy`, [65280, 0, 256]);

// ./test/core/multi-memory/memory_copy0.wast:49
invoke($0, `copy`, [65024, 65280, 256]);

// ./test/core/multi-memory/memory_copy0.wast:52
invoke($0, `copy`, [65536, 0, 0]);

// ./test/core/multi-memory/memory_copy0.wast:53
invoke($0, `copy`, [0, 65536, 0]);

// ./test/core/multi-memory/memory_copy0.wast:56
assert_trap(() => invoke($0, `copy`, [65537, 0, 0]), `out of bounds memory access`);

// ./test/core/multi-memory/memory_copy0.wast:58
assert_trap(() => invoke($0, `copy`, [0, 65537, 0]), `out of bounds memory access`);
