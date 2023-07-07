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

// ./test/core/multi-memory/linking0.wast

// ./test/core/multi-memory/linking0.wast:1
let $0 = instantiate(`(module $$Mt
  (type (func (result i32)))
  (type (func))

  (table (export "tab") 10 funcref)
  (elem (i32.const 2) $$g $$g $$g $$g)
  (func $$g (result i32) (i32.const 4))
  (func (export "h") (result i32) (i32.const -4))

  (func (export "call") (param i32) (result i32)
    (call_indirect (type 0) (local.get 0))
  )
)`);
register($0, `Mt`);

// ./test/core/multi-memory/linking0.wast:14
register(`Mt`, `Mt`);

// ./test/core/multi-memory/linking0.wast:16
assert_unlinkable(
  () => instantiate(`(module
    (table (import "Mt" "tab") 10 funcref)
    (memory (import "spectest" "memory") 1)
    (memory (import "Mt" "mem") 1)  ;; does not exist
    (func $$f (result i32) (i32.const 0))
    (elem (i32.const 7) $$f)
    (elem (i32.const 9) $$f)
  )`),
  `unknown import`,
);

// ./test/core/multi-memory/linking0.wast:27
assert_trap(() => invoke(`Mt`, `call`, [7]), `uninitialized element`);

// ./test/core/multi-memory/linking0.wast:30
assert_trap(
  () => instantiate(`(module
    (table (import "Mt" "tab") 10 funcref)
    (func $$f (result i32) (i32.const 0))
    (elem (i32.const 7) $$f)
    (memory 0)
    (memory $$m 1)
    (memory 0)
    (data $$m (i32.const 0x10000) "d")  ;; out of bounds
  )`),
  `out of bounds memory access`,
);

// ./test/core/multi-memory/linking0.wast:42
assert_return(() => invoke(`Mt`, `call`, [7]), [value("i32", 0)]);
