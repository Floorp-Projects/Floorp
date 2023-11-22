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

// ./test/core/gc/ref_test.wast

// ./test/core/gc/ref_test.wast:3
let $0 = instantiate(`(module
  (type $$ft (func))
  (type $$st (struct))
  (type $$at (array i8))

  (table $$ta 10 anyref)
  (table $$tf 10 funcref)
  (table $$te 10 externref)

  (elem declare func $$f)
  (func $$f)

  (func (export "init") (param $$x externref)
    (table.set $$ta (i32.const 0) (ref.null any))
    (table.set $$ta (i32.const 1) (ref.null struct))
    (table.set $$ta (i32.const 2) (ref.null none))
    (table.set $$ta (i32.const 3) (ref.i31 (i32.const 7)))
    (table.set $$ta (i32.const 4) (struct.new_default $$st))
    (table.set $$ta (i32.const 5) (array.new_default $$at (i32.const 0)))
    (table.set $$ta (i32.const 6) (any.convert_extern (local.get $$x)))
    (table.set $$ta (i32.const 7) (any.convert_extern (ref.null extern)))

    (table.set $$tf (i32.const 0) (ref.null nofunc))
    (table.set $$tf (i32.const 1) (ref.null func))
    (table.set $$tf (i32.const 2) (ref.func $$f))

    (table.set $$te (i32.const 0) (ref.null noextern))
    (table.set $$te (i32.const 1) (ref.null extern))
    (table.set $$te (i32.const 2) (local.get $$x))
    (table.set $$te (i32.const 3) (extern.convert_any (ref.i31 (i32.const 8))))
    (table.set $$te (i32.const 4) (extern.convert_any (struct.new_default $$st)))
    (table.set $$te (i32.const 5) (extern.convert_any (ref.null any)))
  )

  (func (export "ref_test_null_data") (param $$i i32) (result i32)
    (i32.add
      (ref.is_null (table.get $$ta (local.get $$i)))
      (ref.test nullref (table.get $$ta (local.get $$i)))
    )
  )
  (func (export "ref_test_any") (param $$i i32) (result i32)
    (i32.add
      (ref.test (ref any) (table.get $$ta (local.get $$i)))
      (ref.test anyref (table.get $$ta (local.get $$i)))
    )
  )
  (func (export "ref_test_eq") (param $$i i32) (result i32)
    (i32.add
      (ref.test (ref eq) (table.get $$ta (local.get $$i)))
      (ref.test eqref (table.get $$ta (local.get $$i)))
    )
  )
  (func (export "ref_test_i31") (param $$i i32) (result i32)
    (i32.add
      (ref.test (ref i31) (table.get $$ta (local.get $$i)))
      (ref.test i31ref (table.get $$ta (local.get $$i)))
    )
  )
  (func (export "ref_test_struct") (param $$i i32) (result i32)
    (i32.add
      (ref.test (ref struct) (table.get $$ta (local.get $$i)))
      (ref.test structref (table.get $$ta (local.get $$i)))
    )
  )
  (func (export "ref_test_array") (param $$i i32) (result i32)
    (i32.add
      (ref.test (ref array) (table.get $$ta (local.get $$i)))
      (ref.test arrayref (table.get $$ta (local.get $$i)))
    )
  )

  (func (export "ref_test_null_func") (param $$i i32) (result i32)
    (i32.add
      (ref.is_null (table.get $$tf (local.get $$i)))
      (ref.test (ref null nofunc) (table.get $$tf (local.get $$i)))
    )
  )
  (func (export "ref_test_func") (param $$i i32) (result i32)
    (i32.add
      (ref.test (ref func) (table.get $$tf (local.get $$i)))
      (ref.test funcref (table.get $$tf (local.get $$i)))
    )
  )

  (func (export "ref_test_null_extern") (param $$i i32) (result i32)
    (i32.add
      (ref.is_null (table.get $$te (local.get $$i)))
      (ref.test (ref null noextern) (table.get $$te (local.get $$i)))
    )
  )
  (func (export "ref_test_extern") (param $$i i32) (result i32)
    (i32.add
      (ref.test (ref extern) (table.get $$te (local.get $$i)))
      (ref.test externref (table.get $$te (local.get $$i)))
    )
  )
)`);

// ./test/core/gc/ref_test.wast:101
invoke($0, `init`, [externref(0)]);

// ./test/core/gc/ref_test.wast:103
assert_return(() => invoke($0, `ref_test_null_data`, [0]), [value("i32", 2)]);

// ./test/core/gc/ref_test.wast:104
assert_return(() => invoke($0, `ref_test_null_data`, [1]), [value("i32", 2)]);

// ./test/core/gc/ref_test.wast:105
assert_return(() => invoke($0, `ref_test_null_data`, [2]), [value("i32", 2)]);

// ./test/core/gc/ref_test.wast:106
assert_return(() => invoke($0, `ref_test_null_data`, [3]), [value("i32", 0)]);

// ./test/core/gc/ref_test.wast:107
assert_return(() => invoke($0, `ref_test_null_data`, [4]), [value("i32", 0)]);

// ./test/core/gc/ref_test.wast:108
assert_return(() => invoke($0, `ref_test_null_data`, [5]), [value("i32", 0)]);

// ./test/core/gc/ref_test.wast:109
assert_return(() => invoke($0, `ref_test_null_data`, [6]), [value("i32", 0)]);

// ./test/core/gc/ref_test.wast:110
assert_return(() => invoke($0, `ref_test_null_data`, [7]), [value("i32", 2)]);

// ./test/core/gc/ref_test.wast:112
assert_return(() => invoke($0, `ref_test_any`, [0]), [value("i32", 1)]);

// ./test/core/gc/ref_test.wast:113
assert_return(() => invoke($0, `ref_test_any`, [1]), [value("i32", 1)]);

// ./test/core/gc/ref_test.wast:114
assert_return(() => invoke($0, `ref_test_any`, [2]), [value("i32", 1)]);

// ./test/core/gc/ref_test.wast:115
assert_return(() => invoke($0, `ref_test_any`, [3]), [value("i32", 2)]);

// ./test/core/gc/ref_test.wast:116
assert_return(() => invoke($0, `ref_test_any`, [4]), [value("i32", 2)]);

// ./test/core/gc/ref_test.wast:117
assert_return(() => invoke($0, `ref_test_any`, [5]), [value("i32", 2)]);

// ./test/core/gc/ref_test.wast:118
assert_return(() => invoke($0, `ref_test_any`, [6]), [value("i32", 2)]);

// ./test/core/gc/ref_test.wast:119
assert_return(() => invoke($0, `ref_test_any`, [7]), [value("i32", 1)]);

// ./test/core/gc/ref_test.wast:121
assert_return(() => invoke($0, `ref_test_eq`, [0]), [value("i32", 1)]);

// ./test/core/gc/ref_test.wast:122
assert_return(() => invoke($0, `ref_test_eq`, [1]), [value("i32", 1)]);

// ./test/core/gc/ref_test.wast:123
assert_return(() => invoke($0, `ref_test_eq`, [2]), [value("i32", 1)]);

// ./test/core/gc/ref_test.wast:124
assert_return(() => invoke($0, `ref_test_eq`, [3]), [value("i32", 2)]);

// ./test/core/gc/ref_test.wast:125
assert_return(() => invoke($0, `ref_test_eq`, [4]), [value("i32", 2)]);

// ./test/core/gc/ref_test.wast:126
assert_return(() => invoke($0, `ref_test_eq`, [5]), [value("i32", 2)]);

// ./test/core/gc/ref_test.wast:127
assert_return(() => invoke($0, `ref_test_eq`, [6]), [value("i32", 0)]);

// ./test/core/gc/ref_test.wast:128
assert_return(() => invoke($0, `ref_test_eq`, [7]), [value("i32", 1)]);

// ./test/core/gc/ref_test.wast:130
assert_return(() => invoke($0, `ref_test_i31`, [0]), [value("i32", 1)]);

// ./test/core/gc/ref_test.wast:131
assert_return(() => invoke($0, `ref_test_i31`, [1]), [value("i32", 1)]);

// ./test/core/gc/ref_test.wast:132
assert_return(() => invoke($0, `ref_test_i31`, [2]), [value("i32", 1)]);

// ./test/core/gc/ref_test.wast:133
assert_return(() => invoke($0, `ref_test_i31`, [3]), [value("i32", 2)]);

// ./test/core/gc/ref_test.wast:134
assert_return(() => invoke($0, `ref_test_i31`, [4]), [value("i32", 0)]);

// ./test/core/gc/ref_test.wast:135
assert_return(() => invoke($0, `ref_test_i31`, [5]), [value("i32", 0)]);

// ./test/core/gc/ref_test.wast:136
assert_return(() => invoke($0, `ref_test_i31`, [6]), [value("i32", 0)]);

// ./test/core/gc/ref_test.wast:137
assert_return(() => invoke($0, `ref_test_i31`, [7]), [value("i32", 1)]);

// ./test/core/gc/ref_test.wast:139
assert_return(() => invoke($0, `ref_test_struct`, [0]), [value("i32", 1)]);

// ./test/core/gc/ref_test.wast:140
assert_return(() => invoke($0, `ref_test_struct`, [1]), [value("i32", 1)]);

// ./test/core/gc/ref_test.wast:141
assert_return(() => invoke($0, `ref_test_struct`, [2]), [value("i32", 1)]);

// ./test/core/gc/ref_test.wast:142
assert_return(() => invoke($0, `ref_test_struct`, [3]), [value("i32", 0)]);

// ./test/core/gc/ref_test.wast:143
assert_return(() => invoke($0, `ref_test_struct`, [4]), [value("i32", 2)]);

// ./test/core/gc/ref_test.wast:144
assert_return(() => invoke($0, `ref_test_struct`, [5]), [value("i32", 0)]);

// ./test/core/gc/ref_test.wast:145
assert_return(() => invoke($0, `ref_test_struct`, [6]), [value("i32", 0)]);

// ./test/core/gc/ref_test.wast:146
assert_return(() => invoke($0, `ref_test_struct`, [7]), [value("i32", 1)]);

// ./test/core/gc/ref_test.wast:148
assert_return(() => invoke($0, `ref_test_array`, [0]), [value("i32", 1)]);

// ./test/core/gc/ref_test.wast:149
assert_return(() => invoke($0, `ref_test_array`, [1]), [value("i32", 1)]);

// ./test/core/gc/ref_test.wast:150
assert_return(() => invoke($0, `ref_test_array`, [2]), [value("i32", 1)]);

// ./test/core/gc/ref_test.wast:151
assert_return(() => invoke($0, `ref_test_array`, [3]), [value("i32", 0)]);

// ./test/core/gc/ref_test.wast:152
assert_return(() => invoke($0, `ref_test_array`, [4]), [value("i32", 0)]);

// ./test/core/gc/ref_test.wast:153
assert_return(() => invoke($0, `ref_test_array`, [5]), [value("i32", 2)]);

// ./test/core/gc/ref_test.wast:154
assert_return(() => invoke($0, `ref_test_array`, [6]), [value("i32", 0)]);

// ./test/core/gc/ref_test.wast:155
assert_return(() => invoke($0, `ref_test_array`, [7]), [value("i32", 1)]);

// ./test/core/gc/ref_test.wast:157
assert_return(() => invoke($0, `ref_test_null_func`, [0]), [value("i32", 2)]);

// ./test/core/gc/ref_test.wast:158
assert_return(() => invoke($0, `ref_test_null_func`, [1]), [value("i32", 2)]);

// ./test/core/gc/ref_test.wast:159
assert_return(() => invoke($0, `ref_test_null_func`, [2]), [value("i32", 0)]);

// ./test/core/gc/ref_test.wast:161
assert_return(() => invoke($0, `ref_test_func`, [0]), [value("i32", 1)]);

// ./test/core/gc/ref_test.wast:162
assert_return(() => invoke($0, `ref_test_func`, [1]), [value("i32", 1)]);

// ./test/core/gc/ref_test.wast:163
assert_return(() => invoke($0, `ref_test_func`, [2]), [value("i32", 2)]);

// ./test/core/gc/ref_test.wast:165
assert_return(() => invoke($0, `ref_test_null_extern`, [0]), [value("i32", 2)]);

// ./test/core/gc/ref_test.wast:166
assert_return(() => invoke($0, `ref_test_null_extern`, [1]), [value("i32", 2)]);

// ./test/core/gc/ref_test.wast:167
assert_return(() => invoke($0, `ref_test_null_extern`, [2]), [value("i32", 0)]);

// ./test/core/gc/ref_test.wast:168
assert_return(() => invoke($0, `ref_test_null_extern`, [3]), [value("i32", 0)]);

// ./test/core/gc/ref_test.wast:169
assert_return(() => invoke($0, `ref_test_null_extern`, [4]), [value("i32", 0)]);

// ./test/core/gc/ref_test.wast:170
assert_return(() => invoke($0, `ref_test_null_extern`, [5]), [value("i32", 2)]);

// ./test/core/gc/ref_test.wast:172
assert_return(() => invoke($0, `ref_test_extern`, [0]), [value("i32", 1)]);

// ./test/core/gc/ref_test.wast:173
assert_return(() => invoke($0, `ref_test_extern`, [1]), [value("i32", 1)]);

// ./test/core/gc/ref_test.wast:174
assert_return(() => invoke($0, `ref_test_extern`, [2]), [value("i32", 2)]);

// ./test/core/gc/ref_test.wast:175
assert_return(() => invoke($0, `ref_test_extern`, [3]), [value("i32", 2)]);

// ./test/core/gc/ref_test.wast:176
assert_return(() => invoke($0, `ref_test_extern`, [4]), [value("i32", 2)]);

// ./test/core/gc/ref_test.wast:177
assert_return(() => invoke($0, `ref_test_extern`, [5]), [value("i32", 1)]);

// ./test/core/gc/ref_test.wast:182
let $1 = instantiate(`(module
  (type $$t0 (sub (struct)))
  (type $$t1 (sub $$t0 (struct (field i32))))
  (type $$t1' (sub $$t0 (struct (field i32))))
  (type $$t2 (sub $$t1 (struct (field i32 i32))))
  (type $$t2' (sub $$t1' (struct (field i32 i32))))
  (type $$t3 (sub $$t0 (struct (field i32 i32))))
  (type $$t0' (sub $$t0 (struct)))
  (type $$t4 (sub $$t0' (struct (field i32 i32))))

  (table 20 (ref null struct))

  (func $$init
    (table.set (i32.const 0) (struct.new_default $$t0))
    (table.set (i32.const 10) (struct.new_default $$t0))
    (table.set (i32.const 1) (struct.new_default $$t1))
    (table.set (i32.const 11) (struct.new_default $$t1'))
    (table.set (i32.const 2) (struct.new_default $$t2))
    (table.set (i32.const 12) (struct.new_default $$t2'))
    (table.set (i32.const 3) (struct.new_default $$t3))
    (table.set (i32.const 4) (struct.new_default $$t4))
  )

  (func (export "test-sub")
    (call $$init)
    (block $$l
      ;; must hold
      (br_if $$l (i32.eqz (ref.test (ref null $$t0) (ref.null struct))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t0) (ref.null $$t0))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t0) (ref.null $$t1))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t0) (ref.null $$t2))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t0) (ref.null $$t3))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t0) (ref.null $$t4))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t0) (table.get (i32.const 0)))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t0) (table.get (i32.const 1)))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t0) (table.get (i32.const 2)))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t0) (table.get (i32.const 3)))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t0) (table.get (i32.const 4)))))

      (br_if $$l (i32.eqz (ref.test (ref null $$t1) (ref.null struct))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t1) (ref.null $$t0))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t1) (ref.null $$t1))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t1) (ref.null $$t2))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t1) (ref.null $$t3))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t1) (ref.null $$t4))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t1) (table.get (i32.const 1)))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t1) (table.get (i32.const 2)))))

      (br_if $$l (i32.eqz (ref.test (ref null $$t2) (ref.null struct))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t2) (ref.null $$t0))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t2) (ref.null $$t1))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t2) (ref.null $$t2))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t2) (ref.null $$t3))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t2) (ref.null $$t4))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t2) (table.get (i32.const 2)))))

      (br_if $$l (i32.eqz (ref.test (ref null $$t3) (ref.null struct))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t3) (ref.null $$t0))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t3) (ref.null $$t1))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t3) (ref.null $$t2))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t3) (ref.null $$t3))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t3) (ref.null $$t4))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t3) (table.get (i32.const 3)))))

      (br_if $$l (i32.eqz (ref.test (ref null $$t4) (ref.null struct))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t4) (ref.null $$t0))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t4) (ref.null $$t1))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t4) (ref.null $$t2))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t4) (ref.null $$t3))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t4) (ref.null $$t4))))
      (br_if $$l (i32.eqz (ref.test (ref null $$t4) (table.get (i32.const 4)))))

      (br_if $$l (i32.eqz (ref.test (ref $$t0) (table.get (i32.const 0)))))
      (br_if $$l (i32.eqz (ref.test (ref $$t0) (table.get (i32.const 1)))))
      (br_if $$l (i32.eqz (ref.test (ref $$t0) (table.get (i32.const 2)))))
      (br_if $$l (i32.eqz (ref.test (ref $$t0) (table.get (i32.const 3)))))
      (br_if $$l (i32.eqz (ref.test (ref $$t0) (table.get (i32.const 4)))))

      (br_if $$l (i32.eqz (ref.test (ref $$t1) (table.get (i32.const 1)))))
      (br_if $$l (i32.eqz (ref.test (ref $$t1) (table.get (i32.const 2)))))

      (br_if $$l (i32.eqz (ref.test (ref $$t2) (table.get (i32.const 2)))))

      (br_if $$l (i32.eqz (ref.test (ref $$t3) (table.get (i32.const 3)))))

      (br_if $$l (i32.eqz (ref.test (ref $$t4) (table.get (i32.const 4)))))

      ;; must not hold
      (br_if $$l (ref.test (ref $$t0) (ref.null struct)))
      (br_if $$l (ref.test (ref $$t1) (ref.null struct)))
      (br_if $$l (ref.test (ref $$t2) (ref.null struct)))
      (br_if $$l (ref.test (ref $$t3) (ref.null struct)))
      (br_if $$l (ref.test (ref $$t4) (ref.null struct)))

      (br_if $$l (ref.test (ref $$t1) (table.get (i32.const 0))))
      (br_if $$l (ref.test (ref $$t1) (table.get (i32.const 3))))
      (br_if $$l (ref.test (ref $$t1) (table.get (i32.const 4))))

      (br_if $$l (ref.test (ref $$t2) (table.get (i32.const 0))))
      (br_if $$l (ref.test (ref $$t2) (table.get (i32.const 1))))
      (br_if $$l (ref.test (ref $$t2) (table.get (i32.const 3))))
      (br_if $$l (ref.test (ref $$t2) (table.get (i32.const 4))))

      (br_if $$l (ref.test (ref $$t3) (table.get (i32.const 0))))
      (br_if $$l (ref.test (ref $$t3) (table.get (i32.const 1))))
      (br_if $$l (ref.test (ref $$t3) (table.get (i32.const 2))))
      (br_if $$l (ref.test (ref $$t3) (table.get (i32.const 4))))

      (br_if $$l (ref.test (ref $$t4) (table.get (i32.const 0))))
      (br_if $$l (ref.test (ref $$t4) (table.get (i32.const 1))))
      (br_if $$l (ref.test (ref $$t4) (table.get (i32.const 2))))
      (br_if $$l (ref.test (ref $$t4) (table.get (i32.const 3))))

      (return)
    )
    (unreachable)
  )

  (func (export "test-canon")
    (call $$init)
    (block $$l
      (br_if $$l (i32.eqz (ref.test (ref $$t0) (table.get (i32.const 0)))))
      (br_if $$l (i32.eqz (ref.test (ref $$t0) (table.get (i32.const 1)))))
      (br_if $$l (i32.eqz (ref.test (ref $$t0) (table.get (i32.const 2)))))
      (br_if $$l (i32.eqz (ref.test (ref $$t0) (table.get (i32.const 3)))))
      (br_if $$l (i32.eqz (ref.test (ref $$t0) (table.get (i32.const 4)))))

      (br_if $$l (i32.eqz (ref.test (ref $$t0) (table.get (i32.const 10)))))
      (br_if $$l (i32.eqz (ref.test (ref $$t0) (table.get (i32.const 11)))))
      (br_if $$l (i32.eqz (ref.test (ref $$t0) (table.get (i32.const 12)))))

      (br_if $$l (i32.eqz (ref.test (ref $$t1') (table.get (i32.const 1)))))
      (br_if $$l (i32.eqz (ref.test (ref $$t1') (table.get (i32.const 2)))))

      (br_if $$l (i32.eqz (ref.test (ref $$t1) (table.get (i32.const 11)))))
      (br_if $$l (i32.eqz (ref.test (ref $$t1) (table.get (i32.const 12)))))

      (br_if $$l (i32.eqz (ref.test (ref $$t2') (table.get (i32.const 2)))))

      (br_if $$l (i32.eqz (ref.test (ref $$t2) (table.get (i32.const 12)))))

      (return)
    )
    (unreachable)
  )
)`);

// ./test/core/gc/ref_test.wast:329
invoke($1, `test-sub`, []);

// ./test/core/gc/ref_test.wast:330
invoke($1, `test-canon`, []);
