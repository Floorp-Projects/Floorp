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

// ./test/core/multi-memory/data_drop0.wast

// ./test/core/multi-memory/data_drop0.wast:2
let $0 = instantiate(`(module
  (memory $$mem0 0)
  (memory $$mem1 1)
  (memory $$mem2 0)
  (data $$p "x")
  (data $$a (memory 1) (i32.const 0) "x")

  (func (export "drop_passive") (data.drop $$p))
  (func (export "init_passive") (param $$len i32)
    (memory.init $$mem1 $$p (i32.const 0) (i32.const 0) (local.get $$len)))

  (func (export "drop_active") (data.drop $$a))
  (func (export "init_active") (param $$len i32)
    (memory.init $$mem1 $$a (i32.const 0) (i32.const 0) (local.get $$len)))
)`);

// ./test/core/multi-memory/data_drop0.wast:18
invoke($0, `init_passive`, [1]);

// ./test/core/multi-memory/data_drop0.wast:19
invoke($0, `drop_passive`, []);

// ./test/core/multi-memory/data_drop0.wast:20
invoke($0, `drop_passive`, []);

// ./test/core/multi-memory/data_drop0.wast:21
assert_return(() => invoke($0, `init_passive`, [0]), []);

// ./test/core/multi-memory/data_drop0.wast:22
assert_trap(() => invoke($0, `init_passive`, [1]), `out of bounds memory access`);

// ./test/core/multi-memory/data_drop0.wast:23
invoke($0, `init_passive`, [0]);

// ./test/core/multi-memory/data_drop0.wast:24
invoke($0, `drop_active`, []);

// ./test/core/multi-memory/data_drop0.wast:25
assert_return(() => invoke($0, `init_active`, [0]), []);

// ./test/core/multi-memory/data_drop0.wast:26
assert_trap(() => invoke($0, `init_active`, [1]), `out of bounds memory access`);

// ./test/core/multi-memory/data_drop0.wast:27
invoke($0, `init_active`, [0]);
