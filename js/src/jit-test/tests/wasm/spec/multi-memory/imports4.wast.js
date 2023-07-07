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

// ./test/core/multi-memory/imports4.wast

// ./test/core/multi-memory/imports4.wast:1
let $0 = instantiate(`(module
  (memory (export "memory-2-inf") 2)
  (memory (export "memory-2-4") 2 4)
)`);

// ./test/core/multi-memory/imports4.wast:6
register($0, `test`);

// ./test/core/multi-memory/imports4.wast:8
let $1 = instantiate(`(module
  (import "test" "memory-2-4" (memory 1))
  (memory $$m (import "spectest" "memory") 0 3)  ;; actual has max size 2
  (func (export "grow") (param i32) (result i32) (memory.grow $$m (local.get 0)))
)`);

// ./test/core/multi-memory/imports4.wast:13
assert_return(() => invoke($1, `grow`, [0]), [value("i32", 1)]);

// ./test/core/multi-memory/imports4.wast:14
assert_return(() => invoke($1, `grow`, [1]), [value("i32", 1)]);

// ./test/core/multi-memory/imports4.wast:15
assert_return(() => invoke($1, `grow`, [0]), [value("i32", 2)]);

// ./test/core/multi-memory/imports4.wast:16
assert_return(() => invoke($1, `grow`, [1]), [value("i32", -1)]);

// ./test/core/multi-memory/imports4.wast:17
assert_return(() => invoke($1, `grow`, [0]), [value("i32", 2)]);

// ./test/core/multi-memory/imports4.wast:19
let $2 = instantiate(`(module $$Mgm
  (memory 0)
  (memory 0)
  (memory $$m (export "memory") 1) ;; initial size is 1
  (func (export "grow") (result i32) (memory.grow $$m (i32.const 1)))
)`);
register($2, `Mgm`);

// ./test/core/multi-memory/imports4.wast:25
register(`Mgm`, `grown-memory`);

// ./test/core/multi-memory/imports4.wast:26
assert_return(() => invoke(`Mgm`, `grow`, []), [value("i32", 1)]);

// ./test/core/multi-memory/imports4.wast:28
let $3 = instantiate(`(module $$Mgim1
  ;; imported memory limits should match, because external memory size is 2 now
  (import "test" "memory-2-4" (memory 1))
  (memory $$m (export "memory") (import "grown-memory" "memory") 2) 
  (memory 0)
  (memory 0)
  (func (export "grow") (result i32) (memory.grow $$m (i32.const 1)))
)`);
register($3, `Mgim1`);

// ./test/core/multi-memory/imports4.wast:36
register(`Mgim1`, `grown-imported-memory`);

// ./test/core/multi-memory/imports4.wast:37
assert_return(() => invoke(`Mgim1`, `grow`, []), [value("i32", 2)]);

// ./test/core/multi-memory/imports4.wast:39
let $4 = instantiate(`(module $$Mgim2
  ;; imported memory limits should match, because external memory size is 3 now
  (import "test" "memory-2-4" (memory 1))
  (memory $$m (import "grown-imported-memory" "memory") 3)
  (memory 0)
  (memory 0)
  (func (export "size") (result i32) (memory.size $$m))
)`);
register($4, `Mgim2`);

// ./test/core/multi-memory/imports4.wast:47
assert_return(() => invoke(`Mgim2`, `size`, []), [value("i32", 3)]);
