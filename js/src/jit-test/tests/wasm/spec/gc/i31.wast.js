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

// ./test/core/gc/i31.wast

// ./test/core/gc/i31.wast:1
let $0 = instantiate(`(module
  (func (export "new") (param $$i i32) (result (ref i31))
    (ref.i31 (local.get $$i))
  )

  (func (export "get_u") (param $$i i32) (result i32)
    (i31.get_u (ref.i31 (local.get $$i)))
  )
  (func (export "get_s") (param $$i i32) (result i32)
    (i31.get_s (ref.i31 (local.get $$i)))
  )

  (func (export "get_u-null") (result i32)
    (i31.get_u (ref.null i31))
  )
  (func (export "get_s-null") (result i32)
    (i31.get_u (ref.null i31))
  )

  (global $$i (ref i31) (ref.i31 (i32.const 2)))
  (global $$m (mut (ref i31)) (ref.i31 (i32.const 3)))
  (func (export "get_globals") (result i32 i32)
    (i31.get_u (global.get $$i))
    (i31.get_u (global.get $$m))
  )
)`);

// ./test/core/gc/i31.wast:28
assert_return(() => invoke($0, `new`, [1]), [new RefWithType('i31ref')]);

// ./test/core/gc/i31.wast:30
assert_return(() => invoke($0, `get_u`, [0]), [value("i32", 0)]);

// ./test/core/gc/i31.wast:31
assert_return(() => invoke($0, `get_u`, [100]), [value("i32", 100)]);

// ./test/core/gc/i31.wast:32
assert_return(() => invoke($0, `get_u`, [-1]), [value("i32", 2147483647)]);

// ./test/core/gc/i31.wast:33
assert_return(() => invoke($0, `get_u`, [1073741823]), [value("i32", 1073741823)]);

// ./test/core/gc/i31.wast:34
assert_return(() => invoke($0, `get_u`, [1073741824]), [value("i32", 1073741824)]);

// ./test/core/gc/i31.wast:35
assert_return(() => invoke($0, `get_u`, [2147483647]), [value("i32", 2147483647)]);

// ./test/core/gc/i31.wast:36
assert_return(() => invoke($0, `get_u`, [-1431655766]), [value("i32", 715827882)]);

// ./test/core/gc/i31.wast:37
assert_return(() => invoke($0, `get_u`, [-894784854]), [value("i32", 1252698794)]);

// ./test/core/gc/i31.wast:39
assert_return(() => invoke($0, `get_s`, [0]), [value("i32", 0)]);

// ./test/core/gc/i31.wast:40
assert_return(() => invoke($0, `get_s`, [100]), [value("i32", 100)]);

// ./test/core/gc/i31.wast:41
assert_return(() => invoke($0, `get_s`, [-1]), [value("i32", -1)]);

// ./test/core/gc/i31.wast:42
assert_return(() => invoke($0, `get_s`, [1073741823]), [value("i32", 1073741823)]);

// ./test/core/gc/i31.wast:43
assert_return(() => invoke($0, `get_s`, [1073741824]), [value("i32", -1073741824)]);

// ./test/core/gc/i31.wast:44
assert_return(() => invoke($0, `get_s`, [2147483647]), [value("i32", -1)]);

// ./test/core/gc/i31.wast:45
assert_return(() => invoke($0, `get_s`, [-1431655766]), [value("i32", 715827882)]);

// ./test/core/gc/i31.wast:46
assert_return(() => invoke($0, `get_s`, [-894784854]), [value("i32", -894784854)]);

// ./test/core/gc/i31.wast:48
assert_trap(() => invoke($0, `get_u-null`, []), `null i31 reference`);

// ./test/core/gc/i31.wast:49
assert_trap(() => invoke($0, `get_s-null`, []), `null i31 reference`);

// ./test/core/gc/i31.wast:51
assert_return(() => invoke($0, `get_globals`, []), [value("i32", 2), value("i32", 3)]);
