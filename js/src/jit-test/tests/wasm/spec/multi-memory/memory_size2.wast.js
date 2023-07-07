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

// ./test/core/multi-memory/memory_size2.wast

// ./test/core/multi-memory/memory_size2.wast:1
let $0 = instantiate(`(module
  (memory 0 0)
  (memory 0 0)
  (memory $$n 0 0)
  (memory $$m 0 2)
  
  (func (export "size") (result i32) (memory.size $$m))
  (func (export "grow") (param $$sz i32) (drop (memory.grow $$m (local.get $$sz))))

  (func (export "sizen") (result i32) (memory.size $$n))
  (func (export "grown") (param $$sz i32) (drop (memory.grow $$n (local.get $$sz))))
)`);

// ./test/core/multi-memory/memory_size2.wast:14
assert_return(() => invoke($0, `size`, []), [value("i32", 0)]);

// ./test/core/multi-memory/memory_size2.wast:15
assert_return(() => invoke($0, `sizen`, []), [value("i32", 0)]);

// ./test/core/multi-memory/memory_size2.wast:16
assert_return(() => invoke($0, `grow`, [3]), []);

// ./test/core/multi-memory/memory_size2.wast:17
assert_return(() => invoke($0, `sizen`, []), [value("i32", 0)]);

// ./test/core/multi-memory/memory_size2.wast:18
assert_return(() => invoke($0, `size`, []), [value("i32", 0)]);

// ./test/core/multi-memory/memory_size2.wast:19
assert_return(() => invoke($0, `grow`, [1]), []);

// ./test/core/multi-memory/memory_size2.wast:20
assert_return(() => invoke($0, `sizen`, []), [value("i32", 0)]);

// ./test/core/multi-memory/memory_size2.wast:21
assert_return(() => invoke($0, `size`, []), [value("i32", 1)]);

// ./test/core/multi-memory/memory_size2.wast:22
assert_return(() => invoke($0, `grow`, [0]), []);

// ./test/core/multi-memory/memory_size2.wast:23
assert_return(() => invoke($0, `sizen`, []), [value("i32", 0)]);

// ./test/core/multi-memory/memory_size2.wast:24
assert_return(() => invoke($0, `size`, []), [value("i32", 1)]);

// ./test/core/multi-memory/memory_size2.wast:25
assert_return(() => invoke($0, `grow`, [4]), []);

// ./test/core/multi-memory/memory_size2.wast:26
assert_return(() => invoke($0, `sizen`, []), [value("i32", 0)]);

// ./test/core/multi-memory/memory_size2.wast:27
assert_return(() => invoke($0, `size`, []), [value("i32", 1)]);

// ./test/core/multi-memory/memory_size2.wast:28
assert_return(() => invoke($0, `grow`, [1]), []);

// ./test/core/multi-memory/memory_size2.wast:29
assert_return(() => invoke($0, `sizen`, []), [value("i32", 0)]);

// ./test/core/multi-memory/memory_size2.wast:30
assert_return(() => invoke($0, `size`, []), [value("i32", 2)]);

// ./test/core/multi-memory/memory_size2.wast:32
assert_return(() => invoke($0, `grown`, [1]), []);

// ./test/core/multi-memory/memory_size2.wast:33
assert_return(() => invoke($0, `sizen`, []), [value("i32", 0)]);

// ./test/core/multi-memory/memory_size2.wast:34
assert_return(() => invoke($0, `size`, []), [value("i32", 2)]);
