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

// ./test/core/multi-memory/imports2.wast

// ./test/core/multi-memory/imports2.wast:1
let $0 = instantiate(`(module
  (memory (export "z") 0 0)
  (memory (export "memory-2-inf") 2)
  (memory (export "memory-2-4") 2 4)
)`);

// ./test/core/multi-memory/imports2.wast:7
register($0, `test`);

// ./test/core/multi-memory/imports2.wast:9
let $1 = instantiate(`(module
  (import "test" "z" (memory 0))
  (memory $$m (import "spectest" "memory") 1 2)
  (data (memory 1) (i32.const 10) "\\10")

  (func (export "load") (param i32) (result i32) (i32.load $$m (local.get 0)))
)`);

// ./test/core/multi-memory/imports2.wast:17
assert_return(() => invoke($1, `load`, [0]), [value("i32", 0)]);

// ./test/core/multi-memory/imports2.wast:18
assert_return(() => invoke($1, `load`, [10]), [value("i32", 16)]);

// ./test/core/multi-memory/imports2.wast:19
assert_return(() => invoke($1, `load`, [8]), [value("i32", 1048576)]);

// ./test/core/multi-memory/imports2.wast:20
assert_trap(() => invoke($1, `load`, [1000000]), `out of bounds memory access`);

// ./test/core/multi-memory/imports2.wast:22
let $2 = instantiate(`(module
  (memory (import "spectest" "memory") 1 2)
  (data (memory 0) (i32.const 10) "\\10")

  (func (export "load") (param i32) (result i32) (i32.load (local.get 0)))
)`);

// ./test/core/multi-memory/imports2.wast:28
assert_return(() => invoke($2, `load`, [0]), [value("i32", 0)]);

// ./test/core/multi-memory/imports2.wast:29
assert_return(() => invoke($2, `load`, [10]), [value("i32", 16)]);

// ./test/core/multi-memory/imports2.wast:30
assert_return(() => invoke($2, `load`, [8]), [value("i32", 1048576)]);

// ./test/core/multi-memory/imports2.wast:31
assert_trap(() => invoke($2, `load`, [1000000]), `out of bounds memory access`);

// ./test/core/multi-memory/imports2.wast:33
let $3 = instantiate(`(module
  (import "test" "memory-2-inf" (memory 2))
  (import "test" "memory-2-inf" (memory 1))
  (import "test" "memory-2-inf" (memory 0))
)`);

// ./test/core/multi-memory/imports2.wast:39
let $4 = instantiate(`(module
  (import "spectest" "memory" (memory 1))
  (import "spectest" "memory" (memory 0))
  (import "spectest" "memory" (memory 1 2))
  (import "spectest" "memory" (memory 0 2))
  (import "spectest" "memory" (memory 1 3))
  (import "spectest" "memory" (memory 0 3))
)`);

// ./test/core/multi-memory/imports2.wast:48
assert_unlinkable(
  () => instantiate(`(module (import "test" "unknown" (memory 1)))`),
  `unknown import`,
);

// ./test/core/multi-memory/imports2.wast:52
assert_unlinkable(
  () => instantiate(`(module (import "spectest" "unknown" (memory 1)))`),
  `unknown import`,
);

// ./test/core/multi-memory/imports2.wast:57
assert_unlinkable(
  () => instantiate(`(module (import "test" "memory-2-inf" (memory 3)))`),
  `incompatible import type`,
);

// ./test/core/multi-memory/imports2.wast:61
assert_unlinkable(
  () => instantiate(`(module (import "test" "memory-2-inf" (memory 2 3)))`),
  `incompatible import type`,
);

// ./test/core/multi-memory/imports2.wast:65
assert_unlinkable(
  () => instantiate(`(module (import "spectest" "memory" (memory 2)))`),
  `incompatible import type`,
);

// ./test/core/multi-memory/imports2.wast:69
assert_unlinkable(
  () => instantiate(`(module (import "spectest" "memory" (memory 1 1)))`),
  `incompatible import type`,
);
