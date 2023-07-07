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

// ./test/core/multi-memory/linking1.wast

// ./test/core/multi-memory/linking1.wast:1
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

// ./test/core/multi-memory/linking1.wast:12
register(`Mm`, `Mm`);

// ./test/core/multi-memory/linking1.wast:14
let $1 = instantiate(`(module $$Nm
  (func $$loadM (import "Mm" "load") (param i32) (result i32))
  (memory (import "Mm" "mem0") 0)

  (memory $$m 1)
  (data (memory 1) (i32.const 10) "\\f0\\f1\\f2\\f3\\f4\\f5")

  (export "Mm.load" (func $$loadM))
  (func (export "load") (param $$a i32) (result i32)
    (i32.load8_u $$m (local.get 0))
  )
)`);
register($1, `Nm`);

// ./test/core/multi-memory/linking1.wast:27
assert_return(() => invoke(`Mm`, `load`, [12]), [value("i32", 2)]);

// ./test/core/multi-memory/linking1.wast:28
assert_return(() => invoke(`Nm`, `Mm.load`, [12]), [value("i32", 2)]);

// ./test/core/multi-memory/linking1.wast:29
assert_return(() => invoke(`Nm`, `load`, [12]), [value("i32", 242)]);

// ./test/core/multi-memory/linking1.wast:31
let $2 = instantiate(`(module $$Om
  (memory (import "Mm" "mem1") 1)
  (data (i32.const 5) "\\a0\\a1\\a2\\a3\\a4\\a5\\a6\\a7")

  (func (export "load") (param $$a i32) (result i32)
    (i32.load8_u (local.get 0))
  )
)`);
register($2, `Om`);

// ./test/core/multi-memory/linking1.wast:40
assert_return(() => invoke(`Mm`, `load`, [12]), [value("i32", 167)]);

// ./test/core/multi-memory/linking1.wast:41
assert_return(() => invoke(`Nm`, `Mm.load`, [12]), [value("i32", 167)]);

// ./test/core/multi-memory/linking1.wast:42
assert_return(() => invoke(`Nm`, `load`, [12]), [value("i32", 242)]);

// ./test/core/multi-memory/linking1.wast:43
assert_return(() => invoke(`Om`, `load`, [12]), [value("i32", 167)]);

// ./test/core/multi-memory/linking1.wast:45
let $3 = instantiate(`(module
  (memory (import "Mm" "mem1") 0)
  (data (i32.const 0xffff) "a")
)`);

// ./test/core/multi-memory/linking1.wast:50
assert_trap(
  () => instantiate(`(module
    (memory (import "Mm" "mem0") 0)
    (data (i32.const 0xffff) "a")
  )`),
  `out of bounds memory access`,
);

// ./test/core/multi-memory/linking1.wast:58
assert_trap(
  () => instantiate(`(module
    (memory (import "Mm" "mem1") 0)
    (data (i32.const 0x10000) "a")
  )`),
  `out of bounds memory access`,
);
