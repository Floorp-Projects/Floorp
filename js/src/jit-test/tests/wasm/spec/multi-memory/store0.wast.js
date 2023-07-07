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

// ./test/core/multi-memory/store0.wast

// ./test/core/multi-memory/store0.wast:3
let $0 = instantiate(`(module
  (memory $$mem1 1)
  (memory $$mem2 1)

  (func (export "load1") (param i32) (result i64)
    (i64.load $$mem1 (local.get 0))
  )
  (func (export "load2") (param i32) (result i64)
    (i64.load $$mem2 (local.get 0))
  )

  (func (export "store1") (param i32 i64)
    (i64.store $$mem1 (local.get 0) (local.get 1))
  )
  (func (export "store2") (param i32 i64)
    (i64.store $$mem2 (local.get 0) (local.get 1))
  )
)`);

// ./test/core/multi-memory/store0.wast:22
invoke($0, `store1`, [0, 1n]);

// ./test/core/multi-memory/store0.wast:23
invoke($0, `store2`, [0, 2n]);

// ./test/core/multi-memory/store0.wast:24
assert_return(() => invoke($0, `load1`, [0]), [value("i64", 1n)]);

// ./test/core/multi-memory/store0.wast:25
assert_return(() => invoke($0, `load2`, [0]), [value("i64", 2n)]);
