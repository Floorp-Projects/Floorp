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

// ./test/core/simd/simd_f32x4.wast

// ./test/core/simd/simd_f32x4.wast:4
let $0 = instantiate(`(module
  (func (export "f32x4.min") (param v128 v128) (result v128) (f32x4.min (local.get 0) (local.get 1)))
  (func (export "f32x4.max") (param v128 v128) (result v128) (f32x4.max (local.get 0) (local.get 1)))
  (func (export "f32x4.abs") (param v128) (result v128) (f32x4.abs (local.get 0)))
  ;; f32x4.min const vs const
  (func (export "f32x4.min_with_const_0") (result v128) (f32x4.min (v128.const f32x4 0 1 2 -3) (v128.const f32x4 0 2 1 3)))
  (func (export "f32x4.min_with_const_1") (result v128) (f32x4.min (v128.const f32x4 0 1 2 3) (v128.const f32x4 0 1 2 3)))
  (func (export "f32x4.min_with_const_2") (result v128) (f32x4.min (v128.const f32x4 0x00 0x01 0x02 0x80000000) (v128.const f32x4 0x00 0x02 0x01 2147483648)))
  (func (export "f32x4.min_with_const_3") (result v128) (f32x4.min (v128.const f32x4 0x00 0x01 0x02 0x80000000) (v128.const f32x4 0x00 0x01 0x02 0x80000000)))
  ;; f32x4.min param vs const
  (func (export "f32x4.min_with_const_5")(param v128) (result v128) (f32x4.min (local.get 0) (v128.const f32x4 0 1 2 -3)))
  (func (export "f32x4.min_with_const_6")(param v128) (result v128) (f32x4.min (v128.const f32x4 0 1 2 3) (local.get 0)))
  (func (export "f32x4.min_with_const_7")(param v128) (result v128) (f32x4.min (v128.const f32x4 0x00 0x01 0x02 0x80000000) (local.get 0)))
  (func (export "f32x4.min_with_const_8")(param v128) (result v128) (f32x4.min (local.get 0) (v128.const f32x4 0x00 0x01 0x02 0x80000000)))
  ;; f32x4.max const vs const
  (func (export "f32x4.max_with_const_10") (result v128) (f32x4.max (v128.const f32x4 0 1 2 -3) (v128.const f32x4 0 2 1 3)))
  (func (export "f32x4.max_with_const_11") (result v128) (f32x4.max (v128.const f32x4 0 1 2 3) (v128.const f32x4 0 1 2 3)))
  (func (export "f32x4.max_with_const_12") (result v128) (f32x4.max (v128.const f32x4 0x00 0x01 0x02 0x80000000) (v128.const f32x4 0x00 0x02 0x01 2147483648)))
  (func (export "f32x4.max_with_const_13") (result v128) (f32x4.max (v128.const f32x4 0x00 0x01 0x02 0x80000000) (v128.const f32x4 0x00 0x01 0x02 0x80000000)))
  ;; f32x4.max param vs const
  (func (export "f32x4.max_with_const_15")(param v128) (result v128) (f32x4.max (local.get 0) (v128.const f32x4 0 1 2 -3)))
  (func (export "f32x4.max_with_const_16")(param v128) (result v128) (f32x4.max (v128.const f32x4 0 1 2 3) (local.get 0)))
  (func (export "f32x4.max_with_const_17")(param v128) (result v128) (f32x4.max (v128.const f32x4 0x00 0x01 0x02 0x80000000) (local.get 0)))
  (func (export "f32x4.max_with_const_18")(param v128) (result v128) (f32x4.max (local.get 0) (v128.const f32x4 0x00 0x01 0x02 0x80000000)))

  (func (export "f32x4.abs_with_const") (result v128) (f32x4.abs (v128.const f32x4 -0 -1 -2 -3)))
)`);

// ./test/core/simd/simd_f32x4.wast:33
assert_return(() => invoke($0, `f32x4.min_with_const_0`, []), [
  new F32x4Pattern(
    value("f32", 0),
    value("f32", 1),
    value("f32", 1),
    value("f32", -3),
  ),
]);

// ./test/core/simd/simd_f32x4.wast:34
assert_return(() => invoke($0, `f32x4.min_with_const_1`, []), [
  new F32x4Pattern(
    value("f32", 0),
    value("f32", 1),
    value("f32", 2),
    value("f32", 3),
  ),
]);

// ./test/core/simd/simd_f32x4.wast:35
assert_return(() => invoke($0, `f32x4.min_with_const_2`, []), [
  new F32x4Pattern(
    value("f32", 0),
    value("f32", 1),
    value("f32", 1),
    value("f32", 2147483600),
  ),
]);

// ./test/core/simd/simd_f32x4.wast:36
assert_return(() => invoke($0, `f32x4.min_with_const_3`, []), [
  new F32x4Pattern(
    value("f32", 0),
    value("f32", 1),
    value("f32", 2),
    value("f32", 2147483600),
  ),
]);

// ./test/core/simd/simd_f32x4.wast:38
assert_return(
  () => invoke($0, `f32x4.min_with_const_5`, [f32x4([0, 2, 1, 3])]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 1),
      value("f32", 1),
      value("f32", -3),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:40
assert_return(
  () => invoke($0, `f32x4.min_with_const_6`, [f32x4([0, 1, 2, 3])]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 1),
      value("f32", 2),
      value("f32", 3),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:42
assert_return(
  () => invoke($0, `f32x4.min_with_const_7`, [f32x4([0, 2, 1, 2147483600])]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 1),
      value("f32", 1),
      value("f32", 2147483600),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:44
assert_return(
  () => invoke($0, `f32x4.min_with_const_8`, [f32x4([0, 1, 2, 2147483600])]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 1),
      value("f32", 2),
      value("f32", 2147483600),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:47
assert_return(() => invoke($0, `f32x4.max_with_const_10`, []), [
  new F32x4Pattern(
    value("f32", 0),
    value("f32", 2),
    value("f32", 2),
    value("f32", 3),
  ),
]);

// ./test/core/simd/simd_f32x4.wast:48
assert_return(() => invoke($0, `f32x4.max_with_const_11`, []), [
  new F32x4Pattern(
    value("f32", 0),
    value("f32", 1),
    value("f32", 2),
    value("f32", 3),
  ),
]);

// ./test/core/simd/simd_f32x4.wast:49
assert_return(() => invoke($0, `f32x4.max_with_const_12`, []), [
  new F32x4Pattern(
    value("f32", 0),
    value("f32", 2),
    value("f32", 2),
    value("f32", 2147483600),
  ),
]);

// ./test/core/simd/simd_f32x4.wast:50
assert_return(() => invoke($0, `f32x4.max_with_const_13`, []), [
  new F32x4Pattern(
    value("f32", 0),
    value("f32", 1),
    value("f32", 2),
    value("f32", 2147483600),
  ),
]);

// ./test/core/simd/simd_f32x4.wast:52
assert_return(
  () => invoke($0, `f32x4.max_with_const_15`, [f32x4([0, 2, 1, 3])]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 2),
      value("f32", 2),
      value("f32", 3),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:54
assert_return(
  () => invoke($0, `f32x4.max_with_const_16`, [f32x4([0, 1, 2, 3])]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 1),
      value("f32", 2),
      value("f32", 3),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:56
assert_return(
  () => invoke($0, `f32x4.max_with_const_17`, [f32x4([0, 2, 1, 2147483600])]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 2),
      value("f32", 2),
      value("f32", 2147483600),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:58
assert_return(
  () => invoke($0, `f32x4.max_with_const_18`, [f32x4([0, 1, 2, 2147483600])]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 1),
      value("f32", 2),
      value("f32", 2147483600),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:61
assert_return(() => invoke($0, `f32x4.abs_with_const`, []), [
  new F32x4Pattern(
    value("f32", 0),
    value("f32", 1),
    value("f32", 2),
    value("f32", 3),
  ),
]);

// ./test/core/simd/simd_f32x4.wast:65
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      bytes("v128", [
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
        0x0,
        0x0,
        0x80,
        0x3f,
      ]),
      bytes("v128", [
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
        0x80,
        0x3f,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:73
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      bytes("v128", [
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
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
      bytes("v128", [
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
        0x80,
        0x3f,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:81
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      bytes("v128", [
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
        0x0,
        0x0,
        0x80,
        0x3f,
      ]),
      bytes("v128", [
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
        0x80,
        0x3f,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      value("f32", 1),
      value("f32", 1),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:89
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      bytes("v128", [
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
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
      bytes("v128", [
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
        0x80,
        0x3f,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      value("f32", 1),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:97
assert_return(
  () => invoke($0, `f32x4.min`, [f32x4([0, 0, 0, 0]), f32x4([0, 0, 0, 0])]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:100
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([0, 0, 0, 0]),
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

// ./test/core/simd/simd_f32x4.wast:103
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([0, 0, 0, 0]),
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

// ./test/core/simd/simd_f32x4.wast:106
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([0, 0, 0, 0]),
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:109
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([0, 0, 0, 0]),
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

// ./test/core/simd/simd_f32x4.wast:112
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([0, 0, 0, 0]),
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:115
assert_return(
  () =>
    invoke($0, `f32x4.min`, [f32x4([0, 0, 0, 0]), f32x4([0.5, 0.5, 0.5, 0.5])]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:118
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([0, 0, 0, 0]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:121
assert_return(
  () => invoke($0, `f32x4.min`, [f32x4([0, 0, 0, 0]), f32x4([1, 1, 1, 1])]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:124
assert_return(
  () => invoke($0, `f32x4.min`, [f32x4([0, 0, 0, 0]), f32x4([-1, -1, -1, -1])]),
  [
    new F32x4Pattern(
      value("f32", -1),
      value("f32", -1),
      value("f32", -1),
      value("f32", -1),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:127
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([0, 0, 0, 0]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
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

// ./test/core/simd/simd_f32x4.wast:130
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([0, 0, 0, 0]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:133
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([0, 0, 0, 0]),
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
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

// ./test/core/simd/simd_f32x4.wast:136
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([0, 0, 0, 0]),
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

// ./test/core/simd/simd_f32x4.wast:139
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([0, 0, 0, 0]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
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

// ./test/core/simd/simd_f32x4.wast:142
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([0, 0, 0, 0]),
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

// ./test/core/simd/simd_f32x4.wast:145
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([0, 0, 0, 0]),
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

// ./test/core/simd/simd_f32x4.wast:148
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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

// ./test/core/simd/simd_f32x4.wast:151
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
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

// ./test/core/simd/simd_f32x4.wast:154
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:157
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
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

// ./test/core/simd/simd_f32x4.wast:160
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:163
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([0.5, 0.5, 0.5, 0.5]),
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

// ./test/core/simd/simd_f32x4.wast:166
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:169
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([1, 1, 1, 1]),
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

// ./test/core/simd/simd_f32x4.wast:172
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([-1, -1, -1, -1]),
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

// ./test/core/simd/simd_f32x4.wast:175
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
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

// ./test/core/simd/simd_f32x4.wast:178
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:181
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
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

// ./test/core/simd/simd_f32x4.wast:184
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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

// ./test/core/simd/simd_f32x4.wast:187
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([Infinity, Infinity, Infinity, Infinity]),
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

// ./test/core/simd/simd_f32x4.wast:190
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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

// ./test/core/simd/simd_f32x4.wast:193
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([0, 0, 0, 0]),
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

// ./test/core/simd/simd_f32x4.wast:196
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:199
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:202
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:205
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:208
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:211
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:214
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:217
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([1, 1, 1, 1]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:220
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([-1, -1, -1, -1]),
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

// ./test/core/simd/simd_f32x4.wast:223
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:226
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:229
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:232
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:235
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:238
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:241
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([0, 0, 0, 0]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:244
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
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
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:247
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:250
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:253
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:256
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:259
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:262
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:265
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([1, 1, 1, 1]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:268
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([-1, -1, -1, -1]),
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

// ./test/core/simd/simd_f32x4.wast:271
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:274
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:277
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:280
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:283
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:286
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:289
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([0, 0, 0, 0]),
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

// ./test/core/simd/simd_f32x4.wast:292
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:295
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:298
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:301
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:304
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:307
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:310
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:313
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([1, 1, 1, 1]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:316
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([-1, -1, -1, -1]),
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

// ./test/core/simd/simd_f32x4.wast:319
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:322
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:325
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:328
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:331
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:334
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:337
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([0, 0, 0, 0]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:340
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
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
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:343
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:346
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:349
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:352
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:355
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:358
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:361
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([1, 1, 1, 1]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:364
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([-1, -1, -1, -1]),
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

// ./test/core/simd/simd_f32x4.wast:367
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:370
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:373
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:376
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:379
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:382
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:385
assert_return(
  () =>
    invoke($0, `f32x4.min`, [f32x4([0.5, 0.5, 0.5, 0.5]), f32x4([0, 0, 0, 0])]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:388
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([0.5, 0.5, 0.5, 0.5]),
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

// ./test/core/simd/simd_f32x4.wast:391
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([0.5, 0.5, 0.5, 0.5]),
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:394
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([0.5, 0.5, 0.5, 0.5]),
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:397
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([0.5, 0.5, 0.5, 0.5]),
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:400
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([0.5, 0.5, 0.5, 0.5]),
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:403
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([0.5, 0.5, 0.5, 0.5]),
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:406
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([0.5, 0.5, 0.5, 0.5]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:409
assert_return(
  () =>
    invoke($0, `f32x4.min`, [f32x4([0.5, 0.5, 0.5, 0.5]), f32x4([1, 1, 1, 1])]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:412
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([0.5, 0.5, 0.5, 0.5]),
      f32x4([-1, -1, -1, -1]),
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

// ./test/core/simd/simd_f32x4.wast:415
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([0.5, 0.5, 0.5, 0.5]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:418
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([0.5, 0.5, 0.5, 0.5]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:421
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([0.5, 0.5, 0.5, 0.5]),
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:424
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([0.5, 0.5, 0.5, 0.5]),
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

// ./test/core/simd/simd_f32x4.wast:427
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([0.5, 0.5, 0.5, 0.5]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:430
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([0.5, 0.5, 0.5, 0.5]),
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

// ./test/core/simd/simd_f32x4.wast:433
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
      f32x4([0, 0, 0, 0]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:436
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
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
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:439
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:442
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:445
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:448
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:451
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:454
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:457
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
      f32x4([1, 1, 1, 1]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:460
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
      f32x4([-1, -1, -1, -1]),
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

// ./test/core/simd/simd_f32x4.wast:463
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:466
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:469
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:472
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
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

// ./test/core/simd/simd_f32x4.wast:475
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:478
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
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

// ./test/core/simd/simd_f32x4.wast:481
assert_return(
  () => invoke($0, `f32x4.min`, [f32x4([1, 1, 1, 1]), f32x4([0, 0, 0, 0])]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:484
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([1, 1, 1, 1]),
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

// ./test/core/simd/simd_f32x4.wast:487
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([1, 1, 1, 1]),
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:490
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([1, 1, 1, 1]),
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:493
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([1, 1, 1, 1]),
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:496
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([1, 1, 1, 1]),
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:499
assert_return(
  () =>
    invoke($0, `f32x4.min`, [f32x4([1, 1, 1, 1]), f32x4([0.5, 0.5, 0.5, 0.5])]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:502
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([1, 1, 1, 1]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:505
assert_return(
  () => invoke($0, `f32x4.min`, [f32x4([1, 1, 1, 1]), f32x4([1, 1, 1, 1])]),
  [
    new F32x4Pattern(
      value("f32", 1),
      value("f32", 1),
      value("f32", 1),
      value("f32", 1),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:508
assert_return(
  () => invoke($0, `f32x4.min`, [f32x4([1, 1, 1, 1]), f32x4([-1, -1, -1, -1])]),
  [
    new F32x4Pattern(
      value("f32", -1),
      value("f32", -1),
      value("f32", -1),
      value("f32", -1),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:511
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([1, 1, 1, 1]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
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

// ./test/core/simd/simd_f32x4.wast:514
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([1, 1, 1, 1]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:517
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([1, 1, 1, 1]),
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
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

// ./test/core/simd/simd_f32x4.wast:520
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([1, 1, 1, 1]),
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

// ./test/core/simd/simd_f32x4.wast:523
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([1, 1, 1, 1]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
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

// ./test/core/simd/simd_f32x4.wast:526
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([1, 1, 1, 1]),
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

// ./test/core/simd/simd_f32x4.wast:529
assert_return(
  () => invoke($0, `f32x4.min`, [f32x4([-1, -1, -1, -1]), f32x4([0, 0, 0, 0])]),
  [
    new F32x4Pattern(
      value("f32", -1),
      value("f32", -1),
      value("f32", -1),
      value("f32", -1),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:532
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-1, -1, -1, -1]),
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
      value("f32", -1),
      value("f32", -1),
      value("f32", -1),
      value("f32", -1),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:535
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-1, -1, -1, -1]),
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
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

// ./test/core/simd/simd_f32x4.wast:538
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-1, -1, -1, -1]),
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

// ./test/core/simd/simd_f32x4.wast:541
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-1, -1, -1, -1]),
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
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

// ./test/core/simd/simd_f32x4.wast:544
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-1, -1, -1, -1]),
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

// ./test/core/simd/simd_f32x4.wast:547
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-1, -1, -1, -1]),
      f32x4([0.5, 0.5, 0.5, 0.5]),
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

// ./test/core/simd/simd_f32x4.wast:550
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-1, -1, -1, -1]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
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

// ./test/core/simd/simd_f32x4.wast:553
assert_return(
  () => invoke($0, `f32x4.min`, [f32x4([-1, -1, -1, -1]), f32x4([1, 1, 1, 1])]),
  [
    new F32x4Pattern(
      value("f32", -1),
      value("f32", -1),
      value("f32", -1),
      value("f32", -1),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:556
assert_return(
  () =>
    invoke($0, `f32x4.min`, [f32x4([-1, -1, -1, -1]), f32x4([-1, -1, -1, -1])]),
  [
    new F32x4Pattern(
      value("f32", -1),
      value("f32", -1),
      value("f32", -1),
      value("f32", -1),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:559
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-1, -1, -1, -1]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
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

// ./test/core/simd/simd_f32x4.wast:562
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-1, -1, -1, -1]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:565
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-1, -1, -1, -1]),
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
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

// ./test/core/simd/simd_f32x4.wast:568
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-1, -1, -1, -1]),
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

// ./test/core/simd/simd_f32x4.wast:571
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-1, -1, -1, -1]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
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

// ./test/core/simd/simd_f32x4.wast:574
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-1, -1, -1, -1]),
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

// ./test/core/simd/simd_f32x4.wast:577
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
      f32x4([0, 0, 0, 0]),
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

// ./test/core/simd/simd_f32x4.wast:580
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
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

// ./test/core/simd/simd_f32x4.wast:583
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:586
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:589
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:592
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:595
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:598
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:601
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
      f32x4([1, 1, 1, 1]),
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

// ./test/core/simd/simd_f32x4.wast:604
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
      f32x4([-1, -1, -1, -1]),
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

// ./test/core/simd/simd_f32x4.wast:607
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:610
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:613
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:616
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
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

// ./test/core/simd/simd_f32x4.wast:619
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:622
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
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

// ./test/core/simd/simd_f32x4.wast:625
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
      f32x4([0, 0, 0, 0]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:628
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
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
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:631
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:634
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:637
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:640
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:643
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:646
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:649
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
      f32x4([1, 1, 1, 1]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:652
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
      f32x4([-1, -1, -1, -1]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:655
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:658
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:661
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:664
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
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

// ./test/core/simd/simd_f32x4.wast:667
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:670
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
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

// ./test/core/simd/simd_f32x4.wast:673
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
      f32x4([0, 0, 0, 0]),
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

// ./test/core/simd/simd_f32x4.wast:676
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:679
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:682
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:685
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:688
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:691
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:694
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:697
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
      f32x4([1, 1, 1, 1]),
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

// ./test/core/simd/simd_f32x4.wast:700
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
      f32x4([-1, -1, -1, -1]),
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

// ./test/core/simd/simd_f32x4.wast:703
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:706
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:709
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:712
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:715
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
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

// ./test/core/simd/simd_f32x4.wast:718
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:721
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
      f32x4([0, 0, 0, 0]),
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

// ./test/core/simd/simd_f32x4.wast:724
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
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
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:727
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
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

// ./test/core/simd/simd_f32x4.wast:730
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
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

// ./test/core/simd/simd_f32x4.wast:733
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
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

// ./test/core/simd/simd_f32x4.wast:736
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
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

// ./test/core/simd/simd_f32x4.wast:739
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
      f32x4([0.5, 0.5, 0.5, 0.5]),
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

// ./test/core/simd/simd_f32x4.wast:742
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
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

// ./test/core/simd/simd_f32x4.wast:745
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
      f32x4([1, 1, 1, 1]),
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

// ./test/core/simd/simd_f32x4.wast:748
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
      f32x4([-1, -1, -1, -1]),
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

// ./test/core/simd/simd_f32x4.wast:751
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
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

// ./test/core/simd/simd_f32x4.wast:754
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
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

// ./test/core/simd/simd_f32x4.wast:757
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
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

// ./test/core/simd/simd_f32x4.wast:760
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:763
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
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

// ./test/core/simd/simd_f32x4.wast:766
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:769
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([0, 0, 0, 0]),
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

// ./test/core/simd/simd_f32x4.wast:772
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
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

// ./test/core/simd/simd_f32x4.wast:775
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:778
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:781
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:784
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:787
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:790
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:793
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([1, 1, 1, 1]),
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

// ./test/core/simd/simd_f32x4.wast:796
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([-1, -1, -1, -1]),
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

// ./test/core/simd/simd_f32x4.wast:799
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:802
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:805
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
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

// ./test/core/simd/simd_f32x4.wast:808
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
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

// ./test/core/simd/simd_f32x4.wast:811
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
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

// ./test/core/simd/simd_f32x4.wast:814
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
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

// ./test/core/simd/simd_f32x4.wast:817
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([0, 0, 0, 0]),
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

// ./test/core/simd/simd_f32x4.wast:820
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
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
      value("f32", -Infinity),
      value("f32", -Infinity),
      value("f32", -Infinity),
      value("f32", -Infinity),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:823
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:826
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:829
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:832
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:835
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([0.5, 0.5, 0.5, 0.5]),
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

// ./test/core/simd/simd_f32x4.wast:838
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
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

// ./test/core/simd/simd_f32x4.wast:841
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([1, 1, 1, 1]),
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

// ./test/core/simd/simd_f32x4.wast:844
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([-1, -1, -1, -1]),
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

// ./test/core/simd/simd_f32x4.wast:847
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
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

// ./test/core/simd/simd_f32x4.wast:850
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
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

// ./test/core/simd/simd_f32x4.wast:853
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:856
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:859
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
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

// ./test/core/simd/simd_f32x4.wast:862
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
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

// ./test/core/simd/simd_f32x4.wast:865
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:868
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:871
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:874
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:877
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
      f32x4([-123456790, -123456790, -123456790, -123456790]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -123456790),
      value("f32", -123456790),
      value("f32", -123456790),
      value("f32", -123456790),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:880
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:883
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:886
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:889
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:892
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
      f32x4([-123456790, -123456790, -123456790, -123456790]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -123456790),
      value("f32", -123456790),
      value("f32", -123456790),
      value("f32", -123456790),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:895
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:898
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:901
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:904
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:907
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
      f32x4([-123456790, -123456790, -123456790, -123456790]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -123456790),
      value("f32", -123456790),
      value("f32", -123456790),
      value("f32", -123456790),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:910
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:913
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:916
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:919
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:922
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
      f32x4([-123456790, -123456790, -123456790, -123456790]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -123456790),
      value("f32", -123456790),
      value("f32", -123456790),
      value("f32", -123456790),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:925
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-123456790, -123456790, -123456790, -123456790]),
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -123456790),
      value("f32", -123456790),
      value("f32", -123456790),
      value("f32", -123456790),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:928
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-123456790, -123456790, -123456790, -123456790]),
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -123456790),
      value("f32", -123456790),
      value("f32", -123456790),
      value("f32", -123456790),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:931
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-123456790, -123456790, -123456790, -123456790]),
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -123456790),
      value("f32", -123456790),
      value("f32", -123456790),
      value("f32", -123456790),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:934
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-123456790, -123456790, -123456790, -123456790]),
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -123456790),
      value("f32", -123456790),
      value("f32", -123456790),
      value("f32", -123456790),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:937
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      f32x4([-123456790, -123456790, -123456790, -123456790]),
      f32x4([-123456790, -123456790, -123456790, -123456790]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -123456790),
      value("f32", -123456790),
      value("f32", -123456790),
      value("f32", -123456790),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:940
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([0, 0, 0, 0]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:943
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:946
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:949
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:952
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:955
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:958
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:961
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:964
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([1, 1, 1, 1]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:967
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([-1, -1, -1, -1]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:970
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:973
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:976
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:979
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:982
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:985
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:988
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:991
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:994
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:997
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1000
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([0, 0, 0, 0]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1003
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1006
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1009
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1012
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1015
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1018
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1021
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1024
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([1, 1, 1, 1]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1027
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([-1, -1, -1, -1]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1030
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1033
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1036
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1039
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1042
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1045
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1048
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1051
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1054
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1057
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1060
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([0, 0, 0, 0]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1063
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1066
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1069
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1072
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1075
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1078
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1081
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1084
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([1, 1, 1, 1]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1087
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([-1, -1, -1, -1]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1090
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1093
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1096
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1099
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1102
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1105
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1108
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1111
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1114
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1117
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1120
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([0, 0, 0, 0]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1123
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1126
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1129
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1132
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1135
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1138
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1141
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1144
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([1, 1, 1, 1]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1147
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([-1, -1, -1, -1]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1150
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1153
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1156
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1159
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1162
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1165
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1168
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1171
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1174
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1177
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1180
assert_return(
  () => invoke($0, `f32x4.max`, [f32x4([0, 0, 0, 0]), f32x4([0, 0, 0, 0])]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1183
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([0, 0, 0, 0]),
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
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1186
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([0, 0, 0, 0]),
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1189
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([0, 0, 0, 0]),
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
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

// ./test/core/simd/simd_f32x4.wast:1192
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([0, 0, 0, 0]),
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1195
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([0, 0, 0, 0]),
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
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

// ./test/core/simd/simd_f32x4.wast:1198
assert_return(
  () =>
    invoke($0, `f32x4.max`, [f32x4([0, 0, 0, 0]), f32x4([0.5, 0.5, 0.5, 0.5])]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1201
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([0, 0, 0, 0]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
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

// ./test/core/simd/simd_f32x4.wast:1204
assert_return(
  () => invoke($0, `f32x4.max`, [f32x4([0, 0, 0, 0]), f32x4([1, 1, 1, 1])]),
  [
    new F32x4Pattern(
      value("f32", 1),
      value("f32", 1),
      value("f32", 1),
      value("f32", 1),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1207
assert_return(
  () => invoke($0, `f32x4.max`, [f32x4([0, 0, 0, 0]), f32x4([-1, -1, -1, -1])]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1210
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([0, 0, 0, 0]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1213
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([0, 0, 0, 0]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
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

// ./test/core/simd/simd_f32x4.wast:1216
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([0, 0, 0, 0]),
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

// ./test/core/simd/simd_f32x4.wast:1219
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([0, 0, 0, 0]),
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
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

// ./test/core/simd/simd_f32x4.wast:1222
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([0, 0, 0, 0]),
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

// ./test/core/simd/simd_f32x4.wast:1225
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([0, 0, 0, 0]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
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

// ./test/core/simd/simd_f32x4.wast:1228
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([0, 0, 0, 0]),
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

// ./test/core/simd/simd_f32x4.wast:1231
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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

// ./test/core/simd/simd_f32x4.wast:1234
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1237
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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

// ./test/core/simd/simd_f32x4.wast:1240
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1243
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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

// ./test/core/simd/simd_f32x4.wast:1246
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1249
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([-0.5, -0.5, -0.5, -0.5]),
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

// ./test/core/simd/simd_f32x4.wast:1252
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([1, 1, 1, 1]),
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

// ./test/core/simd/simd_f32x4.wast:1255
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([-1, -1, -1, -1]),
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

// ./test/core/simd/simd_f32x4.wast:1258
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1261
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
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

// ./test/core/simd/simd_f32x4.wast:1264
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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

// ./test/core/simd/simd_f32x4.wast:1267
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
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

// ./test/core/simd/simd_f32x4.wast:1270
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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

// ./test/core/simd/simd_f32x4.wast:1273
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
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

// ./test/core/simd/simd_f32x4.wast:1276
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([0, 0, 0, 0]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1279
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
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
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1282
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1285
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1288
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1291
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1294
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1297
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1300
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([1, 1, 1, 1]),
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

// ./test/core/simd/simd_f32x4.wast:1303
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([-1, -1, -1, -1]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1306
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1309
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1312
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1315
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1318
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1321
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1324
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([0, 0, 0, 0]),
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

// ./test/core/simd/simd_f32x4.wast:1327
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1330
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1333
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1336
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1339
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1342
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1345
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1348
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([1, 1, 1, 1]),
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

// ./test/core/simd/simd_f32x4.wast:1351
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([-1, -1, -1, -1]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1354
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1357
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1360
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1363
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1366
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1369
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1372
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([0, 0, 0, 0]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1375
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
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
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1378
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1381
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1384
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1387
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1390
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1393
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1396
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([1, 1, 1, 1]),
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

// ./test/core/simd/simd_f32x4.wast:1399
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([-1, -1, -1, -1]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1402
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1405
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1408
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1411
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1414
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1417
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1420
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([0, 0, 0, 0]),
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

// ./test/core/simd/simd_f32x4.wast:1423
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1426
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1429
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1432
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1435
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1438
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1441
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1444
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([1, 1, 1, 1]),
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

// ./test/core/simd/simd_f32x4.wast:1447
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([-1, -1, -1, -1]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1450
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1453
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1456
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1459
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1462
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1465
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1468
assert_return(
  () =>
    invoke($0, `f32x4.max`, [f32x4([0.5, 0.5, 0.5, 0.5]), f32x4([0, 0, 0, 0])]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1471
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([0.5, 0.5, 0.5, 0.5]),
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
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1474
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([0.5, 0.5, 0.5, 0.5]),
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1477
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([0.5, 0.5, 0.5, 0.5]),
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1480
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([0.5, 0.5, 0.5, 0.5]),
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1483
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([0.5, 0.5, 0.5, 0.5]),
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1486
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([0.5, 0.5, 0.5, 0.5]),
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1489
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([0.5, 0.5, 0.5, 0.5]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1492
assert_return(
  () =>
    invoke($0, `f32x4.max`, [f32x4([0.5, 0.5, 0.5, 0.5]), f32x4([1, 1, 1, 1])]),
  [
    new F32x4Pattern(
      value("f32", 1),
      value("f32", 1),
      value("f32", 1),
      value("f32", 1),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1495
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([0.5, 0.5, 0.5, 0.5]),
      f32x4([-1, -1, -1, -1]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1498
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([0.5, 0.5, 0.5, 0.5]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1501
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([0.5, 0.5, 0.5, 0.5]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1504
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([0.5, 0.5, 0.5, 0.5]),
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

// ./test/core/simd/simd_f32x4.wast:1507
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([0.5, 0.5, 0.5, 0.5]),
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1510
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([0.5, 0.5, 0.5, 0.5]),
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

// ./test/core/simd/simd_f32x4.wast:1513
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([0.5, 0.5, 0.5, 0.5]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1516
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
      f32x4([0, 0, 0, 0]),
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

// ./test/core/simd/simd_f32x4.wast:1519
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
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

// ./test/core/simd/simd_f32x4.wast:1522
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1525
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1528
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1531
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1534
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1537
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1540
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
      f32x4([1, 1, 1, 1]),
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

// ./test/core/simd/simd_f32x4.wast:1543
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
      f32x4([-1, -1, -1, -1]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1546
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1549
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1552
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
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

// ./test/core/simd/simd_f32x4.wast:1555
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1558
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
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

// ./test/core/simd/simd_f32x4.wast:1561
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-0.5, -0.5, -0.5, -0.5]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1564
assert_return(
  () => invoke($0, `f32x4.max`, [f32x4([1, 1, 1, 1]), f32x4([0, 0, 0, 0])]),
  [
    new F32x4Pattern(
      value("f32", 1),
      value("f32", 1),
      value("f32", 1),
      value("f32", 1),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1567
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([1, 1, 1, 1]),
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
      value("f32", 1),
      value("f32", 1),
      value("f32", 1),
      value("f32", 1),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1570
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([1, 1, 1, 1]),
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

// ./test/core/simd/simd_f32x4.wast:1573
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([1, 1, 1, 1]),
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
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

// ./test/core/simd/simd_f32x4.wast:1576
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([1, 1, 1, 1]),
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

// ./test/core/simd/simd_f32x4.wast:1579
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([1, 1, 1, 1]),
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
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

// ./test/core/simd/simd_f32x4.wast:1582
assert_return(
  () =>
    invoke($0, `f32x4.max`, [f32x4([1, 1, 1, 1]), f32x4([0.5, 0.5, 0.5, 0.5])]),
  [
    new F32x4Pattern(
      value("f32", 1),
      value("f32", 1),
      value("f32", 1),
      value("f32", 1),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1585
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([1, 1, 1, 1]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
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

// ./test/core/simd/simd_f32x4.wast:1588
assert_return(
  () => invoke($0, `f32x4.max`, [f32x4([1, 1, 1, 1]), f32x4([1, 1, 1, 1])]),
  [
    new F32x4Pattern(
      value("f32", 1),
      value("f32", 1),
      value("f32", 1),
      value("f32", 1),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1591
assert_return(
  () => invoke($0, `f32x4.max`, [f32x4([1, 1, 1, 1]), f32x4([-1, -1, -1, -1])]),
  [
    new F32x4Pattern(
      value("f32", 1),
      value("f32", 1),
      value("f32", 1),
      value("f32", 1),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1594
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([1, 1, 1, 1]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1597
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([1, 1, 1, 1]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
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

// ./test/core/simd/simd_f32x4.wast:1600
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([1, 1, 1, 1]),
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

// ./test/core/simd/simd_f32x4.wast:1603
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([1, 1, 1, 1]),
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
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

// ./test/core/simd/simd_f32x4.wast:1606
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([1, 1, 1, 1]),
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

// ./test/core/simd/simd_f32x4.wast:1609
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([1, 1, 1, 1]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
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

// ./test/core/simd/simd_f32x4.wast:1612
assert_return(
  () => invoke($0, `f32x4.max`, [f32x4([-1, -1, -1, -1]), f32x4([0, 0, 0, 0])]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1615
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-1, -1, -1, -1]),
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

// ./test/core/simd/simd_f32x4.wast:1618
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-1, -1, -1, -1]),
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1621
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-1, -1, -1, -1]),
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1624
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-1, -1, -1, -1]),
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1627
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-1, -1, -1, -1]),
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1630
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-1, -1, -1, -1]),
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1633
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-1, -1, -1, -1]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1636
assert_return(
  () => invoke($0, `f32x4.max`, [f32x4([-1, -1, -1, -1]), f32x4([1, 1, 1, 1])]),
  [
    new F32x4Pattern(
      value("f32", 1),
      value("f32", 1),
      value("f32", 1),
      value("f32", 1),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1639
assert_return(
  () =>
    invoke($0, `f32x4.max`, [f32x4([-1, -1, -1, -1]), f32x4([-1, -1, -1, -1])]),
  [
    new F32x4Pattern(
      value("f32", -1),
      value("f32", -1),
      value("f32", -1),
      value("f32", -1),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1642
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-1, -1, -1, -1]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1645
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-1, -1, -1, -1]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
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

// ./test/core/simd/simd_f32x4.wast:1648
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-1, -1, -1, -1]),
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

// ./test/core/simd/simd_f32x4.wast:1651
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-1, -1, -1, -1]),
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
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

// ./test/core/simd/simd_f32x4.wast:1654
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-1, -1, -1, -1]),
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

// ./test/core/simd/simd_f32x4.wast:1657
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-1, -1, -1, -1]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
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

// ./test/core/simd/simd_f32x4.wast:1660
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
      f32x4([0, 0, 0, 0]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1663
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
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
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1666
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1669
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1672
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1675
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1678
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1681
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1684
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
      f32x4([1, 1, 1, 1]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1687
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
      f32x4([-1, -1, -1, -1]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1690
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1693
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1696
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
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

// ./test/core/simd/simd_f32x4.wast:1699
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1702
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
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

// ./test/core/simd/simd_f32x4.wast:1705
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1708
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
      f32x4([0, 0, 0, 0]),
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

// ./test/core/simd/simd_f32x4.wast:1711
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
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

// ./test/core/simd/simd_f32x4.wast:1714
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1717
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1720
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1723
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1726
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1729
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1732
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
      f32x4([1, 1, 1, 1]),
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

// ./test/core/simd/simd_f32x4.wast:1735
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
      f32x4([-1, -1, -1, -1]),
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

// ./test/core/simd/simd_f32x4.wast:1738
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1741
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1744
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
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

// ./test/core/simd/simd_f32x4.wast:1747
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1750
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
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

// ./test/core/simd/simd_f32x4.wast:1753
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1756
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
      f32x4([0, 0, 0, 0]),
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

// ./test/core/simd/simd_f32x4.wast:1759
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
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
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1762
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
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

// ./test/core/simd/simd_f32x4.wast:1765
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
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

// ./test/core/simd/simd_f32x4.wast:1768
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
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

// ./test/core/simd/simd_f32x4.wast:1771
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
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

// ./test/core/simd/simd_f32x4.wast:1774
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
      f32x4([0.5, 0.5, 0.5, 0.5]),
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

// ./test/core/simd/simd_f32x4.wast:1777
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
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

// ./test/core/simd/simd_f32x4.wast:1780
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
      f32x4([1, 1, 1, 1]),
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

// ./test/core/simd/simd_f32x4.wast:1783
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
      f32x4([-1, -1, -1, -1]),
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

// ./test/core/simd/simd_f32x4.wast:1786
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
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

// ./test/core/simd/simd_f32x4.wast:1789
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
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

// ./test/core/simd/simd_f32x4.wast:1792
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1795
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
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

// ./test/core/simd/simd_f32x4.wast:1798
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1801
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
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

// ./test/core/simd/simd_f32x4.wast:1804
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
      f32x4([0, 0, 0, 0]),
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

// ./test/core/simd/simd_f32x4.wast:1807
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1810
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1813
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1816
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1819
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1822
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1825
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1828
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
      f32x4([1, 1, 1, 1]),
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

// ./test/core/simd/simd_f32x4.wast:1831
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
      f32x4([-1, -1, -1, -1]),
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

// ./test/core/simd/simd_f32x4.wast:1834
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1837
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1840
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1843
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1846
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1849
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
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

// ./test/core/simd/simd_f32x4.wast:1852
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([0, 0, 0, 0]),
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

// ./test/core/simd/simd_f32x4.wast:1855
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
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
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1858
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1861
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1864
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1867
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1870
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([0.5, 0.5, 0.5, 0.5]),
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

// ./test/core/simd/simd_f32x4.wast:1873
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
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

// ./test/core/simd/simd_f32x4.wast:1876
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([1, 1, 1, 1]),
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

// ./test/core/simd/simd_f32x4.wast:1879
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([-1, -1, -1, -1]),
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

// ./test/core/simd/simd_f32x4.wast:1882
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
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

// ./test/core/simd/simd_f32x4.wast:1885
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
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

// ./test/core/simd/simd_f32x4.wast:1888
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1891
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1894
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
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

// ./test/core/simd/simd_f32x4.wast:1897
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
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

// ./test/core/simd/simd_f32x4.wast:1900
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([0, 0, 0, 0]),
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

// ./test/core/simd/simd_f32x4.wast:1903
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
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

// ./test/core/simd/simd_f32x4.wast:1906
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1909
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1912
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1915
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1918
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1921
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
      value("f32", -0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1924
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([1, 1, 1, 1]),
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

// ./test/core/simd/simd_f32x4.wast:1927
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([-1, -1, -1, -1]),
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

// ./test/core/simd/simd_f32x4.wast:1930
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1933
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
      value("f32", -6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1936
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
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

// ./test/core/simd/simd_f32x4.wast:1939
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
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

// ./test/core/simd/simd_f32x4.wast:1942
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
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

// ./test/core/simd/simd_f32x4.wast:1945
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
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

// ./test/core/simd/simd_f32x4.wast:1948
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1951
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
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

// ./test/core/simd/simd_f32x4.wast:1954
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1957
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1960
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
      f32x4([-123456790, -123456790, -123456790, -123456790]),
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

// ./test/core/simd/simd_f32x4.wast:1963
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1966
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1969
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1972
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1975
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
      f32x4([-123456790, -123456790, -123456790, -123456790]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:1978
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1981
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
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

// ./test/core/simd/simd_f32x4.wast:1984
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1987
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1990
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
      f32x4([-123456790, -123456790, -123456790, -123456790]),
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

// ./test/core/simd/simd_f32x4.wast:1993
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:1996
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
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

// ./test/core/simd/simd_f32x4.wast:1999
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:2002
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
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

// ./test/core/simd/simd_f32x4.wast:2005
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
        1234567900000000000000000000,
      ]),
      f32x4([-123456790, -123456790, -123456790, -123456790]),
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

// ./test/core/simd/simd_f32x4.wast:2008
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-123456790, -123456790, -123456790, -123456790]),
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

// ./test/core/simd/simd_f32x4.wast:2011
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-123456790, -123456790, -123456790, -123456790]),
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2014
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-123456790, -123456790, -123456790, -123456790]),
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

// ./test/core/simd/simd_f32x4.wast:2017
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-123456790, -123456790, -123456790, -123456790]),
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

// ./test/core/simd/simd_f32x4.wast:2020
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      f32x4([-123456790, -123456790, -123456790, -123456790]),
      f32x4([-123456790, -123456790, -123456790, -123456790]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -123456790),
      value("f32", -123456790),
      value("f32", -123456790),
      value("f32", -123456790),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2023
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([0, 0, 0, 0]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2026
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2029
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2032
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2035
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2038
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2041
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2044
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2047
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([1, 1, 1, 1]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2050
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([-1, -1, -1, -1]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2053
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2056
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2059
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2062
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2065
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2068
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2071
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2074
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2077
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2080
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2083
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([0, 0, 0, 0]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2086
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2089
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2092
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2095
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2098
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2101
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2104
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2107
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([1, 1, 1, 1]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2110
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([-1, -1, -1, -1]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2113
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2116
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2119
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2122
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2125
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2128
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [
    new F32x4Pattern(
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2131
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2134
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
      `f32_canonical_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2137
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2140
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2143
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([0, 0, 0, 0]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2146
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2149
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2152
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2155
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2158
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2161
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2164
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2167
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([1, 1, 1, 1]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2170
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([-1, -1, -1, -1]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2173
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2176
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2179
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2182
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2185
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2188
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2191
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2194
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2197
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2200
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2203
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([0, 0, 0, 0]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2206
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2209
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2212
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2215
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2218
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2221
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([0.5, 0.5, 0.5, 0.5]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2224
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([-0.5, -0.5, -0.5, -0.5]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2227
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([1, 1, 1, 1]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2230
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([-1, -1, -1, -1]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2233
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2236
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2239
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
        340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2242
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
      ]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2245
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2248
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [
    new F32x4Pattern(
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2251
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2254
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2257
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2260
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
      `f32_arithmetic_nan`,
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2265
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
      bytes("v128", [
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
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
      bytes("v128", [
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x80,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2268
assert_return(
  () =>
    invoke($0, `f32x4.min`, [
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
      f32x4([0, 0, 0, 0]),
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

// ./test/core/simd/simd_f32x4.wast:2271
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
      bytes("v128", [
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
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
      bytes("v128", [
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x80,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x80,
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

// ./test/core/simd/simd_f32x4.wast:2274
assert_return(
  () =>
    invoke($0, `f32x4.max`, [
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
      f32x4([0, 0, 0, 0]),
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

// ./test/core/simd/simd_f32x4.wast:2279
assert_return(() => invoke($0, `f32x4.abs`, [f32x4([0, 0, 0, 0])]), [
  new F32x4Pattern(
    value("f32", 0),
    value("f32", 0),
    value("f32", 0),
    value("f32", 0),
  ),
]);

// ./test/core/simd/simd_f32x4.wast:2281
assert_return(
  () =>
    invoke($0, `f32x4.abs`, [
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
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2283
assert_return(
  () =>
    invoke($0, `f32x4.abs`, [
      f32x4([
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
        0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2285
assert_return(
  () =>
    invoke($0, `f32x4.abs`, [
      f32x4([
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
        -0.000000000000000000000000000000000000000000001,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2287
assert_return(
  () =>
    invoke($0, `f32x4.abs`, [
      f32x4([
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
        0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2289
assert_return(
  () =>
    invoke($0, `f32x4.abs`, [
      f32x4([
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
        -0.000000000000000000000000000000000000011754944,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2291
assert_return(() => invoke($0, `f32x4.abs`, [f32x4([0.5, 0.5, 0.5, 0.5])]), [
  new F32x4Pattern(
    value("f32", 0.5),
    value("f32", 0.5),
    value("f32", 0.5),
    value("f32", 0.5),
  ),
]);

// ./test/core/simd/simd_f32x4.wast:2293
assert_return(
  () => invoke($0, `f32x4.abs`, [f32x4([-0.5, -0.5, -0.5, -0.5])]),
  [
    new F32x4Pattern(
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
      value("f32", 0.5),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2295
assert_return(() => invoke($0, `f32x4.abs`, [f32x4([1, 1, 1, 1])]), [
  new F32x4Pattern(
    value("f32", 1),
    value("f32", 1),
    value("f32", 1),
    value("f32", 1),
  ),
]);

// ./test/core/simd/simd_f32x4.wast:2297
assert_return(() => invoke($0, `f32x4.abs`, [f32x4([-1, -1, -1, -1])]), [
  new F32x4Pattern(
    value("f32", 1),
    value("f32", 1),
    value("f32", 1),
    value("f32", 1),
  ),
]);

// ./test/core/simd/simd_f32x4.wast:2299
assert_return(
  () =>
    invoke($0, `f32x4.abs`, [
      f32x4([6.2831855, 6.2831855, 6.2831855, 6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2301
assert_return(
  () =>
    invoke($0, `f32x4.abs`, [
      f32x4([-6.2831855, -6.2831855, -6.2831855, -6.2831855]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
      value("f32", 6.2831855),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2303
assert_return(
  () =>
    invoke($0, `f32x4.abs`, [
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

// ./test/core/simd/simd_f32x4.wast:2305
assert_return(
  () =>
    invoke($0, `f32x4.abs`, [
      f32x4([
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
        -340282350000000000000000000000000000000,
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

// ./test/core/simd/simd_f32x4.wast:2307
assert_return(
  () =>
    invoke($0, `f32x4.abs`, [f32x4([Infinity, Infinity, Infinity, Infinity])]),
  [
    new F32x4Pattern(
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2309
assert_return(
  () =>
    invoke($0, `f32x4.abs`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
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

// ./test/core/simd/simd_f32x4.wast:2311
assert_return(
  () =>
    invoke($0, `f32x4.abs`, [
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

// ./test/core/simd/simd_f32x4.wast:2313
assert_return(
  () =>
    invoke($0, `f32x4.abs`, [
      f32x4([
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
        0.000000000012345679,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
      value("f32", 0.000000000012345679),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2315
assert_return(
  () =>
    invoke($0, `f32x4.abs`, [
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

// ./test/core/simd/simd_f32x4.wast:2317
assert_return(
  () =>
    invoke($0, `f32x4.abs`, [
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

// ./test/core/simd/simd_f32x4.wast:2319
assert_return(
  () =>
    invoke($0, `f32x4.abs`, [
      f32x4([-123456790, -123456790, -123456790, -123456790]),
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

// ./test/core/simd/simd_f32x4.wast:2325
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i8x16.min (v128.const i32x4 0 0 0 0) (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_f32x4.wast:2326
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i8x16.max (v128.const i32x4 0 0 0 0) (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_f32x4.wast:2327
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i16x8.min (v128.const i32x4 0 0 0 0) (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_f32x4.wast:2328
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i16x8.max (v128.const i32x4 0 0 0 0) (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_f32x4.wast:2329
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i32x4.min (v128.const i32x4 0 0 0 0) (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_f32x4.wast:2330
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i32x4.max (v128.const i32x4 0 0 0 0) (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_f32x4.wast:2331
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i64x2.min (v128.const i32x4 0 0 0 0) (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_f32x4.wast:2332
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (result v128) (i64x2.max (v128.const i32x4 0 0 0 0) (v128.const i32x4 0 0 0 0))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_f32x4.wast:2335
assert_invalid(
  () => instantiate(`(module (func (result v128) (f32x4.abs (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/simd/simd_f32x4.wast:2336
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (f32x4.min (i32.const 0) (f32.const 0.0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_f32x4.wast:2337
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (f32x4.max (i32.const 0) (f32.const 0.0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_f32x4.wast:2341
assert_invalid(() =>
  instantiate(`(module
    (func $$f32x4.abs-arg-empty (result v128)
      (f32x4.abs)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_f32x4.wast:2349
assert_invalid(() =>
  instantiate(`(module
    (func $$f32x4.min-1st-arg-empty (result v128)
      (f32x4.min (v128.const f32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_f32x4.wast:2357
assert_invalid(() =>
  instantiate(`(module
    (func $$f32x4.min-arg-empty (result v128)
      (f32x4.min)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_f32x4.wast:2365
assert_invalid(() =>
  instantiate(`(module
    (func $$f32x4.max-1st-arg-empty (result v128)
      (f32x4.max (v128.const f32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_f32x4.wast:2373
assert_invalid(() =>
  instantiate(`(module
    (func $$f32x4.max-arg-empty (result v128)
      (f32x4.max)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_f32x4.wast:2383
let $1 = instantiate(`(module
  (func (export "max-min") (param v128 v128 v128) (result v128)
    (f32x4.max (f32x4.min (local.get 0) (local.get 1))(local.get 2)))
  (func (export "min-max") (param v128 v128 v128) (result v128)
    (f32x4.min (f32x4.max (local.get 0) (local.get 1))(local.get 2)))
  (func (export "max-abs") (param v128 v128) (result v128)
    (f32x4.max (f32x4.abs (local.get 0)) (local.get 1)))
  (func (export "min-abs") (param v128 v128) (result v128)
    (f32x4.min (f32x4.abs (local.get 0)) (local.get 1)))
)`);

// ./test/core/simd/simd_f32x4.wast:2394
assert_return(
  () =>
    invoke($1, `max-min`, [
      f32x4([1.125, 1.125, 1.125, 1.125]),
      f32x4([0.25, 0.25, 0.25, 0.25]),
      f32x4([0.125, 0.125, 0.125, 0.125]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.25),
      value("f32", 0.25),
      value("f32", 0.25),
      value("f32", 0.25),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2398
assert_return(
  () =>
    invoke($1, `min-max`, [
      f32x4([1.125, 1.125, 1.125, 1.125]),
      f32x4([0.25, 0.25, 0.25, 0.25]),
      f32x4([0.125, 0.125, 0.125, 0.125]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.125),
      value("f32", 0.125),
      value("f32", 0.125),
      value("f32", 0.125),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2402
assert_return(
  () =>
    invoke($1, `max-abs`, [
      f32x4([-1.125, -1.125, -1.125, -1.125]),
      f32x4([0.125, 0.125, 0.125, 0.125]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 1.125),
      value("f32", 1.125),
      value("f32", 1.125),
      value("f32", 1.125),
    ),
  ],
);

// ./test/core/simd/simd_f32x4.wast:2405
assert_return(
  () =>
    invoke($1, `min-abs`, [
      f32x4([-1.125, -1.125, -1.125, -1.125]),
      f32x4([0.125, 0.125, 0.125, 0.125]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.125),
      value("f32", 0.125),
      value("f32", 0.125),
      value("f32", 0.125),
    ),
  ],
);
