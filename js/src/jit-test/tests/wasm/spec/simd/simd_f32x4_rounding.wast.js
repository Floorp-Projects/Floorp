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

// ./test/core/simd/simd_f32x4_rounding.wast

// ./test/core/simd/simd_f32x4_rounding.wast:4
let $0 = instantiate(`(module
  (func (export "f32x4.ceil") (param v128) (result v128) (f32x4.ceil (local.get 0)))
  (func (export "f32x4.floor") (param v128) (result v128) (f32x4.floor (local.get 0)))
  (func (export "f32x4.trunc") (param v128) (result v128) (f32x4.trunc (local.get 0)))
  (func (export "f32x4.nearest") (param v128) (result v128) (f32x4.nearest (local.get 0)))
)`);

// ./test/core/simd/simd_f32x4_rounding.wast:11
assert_return(() => invoke($0, `f32x4.ceil`, [f32x4([0, 0, 0, 0])]), [
  new F32x4Pattern(
    value("f32", 0),
    value("f32", 0),
    value("f32", 0),
    value("f32", 0),
  ),
]);

// ./test/core/simd/simd_f32x4_rounding.wast:13
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      bytes("v128", [
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x80,
      ]),
    ]),
  [
    new F32x4Pattern(
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:15
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1),
      value("f32", 1),
      value("f32", 1),
      value("f32", 1),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:17
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:19
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1),
      value("f32", 1),
      value("f32", 1),
      value("f32", 1),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:21
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:23
assert_return(() => invoke($0, `f32x4.ceil`, [f32x4([0.5, 0.5, 0.5, 0.5])]), [
  new F32x4Pattern(
    value("f32", 1),
    value("f32", 1),
    value("f32", 1),
    value("f32", 1),
  ),
]);

// ./test/core/simd/simd_f32x4_rounding.wast:25
assert_return(
  () => invoke($0, `f32x4.ceil`, [f32x4([-0.5, -0.5, -0.5, -0.5])]),
  [
    new F32x4Pattern(
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:27
assert_return(() => invoke($0, `f32x4.ceil`, [f32x4([1, 1, 1, 1])]), [
  new F32x4Pattern(
    value("f32", 1),
    value("f32", 1),
    value("f32", 1),
    value("f32", 1),
  ),
]);

// ./test/core/simd/simd_f32x4_rounding.wast:29
assert_return(() => invoke($0, `f32x4.ceil`, [f32x4([-1, -1, -1, -1])]), [
  new F32x4Pattern(
    value("f32", -1),
    value("f32", -1),
    value("f32", -1),
    value("f32", -1),
  ),
]);

// ./test/core/simd/simd_f32x4_rounding.wast:31
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 7),
      value("f32", 7),
      value("f32", 7),
      value("f32", 7),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:33
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6),
      value("f32", -6),
      value("f32", -6),
      value("f32", -6),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:35
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:37
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:39
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [f32x4([Infinity, Infinity, Infinity, Infinity])]),
  [
    new F32x4Pattern(
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:41
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -Infinity),
      value("f32", -Infinity),
      value("f32", -Infinity),
      value("f32", -Infinity),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:43
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([123456790, 123456790, 123456790, 123456790]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 123456790),
      value("f32", 123456790),
      value("f32", 123456790),
      value("f32", 123456790),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:45
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:47
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:49
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1),
      value("f32", 1),
      value("f32", 1),
      value("f32", 1),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:51
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([123456790, 123456790, 123456790, 123456790]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 123456790),
      value("f32", 123456790),
      value("f32", 123456790),
      value("f32", 123456790),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:53
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:55
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:57
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1),
      value("f32", 1),
      value("f32", 1),
      value("f32", 1),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:59
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([123456790, 123456790, 123456790, 123456790]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 123456790),
      value("f32", 123456790),
      value("f32", 123456790),
      value("f32", 123456790),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:61
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:63
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:65
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1),
      value("f32", 1),
      value("f32", 1),
      value("f32", 1),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:67
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([
        81985530000000000,
        81985530000000000,
        81985530000000000,
        81985530000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:69
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:71
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:73
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([156374990000, 156374990000, 156374990000, 156374990000]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 156374990000),
      value("f32", 156374990000),
      value("f32", 156374990000),
      value("f32", 156374990000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:75
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([
        81985530000000000,
        81985530000000000,
        81985530000000000,
        81985530000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:77
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:79
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:81
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([156374990000, 156374990000, 156374990000, 156374990000]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 156374990000),
      value("f32", 156374990000),
      value("f32", 156374990000),
      value("f32", 156374990000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:83
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([
        81985530000000000,
        81985530000000000,
        81985530000000000,
        81985530000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:85
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:87
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:89
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      f32x4([156374990000, 156374990000, 156374990000, 156374990000]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 156374990000),
      value("f32", 156374990000),
      value("f32", 156374990000),
      value("f32", 156374990000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:91
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
    ]),
  [
    new F32x4Pattern(
      `canonical_nan`,
      `canonical_nan`,
      `canonical_nan`,
      `canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:93
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
    ]),
  [
    new F32x4Pattern(
      `canonical_nan`,
      `canonical_nan`,
      `canonical_nan`,
      `canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:95
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      bytes("v128", [
        0x0,
        0x0,
        0xa0,
        0x7f,
        0x0,
        0x0,
        0xa0,
        0x7f,
        0x0,
        0x0,
        0xa0,
        0x7f,
        0x0,
        0x0,
        0xa0,
        0x7f,
      ]),
    ]),
  [
    new F32x4Pattern(
      `arithmetic_nan`,
      `arithmetic_nan`,
      `arithmetic_nan`,
      `arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:97
assert_return(
  () =>
    invoke($0, `f32x4.ceil`, [
      bytes("v128", [
        0x0,
        0x0,
        0xa0,
        0xff,
        0x0,
        0x0,
        0xa0,
        0xff,
        0x0,
        0x0,
        0xa0,
        0xff,
        0x0,
        0x0,
        0xa0,
        0xff,
      ]),
    ]),
  [
    new F32x4Pattern(
      `arithmetic_nan`,
      `arithmetic_nan`,
      `arithmetic_nan`,
      `arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:99
assert_return(() => invoke($0, `f32x4.floor`, [f32x4([0, 0, 0, 0])]), [
  new F32x4Pattern(
    value("f32", 0),
    value("f32", 0),
    value("f32", 0),
    value("f32", 0),
  ),
]);

// ./test/core/simd/simd_f32x4_rounding.wast:101
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      bytes("v128", [
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x80,
      ]),
    ]),
  [
    new F32x4Pattern(
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:103
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:105
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -1),
      value("f32", -1),
      value("f32", -1),
      value("f32", -1),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:107
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:109
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -1),
      value("f32", -1),
      value("f32", -1),
      value("f32", -1),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:111
assert_return(() => invoke($0, `f32x4.floor`, [f32x4([0.5, 0.5, 0.5, 0.5])]), [
  new F32x4Pattern(
    value("f32", 0),
    value("f32", 0),
    value("f32", 0),
    value("f32", 0),
  ),
]);

// ./test/core/simd/simd_f32x4_rounding.wast:113
assert_return(
  () => invoke($0, `f32x4.floor`, [f32x4([-0.5, -0.5, -0.5, -0.5])]),
  [
    new F32x4Pattern(
      value("f32", -1),
      value("f32", -1),
      value("f32", -1),
      value("f32", -1),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:115
assert_return(() => invoke($0, `f32x4.floor`, [f32x4([1, 1, 1, 1])]), [
  new F32x4Pattern(
    value("f32", 1),
    value("f32", 1),
    value("f32", 1),
    value("f32", 1),
  ),
]);

// ./test/core/simd/simd_f32x4_rounding.wast:117
assert_return(() => invoke($0, `f32x4.floor`, [f32x4([-1, -1, -1, -1])]), [
  new F32x4Pattern(
    value("f32", -1),
    value("f32", -1),
    value("f32", -1),
    value("f32", -1),
  ),
]);

// ./test/core/simd/simd_f32x4_rounding.wast:119
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6),
      value("f32", 6),
      value("f32", 6),
      value("f32", 6),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:121
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -7),
      value("f32", -7),
      value("f32", -7),
      value("f32", -7),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:123
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:125
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:127
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:129
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -Infinity),
      value("f32", -Infinity),
      value("f32", -Infinity),
      value("f32", -Infinity),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:131
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([123456790, 123456790, 123456790, 123456790]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 123456790),
      value("f32", 123456790),
      value("f32", 123456790),
      value("f32", 123456790),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:133
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:135
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:137
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:139
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([123456790, 123456790, 123456790, 123456790]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 123456790),
      value("f32", 123456790),
      value("f32", 123456790),
      value("f32", 123456790),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:141
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:143
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:145
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:147
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([123456790, 123456790, 123456790, 123456790]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 123456790),
      value("f32", 123456790),
      value("f32", 123456790),
      value("f32", 123456790),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:149
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:151
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:153
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:155
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([
        81985530000000000,
        81985530000000000,
        81985530000000000,
        81985530000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:157
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:159
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:161
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([156374990000, 156374990000, 156374990000, 156374990000]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 156374990000),
      value("f32", 156374990000),
      value("f32", 156374990000),
      value("f32", 156374990000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:163
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([
        81985530000000000,
        81985530000000000,
        81985530000000000,
        81985530000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:165
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:167
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:169
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([156374990000, 156374990000, 156374990000, 156374990000]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 156374990000),
      value("f32", 156374990000),
      value("f32", 156374990000),
      value("f32", 156374990000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:171
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([
        81985530000000000,
        81985530000000000,
        81985530000000000,
        81985530000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:173
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:175
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:177
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      f32x4([156374990000, 156374990000, 156374990000, 156374990000]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 156374990000),
      value("f32", 156374990000),
      value("f32", 156374990000),
      value("f32", 156374990000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:179
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
    ]),
  [
    new F32x4Pattern(
      `canonical_nan`,
      `canonical_nan`,
      `canonical_nan`,
      `canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:181
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
    ]),
  [
    new F32x4Pattern(
      `canonical_nan`,
      `canonical_nan`,
      `canonical_nan`,
      `canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:183
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      bytes("v128", [
        0x0,
        0x0,
        0xa0,
        0x7f,
        0x0,
        0x0,
        0xa0,
        0x7f,
        0x0,
        0x0,
        0xa0,
        0x7f,
        0x0,
        0x0,
        0xa0,
        0x7f,
      ]),
    ]),
  [
    new F32x4Pattern(
      `arithmetic_nan`,
      `arithmetic_nan`,
      `arithmetic_nan`,
      `arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:185
assert_return(
  () =>
    invoke($0, `f32x4.floor`, [
      bytes("v128", [
        0x0,
        0x0,
        0xa0,
        0xff,
        0x0,
        0x0,
        0xa0,
        0xff,
        0x0,
        0x0,
        0xa0,
        0xff,
        0x0,
        0x0,
        0xa0,
        0xff,
      ]),
    ]),
  [
    new F32x4Pattern(
      `arithmetic_nan`,
      `arithmetic_nan`,
      `arithmetic_nan`,
      `arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:187
assert_return(() => invoke($0, `f32x4.trunc`, [f32x4([0, 0, 0, 0])]), [
  new F32x4Pattern(
    value("f32", 0),
    value("f32", 0),
    value("f32", 0),
    value("f32", 0),
  ),
]);

// ./test/core/simd/simd_f32x4_rounding.wast:189
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      bytes("v128", [
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x80,
      ]),
    ]),
  [
    new F32x4Pattern(
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:191
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:193
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:195
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:197
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:199
assert_return(() => invoke($0, `f32x4.trunc`, [f32x4([0.5, 0.5, 0.5, 0.5])]), [
  new F32x4Pattern(
    value("f32", 0),
    value("f32", 0),
    value("f32", 0),
    value("f32", 0),
  ),
]);

// ./test/core/simd/simd_f32x4_rounding.wast:201
assert_return(
  () => invoke($0, `f32x4.trunc`, [f32x4([-0.5, -0.5, -0.5, -0.5])]),
  [
    new F32x4Pattern(
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:203
assert_return(() => invoke($0, `f32x4.trunc`, [f32x4([1, 1, 1, 1])]), [
  new F32x4Pattern(
    value("f32", 1),
    value("f32", 1),
    value("f32", 1),
    value("f32", 1),
  ),
]);

// ./test/core/simd/simd_f32x4_rounding.wast:205
assert_return(() => invoke($0, `f32x4.trunc`, [f32x4([-1, -1, -1, -1])]), [
  new F32x4Pattern(
    value("f32", -1),
    value("f32", -1),
    value("f32", -1),
    value("f32", -1),
  ),
]);

// ./test/core/simd/simd_f32x4_rounding.wast:207
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6),
      value("f32", 6),
      value("f32", 6),
      value("f32", 6),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:209
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6),
      value("f32", -6),
      value("f32", -6),
      value("f32", -6),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:211
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:213
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:215
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:217
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -Infinity),
      value("f32", -Infinity),
      value("f32", -Infinity),
      value("f32", -Infinity),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:219
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([123456790, 123456790, 123456790, 123456790]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 123456790),
      value("f32", 123456790),
      value("f32", 123456790),
      value("f32", 123456790),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:221
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:223
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:225
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:227
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([123456790, 123456790, 123456790, 123456790]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 123456790),
      value("f32", 123456790),
      value("f32", 123456790),
      value("f32", 123456790),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:229
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:231
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:233
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:235
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([123456790, 123456790, 123456790, 123456790]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 123456790),
      value("f32", 123456790),
      value("f32", 123456790),
      value("f32", 123456790),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:237
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:239
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:241
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:243
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([
        81985530000000000,
        81985530000000000,
        81985530000000000,
        81985530000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:245
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:247
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:249
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([156374990000, 156374990000, 156374990000, 156374990000]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 156374990000),
      value("f32", 156374990000),
      value("f32", 156374990000),
      value("f32", 156374990000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:251
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([
        81985530000000000,
        81985530000000000,
        81985530000000000,
        81985530000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:253
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:255
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:257
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([156374990000, 156374990000, 156374990000, 156374990000]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 156374990000),
      value("f32", 156374990000),
      value("f32", 156374990000),
      value("f32", 156374990000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:259
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([
        81985530000000000,
        81985530000000000,
        81985530000000000,
        81985530000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:261
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:263
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:265
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      f32x4([156374990000, 156374990000, 156374990000, 156374990000]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 156374990000),
      value("f32", 156374990000),
      value("f32", 156374990000),
      value("f32", 156374990000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:267
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
    ]),
  [
    new F32x4Pattern(
      `canonical_nan`,
      `canonical_nan`,
      `canonical_nan`,
      `canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:269
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
    ]),
  [
    new F32x4Pattern(
      `canonical_nan`,
      `canonical_nan`,
      `canonical_nan`,
      `canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:271
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      bytes("v128", [
        0x0,
        0x0,
        0xa0,
        0x7f,
        0x0,
        0x0,
        0xa0,
        0x7f,
        0x0,
        0x0,
        0xa0,
        0x7f,
        0x0,
        0x0,
        0xa0,
        0x7f,
      ]),
    ]),
  [
    new F32x4Pattern(
      `arithmetic_nan`,
      `arithmetic_nan`,
      `arithmetic_nan`,
      `arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:273
assert_return(
  () =>
    invoke($0, `f32x4.trunc`, [
      bytes("v128", [
        0x0,
        0x0,
        0xa0,
        0xff,
        0x0,
        0x0,
        0xa0,
        0xff,
        0x0,
        0x0,
        0xa0,
        0xff,
        0x0,
        0x0,
        0xa0,
        0xff,
      ]),
    ]),
  [
    new F32x4Pattern(
      `arithmetic_nan`,
      `arithmetic_nan`,
      `arithmetic_nan`,
      `arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:275
assert_return(() => invoke($0, `f32x4.nearest`, [f32x4([0, 0, 0, 0])]), [
  new F32x4Pattern(
    value("f32", 0),
    value("f32", 0),
    value("f32", 0),
    value("f32", 0),
  ),
]);

// ./test/core/simd/simd_f32x4_rounding.wast:277
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      bytes("v128", [
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x80,
      ]),
    ]),
  [
    new F32x4Pattern(
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:279
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:281
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:283
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:285
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:287
assert_return(
  () => invoke($0, `f32x4.nearest`, [f32x4([0.5, 0.5, 0.5, 0.5])]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:289
assert_return(
  () => invoke($0, `f32x4.nearest`, [f32x4([-0.5, -0.5, -0.5, -0.5])]),
  [
    new F32x4Pattern(
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:291
assert_return(() => invoke($0, `f32x4.nearest`, [f32x4([1, 1, 1, 1])]), [
  new F32x4Pattern(
    value("f32", 1),
    value("f32", 1),
    value("f32", 1),
    value("f32", 1),
  ),
]);

// ./test/core/simd/simd_f32x4_rounding.wast:293
assert_return(() => invoke($0, `f32x4.nearest`, [f32x4([-1, -1, -1, -1])]), [
  new F32x4Pattern(
    value("f32", -1),
    value("f32", -1),
    value("f32", -1),
    value("f32", -1),
  ),
]);

// ./test/core/simd/simd_f32x4_rounding.wast:295
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6),
      value("f32", 6),
      value("f32", 6),
      value("f32", 6),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:297
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6),
      value("f32", -6),
      value("f32", -6),
      value("f32", -6),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:299
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:301
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:303
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:305
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -Infinity),
      value("f32", -Infinity),
      value("f32", -Infinity),
      value("f32", -Infinity),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:307
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([123456790, 123456790, 123456790, 123456790]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 123456790),
      value("f32", 123456790),
      value("f32", 123456790),
      value("f32", 123456790),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:309
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:311
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:313
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:315
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([123456790, 123456790, 123456790, 123456790]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 123456790),
      value("f32", 123456790),
      value("f32", 123456790),
      value("f32", 123456790),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:317
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:319
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:321
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:323
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([123456790, 123456790, 123456790, 123456790]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 123456790),
      value("f32", 123456790),
      value("f32", 123456790),
      value("f32", 123456790),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:325
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:327
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
      value("f32", 1234567900000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:329
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:331
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([
        81985530000000000,
        81985530000000000,
        81985530000000000,
        81985530000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:333
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:335
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:337
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([156374990000, 156374990000, 156374990000, 156374990000]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 156374990000),
      value("f32", 156374990000),
      value("f32", 156374990000),
      value("f32", 156374990000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:339
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([
        81985530000000000,
        81985530000000000,
        81985530000000000,
        81985530000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:341
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:343
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:345
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([156374990000, 156374990000, 156374990000, 156374990000]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 156374990000),
      value("f32", 156374990000),
      value("f32", 156374990000),
      value("f32", 156374990000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:347
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([
        81985530000000000,
        81985530000000000,
        81985530000000000,
        81985530000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
      value("f32", 81985530000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:349
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:351
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
        42984030000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
      value("f32", 42984030000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:353
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      f32x4([156374990000, 156374990000, 156374990000, 156374990000]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 156374990000),
      value("f32", 156374990000),
      value("f32", 156374990000),
      value("f32", 156374990000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:355
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
    ]),
  [
    new F32x4Pattern(
      `canonical_nan`,
      `canonical_nan`,
      `canonical_nan`,
      `canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:357
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
    ]),
  [
    new F32x4Pattern(
      `canonical_nan`,
      `canonical_nan`,
      `canonical_nan`,
      `canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:359
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      bytes("v128", [
        0x0,
        0x0,
        0xa0,
        0x7f,
        0x0,
        0x0,
        0xa0,
        0x7f,
        0x0,
        0x0,
        0xa0,
        0x7f,
        0x0,
        0x0,
        0xa0,
        0x7f,
      ]),
    ]),
  [
    new F32x4Pattern(
      `arithmetic_nan`,
      `arithmetic_nan`,
      `arithmetic_nan`,
      `arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:361
assert_return(
  () =>
    invoke($0, `f32x4.nearest`, [
      bytes("v128", [
        0x0,
        0x0,
        0xa0,
        0xff,
        0x0,
        0x0,
        0xa0,
        0xff,
        0x0,
        0x0,
        0xa0,
        0xff,
        0x0,
        0x0,
        0xa0,
        0xff,
      ]),
    ]),
  [
    new F32x4Pattern(
      `arithmetic_nan`,
      `arithmetic_nan`,
      `arithmetic_nan`,
      `arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4_rounding.wast:367
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i8x16.ceil (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_f32x4_rounding.wast:368
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i8x16.floor (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_f32x4_rounding.wast:369
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i8x16.trunc (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_f32x4_rounding.wast:370
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i8x16.nearest (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_f32x4_rounding.wast:371
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i16x8.ceil (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_f32x4_rounding.wast:372
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i16x8.floor (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_f32x4_rounding.wast:373
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i16x8.trunc (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_f32x4_rounding.wast:374
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i16x8.nearest (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_f32x4_rounding.wast:375
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i32x4.ceil (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_f32x4_rounding.wast:376
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i32x4.floor (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_f32x4_rounding.wast:377
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i32x4.trunc (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_f32x4_rounding.wast:378
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i32x4.nearest (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_f32x4_rounding.wast:379
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i64x2.ceil (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_f32x4_rounding.wast:380
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i64x2.floor (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_f32x4_rounding.wast:381
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i64x2.trunc (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_f32x4_rounding.wast:382
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i64x2.nearest (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_f32x4_rounding.wast:385
assert_invalid(
  () => instantiate(`(module (func (result v128) (f32x4.ceil (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/simd/simd_f32x4_rounding.wast:386
assert_invalid(
  () =>
    instantiate(`(module (func (result v128) (f32x4.floor (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/simd/simd_f32x4_rounding.wast:387
assert_invalid(
  () =>
    instantiate(`(module (func (result v128) (f32x4.trunc (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/simd/simd_f32x4_rounding.wast:388
assert_invalid(
  () =>
    instantiate(`(module (func (result v128) (f32x4.nearest (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/simd/simd_f32x4_rounding.wast:392
assert_invalid(() =>
  instantiate(`(module
    (func $$f32x4.ceil-arg-empty (result v128)
      (f32x4.ceil)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_f32x4_rounding.wast:400
assert_invalid(() =>
  instantiate(`(module
    (func $$f32x4.floor-arg-empty (result v128)
      (f32x4.floor)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_f32x4_rounding.wast:408
assert_invalid(() =>
  instantiate(`(module
    (func $$f32x4.trunc-arg-empty (result v128)
      (f32x4.trunc)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_f32x4_rounding.wast:416
assert_invalid(() =>
  instantiate(`(module
    (func $$f32x4.nearest-arg-empty (result v128)
      (f32x4.nearest)
    )
  )`), `type mismatch`);
