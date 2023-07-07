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

// ./test/core/multi-memory/start0.wast

// ./test/core/multi-memory/start0.wast:1
let $0 = instantiate(`(module
  (memory 0)
  (memory $$m (data "A"))
  (memory $$n 1)
  
  (func $$inc
    (i32.store8 $$m
      (i32.const 0)
      (i32.add
        (i32.load8_u $$m (i32.const 0))
        (i32.const 1)
      )
    )
  )
  (func $$get (result i32)
    (return (i32.load8_u $$m (i32.const 0)))
  )
  (func $$getn (result i32)
    (return (i32.load8_u $$n (i32.const 0)))
  )
  (func $$main
    (call $$inc)
    (call $$inc)
    (call $$inc)
  )

  (start $$main)
  (export "inc" (func $$inc))
  (export "get" (func $$get))
  (export "getn" (func $$getn))
)`);

// ./test/core/multi-memory/start0.wast:32
assert_return(() => invoke($0, `get`, []), [value("i32", 68)]);

// ./test/core/multi-memory/start0.wast:33
assert_return(() => invoke($0, `getn`, []), [value("i32", 0)]);

// ./test/core/multi-memory/start0.wast:35
invoke($0, `inc`, []);

// ./test/core/multi-memory/start0.wast:36
assert_return(() => invoke($0, `get`, []), [value("i32", 69)]);

// ./test/core/multi-memory/start0.wast:37
assert_return(() => invoke($0, `getn`, []), [value("i32", 0)]);

// ./test/core/multi-memory/start0.wast:39
invoke($0, `inc`, []);

// ./test/core/multi-memory/start0.wast:40
assert_return(() => invoke($0, `get`, []), [value("i32", 70)]);

// ./test/core/multi-memory/start0.wast:41
assert_return(() => invoke($0, `getn`, []), [value("i32", 0)]);
