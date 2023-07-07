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

// ./test/core/multi-memory/load1.wast

// ./test/core/multi-memory/load1.wast:1
let $0 = instantiate(`(module $$M
  (memory (export "mem") 2)

  (func (export "read") (param i32) (result i32)
    (i32.load8_u (local.get 0))
  )
)`);
register($0, `M`);

// ./test/core/multi-memory/load1.wast:8
register($0, `M`);

// ./test/core/multi-memory/load1.wast:10
let $1 = instantiate(`(module
  (memory $$mem1 (import "M" "mem") 2)
  (memory $$mem2 3)

  (data (memory $$mem1) (i32.const 20) "\\01\\02\\03\\04\\05")
  (data (memory $$mem2) (i32.const 50) "\\0A\\0B\\0C\\0D\\0E")

  (func (export "read1") (param i32) (result i32)
    (i32.load8_u $$mem1 (local.get 0))
  )
  (func (export "read2") (param i32) (result i32)
    (i32.load8_u $$mem2 (local.get 0))
  )
)`);

// ./test/core/multi-memory/load1.wast:25
assert_return(() => invoke(`M`, `read`, [20]), [value("i32", 1)]);

// ./test/core/multi-memory/load1.wast:26
assert_return(() => invoke(`M`, `read`, [21]), [value("i32", 2)]);

// ./test/core/multi-memory/load1.wast:27
assert_return(() => invoke(`M`, `read`, [22]), [value("i32", 3)]);

// ./test/core/multi-memory/load1.wast:28
assert_return(() => invoke(`M`, `read`, [23]), [value("i32", 4)]);

// ./test/core/multi-memory/load1.wast:29
assert_return(() => invoke(`M`, `read`, [24]), [value("i32", 5)]);

// ./test/core/multi-memory/load1.wast:31
assert_return(() => invoke($1, `read1`, [20]), [value("i32", 1)]);

// ./test/core/multi-memory/load1.wast:32
assert_return(() => invoke($1, `read1`, [21]), [value("i32", 2)]);

// ./test/core/multi-memory/load1.wast:33
assert_return(() => invoke($1, `read1`, [22]), [value("i32", 3)]);

// ./test/core/multi-memory/load1.wast:34
assert_return(() => invoke($1, `read1`, [23]), [value("i32", 4)]);

// ./test/core/multi-memory/load1.wast:35
assert_return(() => invoke($1, `read1`, [24]), [value("i32", 5)]);

// ./test/core/multi-memory/load1.wast:37
assert_return(() => invoke($1, `read2`, [50]), [value("i32", 10)]);

// ./test/core/multi-memory/load1.wast:38
assert_return(() => invoke($1, `read2`, [51]), [value("i32", 11)]);

// ./test/core/multi-memory/load1.wast:39
assert_return(() => invoke($1, `read2`, [52]), [value("i32", 12)]);

// ./test/core/multi-memory/load1.wast:40
assert_return(() => invoke($1, `read2`, [53]), [value("i32", 13)]);

// ./test/core/multi-memory/load1.wast:41
assert_return(() => invoke($1, `read2`, [54]), [value("i32", 14)]);
