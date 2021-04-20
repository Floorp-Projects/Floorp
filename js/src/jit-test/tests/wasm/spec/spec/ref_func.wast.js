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

// ./test/core/ref_func.wast

// ./test/core/ref_func.wast:1
let $0 = instantiate(`(module
  (func (export "f") (param $$x i32) (result i32) (local.get $$x))
)`);

// ./test/core/ref_func.wast:4
register($0, `M`);

// ./test/core/ref_func.wast:6
let $1 = instantiate(`(module
  (func $$f (import "M" "f") (param i32) (result i32))
  (func $$g (param $$x i32) (result i32)
    (i32.add (local.get $$x) (i32.const 1))
  )

  (global funcref (ref.func $$f))
  (global funcref (ref.func $$g))
  (global $$v (mut funcref) (ref.func $$f))

  (global funcref (ref.func $$gf1))
  (global funcref (ref.func $$gf2))
  (func (drop (ref.func $$ff1)) (drop (ref.func $$ff2)))
  (elem declare func $$gf1 $$ff1)
  (elem declare funcref (ref.func $$gf2) (ref.func $$ff2))
  (func $$gf1)
  (func $$gf2)
  (func $$ff1)
  (func $$ff2)

  (func (export "is_null-f") (result i32)
    (ref.is_null (ref.func $$f))
  )
  (func (export "is_null-g") (result i32)
    (ref.is_null (ref.func $$g))
  )
  (func (export "is_null-v") (result i32)
    (ref.is_null (global.get $$v))
  )

  (func (export "set-f") (global.set $$v (ref.func $$f)))
  (func (export "set-g") (global.set $$v (ref.func $$g)))

  (table $$t 1 funcref)
  (elem declare func $$f $$g)

  (func (export "call-f") (param $$x i32) (result i32)
    (table.set $$t (i32.const 0) (ref.func $$f))
    (call_indirect $$t (param i32) (result i32) (local.get $$x) (i32.const 0))
  )
  (func (export "call-g") (param $$x i32) (result i32)
    (table.set $$t (i32.const 0) (ref.func $$g))
    (call_indirect $$t (param i32) (result i32) (local.get $$x) (i32.const 0))
  )
  (func (export "call-v") (param $$x i32) (result i32)
    (table.set $$t (i32.const 0) (global.get $$v))
    (call_indirect $$t (param i32) (result i32) (local.get $$x) (i32.const 0))
  )
)`);

// ./test/core/ref_func.wast:56
assert_return(() => invoke($1, `is_null-f`, []), [value("i32", 0)]);

// ./test/core/ref_func.wast:57
assert_return(() => invoke($1, `is_null-g`, []), [value("i32", 0)]);

// ./test/core/ref_func.wast:58
assert_return(() => invoke($1, `is_null-v`, []), [value("i32", 0)]);

// ./test/core/ref_func.wast:60
assert_return(() => invoke($1, `call-f`, [4]), [value("i32", 4)]);

// ./test/core/ref_func.wast:61
assert_return(() => invoke($1, `call-g`, [4]), [value("i32", 5)]);

// ./test/core/ref_func.wast:62
assert_return(() => invoke($1, `call-v`, [4]), [value("i32", 4)]);

// ./test/core/ref_func.wast:63
invoke($1, `set-g`, []);

// ./test/core/ref_func.wast:64
assert_return(() => invoke($1, `call-v`, [4]), [value("i32", 5)]);

// ./test/core/ref_func.wast:65
invoke($1, `set-f`, []);

// ./test/core/ref_func.wast:66
assert_return(() => invoke($1, `call-v`, [4]), [value("i32", 4)]);

// ./test/core/ref_func.wast:68
assert_invalid(() =>
  instantiate(`(module
    (func $$f (import "M" "f") (param i32) (result i32))
    (func $$g (import "M" "g") (param i32) (result i32))
    (global funcref (ref.func 7))
  )`), `unknown function 7`);

// ./test/core/ref_func.wast:80
let $2 = instantiate(`(module
  (func $$f1)
  (func $$f2)
  (func $$f3)
  (func $$f4)
  (func $$f5)
  (func $$f6)

  (table $$t 1 funcref)

  (global funcref (ref.func $$f1))
  (export "f" (func $$f2))
  (elem (table $$t) (i32.const 0) func $$f3)
  (elem (table $$t) (i32.const 0) funcref (ref.func $$f4))
  (elem func $$f5)
  (elem funcref (ref.func $$f6))

  (func
    (ref.func $$f1)
    (ref.func $$f2)
    (ref.func $$f3)
    (ref.func $$f4)
    (ref.func $$f5)
    (ref.func $$f6)
    (return)
  )
)`);

// ./test/core/ref_func.wast:108
assert_invalid(
  () => instantiate(`(module (func $$f (drop (ref.func $$f))))`),
  `undeclared function reference`,
);

// ./test/core/ref_func.wast:112
assert_invalid(
  () => instantiate(`(module (start $$f) (func $$f (drop (ref.func $$f))))`),
  `undeclared function reference`,
);
