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

// ./test/core/gc/br_on_cast_fail.wast

// ./test/core/gc/br_on_cast_fail.wast:3
let $0 = instantiate(`(module
  (type $$ft (func (result i32)))
  (type $$st (struct (field i16)))
  (type $$at (array i8))

  (table 10 anyref)

  (elem declare func $$f)
  (func $$f (result i32) (i32.const 9))

  (func (export "init") (param $$x externref)
    (table.set (i32.const 0) (ref.null any))
    (table.set (i32.const 1) (ref.i31 (i32.const 7)))
    (table.set (i32.const 2) (struct.new $$st (i32.const 6)))
    (table.set (i32.const 3) (array.new $$at (i32.const 5) (i32.const 3)))
    (table.set (i32.const 4) (any.convert_extern (local.get $$x)))
  )

  (func (export "br_on_non_null") (param $$i i32) (result i32)
    (block $$l (result (ref any))
      (br_on_non_null $$l (table.get (local.get $$i)))
      (return (i32.const 0))
    )
    (return (i32.const -1))
  )
  (func (export "br_on_non_i31") (param $$i i32) (result i32)
    (block $$l (result anyref)
      (br_on_cast_fail $$l anyref (ref i31) (table.get (local.get $$i)))
      (return (i31.get_u))
    )
    (return (i32.const -1))
  )
  (func (export "br_on_non_struct") (param $$i i32) (result i32)
    (block $$l (result anyref)
      (br_on_cast_fail $$l anyref (ref struct) (table.get (local.get $$i)))
      (block $$l2 (param structref) (result (ref $$st))
        (block $$l3 (param structref) (result (ref $$at))
          (br_on_cast $$l2 structref (ref $$st))
          (br_on_cast $$l3 anyref (ref $$at))
          (return (i32.const -2))
        )
        (return (array.get_u $$at (i32.const 0)))
      )
      (return (struct.get_s $$st 0))
    )
    (return (i32.const -1))
  )
  (func (export "br_on_non_array") (param $$i i32) (result i32)
    (block $$l (result anyref)
      (br_on_cast_fail $$l anyref (ref array) (table.get (local.get $$i)))
      (return (array.len))
    )
    (return (i32.const -1))
  )

  (func (export "null-diff") (param $$i i32) (result i32)
    (block $$l (result (ref any))
      (block (result (ref null struct))
        (br_on_cast_fail $$l (ref null any) (ref null struct) (table.get (local.get $$i)))
      )
      (return (i32.const 1))
    )
    (return (i32.const 0))
  )
)`);

// ./test/core/gc/br_on_cast_fail.wast:69
invoke($0, `init`, [externref(0)]);

// ./test/core/gc/br_on_cast_fail.wast:71
assert_return(() => invoke($0, `br_on_non_null`, [0]), [value("i32", 0)]);

// ./test/core/gc/br_on_cast_fail.wast:72
assert_return(() => invoke($0, `br_on_non_null`, [1]), [value("i32", -1)]);

// ./test/core/gc/br_on_cast_fail.wast:73
assert_return(() => invoke($0, `br_on_non_null`, [2]), [value("i32", -1)]);

// ./test/core/gc/br_on_cast_fail.wast:74
assert_return(() => invoke($0, `br_on_non_null`, [3]), [value("i32", -1)]);

// ./test/core/gc/br_on_cast_fail.wast:75
assert_return(() => invoke($0, `br_on_non_null`, [4]), [value("i32", -1)]);

// ./test/core/gc/br_on_cast_fail.wast:77
assert_return(() => invoke($0, `br_on_non_i31`, [0]), [value("i32", -1)]);

// ./test/core/gc/br_on_cast_fail.wast:78
assert_return(() => invoke($0, `br_on_non_i31`, [1]), [value("i32", 7)]);

// ./test/core/gc/br_on_cast_fail.wast:79
assert_return(() => invoke($0, `br_on_non_i31`, [2]), [value("i32", -1)]);

// ./test/core/gc/br_on_cast_fail.wast:80
assert_return(() => invoke($0, `br_on_non_i31`, [3]), [value("i32", -1)]);

// ./test/core/gc/br_on_cast_fail.wast:81
assert_return(() => invoke($0, `br_on_non_i31`, [4]), [value("i32", -1)]);

// ./test/core/gc/br_on_cast_fail.wast:83
assert_return(() => invoke($0, `br_on_non_struct`, [0]), [value("i32", -1)]);

// ./test/core/gc/br_on_cast_fail.wast:84
assert_return(() => invoke($0, `br_on_non_struct`, [1]), [value("i32", -1)]);

// ./test/core/gc/br_on_cast_fail.wast:85
assert_return(() => invoke($0, `br_on_non_struct`, [2]), [value("i32", 6)]);

// ./test/core/gc/br_on_cast_fail.wast:86
assert_return(() => invoke($0, `br_on_non_struct`, [3]), [value("i32", -1)]);

// ./test/core/gc/br_on_cast_fail.wast:87
assert_return(() => invoke($0, `br_on_non_struct`, [4]), [value("i32", -1)]);

// ./test/core/gc/br_on_cast_fail.wast:89
assert_return(() => invoke($0, `br_on_non_array`, [0]), [value("i32", -1)]);

// ./test/core/gc/br_on_cast_fail.wast:90
assert_return(() => invoke($0, `br_on_non_array`, [1]), [value("i32", -1)]);

// ./test/core/gc/br_on_cast_fail.wast:91
assert_return(() => invoke($0, `br_on_non_array`, [2]), [value("i32", -1)]);

// ./test/core/gc/br_on_cast_fail.wast:92
assert_return(() => invoke($0, `br_on_non_array`, [3]), [value("i32", 3)]);

// ./test/core/gc/br_on_cast_fail.wast:93
assert_return(() => invoke($0, `br_on_non_array`, [4]), [value("i32", -1)]);

// ./test/core/gc/br_on_cast_fail.wast:95
assert_return(() => invoke($0, `null-diff`, [0]), [value("i32", 1)]);

// ./test/core/gc/br_on_cast_fail.wast:96
assert_return(() => invoke($0, `null-diff`, [1]), [value("i32", 0)]);

// ./test/core/gc/br_on_cast_fail.wast:97
assert_return(() => invoke($0, `null-diff`, [2]), [value("i32", 1)]);

// ./test/core/gc/br_on_cast_fail.wast:98
assert_return(() => invoke($0, `null-diff`, [3]), [value("i32", 0)]);

// ./test/core/gc/br_on_cast_fail.wast:99
assert_return(() => invoke($0, `null-diff`, [4]), [value("i32", 0)]);

// ./test/core/gc/br_on_cast_fail.wast:104
let $1 = instantiate(`(module
  (type $$t0 (sub (struct)))
  (type $$t1 (sub $$t0 (struct (field i32))))
  (type $$t1' (sub $$t0 (struct (field i32))))
  (type $$t2 (sub $$t1 (struct (field i32 i32))))
  (type $$t2' (sub $$t1' (struct (field i32 i32))))
  (type $$t3 (sub $$t0 (struct (field i32 i32))))
  (type $$t0' (sub $$t0 (struct)))
  (type $$t4 (sub $$t0' (struct (field i32 i32))))

  (table 20 structref)

  (func $$init
    (table.set (i32.const 0) (struct.new_default $$t0))
    (table.set (i32.const 10) (struct.new_default $$t0))
    (table.set (i32.const 1) (struct.new_default $$t1))
    (table.set (i32.const 11) (struct.new_default $$t1'))
    (table.set (i32.const 2) (struct.new_default $$t2))
    (table.set (i32.const 12) (struct.new_default $$t2'))
    (table.set (i32.const 3) (struct.new_default $$t3 ))
    (table.set (i32.const 4) (struct.new_default $$t4))
  )

  (func (export "test-sub")
    (call $$init)
    (block $$l (result structref)
      ;; must not succeed
      (br_on_cast_fail $$l structref (ref null $$t0) (ref.null struct))
      (br_on_cast_fail $$l structref (ref null $$t0) (table.get (i32.const 0)))
      (br_on_cast_fail $$l structref (ref null $$t0) (table.get (i32.const 1)))
      (br_on_cast_fail $$l structref (ref null $$t0) (table.get (i32.const 2)))
      (br_on_cast_fail $$l structref (ref null $$t0) (table.get (i32.const 3)))
      (br_on_cast_fail $$l structref (ref null $$t0) (table.get (i32.const 4)))
      (br_on_cast_fail $$l structref (ref $$t0) (table.get (i32.const 0)))
      (br_on_cast_fail $$l structref (ref $$t0) (table.get (i32.const 1)))
      (br_on_cast_fail $$l structref (ref $$t0) (table.get (i32.const 2)))
      (br_on_cast_fail $$l structref (ref $$t0) (table.get (i32.const 3)))
      (br_on_cast_fail $$l structref (ref $$t0) (table.get (i32.const 4)))

      (br_on_cast_fail $$l structref (ref null $$t1) (ref.null struct))
      (br_on_cast_fail $$l structref (ref null $$t1) (table.get (i32.const 1)))
      (br_on_cast_fail $$l structref (ref null $$t1) (table.get (i32.const 2)))
      (br_on_cast_fail $$l structref (ref $$t1) (table.get (i32.const 1)))
      (br_on_cast_fail $$l structref (ref $$t1) (table.get (i32.const 2)))

      (br_on_cast_fail $$l structref (ref null $$t2) (ref.null struct))
      (br_on_cast_fail $$l structref (ref null $$t2) (table.get (i32.const 2)))
      (br_on_cast_fail $$l structref (ref $$t2) (table.get (i32.const 2)))

      (br_on_cast_fail $$l structref (ref null $$t3) (ref.null struct))
      (br_on_cast_fail $$l structref (ref null $$t3) (table.get (i32.const 3)))
      (br_on_cast_fail $$l structref (ref $$t3) (table.get (i32.const 3)))

      (br_on_cast_fail $$l structref (ref null $$t4) (ref.null struct))
      (br_on_cast_fail $$l structref (ref null $$t4) (table.get (i32.const 4)))
      (br_on_cast_fail $$l structref (ref $$t4) (table.get (i32.const 4)))

      ;; must succeed
      (drop (block (result structref) (br_on_cast_fail 0 structref (ref $$t0) (ref.null struct))))

      (drop (block (result structref) (br_on_cast_fail 0 structref (ref $$t1) (ref.null struct))))
      (drop (block (result structref) (br_on_cast_fail 0 structref (ref $$t1) (table.get (i32.const 0)))))
      (drop (block (result structref) (br_on_cast_fail 0 structref (ref $$t1) (table.get (i32.const 3)))))
      (drop (block (result structref) (br_on_cast_fail 0 structref (ref $$t1) (table.get (i32.const 4)))))

      (drop (block (result structref) (br_on_cast_fail 0 structref (ref $$t2) (ref.null struct))))
      (drop (block (result structref) (br_on_cast_fail 0 structref (ref $$t2) (table.get (i32.const 0)))))
      (drop (block (result structref) (br_on_cast_fail 0 structref (ref $$t2) (table.get (i32.const 1)))))
      (drop (block (result structref) (br_on_cast_fail 0 structref (ref $$t2) (table.get (i32.const 3)))))
      (drop (block (result structref) (br_on_cast_fail 0 structref (ref $$t2) (table.get (i32.const 4)))))

      (drop (block (result structref) (br_on_cast_fail 0 structref (ref $$t3) (ref.null struct))))
      (drop (block (result structref) (br_on_cast_fail 0 structref (ref $$t3) (table.get (i32.const 0)))))
      (drop (block (result structref) (br_on_cast_fail 0 structref (ref $$t3) (table.get (i32.const 1)))))
      (drop (block (result structref) (br_on_cast_fail 0 structref (ref $$t3) (table.get (i32.const 2)))))
      (drop (block (result structref) (br_on_cast_fail 0 structref (ref $$t3) (table.get (i32.const 4)))))

      (drop (block (result structref) (br_on_cast_fail 0 structref (ref $$t4) (ref.null struct))))
      (drop (block (result structref) (br_on_cast_fail 0 structref (ref $$t4) (table.get (i32.const 0)))))
      (drop (block (result structref) (br_on_cast_fail 0 structref (ref $$t4) (table.get (i32.const 1)))))
      (drop (block (result structref) (br_on_cast_fail 0 structref (ref $$t4) (table.get (i32.const 2)))))
      (drop (block (result structref) (br_on_cast_fail 0 structref (ref $$t4) (table.get (i32.const 3)))))

      (return)
    )
    (unreachable)
  )

  (func (export "test-canon")
    (call $$init)
    (block $$l (result structref)
      (br_on_cast_fail $$l structref (ref $$t0) (table.get (i32.const 0)))
      (br_on_cast_fail $$l structref (ref $$t0) (table.get (i32.const 1)))
      (br_on_cast_fail $$l structref (ref $$t0) (table.get (i32.const 2)))
      (br_on_cast_fail $$l structref (ref $$t0) (table.get (i32.const 3)))
      (br_on_cast_fail $$l structref (ref $$t0) (table.get (i32.const 4)))
      (br_on_cast_fail $$l structref (ref $$t0) (table.get (i32.const 10)))
      (br_on_cast_fail $$l structref (ref $$t0) (table.get (i32.const 11)))
      (br_on_cast_fail $$l structref (ref $$t0) (table.get (i32.const 12)))

      (br_on_cast_fail $$l structref (ref $$t1') (table.get (i32.const 1)))
      (br_on_cast_fail $$l structref (ref $$t1') (table.get (i32.const 2)))

      (br_on_cast_fail $$l structref (ref $$t1) (table.get (i32.const 11)))
      (br_on_cast_fail $$l structref (ref $$t1) (table.get (i32.const 12)))

      (br_on_cast_fail $$l structref (ref $$t2') (table.get (i32.const 2)))

      (br_on_cast_fail $$l structref (ref $$t2) (table.get (i32.const 12)))

      (return)
    )
    (unreachable)
  )
)`);

// ./test/core/gc/br_on_cast_fail.wast:220
invoke($1, `test-sub`, []);

// ./test/core/gc/br_on_cast_fail.wast:221
invoke($1, `test-canon`, []);

// ./test/core/gc/br_on_cast_fail.wast:226
let $2 = instantiate(`(module
  (type $$t (struct))

  (func (param (ref any)) (result (ref any))
    (block (result (ref $$t)) (br_on_cast_fail 1 (ref any) (ref $$t) (local.get 0)))
  )
  (func (param (ref null any)) (result (ref null any))
    (block (result (ref $$t)) (br_on_cast_fail 1 (ref null any) (ref $$t) (local.get 0)))
  )
  (func (param (ref null any)) (result (ref null any))
    (block (result (ref null $$t)) (br_on_cast_fail 1 (ref null any) (ref null $$t) (local.get 0)))
  )
)`);

// ./test/core/gc/br_on_cast_fail.wast:240
assert_invalid(
  () => instantiate(`(module
    (type $$t (struct))
    (func (param (ref any)) (result (ref any))
      (block (result (ref $$t)) (br_on_cast_fail 1 (ref null any) (ref null $$t) (local.get 0)))
    )
  )`),
  `type mismatch`,
);

// ./test/core/gc/br_on_cast_fail.wast:249
assert_invalid(
  () => instantiate(`(module
    (type $$t (struct))
    (func (param (ref any)) (result (ref any))
      (block (result (ref null $$t)) (br_on_cast_fail 1 (ref any) (ref null $$t) (local.get 0))) (ref.as_non_null)
    )
  )`),
  `type mismatch`,
);

// ./test/core/gc/br_on_cast_fail.wast:258
assert_invalid(
  () => instantiate(`(module
    (type $$t (struct))
    (func (param (ref null any)) (result (ref any))
      (block (result (ref $$t)) (br_on_cast_fail 1 (ref null any) (ref $$t) (local.get 0)))
    )
  )`),
  `type mismatch`,
);

// ./test/core/gc/br_on_cast_fail.wast:267
assert_invalid(
  () => instantiate(`(module
    (func (result anyref)
      (br_on_cast_fail 0 eqref anyref (unreachable))
    )
  )`),
  `type mismatch`,
);

// ./test/core/gc/br_on_cast_fail.wast:275
assert_invalid(
  () => instantiate(`(module
    (func (result anyref)
      (br_on_cast_fail 0 structref arrayref (unreachable))
    )
  )`),
  `type mismatch`,
);
