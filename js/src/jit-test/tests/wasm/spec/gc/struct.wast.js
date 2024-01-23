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

// ./test/core/gc/struct.wast

// ./test/core/gc/struct.wast:3
let $0 = instantiate(`(module
  (type (struct))
  (type (struct (field)))
  (type (struct (field i8)))
  (type (struct (field i8 i8 i8 i8)))
  (type (struct (field $$x1 i32) (field $$y1 i32)))
  (type (struct (field i8 i16 i32 i64 f32 f64 anyref funcref (ref 0) (ref null 1))))
  (type (struct (field i32 i64 i8) (field) (field) (field (ref null i31) anyref)))
  (type (struct (field $$x2 i32) (field f32 f64) (field $$y2 i32)))
)`);

// ./test/core/gc/struct.wast:15
assert_malformed(
  () => instantiate(`(type (struct (field $$x i32) (field $$x i32))) `),
  `duplicate field`,
);

// ./test/core/gc/struct.wast:25
let $1 = instantiate(`(module
  (rec
    (type $$s0 (struct (field (ref 0) (ref 1) (ref $$s0) (ref $$s1))))
    (type $$s1 (struct (field (ref 0) (ref 1) (ref $$s0) (ref $$s1))))
  )

  (func (param (ref $$forward)))

  (type $$forward (struct))
)`);

// ./test/core/gc/struct.wast:36
assert_invalid(
  () => instantiate(`(module (type (struct (field (ref 1)))))`),
  `unknown type`,
);

// ./test/core/gc/struct.wast:40
assert_invalid(
  () => instantiate(`(module (type (struct (field (mut (ref 1))))))`),
  `unknown type`,
);

// ./test/core/gc/struct.wast:48
let $2 = instantiate(`(module
  (type (struct (field $$x i32)))
  (type $$t1 (struct (field i32) (field $$x f32)))
  (type $$t2 (struct (field i32 i32) (field $$x i64)))

  (func (param (ref 0)) (result i32) (struct.get 0 $$x (local.get 0)))
  (func (param (ref $$t1)) (result f32) (struct.get 1 $$x (local.get 0)))
  (func (param (ref $$t2)) (result i64) (struct.get $$t2 $$x (local.get 0)))
)`);

// ./test/core/gc/struct.wast:58
assert_invalid(
  () => instantiate(`(module
    (type (struct (field $$x i64)))
    (type $$t (struct (field $$x i32)))
    (func (param (ref 0)) (result i32) (struct.get 0 $$x (local.get 0)))
  )`),
  `type mismatch`,
);

// ./test/core/gc/struct.wast:70
let $3 = instantiate(`(module
  (type $$vec (struct (field f32) (field $$y (mut f32)) (field $$z f32)))

  (global (ref $$vec) (struct.new $$vec (f32.const 1) (f32.const 2) (f32.const 3)))
  (global (ref $$vec) (struct.new_default $$vec))

  (func (export "new") (result anyref)
    (struct.new_default $$vec)
  )

  (func $$get_0_0 (param $$v (ref $$vec)) (result f32)
    (struct.get 0 0 (local.get $$v))
  )
  (func (export "get_0_0") (result f32)
    (call $$get_0_0 (struct.new_default $$vec))
  )
  (func $$get_vec_0 (param $$v (ref $$vec)) (result f32)
    (struct.get $$vec 0 (local.get $$v))
  )
  (func (export "get_vec_0") (result f32)
    (call $$get_vec_0 (struct.new_default $$vec))
  )
  (func $$get_0_y (param $$v (ref $$vec)) (result f32)
    (struct.get 0 $$y (local.get $$v))
  )
  (func (export "get_0_y") (result f32)
    (call $$get_0_y (struct.new_default $$vec))
  )
  (func $$get_vec_y (param $$v (ref $$vec)) (result f32)
    (struct.get $$vec $$y (local.get $$v))
  )
  (func (export "get_vec_y") (result f32)
    (call $$get_vec_y (struct.new_default $$vec))
  )

  (func $$set_get_y (param $$v (ref $$vec)) (param $$y f32) (result f32)
    (struct.set $$vec $$y (local.get $$v) (local.get $$y))
    (struct.get $$vec $$y (local.get $$v))
  )
  (func (export "set_get_y") (param $$y f32) (result f32)
    (call $$set_get_y (struct.new_default $$vec) (local.get $$y))
  )

  (func $$set_get_1 (param $$v (ref $$vec)) (param $$y f32) (result f32)
    (struct.set $$vec 1 (local.get $$v) (local.get $$y))
    (struct.get $$vec $$y (local.get $$v))
  )
  (func (export "set_get_1") (param $$y f32) (result f32)
    (call $$set_get_1 (struct.new_default $$vec) (local.get $$y))
  )
)`);

// ./test/core/gc/struct.wast:122
assert_return(() => invoke($3, `new`, []), [new RefWithType('structref')]);

// ./test/core/gc/struct.wast:124
assert_return(() => invoke($3, `get_0_0`, []), [value("f32", 0)]);

// ./test/core/gc/struct.wast:125
assert_return(() => invoke($3, `get_vec_0`, []), [value("f32", 0)]);

// ./test/core/gc/struct.wast:126
assert_return(() => invoke($3, `get_0_y`, []), [value("f32", 0)]);

// ./test/core/gc/struct.wast:127
assert_return(() => invoke($3, `get_vec_y`, []), [value("f32", 0)]);

// ./test/core/gc/struct.wast:129
assert_return(() => invoke($3, `set_get_y`, [value("f32", 7)]), [value("f32", 7)]);

// ./test/core/gc/struct.wast:130
assert_return(() => invoke($3, `set_get_1`, [value("f32", 7)]), [value("f32", 7)]);

// ./test/core/gc/struct.wast:132
assert_invalid(
  () => instantiate(`(module
    (type $$s (struct (field i64)))
    (func (export "struct.set-immutable") (param $$s (ref $$s))
      (struct.set $$s 0 (local.get $$s) (i64.const 1))
    )
  )`),
  `field is immutable`,
);

// ./test/core/gc/struct.wast:145
let $4 = instantiate(`(module
  (type $$t (struct (field i32 (mut i32))))
  (func (export "struct.get-null")
    (local (ref null $$t)) (drop (struct.get $$t 1 (local.get 0)))
  )
  (func (export "struct.set-null")
    (local (ref null $$t)) (struct.set $$t 1 (local.get 0) (i32.const 0))
  )
)`);

// ./test/core/gc/struct.wast:155
assert_trap(() => invoke($4, `struct.get-null`, []), `null structure reference`);

// ./test/core/gc/struct.wast:156
assert_trap(() => invoke($4, `struct.set-null`, []), `null structure reference`);

// ./test/core/gc/struct.wast:160
let $5 = instantiate(`(module
  (type $$s (struct (field i8) (field (mut i8)) (field i16) (field (mut i16))))

  (global (export "g0") (ref $$s) (struct.new $$s (i32.const 0) (i32.const 1) (i32.const 2) (i32.const 3)))
  (global (export "g1") (ref $$s) (struct.new $$s (i32.const 254) (i32.const 255) (i32.const 65534) (i32.const 65535)))

  (func (export "get_packed_g0_0") (result i32 i32)
    (struct.get_s 0 0 (global.get 0))
    (struct.get_u 0 0 (global.get 0))
  )

  (func (export "get_packed_g1_0") (result i32 i32)
    (struct.get_s 0 0 (global.get 1))
    (struct.get_u 0 0 (global.get 1))
  )

  (func (export "get_packed_g0_1") (result i32 i32)
    (struct.get_s 0 1 (global.get 0))
    (struct.get_u 0 1 (global.get 0))
  )

  (func (export "get_packed_g1_1") (result i32 i32)
    (struct.get_s 0 1 (global.get 1))
    (struct.get_u 0 1 (global.get 1))
  )

  (func (export "get_packed_g0_2") (result i32 i32)
    (struct.get_s 0 2 (global.get 0))
    (struct.get_u 0 2 (global.get 0))
  )

  (func (export "get_packed_g1_2") (result i32 i32)
    (struct.get_s 0 2 (global.get 1))
    (struct.get_u 0 2 (global.get 1))
  )

  (func (export "get_packed_g0_3") (result i32 i32)
    (struct.get_s 0 3 (global.get 0))
    (struct.get_u 0 3 (global.get 0))
  )

  (func (export "get_packed_g1_3") (result i32 i32)
    (struct.get_s 0 3 (global.get 1))
    (struct.get_u 0 3 (global.get 1))
  )

  (func (export "set_get_packed_g0_1") (param i32) (result i32 i32)
    (struct.set 0 1 (global.get 0) (local.get 0))
    (struct.get_s 0 1 (global.get 0))
    (struct.get_u 0 1 (global.get 0))
  )

  (func (export "set_get_packed_g0_3") (param i32) (result i32 i32)
    (struct.set 0 3 (global.get 0) (local.get 0))
    (struct.get_s 0 3 (global.get 0))
    (struct.get_u 0 3 (global.get 0))
  )
)`);

// ./test/core/gc/struct.wast:219
assert_return(() => invoke($5, `get_packed_g0_0`, []), [value("i32", 0), value("i32", 0)]);

// ./test/core/gc/struct.wast:220
assert_return(() => invoke($5, `get_packed_g1_0`, []), [value("i32", -2), value("i32", 254)]);

// ./test/core/gc/struct.wast:221
assert_return(() => invoke($5, `get_packed_g0_1`, []), [value("i32", 1), value("i32", 1)]);

// ./test/core/gc/struct.wast:222
assert_return(() => invoke($5, `get_packed_g1_1`, []), [value("i32", -1), value("i32", 255)]);

// ./test/core/gc/struct.wast:223
assert_return(() => invoke($5, `get_packed_g0_2`, []), [value("i32", 2), value("i32", 2)]);

// ./test/core/gc/struct.wast:224
assert_return(() => invoke($5, `get_packed_g1_2`, []), [value("i32", -2), value("i32", 65534)]);

// ./test/core/gc/struct.wast:225
assert_return(() => invoke($5, `get_packed_g0_3`, []), [value("i32", 3), value("i32", 3)]);

// ./test/core/gc/struct.wast:226
assert_return(() => invoke($5, `get_packed_g1_3`, []), [value("i32", -1), value("i32", 65535)]);

// ./test/core/gc/struct.wast:228
assert_return(() => invoke($5, `set_get_packed_g0_1`, [257]), [value("i32", 1), value("i32", 1)]);

// ./test/core/gc/struct.wast:229
assert_return(() => invoke($5, `set_get_packed_g0_3`, [257]), [value("i32", 257), value("i32", 257)]);
