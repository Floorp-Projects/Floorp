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

// ./test/core/gc/array.wast

// ./test/core/gc/array.wast:3
let $0 = instantiate(`(module
  (type (array i8))
  (type (array i16))
  (type (array i32))
  (type (array i64))
  (type (array f32))
  (type (array f64))
  (type (array anyref))
  (type (array (ref struct)))
  (type (array (ref 0)))
  (type (array (ref null 1)))
  (type (array (mut i8)))
  (type (array (mut i16)))
  (type (array (mut i32)))
  (type (array (mut i64)))
  (type (array (mut i32)))
  (type (array (mut i64)))
  (type (array (mut anyref)))
  (type (array (mut (ref struct))))
  (type (array (mut (ref 0))))
  (type (array (mut (ref null i31))))
)`);

// ./test/core/gc/array.wast:27
assert_invalid(
  () => instantiate(`(module
    (type (array (mut (ref null 10))))
  )`),
  `unknown type`,
);

// ./test/core/gc/array.wast:37
let $1 = instantiate(`(module
  (rec
    (type $$s0 (array (ref $$s1)))
    (type $$s1 (array (ref $$s0)))
  )

  (func (param (ref $$forward)))

  (type $$forward (array i32))
)`);

// ./test/core/gc/array.wast:48
assert_invalid(() => instantiate(`(module (type (array (ref 1))))`), `unknown type`);

// ./test/core/gc/array.wast:52
assert_invalid(() => instantiate(`(module (type (array (mut (ref 1)))))`), `unknown type`);

// ./test/core/gc/array.wast:60
let $2 = instantiate(`(module
  (type $$vec (array f32))
  (type $$mvec (array (mut f32)))

  (global (ref $$vec) (array.new $$vec (f32.const 1) (i32.const 3)))
  (global (ref $$vec) (array.new_default $$vec (i32.const 3)))

  (func $$new (export "new") (result (ref $$vec))
    (array.new_default $$vec (i32.const 3))
  )

  (func $$get (param $$i i32) (param $$v (ref $$vec)) (result f32)
    (array.get $$vec (local.get $$v) (local.get $$i))
  )
  (func (export "get") (param $$i i32) (result f32)
    (call $$get (local.get $$i) (call $$new))
  )

  (func $$set_get (param $$i i32) (param $$v (ref $$mvec)) (param $$y f32) (result f32)
    (array.set $$mvec (local.get $$v) (local.get $$i) (local.get $$y))
    (array.get $$mvec (local.get $$v) (local.get $$i))
  )
  (func (export "set_get") (param $$i i32) (param $$y f32) (result f32)
    (call $$set_get (local.get $$i)
      (array.new_default $$mvec (i32.const 3))
      (local.get $$y)
    )
  )

  (func $$len (param $$v (ref array)) (result i32)
    (array.len (local.get $$v))
  )
  (func (export "len") (result i32)
    (call $$len (call $$new))
  )
)`);

// ./test/core/gc/array.wast:97
assert_return(() => invoke($2, `new`, []), [new RefWithType('arrayref')]);

// ./test/core/gc/array.wast:98
assert_return(() => invoke($2, `new`, []), [new RefWithType('eqref')]);

// ./test/core/gc/array.wast:99
assert_return(() => invoke($2, `get`, [0]), [value("f32", 0)]);

// ./test/core/gc/array.wast:100
assert_return(() => invoke($2, `set_get`, [1, value("f32", 7)]), [value("f32", 7)]);

// ./test/core/gc/array.wast:101
assert_return(() => invoke($2, `len`, []), [value("i32", 3)]);

// ./test/core/gc/array.wast:103
assert_trap(() => invoke($2, `get`, [10]), `out of bounds`);

// ./test/core/gc/array.wast:104
assert_trap(() => invoke($2, `set_get`, [10, value("f32", 7)]), `out of bounds`);

// ./test/core/gc/array.wast:106
let $3 = instantiate(`(module
  (type $$vec (array f32))
  (type $$mvec (array (mut f32)))

  (global (ref $$vec) (array.new_fixed $$vec 2 (f32.const 1) (f32.const 2)))

  (func $$new (export "new") (result (ref $$vec))
    (array.new_fixed $$vec 2 (f32.const 1) (f32.const 2))
  )

  (func $$get (param $$i i32) (param $$v (ref $$vec)) (result f32)
    (array.get $$vec (local.get $$v) (local.get $$i))
  )
  (func (export "get") (param $$i i32) (result f32)
    (call $$get (local.get $$i) (call $$new))
  )

  (func $$set_get (param $$i i32) (param $$v (ref $$mvec)) (param $$y f32) (result f32)
    (array.set $$mvec (local.get $$v) (local.get $$i) (local.get $$y))
    (array.get $$mvec (local.get $$v) (local.get $$i))
  )
  (func (export "set_get") (param $$i i32) (param $$y f32) (result f32)
    (call $$set_get (local.get $$i)
      (array.new_fixed $$mvec 3 (f32.const 1) (f32.const 2) (f32.const 3))
      (local.get $$y)
    )
  )

  (func $$len (param $$v (ref array)) (result i32)
    (array.len (local.get $$v))
  )
  (func (export "len") (result i32)
    (call $$len (call $$new))
  )
)`);

// ./test/core/gc/array.wast:142
assert_return(() => invoke($3, `new`, []), [new RefWithType('arrayref')]);

// ./test/core/gc/array.wast:143
assert_return(() => invoke($3, `new`, []), [new RefWithType('eqref')]);

// ./test/core/gc/array.wast:144
assert_return(() => invoke($3, `get`, [0]), [value("f32", 1)]);

// ./test/core/gc/array.wast:145
assert_return(() => invoke($3, `set_get`, [1, value("f32", 7)]), [value("f32", 7)]);

// ./test/core/gc/array.wast:146
assert_return(() => invoke($3, `len`, []), [value("i32", 2)]);

// ./test/core/gc/array.wast:148
assert_trap(() => invoke($3, `get`, [10]), `out of bounds`);

// ./test/core/gc/array.wast:149
assert_trap(() => invoke($3, `set_get`, [10, value("f32", 7)]), `out of bounds`);

// ./test/core/gc/array.wast:151
let $4 = instantiate(`(module
  (type $$vec (array i8))
  (type $$mvec (array (mut i8)))

  (data $$d "\\00\\01\\02\\03\\04")

  (func $$new (export "new") (result (ref $$vec))
    (array.new_data $$vec $$d (i32.const 1) (i32.const 3))
  )

  (func $$get (param $$i i32) (param $$v (ref $$vec)) (result i32)
    (array.get_u $$vec (local.get $$v) (local.get $$i))
  )
  (func (export "get") (param $$i i32) (result i32)
    (call $$get (local.get $$i) (call $$new))
  )

  (func $$set_get (param $$i i32) (param $$v (ref $$mvec)) (param $$y i32) (result i32)
    (array.set $$mvec (local.get $$v) (local.get $$i) (local.get $$y))
    (array.get_u $$mvec (local.get $$v) (local.get $$i))
  )
  (func (export "set_get") (param $$i i32) (param $$y i32) (result i32)
    (call $$set_get (local.get $$i)
      (array.new_data $$mvec $$d (i32.const 1) (i32.const 3))
      (local.get $$y)
    )
  )

  (func $$len (param $$v (ref array)) (result i32)
    (array.len (local.get $$v))
  )
  (func (export "len") (result i32)
    (call $$len (call $$new))
  )
)`);

// ./test/core/gc/array.wast:187
assert_return(() => invoke($4, `new`, []), [new RefWithType('arrayref')]);

// ./test/core/gc/array.wast:188
assert_return(() => invoke($4, `new`, []), [new RefWithType('eqref')]);

// ./test/core/gc/array.wast:189
assert_return(() => invoke($4, `get`, [0]), [value("i32", 1)]);

// ./test/core/gc/array.wast:190
assert_return(() => invoke($4, `set_get`, [1, 7]), [value("i32", 7)]);

// ./test/core/gc/array.wast:191
assert_return(() => invoke($4, `len`, []), [value("i32", 3)]);

// ./test/core/gc/array.wast:193
assert_trap(() => invoke($4, `get`, [10]), `out of bounds`);

// ./test/core/gc/array.wast:194
assert_trap(() => invoke($4, `set_get`, [10, 7]), `out of bounds`);

// ./test/core/gc/array.wast:196
let $5 = instantiate(`(module
  (type $$bvec (array i8))
  (type $$vec (array (ref $$bvec)))
  (type $$mvec (array (mut (ref $$bvec))))
  (type $$nvec (array (ref null $$bvec)))
  (type $$avec (array (mut anyref)))

  (elem $$e (ref $$bvec)
    (array.new $$bvec (i32.const 7) (i32.const 3))
    (array.new_fixed $$bvec 2 (i32.const 1) (i32.const 2))
  )

  (func $$new (export "new") (result (ref $$vec))
    (array.new_elem $$vec $$e (i32.const 0) (i32.const 2))
  )

  (func $$sub1 (result (ref $$nvec))
    (array.new_elem $$nvec $$e (i32.const 0) (i32.const 2))
  )
  (func $$sub2 (result (ref $$avec))
    (array.new_elem $$avec $$e (i32.const 0) (i32.const 2))
  )

  (func $$get (param $$i i32) (param $$j i32) (param $$v (ref $$vec)) (result i32)
    (array.get_u $$bvec (array.get $$vec (local.get $$v) (local.get $$i)) (local.get $$j))
  )
  (func (export "get") (param $$i i32) (param $$j i32) (result i32)
    (call $$get (local.get $$i) (local.get $$j) (call $$new))
  )

  (func $$set_get (param $$i i32) (param $$j i32) (param $$v (ref $$mvec)) (param $$y i32) (result i32)
    (array.set $$mvec (local.get $$v) (local.get $$i) (array.get $$mvec (local.get $$v) (local.get $$y)))
    (array.get_u $$bvec (array.get $$mvec (local.get $$v) (local.get $$i)) (local.get $$j))
  )
  (func (export "set_get") (param $$i i32) (param $$j i32) (param $$y i32) (result i32)
    (call $$set_get (local.get $$i) (local.get $$j)
      (array.new_elem $$mvec $$e (i32.const 0) (i32.const 2))
      (local.get $$y)
    )
  )

  (func $$len (param $$v (ref array)) (result i32)
    (array.len (local.get $$v))
  )
  (func (export "len") (result i32)
    (call $$len (call $$new))
  )
)`);

// ./test/core/gc/array.wast:245
assert_return(() => invoke($5, `new`, []), [new RefWithType('arrayref')]);

// ./test/core/gc/array.wast:246
assert_return(() => invoke($5, `new`, []), [new RefWithType('eqref')]);

// ./test/core/gc/array.wast:247
assert_return(() => invoke($5, `get`, [0, 0]), [value("i32", 7)]);

// ./test/core/gc/array.wast:248
assert_return(() => invoke($5, `get`, [1, 0]), [value("i32", 1)]);

// ./test/core/gc/array.wast:249
assert_return(() => invoke($5, `set_get`, [0, 1, 1]), [value("i32", 2)]);

// ./test/core/gc/array.wast:250
assert_return(() => invoke($5, `len`, []), [value("i32", 2)]);

// ./test/core/gc/array.wast:252
assert_trap(() => invoke($5, `get`, [10, 0]), `out of bounds`);

// ./test/core/gc/array.wast:253
assert_trap(() => invoke($5, `set_get`, [10, 0, 0]), `out of bounds`);

// ./test/core/gc/array.wast:255
assert_invalid(
  () => instantiate(`(module
    (type $$a (array i64))
    (func (export "array.set-immutable") (param $$a (ref $$a))
      (array.set $$a (local.get $$a) (i32.const 0) (i64.const 1))
    )
  )`),
  `array is immutable`,
);

// ./test/core/gc/array.wast:265
assert_invalid(
  () => instantiate(`(module
    (type $$bvec (array i8))

    (data $$d "\\00\\01\\02\\03\\04")

    (global (ref $$bvec)
      (array.new_data $$bvec $$d (i32.const 1) (i32.const 3))
    )
  )`),
  `constant expression required`,
);

// ./test/core/gc/array.wast:278
assert_invalid(
  () => instantiate(`(module
    (type $$bvec (array i8))
    (type $$vvec (array (ref $$bvec)))

    (elem $$e (ref $$bvec) (ref.null $$bvec))

    (global (ref $$vvec)
      (array.new_elem $$vvec $$e (i32.const 0) (i32.const 1))
    )
  )`),
  `constant expression required`,
);

// ./test/core/gc/array.wast:295
let $6 = instantiate(`(module
  (type $$t (array (mut i32)))
  (func (export "array.get-null")
    (local (ref null $$t)) (drop (array.get $$t (local.get 0) (i32.const 0)))
  )
  (func (export "array.set-null")
    (local (ref null $$t)) (array.set $$t (local.get 0) (i32.const 0) (i32.const 0))
  )
)`);

// ./test/core/gc/array.wast:305
assert_trap(() => invoke($6, `array.get-null`, []), `null array`);

// ./test/core/gc/array.wast:306
assert_trap(() => invoke($6, `array.set-null`, []), `null array`);
