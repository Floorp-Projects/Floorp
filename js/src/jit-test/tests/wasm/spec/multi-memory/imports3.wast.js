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

// ./test/core/multi-memory/imports3.wast

// ./test/core/multi-memory/imports3.wast:1
let $0 = instantiate(`(module
  (func (export "func"))
  (func (export "func-i32") (param i32))
  (func (export "func-f32") (param f32))
  (func (export "func->i32") (result i32) (i32.const 22))
  (func (export "func->f32") (result f32) (f32.const 11))
  (func (export "func-i32->i32") (param i32) (result i32) (local.get 0))
  (func (export "func-i64->i64") (param i64) (result i64) (local.get 0))
  (global (export "global-i32") i32 (i32.const 55))
  (global (export "global-f32") f32 (f32.const 44))
  (global (export "global-mut-i64") (mut i64) (i64.const 66))
  (table (export "table-10-inf") 10 funcref)
  (table (export "table-10-20") 10 20 funcref)
  (memory (export "memory-2-inf") 2)
  (memory (export "memory-2-4") 2 4)
)`);

// ./test/core/multi-memory/imports3.wast:18
register($0, `test`);

// ./test/core/multi-memory/imports3.wast:19
assert_unlinkable(
  () => instantiate(`(module
    (import "test" "memory-2-4" (memory 1))
    (import "test" "func-i32" (memory 1))
  )`),
  `incompatible import type`,
);

// ./test/core/multi-memory/imports3.wast:26
assert_unlinkable(
  () => instantiate(`(module
    (import "test" "memory-2-4" (memory 1))
    (import "test" "global-i32" (memory 1))
  )`),
  `incompatible import type`,
);

// ./test/core/multi-memory/imports3.wast:33
assert_unlinkable(
  () => instantiate(`(module
    (import "test" "memory-2-4" (memory 1))
    (import "test" "table-10-inf" (memory 1))
  )`),
  `incompatible import type`,
);

// ./test/core/multi-memory/imports3.wast:40
assert_unlinkable(
  () => instantiate(`(module
    (import "test" "memory-2-4" (memory 1))
    (import "spectest" "print_i32" (memory 1))
  )`),
  `incompatible import type`,
);

// ./test/core/multi-memory/imports3.wast:47
assert_unlinkable(
  () => instantiate(`(module
    (import "test" "memory-2-4" (memory 1))
    (import "spectest" "global_i32" (memory 1))
  )`),
  `incompatible import type`,
);

// ./test/core/multi-memory/imports3.wast:54
assert_unlinkable(
  () => instantiate(`(module
    (import "test" "memory-2-4" (memory 1))
    (import "spectest" "table" (memory 1))
  )`),
  `incompatible import type`,
);

// ./test/core/multi-memory/imports3.wast:62
assert_unlinkable(
  () => instantiate(`(module
    (import "test" "memory-2-4" (memory 1))
    (import "spectest" "memory" (memory 2))
  )`),
  `incompatible import type`,
);

// ./test/core/multi-memory/imports3.wast:69
assert_unlinkable(
  () => instantiate(`(module
    (import "test" "memory-2-4" (memory 1))
    (import "spectest" "memory" (memory 1 1))
  )`),
  `incompatible import type`,
);
