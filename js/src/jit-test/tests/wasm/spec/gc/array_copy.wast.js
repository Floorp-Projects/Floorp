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

// ./test/core/gc/array_copy.wast

// ./test/core/gc/array_copy.wast:5
assert_invalid(
  () => instantiate(`(module
    (type $$a (array i8))
    (type $$b (array (mut i8)))

    (func (export "array.copy-immutable") (param $$1 (ref $$a)) (param $$2 (ref $$b))
      (array.copy $$a $$b (local.get $$1) (i32.const 0) (local.get $$2) (i32.const 0) (i32.const 0))
    )
  )`),
  `array is immutable`,
);

// ./test/core/gc/array_copy.wast:17
assert_invalid(
  () => instantiate(`(module
    (type $$a (array (mut i8)))
    (type $$b (array i16))

    (func (export "array.copy-packed-invalid") (param $$1 (ref $$a)) (param $$2 (ref $$b))
      (array.copy $$a $$b (local.get $$1) (i32.const 0) (local.get $$2) (i32.const 0) (i32.const 0))
    )
  )`),
  `array types do not match`,
);

// ./test/core/gc/array_copy.wast:29
assert_invalid(
  () => instantiate(`(module
    (type $$a (array (mut i8)))
    (type $$b (array (mut (ref $$a))))

    (func (export "array.copy-ref-invalid-1") (param $$1 (ref $$a)) (param $$2 (ref $$b))
      (array.copy $$a $$b (local.get $$1) (i32.const 0) (local.get $$2) (i32.const 0) (i32.const 0))
    )
  )`),
  `array types do not match`,
);

// ./test/core/gc/array_copy.wast:41
assert_invalid(
  () => instantiate(`(module
    (type $$a (array (mut i8)))
    (type $$b (array (mut (ref $$a))))
    (type $$c (array (mut (ref $$b))))

    (func (export "array.copy-ref-invalid-1") (param $$1 (ref $$b)) (param $$2 (ref $$c))
      (array.copy $$b $$c (local.get $$1) (i32.const 0) (local.get $$2) (i32.const 0) (i32.const 0))
    )
  )`),
  `array types do not match`,
);

// ./test/core/gc/array_copy.wast:54
let $0 = instantiate(`(module
  (type $$arr8 (array i8))
  (type $$arr8_mut (array (mut i8)))

  (global $$g_arr8 (ref $$arr8) (array.new $$arr8 (i32.const 10) (i32.const 12)))
  (global $$g_arr8_mut (mut (ref $$arr8_mut)) (array.new_default $$arr8_mut (i32.const 12)))

  (data $$d1 "abcdefghijkl")

  (func (export "array_get_nth") (param $$1 i32) (result i32)
    (array.get_u $$arr8_mut (global.get $$g_arr8_mut) (local.get $$1))
  )

  (func (export "array_copy-null-left")
    (array.copy $$arr8_mut $$arr8 (ref.null $$arr8_mut) (i32.const 0) (global.get $$g_arr8) (i32.const 0) (i32.const 0))
  )

  (func (export "array_copy-null-right")
    (array.copy $$arr8_mut $$arr8 (global.get $$g_arr8_mut) (i32.const 0) (ref.null $$arr8) (i32.const 0) (i32.const 0))
  )

  (func (export "array_copy") (param $$1 i32) (param $$2 i32) (param $$3 i32)
    (array.copy $$arr8_mut $$arr8 (global.get $$g_arr8_mut) (local.get $$1) (global.get $$g_arr8) (local.get $$2) (local.get $$3))
  )

  (func (export "array_copy_overlap_test-1")
    (local $$1 (ref $$arr8_mut))
    (array.new_data $$arr8_mut $$d1 (i32.const 0) (i32.const 12))
    (local.set $$1)
    (array.copy $$arr8_mut $$arr8_mut (local.get $$1) (i32.const 1) (local.get $$1) (i32.const 0) (i32.const 11))
    (global.set $$g_arr8_mut (local.get $$1))
  )

  (func (export "array_copy_overlap_test-2")
    (local $$1 (ref $$arr8_mut))
    (array.new_data $$arr8_mut $$d1 (i32.const 0) (i32.const 12))
    (local.set $$1)
    (array.copy $$arr8_mut $$arr8_mut (local.get $$1) (i32.const 0) (local.get $$1) (i32.const 1) (i32.const 11))
    (global.set $$g_arr8_mut (local.get $$1))
  )
)`);

// ./test/core/gc/array_copy.wast:97
assert_trap(() => invoke($0, `array_copy-null-left`, []), `null array reference`);

// ./test/core/gc/array_copy.wast:98
assert_trap(() => invoke($0, `array_copy-null-right`, []), `null array reference`);

// ./test/core/gc/array_copy.wast:101
assert_trap(() => invoke($0, `array_copy`, [13, 0, 0]), `out of bounds array access`);

// ./test/core/gc/array_copy.wast:102
assert_trap(() => invoke($0, `array_copy`, [0, 13, 0]), `out of bounds array access`);

// ./test/core/gc/array_copy.wast:105
assert_trap(() => invoke($0, `array_copy`, [0, 0, 13]), `out of bounds array access`);

// ./test/core/gc/array_copy.wast:106
assert_trap(() => invoke($0, `array_copy`, [0, 0, 13]), `out of bounds array access`);

// ./test/core/gc/array_copy.wast:109
assert_return(() => invoke($0, `array_copy`, [12, 0, 0]), []);

// ./test/core/gc/array_copy.wast:110
assert_return(() => invoke($0, `array_copy`, [0, 12, 0]), []);

// ./test/core/gc/array_copy.wast:113
assert_return(() => invoke($0, `array_get_nth`, [0]), [value("i32", 0)]);

// ./test/core/gc/array_copy.wast:114
assert_return(() => invoke($0, `array_get_nth`, [5]), [value("i32", 0)]);

// ./test/core/gc/array_copy.wast:115
assert_return(() => invoke($0, `array_get_nth`, [11]), [value("i32", 0)]);

// ./test/core/gc/array_copy.wast:116
assert_trap(() => invoke($0, `array_get_nth`, [12]), `out of bounds array access`);

// ./test/core/gc/array_copy.wast:119
assert_return(() => invoke($0, `array_copy`, [0, 0, 2]), []);

// ./test/core/gc/array_copy.wast:120
assert_return(() => invoke($0, `array_get_nth`, [0]), [value("i32", 10)]);

// ./test/core/gc/array_copy.wast:121
assert_return(() => invoke($0, `array_get_nth`, [1]), [value("i32", 10)]);

// ./test/core/gc/array_copy.wast:122
assert_return(() => invoke($0, `array_get_nth`, [2]), [value("i32", 0)]);

// ./test/core/gc/array_copy.wast:125
assert_return(() => invoke($0, `array_copy_overlap_test-1`, []), []);

// ./test/core/gc/array_copy.wast:126
assert_return(() => invoke($0, `array_get_nth`, [0]), [value("i32", 97)]);

// ./test/core/gc/array_copy.wast:127
assert_return(() => invoke($0, `array_get_nth`, [1]), [value("i32", 97)]);

// ./test/core/gc/array_copy.wast:128
assert_return(() => invoke($0, `array_get_nth`, [2]), [value("i32", 98)]);

// ./test/core/gc/array_copy.wast:129
assert_return(() => invoke($0, `array_get_nth`, [5]), [value("i32", 101)]);

// ./test/core/gc/array_copy.wast:130
assert_return(() => invoke($0, `array_get_nth`, [10]), [value("i32", 106)]);

// ./test/core/gc/array_copy.wast:131
assert_return(() => invoke($0, `array_get_nth`, [11]), [value("i32", 107)]);

// ./test/core/gc/array_copy.wast:133
assert_return(() => invoke($0, `array_copy_overlap_test-2`, []), []);

// ./test/core/gc/array_copy.wast:134
assert_return(() => invoke($0, `array_get_nth`, [0]), [value("i32", 98)]);

// ./test/core/gc/array_copy.wast:135
assert_return(() => invoke($0, `array_get_nth`, [1]), [value("i32", 99)]);

// ./test/core/gc/array_copy.wast:136
assert_return(() => invoke($0, `array_get_nth`, [5]), [value("i32", 103)]);

// ./test/core/gc/array_copy.wast:137
assert_return(() => invoke($0, `array_get_nth`, [9]), [value("i32", 107)]);

// ./test/core/gc/array_copy.wast:138
assert_return(() => invoke($0, `array_get_nth`, [10]), [value("i32", 108)]);

// ./test/core/gc/array_copy.wast:139
assert_return(() => invoke($0, `array_get_nth`, [11]), [value("i32", 108)]);
