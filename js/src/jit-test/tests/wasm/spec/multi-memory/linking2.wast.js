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

// ./test/core/multi-memory/linking2.wast

// ./test/core/multi-memory/linking2.wast:1
let $0 = instantiate(`(module $$Mm
  (memory $$mem0 (export "mem0") 0 0)
  (memory $$mem1 (export "mem1") 1 5)
  (memory $$mem2 (export "mem2") 0 0)
  
  (data (memory 1) (i32.const 10) "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09")

  (func (export "load") (param $$a i32) (result i32)
    (i32.load8_u $$mem1 (local.get 0))
  )
)`);
register($0, `Mm`);

// ./test/core/multi-memory/linking2.wast:12
register(`Mm`, `Mm`);

// ./test/core/multi-memory/linking2.wast:14
let $1 = instantiate(`(module $$Pm
  (memory (import "Mm" "mem1") 1 8)

  (func (export "grow") (param $$a i32) (result i32)
    (memory.grow (local.get 0))
  )
)`);
register($1, `Pm`);

// ./test/core/multi-memory/linking2.wast:22
assert_return(() => invoke(`Pm`, `grow`, [0]), [value("i32", 1)]);

// ./test/core/multi-memory/linking2.wast:23
assert_return(() => invoke(`Pm`, `grow`, [2]), [value("i32", 1)]);

// ./test/core/multi-memory/linking2.wast:24
assert_return(() => invoke(`Pm`, `grow`, [0]), [value("i32", 3)]);

// ./test/core/multi-memory/linking2.wast:25
assert_return(() => invoke(`Pm`, `grow`, [1]), [value("i32", 3)]);

// ./test/core/multi-memory/linking2.wast:26
assert_return(() => invoke(`Pm`, `grow`, [1]), [value("i32", 4)]);

// ./test/core/multi-memory/linking2.wast:27
assert_return(() => invoke(`Pm`, `grow`, [0]), [value("i32", 5)]);

// ./test/core/multi-memory/linking2.wast:28
assert_return(() => invoke(`Pm`, `grow`, [1]), [value("i32", -1)]);

// ./test/core/multi-memory/linking2.wast:29
assert_return(() => invoke(`Pm`, `grow`, [0]), [value("i32", 5)]);
