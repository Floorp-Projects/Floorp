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

// ./test/core/multi-memory/float_memory0.wast

// ./test/core/multi-memory/float_memory0.wast:5
let $0 = instantiate(`(module
  (memory 0 0)
  (memory 0 0)
  (memory 0 0)
  (memory $$m (data "\\00\\00\\a0\\7f"))
  (memory 0 0)
  (memory 0 0)

  (func (export "f32.load") (result f32) (f32.load $$m (i32.const 0)))
  (func (export "i32.load") (result i32) (i32.load $$m (i32.const 0)))
  (func (export "f32.store") (f32.store $$m (i32.const 0) (f32.const nan:0x200000)))
  (func (export "i32.store") (i32.store $$m (i32.const 0) (i32.const 0x7fa00000)))
  (func (export "reset") (i32.store $$m (i32.const 0) (i32.const 0)))
)`);

// ./test/core/multi-memory/float_memory0.wast:20
assert_return(() => invoke($0, `i32.load`, []), [value("i32", 2141192192)]);

// ./test/core/multi-memory/float_memory0.wast:21
assert_return(() => invoke($0, `f32.load`, []), [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]);

// ./test/core/multi-memory/float_memory0.wast:22
invoke($0, `reset`, []);

// ./test/core/multi-memory/float_memory0.wast:23
assert_return(() => invoke($0, `i32.load`, []), [value("i32", 0)]);

// ./test/core/multi-memory/float_memory0.wast:24
assert_return(() => invoke($0, `f32.load`, []), [value("f32", 0)]);

// ./test/core/multi-memory/float_memory0.wast:25
invoke($0, `f32.store`, []);

// ./test/core/multi-memory/float_memory0.wast:26
assert_return(() => invoke($0, `i32.load`, []), [value("i32", 2141192192)]);

// ./test/core/multi-memory/float_memory0.wast:27
assert_return(() => invoke($0, `f32.load`, []), [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]);

// ./test/core/multi-memory/float_memory0.wast:28
invoke($0, `reset`, []);

// ./test/core/multi-memory/float_memory0.wast:29
assert_return(() => invoke($0, `i32.load`, []), [value("i32", 0)]);

// ./test/core/multi-memory/float_memory0.wast:30
assert_return(() => invoke($0, `f32.load`, []), [value("f32", 0)]);

// ./test/core/multi-memory/float_memory0.wast:31
invoke($0, `i32.store`, []);

// ./test/core/multi-memory/float_memory0.wast:32
assert_return(() => invoke($0, `i32.load`, []), [value("i32", 2141192192)]);

// ./test/core/multi-memory/float_memory0.wast:33
assert_return(() => invoke($0, `f32.load`, []), [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]);

// ./test/core/multi-memory/float_memory0.wast:35
let $1 = instantiate(`(module
  (memory 0 0)
  (memory $$m (data "\\00\\00\\00\\00\\00\\00\\f4\\7f"))

  (func (export "f64.load") (result f64) (f64.load $$m (i32.const 0)))
  (func (export "i64.load") (result i64) (i64.load $$m (i32.const 0)))
  (func (export "f64.store") (f64.store $$m (i32.const 0) (f64.const nan:0x4000000000000)))
  (func (export "i64.store") (i64.store $$m (i32.const 0) (i64.const 0x7ff4000000000000)))
  (func (export "reset") (i64.store $$m (i32.const 0) (i64.const 0)))
)`);

// ./test/core/multi-memory/float_memory0.wast:46
assert_return(() => invoke($1, `i64.load`, []), [value("i64", 9219994337134247936n)]);

// ./test/core/multi-memory/float_memory0.wast:47
assert_return(
  () => invoke($1, `f64.load`, []),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f])],
);

// ./test/core/multi-memory/float_memory0.wast:48
invoke($1, `reset`, []);

// ./test/core/multi-memory/float_memory0.wast:49
assert_return(() => invoke($1, `i64.load`, []), [value("i64", 0n)]);

// ./test/core/multi-memory/float_memory0.wast:50
assert_return(() => invoke($1, `f64.load`, []), [value("f64", 0)]);

// ./test/core/multi-memory/float_memory0.wast:51
invoke($1, `f64.store`, []);

// ./test/core/multi-memory/float_memory0.wast:52
assert_return(() => invoke($1, `i64.load`, []), [value("i64", 9219994337134247936n)]);

// ./test/core/multi-memory/float_memory0.wast:53
assert_return(
  () => invoke($1, `f64.load`, []),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f])],
);

// ./test/core/multi-memory/float_memory0.wast:54
invoke($1, `reset`, []);

// ./test/core/multi-memory/float_memory0.wast:55
assert_return(() => invoke($1, `i64.load`, []), [value("i64", 0n)]);

// ./test/core/multi-memory/float_memory0.wast:56
assert_return(() => invoke($1, `f64.load`, []), [value("f64", 0)]);

// ./test/core/multi-memory/float_memory0.wast:57
invoke($1, `i64.store`, []);

// ./test/core/multi-memory/float_memory0.wast:58
assert_return(() => invoke($1, `i64.load`, []), [value("i64", 9219994337134247936n)]);

// ./test/core/multi-memory/float_memory0.wast:59
assert_return(
  () => invoke($1, `f64.load`, []),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f])],
);
