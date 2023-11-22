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

// ./test/core/gc/ref_cast.wast

// ./test/core/gc/ref_cast.wast:3
let $0 = instantiate(`(module
  (type $$ft (func))
  (type $$st (struct))
  (type $$at (array i8))

  (table 10 anyref)

  (elem declare func $$f)
  (func $$f)

  (func (export "init") (param $$x externref)
    (table.set (i32.const 0) (ref.null any))
    (table.set (i32.const 1) (ref.i31 (i32.const 7)))
    (table.set (i32.const 2) (struct.new_default $$st))
    (table.set (i32.const 3) (array.new_default $$at (i32.const 0)))
    (table.set (i32.const 4) (any.convert_extern (local.get $$x)))
    (table.set (i32.const 5) (ref.null i31))
    (table.set (i32.const 6) (ref.null struct))
    (table.set (i32.const 7) (ref.null none))
  )

  (func (export "ref_cast_non_null") (param $$i i32)
    (drop (ref.as_non_null (table.get (local.get $$i))))
    (drop (ref.cast (ref null any) (table.get (local.get $$i))))
  )
  (func (export "ref_cast_null") (param $$i i32)
    (drop (ref.cast anyref (table.get (local.get $$i))))
    (drop (ref.cast structref (table.get (local.get $$i))))
    (drop (ref.cast arrayref (table.get (local.get $$i))))
    (drop (ref.cast i31ref (table.get (local.get $$i))))
    (drop (ref.cast nullref (table.get (local.get $$i))))
  )
  (func (export "ref_cast_i31") (param $$i i32)
    (drop (ref.cast (ref i31) (table.get (local.get $$i))))
    (drop (ref.cast i31ref (table.get (local.get $$i))))
  )
  (func (export "ref_cast_struct") (param $$i i32)
    (drop (ref.cast (ref struct) (table.get (local.get $$i))))
    (drop (ref.cast structref (table.get (local.get $$i))))
  )
  (func (export "ref_cast_array") (param $$i i32)
    (drop (ref.cast (ref array) (table.get (local.get $$i))))
    (drop (ref.cast arrayref (table.get (local.get $$i))))
  )
)`);

// ./test/core/gc/ref_cast.wast:49
invoke($0, `init`, [externref(0)]);

// ./test/core/gc/ref_cast.wast:51
assert_trap(() => invoke($0, `ref_cast_non_null`, [0]), `null reference`);

// ./test/core/gc/ref_cast.wast:52
assert_return(() => invoke($0, `ref_cast_non_null`, [1]), []);

// ./test/core/gc/ref_cast.wast:53
assert_return(() => invoke($0, `ref_cast_non_null`, [2]), []);

// ./test/core/gc/ref_cast.wast:54
assert_return(() => invoke($0, `ref_cast_non_null`, [3]), []);

// ./test/core/gc/ref_cast.wast:55
assert_return(() => invoke($0, `ref_cast_non_null`, [4]), []);

// ./test/core/gc/ref_cast.wast:56
assert_trap(() => invoke($0, `ref_cast_non_null`, [5]), `null reference`);

// ./test/core/gc/ref_cast.wast:57
assert_trap(() => invoke($0, `ref_cast_non_null`, [6]), `null reference`);

// ./test/core/gc/ref_cast.wast:58
assert_trap(() => invoke($0, `ref_cast_non_null`, [7]), `null reference`);

// ./test/core/gc/ref_cast.wast:60
assert_return(() => invoke($0, `ref_cast_null`, [0]), []);

// ./test/core/gc/ref_cast.wast:61
assert_trap(() => invoke($0, `ref_cast_null`, [1]), `cast failure`);

// ./test/core/gc/ref_cast.wast:62
assert_trap(() => invoke($0, `ref_cast_null`, [2]), `cast failure`);

// ./test/core/gc/ref_cast.wast:63
assert_trap(() => invoke($0, `ref_cast_null`, [3]), `cast failure`);

// ./test/core/gc/ref_cast.wast:64
assert_trap(() => invoke($0, `ref_cast_null`, [4]), `cast failure`);

// ./test/core/gc/ref_cast.wast:65
assert_return(() => invoke($0, `ref_cast_null`, [5]), []);

// ./test/core/gc/ref_cast.wast:66
assert_return(() => invoke($0, `ref_cast_null`, [6]), []);

// ./test/core/gc/ref_cast.wast:67
assert_return(() => invoke($0, `ref_cast_null`, [7]), []);

// ./test/core/gc/ref_cast.wast:69
assert_trap(() => invoke($0, `ref_cast_i31`, [0]), `cast failure`);

// ./test/core/gc/ref_cast.wast:70
assert_return(() => invoke($0, `ref_cast_i31`, [1]), []);

// ./test/core/gc/ref_cast.wast:71
assert_trap(() => invoke($0, `ref_cast_i31`, [2]), `cast failure`);

// ./test/core/gc/ref_cast.wast:72
assert_trap(() => invoke($0, `ref_cast_i31`, [3]), `cast failure`);

// ./test/core/gc/ref_cast.wast:73
assert_trap(() => invoke($0, `ref_cast_i31`, [4]), `cast failure`);

// ./test/core/gc/ref_cast.wast:74
assert_trap(() => invoke($0, `ref_cast_i31`, [5]), `cast failure`);

// ./test/core/gc/ref_cast.wast:75
assert_trap(() => invoke($0, `ref_cast_i31`, [6]), `cast failure`);

// ./test/core/gc/ref_cast.wast:76
assert_trap(() => invoke($0, `ref_cast_i31`, [7]), `cast failure`);

// ./test/core/gc/ref_cast.wast:78
assert_trap(() => invoke($0, `ref_cast_struct`, [0]), `cast failure`);

// ./test/core/gc/ref_cast.wast:79
assert_trap(() => invoke($0, `ref_cast_struct`, [1]), `cast failure`);

// ./test/core/gc/ref_cast.wast:80
assert_return(() => invoke($0, `ref_cast_struct`, [2]), []);

// ./test/core/gc/ref_cast.wast:81
assert_trap(() => invoke($0, `ref_cast_struct`, [3]), `cast failure`);

// ./test/core/gc/ref_cast.wast:82
assert_trap(() => invoke($0, `ref_cast_struct`, [4]), `cast failure`);

// ./test/core/gc/ref_cast.wast:83
assert_trap(() => invoke($0, `ref_cast_struct`, [5]), `cast failure`);

// ./test/core/gc/ref_cast.wast:84
assert_trap(() => invoke($0, `ref_cast_struct`, [6]), `cast failure`);

// ./test/core/gc/ref_cast.wast:85
assert_trap(() => invoke($0, `ref_cast_struct`, [7]), `cast failure`);

// ./test/core/gc/ref_cast.wast:87
assert_trap(() => invoke($0, `ref_cast_array`, [0]), `cast failure`);

// ./test/core/gc/ref_cast.wast:88
assert_trap(() => invoke($0, `ref_cast_array`, [1]), `cast failure`);

// ./test/core/gc/ref_cast.wast:89
assert_trap(() => invoke($0, `ref_cast_array`, [2]), `cast failure`);

// ./test/core/gc/ref_cast.wast:90
assert_return(() => invoke($0, `ref_cast_array`, [3]), []);

// ./test/core/gc/ref_cast.wast:91
assert_trap(() => invoke($0, `ref_cast_array`, [4]), `cast failure`);

// ./test/core/gc/ref_cast.wast:92
assert_trap(() => invoke($0, `ref_cast_array`, [5]), `cast failure`);

// ./test/core/gc/ref_cast.wast:93
assert_trap(() => invoke($0, `ref_cast_array`, [6]), `cast failure`);

// ./test/core/gc/ref_cast.wast:94
assert_trap(() => invoke($0, `ref_cast_array`, [7]), `cast failure`);

// ./test/core/gc/ref_cast.wast:99
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

    (drop (ref.cast (ref null $$t0) (ref.null struct)))
    (drop (ref.cast (ref null $$t0) (table.get (i32.const 0))))
    (drop (ref.cast (ref null $$t0) (table.get (i32.const 1))))
    (drop (ref.cast (ref null $$t0) (table.get (i32.const 2))))
    (drop (ref.cast (ref null $$t0) (table.get (i32.const 3))))
    (drop (ref.cast (ref null $$t0) (table.get (i32.const 4))))

    (drop (ref.cast (ref null $$t0) (ref.null struct)))
    (drop (ref.cast (ref null $$t1) (table.get (i32.const 1))))
    (drop (ref.cast (ref null $$t1) (table.get (i32.const 2))))

    (drop (ref.cast (ref null $$t0) (ref.null struct)))
    (drop (ref.cast (ref null $$t2) (table.get (i32.const 2))))

    (drop (ref.cast (ref null $$t0) (ref.null struct)))
    (drop (ref.cast (ref null $$t3) (table.get (i32.const 3))))

    (drop (ref.cast (ref null $$t4) (table.get (i32.const 4))))

    (drop (ref.cast (ref $$t0) (table.get (i32.const 0))))
    (drop (ref.cast (ref $$t0) (table.get (i32.const 1))))
    (drop (ref.cast (ref $$t0) (table.get (i32.const 2))))
    (drop (ref.cast (ref $$t0) (table.get (i32.const 3))))
    (drop (ref.cast (ref $$t0) (table.get (i32.const 4))))

    (drop (ref.cast (ref $$t1) (table.get (i32.const 1))))
    (drop (ref.cast (ref $$t1) (table.get (i32.const 2))))

    (drop (ref.cast (ref $$t2) (table.get (i32.const 2))))

    (drop (ref.cast (ref $$t3) (table.get (i32.const 3))))

    (drop (ref.cast (ref $$t4) (table.get (i32.const 4))))
  )

  (func (export "test-canon")
    (call $$init)

    (drop (ref.cast (ref $$t0) (table.get (i32.const 0))))
    (drop (ref.cast (ref $$t0) (table.get (i32.const 1))))
    (drop (ref.cast (ref $$t0) (table.get (i32.const 2))))
    (drop (ref.cast (ref $$t0) (table.get (i32.const 3))))
    (drop (ref.cast (ref $$t0) (table.get (i32.const 4))))

    (drop (ref.cast (ref $$t0) (table.get (i32.const 10))))
    (drop (ref.cast (ref $$t0) (table.get (i32.const 11))))
    (drop (ref.cast (ref $$t0) (table.get (i32.const 12))))

    (drop (ref.cast (ref $$t1') (table.get (i32.const 1))))
    (drop (ref.cast (ref $$t1') (table.get (i32.const 2))))

    (drop (ref.cast (ref $$t1) (table.get (i32.const 11))))
    (drop (ref.cast (ref $$t1) (table.get (i32.const 12))))

    (drop (ref.cast (ref $$t2') (table.get (i32.const 2))))

    (drop (ref.cast (ref $$t2) (table.get (i32.const 12))))
  )
)`);

// ./test/core/gc/ref_cast.wast:185
invoke($1, `test-sub`, []);

// ./test/core/gc/ref_cast.wast:186
invoke($1, `test-canon`, []);
