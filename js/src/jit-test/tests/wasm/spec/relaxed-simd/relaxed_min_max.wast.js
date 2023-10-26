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

// ./test/core/relaxed-simd/relaxed_min_max.wast

// ./test/core/relaxed-simd/relaxed_min_max.wast:4
let $0 = instantiate(`(module
    (func (export "f32x4.relaxed_min") (param v128 v128) (result v128) (f32x4.relaxed_min (local.get 0) (local.get 1)))
    (func (export "f32x4.relaxed_max") (param v128 v128) (result v128) (f32x4.relaxed_max (local.get 0) (local.get 1)))
    (func (export "f64x2.relaxed_min") (param v128 v128) (result v128) (f64x2.relaxed_min (local.get 0) (local.get 1)))
    (func (export "f64x2.relaxed_max") (param v128 v128) (result v128) (f64x2.relaxed_max (local.get 0) (local.get 1)))

    (func (export "f32x4.relaxed_min_cmp") (param v128 v128) (result v128)
          (i32x4.eq
            (f32x4.relaxed_min (local.get 0) (local.get 1))
            (f32x4.relaxed_min (local.get 0) (local.get 1))))
    (func (export "f32x4.relaxed_max_cmp") (param v128 v128) (result v128)
          (i32x4.eq
            (f32x4.relaxed_max (local.get 0) (local.get 1))
            (f32x4.relaxed_max (local.get 0) (local.get 1))))
    (func (export "f64x2.relaxed_min_cmp") (param v128 v128) (result v128)
          (i64x2.eq
            (f64x2.relaxed_min (local.get 0) (local.get 1))
            (f64x2.relaxed_min (local.get 0) (local.get 1))))
    (func (export "f64x2.relaxed_max_cmp") (param v128 v128) (result v128)
          (i64x2.eq
            (f64x2.relaxed_max (local.get 0) (local.get 1))
            (f64x2.relaxed_max (local.get 0) (local.get 1))))
)`);

// ./test/core/relaxed-simd/relaxed_min_max.wast:28
assert_return(
  () => invoke($0, `f32x4.relaxed_min`, [
    bytes('v128', [
      0x0,
      0x0,
      0xc0,
      0xff,
      0x0,
      0x0,
      0xc0,
      0x7f,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
    ]),
    bytes('v128', [
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0xc0,
      0xff,
      0x0,
      0x0,
      0xc0,
      0x7f,
    ]),
  ]),
  [
    either(
      new F32x4Pattern(
        `canonical_nan`,
        `canonical_nan`,
        `canonical_nan`,
        `canonical_nan`,
      ),
      new F32x4Pattern(
        `canonical_nan`,
        `canonical_nan`,
        value("f32", 0),
        value("f32", 0),
      ),
      new F32x4Pattern(
        value("f32", 0),
        value("f32", 0),
        `canonical_nan`,
        `canonical_nan`,
      ),
      new F32x4Pattern(
        value("f32", 0),
        value("f32", 0),
        value("f32", 0),
        value("f32", 0),
      ),
    ),
  ],
);

// ./test/core/relaxed-simd/relaxed_min_max.wast:36
assert_return(
  () => invoke($0, `f32x4.relaxed_min`, [f32x4([0, -0, 0, -0]), f32x4([-0, 0, 0, -0])]),
  [
    either(
      new F32x4Pattern(
        value("f32", -0),
        value("f32", -0),
        value("f32", 0),
        value("f32", -0),
      ),
      new F32x4Pattern(
        value("f32", 0),
        value("f32", -0),
        value("f32", 0),
        value("f32", -0),
      ),
      new F32x4Pattern(
        value("f32", -0),
        value("f32", 0),
        value("f32", 0),
        value("f32", -0),
      ),
      new F32x4Pattern(
        value("f32", -0),
        value("f32", -0),
        value("f32", 0),
        value("f32", -0),
      ),
    ),
  ],
);

// ./test/core/relaxed-simd/relaxed_min_max.wast:44
assert_return(
  () => invoke($0, `f32x4.relaxed_max`, [
    bytes('v128', [
      0x0,
      0x0,
      0xc0,
      0xff,
      0x0,
      0x0,
      0xc0,
      0x7f,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
    ]),
    bytes('v128', [
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0xc0,
      0xff,
      0x0,
      0x0,
      0xc0,
      0x7f,
    ]),
  ]),
  [
    either(
      new F32x4Pattern(
        `canonical_nan`,
        `canonical_nan`,
        `canonical_nan`,
        `canonical_nan`,
      ),
      new F32x4Pattern(
        `canonical_nan`,
        `canonical_nan`,
        value("f32", 0),
        value("f32", 0),
      ),
      new F32x4Pattern(
        value("f32", 0),
        value("f32", 0),
        `canonical_nan`,
        `canonical_nan`,
      ),
      new F32x4Pattern(
        value("f32", 0),
        value("f32", 0),
        value("f32", 0),
        value("f32", 0),
      ),
    ),
  ],
);

// ./test/core/relaxed-simd/relaxed_min_max.wast:52
assert_return(
  () => invoke($0, `f32x4.relaxed_max`, [f32x4([0, -0, 0, -0]), f32x4([-0, 0, 0, -0])]),
  [
    either(
      new F32x4Pattern(
        value("f32", 0),
        value("f32", 0),
        value("f32", 0),
        value("f32", -0),
      ),
      new F32x4Pattern(
        value("f32", 0),
        value("f32", -0),
        value("f32", 0),
        value("f32", -0),
      ),
      new F32x4Pattern(
        value("f32", -0),
        value("f32", 0),
        value("f32", 0),
        value("f32", -0),
      ),
      new F32x4Pattern(
        value("f32", -0),
        value("f32", -0),
        value("f32", 0),
        value("f32", -0),
      ),
    ),
  ],
);

// ./test/core/relaxed-simd/relaxed_min_max.wast:60
assert_return(
  () => invoke($0, `f64x2.relaxed_min`, [
    bytes('v128', [
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0xf8,
      0xff,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0xf8,
      0x7f,
    ]),
    f64x2([0, 0]),
  ]),
  [
    either(
      new F64x2Pattern(`canonical_nan`, `canonical_nan`),
      new F64x2Pattern(`canonical_nan`, `canonical_nan`),
      new F64x2Pattern(value("f64", 0), value("f64", 0)),
      new F64x2Pattern(value("f64", 0), value("f64", 0)),
    ),
  ],
);

// ./test/core/relaxed-simd/relaxed_min_max.wast:68
assert_return(
  () => invoke($0, `f64x2.relaxed_min`, [
    f64x2([0, 0]),
    bytes('v128', [
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0xf8,
      0xff,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0xf8,
      0x7f,
    ]),
  ]),
  [
    either(
      new F64x2Pattern(`canonical_nan`, `canonical_nan`),
      new F64x2Pattern(value("f64", 0), value("f64", 0)),
      new F64x2Pattern(`canonical_nan`, `canonical_nan`),
      new F64x2Pattern(value("f64", 0), value("f64", 0)),
    ),
  ],
);

// ./test/core/relaxed-simd/relaxed_min_max.wast:76
assert_return(
  () => invoke($0, `f64x2.relaxed_min`, [f64x2([0, -0]), f64x2([-0, 0])]),
  [
    either(
      new F64x2Pattern(value("f64", -0), value("f64", -0)),
      new F64x2Pattern(value("f64", 0), value("f64", -0)),
      new F64x2Pattern(value("f64", -0), value("f64", 0)),
      new F64x2Pattern(value("f64", -0), value("f64", -0)),
    ),
  ],
);

// ./test/core/relaxed-simd/relaxed_min_max.wast:84
assert_return(
  () => invoke($0, `f64x2.relaxed_min`, [f64x2([0, -0]), f64x2([0, -0])]),
  [
    either(
      new F64x2Pattern(value("f64", 0), value("f64", -0)),
      new F64x2Pattern(value("f64", 0), value("f64", -0)),
      new F64x2Pattern(value("f64", 0), value("f64", -0)),
      new F64x2Pattern(value("f64", 0), value("f64", -0)),
    ),
  ],
);

// ./test/core/relaxed-simd/relaxed_min_max.wast:92
assert_return(
  () => invoke($0, `f64x2.relaxed_max`, [
    bytes('v128', [
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0xf8,
      0xff,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0xf8,
      0x7f,
    ]),
    f64x2([0, 0]),
  ]),
  [
    either(
      new F64x2Pattern(`canonical_nan`, `canonical_nan`),
      new F64x2Pattern(`canonical_nan`, `canonical_nan`),
      new F64x2Pattern(value("f64", 0), value("f64", 0)),
      new F64x2Pattern(value("f64", 0), value("f64", 0)),
    ),
  ],
);

// ./test/core/relaxed-simd/relaxed_min_max.wast:100
assert_return(
  () => invoke($0, `f64x2.relaxed_max`, [
    f64x2([0, 0]),
    bytes('v128', [
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0xf8,
      0xff,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0xf8,
      0x7f,
    ]),
  ]),
  [
    either(
      new F64x2Pattern(`canonical_nan`, `canonical_nan`),
      new F64x2Pattern(value("f64", 0), value("f64", 0)),
      new F64x2Pattern(`canonical_nan`, `canonical_nan`),
      new F64x2Pattern(value("f64", 0), value("f64", 0)),
    ),
  ],
);

// ./test/core/relaxed-simd/relaxed_min_max.wast:108
assert_return(
  () => invoke($0, `f64x2.relaxed_max`, [f64x2([0, -0]), f64x2([-0, 0])]),
  [
    either(
      new F64x2Pattern(value("f64", 0), value("f64", 0)),
      new F64x2Pattern(value("f64", 0), value("f64", -0)),
      new F64x2Pattern(value("f64", -0), value("f64", 0)),
      new F64x2Pattern(value("f64", -0), value("f64", -0)),
    ),
  ],
);

// ./test/core/relaxed-simd/relaxed_min_max.wast:116
assert_return(
  () => invoke($0, `f64x2.relaxed_max`, [f64x2([0, -0]), f64x2([0, -0])]),
  [
    either(
      new F64x2Pattern(value("f64", 0), value("f64", -0)),
      new F64x2Pattern(value("f64", 0), value("f64", -0)),
      new F64x2Pattern(value("f64", 0), value("f64", -0)),
      new F64x2Pattern(value("f64", 0), value("f64", -0)),
    ),
  ],
);

// ./test/core/relaxed-simd/relaxed_min_max.wast:126
assert_return(
  () => invoke($0, `f32x4.relaxed_min_cmp`, [
    bytes('v128', [
      0x0,
      0x0,
      0xc0,
      0xff,
      0x0,
      0x0,
      0xc0,
      0x7f,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
    ]),
    bytes('v128', [
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0xc0,
      0xff,
      0x0,
      0x0,
      0xc0,
      0x7f,
    ]),
  ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/relaxed-simd/relaxed_min_max.wast:131
assert_return(
  () => invoke($0, `f32x4.relaxed_min_cmp`, [
    f32x4([0, -0, 0, -0]),
    f32x4([-0, 0, 0, -0]),
  ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/relaxed-simd/relaxed_min_max.wast:136
assert_return(
  () => invoke($0, `f32x4.relaxed_max_cmp`, [
    bytes('v128', [
      0x0,
      0x0,
      0xc0,
      0xff,
      0x0,
      0x0,
      0xc0,
      0x7f,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
    ]),
    bytes('v128', [
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0xc0,
      0xff,
      0x0,
      0x0,
      0xc0,
      0x7f,
    ]),
  ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/relaxed-simd/relaxed_min_max.wast:141
assert_return(
  () => invoke($0, `f32x4.relaxed_max_cmp`, [
    f32x4([0, -0, 0, -0]),
    f32x4([-0, 0, 0, -0]),
  ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/relaxed-simd/relaxed_min_max.wast:146
assert_return(
  () => invoke($0, `f64x2.relaxed_min_cmp`, [
    bytes('v128', [
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0xf8,
      0xff,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0xf8,
      0x7f,
    ]),
    f64x2([0, 0]),
  ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/relaxed-simd/relaxed_min_max.wast:151
assert_return(
  () => invoke($0, `f64x2.relaxed_min_cmp`, [
    f64x2([0, 0]),
    bytes('v128', [
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0xf8,
      0xff,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0xf8,
      0x7f,
    ]),
  ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/relaxed-simd/relaxed_min_max.wast:156
assert_return(
  () => invoke($0, `f64x2.relaxed_min_cmp`, [f64x2([0, -0]), f64x2([-0, 0])]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/relaxed-simd/relaxed_min_max.wast:161
assert_return(
  () => invoke($0, `f64x2.relaxed_min_cmp`, [f64x2([0, -0]), f64x2([0, -0])]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/relaxed-simd/relaxed_min_max.wast:166
assert_return(
  () => invoke($0, `f64x2.relaxed_max_cmp`, [
    bytes('v128', [
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0xf8,
      0xff,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0xf8,
      0x7f,
    ]),
    f64x2([0, 0]),
  ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/relaxed-simd/relaxed_min_max.wast:171
assert_return(
  () => invoke($0, `f64x2.relaxed_max_cmp`, [
    f64x2([0, 0]),
    bytes('v128', [
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0xf8,
      0xff,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0xf8,
      0x7f,
    ]),
  ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/relaxed-simd/relaxed_min_max.wast:176
assert_return(
  () => invoke($0, `f64x2.relaxed_max_cmp`, [f64x2([0, -0]), f64x2([-0, 0])]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/relaxed-simd/relaxed_min_max.wast:181
assert_return(
  () => invoke($0, `f64x2.relaxed_max_cmp`, [f64x2([0, -0]), f64x2([0, -0])]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);
