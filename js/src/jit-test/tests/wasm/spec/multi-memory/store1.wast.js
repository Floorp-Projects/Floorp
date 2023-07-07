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

// ./test/core/multi-memory/store1.wast

// ./test/core/multi-memory/store1.wast:1
let $0 = instantiate(`(module $$M1
  (memory (export "mem") 1)

  (func (export "load") (param i32) (result i64)
    (i64.load (local.get 0))
  )
  (func (export "store") (param i32 i64)
    (i64.store (local.get 0) (local.get 1))
  )
)`);
register($0, `M1`);

// ./test/core/multi-memory/store1.wast:11
register($0, `M1`);

// ./test/core/multi-memory/store1.wast:13
let $1 = instantiate(`(module $$M2
  (memory (export "mem") 1)

  (func (export "load") (param i32) (result i64)
    (i64.load (local.get 0))
  )
  (func (export "store") (param i32 i64)
    (i64.store (local.get 0) (local.get 1))
  )
)`);
register($1, `M2`);

// ./test/core/multi-memory/store1.wast:23
register($1, `M2`);

// ./test/core/multi-memory/store1.wast:25
invoke(`M1`, `store`, [0, 1n]);

// ./test/core/multi-memory/store1.wast:26
invoke(`M2`, `store`, [0, 2n]);

// ./test/core/multi-memory/store1.wast:27
assert_return(() => invoke(`M1`, `load`, [0]), [value("i64", 1n)]);

// ./test/core/multi-memory/store1.wast:28
assert_return(() => invoke(`M2`, `load`, [0]), [value("i64", 2n)]);

// ./test/core/multi-memory/store1.wast:30
let $2 = instantiate(`(module
  (memory $$mem1 (import "M1" "mem") 1)
  (memory $$mem2 (import "M2" "mem") 1)

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

// ./test/core/multi-memory/store1.wast:49
invoke($2, `store1`, [0, 1n]);

// ./test/core/multi-memory/store1.wast:50
invoke($2, `store2`, [0, 2n]);

// ./test/core/multi-memory/store1.wast:51
assert_return(() => invoke($2, `load1`, [0]), [value("i64", 1n)]);

// ./test/core/multi-memory/store1.wast:52
assert_return(() => invoke($2, `load2`, [0]), [value("i64", 2n)]);
