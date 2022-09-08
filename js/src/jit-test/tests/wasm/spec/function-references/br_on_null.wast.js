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

// ./test/core/br_on_null.wast

// ./test/core/br_on_null.wast:1
let $0 = instantiate(`(module
  (type $$t (func (result i32)))

  (func $$nn (param $$r (ref $$t)) (result i32)
    (block $$l
      (return (call_ref (br_on_null $$l (local.get $$r))))
    )
    (i32.const -1)
  )
  (func $$n (param $$r (ref null $$t)) (result i32)
    (block $$l
      (return (call_ref (br_on_null $$l (local.get $$r))))
    )
    (i32.const -1)
  )

  (elem func $$f)
  (func $$f (result i32) (i32.const 7))

  (func (export "nullable-null") (result i32) (call $$n (ref.null $$t)))
  (func (export "nonnullable-f") (result i32) (call $$nn (ref.func $$f)))
  (func (export "nullable-f") (result i32) (call $$n (ref.func $$f)))

  (func (export "unreachable") (result i32)
    (block $$l
      (return (drop (br_on_null $$l (unreachable))))
    )
    (i32.const -1)
  )
)`);

// ./test/core/br_on_null.wast:32
assert_trap(() => invoke($0, `unreachable`, []), `unreachable`);

// ./test/core/br_on_null.wast:34
assert_return(() => invoke($0, `nullable-null`, []), [value("i32", -1)]);

// ./test/core/br_on_null.wast:35
assert_return(() => invoke($0, `nonnullable-f`, []), [value("i32", 7)]);

// ./test/core/br_on_null.wast:36
assert_return(() => invoke($0, `nullable-f`, []), [value("i32", 7)]);

// ./test/core/br_on_null.wast:38
let $1 = instantiate(`(module
  (type $$t (func))
  (func (param $$r (ref null $$t)) (drop (br_on_null 0 (local.get $$r))))
  (func (param $$r (ref null func)) (drop (br_on_null 0 (local.get $$r))))
  (func (param $$r (ref null extern)) (drop (br_on_null 0 (local.get $$r))))
)`);

// ./test/core/br_on_null.wast:46
let $2 = instantiate(`(module
  (type $$t (func (param i32) (result i32)))
  (elem func $$f)
  (func $$f (param i32) (result i32) (i32.mul (local.get 0) (local.get 0)))

  (func $$a (param $$n i32) (param $$r (ref null $$t)) (result i32)
    (block $$l (result i32)
      (return (call_ref (br_on_null $$l (local.get $$n) (local.get $$r))))
    )
  )

  (func (export "args-null") (param $$n i32) (result i32)
    (call $$a (local.get $$n) (ref.null $$t))
  )
  (func (export "args-f") (param $$n i32) (result i32)
    (call $$a (local.get $$n) (ref.func $$f))
  )
)`);

// ./test/core/br_on_null.wast:65
assert_return(() => invoke($2, `args-null`, [3]), [value("i32", 3)]);

// ./test/core/br_on_null.wast:66
assert_return(() => invoke($2, `args-f`, [3]), [value("i32", 9)]);
