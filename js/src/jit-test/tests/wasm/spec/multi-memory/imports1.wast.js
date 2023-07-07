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

// ./test/core/multi-memory/imports1.wast

// ./test/core/multi-memory/imports1.wast:1
let $0 = instantiate(`(module
  (import "spectest" "memory" (memory 1 2))
  (import "spectest" "memory" (memory 1 2))
  (memory $$m (import "spectest" "memory") 1 2)
  (import "spectest" "memory" (memory 1 2))
  
  (data (memory 2) (i32.const 10) "\\10")

  (func (export "load") (param i32) (result i32) (i32.load $$m (local.get 0)))
)`);

// ./test/core/multi-memory/imports1.wast:12
assert_return(() => invoke($0, `load`, [0]), [value("i32", 0)]);

// ./test/core/multi-memory/imports1.wast:13
assert_return(() => invoke($0, `load`, [10]), [value("i32", 16)]);

// ./test/core/multi-memory/imports1.wast:14
assert_return(() => invoke($0, `load`, [8]), [value("i32", 1048576)]);

// ./test/core/multi-memory/imports1.wast:15
assert_trap(() => invoke($0, `load`, [1000000]), `out of bounds memory access`);
