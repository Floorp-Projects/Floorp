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

// ./test/core/memory_redundancy.wast

// ./test/core/memory_redundancy.wast:5
let $0 = instantiate(`(module
  (memory 1 1)

  (func (export "zero_everything")
    (i32.store (i32.const 0) (i32.const 0))
    (i32.store (i32.const 4) (i32.const 0))
    (i32.store (i32.const 8) (i32.const 0))
    (i32.store (i32.const 12) (i32.const 0))
  )

  (func (export "test_store_to_load") (result i32)
    (i32.store (i32.const 8) (i32.const 0))
    (f32.store (i32.const 5) (f32.const -0.0))
    (i32.load (i32.const 8))
  )

  (func (export "test_redundant_load") (result i32)
    (local $$t i32)
    (local $$s i32)
    (local.set $$t (i32.load (i32.const 8)))
    (i32.store (i32.const 5) (i32.const 0x80000000))
    (local.set $$s (i32.load (i32.const 8)))
    (i32.add (local.get $$t) (local.get $$s))
  )

  (func (export "test_dead_store") (result f32)
    (local $$t f32)
    (i32.store (i32.const 8) (i32.const 0x23232323))
    (local.set $$t (f32.load (i32.const 11)))
    (i32.store (i32.const 8) (i32.const 0))
    (local.get $$t)
  )

  ;; A function named "malloc" which implementations nonetheless shouldn't
  ;; assume behaves like C malloc.
  (func $$malloc (export "malloc")
     (param $$size i32)
     (result i32)
     (i32.const 16)
  )

  ;; Call malloc twice, but unlike C malloc, we don't get non-aliasing pointers.
  (func (export "malloc_aliasing")
     (result i32)
     (local $$x i32)
     (local $$y i32)
     (local.set $$x (call $$malloc (i32.const 4)))
     (local.set $$y (call $$malloc (i32.const 4)))
     (i32.store (local.get $$x) (i32.const 42))
     (i32.store (local.get $$y) (i32.const 43))
     (i32.load (local.get $$x))
  )
)`);

// ./test/core/memory_redundancy.wast:59
assert_return(() => invoke($0, `test_store_to_load`, []), [value("i32", 128)]);

// ./test/core/memory_redundancy.wast:60
invoke($0, `zero_everything`, []);

// ./test/core/memory_redundancy.wast:61
assert_return(() => invoke($0, `test_redundant_load`, []), [value("i32", 128)]);

// ./test/core/memory_redundancy.wast:62
invoke($0, `zero_everything`, []);

// ./test/core/memory_redundancy.wast:63
assert_return(() => invoke($0, `test_dead_store`, []), [
  value("f32", 0.000000000000000000000000000000000000000000049),
]);

// ./test/core/memory_redundancy.wast:64
invoke($0, `zero_everything`, []);

// ./test/core/memory_redundancy.wast:65
assert_return(() => invoke($0, `malloc_aliasing`, []), [value("i32", 43)]);
