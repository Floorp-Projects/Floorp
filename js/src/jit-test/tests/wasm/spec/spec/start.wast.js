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

// ./test/core/start.wast

// ./test/core/start.wast:1
assert_invalid(
  () => instantiate(`(module (func) (start 1))`),
  `unknown function`,
);

// ./test/core/start.wast:6
assert_invalid(() =>
  instantiate(`(module
    (func $$main (result i32) (return (i32.const 0)))
    (start $$main)
  )`), `start function`);

// ./test/core/start.wast:13
assert_invalid(() =>
  instantiate(`(module
    (func $$main (param $$a i32))
    (start $$main)
  )`), `start function`);

// ./test/core/start.wast:21
let $0 = instantiate(`(module
  (memory (data "A"))
  (func $$inc
    (i32.store8
      (i32.const 0)
      (i32.add
        (i32.load8_u (i32.const 0))
        (i32.const 1)
      )
    )
  )
  (func $$get (result i32)
    (return (i32.load8_u (i32.const 0)))
  )
  (func $$main
    (call $$inc)
    (call $$inc)
    (call $$inc)
  )

  (start $$main)
  (export "inc" (func $$inc))
  (export "get" (func $$get))
)`);

// ./test/core/start.wast:45
assert_return(() => invoke($0, `get`, []), [value("i32", 68)]);

// ./test/core/start.wast:46
invoke($0, `inc`, []);

// ./test/core/start.wast:47
assert_return(() => invoke($0, `get`, []), [value("i32", 69)]);

// ./test/core/start.wast:48
invoke($0, `inc`, []);

// ./test/core/start.wast:49
assert_return(() => invoke($0, `get`, []), [value("i32", 70)]);

// ./test/core/start.wast:51
let $1 = instantiate(`(module
  (memory (data "A"))
  (func $$inc
    (i32.store8
      (i32.const 0)
      (i32.add
        (i32.load8_u (i32.const 0))
        (i32.const 1)
      )
    )
  )
  (func $$get (result i32)
    (return (i32.load8_u (i32.const 0)))
  )
  (func $$main
    (call $$inc)
    (call $$inc)
    (call $$inc)
  )
  (start 2)
  (export "inc" (func $$inc))
  (export "get" (func $$get))
)`);

// ./test/core/start.wast:74
assert_return(() => invoke($1, `get`, []), [value("i32", 68)]);

// ./test/core/start.wast:75
invoke($1, `inc`, []);

// ./test/core/start.wast:76
assert_return(() => invoke($1, `get`, []), [value("i32", 69)]);

// ./test/core/start.wast:77
invoke($1, `inc`, []);

// ./test/core/start.wast:78
assert_return(() => invoke($1, `get`, []), [value("i32", 70)]);

// ./test/core/start.wast:80
let $2 = instantiate(`(module
  (func $$print_i32 (import "spectest" "print_i32") (param i32))
  (func $$main (call $$print_i32 (i32.const 1)))
  (start 1)
)`);

// ./test/core/start.wast:86
let $3 = instantiate(`(module
  (func $$print_i32 (import "spectest" "print_i32") (param i32))
  (func $$main (call $$print_i32 (i32.const 2)))
  (start $$main)
)`);

// ./test/core/start.wast:92
let $4 = instantiate(`(module
  (func $$print (import "spectest" "print"))
  (start $$print)
)`);

// ./test/core/start.wast:97
assert_trap(
  () => instantiate(`(module (func $$main (unreachable)) (start $$main))`),
  `unreachable`,
);

// ./test/core/start.wast:102
assert_malformed(
  () =>
    instantiate(
      `(module (func $$a (unreachable)) (func $$b (unreachable)) (start $$a) (start $$b)) `,
    ),
  `multiple start sections`,
);
