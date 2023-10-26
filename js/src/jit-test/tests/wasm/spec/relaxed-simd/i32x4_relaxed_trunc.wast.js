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

// ./test/core/relaxed-simd/i32x4_relaxed_trunc.wast

// ./test/core/relaxed-simd/i32x4_relaxed_trunc.wast:4
let $0 = instantiate(`(module
    (func (export "i32x4.relaxed_trunc_f32x4_s") (param v128) (result v128) (i32x4.relaxed_trunc_f32x4_s (local.get 0)))
    (func (export "i32x4.relaxed_trunc_f32x4_u") (param v128) (result v128) (i32x4.relaxed_trunc_f32x4_u (local.get 0)))
    (func (export "i32x4.relaxed_trunc_f64x2_s_zero") (param v128) (result v128) (i32x4.relaxed_trunc_f64x2_s_zero (local.get 0)))
    (func (export "i32x4.relaxed_trunc_f64x2_u_zero") (param v128) (result v128) (i32x4.relaxed_trunc_f64x2_u_zero (local.get 0)))

    (func (export "i32x4.relaxed_trunc_f32x4_s_cmp") (param v128) (result v128)
          (i32x4.eq
            (i32x4.relaxed_trunc_f32x4_s (local.get 0))
            (i32x4.relaxed_trunc_f32x4_s (local.get 0))))
    (func (export "i32x4.relaxed_trunc_f32x4_u_cmp") (param v128) (result v128)
          (i32x4.eq
            (i32x4.relaxed_trunc_f32x4_u (local.get 0))
            (i32x4.relaxed_trunc_f32x4_u (local.get 0))))
    (func (export "i32x4.relaxed_trunc_f64x2_s_zero_cmp") (param v128) (result v128)
          (i32x4.eq
            (i32x4.relaxed_trunc_f64x2_s_zero (local.get 0))
            (i32x4.relaxed_trunc_f64x2_s_zero (local.get 0))))
    (func (export "i32x4.relaxed_trunc_f64x2_u_zero_cmp") (param v128) (result v128)
          (i32x4.eq
            (i32x4.relaxed_trunc_f64x2_u_zero (local.get 0))
            (i32x4.relaxed_trunc_f64x2_u_zero (local.get 0))))
)`);

// ./test/core/relaxed-simd/i32x4_relaxed_trunc.wast:36
assert_return(
  () => invoke($0, `i32x4.relaxed_trunc_f32x4_s`, [
    f32x4([-2147483600, -2147484000, 2, 2147484000]),
  ]),
  [
    either(
      i32x4([0x80000000, 0x80000000, 0x2, 0x7fffffff]),
      i32x4([0x80000000, 0x80000000, 0x2, 0x80000000]),
    ),
  ],
);

// ./test/core/relaxed-simd/i32x4_relaxed_trunc.wast:43
assert_return(
  () => invoke($0, `i32x4.relaxed_trunc_f32x4_s`, [
    bytes('v128', [
      0x0,
      0x0,
      0xc0,
      0x7f,
      0x0,
      0x0,
      0xc0,
      0xff,
      0x44,
      0x44,
      0xc4,
      0x7f,
      0x44,
      0x44,
      0xc4,
      0xff,
    ]),
  ]),
  [
    either(
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ),
  ],
);

// ./test/core/relaxed-simd/i32x4_relaxed_trunc.wast:49
assert_return(
  () => invoke($0, `i32x4.relaxed_trunc_f32x4_u`, [
    f32x4([0, -1, 4294967000, 4294967300]),
  ]),
  [
    either(
      i32x4([0x0, 0x0, 0xffffff00, 0xffffffff]),
      i32x4([0x0, 0xffffffff, 0xffffff00, 0xffffffff]),
      i32x4([0x0, 0xffffffff, 0xffffff00, 0x0]),
    ),
  ],
);

// ./test/core/relaxed-simd/i32x4_relaxed_trunc.wast:56
assert_return(
  () => invoke($0, `i32x4.relaxed_trunc_f32x4_u`, [
    bytes('v128', [
      0x0,
      0x0,
      0xc0,
      0x7f,
      0x0,
      0x0,
      0xc0,
      0xff,
      0x44,
      0x44,
      0xc4,
      0x7f,
      0x44,
      0x44,
      0xc4,
      0xff,
    ]),
  ]),
  [
    either(
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000]),
    ),
  ],
);

// ./test/core/relaxed-simd/i32x4_relaxed_trunc.wast:62
assert_return(
  () => invoke($0, `i32x4.relaxed_trunc_f64x2_s_zero`, [
    f64x2([-2147483904, 2147483904]),
  ]),
  [
    either(
      i32x4([0x80000000, 0x7fffffff, 0x0, 0x0]),
      i32x4([0x80000000, 0x80000000, 0x0, 0x0]),
    ),
  ],
);

// ./test/core/relaxed-simd/i32x4_relaxed_trunc.wast:68
assert_return(
  () => invoke($0, `i32x4.relaxed_trunc_f64x2_s_zero`, [
    bytes('v128', [
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0xf8,
      0x7f,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0xf8,
      0xff,
    ]),
  ]),
  [
    either(i32x4([0x0, 0x0, 0x0, 0x0]), i32x4([0x80000000, 0x80000000, 0x0, 0x0])),
  ],
);

// ./test/core/relaxed-simd/i32x4_relaxed_trunc.wast:73
assert_return(
  () => invoke($0, `i32x4.relaxed_trunc_f64x2_u_zero`, [f64x2([-1, 4294967296])]),
  [
    either(
      i32x4([0x0, 0xffffffff, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
      i32x4([0xfffffffe, 0x0, 0x0, 0x0]),
    ),
  ],
);

// ./test/core/relaxed-simd/i32x4_relaxed_trunc.wast:79
assert_return(
  () => invoke($0, `i32x4.relaxed_trunc_f64x2_u_zero`, [
    bytes('v128', [
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0xf8,
      0x7f,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0xf8,
      0xff,
    ]),
  ]),
  [
    either(i32x4([0x0, 0x0, 0x0, 0x0]), i32x4([0x0, 0x0, 0xffffffff, 0xffffffff])),
  ],
);

// ./test/core/relaxed-simd/i32x4_relaxed_trunc.wast:86
assert_return(
  () => invoke($0, `i32x4.relaxed_trunc_f32x4_s_cmp`, [
    f32x4([-2147483600, -2147484000, 2147483600, 2147484000]),
  ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/relaxed-simd/i32x4_relaxed_trunc.wast:92
assert_return(
  () => invoke($0, `i32x4.relaxed_trunc_f32x4_s_cmp`, [
    bytes('v128', [
      0x0,
      0x0,
      0xc0,
      0x7f,
      0x0,
      0x0,
      0xc0,
      0xff,
      0x44,
      0x44,
      0xc4,
      0x7f,
      0x44,
      0x44,
      0xc4,
      0xff,
    ]),
  ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/relaxed-simd/i32x4_relaxed_trunc.wast:97
assert_return(
  () => invoke($0, `i32x4.relaxed_trunc_f32x4_u_cmp`, [
    f32x4([0, -1, 4294967000, 4294967300]),
  ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/relaxed-simd/i32x4_relaxed_trunc.wast:103
assert_return(
  () => invoke($0, `i32x4.relaxed_trunc_f32x4_u_cmp`, [
    bytes('v128', [
      0x0,
      0x0,
      0xc0,
      0x7f,
      0x0,
      0x0,
      0xc0,
      0xff,
      0x44,
      0x44,
      0xc4,
      0x7f,
      0x44,
      0x44,
      0xc4,
      0xff,
    ]),
  ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/relaxed-simd/i32x4_relaxed_trunc.wast:108
assert_return(
  () => invoke($0, `i32x4.relaxed_trunc_f64x2_s_zero_cmp`, [
    f64x2([-2147483904, 2147483904]),
  ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/relaxed-simd/i32x4_relaxed_trunc.wast:113
assert_return(
  () => invoke($0, `i32x4.relaxed_trunc_f64x2_s_zero_cmp`, [
    bytes('v128', [
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0xf8,
      0x7f,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0xf8,
      0xff,
    ]),
  ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/relaxed-simd/i32x4_relaxed_trunc.wast:117
assert_return(
  () => invoke($0, `i32x4.relaxed_trunc_f64x2_u_zero_cmp`, [f64x2([-1, 4294967296])]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/relaxed-simd/i32x4_relaxed_trunc.wast:122
assert_return(
  () => invoke($0, `i32x4.relaxed_trunc_f64x2_u_zero_cmp`, [
    bytes('v128', [
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0xf8,
      0x7f,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0xf8,
      0xff,
    ]),
  ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);
