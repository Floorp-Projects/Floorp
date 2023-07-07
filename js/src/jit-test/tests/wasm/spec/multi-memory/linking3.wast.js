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

// ./test/core/multi-memory/linking3.wast

// ./test/core/multi-memory/linking3.wast:1
let $0 = instantiate(`(module $$Mm
  (memory $$mem0 (export "mem0") 0 0)
  (memory $$mem1 (export "mem1") 5 5)
  (memory $$mem2 (export "mem2") 0 0)
  
  (data (memory 1) (i32.const 10) "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09")

  (func (export "load") (param $$a i32) (result i32)
    (i32.load8_u $$mem1 (local.get 0))
  )
)`);
register($0, `Mm`);

// ./test/core/multi-memory/linking3.wast:12
register(`Mm`, `Mm`);

// ./test/core/multi-memory/linking3.wast:14
assert_unlinkable(
  () => instantiate(`(module
    (func $$host (import "spectest" "print"))
    (memory (import "Mm" "mem1") 1)
    (table (import "Mm" "tab") 0 funcref)  ;; does not exist
    (data (i32.const 0) "abc")
  )`),
  `unknown import`,
);

// ./test/core/multi-memory/linking3.wast:23
assert_return(() => invoke(`Mm`, `load`, [0]), [value("i32", 0)]);

// ./test/core/multi-memory/linking3.wast:27
assert_trap(
  () => instantiate(`(module
    ;; Note: the memory is 5 pages large by the time we get here.
    (memory (import "Mm" "mem1") 1)
    (data (i32.const 0) "abc")
    (data (i32.const 327670) "zzzzzzzzzzzzzzzzzz") ;; (partially) out of bounds
  )`),
  `out of bounds memory access`,
);

// ./test/core/multi-memory/linking3.wast:36
assert_return(() => invoke(`Mm`, `load`, [0]), [value("i32", 97)]);

// ./test/core/multi-memory/linking3.wast:37
assert_return(() => invoke(`Mm`, `load`, [327670]), [value("i32", 0)]);

// ./test/core/multi-memory/linking3.wast:39
assert_trap(
  () => instantiate(`(module
    (memory (import "Mm" "mem1") 1)
    (data (i32.const 0) "abc")
    (table 0 funcref)
    (func)
    (elem (i32.const 0) 0)  ;; out of bounds
  )`),
  `out of bounds table access`,
);

// ./test/core/multi-memory/linking3.wast:49
assert_return(() => invoke(`Mm`, `load`, [0]), [value("i32", 97)]);

// ./test/core/multi-memory/linking3.wast:52
let $1 = instantiate(`(module $$Ms
  (type $$t (func (result i32)))
  (memory (export "memory") 1)
  (table (export "table") 1 funcref)
  (func (export "get memory[0]") (type $$t)
    (i32.load8_u (i32.const 0))
  )
  (func (export "get table[0]") (type $$t)
    (call_indirect (type $$t) (i32.const 0))
  )
)`);
register($1, `Ms`);

// ./test/core/multi-memory/linking3.wast:63
register(`Ms`, `Ms`);

// ./test/core/multi-memory/linking3.wast:65
assert_trap(
  () => instantiate(`(module
    (import "Ms" "memory" (memory 1))
    (import "Ms" "table" (table 1 funcref))
    (data (i32.const 0) "hello")
    (elem (i32.const 0) $$f)
    (func $$f (result i32)
      (i32.const 0xdead)
    )
    (func $$main
      (unreachable)
    )
    (start $$main)
  )`),
  `unreachable`,
);

// ./test/core/multi-memory/linking3.wast:82
assert_return(() => invoke(`Ms`, `get memory[0]`, []), [value("i32", 104)]);

// ./test/core/multi-memory/linking3.wast:83
assert_return(() => invoke(`Ms`, `get table[0]`, []), [value("i32", 57005)]);
