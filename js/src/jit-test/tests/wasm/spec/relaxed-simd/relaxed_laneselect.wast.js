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

// ./test/core/relaxed-simd/relaxed_laneselect.wast

// ./test/core/relaxed-simd/relaxed_laneselect.wast:4
let $0 = instantiate(`(module
    (func (export "i8x16.relaxed_laneselect") (param v128 v128 v128) (result v128) (i8x16.relaxed_laneselect (local.get 0) (local.get 1) (local.get 2)))
    (func (export "i16x8.relaxed_laneselect") (param v128 v128 v128) (result v128) (i16x8.relaxed_laneselect (local.get 0) (local.get 1) (local.get 2)))
    (func (export "i32x4.relaxed_laneselect") (param v128 v128 v128) (result v128) (i32x4.relaxed_laneselect (local.get 0) (local.get 1) (local.get 2)))
    (func (export "i64x2.relaxed_laneselect") (param v128 v128 v128) (result v128) (i64x2.relaxed_laneselect (local.get 0) (local.get 1) (local.get 2)))

    (func (export "i8x16.relaxed_laneselect_cmp") (param v128 v128 v128) (result v128)
          (i8x16.eq
            (i8x16.relaxed_laneselect (local.get 0) (local.get 1) (local.get 2))
            (i8x16.relaxed_laneselect (local.get 0) (local.get 1) (local.get 2))))
    (func (export "i16x8.relaxed_laneselect_cmp") (param v128 v128 v128) (result v128)
          (i16x8.eq
            (i16x8.relaxed_laneselect (local.get 0) (local.get 1) (local.get 2))
            (i16x8.relaxed_laneselect (local.get 0) (local.get 1) (local.get 2))))
    (func (export "i32x4.relaxed_laneselect_cmp") (param v128 v128 v128) (result v128)
          (i32x4.eq
            (i32x4.relaxed_laneselect (local.get 0) (local.get 1) (local.get 2))
            (i32x4.relaxed_laneselect (local.get 0) (local.get 1) (local.get 2))))
    (func (export "i64x2.relaxed_laneselect_cmp") (param v128 v128 v128) (result v128)
          (i64x2.eq
            (i64x2.relaxed_laneselect (local.get 0) (local.get 1) (local.get 2))
            (i64x2.relaxed_laneselect (local.get 0) (local.get 1) (local.get 2))))
)`);

// ./test/core/relaxed-simd/relaxed_laneselect.wast:28
assert_return(
  () => invoke($0, `i8x16.relaxed_laneselect`, [
    i8x16([0x0, 0x1, 0x12, 0x12, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf]),
    i8x16([0x10, 0x11, 0x34, 0x34, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f]),
    i8x16([0xff, 0x0, 0xf0, 0xf, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
  ]),
  [
    either(
      i8x16([0x0, 0x11, 0x14, 0x32, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f]),
      i8x16([0x0, 0x11, 0x12, 0x34, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f]),
    ),
  ],
);

// ./test/core/relaxed-simd/relaxed_laneselect.wast:35
assert_return(
  () => invoke($0, `i16x8.relaxed_laneselect`, [
    i16x8([0x0, 0x1, 0x1234, 0x1234, 0x4, 0x5, 0x6, 0x7]),
    i16x8([0x8, 0x9, 0x5678, 0x5678, 0xc, 0xd, 0xe, 0xf]),
    i16x8([0xffff, 0x0, 0xff00, 0xff, 0x0, 0x0, 0x0, 0x0]),
  ]),
  [
    either(
      i16x8([0x0, 0x9, 0x1278, 0x5634, 0xc, 0xd, 0xe, 0xf]),
      i16x8([0x0, 0x9, 0x1234, 0x5678, 0xc, 0xd, 0xe, 0xf]),
    ),
  ],
);

// ./test/core/relaxed-simd/relaxed_laneselect.wast:43
assert_return(
  () => invoke($0, `i16x8.relaxed_laneselect`, [
    i16x8([0x0, 0x1, 0x1234, 0x1234, 0x4, 0x5, 0x6, 0x7]),
    i16x8([0x8, 0x9, 0x5678, 0x5678, 0xc, 0xd, 0xe, 0xf]),
    i16x8([0xffff, 0x0, 0xff00, 0x80, 0x0, 0x0, 0x0, 0x0]),
  ]),
  [
    either(
      i16x8([0x0, 0x9, 0x1278, 0x5678, 0xc, 0xd, 0xe, 0xf]),
      i16x8([0x0, 0x9, 0x1234, 0x5678, 0xc, 0xd, 0xe, 0xf]),
      i16x8([0x0, 0x9, 0x1278, 0x5634, 0xc, 0xd, 0xe, 0xf]),
    ),
  ],
);

// ./test/core/relaxed-simd/relaxed_laneselect.wast:52
assert_return(
  () => invoke($0, `i32x4.relaxed_laneselect`, [
    i32x4([0x0, 0x1, 0x12341234, 0x12341234]),
    i32x4([0x4, 0x5, 0x56785678, 0x56785678]),
    i32x4([0xffffffff, 0x0, 0xffff0000, 0xffff]),
  ]),
  [
    either(
      i32x4([0x0, 0x5, 0x12345678, 0x56781234]),
      i32x4([0x0, 0x5, 0x12341234, 0x56785678]),
    ),
  ],
);

// ./test/core/relaxed-simd/relaxed_laneselect.wast:59
assert_return(
  () => invoke($0, `i64x2.relaxed_laneselect`, [
    i64x2([0x0n, 0x1n]),
    i64x2([0x2n, 0x3n]),
    i64x2([0xffffffffffffffffn, 0x0n]),
  ]),
  [either(i64x2([0x0n, 0x3n]), i64x2([0x0n, 0x3n]))],
);

// ./test/core/relaxed-simd/relaxed_laneselect.wast:66
assert_return(
  () => invoke($0, `i64x2.relaxed_laneselect`, [
    i64x2([0x1234123412341234n, 0x1234123412341234n]),
    i64x2([0x5678567856785678n, 0x5678567856785678n]),
    i64x2([0xffffffff00000000n, 0xffffffffn]),
  ]),
  [
    either(
      i64x2([0x1234123456785678n, 0x5678567812341234n]),
      i64x2([0x1234123412341234n, 0x5678567856785678n]),
    ),
  ],
);

// ./test/core/relaxed-simd/relaxed_laneselect.wast:75
assert_return(
  () => invoke($0, `i8x16.relaxed_laneselect_cmp`, [
    i8x16([0x0, 0x1, 0x12, 0x12, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf]),
    i8x16([0x10, 0x11, 0x34, 0x34, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f]),
    i8x16([0xff, 0x0, 0xf0, 0xf, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
  ]),
  [
    i8x16([0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff]),
  ],
);

// ./test/core/relaxed-simd/relaxed_laneselect.wast:81
assert_return(
  () => invoke($0, `i16x8.relaxed_laneselect_cmp`, [
    i16x8([0x0, 0x1, 0x1234, 0x1234, 0x4, 0x5, 0x6, 0x7]),
    i16x8([0x8, 0x9, 0x5678, 0x5678, 0xc, 0xd, 0xe, 0xf]),
    i16x8([0xffff, 0x0, 0xff00, 0xff, 0x0, 0x0, 0x0, 0x0]),
  ]),
  [i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff])],
);

// ./test/core/relaxed-simd/relaxed_laneselect.wast:87
assert_return(
  () => invoke($0, `i32x4.relaxed_laneselect_cmp`, [
    i32x4([0x0, 0x1, 0x12341234, 0x12341234]),
    i32x4([0x4, 0x5, 0x56785678, 0x56785678]),
    i32x4([0xffffffff, 0x0, 0xffff0000, 0xffff]),
  ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/relaxed-simd/relaxed_laneselect.wast:93
assert_return(
  () => invoke($0, `i64x2.relaxed_laneselect_cmp`, [
    i64x2([0x0n, 0x1n]),
    i64x2([0x2n, 0x3n]),
    i64x2([0xffffffffffffffffn, 0x0n]),
  ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/relaxed-simd/relaxed_laneselect.wast:99
assert_return(
  () => invoke($0, `i64x2.relaxed_laneselect_cmp`, [
    i64x2([0x1234123412341234n, 0x1234123412341234n]),
    i64x2([0x5678567856785678n, 0x5678567856785678n]),
    i64x2([0xffffffff00000000n, 0xffffffffn]),
  ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);
