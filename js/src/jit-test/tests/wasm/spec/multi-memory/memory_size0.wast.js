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

// ./test/core/multi-memory/memory_size0.wast

// ./test/core/multi-memory/memory_size0.wast:1
let $0 = instantiate(`(module
  (memory 0)
  (memory 0)
  (memory 0)
  (memory 0)
  (memory $$m 0)
  
  (func (export "size") (result i32) (memory.size $$m))
  (func (export "grow") (param $$sz i32) (drop (memory.grow $$m (local.get $$sz))))
)`);

// ./test/core/multi-memory/memory_size0.wast:12
assert_return(() => invoke($0, `size`, []), [value("i32", 0)]);

// ./test/core/multi-memory/memory_size0.wast:13
assert_return(() => invoke($0, `grow`, [1]), []);

// ./test/core/multi-memory/memory_size0.wast:14
assert_return(() => invoke($0, `size`, []), [value("i32", 1)]);

// ./test/core/multi-memory/memory_size0.wast:15
assert_return(() => invoke($0, `grow`, [4]), []);

// ./test/core/multi-memory/memory_size0.wast:16
assert_return(() => invoke($0, `size`, []), [value("i32", 5)]);

// ./test/core/multi-memory/memory_size0.wast:17
assert_return(() => invoke($0, `grow`, [0]), []);

// ./test/core/multi-memory/memory_size0.wast:18
assert_return(() => invoke($0, `size`, []), [value("i32", 5)]);
