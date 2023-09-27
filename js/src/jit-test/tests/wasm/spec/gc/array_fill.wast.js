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

// ./test/core/gc/array_fill.wast

// ./test/core/gc/array_fill.wast:5
assert_invalid(
  () => instantiate(`(module
    (type $$a (array i8))

    (func (export "array.fill-immutable") (param $$1 (ref $$a)) (param $$2 i32)
      (array.fill $$a (local.get $$1) (i32.const 0) (local.get $$2) (i32.const 0))
    )
  )`),
  `array is immutable`,
);

// ./test/core/gc/array_fill.wast:16
assert_invalid(
  () => instantiate(`(module
    (type $$a (array (mut i8)))

    (func (export "array.fill-invalid-1") (param $$1 (ref $$a)) (param $$2 funcref)
      (array.fill $$a (local.get $$1) (i32.const 0) (local.get $$2) (i32.const 0))
    )
  )`),
  `type mismatch`,
);

// ./test/core/gc/array_fill.wast:27
assert_invalid(
  () => instantiate(`(module
    (type $$b (array (mut funcref)))

    (func (export "array.fill-invalid-1") (param $$1 (ref $$b)) (param $$2 i32)
      (array.fill $$b (local.get $$1) (i32.const 0) (local.get $$2) (i32.const 0))
    )
  )`),
  `type mismatch`,
);

// ./test/core/gc/array_fill.wast:38
let $0 = instantiate(`(module
  (type $$arr8 (array i8))
  (type $$arr8_mut (array (mut i8)))

  (global $$g_arr8 (ref $$arr8) (array.new $$arr8 (i32.const 10) (i32.const 12)))
  (global $$g_arr8_mut (mut (ref $$arr8_mut)) (array.new_default $$arr8_mut (i32.const 12)))

  (func (export "array_get_nth") (param $$1 i32) (result i32)
    (array.get_u $$arr8_mut (global.get $$g_arr8_mut) (local.get $$1))
  )

  (func (export "array_fill-null")
    (array.fill $$arr8_mut (ref.null $$arr8_mut) (i32.const 0) (i32.const 0) (i32.const 0))
  )

  (func (export "array_fill") (param $$1 i32) (param $$2 i32) (param $$3 i32)
    (array.fill $$arr8_mut (global.get $$g_arr8_mut) (local.get $$1) (local.get $$2) (local.get $$3))
  )
)`);

// ./test/core/gc/array_fill.wast:59
assert_trap(() => invoke($0, `array_fill-null`, []), `null array reference`);

// ./test/core/gc/array_fill.wast:62
assert_trap(() => invoke($0, `array_fill`, [13, 0, 0]), `out of bounds array access`);

// ./test/core/gc/array_fill.wast:65
assert_trap(() => invoke($0, `array_fill`, [0, 0, 13]), `out of bounds array access`);

// ./test/core/gc/array_fill.wast:68
assert_return(() => invoke($0, `array_fill`, [12, 0, 0]), []);

// ./test/core/gc/array_fill.wast:71
assert_return(() => invoke($0, `array_get_nth`, [0]), [value("i32", 0)]);

// ./test/core/gc/array_fill.wast:72
assert_return(() => invoke($0, `array_get_nth`, [5]), [value("i32", 0)]);

// ./test/core/gc/array_fill.wast:73
assert_return(() => invoke($0, `array_get_nth`, [11]), [value("i32", 0)]);

// ./test/core/gc/array_fill.wast:74
assert_trap(() => invoke($0, `array_get_nth`, [12]), `out of bounds array access`);

// ./test/core/gc/array_fill.wast:77
assert_return(() => invoke($0, `array_fill`, [2, 11, 2]), []);

// ./test/core/gc/array_fill.wast:78
assert_return(() => invoke($0, `array_get_nth`, [1]), [value("i32", 0)]);

// ./test/core/gc/array_fill.wast:79
assert_return(() => invoke($0, `array_get_nth`, [2]), [value("i32", 11)]);

// ./test/core/gc/array_fill.wast:80
assert_return(() => invoke($0, `array_get_nth`, [3]), [value("i32", 11)]);

// ./test/core/gc/array_fill.wast:81
assert_return(() => invoke($0, `array_get_nth`, [4]), [value("i32", 0)]);
