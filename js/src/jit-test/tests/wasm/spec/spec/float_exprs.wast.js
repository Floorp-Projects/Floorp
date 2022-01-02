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

// ./test/core/float_exprs.wast

// ./test/core/float_exprs.wast:6
let $0 = instantiate(`(module
  (func (export "f64.no_contraction") (param $$x f64) (param $$y f64) (param $$z f64) (result f64)
    (f64.add (f64.mul (local.get $$x) (local.get $$y)) (local.get $$z)))
)`);

// ./test/core/float_exprs.wast:11
assert_return(
  () =>
    invoke($0, `f64.no_contraction`, [
      value("f64", -0.00000000000000000000000000000015967133604096234),
      value(
        "f64",
        87633521608271230000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        42896576204898460000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    -13992561434270632000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:12
assert_return(
  () =>
    invoke($0, `f64.no_contraction`, [
      value("f64", 8341016642481988),
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000003223424965918293,
      ),
      value(
        "f64",
        0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000023310835741659086,
      ),
    ]),
  [value(
    "f64",
    0.0000000000000000000000000000000000000000000000000000000000000000000000000000000026886641288847496,
  )],
);

// ./test/core/float_exprs.wast:13
assert_return(
  () =>
    invoke($0, `f64.no_contraction`, [
      value(
        "f64",
        -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000030119045290520013,
      ),
      value(
        "f64",
        52699336439236750000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value("f64", 0.00000000000000000000000000000006654454781339856),
    ]),
  [value("f64", -0.0000000000000000015872537009936566)],
);

// ./test/core/float_exprs.wast:14
assert_return(
  () =>
    invoke($0, `f64.no_contraction`, [
      value("f64", 0.0000000000000000000031413936116780743),
      value("f64", -0.0000000000000000000000000000007262766035707377),
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000004619684894228461,
      ),
    ]),
  [value(
    "f64",
    -0.00000000000000000000000000000000000000000000000000228152068276836,
  )],
);

// ./test/core/float_exprs.wast:15
assert_return(
  () =>
    invoke($0, `f64.no_contraction`, [
      value(
        "f64",
        -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000016080464217402378,
      ),
      value(
        "f64",
        -382103410226833000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value("f64", 0.00000000000000010541980504151345),
    ]),
  [value("f64", 0.00006144400215510552)],
);

// ./test/core/float_exprs.wast:19
let $1 = instantiate(`(module
  (func (export "f32.no_fma") (param $$x f32) (param $$y f32) (param $$z f32) (result f32)
    (f32.add (f32.mul (local.get $$x) (local.get $$y)) (local.get $$z)))
  (func (export "f64.no_fma") (param $$x f64) (param $$y f64) (param $$z f64) (result f64)
    (f64.add (f64.mul (local.get $$x) (local.get $$y)) (local.get $$z)))
)`);

// ./test/core/float_exprs.wast:26
assert_return(
  () =>
    invoke($1, `f32.no_fma`, [
      value("f32", 35184304000000000000000000000000000000),
      value("f32", 0.00000021584361),
      value("f32", 259340640000000000000000000000000),
    ]),
  [value("f32", 266934960000000000000000000000000)],
);

// ./test/core/float_exprs.wast:27
assert_return(
  () =>
    invoke($1, `f32.no_fma`, [
      value("f32", 0.0000000071753243),
      value("f32", -0.000000000000001225534),
      value("f32", 0.0000000000000000000000000041316436),
    ]),
  [value("f32", -0.0000000000000000000000087894724)],
);

// ./test/core/float_exprs.wast:28
assert_return(
  () =>
    invoke($1, `f32.no_fma`, [
      value("f32", 231063440000),
      value("f32", 0.00020773262),
      value("f32", 1797.6421),
    ]),
  [value("f32", 48001210)],
);

// ./test/core/float_exprs.wast:29
assert_return(
  () =>
    invoke($1, `f32.no_fma`, [
      value("f32", 0.0045542703),
      value("f32", -7265493.5),
      value("f32", -2.3964283),
    ]),
  [value("f32", -33091.414)],
);

// ./test/core/float_exprs.wast:30
assert_return(
  () =>
    invoke($1, `f32.no_fma`, [
      value("f32", 98881730000000000000000000000000000000),
      value("f32", -0.0000000000000000000008570631),
      value("f32", -21579143000),
    ]),
  [value("f32", -84747910000000000)],
);

// ./test/core/float_exprs.wast:31
assert_return(
  () =>
    invoke($1, `f64.no_fma`, [
      value(
        "f64",
        789084284375179200000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        4215020052117360000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        -1336601081131744700000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    1989405000320312800000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:32
assert_return(
  () =>
    invoke($1, `f64.no_fma`, [
      value(
        "f64",
        5586822348009285500000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value("f64", 0.0000000000000000000000000000000000000007397302005677334),
      value(
        "f64",
        36567834172040920000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    4132741216029240700000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:33
assert_return(
  () =>
    invoke($1, `f64.no_fma`, [
      value(
        "f64",
        0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000014260477822274587,
      ),
      value(
        "f64",
        -31087632036599860000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        343269235523777630000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    -4433244872049653000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:34
assert_return(
  () =>
    invoke($1, `f64.no_fma`, [
      value(
        "f64",
        -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000809034701735478,
      ),
      value(
        "f64",
        -24874417850667450000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value("f64", 49484765138510810),
    ]),
  [value("f64", 250727437405094720)],
);

// ./test/core/float_exprs.wast:35
assert_return(
  () =>
    invoke($1, `f64.no_fma`, [
      value("f64", 6723256985364377),
      value(
        "f64",
        285456566692879460000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        -5593839470050757000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    1919197856036028600000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:40
let $2 = instantiate(`(module
  (func (export "f32.no_fold_add_zero") (param $$x f32) (result f32)
    (f32.add (local.get $$x) (f32.const 0.0)))
  (func (export "f64.no_fold_add_zero") (param $$x f64) (result f64)
    (f64.add (local.get $$x) (f64.const 0.0)))
)`);

// ./test/core/float_exprs.wast:47
assert_return(() => invoke($2, `f32.no_fold_add_zero`, [value("f32", -0)]), [
  value("f32", 0),
]);

// ./test/core/float_exprs.wast:48
assert_return(() => invoke($2, `f64.no_fold_add_zero`, [value("f64", -0)]), [
  value("f64", 0),
]);

// ./test/core/float_exprs.wast:49
assert_return(
  () =>
    invoke($2, `f32.no_fold_add_zero`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/float_exprs.wast:50
assert_return(
  () =>
    invoke($2, `f64.no_fold_add_zero`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/float_exprs.wast:54
let $3 = instantiate(`(module
  (func (export "f32.no_fold_zero_sub") (param $$x f32) (result f32)
    (f32.sub (f32.const 0.0) (local.get $$x)))
  (func (export "f64.no_fold_zero_sub") (param $$x f64) (result f64)
    (f64.sub (f64.const 0.0) (local.get $$x)))
)`);

// ./test/core/float_exprs.wast:61
assert_return(() => invoke($3, `f32.no_fold_zero_sub`, [value("f32", 0)]), [
  value("f32", 0),
]);

// ./test/core/float_exprs.wast:62
assert_return(() => invoke($3, `f64.no_fold_zero_sub`, [value("f64", 0)]), [
  value("f64", 0),
]);

// ./test/core/float_exprs.wast:63
assert_return(
  () =>
    invoke($3, `f32.no_fold_zero_sub`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/float_exprs.wast:64
assert_return(
  () =>
    invoke($3, `f64.no_fold_zero_sub`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/float_exprs.wast:68
let $4 = instantiate(`(module
  (func (export "f32.no_fold_sub_zero") (param $$x f32) (result f32)
    (f32.sub (local.get $$x) (f32.const 0.0)))
  (func (export "f64.no_fold_sub_zero") (param $$x f64) (result f64)
    (f64.sub (local.get $$x) (f64.const 0.0)))
)`);

// ./test/core/float_exprs.wast:75
assert_return(
  () =>
    invoke($4, `f32.no_fold_sub_zero`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/float_exprs.wast:76
assert_return(
  () =>
    invoke($4, `f64.no_fold_sub_zero`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/float_exprs.wast:80
let $5 = instantiate(`(module
  (func (export "f32.no_fold_mul_zero") (param $$x f32) (result f32)
    (f32.mul (local.get $$x) (f32.const 0.0)))
  (func (export "f64.no_fold_mul_zero") (param $$x f64) (result f64)
    (f64.mul (local.get $$x) (f64.const 0.0)))
)`);

// ./test/core/float_exprs.wast:87
assert_return(() => invoke($5, `f32.no_fold_mul_zero`, [value("f32", -0)]), [
  value("f32", -0),
]);

// ./test/core/float_exprs.wast:88
assert_return(() => invoke($5, `f32.no_fold_mul_zero`, [value("f32", -1)]), [
  value("f32", -0),
]);

// ./test/core/float_exprs.wast:89
assert_return(() => invoke($5, `f32.no_fold_mul_zero`, [value("f32", -2)]), [
  value("f32", -0),
]);

// ./test/core/float_exprs.wast:90
assert_return(
  () =>
    invoke($5, `f32.no_fold_mul_zero`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/float_exprs.wast:91
assert_return(() => invoke($5, `f64.no_fold_mul_zero`, [value("f64", -0)]), [
  value("f64", -0),
]);

// ./test/core/float_exprs.wast:92
assert_return(() => invoke($5, `f64.no_fold_mul_zero`, [value("f64", -1)]), [
  value("f64", -0),
]);

// ./test/core/float_exprs.wast:93
assert_return(() => invoke($5, `f64.no_fold_mul_zero`, [value("f64", -2)]), [
  value("f64", -0),
]);

// ./test/core/float_exprs.wast:94
assert_return(
  () =>
    invoke($5, `f64.no_fold_mul_zero`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/float_exprs.wast:99
let $6 = instantiate(`(module
  (func (export "f32.no_fold_mul_one") (param $$x f32) (result f32)
    (f32.mul (local.get $$x) (f32.const 1.0)))
  (func (export "f64.no_fold_mul_one") (param $$x f64) (result f64)
    (f64.mul (local.get $$x) (f64.const 1.0)))
)`);

// ./test/core/float_exprs.wast:106
assert_return(
  () =>
    invoke($6, `f32.no_fold_mul_one`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/float_exprs.wast:107
assert_return(
  () =>
    invoke($6, `f64.no_fold_mul_one`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/float_exprs.wast:111
let $7 = instantiate(`(module
  (func (export "f32.no_fold_zero_div") (param $$x f32) (result f32)
    (f32.div (f32.const 0.0) (local.get $$x)))
  (func (export "f64.no_fold_zero_div") (param $$x f64) (result f64)
    (f64.div (f64.const 0.0) (local.get $$x)))
)`);

// ./test/core/float_exprs.wast:118
assert_return(() => invoke($7, `f32.no_fold_zero_div`, [value("f32", 0)]), [
  `canonical_nan`,
]);

// ./test/core/float_exprs.wast:119
assert_return(() => invoke($7, `f32.no_fold_zero_div`, [value("f32", -0)]), [
  `canonical_nan`,
]);

// ./test/core/float_exprs.wast:120
assert_return(
  () =>
    invoke($7, `f32.no_fold_zero_div`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/float_exprs.wast:121
assert_return(
  () =>
    invoke($7, `f32.no_fold_zero_div`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/float_exprs.wast:122
assert_return(() => invoke($7, `f64.no_fold_zero_div`, [value("f64", 0)]), [
  `canonical_nan`,
]);

// ./test/core/float_exprs.wast:123
assert_return(() => invoke($7, `f64.no_fold_zero_div`, [value("f64", -0)]), [
  `canonical_nan`,
]);

// ./test/core/float_exprs.wast:124
assert_return(
  () =>
    invoke($7, `f64.no_fold_zero_div`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/float_exprs.wast:125
assert_return(
  () =>
    invoke($7, `f64.no_fold_zero_div`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/float_exprs.wast:129
let $8 = instantiate(`(module
  (func (export "f32.no_fold_div_one") (param $$x f32) (result f32)
    (f32.div (local.get $$x) (f32.const 1.0)))
  (func (export "f64.no_fold_div_one") (param $$x f64) (result f64)
    (f64.div (local.get $$x) (f64.const 1.0)))
)`);

// ./test/core/float_exprs.wast:136
assert_return(
  () =>
    invoke($8, `f32.no_fold_div_one`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/float_exprs.wast:137
assert_return(
  () =>
    invoke($8, `f64.no_fold_div_one`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/float_exprs.wast:141
let $9 = instantiate(`(module
  (func (export "f32.no_fold_div_neg1") (param $$x f32) (result f32)
    (f32.div (local.get $$x) (f32.const -1.0)))
  (func (export "f64.no_fold_div_neg1") (param $$x f64) (result f64)
    (f64.div (local.get $$x) (f64.const -1.0)))
)`);

// ./test/core/float_exprs.wast:148
assert_return(
  () =>
    invoke($9, `f32.no_fold_div_neg1`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/float_exprs.wast:149
assert_return(
  () =>
    invoke($9, `f64.no_fold_div_neg1`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/float_exprs.wast:153
let $10 = instantiate(`(module
  (func (export "f32.no_fold_neg0_sub") (param $$x f32) (result f32)
    (f32.sub (f32.const -0.0) (local.get $$x)))
  (func (export "f64.no_fold_neg0_sub") (param $$x f64) (result f64)
    (f64.sub (f64.const -0.0) (local.get $$x)))
)`);

// ./test/core/float_exprs.wast:160
assert_return(
  () =>
    invoke($10, `f32.no_fold_neg0_sub`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/float_exprs.wast:161
assert_return(
  () =>
    invoke($10, `f64.no_fold_neg0_sub`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/float_exprs.wast:165
let $11 = instantiate(`(module
  (func (export "f32.no_fold_neg1_mul") (param $$x f32) (result f32)
    (f32.mul (f32.const -1.0) (local.get $$x)))
  (func (export "f64.no_fold_neg1_mul") (param $$x f64) (result f64)
    (f64.mul (f64.const -1.0) (local.get $$x)))
)`);

// ./test/core/float_exprs.wast:172
assert_return(
  () =>
    invoke($11, `f32.no_fold_neg1_mul`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/float_exprs.wast:173
assert_return(
  () =>
    invoke($11, `f64.no_fold_neg1_mul`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/float_exprs.wast:177
let $12 = instantiate(`(module
  (func (export "f32.no_fold_eq_self") (param $$x f32) (result i32)
    (f32.eq (local.get $$x) (local.get $$x)))
  (func (export "f64.no_fold_eq_self") (param $$x f64) (result i32)
    (f64.eq (local.get $$x) (local.get $$x)))
)`);

// ./test/core/float_exprs.wast:184
assert_return(
  () =>
    invoke($12, `f32.no_fold_eq_self`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:185
assert_return(
  () =>
    invoke($12, `f64.no_fold_eq_self`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:189
let $13 = instantiate(`(module
  (func (export "f32.no_fold_ne_self") (param $$x f32) (result i32)
    (f32.ne (local.get $$x) (local.get $$x)))
  (func (export "f64.no_fold_ne_self") (param $$x f64) (result i32)
    (f64.ne (local.get $$x) (local.get $$x)))
)`);

// ./test/core/float_exprs.wast:196
assert_return(
  () =>
    invoke($13, `f32.no_fold_ne_self`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:197
assert_return(
  () =>
    invoke($13, `f64.no_fold_ne_self`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:201
let $14 = instantiate(`(module
  (func (export "f32.no_fold_sub_self") (param $$x f32) (result f32)
    (f32.sub (local.get $$x) (local.get $$x)))
  (func (export "f64.no_fold_sub_self") (param $$x f64) (result f64)
    (f64.sub (local.get $$x) (local.get $$x)))
)`);

// ./test/core/float_exprs.wast:208
assert_return(
  () => invoke($14, `f32.no_fold_sub_self`, [value("f32", Infinity)]),
  [`canonical_nan`],
);

// ./test/core/float_exprs.wast:209
assert_return(
  () =>
    invoke($14, `f32.no_fold_sub_self`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/float_exprs.wast:210
assert_return(
  () => invoke($14, `f64.no_fold_sub_self`, [value("f64", Infinity)]),
  [`canonical_nan`],
);

// ./test/core/float_exprs.wast:211
assert_return(
  () =>
    invoke($14, `f64.no_fold_sub_self`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/float_exprs.wast:215
let $15 = instantiate(`(module
  (func (export "f32.no_fold_div_self") (param $$x f32) (result f32)
    (f32.div (local.get $$x) (local.get $$x)))
  (func (export "f64.no_fold_div_self") (param $$x f64) (result f64)
    (f64.div (local.get $$x) (local.get $$x)))
)`);

// ./test/core/float_exprs.wast:222
assert_return(
  () => invoke($15, `f32.no_fold_div_self`, [value("f32", Infinity)]),
  [`canonical_nan`],
);

// ./test/core/float_exprs.wast:223
assert_return(
  () =>
    invoke($15, `f32.no_fold_div_self`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/float_exprs.wast:224
assert_return(() => invoke($15, `f32.no_fold_div_self`, [value("f32", 0)]), [
  `canonical_nan`,
]);

// ./test/core/float_exprs.wast:225
assert_return(() => invoke($15, `f32.no_fold_div_self`, [value("f32", -0)]), [
  `canonical_nan`,
]);

// ./test/core/float_exprs.wast:226
assert_return(
  () => invoke($15, `f64.no_fold_div_self`, [value("f64", Infinity)]),
  [`canonical_nan`],
);

// ./test/core/float_exprs.wast:227
assert_return(
  () =>
    invoke($15, `f64.no_fold_div_self`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/float_exprs.wast:228
assert_return(() => invoke($15, `f64.no_fold_div_self`, [value("f64", 0)]), [
  `canonical_nan`,
]);

// ./test/core/float_exprs.wast:229
assert_return(() => invoke($15, `f64.no_fold_div_self`, [value("f64", -0)]), [
  `canonical_nan`,
]);

// ./test/core/float_exprs.wast:233
let $16 = instantiate(`(module
  (func (export "f32.no_fold_div_3") (param $$x f32) (result f32)
    (f32.div (local.get $$x) (f32.const 3.0)))
  (func (export "f64.no_fold_div_3") (param $$x f64) (result f64)
    (f64.div (local.get $$x) (f64.const 3.0)))
)`);

// ./test/core/float_exprs.wast:240
assert_return(
  () => invoke($16, `f32.no_fold_div_3`, [value("f32", -1361679000000000)]),
  [value("f32", -453892980000000)],
);

// ./test/core/float_exprs.wast:241
assert_return(
  () =>
    invoke($16, `f32.no_fold_div_3`, [
      value("f32", -18736880000000000000000000000),
    ]),
  [value("f32", -6245626600000000000000000000)],
);

// ./test/core/float_exprs.wast:242
assert_return(
  () =>
    invoke($16, `f32.no_fold_div_3`, [
      value("f32", -0.00000000000000000000000012045131),
    ]),
  [value("f32", -0.000000000000000000000000040150435)],
);

// ./test/core/float_exprs.wast:243
assert_return(
  () =>
    invoke($16, `f32.no_fold_div_3`, [
      value("f32", -0.00000000000000000000000000000000000005281346),
    ]),
  [value("f32", -0.000000000000000000000000000000000000017604486)],
);

// ./test/core/float_exprs.wast:244
assert_return(
  () =>
    invoke($16, `f32.no_fold_div_3`, [
      value("f32", -0.000000000000000025495563),
    ]),
  [value("f32", -0.000000000000000008498521)],
);

// ./test/core/float_exprs.wast:245
assert_return(
  () =>
    invoke($16, `f64.no_fold_div_3`, [
      value(
        "f64",
        -29563579573969634000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    -9854526524656545000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:246
assert_return(
  () =>
    invoke($16, `f64.no_fold_div_3`, [
      value(
        "f64",
        -0.000000000000000000000000000000000000000000000000009291150921449772,
      ),
    ]),
  [value(
    "f64",
    -0.000000000000000000000000000000000000000000000000003097050307149924,
  )],
);

// ./test/core/float_exprs.wast:247
assert_return(
  () =>
    invoke($16, `f64.no_fold_div_3`, [
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000013808061543557006,
      ),
    ]),
  [value(
    "f64",
    0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000004602687181185669,
  )],
);

// ./test/core/float_exprs.wast:248
assert_return(
  () =>
    invoke($16, `f64.no_fold_div_3`, [
      value(
        "f64",
        -1378076163468349000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    -459358721156116300000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:249
assert_return(
  () =>
    invoke($16, `f64.no_fold_div_3`, [
      value(
        "f64",
        86324008088313660000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    28774669362771220000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:253
let $17 = instantiate(`(module
  (func (export "f32.no_factor") (param $$x f32) (param $$y f32) (param $$z f32) (result f32)
    (f32.add (f32.mul (local.get $$x) (local.get $$z)) (f32.mul (local.get $$y) (local.get $$z))))
  (func (export "f64.no_factor") (param $$x f64) (param $$y f64) (param $$z f64) (result f64)
    (f64.add (f64.mul (local.get $$x) (local.get $$z)) (f64.mul (local.get $$y) (local.get $$z))))
)`);

// ./test/core/float_exprs.wast:260
assert_return(
  () =>
    invoke($17, `f32.no_factor`, [
      value("f32", -1435111700000),
      value("f32", -853617640000000),
      value("f32", 1113849300000000000),
    ]),
  [value("f32", -952399900000000000000000000000000)],
);

// ./test/core/float_exprs.wast:261
assert_return(
  () =>
    invoke($17, `f32.no_factor`, [
      value("f32", -0.026666632),
      value("f32", 0.048412822),
      value("f32", -0.002813697),
    ]),
  [value("f32", -0.0000611872)],
);

// ./test/core/float_exprs.wast:262
assert_return(
  () =>
    invoke($17, `f32.no_factor`, [
      value("f32", -0.00000000000046619777),
      value("f32", 0.00000000000000000010478377),
      value("f32", 14469202000000000000000000000000000000),
    ]),
  [value("f32", -6745508000000000000000000)],
);

// ./test/core/float_exprs.wast:263
assert_return(
  () =>
    invoke($17, `f32.no_factor`, [
      value("f32", -0.00000000000000000010689046),
      value("f32", 0.00000000000000000000000010694433),
      value("f32", 568307000000000000000000000000000000),
    ]),
  [value("f32", -60746540000000000)],
);

// ./test/core/float_exprs.wast:264
assert_return(
  () =>
    invoke($17, `f32.no_factor`, [
      value("f32", -0.000000000000000000000000063545994),
      value("f32", 0.0000000000000000000007524625),
      value("f32", 1626770.3),
    ]),
  [value("f32", 0.0000000000000012239803)],
);

// ./test/core/float_exprs.wast:265
assert_return(
  () =>
    invoke($17, `f64.no_factor`, [
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000028390554709988774,
      ),
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001473981250649641,
      ),
      value(
        "f64",
        -0.000000000000000000000000000000000000000000000000000000000000000000029001229846550766,
      ),
    ]),
  [value(
    "f64",
    -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000008233610026197336,
  )],
);

// ./test/core/float_exprs.wast:266
assert_return(
  () =>
    invoke($17, `f64.no_factor`, [
      value(
        "f64",
        0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000006461015505916123,
      ),
      value(
        "f64",
        -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000023923242802975938,
      ),
      value(
        "f64",
        -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000015300738798561604,
      ),
    ]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:267
assert_return(
  () =>
    invoke($17, `f64.no_factor`, [
      value(
        "f64",
        -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002939056292080733,
      ),
      value(
        "f64",
        -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000002146156743463356,
      ),
      value(
        "f64",
        -2510967223130241600000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value("f64", 538892923853642600000000000000000000000000000000000000000000)],
);

// ./test/core/float_exprs.wast:268
assert_return(
  () =>
    invoke($17, `f64.no_factor`, [
      value(
        "f64",
        -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000017785466771708878,
      ),
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000009328516775403213,
      ),
      value(
        "f64",
        -0.00000000000000000000000000000000000000000000000000000000000000000000000000012121009044876735,
      ),
    ]),
  [value(
    "f64",
    -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001130710359943689,
  )],
);

// ./test/core/float_exprs.wast:269
assert_return(
  () =>
    invoke($17, `f64.no_factor`, [
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000015194859063177362,
      ),
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000883589921438065,
      ),
      value(
        "f64",
        -1735830019469195800000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value("f64", -0.0000000000000000000000000015337619131701908)],
);

// ./test/core/float_exprs.wast:273
let $18 = instantiate(`(module
  (func (export "f32.no_distribute") (param $$x f32) (param $$y f32) (param $$z f32) (result f32)
    (f32.mul (f32.add (local.get $$x) (local.get $$y)) (local.get $$z)))
  (func (export "f64.no_distribute") (param $$x f64) (param $$y f64) (param $$z f64) (result f64)
    (f64.mul (f64.add (local.get $$x) (local.get $$y)) (local.get $$z)))
)`);

// ./test/core/float_exprs.wast:280
assert_return(
  () =>
    invoke($18, `f32.no_distribute`, [
      value("f32", -1435111700000),
      value("f32", -853617640000000),
      value("f32", 1113849300000000000),
    ]),
  [value("f32", -952400000000000000000000000000000)],
);

// ./test/core/float_exprs.wast:281
assert_return(
  () =>
    invoke($18, `f32.no_distribute`, [
      value("f32", -0.026666632),
      value("f32", 0.048412822),
      value("f32", -0.002813697),
    ]),
  [value("f32", -0.000061187195)],
);

// ./test/core/float_exprs.wast:282
assert_return(
  () =>
    invoke($18, `f32.no_distribute`, [
      value("f32", -0.00000000000046619777),
      value("f32", 0.00000000000000000010478377),
      value("f32", 14469202000000000000000000000000000000),
    ]),
  [value("f32", -6745508500000000000000000)],
);

// ./test/core/float_exprs.wast:283
assert_return(
  () =>
    invoke($18, `f32.no_distribute`, [
      value("f32", -0.00000000000000000010689046),
      value("f32", 0.00000000000000000000000010694433),
      value("f32", 568307000000000000000000000000000000),
    ]),
  [value("f32", -60746536000000000)],
);

// ./test/core/float_exprs.wast:284
assert_return(
  () =>
    invoke($18, `f32.no_distribute`, [
      value("f32", -0.000000000000000000000000063545994),
      value("f32", 0.0000000000000000000007524625),
      value("f32", 1626770.3),
    ]),
  [value("f32", 0.0000000000000012239802)],
);

// ./test/core/float_exprs.wast:285
assert_return(
  () =>
    invoke($18, `f64.no_distribute`, [
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000028390554709988774,
      ),
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001473981250649641,
      ),
      value(
        "f64",
        -0.000000000000000000000000000000000000000000000000000000000000000000029001229846550766,
      ),
    ]),
  [value(
    "f64",
    -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000008233610026197337,
  )],
);

// ./test/core/float_exprs.wast:286
assert_return(
  () =>
    invoke($18, `f64.no_distribute`, [
      value(
        "f64",
        0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000006461015505916123,
      ),
      value(
        "f64",
        -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000023923242802975938,
      ),
      value(
        "f64",
        -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000015300738798561604,
      ),
    ]),
  [value("f64", -0)],
);

// ./test/core/float_exprs.wast:287
assert_return(
  () =>
    invoke($18, `f64.no_distribute`, [
      value(
        "f64",
        -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002939056292080733,
      ),
      value(
        "f64",
        -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000002146156743463356,
      ),
      value(
        "f64",
        -2510967223130241600000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value("f64", 538892923853642500000000000000000000000000000000000000000000)],
);

// ./test/core/float_exprs.wast:288
assert_return(
  () =>
    invoke($18, `f64.no_distribute`, [
      value(
        "f64",
        -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000017785466771708878,
      ),
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000009328516775403213,
      ),
      value(
        "f64",
        -0.00000000000000000000000000000000000000000000000000000000000000000000000000012121009044876735,
      ),
    ]),
  [value(
    "f64",
    -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000011307103599436889,
  )],
);

// ./test/core/float_exprs.wast:289
assert_return(
  () =>
    invoke($18, `f64.no_distribute`, [
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000015194859063177362,
      ),
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000883589921438065,
      ),
      value(
        "f64",
        -1735830019469195800000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value("f64", -0.0000000000000000000000000015337619131701907)],
);

// ./test/core/float_exprs.wast:293
let $19 = instantiate(`(module
  (func (export "f32.no_regroup_div_mul") (param $$x f32) (param $$y f32) (param $$z f32) (result f32)
    (f32.mul (local.get $$x) (f32.div (local.get $$y) (local.get $$z))))
  (func (export "f64.no_regroup_div_mul") (param $$x f64) (param $$y f64) (param $$z f64) (result f64)
    (f64.mul (local.get $$x) (f64.div (local.get $$y) (local.get $$z))))
)`);

// ./test/core/float_exprs.wast:300
assert_return(
  () =>
    invoke($19, `f32.no_regroup_div_mul`, [
      value("f32", -0.00000000000000000000000000000000002831349),
      value("f32", -0.00000000000000000007270787),
      value("f32", 0.000000000000000000000000000000000016406605),
    ]),
  [value("f32", 0.00000000000000000012547468)],
);

// ./test/core/float_exprs.wast:301
assert_return(
  () =>
    invoke($19, `f32.no_regroup_div_mul`, [
      value("f32", -3145897700000000000000000000),
      value("f32", -0.000000000000000000000000000000000040864003),
      value("f32", -9245928300000000000000),
    ]),
  [value("f32", -0)],
);

// ./test/core/float_exprs.wast:302
assert_return(
  () =>
    invoke($19, `f32.no_regroup_div_mul`, [
      value("f32", -93157.43),
      value("f32", -0.00000081292654),
      value("f32", -0.00000000000000000000000000000000000015469397),
    ]),
  [value("f32", -489548120000000000000000000000000000)],
);

// ./test/core/float_exprs.wast:303
assert_return(
  () =>
    invoke($19, `f32.no_regroup_div_mul`, [
      value("f32", -0.00000000000000000000000000008899643),
      value("f32", 17887725000000000000000),
      value("f32", 514680230000000000000),
    ]),
  [value("f32", -0.000000000000000000000000003093073)],
);

// ./test/core/float_exprs.wast:304
assert_return(
  () =>
    invoke($19, `f32.no_regroup_div_mul`, [
      value("f32", 9222036000000000000000000000000000),
      value("f32", 33330492),
      value("f32", -3253108800000000000000),
    ]),
  [value("f32", -94486550000000000000)],
);

// ./test/core/float_exprs.wast:305
assert_return(
  () =>
    invoke($19, `f64.no_regroup_div_mul`, [
      value(
        "f64",
        0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005698811412550059,
      ),
      value("f64", -0.0000000000000000000000000000000000018313439132919336),
      value(
        "f64",
        0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000009543270551003098,
      ),
    ]),
  [value("f64", -1093596114413331000000000000000)],
);

// ./test/core/float_exprs.wast:306
assert_return(
  () =>
    invoke($19, `f64.no_regroup_div_mul`, [
      value(
        "f64",
        357289288425507550000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000003952760844538651,
      ),
      value(
        "f64",
        -1450781241254900800000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value("f64", -0)],
);

// ./test/core/float_exprs.wast:307
assert_return(
  () =>
    invoke($19, `f64.no_regroup_div_mul`, [
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000009124278383497107,
      ),
      value(
        "f64",
        55561345277147970000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000025090826940306507,
      ),
    ]),
  [value("f64", Infinity)],
);

// ./test/core/float_exprs.wast:308
assert_return(
  () =>
    invoke($19, `f64.no_regroup_div_mul`, [
      value(
        "f64",
        -4492093000352015000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        -12087878984017852000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        -596613380626062300000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    -91013507803376260000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:309
assert_return(
  () =>
    invoke($19, `f64.no_regroup_div_mul`, [
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000007470269158630455,
      ),
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000007568026329781282,
      ),
      value(
        "f64",
        -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001055389683973521,
      ),
    ]),
  [value(
    "f64",
    -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005356807494101561,
  )],
);

// ./test/core/float_exprs.wast:313
let $20 = instantiate(`(module
  (func (export "f32.no_regroup_mul_div") (param $$x f32) (param $$y f32) (param $$z f32) (result f32)
    (f32.div (f32.mul (local.get $$x) (local.get $$y)) (local.get $$z)))
  (func (export "f64.no_regroup_mul_div") (param $$x f64) (param $$y f64) (param $$z f64) (result f64)
    (f64.div (f64.mul (local.get $$x) (local.get $$y)) (local.get $$z)))
)`);

// ./test/core/float_exprs.wast:320
assert_return(
  () =>
    invoke($20, `f32.no_regroup_mul_div`, [
      value("f32", -0.00000000000000000000000000000000002831349),
      value("f32", -0.00000000000000000007270787),
      value("f32", 0.000000000000000000000000000000000016406605),
    ]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:321
assert_return(
  () =>
    invoke($20, `f32.no_regroup_mul_div`, [
      value("f32", -3145897700000000000000000000),
      value("f32", -0.000000000000000000000000000000000040864003),
      value("f32", -9245928300000000000000),
    ]),
  [value("f32", -0.000000000000000000000000000013903848)],
);

// ./test/core/float_exprs.wast:322
assert_return(
  () =>
    invoke($20, `f32.no_regroup_mul_div`, [
      value("f32", -93157.43),
      value("f32", -0.00000081292654),
      value("f32", -0.00000000000000000000000000000000000015469397),
    ]),
  [value("f32", -489548160000000000000000000000000000)],
);

// ./test/core/float_exprs.wast:323
assert_return(
  () =>
    invoke($20, `f32.no_regroup_mul_div`, [
      value("f32", -0.00000000000000000000000000008899643),
      value("f32", 17887725000000000000000),
      value("f32", 514680230000000000000),
    ]),
  [value("f32", -0.0000000000000000000000000030930732)],
);

// ./test/core/float_exprs.wast:324
assert_return(
  () =>
    invoke($20, `f32.no_regroup_mul_div`, [
      value("f32", 9222036000000000000000000000000000),
      value("f32", 33330492),
      value("f32", -3253108800000000000000),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/float_exprs.wast:325
assert_return(
  () =>
    invoke($20, `f64.no_regroup_mul_div`, [
      value(
        "f64",
        0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005698811412550059,
      ),
      value("f64", -0.0000000000000000000000000000000000018313439132919336),
      value(
        "f64",
        0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000009543270551003098,
      ),
    ]),
  [value("f64", -1093596114413331100000000000000)],
);

// ./test/core/float_exprs.wast:326
assert_return(
  () =>
    invoke($20, `f64.no_regroup_mul_div`, [
      value(
        "f64",
        357289288425507550000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000003952760844538651,
      ),
      value(
        "f64",
        -1450781241254900800000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000009734611044734945,
  )],
);

// ./test/core/float_exprs.wast:327
assert_return(
  () =>
    invoke($20, `f64.no_regroup_mul_div`, [
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000009124278383497107,
      ),
      value(
        "f64",
        55561345277147970000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000025090826940306507,
      ),
    ]),
  [value(
    "f64",
    20204881364667663000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:328
assert_return(
  () =>
    invoke($20, `f64.no_regroup_mul_div`, [
      value(
        "f64",
        -4492093000352015000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        -12087878984017852000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        -596613380626062300000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value("f64", -Infinity)],
);

// ./test/core/float_exprs.wast:329
assert_return(
  () =>
    invoke($20, `f64.no_regroup_mul_div`, [
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000007470269158630455,
      ),
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000007568026329781282,
      ),
      value(
        "f64",
        -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001055389683973521,
      ),
    ]),
  [value("f64", -0)],
);

// ./test/core/float_exprs.wast:333
let $21 = instantiate(`(module
  (func (export "f32.no_reassociate_add") (param $$x f32) (param $$y f32) (param $$z f32) (param $$w f32) (result f32)
    (f32.add (f32.add (f32.add (local.get $$x) (local.get $$y)) (local.get $$z)) (local.get $$w)))
  (func (export "f64.no_reassociate_add") (param $$x f64) (param $$y f64) (param $$z f64) (param $$w f64) (result f64)
    (f64.add (f64.add (f64.add (local.get $$x) (local.get $$y)) (local.get $$z)) (local.get $$w)))
)`);

// ./test/core/float_exprs.wast:340
assert_return(
  () =>
    invoke($21, `f32.no_reassociate_add`, [
      value("f32", -24154321000000),
      value("f32", 26125812000),
      value("f32", -238608080000000),
      value("f32", -2478953500000),
    ]),
  [value("f32", -265215220000000)],
);

// ./test/core/float_exprs.wast:341
assert_return(
  () =>
    invoke($21, `f32.no_reassociate_add`, [
      value("f32", 0.0036181053),
      value("f32", -0.00985944),
      value("f32", 0.063375376),
      value("f32", -0.011150199),
    ]),
  [value("f32", 0.04598384)],
);

// ./test/core/float_exprs.wast:342
assert_return(
  () =>
    invoke($21, `f32.no_reassociate_add`, [
      value("f32", -34206968000),
      value("f32", -3770877200000),
      value("f32", 30868425000000),
      value("f32", 421132080000),
    ]),
  [value("f32", 27484470000000)],
);

// ./test/core/float_exprs.wast:343
assert_return(
  () =>
    invoke($21, `f32.no_reassociate_add`, [
      value("f32", 153506400000000),
      value("f32", 925114700000000),
      value("f32", -36021854000),
      value("f32", 2450846000000000),
    ]),
  [value("f32", 3529431000000000)],
);

// ./test/core/float_exprs.wast:344
assert_return(
  () =>
    invoke($21, `f32.no_reassociate_add`, [
      value("f32", 470600300000000000000000000000000),
      value("f32", -396552040000000000000000000000000),
      value("f32", 48066940000000000000000000000000),
      value("f32", -35644073000000000000000000000),
    ]),
  [value("f32", 122079560000000000000000000000000)],
);

// ./test/core/float_exprs.wast:345
assert_return(
  () =>
    invoke($21, `f64.no_reassociate_add`, [
      value(
        "f64",
        -20704652927717020000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        1594689704376369700000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        451106636559416130000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        -1374333509186863300000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    -921652887575998600000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:346
assert_return(
  () =>
    invoke($21, `f64.no_reassociate_add`, [
      value(
        "f64",
        0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000003485747658213531,
      ),
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000031210957391311754,
      ),
      value(
        "f64",
        -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000683008546431621,
      ),
      value(
        "f64",
        -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002617177347131095,
      ),
    ]),
  [value(
    "f64",
    -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000022324206697150536,
  )],
);

// ./test/core/float_exprs.wast:347
assert_return(
  () =>
    invoke($21, `f64.no_reassociate_add`, [
      value(
        "f64",
        -5412584921122726300000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        597603656170379500000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        -355830077793396300000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        373627259957625440000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    -5768414998318146000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:348
assert_return(
  () =>
    invoke($21, `f64.no_reassociate_add`, [
      value(
        "f64",
        -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000006469047714189599,
      ),
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000064286584974746,
      ),
      value(
        "f64",
        -0.00000000000000000000000000000000000000000000000000000000000000000000000000000021277698072285604,
      ),
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000008768287273189493,
      ),
    ]),
  [value(
    "f64",
    0.0000000000000000000000000000000000000000000000000000000000000000000000000000006640517465960996,
  )],
);

// ./test/core/float_exprs.wast:349
assert_return(
  () =>
    invoke($21, `f64.no_reassociate_add`, [
      value(
        "f64",
        -16422137086414828000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        -88032137939790710000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        449957059782857850000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        -114091267166274390000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    319443655442136560000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:353
let $22 = instantiate(`(module
  (func (export "f32.no_reassociate_mul") (param $$x f32) (param $$y f32) (param $$z f32) (param $$w f32) (result f32)
    (f32.mul (f32.mul (f32.mul (local.get $$x) (local.get $$y)) (local.get $$z)) (local.get $$w)))
  (func (export "f64.no_reassociate_mul") (param $$x f64) (param $$y f64) (param $$z f64) (param $$w f64) (result f64)
    (f64.mul (f64.mul (f64.mul (local.get $$x) (local.get $$y)) (local.get $$z)) (local.get $$w)))
)`);

// ./test/core/float_exprs.wast:360
assert_return(
  () =>
    invoke($22, `f32.no_reassociate_mul`, [
      value("f32", 0.00000000000000000000000000000000001904515),
      value("f32", 0.00000000022548861),
      value("f32", -6964322000000000000000000000000),
      value("f32", 0.000000000000000026902832),
    ]),
  [value("f32", -0.00000000000000000000000000000078764173)],
);

// ./test/core/float_exprs.wast:361
assert_return(
  () =>
    invoke($22, `f32.no_reassociate_mul`, [
      value("f32", 0.000000000000000018733125),
      value("f32", -7565904000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000030807684),
      value("f32", -1592759200000000000000),
    ]),
  [value("f32", -0.0069547286)],
);

// ./test/core/float_exprs.wast:362
assert_return(
  () =>
    invoke($22, `f32.no_reassociate_mul`, [
      value("f32", 0.0000000000000050355575),
      value("f32", -56466884000000000),
      value("f32", -0.0000000000011740512),
      value("f32", 84984730000000000000000),
    ]),
  [value("f32", 28370654000000)],
);

// ./test/core/float_exprs.wast:363
assert_return(
  () =>
    invoke($22, `f32.no_reassociate_mul`, [
      value("f32", 0.000000000000000000000000000000046394946),
      value("f32", 254449360000000000000000),
      value("f32", -72460980000000000),
      value("f32", -962511040000000000),
    ]),
  [value("f32", 823345100000000000000000000)],
);

// ./test/core/float_exprs.wast:364
assert_return(
  () =>
    invoke($22, `f32.no_reassociate_mul`, [
      value("f32", -0.0000000000000000000000000000019420536),
      value("f32", 0.0000000000000023200355),
      value("f32", -9.772748),
      value("f32", 864066000000000000),
    ]),
  [value("f32", 0.000000000000000000000000035113616)],
);

// ./test/core/float_exprs.wast:365
assert_return(
  () =>
    invoke($22, `f64.no_reassociate_mul`, [
      value(
        "f64",
        -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000003109868654414946,
      ),
      value(
        "f64",
        -20713190487745434000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000007162612845524978,
      ),
      value(
        "f64",
        -88478253295969090000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    40822261813278614000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:366
assert_return(
  () =>
    invoke($22, `f64.no_reassociate_mul`, [
      value(
        "f64",
        60442716412956810000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000006700545015107397,
      ),
      value(
        "f64",
        -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000031469664275798185,
      ),
      value(
        "f64",
        -6401677295640561500000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000008159057447560471,
  )],
);

// ./test/core/float_exprs.wast:367
assert_return(
  () =>
    invoke($22, `f64.no_reassociate_mul`, [
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002216807720454268,
      ),
      value(
        "f64",
        -1802234186536721600000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000007500283778521931,
      ),
      value("f64", -414412152433956900000000000),
    ]),
  [value(
    "f64",
    0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001241793627299937,
  )],
);

// ./test/core/float_exprs.wast:368
assert_return(
  () =>
    invoke($22, `f64.no_reassociate_mul`, [
      value(
        "f64",
        24318065966298720000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000006508014075793436,
      ),
      value(
        "f64",
        17596421287233897000000000000000000000000000000000000000000000000000,
      ),
      value("f64", -0.0000001416141401305358),
    ]),
  [value(
    "f64",
    -3943741918531223000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:369
assert_return(
  () =>
    invoke($22, `f64.no_reassociate_mul`, [
      value(
        "f64",
        0.00000000000000000000000000000000000000000000000000000000000000000000003849767156964772,
      ),
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000070008754943224875,
      ),
      value(
        "f64",
        -2536887825218386500000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000006101114518858449,
      ),
    ]),
  [value(
    "f64",
    0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000004171548126376117,
  )],
);

// ./test/core/float_exprs.wast:373
let $23 = instantiate(`(module
  (func (export "f32.no_fold_div_0") (param $$x f32) (result f32)
    (f32.div (local.get $$x) (f32.const 0.0)))
  (func (export "f64.no_fold_div_0") (param $$x f64) (result f64)
    (f64.div (local.get $$x) (f64.const 0.0)))
)`);

// ./test/core/float_exprs.wast:380
assert_return(() => invoke($23, `f32.no_fold_div_0`, [value("f32", 1)]), [
  value("f32", Infinity),
]);

// ./test/core/float_exprs.wast:381
assert_return(() => invoke($23, `f32.no_fold_div_0`, [value("f32", -1)]), [
  value("f32", -Infinity),
]);

// ./test/core/float_exprs.wast:382
assert_return(
  () => invoke($23, `f32.no_fold_div_0`, [value("f32", Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/float_exprs.wast:383
assert_return(
  () => invoke($23, `f32.no_fold_div_0`, [value("f32", -Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/float_exprs.wast:384
assert_return(() => invoke($23, `f32.no_fold_div_0`, [value("f32", 0)]), [
  `canonical_nan`,
]);

// ./test/core/float_exprs.wast:385
assert_return(() => invoke($23, `f32.no_fold_div_0`, [value("f32", -0)]), [
  `canonical_nan`,
]);

// ./test/core/float_exprs.wast:386
assert_return(
  () =>
    invoke($23, `f32.no_fold_div_0`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/float_exprs.wast:387
assert_return(
  () =>
    invoke($23, `f32.no_fold_div_0`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/float_exprs.wast:388
assert_return(() => invoke($23, `f64.no_fold_div_0`, [value("f64", 1)]), [
  value("f64", Infinity),
]);

// ./test/core/float_exprs.wast:389
assert_return(() => invoke($23, `f64.no_fold_div_0`, [value("f64", -1)]), [
  value("f64", -Infinity),
]);

// ./test/core/float_exprs.wast:390
assert_return(
  () => invoke($23, `f64.no_fold_div_0`, [value("f64", Infinity)]),
  [value("f64", Infinity)],
);

// ./test/core/float_exprs.wast:391
assert_return(
  () => invoke($23, `f64.no_fold_div_0`, [value("f64", -Infinity)]),
  [value("f64", -Infinity)],
);

// ./test/core/float_exprs.wast:392
assert_return(() => invoke($23, `f64.no_fold_div_0`, [value("f64", 0)]), [
  `canonical_nan`,
]);

// ./test/core/float_exprs.wast:393
assert_return(() => invoke($23, `f64.no_fold_div_0`, [value("f64", -0)]), [
  `canonical_nan`,
]);

// ./test/core/float_exprs.wast:394
assert_return(
  () =>
    invoke($23, `f64.no_fold_div_0`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/float_exprs.wast:395
assert_return(
  () =>
    invoke($23, `f64.no_fold_div_0`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/float_exprs.wast:399
let $24 = instantiate(`(module
  (func (export "f32.no_fold_div_neg0") (param $$x f32) (result f32)
    (f32.div (local.get $$x) (f32.const -0.0)))
  (func (export "f64.no_fold_div_neg0") (param $$x f64) (result f64)
    (f64.div (local.get $$x) (f64.const -0.0)))
)`);

// ./test/core/float_exprs.wast:406
assert_return(() => invoke($24, `f32.no_fold_div_neg0`, [value("f32", 1)]), [
  value("f32", -Infinity),
]);

// ./test/core/float_exprs.wast:407
assert_return(() => invoke($24, `f32.no_fold_div_neg0`, [value("f32", -1)]), [
  value("f32", Infinity),
]);

// ./test/core/float_exprs.wast:408
assert_return(
  () => invoke($24, `f32.no_fold_div_neg0`, [value("f32", Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/float_exprs.wast:409
assert_return(
  () => invoke($24, `f32.no_fold_div_neg0`, [value("f32", -Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/float_exprs.wast:410
assert_return(() => invoke($24, `f32.no_fold_div_neg0`, [value("f32", 0)]), [
  `canonical_nan`,
]);

// ./test/core/float_exprs.wast:411
assert_return(() => invoke($24, `f32.no_fold_div_neg0`, [value("f32", -0)]), [
  `canonical_nan`,
]);

// ./test/core/float_exprs.wast:412
assert_return(
  () =>
    invoke($24, `f32.no_fold_div_neg0`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/float_exprs.wast:413
assert_return(
  () =>
    invoke($24, `f32.no_fold_div_neg0`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/float_exprs.wast:414
assert_return(() => invoke($24, `f64.no_fold_div_neg0`, [value("f64", 1)]), [
  value("f64", -Infinity),
]);

// ./test/core/float_exprs.wast:415
assert_return(() => invoke($24, `f64.no_fold_div_neg0`, [value("f64", -1)]), [
  value("f64", Infinity),
]);

// ./test/core/float_exprs.wast:416
assert_return(
  () => invoke($24, `f64.no_fold_div_neg0`, [value("f64", Infinity)]),
  [value("f64", -Infinity)],
);

// ./test/core/float_exprs.wast:417
assert_return(
  () => invoke($24, `f64.no_fold_div_neg0`, [value("f64", -Infinity)]),
  [value("f64", Infinity)],
);

// ./test/core/float_exprs.wast:418
assert_return(() => invoke($24, `f64.no_fold_div_neg0`, [value("f64", 0)]), [
  `canonical_nan`,
]);

// ./test/core/float_exprs.wast:419
assert_return(() => invoke($24, `f64.no_fold_div_neg0`, [value("f64", -0)]), [
  `canonical_nan`,
]);

// ./test/core/float_exprs.wast:420
assert_return(
  () =>
    invoke($24, `f64.no_fold_div_neg0`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/float_exprs.wast:421
assert_return(
  () =>
    invoke($24, `f64.no_fold_div_neg0`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/float_exprs.wast:425
let $25 = instantiate(`(module
  (func (export "f32.no_fold_to_hypot") (param $$x f32) (param $$y f32) (result f32)
    (f32.sqrt (f32.add (f32.mul (local.get $$x) (local.get $$x))
                       (f32.mul (local.get $$y) (local.get $$y)))))
  (func (export "f64.no_fold_to_hypot") (param $$x f64) (param $$y f64) (result f64)
    (f64.sqrt (f64.add (f64.mul (local.get $$x) (local.get $$x))
                       (f64.mul (local.get $$y) (local.get $$y)))))
)`);

// ./test/core/float_exprs.wast:434
assert_return(
  () =>
    invoke($25, `f32.no_fold_to_hypot`, [
      value("f32", 0.00000000000000000000000072854914),
      value("f32", 0.0000000000000000000042365796),
    ]),
  [value("f32", 0.0000000000000000000042366535)],
);

// ./test/core/float_exprs.wast:435
assert_return(
  () =>
    invoke($25, `f32.no_fold_to_hypot`, [
      value("f32", -0.0000000000000000000007470285),
      value("f32", -0.000000000000000000000000000000007453745),
    ]),
  [value("f32", 0.0000000000000000000007468044)],
);

// ./test/core/float_exprs.wast:436
assert_return(
  () =>
    invoke($25, `f32.no_fold_to_hypot`, [
      value("f32", -0.0000000000000000000000000000000000770895),
      value("f32", -0.0000000000000000000032627214),
    ]),
  [value("f32", 0.0000000000000000000032627695)],
);

// ./test/core/float_exprs.wast:437
assert_return(
  () =>
    invoke($25, `f32.no_fold_to_hypot`, [
      value("f32", -35.42818),
      value("f32", 174209.48),
    ]),
  [value("f32", 174209.5)],
);

// ./test/core/float_exprs.wast:438
assert_return(
  () =>
    invoke($25, `f32.no_fold_to_hypot`, [
      value("f32", 0.000000000000000000000020628143),
      value("f32", -0.00000000000000000000046344753),
    ]),
  [value("f32", 0.000000000000000000000463032)],
);

// ./test/core/float_exprs.wast:439
assert_return(
  () =>
    invoke($25, `f64.no_fold_to_hypot`, [
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000003863640258986321,
      ),
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000019133014752624014,
      ),
    ]),
  [value(
    "f64",
    0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000019120893753235554,
  )],
);

// ./test/core/float_exprs.wast:440
assert_return(
  () =>
    invoke($25, `f64.no_fold_to_hypot`, [
      value(
        "f64",
        138561238950329770000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        -2828038515930043000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    2828038519324483400000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:441
assert_return(
  () =>
    invoke($25, `f64.no_fold_to_hypot`, [
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000006502729096641792,
      ),
      value(
        "f64",
        -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000004544399933151275,
      ),
    ]),
  [value(
    "f64",
    0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000006668276248455232,
  )],
);

// ./test/core/float_exprs.wast:442
assert_return(
  () =>
    invoke($25, `f64.no_fold_to_hypot`, [
      value(
        "f64",
        -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000022340232024202604,
      ),
      value(
        "f64",
        0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000003435929714143315,
      ),
    ]),
  [value(
    "f64",
    0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000022340232023799234,
  )],
);

// ./test/core/float_exprs.wast:443
assert_return(
  () =>
    invoke($25, `f64.no_fold_to_hypot`, [
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002797963998630554,
      ),
      value(
        "f64",
        -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001906867996862016,
      ),
    ]),
  [value(
    "f64",
    0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000019068679968620105,
  )],
);

// ./test/core/float_exprs.wast:447
let $26 = instantiate(`(module
  (func (export "f32.no_approximate_reciprocal") (param $$x f32) (result f32)
    (f32.div (f32.const 1.0) (local.get $$x)))
)`);

// ./test/core/float_exprs.wast:452
assert_return(
  () =>
    invoke($26, `f32.no_approximate_reciprocal`, [value("f32", -0.0011329757)]),
  [value("f32", -882.6315)],
);

// ./test/core/float_exprs.wast:453
assert_return(
  () =>
    invoke($26, `f32.no_approximate_reciprocal`, [
      value("f32", 323753010000000000000000000000000000000),
    ]),
  [value("f32", 0.000000000000000000000000000000000000003088774)],
);

// ./test/core/float_exprs.wast:454
assert_return(
  () =>
    invoke($26, `f32.no_approximate_reciprocal`, [
      value("f32", -0.0000000000000000000000000001272599),
    ]),
  [value("f32", -7857934600000000000000000000)],
);

// ./test/core/float_exprs.wast:455
assert_return(
  () =>
    invoke($26, `f32.no_approximate_reciprocal`, [
      value("f32", 103020680000000000000000),
    ]),
  [value("f32", 0.000000000000000000000009706789)],
);

// ./test/core/float_exprs.wast:456
assert_return(
  () =>
    invoke($26, `f32.no_approximate_reciprocal`, [
      value("f32", -0.00000000000000000000000028443763),
    ]),
  [value("f32", -3515709300000000000000000)],
);

// ./test/core/float_exprs.wast:460
let $27 = instantiate(`(module
  (func (export "f32.no_approximate_reciprocal_sqrt") (param $$x f32) (result f32)
    (f32.div (f32.const 1.0) (f32.sqrt (local.get $$x))))
  (func (export "f64.no_fuse_reciprocal_sqrt") (param $$x f64) (result f64)
    (f64.div (f64.const 1.0) (f64.sqrt (local.get $$x))))
)`);

// ./test/core/float_exprs.wast:467
assert_return(
  () =>
    invoke($27, `f32.no_approximate_reciprocal_sqrt`, [
      value("f32", 0.00000000000016117865),
    ]),
  [value("f32", 2490842.5)],
);

// ./test/core/float_exprs.wast:468
assert_return(
  () =>
    invoke($27, `f32.no_approximate_reciprocal_sqrt`, [
      value("f32", 0.0074491366),
    ]),
  [value("f32", 11.58636)],
);

// ./test/core/float_exprs.wast:469
assert_return(
  () =>
    invoke($27, `f32.no_approximate_reciprocal_sqrt`, [
      value("f32", 0.00000000000000000002339817),
    ]),
  [value("f32", 6537460000)],
);

// ./test/core/float_exprs.wast:470
assert_return(
  () =>
    invoke($27, `f32.no_approximate_reciprocal_sqrt`, [
      value("f32", 0.00000000000011123504),
    ]),
  [value("f32", 2998328.3)],
);

// ./test/core/float_exprs.wast:471
assert_return(
  () =>
    invoke($27, `f32.no_approximate_reciprocal_sqrt`, [
      value("f32", 0.000000000000000000000000017653063),
    ]),
  [value("f32", 7526446300000)],
);

// ./test/core/float_exprs.wast:473
assert_return(
  () =>
    invoke($27, `f64.no_fuse_reciprocal_sqrt`, [
      value(
        "f64",
        4472459252766337000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000014952947335898096,
  )],
);

// ./test/core/float_exprs.wast:474
assert_return(
  () =>
    invoke($27, `f64.no_fuse_reciprocal_sqrt`, [
      value(
        "f64",
        4752392260007119000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    0.000000000000000000000000000000000000000000000014505872638954843,
  )],
);

// ./test/core/float_exprs.wast:475
assert_return(
  () =>
    invoke($27, `f64.no_fuse_reciprocal_sqrt`, [
      value("f64", 29014415885392436000000000000000),
    ]),
  [value("f64", 0.00000000000000018564920084793608)],
);

// ./test/core/float_exprs.wast:476
assert_return(
  () =>
    invoke($27, `f64.no_fuse_reciprocal_sqrt`, [
      value(
        "f64",
        1396612507697477800000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000026758516751072132,
  )],
);

// ./test/core/float_exprs.wast:477
assert_return(
  () =>
    invoke($27, `f64.no_fuse_reciprocal_sqrt`, [
      value("f64", 151596415440704430000000000000000000000000000),
    ]),
  [value("f64", 0.00000000000000000000008121860649480894)],
);

// ./test/core/float_exprs.wast:481
let $28 = instantiate(`(module
  (func (export "f32.no_approximate_sqrt_reciprocal") (param $$x f32) (result f32)
    (f32.sqrt (f32.div (f32.const 1.0) (local.get $$x))))
)`);

// ./test/core/float_exprs.wast:486
assert_return(
  () =>
    invoke($28, `f32.no_approximate_sqrt_reciprocal`, [
      value("f32", 1895057100000000000),
    ]),
  [value("f32", 0.00000000072642176)],
);

// ./test/core/float_exprs.wast:487
assert_return(
  () =>
    invoke($28, `f32.no_approximate_sqrt_reciprocal`, [
      value("f32", 0.002565894),
    ]),
  [value("f32", 19.741522)],
);

// ./test/core/float_exprs.wast:488
assert_return(
  () =>
    invoke($28, `f32.no_approximate_sqrt_reciprocal`, [
      value("f32", 632654500000000000000),
    ]),
  [value("f32", 0.000000000039757284)],
);

// ./test/core/float_exprs.wast:489
assert_return(
  () =>
    invoke($28, `f32.no_approximate_sqrt_reciprocal`, [
      value("f32", 14153.539),
    ]),
  [value("f32", 0.008405576)],
);

// ./test/core/float_exprs.wast:490
assert_return(
  () =>
    invoke($28, `f32.no_approximate_sqrt_reciprocal`, [
      value("f32", 26173730000000000000000000000000),
    ]),
  [value("f32", 0.00000000000000019546418)],
);

// ./test/core/float_exprs.wast:494
let $29 = instantiate(`(module
  (func (export "i32.no_fold_f32_s") (param i32) (result i32)
    (i32.trunc_f32_s (f32.convert_i32_s (local.get 0))))
  (func (export "i32.no_fold_f32_u") (param i32) (result i32)
    (i32.trunc_f32_u (f32.convert_i32_u (local.get 0))))
  (func (export "i64.no_fold_f64_s") (param i64) (result i64)
    (i64.trunc_f64_s (f64.convert_i64_s (local.get 0))))
  (func (export "i64.no_fold_f64_u") (param i64) (result i64)
    (i64.trunc_f64_u (f64.convert_i64_u (local.get 0))))
)`);

// ./test/core/float_exprs.wast:505
assert_return(() => invoke($29, `i32.no_fold_f32_s`, [16777216]), [
  value("i32", 16777216),
]);

// ./test/core/float_exprs.wast:506
assert_return(() => invoke($29, `i32.no_fold_f32_s`, [16777217]), [
  value("i32", 16777216),
]);

// ./test/core/float_exprs.wast:507
assert_return(() => invoke($29, `i32.no_fold_f32_s`, [-268435440]), [
  value("i32", -268435440),
]);

// ./test/core/float_exprs.wast:509
assert_return(() => invoke($29, `i32.no_fold_f32_u`, [16777216]), [
  value("i32", 16777216),
]);

// ./test/core/float_exprs.wast:510
assert_return(() => invoke($29, `i32.no_fold_f32_u`, [16777217]), [
  value("i32", 16777216),
]);

// ./test/core/float_exprs.wast:511
assert_return(() => invoke($29, `i32.no_fold_f32_u`, [-268435440]), [
  value("i32", -268435456),
]);

// ./test/core/float_exprs.wast:513
assert_return(() => invoke($29, `i64.no_fold_f64_s`, [9007199254740992n]), [
  value("i64", 9007199254740992n),
]);

// ./test/core/float_exprs.wast:514
assert_return(() => invoke($29, `i64.no_fold_f64_s`, [9007199254740993n]), [
  value("i64", 9007199254740992n),
]);

// ./test/core/float_exprs.wast:515
assert_return(() => invoke($29, `i64.no_fold_f64_s`, [-1152921504606845952n]), [
  value("i64", -1152921504606845952n),
]);

// ./test/core/float_exprs.wast:517
assert_return(() => invoke($29, `i64.no_fold_f64_u`, [9007199254740992n]), [
  value("i64", 9007199254740992n),
]);

// ./test/core/float_exprs.wast:518
assert_return(() => invoke($29, `i64.no_fold_f64_u`, [9007199254740993n]), [
  value("i64", 9007199254740992n),
]);

// ./test/core/float_exprs.wast:519
assert_return(() => invoke($29, `i64.no_fold_f64_u`, [-1152921504606845952n]), [
  value("i64", -1152921504606846976n),
]);

// ./test/core/float_exprs.wast:523
let $30 = instantiate(`(module
  (func (export "f32.no_fold_add_sub") (param $$x f32) (param $$y f32) (result f32)
    (f32.sub (f32.add (local.get $$x) (local.get $$y)) (local.get $$y)))
  (func (export "f64.no_fold_add_sub") (param $$x f64) (param $$y f64) (result f64)
    (f64.sub (f64.add (local.get $$x) (local.get $$y)) (local.get $$y)))
)`);

// ./test/core/float_exprs.wast:530
assert_return(
  () =>
    invoke($30, `f32.no_fold_add_sub`, [
      value("f32", 0.000000000000012138282),
      value("f32", -0.000000020946384),
    ]),
  [value("f32", 0.000000000000012434498)],
);

// ./test/core/float_exprs.wast:531
assert_return(
  () =>
    invoke($30, `f32.no_fold_add_sub`, [
      value("f32", -0.00000019768197),
      value("f32", 0.0000037154566),
    ]),
  [value("f32", -0.00000019768208)],
);

// ./test/core/float_exprs.wast:532
assert_return(
  () =>
    invoke($30, `f32.no_fold_add_sub`, [
      value("f32", -9596213000000000000000000),
      value("f32", -3538041400000000000000000000000),
    ]),
  [value("f32", -9671407000000000000000000)],
);

// ./test/core/float_exprs.wast:533
assert_return(
  () =>
    invoke($30, `f32.no_fold_add_sub`, [
      value("f32", 0.000000000000000000000005054346),
      value("f32", 0.000000000000000024572656),
    ]),
  [value("f32", 0.0000000000000000000000049630837)],
);

// ./test/core/float_exprs.wast:534
assert_return(
  () =>
    invoke($30, `f32.no_fold_add_sub`, [
      value("f32", -0.0000000000000000000000000000000033693147),
      value("f32", -0.000000000000000000000000071014917),
    ]),
  [value("f32", -0.000000000000000000000000000000006162976)],
);

// ./test/core/float_exprs.wast:536
assert_return(
  () =>
    invoke($30, `f64.no_fold_add_sub`, [
      value(
        "f64",
        -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000008445702651973109,
      ),
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001110684389828854,
      ),
    ]),
  [value(
    "f64",
    -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000008445702651873978,
  )],
);

// ./test/core/float_exprs.wast:537
assert_return(
  () =>
    invoke($30, `f64.no_fold_add_sub`, [
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000008198798715927055,
      ),
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000004624035606110903,
      ),
    ]),
  [value(
    "f64",
    0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000008198798715897085,
  )],
);

// ./test/core/float_exprs.wast:538
assert_return(
  () =>
    invoke($30, `f64.no_fold_add_sub`, [
      value("f64", -0.0000000013604511322066714),
      value("f64", -0.1751431740707098),
    ]),
  [value("f64", -0.0000000013604511406306585)],
);

// ./test/core/float_exprs.wast:539
assert_return(
  () =>
    invoke($30, `f64.no_fold_add_sub`, [
      value(
        "f64",
        -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000003944335437865966,
      ),
      value(
        "f64",
        0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001663809741322667,
      ),
    ]),
  [value(
    "f64",
    -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000039443355500325104,
  )],
);

// ./test/core/float_exprs.wast:540
assert_return(
  () =>
    invoke($30, `f64.no_fold_add_sub`, [
      value(
        "f64",
        -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005078309818866,
      ),
      value(
        "f64",
        -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000010790431644461104,
      ),
    ]),
  [value(
    "f64",
    -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000507831047937567,
  )],
);

// ./test/core/float_exprs.wast:544
let $31 = instantiate(`(module
  (func (export "f32.no_fold_sub_add") (param $$x f32) (param $$y f32) (result f32)
    (f32.add (f32.sub (local.get $$x) (local.get $$y)) (local.get $$y)))
  (func (export "f64.no_fold_sub_add") (param $$x f64) (param $$y f64) (result f64)
    (f64.add (f64.sub (local.get $$x) (local.get $$y)) (local.get $$y)))
)`);

// ./test/core/float_exprs.wast:551
assert_return(
  () =>
    invoke($31, `f32.no_fold_sub_add`, [
      value("f32", -676.47437),
      value("f32", 403.0368),
    ]),
  [value("f32", -676.4744)],
);

// ./test/core/float_exprs.wast:552
assert_return(
  () =>
    invoke($31, `f32.no_fold_sub_add`, [
      value("f32", -0.0000000000000000000000000000000006305943),
      value("f32", 0.0000000000000000000000000000367186),
    ]),
  [value("f32", -0.00000000000000000000000000000000063194576)],
);

// ./test/core/float_exprs.wast:553
assert_return(
  () =>
    invoke($31, `f32.no_fold_sub_add`, [
      value("f32", 83184800),
      value("f32", 46216217000),
    ]),
  [value("f32", 83185660)],
);

// ./test/core/float_exprs.wast:554
assert_return(
  () =>
    invoke($31, `f32.no_fold_sub_add`, [
      value("f32", 0.000000000002211957),
      value("f32", -0.00000001043793),
    ]),
  [value("f32", 0.0000000000022115643)],
);

// ./test/core/float_exprs.wast:555
assert_return(
  () =>
    invoke($31, `f32.no_fold_sub_add`, [
      value("f32", 0.14944395),
      value("f32", -27393.65),
    ]),
  [value("f32", 0.15039063)],
);

// ./test/core/float_exprs.wast:557
assert_return(
  () =>
    invoke($31, `f64.no_fold_sub_add`, [
      value(
        "f64",
        90365982617946240000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        -958186427535552000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    90365982617946280000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:558
assert_return(
  () =>
    invoke($31, `f64.no_fold_sub_add`, [
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000044230403564658815,
      ),
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000026713491049366576,
      ),
    ]),
  [value(
    "f64",
    0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000004423040356647721,
  )],
);

// ./test/core/float_exprs.wast:559
assert_return(
  () =>
    invoke($31, `f64.no_fold_sub_add`, [
      value(
        "f64",
        4095348452776429000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        -4050190019576568700000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    4070815637249397500000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:560
assert_return(
  () =>
    invoke($31, `f64.no_fold_sub_add`, [
      value("f64", 0.000000024008889207554433),
      value("f64", -0.00017253797929188484),
    ]),
  [value("f64", 0.00000002400888920756506)],
);

// ./test/core/float_exprs.wast:561
assert_return(
  () =>
    invoke($31, `f64.no_fold_sub_add`, [
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000043367542918305866,
      ),
      value(
        "f64",
        -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000039597706708227122,
      ),
    ]),
  [value(
    "f64",
    0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000004336754291830597,
  )],
);

// ./test/core/float_exprs.wast:565
let $32 = instantiate(`(module
  (func (export "f32.no_fold_mul_div") (param $$x f32) (param $$y f32) (result f32)
    (f32.div (f32.mul (local.get $$x) (local.get $$y)) (local.get $$y)))
  (func (export "f64.no_fold_mul_div") (param $$x f64) (param $$y f64) (result f64)
    (f64.div (f64.mul (local.get $$x) (local.get $$y)) (local.get $$y)))
)`);

// ./test/core/float_exprs.wast:572
assert_return(
  () =>
    invoke($32, `f32.no_fold_mul_div`, [
      value("f32", -32476715000000000),
      value("f32", 0.000000000000010121375),
    ]),
  [value("f32", -32476713000000000)],
);

// ./test/core/float_exprs.wast:573
assert_return(
  () =>
    invoke($32, `f32.no_fold_mul_div`, [
      value("f32", -0.000000015561163),
      value("f32", 0.000000000000000000000000000000015799828),
    ]),
  [value("f32", -0.000000015561145)],
);

// ./test/core/float_exprs.wast:574
assert_return(
  () =>
    invoke($32, `f32.no_fold_mul_div`, [
      value("f32", -0.00000000000000676311),
      value("f32", -441324000000000),
    ]),
  [value("f32", -0.0000000000000067631096)],
);

// ./test/core/float_exprs.wast:575
assert_return(
  () =>
    invoke($32, `f32.no_fold_mul_div`, [
      value("f32", 7505613700000000),
      value("f32", -2160384100000000000),
    ]),
  [value("f32", 7505613000000000)],
);

// ./test/core/float_exprs.wast:576
assert_return(
  () =>
    invoke($32, `f32.no_fold_mul_div`, [
      value("f32", -0.0000000000000000000000000002362576),
      value("f32", -0.000000000010808759),
    ]),
  [value("f32", -0.00000000000000000000000000023625765)],
);

// ./test/core/float_exprs.wast:578
assert_return(
  () =>
    invoke($32, `f64.no_fold_mul_div`, [
      value(
        "f64",
        -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000013532103713575586,
      ),
      value(
        "f64",
        -0.00000000000000000000000000000000000000000000000000000000000003347836467564916,
      ),
    ]),
  [value(
    "f64",
    -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000013532103713672434,
  )],
);

// ./test/core/float_exprs.wast:579
assert_return(
  () =>
    invoke($32, `f64.no_fold_mul_div`, [
      value(
        "f64",
        77662174313180845000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        195959155606939530000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    77662174313180850000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:580
assert_return(
  () =>
    invoke($32, `f64.no_fold_mul_div`, [
      value(
        "f64",
        -718011781190294800000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000009320036042623636,
      ),
    ]),
  [value(
    "f64",
    -718011781190294750000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:581
assert_return(
  () =>
    invoke($32, `f64.no_fold_mul_div`, [
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000017260010724693063,
      ),
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000003568792428129926,
      ),
    ]),
  [value(
    "f64",
    0.00000000000000000000000000000000000000000000000001661286799244216,
  )],
);

// ./test/core/float_exprs.wast:582
assert_return(
  () =>
    invoke($32, `f64.no_fold_mul_div`, [
      value(
        "f64",
        -9145223045828962000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005562094034342845,
      ),
    ]),
  [value(
    "f64",
    -9145223045828963000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:586
let $33 = instantiate(`(module
  (func (export "f32.no_fold_div_mul") (param $$x f32) (param $$y f32) (result f32)
    (f32.mul (f32.div (local.get $$x) (local.get $$y)) (local.get $$y)))
  (func (export "f64.no_fold_div_mul") (param $$x f64) (param $$y f64) (result f64)
    (f64.mul (f64.div (local.get $$x) (local.get $$y)) (local.get $$y)))
)`);

// ./test/core/float_exprs.wast:593
assert_return(
  () =>
    invoke($33, `f32.no_fold_div_mul`, [
      value("f32", -511517980000),
      value("f32", 986062200),
    ]),
  [value("f32", -511517950000)],
);

// ./test/core/float_exprs.wast:594
assert_return(
  () =>
    invoke($33, `f32.no_fold_div_mul`, [
      value("f32", -0.00000000000000024944853),
      value("f32", -0.0000041539834),
    ]),
  [value("f32", -0.00000000000000024944856)],
);

// ./test/core/float_exprs.wast:595
assert_return(
  () =>
    invoke($33, `f32.no_fold_div_mul`, [
      value("f32", 0.000000000000000000000000000000000000020827855),
      value("f32", -235.19847),
    ]),
  [value("f32", 0.000000000000000000000000000000000000020828013)],
);

// ./test/core/float_exprs.wast:596
assert_return(
  () =>
    invoke($33, `f32.no_fold_div_mul`, [
      value("f32", -0.000000000000000000000062499487),
      value("f32", -696312600000000000),
    ]),
  [value("f32", -0.00000000000000000000006249919)],
);

// ./test/core/float_exprs.wast:597
assert_return(
  () =>
    invoke($33, `f32.no_fold_div_mul`, [
      value("f32", 0.0000000000000000000000000000058353514),
      value("f32", 212781120),
    ]),
  [value("f32", 0.000000000000000000000000000005835352)],
);

// ./test/core/float_exprs.wast:599
assert_return(
  () =>
    invoke($33, `f64.no_fold_div_mul`, [
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000035984644259935362,
      ),
      value("f64", -28812263298033320000000000000000000000000000000000000000),
    ]),
  [value(
    "f64",
    0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000035985034356565485,
  )],
);

// ./test/core/float_exprs.wast:600
assert_return(
  () =>
    invoke($33, `f64.no_fold_div_mul`, [
      value(
        "f64",
        -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000017486367047479447,
      ),
      value("f64", 0.00000000000000016508738454798636),
    ]),
  [value(
    "f64",
    -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001748636704747945,
  )],
);

// ./test/core/float_exprs.wast:601
assert_return(
  () =>
    invoke($33, `f64.no_fold_div_mul`, [
      value(
        "f64",
        -0.00000000000000000000000000000000000000000003140341989542684,
      ),
      value(
        "f64",
        942829809081919600000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    -0.000000000000000000000000000000000000000000031403419895426836,
  )],
);

// ./test/core/float_exprs.wast:602
assert_return(
  () =>
    invoke($33, `f64.no_fold_div_mul`, [
      value(
        "f64",
        -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000003919745428533519,
      ),
      value(
        "f64",
        -21314747179654705000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000039197454285335185,
  )],
);

// ./test/core/float_exprs.wast:603
assert_return(
  () =>
    invoke($33, `f64.no_fold_div_mul`, [
      value(
        "f64",
        -5734160003788982000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        6350805843612229000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    -5734160003788981000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:607
let $34 = instantiate(`(module
  (func (export "f32.no_fold_div2_mul2") (param $$x f32) (result f32)
    (f32.mul (f32.div (local.get $$x) (f32.const 2.0)) (f32.const 2.0)))
  (func (export "f64.no_fold_div2_mul2") (param $$x f64) (result f64)
    (f64.mul (f64.div (local.get $$x) (f64.const 2.0)) (f64.const 2.0)))
)`);

// ./test/core/float_exprs.wast:614
assert_return(
  () =>
    invoke($34, `f32.no_fold_div2_mul2`, [
      value("f32", 0.000000000000000000000000000000000000023509886),
    ]),
  [value("f32", 0.000000000000000000000000000000000000023509887)],
);

// ./test/core/float_exprs.wast:615
assert_return(
  () =>
    invoke($34, `f64.no_fold_div2_mul2`, [
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000044501477170144023,
      ),
    ]),
  [value(
    "f64",
    0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000004450147717014403,
  )],
);

// ./test/core/float_exprs.wast:619
let $35 = instantiate(`(module
  (func (export "no_fold_demote_promote") (param $$x f64) (result f64)
    (f64.promote_f32 (f32.demote_f64 (local.get $$x))))
)`);

// ./test/core/float_exprs.wast:624
assert_return(
  () =>
    invoke($35, `no_fold_demote_promote`, [
      value("f64", -0.00000000000000000000000000000000000000017176297220569481),
    ]),
  [value("f64", -0.00000000000000000000000000000000000000017176275796615013)],
);

// ./test/core/float_exprs.wast:625
assert_return(
  () =>
    invoke($35, `no_fold_demote_promote`, [
      value("f64", -0.000000000000000000000000028464775573304055),
    ]),
  [value("f64", -0.00000000000000000000000002846477619188087)],
);

// ./test/core/float_exprs.wast:626
assert_return(
  () =>
    invoke($35, `no_fold_demote_promote`, [
      value("f64", 208970699699909230000000000000000),
    ]),
  [value("f64", 208970700445326000000000000000000)],
);

// ./test/core/float_exprs.wast:627
assert_return(
  () =>
    invoke($35, `no_fold_demote_promote`, [
      value("f64", -0.0000000000000000000000000047074160416121775),
    ]),
  [value("f64", -0.0000000000000000000000000047074161331556024)],
);

// ./test/core/float_exprs.wast:628
assert_return(
  () =>
    invoke($35, `no_fold_demote_promote`, [
      value("f64", 23359451497950880000000000000000),
    ]),
  [value("f64", 23359452224542198000000000000000)],
);

// ./test/core/float_exprs.wast:633
let $36 = instantiate(`(module
  (func (export "no_fold_promote_demote") (param $$x f32) (result f32)
    (f32.demote_f64 (f64.promote_f32 (local.get $$x))))
)`);

// ./test/core/float_exprs.wast:638
assert_return(
  () =>
    invoke($36, `no_fold_promote_demote`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/float_exprs.wast:639
assert_return(() => invoke($36, `no_fold_promote_demote`, [value("f32", 0)]), [
  value("f32", 0),
]);

// ./test/core/float_exprs.wast:640
assert_return(() => invoke($36, `no_fold_promote_demote`, [value("f32", -0)]), [
  value("f32", -0),
]);

// ./test/core/float_exprs.wast:641
assert_return(
  () =>
    invoke($36, `no_fold_promote_demote`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/float_exprs.wast:642
assert_return(
  () =>
    invoke($36, `no_fold_promote_demote`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/float_exprs.wast:643
assert_return(
  () =>
    invoke($36, `no_fold_promote_demote`, [
      value("f32", 0.000000000000000000000000000000000000011754942),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754942)],
);

// ./test/core/float_exprs.wast:644
assert_return(
  () =>
    invoke($36, `no_fold_promote_demote`, [
      value("f32", -0.000000000000000000000000000000000000011754942),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754942)],
);

// ./test/core/float_exprs.wast:645
assert_return(
  () =>
    invoke($36, `no_fold_promote_demote`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/float_exprs.wast:646
assert_return(
  () =>
    invoke($36, `no_fold_promote_demote`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/float_exprs.wast:647
assert_return(
  () =>
    invoke($36, `no_fold_promote_demote`, [
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/float_exprs.wast:648
assert_return(
  () =>
    invoke($36, `no_fold_promote_demote`, [
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/float_exprs.wast:649
assert_return(
  () => invoke($36, `no_fold_promote_demote`, [value("f32", Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/float_exprs.wast:650
assert_return(
  () => invoke($36, `no_fold_promote_demote`, [value("f32", -Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/float_exprs.wast:654
let $37 = instantiate(`(module
  (func (export "no_demote_mixed_add") (param $$x f64) (param $$y f32) (result f32)
    (f32.demote_f64 (f64.add (local.get $$x) (f64.promote_f32 (local.get $$y)))))
  (func (export "no_demote_mixed_add_commuted") (param $$y f32) (param $$x f64) (result f32)
    (f32.demote_f64 (f64.add (f64.promote_f32 (local.get $$y)) (local.get $$x))))
)`);

// ./test/core/float_exprs.wast:661
assert_return(
  () =>
    invoke($37, `no_demote_mixed_add`, [
      value("f64", 0.00000000000000000000000000004941266527909197),
      value("f32", 0.0000000000000000000000000000000000018767183),
    ]),
  [value("f32", 0.000000000000000000000000000049412667)],
);

// ./test/core/float_exprs.wast:662
assert_return(
  () =>
    invoke($37, `no_demote_mixed_add`, [
      value("f64", 140851523637.69385),
      value("f32", 401096440000),
    ]),
  [value("f32", 541947950000)],
);

// ./test/core/float_exprs.wast:663
assert_return(
  () =>
    invoke($37, `no_demote_mixed_add`, [
      value("f64", 0.0000000000000000000000000000000000020831160914192852),
      value("f32", -0.0000000000000000000000000000000000006050095),
    ]),
  [value("f32", 0.0000000000000000000000000000000000014781066)],
);

// ./test/core/float_exprs.wast:664
assert_return(
  () =>
    invoke($37, `no_demote_mixed_add`, [
      value("f64", -0.0000010032827553674626),
      value("f32", 0.0000000019312918),
    ]),
  [value("f32", -0.0000010013515)],
);

// ./test/core/float_exprs.wast:665
assert_return(
  () =>
    invoke($37, `no_demote_mixed_add`, [
      value("f64", -0.0000013840207035752711),
      value("f32", -0.0000000000005202814),
    ]),
  [value("f32", -0.0000013840212)],
);

// ./test/core/float_exprs.wast:667
assert_return(
  () =>
    invoke($37, `no_demote_mixed_add_commuted`, [
      value("f32", 0.0000000000000000000000000000000000018767183),
      value("f64", 0.00000000000000000000000000004941266527909197),
    ]),
  [value("f32", 0.000000000000000000000000000049412667)],
);

// ./test/core/float_exprs.wast:668
assert_return(
  () =>
    invoke($37, `no_demote_mixed_add_commuted`, [
      value("f32", 401096440000),
      value("f64", 140851523637.69385),
    ]),
  [value("f32", 541947950000)],
);

// ./test/core/float_exprs.wast:669
assert_return(
  () =>
    invoke($37, `no_demote_mixed_add_commuted`, [
      value("f32", -0.0000000000000000000000000000000000006050095),
      value("f64", 0.0000000000000000000000000000000000020831160914192852),
    ]),
  [value("f32", 0.0000000000000000000000000000000000014781066)],
);

// ./test/core/float_exprs.wast:670
assert_return(
  () =>
    invoke($37, `no_demote_mixed_add_commuted`, [
      value("f32", 0.0000000019312918),
      value("f64", -0.0000010032827553674626),
    ]),
  [value("f32", -0.0000010013515)],
);

// ./test/core/float_exprs.wast:671
assert_return(
  () =>
    invoke($37, `no_demote_mixed_add_commuted`, [
      value("f32", -0.0000000000005202814),
      value("f64", -0.0000013840207035752711),
    ]),
  [value("f32", -0.0000013840212)],
);

// ./test/core/float_exprs.wast:675
let $38 = instantiate(`(module
  (func (export "no_demote_mixed_sub") (param $$x f64) (param $$y f32) (result f32)
    (f32.demote_f64 (f64.sub (local.get $$x) (f64.promote_f32 (local.get $$y)))))
)`);

// ./test/core/float_exprs.wast:680
assert_return(
  () =>
    invoke($38, `no_demote_mixed_sub`, [
      value("f64", 7869935327202668000000000),
      value("f32", 4086347000000000000),
    ]),
  [value("f32", 7869931000000000000000000)],
);

// ./test/core/float_exprs.wast:681
assert_return(
  () =>
    invoke($38, `no_demote_mixed_sub`, [
      value("f64", -1535841968.9056544),
      value("f32", 239897.28),
    ]),
  [value("f32", -1536081900)],
);

// ./test/core/float_exprs.wast:682
assert_return(
  () =>
    invoke($38, `no_demote_mixed_sub`, [
      value("f64", -102.19459272722602),
      value("f32", 0.00039426138),
    ]),
  [value("f32", -102.194984)],
);

// ./test/core/float_exprs.wast:683
assert_return(
  () =>
    invoke($38, `no_demote_mixed_sub`, [
      value("f64", 0.00000000000000005645470375565188),
      value("f32", 0.0000000000000000000005851077),
    ]),
  [value("f32", 0.00000000000000005645412)],
);

// ./test/core/float_exprs.wast:684
assert_return(
  () =>
    invoke($38, `no_demote_mixed_sub`, [
      value("f64", 27090.388466832894),
      value("f32", 63120.89),
    ]),
  [value("f32", -36030.504)],
);

// ./test/core/float_exprs.wast:688
let $39 = instantiate(`(module
  (func (export "f32.i32.no_fold_trunc_s_convert_s") (param $$x f32) (result f32)
    (f32.convert_i32_s (i32.trunc_f32_s (local.get $$x))))
  (func (export "f32.i32.no_fold_trunc_u_convert_s") (param $$x f32) (result f32)
    (f32.convert_i32_s (i32.trunc_f32_u (local.get $$x))))
  (func (export "f32.i32.no_fold_trunc_s_convert_u") (param $$x f32) (result f32)
    (f32.convert_i32_u (i32.trunc_f32_s (local.get $$x))))
  (func (export "f32.i32.no_fold_trunc_u_convert_u") (param $$x f32) (result f32)
    (f32.convert_i32_u (i32.trunc_f32_u (local.get $$x))))
  (func (export "f64.i32.no_fold_trunc_s_convert_s") (param $$x f64) (result f64)
    (f64.convert_i32_s (i32.trunc_f64_s (local.get $$x))))
  (func (export "f64.i32.no_fold_trunc_u_convert_s") (param $$x f64) (result f64)
    (f64.convert_i32_s (i32.trunc_f64_u (local.get $$x))))
  (func (export "f64.i32.no_fold_trunc_s_convert_u") (param $$x f64) (result f64)
    (f64.convert_i32_u (i32.trunc_f64_s (local.get $$x))))
  (func (export "f64.i32.no_fold_trunc_u_convert_u") (param $$x f64) (result f64)
    (f64.convert_i32_u (i32.trunc_f64_u (local.get $$x))))
  (func (export "f32.i64.no_fold_trunc_s_convert_s") (param $$x f32) (result f32)
    (f32.convert_i64_s (i64.trunc_f32_s (local.get $$x))))
  (func (export "f32.i64.no_fold_trunc_u_convert_s") (param $$x f32) (result f32)
    (f32.convert_i64_s (i64.trunc_f32_u (local.get $$x))))
  (func (export "f32.i64.no_fold_trunc_s_convert_u") (param $$x f32) (result f32)
    (f32.convert_i64_u (i64.trunc_f32_s (local.get $$x))))
  (func (export "f32.i64.no_fold_trunc_u_convert_u") (param $$x f32) (result f32)
    (f32.convert_i64_u (i64.trunc_f32_u (local.get $$x))))
  (func (export "f64.i64.no_fold_trunc_s_convert_s") (param $$x f64) (result f64)
    (f64.convert_i64_s (i64.trunc_f64_s (local.get $$x))))
  (func (export "f64.i64.no_fold_trunc_u_convert_s") (param $$x f64) (result f64)
    (f64.convert_i64_s (i64.trunc_f64_u (local.get $$x))))
  (func (export "f64.i64.no_fold_trunc_s_convert_u") (param $$x f64) (result f64)
    (f64.convert_i64_u (i64.trunc_f64_s (local.get $$x))))
  (func (export "f64.i64.no_fold_trunc_u_convert_u") (param $$x f64) (result f64)
    (f64.convert_i64_u (i64.trunc_f64_u (local.get $$x))))
)`);

// ./test/core/float_exprs.wast:723
assert_return(
  () => invoke($39, `f32.i32.no_fold_trunc_s_convert_s`, [value("f32", 1.5)]),
  [value("f32", 1)],
);

// ./test/core/float_exprs.wast:724
assert_return(
  () => invoke($39, `f32.i32.no_fold_trunc_s_convert_s`, [value("f32", -1.5)]),
  [value("f32", -1)],
);

// ./test/core/float_exprs.wast:725
assert_return(
  () => invoke($39, `f32.i32.no_fold_trunc_u_convert_s`, [value("f32", 1.5)]),
  [value("f32", 1)],
);

// ./test/core/float_exprs.wast:726
assert_return(
  () => invoke($39, `f32.i32.no_fold_trunc_u_convert_s`, [value("f32", -0.5)]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:727
assert_return(
  () => invoke($39, `f32.i32.no_fold_trunc_s_convert_u`, [value("f32", 1.5)]),
  [value("f32", 1)],
);

// ./test/core/float_exprs.wast:728
assert_return(
  () => invoke($39, `f32.i32.no_fold_trunc_s_convert_u`, [value("f32", -1.5)]),
  [value("f32", 4294967300)],
);

// ./test/core/float_exprs.wast:729
assert_return(
  () => invoke($39, `f32.i32.no_fold_trunc_u_convert_u`, [value("f32", 1.5)]),
  [value("f32", 1)],
);

// ./test/core/float_exprs.wast:730
assert_return(
  () => invoke($39, `f32.i32.no_fold_trunc_u_convert_u`, [value("f32", -0.5)]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:732
assert_return(
  () => invoke($39, `f64.i32.no_fold_trunc_s_convert_s`, [value("f64", 1.5)]),
  [value("f64", 1)],
);

// ./test/core/float_exprs.wast:733
assert_return(
  () => invoke($39, `f64.i32.no_fold_trunc_s_convert_s`, [value("f64", -1.5)]),
  [value("f64", -1)],
);

// ./test/core/float_exprs.wast:734
assert_return(
  () => invoke($39, `f64.i32.no_fold_trunc_u_convert_s`, [value("f64", 1.5)]),
  [value("f64", 1)],
);

// ./test/core/float_exprs.wast:735
assert_return(
  () => invoke($39, `f64.i32.no_fold_trunc_u_convert_s`, [value("f64", -0.5)]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:736
assert_return(
  () => invoke($39, `f64.i32.no_fold_trunc_s_convert_u`, [value("f64", 1.5)]),
  [value("f64", 1)],
);

// ./test/core/float_exprs.wast:737
assert_return(
  () => invoke($39, `f64.i32.no_fold_trunc_s_convert_u`, [value("f64", -1.5)]),
  [value("f64", 4294967295)],
);

// ./test/core/float_exprs.wast:738
assert_return(
  () => invoke($39, `f64.i32.no_fold_trunc_u_convert_u`, [value("f64", 1.5)]),
  [value("f64", 1)],
);

// ./test/core/float_exprs.wast:739
assert_return(
  () => invoke($39, `f64.i32.no_fold_trunc_u_convert_u`, [value("f64", -0.5)]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:741
assert_return(
  () => invoke($39, `f32.i64.no_fold_trunc_s_convert_s`, [value("f32", 1.5)]),
  [value("f32", 1)],
);

// ./test/core/float_exprs.wast:742
assert_return(
  () => invoke($39, `f32.i64.no_fold_trunc_s_convert_s`, [value("f32", -1.5)]),
  [value("f32", -1)],
);

// ./test/core/float_exprs.wast:743
assert_return(
  () => invoke($39, `f32.i64.no_fold_trunc_u_convert_s`, [value("f32", 1.5)]),
  [value("f32", 1)],
);

// ./test/core/float_exprs.wast:744
assert_return(
  () => invoke($39, `f32.i64.no_fold_trunc_u_convert_s`, [value("f32", -0.5)]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:745
assert_return(
  () => invoke($39, `f32.i64.no_fold_trunc_s_convert_u`, [value("f32", 1.5)]),
  [value("f32", 1)],
);

// ./test/core/float_exprs.wast:746
assert_return(
  () => invoke($39, `f32.i64.no_fold_trunc_s_convert_u`, [value("f32", -1.5)]),
  [value("f32", 18446744000000000000)],
);

// ./test/core/float_exprs.wast:747
assert_return(
  () => invoke($39, `f32.i64.no_fold_trunc_u_convert_u`, [value("f32", 1.5)]),
  [value("f32", 1)],
);

// ./test/core/float_exprs.wast:748
assert_return(
  () => invoke($39, `f32.i64.no_fold_trunc_u_convert_u`, [value("f32", -0.5)]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:750
assert_return(
  () => invoke($39, `f64.i64.no_fold_trunc_s_convert_s`, [value("f64", 1.5)]),
  [value("f64", 1)],
);

// ./test/core/float_exprs.wast:751
assert_return(
  () => invoke($39, `f64.i64.no_fold_trunc_s_convert_s`, [value("f64", -1.5)]),
  [value("f64", -1)],
);

// ./test/core/float_exprs.wast:752
assert_return(
  () => invoke($39, `f64.i64.no_fold_trunc_u_convert_s`, [value("f64", 1.5)]),
  [value("f64", 1)],
);

// ./test/core/float_exprs.wast:753
assert_return(
  () => invoke($39, `f64.i64.no_fold_trunc_u_convert_s`, [value("f64", -0.5)]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:754
assert_return(
  () => invoke($39, `f64.i64.no_fold_trunc_s_convert_u`, [value("f64", 1.5)]),
  [value("f64", 1)],
);

// ./test/core/float_exprs.wast:755
assert_return(
  () => invoke($39, `f64.i64.no_fold_trunc_s_convert_u`, [value("f64", -1.5)]),
  [value("f64", 18446744073709552000)],
);

// ./test/core/float_exprs.wast:756
assert_return(
  () => invoke($39, `f64.i64.no_fold_trunc_u_convert_u`, [value("f64", 1.5)]),
  [value("f64", 1)],
);

// ./test/core/float_exprs.wast:757
assert_return(
  () => invoke($39, `f64.i64.no_fold_trunc_u_convert_u`, [value("f64", -0.5)]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:763
let $40 = instantiate(`(module
  (memory 1 1)
  (func (export "init") (param $$i i32) (param $$x f32) (f32.store (local.get $$i) (local.get $$x)))

  (func (export "run") (param $$n i32) (param $$z f32)
    (local $$i i32)
    (block $$exit
      (loop $$cont
        (f32.store
          (local.get $$i)
          (f32.div (f32.load (local.get $$i)) (local.get $$z))
        )
        (local.set $$i (i32.add (local.get $$i) (i32.const 4)))
        (br_if $$cont (i32.lt_u (local.get $$i) (local.get $$n)))
      )
    )
  )

  (func (export "check") (param $$i i32) (result f32) (f32.load (local.get $$i)))
)`);

// ./test/core/float_exprs.wast:784
invoke($40, `init`, [0, value("f32", 15.1)]);

// ./test/core/float_exprs.wast:785
invoke($40, `init`, [4, value("f32", 15.2)]);

// ./test/core/float_exprs.wast:786
invoke($40, `init`, [8, value("f32", 15.3)]);

// ./test/core/float_exprs.wast:787
invoke($40, `init`, [12, value("f32", 15.4)]);

// ./test/core/float_exprs.wast:788
assert_return(() => invoke($40, `check`, [0]), [value("f32", 15.1)]);

// ./test/core/float_exprs.wast:789
assert_return(() => invoke($40, `check`, [4]), [value("f32", 15.2)]);

// ./test/core/float_exprs.wast:790
assert_return(() => invoke($40, `check`, [8]), [value("f32", 15.3)]);

// ./test/core/float_exprs.wast:791
assert_return(() => invoke($40, `check`, [12]), [value("f32", 15.4)]);

// ./test/core/float_exprs.wast:792
invoke($40, `run`, [16, value("f32", 3)]);

// ./test/core/float_exprs.wast:793
assert_return(() => invoke($40, `check`, [0]), [value("f32", 5.0333333)]);

// ./test/core/float_exprs.wast:794
assert_return(() => invoke($40, `check`, [4]), [value("f32", 5.0666666)]);

// ./test/core/float_exprs.wast:795
assert_return(() => invoke($40, `check`, [8]), [value("f32", 5.1)]);

// ./test/core/float_exprs.wast:796
assert_return(() => invoke($40, `check`, [12]), [value("f32", 5.133333)]);

// ./test/core/float_exprs.wast:798
let $41 = instantiate(`(module
  (memory 1 1)
  (func (export "init") (param $$i i32) (param $$x f64) (f64.store (local.get $$i) (local.get $$x)))

  (func (export "run") (param $$n i32) (param $$z f64)
    (local $$i i32)
    (block $$exit
      (loop $$cont
        (f64.store
          (local.get $$i)
          (f64.div (f64.load (local.get $$i)) (local.get $$z))
        )
        (local.set $$i (i32.add (local.get $$i) (i32.const 8)))
        (br_if $$cont (i32.lt_u (local.get $$i) (local.get $$n)))
      )
    )
  )

  (func (export "check") (param $$i i32) (result f64) (f64.load (local.get $$i)))
)`);

// ./test/core/float_exprs.wast:819
invoke($41, `init`, [0, value("f64", 15.1)]);

// ./test/core/float_exprs.wast:820
invoke($41, `init`, [8, value("f64", 15.2)]);

// ./test/core/float_exprs.wast:821
invoke($41, `init`, [16, value("f64", 15.3)]);

// ./test/core/float_exprs.wast:822
invoke($41, `init`, [24, value("f64", 15.4)]);

// ./test/core/float_exprs.wast:823
assert_return(() => invoke($41, `check`, [0]), [value("f64", 15.1)]);

// ./test/core/float_exprs.wast:824
assert_return(() => invoke($41, `check`, [8]), [value("f64", 15.2)]);

// ./test/core/float_exprs.wast:825
assert_return(() => invoke($41, `check`, [16]), [value("f64", 15.3)]);

// ./test/core/float_exprs.wast:826
assert_return(() => invoke($41, `check`, [24]), [value("f64", 15.4)]);

// ./test/core/float_exprs.wast:827
invoke($41, `run`, [32, value("f64", 3)]);

// ./test/core/float_exprs.wast:828
assert_return(() => invoke($41, `check`, [0]), [
  value("f64", 5.033333333333333),
]);

// ./test/core/float_exprs.wast:829
assert_return(() => invoke($41, `check`, [8]), [
  value("f64", 5.066666666666666),
]);

// ./test/core/float_exprs.wast:830
assert_return(() => invoke($41, `check`, [16]), [
  value("f64", 5.1000000000000005),
]);

// ./test/core/float_exprs.wast:831
assert_return(() => invoke($41, `check`, [24]), [
  value("f64", 5.133333333333334),
]);

// ./test/core/float_exprs.wast:835
let $42 = instantiate(`(module
  (func (export "f32.ult") (param $$x f32) (param $$y f32) (result i32) (i32.eqz (f32.ge (local.get $$x) (local.get $$y))))
  (func (export "f32.ule") (param $$x f32) (param $$y f32) (result i32) (i32.eqz (f32.gt (local.get $$x) (local.get $$y))))
  (func (export "f32.ugt") (param $$x f32) (param $$y f32) (result i32) (i32.eqz (f32.le (local.get $$x) (local.get $$y))))
  (func (export "f32.uge") (param $$x f32) (param $$y f32) (result i32) (i32.eqz (f32.lt (local.get $$x) (local.get $$y))))

  (func (export "f64.ult") (param $$x f64) (param $$y f64) (result i32) (i32.eqz (f64.ge (local.get $$x) (local.get $$y))))
  (func (export "f64.ule") (param $$x f64) (param $$y f64) (result i32) (i32.eqz (f64.gt (local.get $$x) (local.get $$y))))
  (func (export "f64.ugt") (param $$x f64) (param $$y f64) (result i32) (i32.eqz (f64.le (local.get $$x) (local.get $$y))))
  (func (export "f64.uge") (param $$x f64) (param $$y f64) (result i32) (i32.eqz (f64.lt (local.get $$x) (local.get $$y))))
)`);

// ./test/core/float_exprs.wast:847
assert_return(
  () => invoke($42, `f32.ult`, [value("f32", 3), value("f32", 2)]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:848
assert_return(
  () => invoke($42, `f32.ult`, [value("f32", 2), value("f32", 2)]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:849
assert_return(
  () => invoke($42, `f32.ult`, [value("f32", 2), value("f32", 3)]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:850
assert_return(
  () =>
    invoke($42, `f32.ult`, [
      value("f32", 2),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:851
assert_return(
  () => invoke($42, `f32.ule`, [value("f32", 3), value("f32", 2)]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:852
assert_return(
  () => invoke($42, `f32.ule`, [value("f32", 2), value("f32", 2)]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:853
assert_return(
  () => invoke($42, `f32.ule`, [value("f32", 2), value("f32", 3)]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:854
assert_return(
  () =>
    invoke($42, `f32.ule`, [
      value("f32", 2),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:855
assert_return(
  () => invoke($42, `f32.ugt`, [value("f32", 3), value("f32", 2)]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:856
assert_return(
  () => invoke($42, `f32.ugt`, [value("f32", 2), value("f32", 2)]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:857
assert_return(
  () => invoke($42, `f32.ugt`, [value("f32", 2), value("f32", 3)]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:858
assert_return(
  () =>
    invoke($42, `f32.ugt`, [
      value("f32", 2),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:859
assert_return(
  () => invoke($42, `f32.uge`, [value("f32", 3), value("f32", 2)]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:860
assert_return(
  () => invoke($42, `f32.uge`, [value("f32", 2), value("f32", 2)]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:861
assert_return(
  () => invoke($42, `f32.uge`, [value("f32", 2), value("f32", 3)]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:862
assert_return(
  () =>
    invoke($42, `f32.uge`, [
      value("f32", 2),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:863
assert_return(
  () => invoke($42, `f64.ult`, [value("f64", 3), value("f64", 2)]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:864
assert_return(
  () => invoke($42, `f64.ult`, [value("f64", 2), value("f64", 2)]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:865
assert_return(
  () => invoke($42, `f64.ult`, [value("f64", 2), value("f64", 3)]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:866
assert_return(
  () =>
    invoke($42, `f64.ult`, [
      value("f64", 2),
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:867
assert_return(
  () => invoke($42, `f64.ule`, [value("f64", 3), value("f64", 2)]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:868
assert_return(
  () => invoke($42, `f64.ule`, [value("f64", 2), value("f64", 2)]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:869
assert_return(
  () => invoke($42, `f64.ule`, [value("f64", 2), value("f64", 3)]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:870
assert_return(
  () =>
    invoke($42, `f64.ule`, [
      value("f64", 2),
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:871
assert_return(
  () => invoke($42, `f64.ugt`, [value("f64", 3), value("f64", 2)]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:872
assert_return(
  () => invoke($42, `f64.ugt`, [value("f64", 2), value("f64", 2)]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:873
assert_return(
  () => invoke($42, `f64.ugt`, [value("f64", 2), value("f64", 3)]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:874
assert_return(
  () =>
    invoke($42, `f64.ugt`, [
      value("f64", 2),
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:875
assert_return(
  () => invoke($42, `f64.uge`, [value("f64", 3), value("f64", 2)]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:876
assert_return(
  () => invoke($42, `f64.uge`, [value("f64", 2), value("f64", 2)]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:877
assert_return(
  () => invoke($42, `f64.uge`, [value("f64", 2), value("f64", 3)]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:878
assert_return(
  () =>
    invoke($42, `f64.uge`, [
      value("f64", 2),
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:882
let $43 = instantiate(`(module
  (func (export "f32.no_fold_lt_select") (param $$x f32) (param $$y f32) (result f32) (select (local.get $$x) (local.get $$y) (f32.lt (local.get $$x) (local.get $$y))))
  (func (export "f32.no_fold_le_select") (param $$x f32) (param $$y f32) (result f32) (select (local.get $$x) (local.get $$y) (f32.le (local.get $$x) (local.get $$y))))
  (func (export "f32.no_fold_gt_select") (param $$x f32) (param $$y f32) (result f32) (select (local.get $$x) (local.get $$y) (f32.gt (local.get $$x) (local.get $$y))))
  (func (export "f32.no_fold_ge_select") (param $$x f32) (param $$y f32) (result f32) (select (local.get $$x) (local.get $$y) (f32.ge (local.get $$x) (local.get $$y))))

  (func (export "f64.no_fold_lt_select") (param $$x f64) (param $$y f64) (result f64) (select (local.get $$x) (local.get $$y) (f64.lt (local.get $$x) (local.get $$y))))
  (func (export "f64.no_fold_le_select") (param $$x f64) (param $$y f64) (result f64) (select (local.get $$x) (local.get $$y) (f64.le (local.get $$x) (local.get $$y))))
  (func (export "f64.no_fold_gt_select") (param $$x f64) (param $$y f64) (result f64) (select (local.get $$x) (local.get $$y) (f64.gt (local.get $$x) (local.get $$y))))
  (func (export "f64.no_fold_ge_select") (param $$x f64) (param $$y f64) (result f64) (select (local.get $$x) (local.get $$y) (f64.ge (local.get $$x) (local.get $$y))))
)`);

// ./test/core/float_exprs.wast:894
assert_return(
  () =>
    invoke($43, `f32.no_fold_lt_select`, [
      value("f32", 0),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])],
);

// ./test/core/float_exprs.wast:895
assert_return(
  () =>
    invoke($43, `f32.no_fold_lt_select`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0),
    ]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:896
assert_return(
  () =>
    invoke($43, `f32.no_fold_lt_select`, [value("f32", 0), value("f32", -0)]),
  [value("f32", -0)],
);

// ./test/core/float_exprs.wast:897
assert_return(
  () =>
    invoke($43, `f32.no_fold_lt_select`, [value("f32", -0), value("f32", 0)]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:898
assert_return(
  () =>
    invoke($43, `f32.no_fold_le_select`, [
      value("f32", 0),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])],
);

// ./test/core/float_exprs.wast:899
assert_return(
  () =>
    invoke($43, `f32.no_fold_le_select`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0),
    ]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:900
assert_return(
  () =>
    invoke($43, `f32.no_fold_le_select`, [value("f32", 0), value("f32", -0)]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:901
assert_return(
  () =>
    invoke($43, `f32.no_fold_le_select`, [value("f32", -0), value("f32", 0)]),
  [value("f32", -0)],
);

// ./test/core/float_exprs.wast:902
assert_return(
  () =>
    invoke($43, `f32.no_fold_gt_select`, [
      value("f32", 0),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])],
);

// ./test/core/float_exprs.wast:903
assert_return(
  () =>
    invoke($43, `f32.no_fold_gt_select`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0),
    ]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:904
assert_return(
  () =>
    invoke($43, `f32.no_fold_gt_select`, [value("f32", 0), value("f32", -0)]),
  [value("f32", -0)],
);

// ./test/core/float_exprs.wast:905
assert_return(
  () =>
    invoke($43, `f32.no_fold_gt_select`, [value("f32", -0), value("f32", 0)]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:906
assert_return(
  () =>
    invoke($43, `f32.no_fold_ge_select`, [
      value("f32", 0),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])],
);

// ./test/core/float_exprs.wast:907
assert_return(
  () =>
    invoke($43, `f32.no_fold_ge_select`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0),
    ]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:908
assert_return(
  () =>
    invoke($43, `f32.no_fold_ge_select`, [value("f32", 0), value("f32", -0)]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:909
assert_return(
  () =>
    invoke($43, `f32.no_fold_ge_select`, [value("f32", -0), value("f32", 0)]),
  [value("f32", -0)],
);

// ./test/core/float_exprs.wast:910
assert_return(
  () =>
    invoke($43, `f64.no_fold_lt_select`, [
      value("f64", 0),
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])],
);

// ./test/core/float_exprs.wast:911
assert_return(
  () =>
    invoke($43, `f64.no_fold_lt_select`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
      value("f64", 0),
    ]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:912
assert_return(
  () =>
    invoke($43, `f64.no_fold_lt_select`, [value("f64", 0), value("f64", -0)]),
  [value("f64", -0)],
);

// ./test/core/float_exprs.wast:913
assert_return(
  () =>
    invoke($43, `f64.no_fold_lt_select`, [value("f64", -0), value("f64", 0)]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:914
assert_return(
  () =>
    invoke($43, `f64.no_fold_le_select`, [
      value("f64", 0),
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])],
);

// ./test/core/float_exprs.wast:915
assert_return(
  () =>
    invoke($43, `f64.no_fold_le_select`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
      value("f64", 0),
    ]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:916
assert_return(
  () =>
    invoke($43, `f64.no_fold_le_select`, [value("f64", 0), value("f64", -0)]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:917
assert_return(
  () =>
    invoke($43, `f64.no_fold_le_select`, [value("f64", -0), value("f64", 0)]),
  [value("f64", -0)],
);

// ./test/core/float_exprs.wast:918
assert_return(
  () =>
    invoke($43, `f64.no_fold_gt_select`, [
      value("f64", 0),
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])],
);

// ./test/core/float_exprs.wast:919
assert_return(
  () =>
    invoke($43, `f64.no_fold_gt_select`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
      value("f64", 0),
    ]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:920
assert_return(
  () =>
    invoke($43, `f64.no_fold_gt_select`, [value("f64", 0), value("f64", -0)]),
  [value("f64", -0)],
);

// ./test/core/float_exprs.wast:921
assert_return(
  () =>
    invoke($43, `f64.no_fold_gt_select`, [value("f64", -0), value("f64", 0)]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:922
assert_return(
  () =>
    invoke($43, `f64.no_fold_ge_select`, [
      value("f64", 0),
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])],
);

// ./test/core/float_exprs.wast:923
assert_return(
  () =>
    invoke($43, `f64.no_fold_ge_select`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
      value("f64", 0),
    ]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:924
assert_return(
  () =>
    invoke($43, `f64.no_fold_ge_select`, [value("f64", 0), value("f64", -0)]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:925
assert_return(
  () =>
    invoke($43, `f64.no_fold_ge_select`, [value("f64", -0), value("f64", 0)]),
  [value("f64", -0)],
);

// ./test/core/float_exprs.wast:929
let $44 = instantiate(`(module
  (func (export "f32.no_fold_lt_if") (param $$x f32) (param $$y f32) (result f32)
    (if (result f32) (f32.lt (local.get $$x) (local.get $$y))
      (then (local.get $$x)) (else (local.get $$y))
    )
  )
  (func (export "f32.no_fold_le_if") (param $$x f32) (param $$y f32) (result f32)
    (if (result f32) (f32.le (local.get $$x) (local.get $$y))
      (then (local.get $$x)) (else (local.get $$y))
    )
  )
  (func (export "f32.no_fold_gt_if") (param $$x f32) (param $$y f32) (result f32)
    (if (result f32) (f32.gt (local.get $$x) (local.get $$y))
      (then (local.get $$x)) (else (local.get $$y))
    )
  )
  (func (export "f32.no_fold_ge_if") (param $$x f32) (param $$y f32) (result f32)
    (if (result f32) (f32.ge (local.get $$x) (local.get $$y))
      (then (local.get $$x)) (else (local.get $$y))
    )
  )

  (func (export "f64.no_fold_lt_if") (param $$x f64) (param $$y f64) (result f64)
    (if (result f64) (f64.lt (local.get $$x) (local.get $$y))
      (then (local.get $$x)) (else (local.get $$y))
    )
  )
  (func (export "f64.no_fold_le_if") (param $$x f64) (param $$y f64) (result f64)
    (if (result f64) (f64.le (local.get $$x) (local.get $$y))
      (then (local.get $$x)) (else (local.get $$y))
    )
  )
  (func (export "f64.no_fold_gt_if") (param $$x f64) (param $$y f64) (result f64)
    (if (result f64) (f64.gt (local.get $$x) (local.get $$y))
      (then (local.get $$x)) (else (local.get $$y))
    )
  )
  (func (export "f64.no_fold_ge_if") (param $$x f64) (param $$y f64) (result f64)
    (if (result f64) (f64.ge (local.get $$x) (local.get $$y))
      (then (local.get $$x)) (else (local.get $$y))
    )
  )
)`);

// ./test/core/float_exprs.wast:973
assert_return(
  () =>
    invoke($44, `f32.no_fold_lt_if`, [
      value("f32", 0),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])],
);

// ./test/core/float_exprs.wast:974
assert_return(
  () =>
    invoke($44, `f32.no_fold_lt_if`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0),
    ]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:975
assert_return(
  () => invoke($44, `f32.no_fold_lt_if`, [value("f32", 0), value("f32", -0)]),
  [value("f32", -0)],
);

// ./test/core/float_exprs.wast:976
assert_return(
  () => invoke($44, `f32.no_fold_lt_if`, [value("f32", -0), value("f32", 0)]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:977
assert_return(
  () =>
    invoke($44, `f32.no_fold_le_if`, [
      value("f32", 0),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])],
);

// ./test/core/float_exprs.wast:978
assert_return(
  () =>
    invoke($44, `f32.no_fold_le_if`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0),
    ]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:979
assert_return(
  () => invoke($44, `f32.no_fold_le_if`, [value("f32", 0), value("f32", -0)]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:980
assert_return(
  () => invoke($44, `f32.no_fold_le_if`, [value("f32", -0), value("f32", 0)]),
  [value("f32", -0)],
);

// ./test/core/float_exprs.wast:981
assert_return(
  () =>
    invoke($44, `f32.no_fold_gt_if`, [
      value("f32", 0),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])],
);

// ./test/core/float_exprs.wast:982
assert_return(
  () =>
    invoke($44, `f32.no_fold_gt_if`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0),
    ]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:983
assert_return(
  () => invoke($44, `f32.no_fold_gt_if`, [value("f32", 0), value("f32", -0)]),
  [value("f32", -0)],
);

// ./test/core/float_exprs.wast:984
assert_return(
  () => invoke($44, `f32.no_fold_gt_if`, [value("f32", -0), value("f32", 0)]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:985
assert_return(
  () =>
    invoke($44, `f32.no_fold_ge_if`, [
      value("f32", 0),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])],
);

// ./test/core/float_exprs.wast:986
assert_return(
  () =>
    invoke($44, `f32.no_fold_ge_if`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0),
    ]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:987
assert_return(
  () => invoke($44, `f32.no_fold_ge_if`, [value("f32", 0), value("f32", -0)]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:988
assert_return(
  () => invoke($44, `f32.no_fold_ge_if`, [value("f32", -0), value("f32", 0)]),
  [value("f32", -0)],
);

// ./test/core/float_exprs.wast:989
assert_return(
  () =>
    invoke($44, `f64.no_fold_lt_if`, [
      value("f64", 0),
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])],
);

// ./test/core/float_exprs.wast:990
assert_return(
  () =>
    invoke($44, `f64.no_fold_lt_if`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
      value("f64", 0),
    ]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:991
assert_return(
  () => invoke($44, `f64.no_fold_lt_if`, [value("f64", 0), value("f64", -0)]),
  [value("f64", -0)],
);

// ./test/core/float_exprs.wast:992
assert_return(
  () => invoke($44, `f64.no_fold_lt_if`, [value("f64", -0), value("f64", 0)]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:993
assert_return(
  () =>
    invoke($44, `f64.no_fold_le_if`, [
      value("f64", 0),
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])],
);

// ./test/core/float_exprs.wast:994
assert_return(
  () =>
    invoke($44, `f64.no_fold_le_if`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
      value("f64", 0),
    ]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:995
assert_return(
  () => invoke($44, `f64.no_fold_le_if`, [value("f64", 0), value("f64", -0)]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:996
assert_return(
  () => invoke($44, `f64.no_fold_le_if`, [value("f64", -0), value("f64", 0)]),
  [value("f64", -0)],
);

// ./test/core/float_exprs.wast:997
assert_return(
  () =>
    invoke($44, `f64.no_fold_gt_if`, [
      value("f64", 0),
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])],
);

// ./test/core/float_exprs.wast:998
assert_return(
  () =>
    invoke($44, `f64.no_fold_gt_if`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
      value("f64", 0),
    ]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:999
assert_return(
  () => invoke($44, `f64.no_fold_gt_if`, [value("f64", 0), value("f64", -0)]),
  [value("f64", -0)],
);

// ./test/core/float_exprs.wast:1000
assert_return(
  () => invoke($44, `f64.no_fold_gt_if`, [value("f64", -0), value("f64", 0)]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:1001
assert_return(
  () =>
    invoke($44, `f64.no_fold_ge_if`, [
      value("f64", 0),
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])],
);

// ./test/core/float_exprs.wast:1002
assert_return(
  () =>
    invoke($44, `f64.no_fold_ge_if`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
      value("f64", 0),
    ]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:1003
assert_return(
  () => invoke($44, `f64.no_fold_ge_if`, [value("f64", 0), value("f64", -0)]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:1004
assert_return(
  () => invoke($44, `f64.no_fold_ge_if`, [value("f64", -0), value("f64", 0)]),
  [value("f64", -0)],
);

// ./test/core/float_exprs.wast:1008
let $45 = instantiate(`(module
  (func (export "f32.no_fold_lt_select_to_abs") (param $$x f32) (result f32) (select (f32.neg (local.get $$x)) (local.get $$x) (f32.lt (local.get $$x) (f32.const 0.0))))
  (func (export "f32.no_fold_le_select_to_abs") (param $$x f32) (result f32) (select (f32.neg (local.get $$x)) (local.get $$x) (f32.le (local.get $$x) (f32.const -0.0))))
  (func (export "f32.no_fold_gt_select_to_abs") (param $$x f32) (result f32) (select (local.get $$x) (f32.neg (local.get $$x)) (f32.gt (local.get $$x) (f32.const -0.0))))
  (func (export "f32.no_fold_ge_select_to_abs") (param $$x f32) (result f32) (select (local.get $$x) (f32.neg (local.get $$x)) (f32.ge (local.get $$x) (f32.const 0.0))))

  (func (export "f64.no_fold_lt_select_to_abs") (param $$x f64) (result f64) (select (f64.neg (local.get $$x)) (local.get $$x) (f64.lt (local.get $$x) (f64.const 0.0))))
  (func (export "f64.no_fold_le_select_to_abs") (param $$x f64) (result f64) (select (f64.neg (local.get $$x)) (local.get $$x) (f64.le (local.get $$x) (f64.const -0.0))))
  (func (export "f64.no_fold_gt_select_to_abs") (param $$x f64) (result f64) (select (local.get $$x) (f64.neg (local.get $$x)) (f64.gt (local.get $$x) (f64.const -0.0))))
  (func (export "f64.no_fold_ge_select_to_abs") (param $$x f64) (result f64) (select (local.get $$x) (f64.neg (local.get $$x)) (f64.ge (local.get $$x) (f64.const 0.0))))
)`);

// ./test/core/float_exprs.wast:1020
assert_return(
  () =>
    invoke($45, `f32.no_fold_lt_select_to_abs`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])],
);

// ./test/core/float_exprs.wast:1021
assert_return(
  () =>
    invoke($45, `f32.no_fold_lt_select_to_abs`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [bytes("f32", [0x0, 0x0, 0xc0, 0xff])],
);

// ./test/core/float_exprs.wast:1022
assert_return(
  () => invoke($45, `f32.no_fold_lt_select_to_abs`, [value("f32", 0)]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:1023
assert_return(
  () => invoke($45, `f32.no_fold_lt_select_to_abs`, [value("f32", -0)]),
  [value("f32", -0)],
);

// ./test/core/float_exprs.wast:1024
assert_return(
  () =>
    invoke($45, `f32.no_fold_le_select_to_abs`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])],
);

// ./test/core/float_exprs.wast:1025
assert_return(
  () =>
    invoke($45, `f32.no_fold_le_select_to_abs`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [bytes("f32", [0x0, 0x0, 0xc0, 0xff])],
);

// ./test/core/float_exprs.wast:1026
assert_return(
  () => invoke($45, `f32.no_fold_le_select_to_abs`, [value("f32", 0)]),
  [value("f32", -0)],
);

// ./test/core/float_exprs.wast:1027
assert_return(
  () => invoke($45, `f32.no_fold_le_select_to_abs`, [value("f32", -0)]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:1028
assert_return(
  () =>
    invoke($45, `f32.no_fold_gt_select_to_abs`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [bytes("f32", [0x0, 0x0, 0xa0, 0xff])],
);

// ./test/core/float_exprs.wast:1029
assert_return(
  () =>
    invoke($45, `f32.no_fold_gt_select_to_abs`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])],
);

// ./test/core/float_exprs.wast:1030
assert_return(
  () => invoke($45, `f32.no_fold_gt_select_to_abs`, [value("f32", 0)]),
  [value("f32", -0)],
);

// ./test/core/float_exprs.wast:1031
assert_return(
  () => invoke($45, `f32.no_fold_gt_select_to_abs`, [value("f32", -0)]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:1032
assert_return(
  () =>
    invoke($45, `f32.no_fold_ge_select_to_abs`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [bytes("f32", [0x0, 0x0, 0xa0, 0xff])],
);

// ./test/core/float_exprs.wast:1033
assert_return(
  () =>
    invoke($45, `f32.no_fold_ge_select_to_abs`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])],
);

// ./test/core/float_exprs.wast:1034
assert_return(
  () => invoke($45, `f32.no_fold_ge_select_to_abs`, [value("f32", 0)]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:1035
assert_return(
  () => invoke($45, `f32.no_fold_ge_select_to_abs`, [value("f32", -0)]),
  [value("f32", -0)],
);

// ./test/core/float_exprs.wast:1036
assert_return(
  () =>
    invoke($45, `f64.no_fold_lt_select_to_abs`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f]),
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f])],
);

// ./test/core/float_exprs.wast:1037
assert_return(
  () =>
    invoke($45, `f64.no_fold_lt_select_to_abs`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff]),
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff])],
);

// ./test/core/float_exprs.wast:1038
assert_return(
  () => invoke($45, `f64.no_fold_lt_select_to_abs`, [value("f64", 0)]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:1039
assert_return(
  () => invoke($45, `f64.no_fold_lt_select_to_abs`, [value("f64", -0)]),
  [value("f64", -0)],
);

// ./test/core/float_exprs.wast:1040
assert_return(
  () =>
    invoke($45, `f64.no_fold_le_select_to_abs`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f]),
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f])],
);

// ./test/core/float_exprs.wast:1041
assert_return(
  () =>
    invoke($45, `f64.no_fold_le_select_to_abs`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff]),
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff])],
);

// ./test/core/float_exprs.wast:1042
assert_return(
  () => invoke($45, `f64.no_fold_le_select_to_abs`, [value("f64", 0)]),
  [value("f64", -0)],
);

// ./test/core/float_exprs.wast:1043
assert_return(
  () => invoke($45, `f64.no_fold_le_select_to_abs`, [value("f64", -0)]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:1044
assert_return(
  () =>
    invoke($45, `f64.no_fold_gt_select_to_abs`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f]),
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0xff])],
);

// ./test/core/float_exprs.wast:1045
assert_return(
  () =>
    invoke($45, `f64.no_fold_gt_select_to_abs`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff]),
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])],
);

// ./test/core/float_exprs.wast:1046
assert_return(
  () => invoke($45, `f64.no_fold_gt_select_to_abs`, [value("f64", 0)]),
  [value("f64", -0)],
);

// ./test/core/float_exprs.wast:1047
assert_return(
  () => invoke($45, `f64.no_fold_gt_select_to_abs`, [value("f64", -0)]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:1048
assert_return(
  () =>
    invoke($45, `f64.no_fold_ge_select_to_abs`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f]),
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0xff])],
);

// ./test/core/float_exprs.wast:1049
assert_return(
  () =>
    invoke($45, `f64.no_fold_ge_select_to_abs`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff]),
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])],
);

// ./test/core/float_exprs.wast:1050
assert_return(
  () => invoke($45, `f64.no_fold_ge_select_to_abs`, [value("f64", 0)]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:1051
assert_return(
  () => invoke($45, `f64.no_fold_ge_select_to_abs`, [value("f64", -0)]),
  [value("f64", -0)],
);

// ./test/core/float_exprs.wast:1055
let $46 = instantiate(`(module
  (func (export "f32.no_fold_lt_if_to_abs") (param $$x f32) (result f32)
    (if (result f32) (f32.lt (local.get $$x) (f32.const 0.0))
      (then (f32.neg (local.get $$x))) (else (local.get $$x))
    )
  )
  (func (export "f32.no_fold_le_if_to_abs") (param $$x f32) (result f32)
    (if (result f32) (f32.le (local.get $$x) (f32.const -0.0))
      (then (f32.neg (local.get $$x))) (else (local.get $$x))
    )
  )
  (func (export "f32.no_fold_gt_if_to_abs") (param $$x f32) (result f32)
    (if (result f32) (f32.gt (local.get $$x) (f32.const -0.0))
      (then (local.get $$x)) (else (f32.neg (local.get $$x)))
    )
  )
  (func (export "f32.no_fold_ge_if_to_abs") (param $$x f32) (result f32)
    (if (result f32) (f32.ge (local.get $$x) (f32.const 0.0))
      (then (local.get $$x)) (else (f32.neg (local.get $$x)))
    )
  )

  (func (export "f64.no_fold_lt_if_to_abs") (param $$x f64) (result f64)
    (if (result f64) (f64.lt (local.get $$x) (f64.const 0.0))
      (then (f64.neg (local.get $$x))) (else (local.get $$x))
    )
  )
  (func (export "f64.no_fold_le_if_to_abs") (param $$x f64) (result f64)
    (if (result f64) (f64.le (local.get $$x) (f64.const -0.0))
      (then (f64.neg (local.get $$x))) (else (local.get $$x))
    )
  )
  (func (export "f64.no_fold_gt_if_to_abs") (param $$x f64) (result f64)
    (if (result f64) (f64.gt (local.get $$x) (f64.const -0.0))
      (then (local.get $$x)) (else (f64.neg (local.get $$x)))
    )
  )
  (func (export "f64.no_fold_ge_if_to_abs") (param $$x f64) (result f64)
    (if (result f64) (f64.ge (local.get $$x) (f64.const 0.0))
      (then (local.get $$x)) (else (f64.neg (local.get $$x)))
    )
  )
)`);

// ./test/core/float_exprs.wast:1099
assert_return(
  () =>
    invoke($46, `f32.no_fold_lt_if_to_abs`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])],
);

// ./test/core/float_exprs.wast:1100
assert_return(
  () =>
    invoke($46, `f32.no_fold_lt_if_to_abs`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [bytes("f32", [0x0, 0x0, 0xc0, 0xff])],
);

// ./test/core/float_exprs.wast:1101
assert_return(
  () => invoke($46, `f32.no_fold_lt_if_to_abs`, [value("f32", 0)]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:1102
assert_return(
  () => invoke($46, `f32.no_fold_lt_if_to_abs`, [value("f32", -0)]),
  [value("f32", -0)],
);

// ./test/core/float_exprs.wast:1103
assert_return(
  () =>
    invoke($46, `f32.no_fold_le_if_to_abs`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])],
);

// ./test/core/float_exprs.wast:1104
assert_return(
  () =>
    invoke($46, `f32.no_fold_le_if_to_abs`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [bytes("f32", [0x0, 0x0, 0xc0, 0xff])],
);

// ./test/core/float_exprs.wast:1105
assert_return(
  () => invoke($46, `f32.no_fold_le_if_to_abs`, [value("f32", 0)]),
  [value("f32", -0)],
);

// ./test/core/float_exprs.wast:1106
assert_return(
  () => invoke($46, `f32.no_fold_le_if_to_abs`, [value("f32", -0)]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:1107
assert_return(
  () =>
    invoke($46, `f32.no_fold_gt_if_to_abs`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [bytes("f32", [0x0, 0x0, 0xa0, 0xff])],
);

// ./test/core/float_exprs.wast:1108
assert_return(
  () =>
    invoke($46, `f32.no_fold_gt_if_to_abs`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])],
);

// ./test/core/float_exprs.wast:1109
assert_return(
  () => invoke($46, `f32.no_fold_gt_if_to_abs`, [value("f32", 0)]),
  [value("f32", -0)],
);

// ./test/core/float_exprs.wast:1110
assert_return(
  () => invoke($46, `f32.no_fold_gt_if_to_abs`, [value("f32", -0)]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:1111
assert_return(
  () =>
    invoke($46, `f32.no_fold_ge_if_to_abs`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [bytes("f32", [0x0, 0x0, 0xa0, 0xff])],
);

// ./test/core/float_exprs.wast:1112
assert_return(
  () =>
    invoke($46, `f32.no_fold_ge_if_to_abs`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])],
);

// ./test/core/float_exprs.wast:1113
assert_return(
  () => invoke($46, `f32.no_fold_ge_if_to_abs`, [value("f32", 0)]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:1114
assert_return(
  () => invoke($46, `f32.no_fold_ge_if_to_abs`, [value("f32", -0)]),
  [value("f32", -0)],
);

// ./test/core/float_exprs.wast:1115
assert_return(
  () =>
    invoke($46, `f64.no_fold_lt_if_to_abs`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f]),
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f])],
);

// ./test/core/float_exprs.wast:1116
assert_return(
  () =>
    invoke($46, `f64.no_fold_lt_if_to_abs`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff]),
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff])],
);

// ./test/core/float_exprs.wast:1117
assert_return(
  () => invoke($46, `f64.no_fold_lt_if_to_abs`, [value("f64", 0)]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:1118
assert_return(
  () => invoke($46, `f64.no_fold_lt_if_to_abs`, [value("f64", -0)]),
  [value("f64", -0)],
);

// ./test/core/float_exprs.wast:1119
assert_return(
  () =>
    invoke($46, `f64.no_fold_le_if_to_abs`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f]),
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f])],
);

// ./test/core/float_exprs.wast:1120
assert_return(
  () =>
    invoke($46, `f64.no_fold_le_if_to_abs`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff]),
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff])],
);

// ./test/core/float_exprs.wast:1121
assert_return(
  () => invoke($46, `f64.no_fold_le_if_to_abs`, [value("f64", 0)]),
  [value("f64", -0)],
);

// ./test/core/float_exprs.wast:1122
assert_return(
  () => invoke($46, `f64.no_fold_le_if_to_abs`, [value("f64", -0)]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:1123
assert_return(
  () =>
    invoke($46, `f64.no_fold_gt_if_to_abs`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f]),
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0xff])],
);

// ./test/core/float_exprs.wast:1124
assert_return(
  () =>
    invoke($46, `f64.no_fold_gt_if_to_abs`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff]),
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])],
);

// ./test/core/float_exprs.wast:1125
assert_return(
  () => invoke($46, `f64.no_fold_gt_if_to_abs`, [value("f64", 0)]),
  [value("f64", -0)],
);

// ./test/core/float_exprs.wast:1126
assert_return(
  () => invoke($46, `f64.no_fold_gt_if_to_abs`, [value("f64", -0)]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:1127
assert_return(
  () =>
    invoke($46, `f64.no_fold_ge_if_to_abs`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f]),
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0xff])],
);

// ./test/core/float_exprs.wast:1128
assert_return(
  () =>
    invoke($46, `f64.no_fold_ge_if_to_abs`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0xff]),
    ]),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f])],
);

// ./test/core/float_exprs.wast:1129
assert_return(
  () => invoke($46, `f64.no_fold_ge_if_to_abs`, [value("f64", 0)]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:1130
assert_return(
  () => invoke($46, `f64.no_fold_ge_if_to_abs`, [value("f64", -0)]),
  [value("f64", -0)],
);

// ./test/core/float_exprs.wast:1135
let $47 = instantiate(`(module
  (func (export "f32.incorrect_correction") (result f32)
    (f32.sub (f32.sub (f32.add (f32.const 1.333) (f32.const 1.225)) (f32.const 1.333)) (f32.const 1.225))
  )
  (func (export "f64.incorrect_correction") (result f64)
    (f64.sub (f64.sub (f64.add (f64.const 1.333) (f64.const 1.225)) (f64.const 1.333)) (f64.const 1.225))
  )
)`);

// ./test/core/float_exprs.wast:1144
assert_return(() => invoke($47, `f32.incorrect_correction`, []), [
  value("f32", 0.00000011920929),
]);

// ./test/core/float_exprs.wast:1145
assert_return(() => invoke($47, `f64.incorrect_correction`, []), [
  value("f64", -0.0000000000000002220446049250313),
]);

// ./test/core/float_exprs.wast:1150
let $48 = instantiate(`(module
  (func (export "calculate") (result f32)
    (local $$x f32)
    (local $$r f32)
    (local $$q f32)
    (local $$z0 f32)
    (local $$z1 f32)
    (local.set $$x (f32.const 156.25))
    (local.set $$r (f32.const 208.333333334))
    (local.set $$q (f32.const 1.77951304201))
    (local.set $$z0 (f32.div (f32.mul (f32.neg (local.get $$r)) (local.get $$x)) (f32.sub (f32.mul (local.get $$x) (local.get $$q)) (local.get $$r))))
    (local.set $$z1 (f32.div (f32.mul (f32.neg (local.get $$r)) (local.get $$x)) (f32.sub (f32.mul (local.get $$x) (local.get $$q)) (local.get $$r))))
    (block (br_if 0 (f32.eq (local.get $$z0) (local.get $$z1))) (unreachable))
    (local.get $$z1)
  )
)`);

// ./test/core/float_exprs.wast:1167
assert_return(() => invoke($48, `calculate`, []), [value("f32", -466.92685)]);

// ./test/core/float_exprs.wast:1169
let $49 = instantiate(`(module
  (func (export "calculate") (result f64)
    (local $$x f64)
    (local $$r f64)
    (local $$q f64)
    (local $$z0 f64)
    (local $$z1 f64)
    (local.set $$x (f64.const 156.25))
    (local.set $$r (f64.const 208.333333334))
    (local.set $$q (f64.const 1.77951304201))
    (local.set $$z0 (f64.div (f64.mul (f64.neg (local.get $$r)) (local.get $$x)) (f64.sub (f64.mul (local.get $$x) (local.get $$q)) (local.get $$r))))
    (local.set $$z1 (f64.div (f64.mul (f64.neg (local.get $$r)) (local.get $$x)) (f64.sub (f64.mul (local.get $$x) (local.get $$q)) (local.get $$r))))
    (block (br_if 0 (f64.eq (local.get $$z0) (local.get $$z1))) (unreachable))
    (local.get $$z1)
  )
)`);

// ./test/core/float_exprs.wast:1186
assert_return(() => invoke($49, `calculate`, []), [
  value("f64", -466.926956301738),
]);

// ./test/core/float_exprs.wast:1191
let $50 = instantiate(`(module
  (func (export "llvm_pr26746") (param $$x f32) (result f32)
    (f32.sub (f32.const 0.0) (f32.sub (f32.const -0.0) (local.get $$x)))
  )
)`);

// ./test/core/float_exprs.wast:1197
assert_return(() => invoke($50, `llvm_pr26746`, [value("f32", -0)]), [
  value("f32", 0),
]);

// ./test/core/float_exprs.wast:1202
let $51 = instantiate(`(module
  (func (export "llvm_pr27153") (param $$x i32) (result f32)
    (f32.add (f32.convert_i32_s (i32.and (local.get $$x) (i32.const 268435455))) (f32.const -8388608.0))
  )
)`);

// ./test/core/float_exprs.wast:1208
assert_return(() => invoke($51, `llvm_pr27153`, [33554434]), [
  value("f32", 25165824),
]);

// ./test/core/float_exprs.wast:1213
let $52 = instantiate(`(module
  (func (export "llvm_pr27036") (param $$x i32) (param $$y i32) (result f32)
    (f32.add (f32.convert_i32_s (i32.or (local.get $$x) (i32.const -25034805)))
             (f32.convert_i32_s (i32.and (local.get $$y) (i32.const 14942208))))
  )
)`);

// ./test/core/float_exprs.wast:1220
assert_return(() => invoke($52, `llvm_pr27036`, [-25034805, 14942208]), [
  value("f32", -10092596),
]);

// ./test/core/float_exprs.wast:1230
let $53 = instantiate(`(module
  (func (export "thepast0") (param $$a f64) (param $$b f64) (param $$c f64) (param $$d f64) (result f64)
    (f64.div (f64.mul (local.get $$a) (local.get $$b)) (f64.mul (local.get $$c) (local.get $$d)))
  )

  (func (export "thepast1") (param $$a f64) (param $$b f64) (param $$c f64) (result f64)
    (f64.sub (f64.mul (local.get $$a) (local.get $$b)) (local.get $$c))
  )

  (func (export "thepast2") (param $$a f32) (param $$b f32) (param $$c f32) (result f32)
    (f32.mul (f32.mul (local.get $$a) (local.get $$b)) (local.get $$c))
  )
)`);

// ./test/core/float_exprs.wast:1244
assert_return(
  () =>
    invoke($53, `thepast0`, [
      value(
        "f64",
        0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000004450147717014403,
      ),
      value("f64", 0.9999999999999999),
      value("f64", 2),
      value("f64", 0.5),
    ]),
  [value(
    "f64",
    0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000044501477170144023,
  )],
);

// ./test/core/float_exprs.wast:1245
assert_return(
  () =>
    invoke($53, `thepast1`, [
      value("f64", 0.00000000000000005551115123125783),
      value("f64", 0.9999999999999999),
      value("f64", 0.00000000000000005551115123125783),
    ]),
  [value("f64", -0.000000000000000000000000000000006162975822039155)],
);

// ./test/core/float_exprs.wast:1246
assert_return(
  () =>
    invoke($53, `thepast2`, [
      value("f32", 0.000000000000000000000000000000000000023509887),
      value("f32", 0.5),
      value("f32", 1),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/float_exprs.wast:1251
let $54 = instantiate(`(module
  (func (export "inverse") (param $$x f32) (result f32)
    (f32.div (f32.const 1.0) (local.get $$x))
  )
)`);

// ./test/core/float_exprs.wast:1257
assert_return(() => invoke($54, `inverse`, [value("f32", 96)]), [
  value("f32", 0.010416667),
]);

// ./test/core/float_exprs.wast:1262
let $55 = instantiate(`(module
  (func (export "f32_sqrt_minus_2") (param $$x f32) (result f32)
    (f32.sub (f32.sqrt (local.get $$x)) (f32.const 2.0))
  )

  (func (export "f64_sqrt_minus_2") (param $$x f64) (result f64)
    (f64.sub (f64.sqrt (local.get $$x)) (f64.const 2.0))
  )
)`);

// ./test/core/float_exprs.wast:1272
assert_return(() => invoke($55, `f32_sqrt_minus_2`, [value("f32", 4)]), [
  value("f32", 0),
]);

// ./test/core/float_exprs.wast:1273
assert_return(() => invoke($55, `f64_sqrt_minus_2`, [value("f64", 4)]), [
  value("f64", 0),
]);

// ./test/core/float_exprs.wast:1277
let $56 = instantiate(`(module
  (func (export "f32.no_fold_recip_recip") (param $$x f32) (result f32)
    (f32.div (f32.const 1.0) (f32.div (f32.const 1.0) (local.get $$x))))

  (func (export "f64.no_fold_recip_recip") (param $$x f64) (result f64)
    (f64.div (f64.const 1.0) (f64.div (f64.const 1.0) (local.get $$x))))
)`);

// ./test/core/float_exprs.wast:1285
assert_return(
  () =>
    invoke($56, `f32.no_fold_recip_recip`, [
      value("f32", -70435790000000000000),
    ]),
  [value("f32", -70435784000000000000)],
);

// ./test/core/float_exprs.wast:1286
assert_return(
  () =>
    invoke($56, `f32.no_fold_recip_recip`, [
      value("f32", 0.000000000000000000000012466101),
    ]),
  [value("f32", 0.0000000000000000000000124661)],
);

// ./test/core/float_exprs.wast:1287
assert_return(
  () =>
    invoke($56, `f32.no_fold_recip_recip`, [
      value("f32", 0.000000000000000000097184545),
    ]),
  [value("f32", 0.00000000000000000009718455)],
);

// ./test/core/float_exprs.wast:1288
assert_return(
  () => invoke($56, `f32.no_fold_recip_recip`, [value("f32", -30.400759)]),
  [value("f32", -30.40076)],
);

// ./test/core/float_exprs.wast:1289
assert_return(
  () =>
    invoke($56, `f32.no_fold_recip_recip`, [
      value("f32", 2331659200000000000000),
    ]),
  [value("f32", 2331659000000000000000)],
);

// ./test/core/float_exprs.wast:1291
assert_return(
  () => invoke($56, `f32.no_fold_recip_recip`, [value("f32", -0)]),
  [value("f32", -0)],
);

// ./test/core/float_exprs.wast:1292
assert_return(() => invoke($56, `f32.no_fold_recip_recip`, [value("f32", 0)]), [
  value("f32", 0),
]);

// ./test/core/float_exprs.wast:1293
assert_return(
  () => invoke($56, `f32.no_fold_recip_recip`, [value("f32", -Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/float_exprs.wast:1294
assert_return(
  () => invoke($56, `f32.no_fold_recip_recip`, [value("f32", Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/float_exprs.wast:1296
assert_return(
  () =>
    invoke($56, `f64.no_fold_recip_recip`, [
      value("f64", -657971534362886860000000000000000000000000000),
    ]),
  [value("f64", -657971534362886900000000000000000000000000000)],
);

// ./test/core/float_exprs.wast:1297
assert_return(
  () =>
    invoke($56, `f64.no_fold_recip_recip`, [
      value("f64", -144246931868576430000),
    ]),
  [value("f64", -144246931868576420000)],
);

// ./test/core/float_exprs.wast:1298
assert_return(
  () =>
    invoke($56, `f64.no_fold_recip_recip`, [
      value("f64", 184994689206231350000000000000000000000000000000000),
    ]),
  [value("f64", 184994689206231330000000000000000000000000000000000)],
);

// ./test/core/float_exprs.wast:1299
assert_return(
  () =>
    invoke($56, `f64.no_fold_recip_recip`, [
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005779584288006583,
      ),
    ]),
  [value(
    "f64",
    0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005779584288006582,
  )],
);

// ./test/core/float_exprs.wast:1300
assert_return(
  () =>
    invoke($56, `f64.no_fold_recip_recip`, [
      value(
        "f64",
        51501178696141640000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    51501178696141634000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:1302
assert_return(
  () => invoke($56, `f64.no_fold_recip_recip`, [value("f64", -0)]),
  [value("f64", -0)],
);

// ./test/core/float_exprs.wast:1303
assert_return(() => invoke($56, `f64.no_fold_recip_recip`, [value("f64", 0)]), [
  value("f64", 0),
]);

// ./test/core/float_exprs.wast:1304
assert_return(
  () => invoke($56, `f64.no_fold_recip_recip`, [value("f64", -Infinity)]),
  [value("f64", -Infinity)],
);

// ./test/core/float_exprs.wast:1305
assert_return(
  () => invoke($56, `f64.no_fold_recip_recip`, [value("f64", Infinity)]),
  [value("f64", Infinity)],
);

// ./test/core/float_exprs.wast:1309
let $57 = instantiate(`(module
  (func (export "f32.no_algebraic_factoring") (param $$x f32) (param $$y f32) (result f32)
    (f32.mul (f32.add (local.get $$x) (local.get $$y))
             (f32.sub (local.get $$x) (local.get $$y))))

  (func (export "f64.no_algebraic_factoring") (param $$x f64) (param $$y f64) (result f64)
    (f64.mul (f64.add (local.get $$x) (local.get $$y))
             (f64.sub (local.get $$x) (local.get $$y))))
)`);

// ./test/core/float_exprs.wast:1319
assert_return(
  () =>
    invoke($57, `f32.no_algebraic_factoring`, [
      value("f32", -0.000000000000000053711865),
      value("f32", 0.00000000000000009744328),
    ]),
  [value("f32", -0.000000000000000000000000000000006610229)],
);

// ./test/core/float_exprs.wast:1320
assert_return(
  () =>
    invoke($57, `f32.no_algebraic_factoring`, [
      value("f32", -19756732),
      value("f32", 32770204),
    ]),
  [value("f32", -683557800000000)],
);

// ./test/core/float_exprs.wast:1321
assert_return(
  () =>
    invoke($57, `f32.no_algebraic_factoring`, [
      value("f32", 52314150000000),
      value("f32", -145309980000000),
    ]),
  [value("f32", -18378221000000000000000000000)],
);

// ./test/core/float_exprs.wast:1322
assert_return(
  () =>
    invoke($57, `f32.no_algebraic_factoring`, [
      value("f32", 195260.38),
      value("f32", -227.75723),
    ]),
  [value("f32", 38126563000)],
);

// ./test/core/float_exprs.wast:1323
assert_return(
  () =>
    invoke($57, `f32.no_algebraic_factoring`, [
      value("f32", -237.48706),
      value("f32", -972341.5),
    ]),
  [value("f32", -945447960000)],
);

// ./test/core/float_exprs.wast:1325
assert_return(
  () =>
    invoke($57, `f64.no_algebraic_factoring`, [
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000009639720335949767,
      ),
      value(
        "f64",
        0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000008019175443606207,
      ),
    ]),
  [value(
    "f64",
    -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000006430717386609473,
  )],
);

// ./test/core/float_exprs.wast:1326
assert_return(
  () =>
    invoke($57, `f64.no_algebraic_factoring`, [
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005166066590392027,
      ),
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001494333315888213,
      ),
    ]),
  [value(
    "f64",
    0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000026688244016362468,
  )],
);

// ./test/core/float_exprs.wast:1327
assert_return(
  () =>
    invoke($57, `f64.no_algebraic_factoring`, [
      value(
        "f64",
        -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002866135870517635,
      ),
      value(
        "f64",
        -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000012114355254268516,
      ),
    ]),
  [value(
    "f64",
    -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000014675678175314036,
  )],
);

// ./test/core/float_exprs.wast:1328
assert_return(
  () =>
    invoke($57, `f64.no_algebraic_factoring`, [
      value("f64", -1292099281007814900000000000000000000000000000000000000),
      value("f64", 662717187728034000000000000000000000000000000000000000000),
    ]),
  [value(
    "f64",
    -439192401389602300000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:1329
assert_return(
  () =>
    invoke($57, `f64.no_algebraic_factoring`, [
      value("f64", 26242795689010570000000000000000000),
      value("f64", -1625023398605080200000000000),
    ]),
  [value(
    "f64",
    688684325575149100000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:1333
let $58 = instantiate(`(module
  (func (export "f32.no_algebraic_factoring") (param $$x f32) (param $$y f32) (result f32)
    (f32.sub (f32.mul (local.get $$x) (local.get $$x))
             (f32.mul (local.get $$y) (local.get $$y))))

  (func (export "f64.no_algebraic_factoring") (param $$x f64) (param $$y f64) (result f64)
    (f64.sub (f64.mul (local.get $$x) (local.get $$x))
             (f64.mul (local.get $$y) (local.get $$y))))
)`);

// ./test/core/float_exprs.wast:1343
assert_return(
  () =>
    invoke($58, `f32.no_algebraic_factoring`, [
      value("f32", 0.000000000000022102996),
      value("f32", 0.0000000000031465275),
    ]),
  [value("f32", -0.0000000000000000000000099001476)],
);

// ./test/core/float_exprs.wast:1344
assert_return(
  () =>
    invoke($58, `f32.no_algebraic_factoring`, [
      value("f32", -3289460800000),
      value("f32", -15941539000),
    ]),
  [value("f32", 10820299000000000000000000)],
);

// ./test/core/float_exprs.wast:1345
assert_return(
  () =>
    invoke($58, `f32.no_algebraic_factoring`, [
      value("f32", 0.00036497542),
      value("f32", -0.00016153714),
    ]),
  [value("f32", 0.000000107112804)],
);

// ./test/core/float_exprs.wast:1346
assert_return(
  () =>
    invoke($58, `f32.no_algebraic_factoring`, [
      value("f32", 0.000000000000065383266),
      value("f32", -0.000000000000027412773),
    ]),
  [value("f32", 0.000000000000000000000000003523511)],
);

// ./test/core/float_exprs.wast:1347
assert_return(
  () =>
    invoke($58, `f32.no_algebraic_factoring`, [
      value("f32", 3609682000000000),
      value("f32", -5260104400000000),
    ]),
  [value("f32", -14638896000000000000000000000000)],
);

// ./test/core/float_exprs.wast:1349
assert_return(
  () =>
    invoke($58, `f64.no_algebraic_factoring`, [
      value(
        "f64",
        213640454349895100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        -292858755839442800000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    45642243734743850000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:1350
assert_return(
  () =>
    invoke($58, `f64.no_algebraic_factoring`, [
      value(
        "f64",
        -1229017115924435800000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        -8222158919016600000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    -67603897289562710000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:1351
assert_return(
  () =>
    invoke($58, `f64.no_algebraic_factoring`, [
      value("f64", 5477733829752.252),
      value("f64", -970738900948.5906),
    ]),
  [value("f64", 29063233895797397000000000)],
);

// ./test/core/float_exprs.wast:1352
assert_return(
  () =>
    invoke($58, `f64.no_algebraic_factoring`, [
      value("f64", -10689141744923551000000000000000000000000000000000000000),
      value("f64", -173378393593738040000000000000000000000000000000000),
    ]),
  [value(
    "f64",
    114257751213007240000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:1353
assert_return(
  () =>
    invoke($58, `f64.no_algebraic_factoring`, [
      value(
        "f64",
        -0.000000000000000000000000000000000000000000000000000000000000000000000010295699877022106,
      ),
      value(
        "f64",
        -0.000000000000000000000000000000000000000000000000000000000000000000000008952274637805908,
      ),
    ]),
  [value(
    "f64",
    0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000025858214767010105,
  )],
);

// ./test/core/float_exprs.wast:1358
let $59 = instantiate(`(module
  (memory (data
    "\\01\\00\\00\\00\\01\\00\\00\\80\\01\\00\\00\\00\\01\\00\\00\\80"
    "\\01\\00\\00\\00\\01\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00"
    "\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00"
  ))

  (func (export "f32.simple_x4_sum")
    (param $$i i32)
    (param $$j i32)
    (param $$k i32)
    (local $$x0 f32) (local $$x1 f32) (local $$x2 f32) (local $$x3 f32)
    (local $$y0 f32) (local $$y1 f32) (local $$y2 f32) (local $$y3 f32)
    (local.set $$x0 (f32.load offset=0 (local.get $$i)))
    (local.set $$x1 (f32.load offset=4 (local.get $$i)))
    (local.set $$x2 (f32.load offset=8 (local.get $$i)))
    (local.set $$x3 (f32.load offset=12 (local.get $$i)))
    (local.set $$y0 (f32.load offset=0 (local.get $$j)))
    (local.set $$y1 (f32.load offset=4 (local.get $$j)))
    (local.set $$y2 (f32.load offset=8 (local.get $$j)))
    (local.set $$y3 (f32.load offset=12 (local.get $$j)))
    (f32.store offset=0 (local.get $$k) (f32.add (local.get $$x0) (local.get $$y0)))
    (f32.store offset=4 (local.get $$k) (f32.add (local.get $$x1) (local.get $$y1)))
    (f32.store offset=8 (local.get $$k) (f32.add (local.get $$x2) (local.get $$y2)))
    (f32.store offset=12 (local.get $$k) (f32.add (local.get $$x3) (local.get $$y3)))
  )

  (func (export "f32.load")
    (param $$k i32) (result f32)
    (f32.load (local.get $$k))
  )
)`);

// ./test/core/float_exprs.wast:1391
assert_return(() => invoke($59, `f32.simple_x4_sum`, [0, 16, 32]), []);

// ./test/core/float_exprs.wast:1392
assert_return(() => invoke($59, `f32.load`, [32]), [
  value("f32", 0.000000000000000000000000000000000000000000003),
]);

// ./test/core/float_exprs.wast:1393
assert_return(() => invoke($59, `f32.load`, [36]), [value("f32", 0)]);

// ./test/core/float_exprs.wast:1394
assert_return(() => invoke($59, `f32.load`, [40]), [
  value("f32", 0.000000000000000000000000000000000000000000001),
]);

// ./test/core/float_exprs.wast:1395
assert_return(() => invoke($59, `f32.load`, [44]), [
  value("f32", -0.000000000000000000000000000000000000000000001),
]);

// ./test/core/float_exprs.wast:1397
let $60 = instantiate(`(module
  (memory (data
    "\\01\\00\\00\\00\\00\\00\\00\\00\\01\\00\\00\\00\\00\\00\\00\\80\\01\\00\\00\\00\\00\\00\\00\\00\\01\\00\\00\\00\\00\\00\\00\\80"
    "\\01\\00\\00\\00\\00\\00\\00\\00\\01\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00"
    "\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00\\00"
  ))

  (func (export "f64.simple_x4_sum")
    (param $$i i32)
    (param $$j i32)
    (param $$k i32)
    (local $$x0 f64) (local $$x1 f64) (local $$x2 f64) (local $$x3 f64)
    (local $$y0 f64) (local $$y1 f64) (local $$y2 f64) (local $$y3 f64)
    (local.set $$x0 (f64.load offset=0 (local.get $$i)))
    (local.set $$x1 (f64.load offset=8 (local.get $$i)))
    (local.set $$x2 (f64.load offset=16 (local.get $$i)))
    (local.set $$x3 (f64.load offset=24 (local.get $$i)))
    (local.set $$y0 (f64.load offset=0 (local.get $$j)))
    (local.set $$y1 (f64.load offset=8 (local.get $$j)))
    (local.set $$y2 (f64.load offset=16 (local.get $$j)))
    (local.set $$y3 (f64.load offset=24 (local.get $$j)))
    (f64.store offset=0 (local.get $$k) (f64.add (local.get $$x0) (local.get $$y0)))
    (f64.store offset=8 (local.get $$k) (f64.add (local.get $$x1) (local.get $$y1)))
    (f64.store offset=16 (local.get $$k) (f64.add (local.get $$x2) (local.get $$y2)))
    (f64.store offset=24 (local.get $$k) (f64.add (local.get $$x3) (local.get $$y3)))
  )

  (func (export "f64.load")
    (param $$k i32) (result f64)
    (f64.load (local.get $$k))
  )
)`);

// ./test/core/float_exprs.wast:1430
assert_return(() => invoke($60, `f64.simple_x4_sum`, [0, 32, 64]), []);

// ./test/core/float_exprs.wast:1431
assert_return(() => invoke($60, `f64.load`, [64]), [
  value(
    "f64",
    0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001,
  ),
]);

// ./test/core/float_exprs.wast:1432
assert_return(() => invoke($60, `f64.load`, [72]), [value("f64", 0)]);

// ./test/core/float_exprs.wast:1433
assert_return(() => invoke($60, `f64.load`, [80]), [
  value(
    "f64",
    0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005,
  ),
]);

// ./test/core/float_exprs.wast:1434
assert_return(() => invoke($60, `f64.load`, [88]), [
  value(
    "f64",
    -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005,
  ),
]);

// ./test/core/float_exprs.wast:1439
let $61 = instantiate(`(module
  (memory (data
    "\\c4\\c5\\57\\24\\a5\\84\\c8\\0b\\6d\\b8\\4b\\2e\\f2\\76\\17\\1c\\ca\\4a\\56\\1e\\1b\\6e\\71\\22"
    "\\5d\\17\\1e\\6e\\bf\\cd\\14\\5c\\c7\\21\\55\\51\\39\\9c\\1f\\b2\\51\\f0\\a3\\93\\d7\\c1\\2c\\ae"
    "\\7e\\a8\\28\\3a\\01\\21\\f4\\0a\\58\\93\\f8\\42\\77\\9f\\83\\39\\6a\\5f\\ba\\f7\\0a\\d8\\51\\6a"
    "\\34\\ca\\ad\\c6\\34\\0e\\d8\\26\\dc\\4c\\33\\1c\\ed\\29\\90\\a8\\78\\0f\\d1\\ce\\76\\31\\23\\83"
    "\\b8\\35\\e8\\f2\\44\\b0\\d3\\a1\\fc\\bb\\32\\e1\\b0\\ba\\69\\44\\09\\d6\\d9\\7d\\ff\\2e\\c0\\5a"
    "\\36\\14\\33\\14\\3e\\a9\\fa\\87\\6d\\8b\\bc\\ce\\9d\\a7\\fd\\c4\\e9\\85\\3f\\dd\\d7\\e1\\18\\a6"
    "\\50\\26\\72\\6e\\3f\\73\\0f\\f8\\12\\93\\23\\34\\61\\76\\12\\48\\c0\\9b\\05\\93\\eb\\ac\\86\\de"
    "\\94\\3e\\55\\e8\\8c\\e8\\dd\\e4\\fc\\95\\47\\be\\56\\03\\21\\20\\4c\\e6\\bf\\7b\\f6\\7f\\d5\\ba"
    "\\73\\1c\\c1\\14\\8f\\c4\\27\\96\\b3\\bd\\33\\ff\\78\\41\\5f\\c0\\5a\\ce\\f6\\67\\6e\\73\\9a\\17"
    "\\66\\70\\03\\f8\\ce\\27\\a3\\52\\b2\\9f\\3b\\bf\\fb\\ae\\ed\\d3\\5a\\f8\\37\\57\\f0\\f5\\6e\\ef"
    "\\b1\\4d\\70\\3d\\54\\a7\\01\\9a\\85\\08\\48\\91\\f5\\9d\\0c\\60\\87\\5b\\d9\\54\\1e\\51\\6d\\88"
    "\\8e\\08\\8c\\a5\\71\\3a\\56\\08\\67\\46\\8f\\8f\\13\\2a\\2c\\ec\\2c\\1f\\b4\\62\\2b\\6f\\41\\0a"
    "\\c4\\65\\42\\a2\\31\\6b\\2c\\7d\\3e\\bb\\75\\ac\\86\\97\\30\\d9\\48\\cd\\9a\\1f\\56\\c4\\c6\\e4"
    "\\12\\c0\\9d\\fb\\ee\\02\\8c\\ce\\1c\\f2\\1e\\a1\\78\\23\\db\\c4\\1e\\49\\03\\d3\\71\\cc\\08\\50"
    "\\c5\\d8\\5c\\ed\\d5\\b5\\65\\ac\\b5\\c9\\21\\d2\\c9\\29\\76\\de\\f0\\30\\1a\\5b\\3c\\f2\\3b\\db"
    "\\3a\\39\\82\\3a\\16\\08\\6f\\a8\\f1\\be\\69\\69\\99\\71\\a6\\05\\d3\\14\\93\\2a\\16\\f2\\2f\\11"
    "\\c7\\7e\\20\\bb\\91\\44\\ee\\f8\\e4\\01\\53\\c0\\b9\\7f\\f0\\bf\\f0\\03\\9c\\6d\\b1\\df\\a2\\44"
    "\\01\\6d\\6b\\71\\2b\\5c\\b3\\21\\19\\46\\5e\\8f\\db\\91\\d3\\7c\\78\\6b\\b7\\12\\00\\8f\\eb\\bd"
    "\\8a\\f5\\d4\\2e\\c4\\c1\\1e\\df\\73\\63\\59\\47\\49\\03\\0a\\b7\\cf\\24\\cf\\9c\\0e\\44\\7a\\9e"
    "\\14\\fb\\42\\bf\\9d\\39\\30\\9e\\a0\\ab\\2f\\d1\\ae\\9e\\6a\\83\\43\\e3\\55\\7d\\85\\bf\\63\\8a"
    "\\f8\\96\\10\\1f\\fe\\6d\\e7\\22\\1b\\e1\\69\\46\\8a\\44\\c8\\c8\\f9\\0c\\2b\\19\\07\\a5\\02\\3e"
    "\\f2\\30\\10\\9a\\85\\8a\\5f\\ef\\81\\45\\a0\\77\\b1\\03\\10\\73\\4b\\ae\\98\\9d\\47\\bf\\9a\\2d"
    "\\3a\\d5\\0f\\03\\66\\e3\\3d\\53\\d9\\40\\ce\\1f\\6f\\32\\2f\\21\\2b\\23\\21\\6c\\62\\d4\\a7\\3e"
    "\\a8\\ce\\28\\31\\2d\\00\\3d\\67\\5e\\af\\a0\\cf\\2e\\d2\\b9\\6b\\84\\eb\\69\\08\\3c\\62\\36\\be"
    "\\12\\fd\\36\\7f\\88\\3e\\ad\\bc\\0b\\c0\\41\\c4\\50\\b6\\e3\\50\\31\\e8\\ce\\e2\\96\\65\\55\\9c"
    "\\16\\46\\e6\\b0\\2d\\3a\\e8\\81\\05\\b0\\bf\\34\\f7\\bc\\10\\1c\\fb\\cc\\3c\\f1\\85\\97\\42\\9f"
    "\\eb\\14\\8d\\3c\\bf\\d7\\17\\88\\49\\9d\\8b\\2b\\b2\\3a\\83\\d1\\4f\\04\\9e\\a1\\0f\\ad\\08\\9d"
    "\\54\\af\\d1\\82\\c3\\ec\\32\\2f\\02\\8f\\05\\21\\2d\\a2\\b7\\e4\\f4\\6f\\2e\\81\\2b\\0b\\9c\\fc"
    "\\cb\\fe\\74\\02\\f9\\db\\f4\\f3\\ea\\00\\a8\\ec\\d1\\99\\74\\26\\dd\\d6\\34\\d5\\25\\b1\\46\\dd"
    "\\9c\\aa\\71\\f5\\60\\b0\\88\\c8\\e0\\0b\\59\\5a\\25\\4f\\29\\66\\f9\\e3\\2e\\fe\\e9\\da\\e5\\18"
    "\\4f\\27\\62\\f4\\ce\\a4\\21\\95\\74\\c7\\57\\64\\27\\9a\\4c\\fd\\54\\7d\\61\\ce\\c3\\ac\\87\\46"
    "\\9c\\fa\\ff\\09\\ca\\79\\97\\67\\24\\74\\ca\\d4\\21\\83\\26\\25\\19\\12\\37\\64\\19\\e5\\65\\e0"
    "\\74\\75\\8e\\dd\\c8\\ef\\74\\c7\\d8\\21\\2b\\79\\04\\51\\46\\65\\60\\03\\5d\\fa\\d8\\f4\\65\\a4"
    "\\9e\\5d\\23\\da\\d7\\8a\\92\\80\\a4\\de\\78\\3c\\f1\\57\\42\\6d\\cd\\c9\\2f\\d5\\a4\\9e\\ab\\40"
    "\\f4\\cb\\1b\\d7\\a3\\ca\\fc\\eb\\a7\\01\\b2\\9a\\69\\4e\\46\\9b\\18\\4e\\dd\\79\\a7\\aa\\a6\\52"
    "\\39\\1e\\ef\\30\\cc\\9b\\bd\\5b\\ee\\4c\\21\\6d\\30\\00\\72\\b0\\46\\5f\\08\\cf\\c5\\b9\\e0\\3e"
    "\\c2\\b3\\0c\\dc\\8e\\64\\de\\19\\42\\79\\cf\\43\\ea\\43\\5d\\8e\\88\\f7\\ab\\15\\dc\\3f\\c8\\67"
    "\\20\\db\\b8\\64\\b1\\47\\1f\\de\\f2\\cb\\3f\\59\\9f\\d8\\46\\90\\dc\\ae\\2f\\22\\f9\\e2\\31\\89"
    "\\d9\\9c\\1c\\4c\\d3\\a9\\4a\\57\\84\\9c\\9f\\ea\\2c\\3c\\ae\\3c\\c3\\1e\\8b\\e5\\4e\\17\\01\\25"
    "\\db\\34\\46\\5f\\15\\ea\\05\\0c\\7c\\d9\\45\\8c\\19\\d0\\73\\8a\\96\\16\\dd\\44\\f9\\05\\b7\\5b"
    "\\71\\b0\\e6\\21\\36\\5f\\75\\89\\91\\73\\75\\ab\\7d\\ae\\d3\\73\\ec\\37\\c6\\ea\\55\\75\\ef\\ea"
    "\\ab\\8b\\7b\\11\\dc\\6d\\1a\\b2\\6a\\c4\\25\\cf\\aa\\e3\\9f\\49\\49\\89\\cb\\37\\9b\\0a\\a7\\01"
    "\\60\\70\\dc\\b7\\c8\\83\\e1\\42\\f5\\be\\ad\\62\\94\\ad\\8d\\a1"
  ))

  (func (export "f32.kahan_sum") (param $$p i32) (param $$n i32) (result f32)
    (local $$sum f32)
    (local $$c f32)
    (local $$t f32)
    (block $$exit
      (loop $$top
        (local.set $$t
          (f32.sub
            (f32.sub
              (local.tee $$sum
                (f32.add
                  (local.get $$c)
                  (local.tee $$t
                    (f32.sub (f32.load (local.get $$p)) (local.get $$t))
                  )
                )
              )
              (local.get $$c)
            )
            (local.get $$t)
          )
        )
        (local.set $$p (i32.add (local.get $$p) (i32.const 4)))
        (local.set $$c (local.get $$sum))
        (br_if $$top (local.tee $$n (i32.add (local.get $$n) (i32.const -1))))
      )
    )
    (local.get $$sum)
  )

  (func (export "f32.plain_sum") (param $$p i32) (param $$n i32) (result f32)
    (local $$sum f32)
    (block $$exit
      (loop $$top
        (local.set $$sum (f32.add (local.get $$sum) (f32.load (local.get $$p))))
        (local.set $$p (i32.add (local.get $$p) (i32.const 4)))
        (local.set $$n (i32.add (local.get $$n) (i32.const -1)))
        (br_if $$top (local.get $$n))
      )
    )
    (local.get $$sum)
  )
)`);

// ./test/core/float_exprs.wast:1530
assert_return(() => invoke($61, `f32.kahan_sum`, [0, 256]), [
  value("f32", -21558138000000000000000000000000),
]);

// ./test/core/float_exprs.wast:1531
assert_return(() => invoke($61, `f32.plain_sum`, [0, 256]), [
  value("f32", -16487540000000000000000000000000),
]);

// ./test/core/float_exprs.wast:1533
let $62 = instantiate(`(module
  (memory (data "\\13\\05\\84\\42\\5d\\a2\\2c\\c6\\43\\db\\55\\a9\\cd\\da\\55\\e3\\73\\fc\\58\\d6\\ba\\d5\\00\\fd\\83\\35\\42\\88\\8b\\13\\5d\\38\\4a\\47\\0d\\72\\73\\a1\\1a\\ef\\c4\\45\\17\\57\\d8\\c9\\46\\e0\\8d\\6c\\e1\\37\\70\\c8\\83\\5b\\55\\5e\\5a\\2d\\73\\1e\\56\\c8\\e1\\6d\\69\\14\\78\\0a\\8a\\5a\\64\\3a\\09\\c7\\a8\\87\\c5\\f0\\d3\\5d\\e6\\03\\fc\\93\\be\\26\\ca\\d6\\a9\\91\\60\\bd\\b0\\ed\\ae\\f7\\30\\7e\\92\\3a\\6f\\a7\\59\\8e\\aa\\7d\\bf\\67\\58\\2a\\54\\f8\\4e\\fe\\ed\\35\\58\\a6\\51\\bf\\42\\e5\\4b\\66\\27\\24\\6d\\7f\\42\\2d\\28\\92\\18\\ec\\08\\ae\\e7\\55\\da\\b1\\a6\\65\\a5\\72\\50\\47\\1b\\b8\\a9\\54\\d7\\a6\\06\\5b\\0f\\42\\58\\83\\8a\\17\\82\\c6\\10\\43\\a0\\c0\\2e\\6d\\bc\\5a\\85\\53\\72\\7f\\ad\\44\\bc\\30\\3c\\55\\b2\\24\\9a\\74\\3a\\9e\\e1\\d8\\0f\\70\\fc\\a9\\3a\\cd\\93\\4b\\ec\\e3\\7e\\dd\\5d\\27\\cd\\f8\\a0\\9d\\1c\\11\\c0\\57\\2e\\fd\\c8\\13\\32\\cc\\3a\\1a\\7d\\a3\\41\\55\\ed\\c3\\82\\49\\2a\\04\\1e\\ef\\73\\b9\\2e\\2e\\e3\\5f\\f4\\df\\e6\\b2\\33\\0c\\39\\3f\\6f\\44\\6a\\03\\c1\\42\\b9\\fa\\b1\\c8\\ed\\a5\\58\\99\\7f\\ed\\b4\\72\\9e\\79\\eb\\fb\\43\\82\\45\\aa\\bb\\95\\d2\\ff\\28\\9e\\f6\\a1\\ad\\95\\d6\\55\\95\\0d\\6f\\60\\11\\c7\\78\\3e\\49\\f2\\7e\\48\\f4\\a2\\71\\d0\\13\\8e\\b3\\de\\99\\52\\e3\\45\\74\\ea\\76\\0e\\1b\\2a\\c8\\ee\\14\\01\\c4\\50\\5b\\36\\3c\\ef\\ba\\72\\a2\\a6\\08\\f8\\7b\\36\\9d\\f9\\ef\\0b\\c7\\56\\2d\\5c\\f0\\9d\\5d\\de\\fc\\b8\\ad\\0f\\64\\0e\\97\\15\\32\\26\\c2\\31\\e6\\05\\1e\\ef\\cb\\17\\1b\\6d\\15\\0b\\74\\5d\\d3\\2e\\f8\\6b\\86\\b4\\ba\\73\\52\\53\\99\\a9\\76\\20\\45\\c9\\40\\80\\6b\\14\\ed\\a1\\fa\\80\\46\\e6\\26\\d2\\e6\\98\\c4\\57\\bf\\c4\\1c\\a4\\90\\7a\\36\\94\\14\\ba\\15\\89\\6e\\e6\\9c\\37\\8c\\f4\\de\\12\\22\\5d\\a1\\79\\50\\67\\0d\\3d\\7a\\e9\\d4\\aa\\2e\\7f\\2a\\7a\\30\\3d\\ea\\5d\\12\\48\\fe\\e1\\18\\cd\\a4\\57\\a2\\87\\3e\\b6\\9a\\8b\\db\\da\\9d\\78\\9c\\cf\\8d\\b1\\4f\\90\\b4\\34\\e0\\9d\\f6\\ca\\fe\\4c\\3b\\78\\6d\\0a\\5c\\18\\9f\\61\\b9\\dd\\b4\\e0\\0f\\76\\e0\\1b\\69\\0d\\5e\\58\\73\\70\\5e\\0e\\2d\\a1\\7d\\ff\\20\\eb\\91\\34\\92\\ac\\38\\72\\2a\\1f\\8e\\71\\2e\\6a\\f1\\af\\c7\\27\\70\\d9\\c4\\57\\f7\\d2\\3c\\1d\\b8\\f0\\f0\\64\\cf\\dc\\ae\\be\\a3\\cc\\3e\\22\\7d\\4e\\69\\21\\63\\17\\ed\\03\\02\\54\\9a\\0f\\50\\4e\\13\\5a\\35\\a1\\22\\a4\\df\\86\\c2\\74\\79\\16\\b8\\69\\69\\a0\\52\\5d\\11\\64\\bd\\5b\\93\\fc\\69\\a0\\f4\\13\\d0\\81\\51\\dd\\fa\\0c\\15\\c3\\7a\\c9\\62\\7a\\a9\\1d\\c9\\e6\\5a\\b3\\5b\\97\\02\\3c\\64\\22\\12\\3c\\22\\90\\64\\2d\\30\\54\\4c\\b4\\a1\\22\\09\\57\\22\\5e\\8e\\38\\2b\\02\\a8\\ae\\f6\\be\\0d\\2b\\f2\\03\\ad\\fa\\10\\01\\71\\77\\2a\\30\\02\\95\\f6\\00\\3e\\d0\\c4\\8d\\34\\19\\50\\21\\0a\\bc\\50\\da\\3c\\30\\d6\\3a\\31\\94\\8d\\3a\\fe\\ef\\14\\57\\9d\\4b\\93\\00\\96\\24\\0c\\6f\\fd\\bc\\23\\76\\02\\6c\\eb\\52\\72\\80\\11\\7e\\80\\3a\\13\\12\\38\\1d\\38\\49\\95\\40\\27\\8a\\44\\7b\\e8\\dc\\6d\\8c\\8c\\8e\\3c\\b5\\b3\\18\\0e\\f6\\08\\1a\\84\\41\\35\\ff\\8b\\b8\\93\\40\\ea\\e1\\51\\1d\\89\\a5\\8d\\42\\68\\29\\ea\\2f\\c1\\7a\\52\\eb\\90\\5d\\4d\\d6\\80\\e3\\d7\\75\\48\\ce\\ed\\d3\\01\\1c\\8d\\5b\\a5\\94\\0d\\78\\cf\\f1\\06\\13\\2f\\98\\02\\a4\\6d\\2e\\6c\\f2\\d5\\74\\29\\89\\4c\\f9\\03\\f5\\c7\\18\\ad\\7a\\f0\\68\\f8\\5c\\d6\\59\\87\\6e\\d6\\3f\\06\\be\\86\\20\\e3\\41\\91\\22\\f3\\6e\\8b\\f0\\68\\1c\\57\\a7\\fc\\b0\\7c\\9e\\99\\0b\\96\\1a\\89\\5f\\e6\\0d\\7c\\08\\51\\a0\\a2\\67\\9a\\47\\00\\93\\6b\\f9\\28\\f0\\68\\db\\62\\f1\\e0\\65\\2c\\53\\33\\e0\\a7\\ca\\11\\42\\30\\f6\\af\\01\\c1\\65\\3d\\32\\01\\6f\\ab\\2e\\be\\d3\\8b\\be\\14\\c3\\ff\\ec\\fb\\f0\\f9\\c5\\0c\\05\\6f\\01\\09\\6b\\e3\\34\\31\\0c\\1f\\66\\a6\\42\\bc\\1a\\87\\49\\16\\16\\8c\\b0\\90\\0d\\34\\8c\\0a\\e1\\09\\5e\\10\\a4\\6b\\56\\cc\\f0\\c9\\bb\\dc\\b8\\5c\\ce\\f6\\cc\\8d\\75\\7e\\b3\\07\\88\\04\\2f\\b4\\5e\\c9\\e3\\4a\\23\\73\\19\\62\\6c\\9a\\03\\76\\44\\86\\9c\\60\\fc\\db\\72\\8f\\27\\a0\\dd\\b3\\c5\\da\\ff\\f9\\ec\\6a\\b1\\7b\\d3\\cf\\50\\37\\c9\\7a\\78\\0c\\e4\\3a\\b6\\f5\\e6\\f4\\98\\6e\\42\\7d\\35\\73\\8b\\45\\c0\\56\\97\\cd\\6d\\ce\\cf\\ad\\31\\b3\\c3\\54\\fa\\ef\\d5\\c0\\f4\\6a\\5f\\54\\e7\\49\\3e\\33\\0a\\30\\38\\fd\\d9\\05\\ff\\a5\\3f\\57\\46\\14\\b5\\91\\17\\ca\\6b\\98\\23\\7a\\65\\b3\\6c\\02\\b4\\cc\\79\\5d\\58\\d8\\b3\\d5\\94\\ae\\f4\\6d\\75\\65\\f7\\92\\bf\\7e\\47\\4c\\3c\\ee\\db\\ac\\f1\\32\\5d\\fb\\6f\\41\\1c\\34\\c8\\83\\4f\\c2\\58\\01\\be\\05\\3e\\66\\16\\a6\\04\\6d\\5d\\4f\\86\\09\\27\\82\\25\\12\\cd\\3a\\cd\\ce\\6b\\bc\\ca\\ac\\28\\9b\\ee\\6a\\25\\86\\9e\\45\\70\\c6\\d2\\bd\\3b\\7d\\42\\e5\\27\\af\\c7\\1d\\f4\\81\\c8\\b3\\76\\8a\\a8\\36\\a3\\ae\\2a\\e6\\18\\e1\\36\\22\\ad\\f6\\25\\72\\b0\\39\\8b\\01\\9a\\22\\7b\\84\\c3\\2d\\5f\\72\\a4\\98\\ac\\15\\70\\e7\\d4\\18\\e2\\7d\\d2\\30\\7c\\33\\08\\cd\\ca\\c4\\22\\85\\88\\75\\81\\c6\\4a\\74\\58\\8d\\e0\\e8\\ac\\c5\\ab\\75\\5a\\f4\\28\\12\\f0\\18\\45\\52\\f2\\97\\b2\\93\\41\\6f\\8d\\7f\\db\\70\\fb\\a3\\5d\\1f\\a7\\8d\\98\\20\\2b\\22\\9f\\3a\\01\\b5\\8b\\1b\\d2\\cb\\14\\03\\0e\\14\\14\\d2\\19\\5a\\1f\\ce\\5e\\cd\\81\\79\\15\\01\\ca\\de\\73\\74\\8c\\56\\20\\9f\\77\\2d\\25\\16\\f6\\61\\51\\1d\\a4\\8e\\9b\\98\\a5\\c6\\ec\\a8\\45\\57\\82\\59\\78\\0d\\90\\b4\\df\\51\\b0\\c3\\82\\94\\cc\\b3\\53\\09\\15\\6d\\96\\6c\\3a\\40\\47\\b7\\4a\\7a\\05\\2f\\a1\\1e\\8c\\9d\\a0\\20\\88\\fb\\52\\b7\\9f\\f3\\f3\\bb\\5f\\e7\\8a\\61\\a7\\21\\b1\\ac\\fa\\09\\aa\\a4\\6c\\bc\\24\\80\\ba\\2a\\e9\\65\\ff\\70\\ff\\cc\\fa\\65\\87\\76\\f3\\c5\\15\\ce\\cb\\e8\\42\\31\\00\\0c\\91\\57\\d9\\e0\\9d\\35\\54\\24\\ad\\a4\\d8\\f9\\08\\67\\63\\c8\\cf\\81\\dd\\90\\a2\\d7\\c4\\07\\4a\\e6\\10\\6f\\67\\e7\\27\\d4\\23\\59\\18\\f2\\a8\\9d\\5f\\d8\\94\\30\\aa\\54\\86\\4f\\87\\9d\\82\\b5\\26\\ca\\a6\\96\\bf\\cf\\55\\f9\\9d\\37\\01\\19\\48\\43\\c5\\94\\6c\\f3\\74\\97\\58\\4c\\3c\\9d\\08\\e8\\04\\c2\\58\\30\\76\\e1\\a0\\f8\\ea\\e9\\c5\\ae\\cf\\78\\9e\\a9\\0c\\ac\\b3\\44\\42\\e0\\bc\\5d\\1b\\9c\\49\\58\\4a\\1c\\19\\49\\c1\\3a\\ea\\f5\\eb\\3b\\81\\a9\\4b\\70\\0c\\cc\\9e\\1a\\d3\\2f\\b7\\52\\2f\\20\\3b\\eb\\64\\51\\1d\\a0\\2d\\b2\\3e\\be\\13\\85\\48\\92\\32\\2e\\db\\5c\\a1\\e7\\8c\\45\\91\\35\\01\\0a\\93\\c2\\eb\\09\\ce\\f3\\d2\\22\\24\\d0\\8c\\cc\\1d\\9d\\38\\c8\\4d\\e3\\82\\cc\\64\\15\\06\\2d\\e7\\01\\2f\\ab\\bb\\b5\\04\\4c\\92\\1c\\7a\\d6\\3f\\e8\\5f\\31\\15\\0c\\dc\\e4\\31\\b4\\c4\\25\\3e\\2a\\aa\\00\\9e\\c8\\e5\\21\\7a\\7f\\29\\f1\\c0\\af\\1d\\5e\\e8\\63\\39\\ad\\f8\\7e\\6c\\c8\\c5\\7f\\c2\\a8\\97\\27\\0a\\d9\\f4\\21\\6a\\ea\\03\\09\\fb\\f7\\96\\3b\\83\\79\\5f\\7c\\4b\\30\\9f\\56\\35\\de\\b4\\73\\d4\\95\\f0\\14\\c3\\74\\2f\\0d\\a3\\1d\\4e\\8d\\31\\24\\b3\\1a\\84\\85\\62\\5a\\7b\\3c\\14\\39\\17\\e6\\6d\\eb\\37\\c2\\00\\58\\5b\\0b\\e3\\3c\\8a\\62\\e1\\f8\\35\\4b\\56\\e2\\87\\60\\8b\\be\\a7\\38\\91\\77\\54\\a9\\5a\\24\\25\\90\\9f\\a5\\42\\77\\f3\\5c\\39\\df\\ff\\74\\07\\76\\a1\\cd\\1f\\62\\0b\\81\\81\\68\\af\\05\\c1\\c0\\7f\\26\\ee\\c0\\91\\a3\\6a\\7d\\29\\61\\45\\27\\e5\\57\\88\\dc\\0d\\97\\04\\1a\\33\\a9\\44\\8a\\da\\02\\10\\45\\3f\\8e\\55\\a6\\76\\8c\\4d\\e3\\f1\\89\\83\\c8\\d0\\f8\\9b\\50\\77\\9f\\47\\df\\4c\\9c\\66\\0d\\aa\\18\\b8\\5f\\4f\\c4\\01\\ce\\dc\\84\\ac\\46\\9e\\69\\e1\\76\\45\\6b\\61\\89\\e4\\5d\\94\\bb\\11\\83\\9f\\78\\d8\\0a\\d2\\f5\\7e\\5d\\43\\ea\\bc\\10\\f1\\3a\\c9\\e2\\64\\fb\\53\\65\\d0\\c7\\b4\\a7\\fb\\d4\\05\\53\\25\\d0\\cd\\29\\88\\00\\56\\25\\24\\7d\\5d\\b4\\f3\\41\\9f\\e9\\b5\\f7\\ae\\64\\2c\\e3\\c9\\6d\\d5\\84\\3a\\72\\12\\b8\\7a\\d9\\1b\\09\\e8\\38\\da\\26\\4f\\04\\ce\\03\\71\\6e\\8a\\44\\7b\\5c\\81\\59\\9c\\d2\\e4\\c3\\ba\\59\\a6\\e5\\28\\a7\\8f\\9a\\e4\\d5\\4e\\b9\\ca\\7f\\cb\\75\\b8\\2b\\43\\3e\\b3\\15\\46\\b1\\a5\\bc\\9d\\9e\\38\\15\\f1\\bd\\1b\\21\\aa\\f1\\82\\00\\95\\fc\\a7\\77\\47\\39\\a7\\33\\43\\92\\d7\\52\\40\\4b\\06\\81\\8a\\a0\\bd\\f1\\6b\\99\\84\\42\\5b\\e2\\3b\\c5\\5e\\12\\5c\\28\\4d\\b6\\0e\\4e\\c8\\5c\\e8\\01\\8a\\c5\\e7\\e4\\9d\\42\\ee\\5d\\9c\\c4\\eb\\eb\\68\\09\\27\\92\\95\\9a\\11\\54\\73\\c4\\12\\80\\fb\\7d\\fe\\c5\\08\\60\\7f\\36\\41\\e0\\10\\ba\\d6\\2b\\6c\\f1\\b4\\17\\fe\\26\\34\\e3\\4b\\f8\\a8\\e3\\91\\be\\4f\\2a\\fc\\da\\81\\b8\\e7\\fe\\d5\\26\\50\\47\\f3\\1a\\65\\32\\81\\e0\\05\\b8\\4f\\32\\31\\26\\00\\4a\\53\\97\\c2\\c3\\0e\\2e\\a1\\26\\54\\ab\\05\\8e\\56\\2f\\7d\\af\\22\\84\\68\\a5\\8b\\97\\f6\\a4\\fd\\a8\\cc\\75\\41\\96\\86\\fd\\27\\3d\\29\\86\\8d\\7f\\4c\\d4\\8e\\73\\41\\f4\\1e\\e2\\dd\\58\\27\\97\\ce\\9c\\94\\cf\\7a\\04\\2f\\dc\\ed"
  ))

  (func (export "f64.kahan_sum") (param $$p i32) (param $$n i32) (result f64)
    (local $$sum f64)
    (local $$c f64)
    (local $$t f64)
    (block $$exit
      (loop $$top
        (local.set $$t
          (f64.sub
            (f64.sub
              (local.tee $$sum
                (f64.add
                  (local.get $$c)
                  (local.tee $$t
                    (f64.sub (f64.load (local.get $$p)) (local.get $$t))
                  )
                )
              )
              (local.get $$c)
            )
            (local.get $$t)
          )
        )
        (local.set $$p (i32.add (local.get $$p) (i32.const 8)))
        (local.set $$c (local.get $$sum))
        (br_if $$top (local.tee $$n (i32.add (local.get $$n) (i32.const -1))))
      )
    )
    (local.get $$sum)
  )

  (func (export "f64.plain_sum") (param $$p i32) (param $$n i32) (result f64)
    (local $$sum f64)
    (block $$exit
      (loop $$top
        (local.set $$sum (f64.add (local.get $$sum) (f64.load (local.get $$p))))
        (local.set $$p (i32.add (local.get $$p) (i32.const 8)))
        (local.set $$n (i32.add (local.get $$n) (i32.const -1)))
        (br_if $$top (local.get $$n))
      )
    )
    (local.get $$sum)
  )
)`);

// ./test/core/float_exprs.wast:1581
assert_return(() => invoke($62, `f64.kahan_sum`, [0, 256]), [
  value(
    "f64",
    4996401743142033000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  ),
]);

// ./test/core/float_exprs.wast:1582
assert_return(() => invoke($62, `f64.plain_sum`, [0, 256]), [
  value(
    "f64",
    4996401743297957600000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  ),
]);

// ./test/core/float_exprs.wast:1586
let $63 = instantiate(`(module
  (func (export "f32.no_fold_neg_sub") (param $$x f32) (param $$y f32) (result f32)
    (f32.neg (f32.sub (local.get $$x) (local.get $$y))))

  (func (export "f64.no_fold_neg_sub") (param $$x f64) (param $$y f64) (result f64)
    (f64.neg (f64.sub (local.get $$x) (local.get $$y))))
)`);

// ./test/core/float_exprs.wast:1594
assert_return(
  () =>
    invoke($63, `f32.no_fold_neg_sub`, [value("f32", -0), value("f32", -0)]),
  [value("f32", -0)],
);

// ./test/core/float_exprs.wast:1595
assert_return(
  () => invoke($63, `f32.no_fold_neg_sub`, [value("f32", 0), value("f32", -0)]),
  [value("f32", -0)],
);

// ./test/core/float_exprs.wast:1596
assert_return(
  () => invoke($63, `f32.no_fold_neg_sub`, [value("f32", -0), value("f32", 0)]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:1597
assert_return(
  () => invoke($63, `f32.no_fold_neg_sub`, [value("f32", 0), value("f32", 0)]),
  [value("f32", -0)],
);

// ./test/core/float_exprs.wast:1599
assert_return(
  () =>
    invoke($63, `f64.no_fold_neg_sub`, [value("f64", -0), value("f64", -0)]),
  [value("f64", -0)],
);

// ./test/core/float_exprs.wast:1600
assert_return(
  () => invoke($63, `f64.no_fold_neg_sub`, [value("f64", 0), value("f64", -0)]),
  [value("f64", -0)],
);

// ./test/core/float_exprs.wast:1601
assert_return(
  () => invoke($63, `f64.no_fold_neg_sub`, [value("f64", -0), value("f64", 0)]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:1602
assert_return(
  () => invoke($63, `f64.no_fold_neg_sub`, [value("f64", 0), value("f64", 0)]),
  [value("f64", -0)],
);

// ./test/core/float_exprs.wast:1606
let $64 = instantiate(`(module
  (func (export "f32.no_fold_neg_add") (param $$x f32) (param $$y f32) (result f32)
    (f32.neg (f32.add (local.get $$x) (local.get $$y))))

  (func (export "f64.no_fold_neg_add") (param $$x f64) (param $$y f64) (result f64)
    (f64.neg (f64.add (local.get $$x) (local.get $$y))))
)`);

// ./test/core/float_exprs.wast:1614
assert_return(
  () =>
    invoke($64, `f32.no_fold_neg_add`, [value("f32", -0), value("f32", -0)]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:1615
assert_return(
  () => invoke($64, `f32.no_fold_neg_add`, [value("f32", 0), value("f32", -0)]),
  [value("f32", -0)],
);

// ./test/core/float_exprs.wast:1616
assert_return(
  () => invoke($64, `f32.no_fold_neg_add`, [value("f32", -0), value("f32", 0)]),
  [value("f32", -0)],
);

// ./test/core/float_exprs.wast:1617
assert_return(
  () => invoke($64, `f32.no_fold_neg_add`, [value("f32", 0), value("f32", 0)]),
  [value("f32", -0)],
);

// ./test/core/float_exprs.wast:1619
assert_return(
  () =>
    invoke($64, `f64.no_fold_neg_add`, [value("f64", -0), value("f64", -0)]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:1620
assert_return(
  () => invoke($64, `f64.no_fold_neg_add`, [value("f64", 0), value("f64", -0)]),
  [value("f64", -0)],
);

// ./test/core/float_exprs.wast:1621
assert_return(
  () => invoke($64, `f64.no_fold_neg_add`, [value("f64", -0), value("f64", 0)]),
  [value("f64", -0)],
);

// ./test/core/float_exprs.wast:1622
assert_return(
  () => invoke($64, `f64.no_fold_neg_add`, [value("f64", 0), value("f64", 0)]),
  [value("f64", -0)],
);

// ./test/core/float_exprs.wast:1626
let $65 = instantiate(`(module
  (func (export "f32.no_fold_add_neg_neg") (param $$x f32) (param $$y f32) (result f32)
    (f32.add (f32.neg (local.get $$x)) (f32.neg (local.get $$y))))

  (func (export "f64.no_fold_add_neg_neg") (param $$x f64) (param $$y f64) (result f64)
    (f64.add (f64.neg (local.get $$x)) (f64.neg (local.get $$y))))
)`);

// ./test/core/float_exprs.wast:1634
assert_return(
  () =>
    invoke($65, `f32.no_fold_add_neg_neg`, [
      value("f32", -0),
      value("f32", -0),
    ]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:1635
assert_return(
  () =>
    invoke($65, `f32.no_fold_add_neg_neg`, [value("f32", 0), value("f32", -0)]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:1636
assert_return(
  () =>
    invoke($65, `f32.no_fold_add_neg_neg`, [value("f32", -0), value("f32", 0)]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:1637
assert_return(
  () =>
    invoke($65, `f32.no_fold_add_neg_neg`, [value("f32", 0), value("f32", 0)]),
  [value("f32", -0)],
);

// ./test/core/float_exprs.wast:1639
assert_return(
  () =>
    invoke($65, `f64.no_fold_add_neg_neg`, [
      value("f64", -0),
      value("f64", -0),
    ]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:1640
assert_return(
  () =>
    invoke($65, `f64.no_fold_add_neg_neg`, [value("f64", 0), value("f64", -0)]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:1641
assert_return(
  () =>
    invoke($65, `f64.no_fold_add_neg_neg`, [value("f64", -0), value("f64", 0)]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:1642
assert_return(
  () =>
    invoke($65, `f64.no_fold_add_neg_neg`, [value("f64", 0), value("f64", 0)]),
  [value("f64", -0)],
);

// ./test/core/float_exprs.wast:1646
let $66 = instantiate(`(module
  (func (export "f32.no_fold_add_neg") (param $$x f32) (result f32)
    (f32.add (f32.neg (local.get $$x)) (local.get $$x)))

  (func (export "f64.no_fold_add_neg") (param $$x f64) (result f64)
    (f64.add (f64.neg (local.get $$x)) (local.get $$x)))
)`);

// ./test/core/float_exprs.wast:1654
assert_return(() => invoke($66, `f32.no_fold_add_neg`, [value("f32", 0)]), [
  value("f32", 0),
]);

// ./test/core/float_exprs.wast:1655
assert_return(() => invoke($66, `f32.no_fold_add_neg`, [value("f32", -0)]), [
  value("f32", 0),
]);

// ./test/core/float_exprs.wast:1656
assert_return(
  () => invoke($66, `f32.no_fold_add_neg`, [value("f32", Infinity)]),
  [`canonical_nan`],
);

// ./test/core/float_exprs.wast:1657
assert_return(
  () => invoke($66, `f32.no_fold_add_neg`, [value("f32", -Infinity)]),
  [`canonical_nan`],
);

// ./test/core/float_exprs.wast:1659
assert_return(() => invoke($66, `f64.no_fold_add_neg`, [value("f64", 0)]), [
  value("f64", 0),
]);

// ./test/core/float_exprs.wast:1660
assert_return(() => invoke($66, `f64.no_fold_add_neg`, [value("f64", -0)]), [
  value("f64", 0),
]);

// ./test/core/float_exprs.wast:1661
assert_return(
  () => invoke($66, `f64.no_fold_add_neg`, [value("f64", Infinity)]),
  [`canonical_nan`],
);

// ./test/core/float_exprs.wast:1662
assert_return(
  () => invoke($66, `f64.no_fold_add_neg`, [value("f64", -Infinity)]),
  [`canonical_nan`],
);

// ./test/core/float_exprs.wast:1666
let $67 = instantiate(`(module
  (func (export "f32.no_fold_6x_via_add") (param $$x f32) (result f32)
    (f32.add (f32.add (f32.add (f32.add (f32.add
    (local.get $$x)
    (local.get $$x)) (local.get $$x)) (local.get $$x))
    (local.get $$x)) (local.get $$x)))

  (func (export "f64.no_fold_6x_via_add") (param $$x f64) (result f64)
    (f64.add (f64.add (f64.add (f64.add (f64.add
    (local.get $$x)
    (local.get $$x)) (local.get $$x)) (local.get $$x))
    (local.get $$x)) (local.get $$x)))
)`);

// ./test/core/float_exprs.wast:1680
assert_return(
  () =>
    invoke($67, `f32.no_fold_6x_via_add`, [
      value("f32", -855513700000000000000000000000),
    ]),
  [value("f32", -5133083000000000000000000000000)],
);

// ./test/core/float_exprs.wast:1681
assert_return(
  () =>
    invoke($67, `f32.no_fold_6x_via_add`, [
      value("f32", -0.00000000000000000000001209506),
    ]),
  [value("f32", -0.00000000000000000000007257036)],
);

// ./test/core/float_exprs.wast:1682
assert_return(
  () =>
    invoke($67, `f32.no_fold_6x_via_add`, [
      value("f32", 0.000000000000000000000006642689),
    ]),
  [value("f32", 0.000000000000000000000039856134)],
);

// ./test/core/float_exprs.wast:1683
assert_return(
  () =>
    invoke($67, `f32.no_fold_6x_via_add`, [value("f32", -0.0000000006147346)]),
  [value("f32", -0.0000000036884074)],
);

// ./test/core/float_exprs.wast:1684
assert_return(
  () =>
    invoke($67, `f32.no_fold_6x_via_add`, [
      value("f32", -1209858100000000000000000),
    ]),
  [value("f32", -7259148300000000000000000)],
);

// ./test/core/float_exprs.wast:1686
assert_return(
  () =>
    invoke($67, `f64.no_fold_6x_via_add`, [
      value("f64", -351704490602771400000),
    ]),
  [value("f64", -2110226943616628600000)],
);

// ./test/core/float_exprs.wast:1687
assert_return(
  () =>
    invoke($67, `f64.no_fold_6x_via_add`, [
      value(
        "f64",
        -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000014824294109868734,
      ),
    ]),
  [value(
    "f64",
    -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000008894576465921239,
  )],
);

// ./test/core/float_exprs.wast:1688
assert_return(
  () =>
    invoke($67, `f64.no_fold_6x_via_add`, [
      value(
        "f64",
        -7484567838781003000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    -44907407032686014000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:1689
assert_return(
  () =>
    invoke($67, `f64.no_fold_6x_via_add`, [
      value(
        "f64",
        17277868192936067000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    103667209157616410000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:1690
assert_return(
  () =>
    invoke($67, `f64.no_fold_6x_via_add`, [
      value(
        "f64",
        -43116397525195610000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    -258698385151173640000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:1695
let $68 = instantiate(`(module
  (func (export "f32.no_fold_div_div") (param $$x f32) (param $$y f32) (param $$z f32) (result f32)
    (f32.div (f32.div (local.get $$x) (local.get $$y)) (local.get $$z)))

  (func (export "f64.no_fold_div_div") (param $$x f64) (param $$y f64) (param $$z f64) (result f64)
    (f64.div (f64.div (local.get $$x) (local.get $$y)) (local.get $$z)))
)`);

// ./test/core/float_exprs.wast:1703
assert_return(
  () =>
    invoke($68, `f32.no_fold_div_div`, [
      value("f32", -593847530000000000000000),
      value("f32", -0.000030265672),
      value("f32", -1584.8682),
    ]),
  [value("f32", -12380309000000000000000000)],
);

// ./test/core/float_exprs.wast:1704
assert_return(
  () =>
    invoke($68, `f32.no_fold_div_div`, [
      value("f32", 0.0000000000000000000015438962),
      value("f32", 2533429300000000000000000000000000),
      value("f32", -0.00000000000000000000000000000000026844783),
    ]),
  [value("f32", -0)],
);

// ./test/core/float_exprs.wast:1705
assert_return(
  () =>
    invoke($68, `f32.no_fold_div_div`, [
      value("f32", 13417423000000),
      value("f32", 0.000000000000000000000000000000029339205),
      value("f32", 76386374000000000000000000000000),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/float_exprs.wast:1706
assert_return(
  () =>
    invoke($68, `f32.no_fold_div_div`, [
      value("f32", -0.00010776529),
      value("f32", -34220943000000000000000000000000000000),
      value("f32", -0.00000000000016562324),
    ]),
  [value("f32", -0.000000000000000000000000000019011327)],
);

// ./test/core/float_exprs.wast:1707
assert_return(
  () =>
    invoke($68, `f32.no_fold_div_div`, [
      value("f32", 130582500000000),
      value("f32", 96245350000000000),
      value("f32", -41461545000000000000000000000000000000),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000032723)],
);

// ./test/core/float_exprs.wast:1709
assert_return(
  () =>
    invoke($68, `f64.no_fold_div_div`, [
      value(
        "f64",
        477762874671014340000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        102786720420404010000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000718999894988884,
      ),
    ]),
  [value(
    "f64",
    -64646730118787990000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:1710
assert_return(
  () =>
    invoke($68, `f64.no_fold_div_div`, [
      value(
        "f64",
        -21790236783875714000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value("f64", 0.0000000028324436844616576),
      value(
        "f64",
        186110768259868700000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    -41336068079920670000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:1711
assert_return(
  () =>
    invoke($68, `f64.no_fold_div_div`, [
      value("f64", -7.287619347826683),
      value(
        "f64",
        -13467607316739855000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value("f64", 2462719007013688000000000000000000000000000000000000),
    ]),
  [value(
    "f64",
    0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000219725454,
  )],
);

// ./test/core/float_exprs.wast:1712
assert_return(
  () =>
    invoke($68, `f64.no_fold_div_div`, [
      value(
        "f64",
        -286552397862963300000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000010211980370639414,
      ),
      value(
        "f64",
        28764586483324010000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value("f64", -Infinity)],
);

// ./test/core/float_exprs.wast:1713
assert_return(
  () =>
    invoke($68, `f64.no_fold_div_div`, [
      value(
        "f64",
        -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000009525735602663874,
      ),
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000050233948816631796,
      ),
      value(
        "f64",
        -0.0000000000000000000000000000000000000000028304570228221077,
      ),
    ]),
  [value(
    "f64",
    0.00000000000000000000000000000000000000000000006699534674970116,
  )],
);

// ./test/core/float_exprs.wast:1719
let $69 = instantiate(`(module
  (func (export "f32.no_fold_mul_divs") (param $$x f32) (param $$y f32) (param $$z f32) (param $$w f32) (result f32)
    (f32.mul (f32.div (local.get $$x) (local.get $$y)) (f32.div (local.get $$z) (local.get $$w))))

  (func (export "f64.no_fold_mul_divs") (param $$x f64) (param $$y f64) (param $$z f64) (param $$w f64) (result f64)
    (f64.mul (f64.div (local.get $$x) (local.get $$y)) (f64.div (local.get $$z) (local.get $$w))))
)`);

// ./test/core/float_exprs.wast:1727
assert_return(
  () =>
    invoke($69, `f32.no_fold_mul_divs`, [
      value("f32", -0.0000000000000000000000000000000027234733),
      value("f32", 0.0000000000000000000000000003897843),
      value("f32", 0.000000000000000000000000004847123),
      value("f32", -25.357775),
    ]),
  [value("f32", 0.0000000000000000000000000000000013355855)],
);

// ./test/core/float_exprs.wast:1728
assert_return(
  () =>
    invoke($69, `f32.no_fold_mul_divs`, [
      value("f32", -5372844000000000000000000000000),
      value("f32", 38340910),
      value("f32", 0.000014973162),
      value("f32", 0.19213825),
    ]),
  [value("f32", -10920475000000000000)],
);

// ./test/core/float_exprs.wast:1729
assert_return(
  () =>
    invoke($69, `f32.no_fold_mul_divs`, [
      value("f32", -16085042000),
      value("f32", -1092920200000),
      value("f32", -869606000),
      value("f32", -1201.206),
    ]),
  [value("f32", 10654.639)],
);

// ./test/core/float_exprs.wast:1730
assert_return(
  () =>
    invoke($69, `f32.no_fold_mul_divs`, [
      value("f32", -1271223140000000000000000000000000),
      value("f32", 0.00000000010768114),
      value("f32", 0.000018576271),
      value("f32", 492686200000000000000000),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/float_exprs.wast:1731
assert_return(
  () =>
    invoke($69, `f32.no_fold_mul_divs`, [
      value("f32", 0.00000000000000013783864),
      value("f32", -0.000000000000000000065046285),
      value("f32", 0.00000000000000000000000000068167684),
      value("f32", 0.000000000022892627),
    ]),
  [value("f32", -0.000000000000063100295)],
);

// ./test/core/float_exprs.wast:1733
assert_return(
  () =>
    invoke($69, `f64.no_fold_mul_divs`, [
      value(
        "f64",
        -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000003466499805233369,
      ),
      value(
        "f64",
        -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000004045567512248635,
      ),
      value(
        "f64",
        -646234107060759200000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value("f64", 100455895333540740000000000000000000000000000000000000000),
    ]),
  [value("f64", -55.12215321310017)],
);

// ./test/core/float_exprs.wast:1734
assert_return(
  () =>
    invoke($69, `f64.no_fold_mul_divs`, [
      value("f64", -50548839076363250000000000000000000),
      value(
        "f64",
        0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000022223781649976275,
      ),
      value(
        "f64",
        -15029790371100852000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        -699412375953812100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value("f64", -Infinity)],
);

// ./test/core/float_exprs.wast:1735
assert_return(
  () =>
    invoke($69, `f64.no_fold_mul_divs`, [
      value(
        "f64",
        -836111653634494700000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        -10029528876067567000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        -0.0000000000000000000000000000000000000000012867801766038772,
      ),
      value(
        "f64",
        -42230277746883753000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002540178100556387,
  )],
);

// ./test/core/float_exprs.wast:1736
assert_return(
  () =>
    invoke($69, `f64.no_fold_mul_divs`, [
      value("f64", -1202003211641119300000000000000000000000),
      value(
        "f64",
        -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000004667409771338769,
      ),
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000010888652376540085,
      ),
      value(
        "f64",
        18334948666517216000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:1737
assert_return(
  () =>
    invoke($69, `f64.no_fold_mul_divs`, [
      value("f64", 0.000006331839568840419),
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000000000000000000000005544474241905778,
      ),
      value(
        "f64",
        0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000027822472480359097,
      ),
      value(
        "f64",
        -14419321081893022000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000022035374770746518,
  )],
);

// ./test/core/float_exprs.wast:1741
let $70 = instantiate(`(module
  (func (export "f32.no_fold_add_divs") (param $$x f32) (param $$y f32) (param $$z f32) (result f32)
    (f32.add (f32.div (local.get $$x) (local.get $$z)) (f32.div (local.get $$y) (local.get $$z))))

  (func (export "f64.no_fold_add_divs") (param $$x f64) (param $$y f64) (param $$z f64) (result f64)
    (f64.add (f64.div (local.get $$x) (local.get $$z)) (f64.div (local.get $$y) (local.get $$z))))
)`);

// ./test/core/float_exprs.wast:1749
assert_return(
  () =>
    invoke($70, `f32.no_fold_add_divs`, [
      value("f32", 377.3689),
      value("f32", -0.040118184),
      value("f32", -136292990000000000000000000000000000000),
    ]),
  [value("f32", -0.0000000000000000000000000000000000027685121)],
);

// ./test/core/float_exprs.wast:1750
assert_return(
  () =>
    invoke($70, `f32.no_fold_add_divs`, [
      value("f32", -0.00000000000000000018234023),
      value("f32", -0.0000000000000033970288),
      value("f32", -170996700000000),
    ]),
  [value("f32", 0.000000000000000000000000000019867115)],
);

// ./test/core/float_exprs.wast:1751
assert_return(
  () =>
    invoke($70, `f32.no_fold_add_divs`, [
      value("f32", -0.000000000000019672638),
      value("f32", 0.00000000000000000006414099),
      value("f32", -541989070000000),
    ]),
  [value("f32", 0.000000000000000000000000000036296997)],
);

// ./test/core/float_exprs.wast:1752
assert_return(
  () =>
    invoke($70, `f32.no_fold_add_divs`, [
      value("f32", -0.0000000000000000000000000000004038506),
      value("f32", 0.000000000000000000000000000003848228),
      value("f32", -345237200000000000000000000),
    ]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:1753
assert_return(
  () =>
    invoke($70, `f32.no_fold_add_divs`, [
      value("f32", 0.0010934415),
      value("f32", 0.20703124),
      value("f32", 0.00000000000000000000000000000000000013509784),
    ]),
  [value("f32", 1540547700000000000000000000000000000)],
);

// ./test/core/float_exprs.wast:1755
assert_return(
  () =>
    invoke($70, `f64.no_fold_add_divs`, [
      value(
        "f64",
        -4917019432143760000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        68132156322019020000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        26125410100237784000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000024196801752520584,
  )],
);

// ./test/core/float_exprs.wast:1756
assert_return(
  () =>
    invoke($70, `f64.no_fold_add_divs`, [
      value("f64", -10206467953224550),
      value("f64", 63.422616671746226),
      value(
        "f64",
        -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000016024747869814892,
      ),
    ]),
  [value(
    "f64",
    6369190976445851000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:1757
assert_return(
  () =>
    invoke($70, `f64.no_fold_add_divs`, [
      value(
        "f64",
        -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000015270569633109837,
      ),
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000025755503329232514,
      ),
      value(
        "f64",
        58826939164214920000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:1758
assert_return(
  () =>
    invoke($70, `f64.no_fold_add_divs`, [
      value(
        "f64",
        26667964874394640000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        -2131569252493657800000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value("f64", 0.000000000000000000000000000000000000012377004518680012),
    ]),
  [value(
    "f64",
    -172217969324625340000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:1759
assert_return(
  () =>
    invoke($70, `f64.no_fold_add_divs`, [
      value(
        "f64",
        -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000012952888377288216,
      ),
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005808769259900048,
      ),
      value("f64", 0.0000000000000000000016745741699443756),
    ]),
  [value(
    "f64",
    -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000007735034106987796,
  )],
);

// ./test/core/float_exprs.wast:1763
let $71 = instantiate(`(module
  (func (export "f32.no_fold_sqrt_square") (param $$x f32) (result f32)
    (f32.sqrt (f32.mul (local.get $$x) (local.get $$x))))

  (func (export "f64.no_fold_sqrt_square") (param $$x f64) (result f64)
    (f64.sqrt (f64.mul (local.get $$x) (local.get $$x))))
)`);

// ./test/core/float_exprs.wast:1771
assert_return(
  () =>
    invoke($71, `f32.no_fold_sqrt_square`, [
      value("f32", -0.00000000000000000001846),
    ]),
  [value("f32", 0.00000000000000000001846001)],
);

// ./test/core/float_exprs.wast:1772
assert_return(
  () =>
    invoke($71, `f32.no_fold_sqrt_square`, [
      value("f32", -0.00000000000000000000017907473),
    ]),
  [value("f32", 0.00000000000000000000017952678)],
);

// ./test/core/float_exprs.wast:1773
assert_return(
  () =>
    invoke($71, `f32.no_fold_sqrt_square`, [
      value("f32", -0.00000000000000000000079120785),
    ]),
  [value("f32", 0.000000000000000000000791442)],
);

// ./test/core/float_exprs.wast:1774
assert_return(
  () =>
    invoke($71, `f32.no_fold_sqrt_square`, [
      value("f32", 0.000000000000000000000000018012938),
    ]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:1775
assert_return(
  () =>
    invoke($71, `f32.no_fold_sqrt_square`, [
      value("f32", 610501970000000000000000000000000),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/float_exprs.wast:1777
assert_return(
  () =>
    invoke($71, `f64.no_fold_sqrt_square`, [
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000006209297167747496,
      ),
    ]),
  [value(
    "f64",
    0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000006209299542179727,
  )],
);

// ./test/core/float_exprs.wast:1778
assert_return(
  () =>
    invoke($71, `f64.no_fold_sqrt_square`, [
      value(
        "f64",
        -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000024211175303738945,
      ),
    ]),
  [value(
    "f64",
    0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000024211175303738937,
  )],
);

// ./test/core/float_exprs.wast:1779
assert_return(
  () =>
    invoke($71, `f64.no_fold_sqrt_square`, [
      value(
        "f64",
        -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000016460687611875645,
      ),
    ]),
  [value(
    "f64",
    0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000016460687611532367,
  )],
);

// ./test/core/float_exprs.wast:1780
assert_return(
  () =>
    invoke($71, `f64.no_fold_sqrt_square`, [
      value(
        "f64",
        -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000003797811613378828,
      ),
    ]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:1781
assert_return(
  () =>
    invoke($71, `f64.no_fold_sqrt_square`, [
      value(
        "f64",
        815808428460559200000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value("f64", Infinity)],
);

// ./test/core/float_exprs.wast:1785
let $72 = instantiate(`(module
  (func (export "f32.no_fold_mul_sqrts") (param $$x f32) (param $$y f32) (result f32)
    (f32.mul (f32.sqrt (local.get $$x)) (f32.sqrt (local.get $$y))))

  (func (export "f64.no_fold_mul_sqrts") (param $$x f64) (param $$y f64) (result f64)
    (f64.mul (f64.sqrt (local.get $$x)) (f64.sqrt (local.get $$y))))
)`);

// ./test/core/float_exprs.wast:1793
assert_return(
  () =>
    invoke($72, `f32.no_fold_mul_sqrts`, [
      value("f32", 0.000000000000000000000000000000000000043885047),
      value("f32", -0.00000000000000000000000011867334),
    ]),
  [`canonical_nan`],
);

// ./test/core/float_exprs.wast:1794
assert_return(
  () =>
    invoke($72, `f32.no_fold_mul_sqrts`, [
      value("f32", 0.00000000000000000000000000025365908),
      value("f32", 0.00000000041320675),
    ]),
  [value("f32", 0.00000000000000000032374932)],
);

// ./test/core/float_exprs.wast:1795
assert_return(
  () =>
    invoke($72, `f32.no_fold_mul_sqrts`, [
      value("f32", 0.0000000000000000000000000042144832),
      value("f32", 97.249115),
    ]),
  [value("f32", 0.00000000000064019905)],
);

// ./test/core/float_exprs.wast:1796
assert_return(
  () =>
    invoke($72, `f32.no_fold_mul_sqrts`, [
      value("f32", 3724076300000000000000000000000),
      value("f32", 0.002944908),
    ]),
  [value("f32", 104723750000000)],
);

// ./test/core/float_exprs.wast:1797
assert_return(
  () =>
    invoke($72, `f32.no_fold_mul_sqrts`, [
      value("f32", 0.00000000000000001866056),
      value("f32", 0.002111261),
    ]),
  [value("f32", 0.00000000019848755)],
);

// ./test/core/float_exprs.wast:1799
assert_return(
  () =>
    invoke($72, `f64.no_fold_mul_sqrts`, [
      value(
        "f64",
        -0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000012742064369772862,
      ),
      value("f64", -0.006829962938197246),
    ]),
  [`canonical_nan`],
);

// ./test/core/float_exprs.wast:1800
assert_return(
  () =>
    invoke($72, `f64.no_fold_mul_sqrts`, [
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000037082569269527534,
      ),
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000047183002857015043,
      ),
    ]),
  [value(
    "f64",
    0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000041829020688865954,
  )],
);

// ./test/core/float_exprs.wast:1801
assert_return(
  () =>
    invoke($72, `f64.no_fold_mul_sqrts`, [
      value("f64", 0.000000000000000000000000002329359505918655),
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000020743399642806364,
      ),
    ]),
  [value(
    "f64",
    0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000021981545701574452,
  )],
);

// ./test/core/float_exprs.wast:1802
assert_return(
  () =>
    invoke($72, `f64.no_fold_mul_sqrts`, [
      value(
        "f64",
        0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000010541899336289437,
      ),
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000598123819872803,
      ),
    ]),
  [value(
    "f64",
    0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002511047809129887,
  )],
);

// ./test/core/float_exprs.wast:1803
assert_return(
  () =>
    invoke($72, `f64.no_fold_mul_sqrts`, [
      value("f64", 25589482.717358638),
      value(
        "f64",
        39138912071199020000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    1000771959050695500000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:1807
let $73 = instantiate(`(module
  (func (export "f32.no_fold_div_sqrts") (param $$x f32) (param $$y f32) (result f32)
    (f32.div (f32.sqrt (local.get $$x)) (f32.sqrt (local.get $$y))))

  (func (export "f64.no_fold_div_sqrts") (param $$x f64) (param $$y f64) (result f64)
    (f64.div (f64.sqrt (local.get $$x)) (f64.sqrt (local.get $$y))))
)`);

// ./test/core/float_exprs.wast:1815
assert_return(
  () =>
    invoke($73, `f32.no_fold_div_sqrts`, [
      value("f32", -58545012),
      value("f32", -0.000000000000000006443773),
    ]),
  [`canonical_nan`],
);

// ./test/core/float_exprs.wast:1816
assert_return(
  () =>
    invoke($73, `f32.no_fold_div_sqrts`, [
      value("f32", 7407384000),
      value("f32", 209778930),
    ]),
  [value("f32", 5.9422584)],
);

// ./test/core/float_exprs.wast:1817
assert_return(
  () =>
    invoke($73, `f32.no_fold_div_sqrts`, [
      value("f32", 0.0000000000000000000000000000000000013764126),
      value("f32", 54692.9),
    ]),
  [value("f32", 0.0000000000000000000050165927)],
);

// ./test/core/float_exprs.wast:1818
assert_return(
  () =>
    invoke($73, `f32.no_fold_div_sqrts`, [
      value("f32", 979288960000000000),
      value("f32", 0.0000000012643552),
    ]),
  [value("f32", 27830490000000)],
);

// ./test/core/float_exprs.wast:1819
assert_return(
  () =>
    invoke($73, `f32.no_fold_div_sqrts`, [
      value("f32", 0.00000000000000000000000000000000029141283),
      value("f32", 0.00000000000000000000000000000017928174),
    ]),
  [value("f32", 0.04031682)],
);

// ./test/core/float_exprs.wast:1821
assert_return(
  () =>
    invoke($73, `f64.no_fold_div_sqrts`, [
      value(
        "f64",
        -0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000012206137319883022,
      ),
      value(
        "f64",
        -0.000000000000000000000000000000000000000000000000000000008209583449676083,
      ),
    ]),
  [`canonical_nan`],
);

// ./test/core/float_exprs.wast:1822
assert_return(
  () =>
    invoke($73, `f64.no_fold_div_sqrts`, [
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000033818852462305824,
      ),
      value(
        "f64",
        7655783976315048000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000021017671425665687,
  )],
);

// ./test/core/float_exprs.wast:1823
assert_return(
  () =>
    invoke($73, `f64.no_fold_div_sqrts`, [
      value(
        "f64",
        45963335670647510000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value("f64", 0.0000000000000000000000000000000023932467846883046),
    ]),
  [value(
    "f64",
    138583660172663150000000000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:1824
assert_return(
  () =>
    invoke($73, `f64.no_fold_div_sqrts`, [
      value(
        "f64",
        0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000025327340978668086,
      ),
      value(
        "f64",
        4475305129961258000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000023789399141325018,
  )],
);

// ./test/core/float_exprs.wast:1825
assert_return(
  () =>
    invoke($73, `f64.no_fold_div_sqrts`, [
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000005103070160197939,
      ),
      value(
        "f64",
        460157669098082500000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000010530826009924495,
  )],
);

// ./test/core/float_exprs.wast:1829
let $74 = instantiate(`(module
  (func (export "f32.no_fold_mul_sqrt_div") (param $$x f32) (param $$y f32) (result f32)
    (f32.div (f32.mul (local.get $$x) (f32.sqrt (local.get $$y))) (local.get $$y)))

  (func (export "f64.no_fold_mul_sqrt_div") (param $$x f64) (param $$y f64) (result f64)
    (f64.div (f64.mul (local.get $$x) (f64.sqrt (local.get $$y))) (local.get $$y)))
)`);

// ./test/core/float_exprs.wast:1837
assert_return(
  () =>
    invoke($74, `f32.no_fold_mul_sqrt_div`, [
      value("f32", -4728556800000000000000000),
      value("f32", 8677282000000000000000000000),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/float_exprs.wast:1838
assert_return(
  () =>
    invoke($74, `f32.no_fold_mul_sqrt_div`, [
      value("f32", -0.0000000000000000000000000000000000011776882),
      value("f32", 0.000000000000000000000000000009805153),
    ]),
  [value("f32", -0)],
);

// ./test/core/float_exprs.wast:1839
assert_return(
  () =>
    invoke($74, `f32.no_fold_mul_sqrt_div`, [
      value("f32", 816717060),
      value("f32", 0.000000000000000000000000000000000000003323171),
    ]),
  [value("f32", 14167568000000000000000000000)],
);

// ./test/core/float_exprs.wast:1840
assert_return(
  () =>
    invoke($74, `f32.no_fold_mul_sqrt_div`, [
      value("f32", -11932267000000),
      value("f32", 8637067000000000000000000000000000),
    ]),
  [value("f32", -0.00012839255)],
);

// ./test/core/float_exprs.wast:1841
assert_return(
  () =>
    invoke($74, `f32.no_fold_mul_sqrt_div`, [
      value("f32", -401.0235),
      value("f32", 134.33022),
    ]),
  [value("f32", -34.600548)],
);

// ./test/core/float_exprs.wast:1843
assert_return(
  () =>
    invoke($74, `f64.no_fold_mul_sqrt_div`, [
      value(
        "f64",
        1468134622910490500000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        2466074582285183000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value("f64", Infinity)],
);

// ./test/core/float_exprs.wast:1844
assert_return(
  () =>
    invoke($74, `f64.no_fold_mul_sqrt_div`, [
      value(
        "f64",
        -0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000017254022016758028,
      ),
      value(
        "f64",
        0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000055835540747130025,
      ),
    ]),
  [value("f64", -0)],
);

// ./test/core/float_exprs.wast:1845
assert_return(
  () =>
    invoke($74, `f64.no_fold_mul_sqrt_div`, [
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000016812810256029166,
      ),
      value(
        "f64",
        7362783602442129000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000006196112486187196,
  )],
);

// ./test/core/float_exprs.wast:1846
assert_return(
  () =>
    invoke($74, `f64.no_fold_mul_sqrt_div`, [
      value(
        "f64",
        -10605483729939836000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
      value(
        "f64",
        0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000622591783694072,
      ),
    ]),
  [value(
    "f64",
    -42503900822233765000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
  )],
);

// ./test/core/float_exprs.wast:1847
assert_return(
  () =>
    invoke($74, `f64.no_fold_mul_sqrt_div`, [
      value("f64", 26336349695373093000000000000000),
      value(
        "f64",
        30791413285853300000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000,
      ),
    ]),
  [value(
    "f64",
    0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000004746142447510695,
  )],
);

// ./test/core/float_exprs.wast:1852
let $75 = instantiate(`(module
  (func (export "f32.no_flush_intermediate_subnormal") (param $$x f32) (param $$y f32) (param $$z f32) (result f32)
    (f32.mul (f32.mul (local.get $$x) (local.get $$y)) (local.get $$z)))

  (func (export "f64.no_flush_intermediate_subnormal") (param $$x f64) (param $$y f64) (param $$z f64) (result f64)
    (f64.mul (f64.mul (local.get $$x) (local.get $$y)) (local.get $$z)))
)`);

// ./test/core/float_exprs.wast:1860
assert_return(
  () =>
    invoke($75, `f32.no_flush_intermediate_subnormal`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.00000011920929),
      value("f32", 8388608),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/float_exprs.wast:1861
assert_return(
  () =>
    invoke($75, `f64.no_flush_intermediate_subnormal`, [
      value(
        "f64",
        0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000022250738585072014,
      ),
      value("f64", 0.0000000000000002220446049250313),
      value("f64", 4503599627370496),
    ]),
  [value(
    "f64",
    0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000022250738585072014,
  )],
);

// ./test/core/float_exprs.wast:1866
let $76 = instantiate(`(module
  (func (export "f32.recoding_eq") (param $$x f32) (param $$y f32) (result i32)
    (f32.eq (f32.mul (local.get $$x) (local.get $$y)) (local.get $$x)))

  (func (export "f32.recoding_le") (param $$x f32) (param $$y f32) (result i32)
    (f32.le (f32.mul (local.get $$x) (local.get $$y)) (local.get $$x)))

  (func (export "f32.recoding_lt") (param $$x f32) (param $$y f32) (result i32)
    (f32.lt (f32.mul (local.get $$x) (local.get $$y)) (local.get $$x)))

  (func (export "f64.recoding_eq") (param $$x f64) (param $$y f64) (result i32)
    (f64.eq (f64.mul (local.get $$x) (local.get $$y)) (local.get $$x)))

  (func (export "f64.recoding_le") (param $$x f64) (param $$y f64) (result i32)
    (f64.le (f64.mul (local.get $$x) (local.get $$y)) (local.get $$x)))

  (func (export "f64.recoding_lt") (param $$x f64) (param $$y f64) (result i32)
    (f64.lt (f64.mul (local.get $$x) (local.get $$y)) (local.get $$x)))

  (func (export "recoding_demote") (param $$x f64) (param $$y f32) (result f32)
    (f32.mul (f32.demote_f64 (local.get $$x)) (local.get $$y)))
)`);

// ./test/core/float_exprs.wast:1889
assert_return(
  () =>
    invoke($76, `f32.recoding_eq`, [value("f32", -Infinity), value("f32", 3)]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:1890
assert_return(
  () =>
    invoke($76, `f32.recoding_le`, [value("f32", -Infinity), value("f32", 3)]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:1891
assert_return(
  () =>
    invoke($76, `f32.recoding_lt`, [value("f32", -Infinity), value("f32", 3)]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:1893
assert_return(
  () => invoke($76, `f32.recoding_eq`, [value("f32", 0), value("f32", 1)]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:1894
assert_return(
  () => invoke($76, `f32.recoding_le`, [value("f32", 0), value("f32", 1)]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:1895
assert_return(
  () => invoke($76, `f32.recoding_lt`, [value("f32", 0), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:1897
assert_return(
  () =>
    invoke($76, `f64.recoding_eq`, [value("f64", -Infinity), value("f64", 3)]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:1898
assert_return(
  () =>
    invoke($76, `f64.recoding_le`, [value("f64", -Infinity), value("f64", 3)]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:1899
assert_return(
  () =>
    invoke($76, `f64.recoding_lt`, [value("f64", -Infinity), value("f64", 3)]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:1901
assert_return(
  () => invoke($76, `f64.recoding_eq`, [value("f64", 0), value("f64", 1)]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:1902
assert_return(
  () => invoke($76, `f64.recoding_le`, [value("f64", 0), value("f64", 1)]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:1903
assert_return(
  () => invoke($76, `f64.recoding_lt`, [value("f64", 0), value("f64", 1)]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:1905
assert_return(
  () =>
    invoke($76, `recoding_demote`, [
      value("f64", 0.00000000000000000000000000000000000000023860049081905093),
      value("f32", 1221),
    ]),
  [value("f32", 0.0000000000000000000000000000000000002913312)],
);

// ./test/core/float_exprs.wast:1910
let $77 = instantiate(`(module
  (func (export "f32.no_extended_precision_div") (param $$x f32) (param $$y f32) (param $$z f32) (result i32)
    (f32.eq (f32.div (local.get $$x) (local.get $$y)) (local.get $$z)))

  (func (export "f64.no_extended_precision_div") (param $$x f64) (param $$y f64) (param $$z f64) (result i32)
    (f64.eq (f64.div (local.get $$x) (local.get $$y)) (local.get $$z)))
)`);

// ./test/core/float_exprs.wast:1918
assert_return(
  () =>
    invoke($77, `f32.no_extended_precision_div`, [
      value("f32", 3),
      value("f32", 7),
      value("f32", 0.42857143),
    ]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:1919
assert_return(
  () =>
    invoke($77, `f64.no_extended_precision_div`, [
      value("f64", 3),
      value("f64", 7),
      value("f64", 0.42857142857142855),
    ]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:1926
let $78 = instantiate(`(module
  (func (export "f32.no_distribute_exact") (param $$x f32) (result f32)
    (f32.add (f32.mul (f32.const -8.0) (local.get $$x)) (f32.mul (f32.const 8.0) (local.get $$x))))

  (func (export "f64.no_distribute_exact") (param $$x f64) (result f64)
    (f64.add (f64.mul (f64.const -8.0) (local.get $$x)) (f64.mul (f64.const 8.0) (local.get $$x))))
)`);

// ./test/core/float_exprs.wast:1934
assert_return(
  () => invoke($78, `f32.no_distribute_exact`, [value("f32", -0)]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:1935
assert_return(
  () => invoke($78, `f64.no_distribute_exact`, [value("f64", -0)]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:1940
let $79 = instantiate(`(module
  (func (export "f32.sqrt") (param f32) (result f32)
    (f32.sqrt (local.get 0)))

  (func (export "f32.xkcd_sqrt_2") (param f32) (param f32) (param f32) (param f32) (result f32)
    (f32.add (f32.div (local.get 0) (local.get 1)) (f32.div (local.get 2) (f32.sub (local.get 3) (local.get 2)))))

  (func (export "f32.xkcd_sqrt_3") (param f32) (param f32) (param f32) (result f32)
    (f32.div (f32.mul (local.get 0) (local.get 1)) (local.get 2)))

  (func (export "f32.xkcd_sqrt_5") (param f32) (param f32) (param f32) (result f32)
    (f32.add (f32.div (local.get 0) (local.get 1)) (f32.div (local.get 2) (local.get 0))))

  (func (export "f32.xkcd_better_sqrt_5") (param f32) (param f32) (param f32) (param f32) (result f32)
    (f32.div (f32.add (local.get 0) (f32.mul (local.get 1) (local.get 2))) (f32.sub (local.get 3) (f32.mul (local.get 1) (local.get 2)))))

  (func (export "f64.sqrt") (param f64) (result f64)
    (f64.sqrt (local.get 0)))

  (func (export "f64.xkcd_sqrt_2") (param f64) (param f64) (param f64) (param f64) (result f64)
    (f64.add (f64.div (local.get 0) (local.get 1)) (f64.div (local.get 2) (f64.sub (local.get 3) (local.get 2)))))

  (func (export "f64.xkcd_sqrt_3") (param f64) (param f64) (param f64) (result f64)
    (f64.div (f64.mul (local.get 0) (local.get 1)) (local.get 2)))

  (func (export "f64.xkcd_sqrt_5") (param f64) (param f64) (param f64) (result f64)
    (f64.add (f64.div (local.get 0) (local.get 1)) (f64.div (local.get 2) (local.get 0))))

  (func (export "f64.xkcd_better_sqrt_5") (param f64) (param f64) (param f64) (param f64) (result f64)
    (f64.div (f64.add (local.get 0) (f64.mul (local.get 1) (local.get 2))) (f64.sub (local.get 3) (f64.mul (local.get 1) (local.get 2)))))
)`);

// ./test/core/float_exprs.wast:1972
assert_return(() => invoke($79, `f32.sqrt`, [value("f32", 2)]), [
  value("f32", 1.4142135),
]);

// ./test/core/float_exprs.wast:1973
assert_return(
  () =>
    invoke($79, `f32.xkcd_sqrt_2`, [
      value("f32", 3),
      value("f32", 5),
      value("f32", 3.1415927),
      value("f32", 7),
    ]),
  [value("f32", 1.4142201)],
);

// ./test/core/float_exprs.wast:1974
assert_return(() => invoke($79, `f32.sqrt`, [value("f32", 3)]), [
  value("f32", 1.7320508),
]);

// ./test/core/float_exprs.wast:1975
assert_return(
  () =>
    invoke($79, `f32.xkcd_sqrt_3`, [
      value("f32", 2),
      value("f32", 2.7182817),
      value("f32", 3.1415927),
    ]),
  [value("f32", 1.7305119)],
);

// ./test/core/float_exprs.wast:1976
assert_return(() => invoke($79, `f32.sqrt`, [value("f32", 5)]), [
  value("f32", 2.236068),
]);

// ./test/core/float_exprs.wast:1977
assert_return(
  () =>
    invoke($79, `f32.xkcd_sqrt_5`, [
      value("f32", 2),
      value("f32", 2.7182817),
      value("f32", 3),
    ]),
  [value("f32", 2.2357588)],
);

// ./test/core/float_exprs.wast:1978
assert_return(
  () =>
    invoke($79, `f32.xkcd_better_sqrt_5`, [
      value("f32", 13),
      value("f32", 4),
      value("f32", 3.1415927),
      value("f32", 24),
    ]),
  [value("f32", 2.236068)],
);

// ./test/core/float_exprs.wast:1980
assert_return(() => invoke($79, `f64.sqrt`, [value("f64", 2)]), [
  value("f64", 1.4142135623730951),
]);

// ./test/core/float_exprs.wast:1981
assert_return(
  () =>
    invoke($79, `f64.xkcd_sqrt_2`, [
      value("f64", 3),
      value("f64", 5),
      value("f64", 3.141592653589793),
      value("f64", 7),
    ]),
  [value("f64", 1.4142200580539208)],
);

// ./test/core/float_exprs.wast:1982
assert_return(() => invoke($79, `f64.sqrt`, [value("f64", 3)]), [
  value("f64", 1.7320508075688772),
]);

// ./test/core/float_exprs.wast:1983
assert_return(
  () =>
    invoke($79, `f64.xkcd_sqrt_3`, [
      value("f64", 2),
      value("f64", 2.718281828459045),
      value("f64", 3.141592653589793),
    ]),
  [value("f64", 1.7305119588645301)],
);

// ./test/core/float_exprs.wast:1984
assert_return(() => invoke($79, `f64.sqrt`, [value("f64", 5)]), [
  value("f64", 2.23606797749979),
]);

// ./test/core/float_exprs.wast:1985
assert_return(
  () =>
    invoke($79, `f64.xkcd_sqrt_5`, [
      value("f64", 2),
      value("f64", 2.718281828459045),
      value("f64", 3),
    ]),
  [value("f64", 2.2357588823428847)],
);

// ./test/core/float_exprs.wast:1986
assert_return(
  () =>
    invoke($79, `f64.xkcd_better_sqrt_5`, [
      value("f64", 13),
      value("f64", 4),
      value("f64", 3.141592653589793),
      value("f64", 24),
    ]),
  [value("f64", 2.2360678094452893)],
);

// ./test/core/float_exprs.wast:1991
let $80 = instantiate(`(module
  (func (export "f32.compute_radix") (param $$0 f32) (param $$1 f32) (result f32)
    (loop $$label$$0
      (br_if $$label$$0
        (f32.eq
          (f32.add
            (f32.sub
              (f32.add
                (local.tee $$0 (f32.add (local.get $$0) (local.get $$0)))
                (f32.const 1)
              )
              (local.get $$0)
            )
            (f32.const -1)
          )
          (f32.const 0)
        )
      )
    )
    (loop $$label$$2
      (br_if $$label$$2
        (f32.ne
          (f32.sub
            (f32.sub
              (f32.add
                (local.get $$0)
                (local.tee $$1 (f32.add (local.get $$1) (f32.const 1)))
              )
              (local.get $$0)
            )
            (local.get $$1)
          )
          (f32.const 0)
        )
      )
    )
    (local.get $$1)
  )

  (func (export "f64.compute_radix") (param $$0 f64) (param $$1 f64) (result f64)
    (loop $$label$$0
      (br_if $$label$$0
        (f64.eq
          (f64.add
            (f64.sub
              (f64.add
                (local.tee $$0 (f64.add (local.get $$0) (local.get $$0)))
                (f64.const 1)
              )
              (local.get $$0)
            )
            (f64.const -1)
          )
          (f64.const 0)
        )
      )
    )
    (loop $$label$$2
      (br_if $$label$$2
        (f64.ne
          (f64.sub
            (f64.sub
              (f64.add
                (local.get $$0)
                (local.tee $$1 (f64.add (local.get $$1) (f64.const 1)))
              )
              (local.get $$0)
            )
            (local.get $$1)
          )
          (f64.const 0)
        )
      )
    )
    (local.get $$1)
  )
)`);

// ./test/core/float_exprs.wast:2069
assert_return(
  () => invoke($80, `f32.compute_radix`, [value("f32", 1), value("f32", 1)]),
  [value("f32", 2)],
);

// ./test/core/float_exprs.wast:2070
assert_return(
  () => invoke($80, `f64.compute_radix`, [value("f64", 1), value("f64", 1)]),
  [value("f64", 2)],
);

// ./test/core/float_exprs.wast:2075
let $81 = instantiate(`(module
  (func (export "f32.no_fold_sub1_mul_add") (param $$x f32) (param $$y f32) (result f32)
    (f32.add (f32.mul (f32.sub (local.get $$x) (f32.const 1.0)) (local.get $$y)) (local.get $$y)))

  (func (export "f64.no_fold_sub1_mul_add") (param $$x f64) (param $$y f64) (result f64)
    (f64.add (f64.mul (f64.sub (local.get $$x) (f64.const 1.0)) (local.get $$y)) (local.get $$y)))
)`);

// ./test/core/float_exprs.wast:2083
assert_return(
  () =>
    invoke($81, `f32.no_fold_sub1_mul_add`, [
      value("f32", 0.00000000023283064),
      value("f32", 1),
    ]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:2084
assert_return(
  () =>
    invoke($81, `f64.no_fold_sub1_mul_add`, [
      value("f64", 0.00000000000000000005421010862427522),
      value("f64", 1),
    ]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:2089
let $82 = instantiate(`(module
  (func (export "f32.no_fold_add_le_monotonicity") (param $$x f32) (param $$y f32) (param $$z f32) (result i32)
    (f32.le (f32.add (local.get $$x) (local.get $$z)) (f32.add (local.get $$y) (local.get $$z))))

  (func (export "f32.no_fold_add_ge_monotonicity") (param $$x f32) (param $$y f32) (param $$z f32) (result i32)
    (f32.ge (f32.add (local.get $$x) (local.get $$z)) (f32.add (local.get $$y) (local.get $$z))))

  (func (export "f64.no_fold_add_le_monotonicity") (param $$x f64) (param $$y f64) (param $$z f64) (result i32)
    (f64.le (f64.add (local.get $$x) (local.get $$z)) (f64.add (local.get $$y) (local.get $$z))))

  (func (export "f64.no_fold_add_ge_monotonicity") (param $$x f64) (param $$y f64) (param $$z f64) (result i32)
    (f64.ge (f64.add (local.get $$x) (local.get $$z)) (f64.add (local.get $$y) (local.get $$z))))
)`);

// ./test/core/float_exprs.wast:2103
assert_return(
  () =>
    invoke($82, `f32.no_fold_add_le_monotonicity`, [
      value("f32", 0),
      value("f32", 0),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:2104
assert_return(
  () =>
    invoke($82, `f32.no_fold_add_le_monotonicity`, [
      value("f32", Infinity),
      value("f32", -Infinity),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:2105
assert_return(
  () =>
    invoke($82, `f64.no_fold_add_le_monotonicity`, [
      value("f64", 0),
      value("f64", 0),
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:2106
assert_return(
  () =>
    invoke($82, `f64.no_fold_add_le_monotonicity`, [
      value("f64", Infinity),
      value("f64", -Infinity),
      value("f64", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:2110
let $83 = instantiate(`(module
  (func (export "f32.not_lt") (param $$x f32) (param $$y f32) (result i32)
    (i32.eqz (f32.lt (local.get $$x) (local.get $$y))))

  (func (export "f32.not_le") (param $$x f32) (param $$y f32) (result i32)
    (i32.eqz (f32.le (local.get $$x) (local.get $$y))))

  (func (export "f32.not_gt") (param $$x f32) (param $$y f32) (result i32)
    (i32.eqz (f32.gt (local.get $$x) (local.get $$y))))

  (func (export "f32.not_ge") (param $$x f32) (param $$y f32) (result i32)
    (i32.eqz (f32.ge (local.get $$x) (local.get $$y))))

  (func (export "f64.not_lt") (param $$x f64) (param $$y f64) (result i32)
    (i32.eqz (f64.lt (local.get $$x) (local.get $$y))))

  (func (export "f64.not_le") (param $$x f64) (param $$y f64) (result i32)
    (i32.eqz (f64.le (local.get $$x) (local.get $$y))))

  (func (export "f64.not_gt") (param $$x f64) (param $$y f64) (result i32)
    (i32.eqz (f64.gt (local.get $$x) (local.get $$y))))

  (func (export "f64.not_ge") (param $$x f64) (param $$y f64) (result i32)
    (i32.eqz (f64.ge (local.get $$x) (local.get $$y))))
)`);

// ./test/core/float_exprs.wast:2136
assert_return(
  () =>
    invoke($83, `f32.not_lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0),
    ]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:2137
assert_return(
  () =>
    invoke($83, `f32.not_le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0),
    ]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:2138
assert_return(
  () =>
    invoke($83, `f32.not_gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0),
    ]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:2139
assert_return(
  () =>
    invoke($83, `f32.not_ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0),
    ]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:2140
assert_return(
  () =>
    invoke($83, `f64.not_lt`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
      value("f64", 0),
    ]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:2141
assert_return(
  () =>
    invoke($83, `f64.not_le`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
      value("f64", 0),
    ]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:2142
assert_return(
  () =>
    invoke($83, `f64.not_gt`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
      value("f64", 0),
    ]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:2143
assert_return(
  () =>
    invoke($83, `f64.not_ge`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
      value("f64", 0),
    ]),
  [value("i32", 1)],
);

// ./test/core/float_exprs.wast:2149
let $84 = instantiate(`(module
  (func (export "f32.epsilon") (result f32)
    (f32.sub (f32.const 1.0) (f32.mul (f32.const 3.0) (f32.sub (f32.div (f32.const 4.0) (f32.const 3.0)) (f32.const 1.0)))))

  (func (export "f64.epsilon") (result f64)
    (f64.sub (f64.const 1.0) (f64.mul (f64.const 3.0) (f64.sub (f64.div (f64.const 4.0) (f64.const 3.0)) (f64.const 1.0)))))
)`);

// ./test/core/float_exprs.wast:2157
assert_return(() => invoke($84, `f32.epsilon`, []), [
  value("f32", -0.00000011920929),
]);

// ./test/core/float_exprs.wast:2158
assert_return(() => invoke($84, `f64.epsilon`, []), [
  value("f64", 0.0000000000000002220446049250313),
]);

// ./test/core/float_exprs.wast:2164
let $85 = instantiate(`(module
  (func (export "f32.epsilon") (result f32)
    (local $$x f32)
    (local $$result f32)
    (local.set $$x (f32.const 1))
    (loop $$loop
      (br_if $$loop
        (f32.gt
          (f32.add
            (local.tee $$x
              (f32.mul
                (local.tee $$result (local.get $$x))
                (f32.const 0.5)
              )
            )
            (f32.const 1)
          )
          (f32.const 1)
        )
      )
    )
    (local.get $$result)
  )

  (func (export "f64.epsilon") (result f64)
    (local $$x f64)
    (local $$result f64)
    (local.set $$x (f64.const 1))
    (loop $$loop
      (br_if $$loop
        (f64.gt
          (f64.add
            (local.tee $$x
              (f64.mul
                (local.tee $$result (local.get $$x))
                (f64.const 0.5)
              )
            )
            (f64.const 1)
          )
          (f64.const 1)
        )
      )
    )
    (local.get $$result)
  )
)`);

// ./test/core/float_exprs.wast:2212
assert_return(() => invoke($85, `f32.epsilon`, []), [
  value("f32", 0.00000011920929),
]);

// ./test/core/float_exprs.wast:2213
assert_return(() => invoke($85, `f64.epsilon`, []), [
  value("f64", 0.0000000000000002220446049250313),
]);

// ./test/core/float_exprs.wast:2218
let $86 = instantiate(`(module
  (func (export "f32.no_trichotomy_lt") (param $$x f32) (param $$y f32) (result i32)
    (i32.or (f32.lt (local.get $$x) (local.get $$y)) (f32.ge (local.get $$x) (local.get $$y))))
  (func (export "f32.no_trichotomy_le") (param $$x f32) (param $$y f32) (result i32)
    (i32.or (f32.le (local.get $$x) (local.get $$y)) (f32.gt (local.get $$x) (local.get $$y))))
  (func (export "f32.no_trichotomy_gt") (param $$x f32) (param $$y f32) (result i32)
    (i32.or (f32.gt (local.get $$x) (local.get $$y)) (f32.le (local.get $$x) (local.get $$y))))
  (func (export "f32.no_trichotomy_ge") (param $$x f32) (param $$y f32) (result i32)
    (i32.or (f32.ge (local.get $$x) (local.get $$y)) (f32.lt (local.get $$x) (local.get $$y))))

  (func (export "f64.no_trichotomy_lt") (param $$x f64) (param $$y f64) (result i32)
    (i32.or (f64.lt (local.get $$x) (local.get $$y)) (f64.ge (local.get $$x) (local.get $$y))))
  (func (export "f64.no_trichotomy_le") (param $$x f64) (param $$y f64) (result i32)
    (i32.or (f64.le (local.get $$x) (local.get $$y)) (f64.gt (local.get $$x) (local.get $$y))))
  (func (export "f64.no_trichotomy_gt") (param $$x f64) (param $$y f64) (result i32)
    (i32.or (f64.gt (local.get $$x) (local.get $$y)) (f64.le (local.get $$x) (local.get $$y))))
  (func (export "f64.no_trichotomy_ge") (param $$x f64) (param $$y f64) (result i32)
    (i32.or (f64.ge (local.get $$x) (local.get $$y)) (f64.lt (local.get $$x) (local.get $$y))))
)`);

// ./test/core/float_exprs.wast:2238
assert_return(
  () =>
    invoke($86, `f32.no_trichotomy_lt`, [
      value("f32", 0),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:2239
assert_return(
  () =>
    invoke($86, `f32.no_trichotomy_le`, [
      value("f32", 0),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:2240
assert_return(
  () =>
    invoke($86, `f32.no_trichotomy_gt`, [
      value("f32", 0),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:2241
assert_return(
  () =>
    invoke($86, `f32.no_trichotomy_ge`, [
      value("f32", 0),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:2242
assert_return(
  () =>
    invoke($86, `f64.no_trichotomy_lt`, [
      value("f64", 0),
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:2243
assert_return(
  () =>
    invoke($86, `f64.no_trichotomy_le`, [
      value("f64", 0),
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:2244
assert_return(
  () =>
    invoke($86, `f64.no_trichotomy_gt`, [
      value("f64", 0),
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:2245
assert_return(
  () =>
    invoke($86, `f64.no_trichotomy_ge`, [
      value("f64", 0),
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:2250
let $87 = instantiate(`(module
  (func (export "f32.arithmetic_nan_bitpattern")
        (param $$x i32) (param $$y i32) (result i32)
    (i32.and (i32.reinterpret_f32
               (f32.div
                 (f32.reinterpret_i32 (local.get $$x))
                 (f32.reinterpret_i32 (local.get $$y))))
             (i32.const 0x7fc00000)))
  (func (export "f32.canonical_nan_bitpattern")
        (param $$x i32) (param $$y i32) (result i32)
    (i32.and (i32.reinterpret_f32
               (f32.div
                 (f32.reinterpret_i32 (local.get $$x))
                 (f32.reinterpret_i32 (local.get $$y))))
             (i32.const 0x7fffffff)))
  (func (export "f32.nonarithmetic_nan_bitpattern")
        (param $$x i32) (result i32)
    (i32.reinterpret_f32 (f32.neg (f32.reinterpret_i32 (local.get $$x)))))

  (func (export "f64.arithmetic_nan_bitpattern")
        (param $$x i64) (param $$y i64) (result i64)
    (i64.and (i64.reinterpret_f64
               (f64.div
                 (f64.reinterpret_i64 (local.get $$x))
                 (f64.reinterpret_i64 (local.get $$y))))
             (i64.const 0x7ff8000000000000)))
  (func (export "f64.canonical_nan_bitpattern")
        (param $$x i64) (param $$y i64) (result i64)
    (i64.and (i64.reinterpret_f64
               (f64.div
                 (f64.reinterpret_i64 (local.get $$x))
                 (f64.reinterpret_i64 (local.get $$y))))
             (i64.const 0x7fffffffffffffff)))
  (func (export "f64.nonarithmetic_nan_bitpattern")
        (param $$x i64) (result i64)
    (i64.reinterpret_f64 (f64.neg (f64.reinterpret_i64 (local.get $$x)))))

  ;; Versions of no_fold testcases that only care about NaN bitpatterns.
  (func (export "f32.no_fold_sub_zero") (param $$x i32) (result i32)
    (i32.and (i32.reinterpret_f32 (f32.sub (f32.reinterpret_i32 (local.get $$x)) (f32.const 0.0)))
             (i32.const 0x7fc00000)))
  (func (export "f32.no_fold_neg0_sub") (param $$x i32) (result i32)
    (i32.and (i32.reinterpret_f32 (f32.sub (f32.const -0.0) (f32.reinterpret_i32 (local.get $$x))))
             (i32.const 0x7fc00000)))
  (func (export "f32.no_fold_mul_one") (param $$x i32) (result i32)
    (i32.and (i32.reinterpret_f32 (f32.mul (f32.reinterpret_i32 (local.get $$x)) (f32.const 1.0)))
             (i32.const 0x7fc00000)))
  (func (export "f32.no_fold_neg1_mul") (param $$x i32) (result i32)
    (i32.and (i32.reinterpret_f32 (f32.mul (f32.const -1.0) (f32.reinterpret_i32 (local.get $$x))))
             (i32.const 0x7fc00000)))
  (func (export "f32.no_fold_div_one") (param $$x i32) (result i32)
    (i32.and (i32.reinterpret_f32 (f32.div (f32.reinterpret_i32 (local.get $$x)) (f32.const 1.0)))
             (i32.const 0x7fc00000)))
  (func (export "f32.no_fold_div_neg1") (param $$x i32) (result i32)
    (i32.and (i32.reinterpret_f32 (f32.div (f32.reinterpret_i32 (local.get $$x)) (f32.const -1.0)))
             (i32.const 0x7fc00000)))
  (func (export "f64.no_fold_sub_zero") (param $$x i64) (result i64)
    (i64.and (i64.reinterpret_f64 (f64.sub (f64.reinterpret_i64 (local.get $$x)) (f64.const 0.0)))
             (i64.const 0x7ff8000000000000)))
  (func (export "f64.no_fold_neg0_sub") (param $$x i64) (result i64)
    (i64.and (i64.reinterpret_f64 (f64.sub (f64.const -0.0) (f64.reinterpret_i64 (local.get $$x))))
             (i64.const 0x7ff8000000000000)))
  (func (export "f64.no_fold_mul_one") (param $$x i64) (result i64)
    (i64.and (i64.reinterpret_f64 (f64.mul (f64.reinterpret_i64 (local.get $$x)) (f64.const 1.0)))
             (i64.const 0x7ff8000000000000)))
  (func (export "f64.no_fold_neg1_mul") (param $$x i64) (result i64)
    (i64.and (i64.reinterpret_f64 (f64.mul (f64.const -1.0) (f64.reinterpret_i64 (local.get $$x))))
             (i64.const 0x7ff8000000000000)))
  (func (export "f64.no_fold_div_one") (param $$x i64) (result i64)
    (i64.and (i64.reinterpret_f64 (f64.div (f64.reinterpret_i64 (local.get $$x)) (f64.const 1.0)))
             (i64.const 0x7ff8000000000000)))
  (func (export "f64.no_fold_div_neg1") (param $$x i64) (result i64)
    (i64.and (i64.reinterpret_f64 (f64.div (f64.reinterpret_i64 (local.get $$x)) (f64.const -1.0)))
             (i64.const 0x7ff8000000000000)))
  (func (export "no_fold_promote_demote") (param $$x i32) (result i32)
    (i32.and (i32.reinterpret_f32 (f32.demote_f64 (f64.promote_f32 (f32.reinterpret_i32 (local.get $$x)))))
             (i32.const 0x7fc00000)))
)`);

// ./test/core/float_exprs.wast:2329
assert_return(
  () => invoke($87, `f32.arithmetic_nan_bitpattern`, [2139107856, 2139107856]),
  [value("i32", 2143289344)],
);

// ./test/core/float_exprs.wast:2330
assert_return(() => invoke($87, `f32.canonical_nan_bitpattern`, [0, 0]), [
  value("i32", 2143289344),
]);

// ./test/core/float_exprs.wast:2331
assert_return(
  () => invoke($87, `f32.canonical_nan_bitpattern`, [2143289344, 2143289344]),
  [value("i32", 2143289344)],
);

// ./test/core/float_exprs.wast:2332
assert_return(
  () => invoke($87, `f32.canonical_nan_bitpattern`, [-4194304, 2143289344]),
  [value("i32", 2143289344)],
);

// ./test/core/float_exprs.wast:2333
assert_return(
  () => invoke($87, `f32.canonical_nan_bitpattern`, [2143289344, -4194304]),
  [value("i32", 2143289344)],
);

// ./test/core/float_exprs.wast:2334
assert_return(
  () => invoke($87, `f32.canonical_nan_bitpattern`, [-4194304, -4194304]),
  [value("i32", 2143289344)],
);

// ./test/core/float_exprs.wast:2335
assert_return(
  () => invoke($87, `f32.nonarithmetic_nan_bitpattern`, [2143302160]),
  [value("i32", -4181488)],
);

// ./test/core/float_exprs.wast:2336
assert_return(
  () => invoke($87, `f32.nonarithmetic_nan_bitpattern`, [-4181488]),
  [value("i32", 2143302160)],
);

// ./test/core/float_exprs.wast:2337
assert_return(
  () => invoke($87, `f32.nonarithmetic_nan_bitpattern`, [2139107856]),
  [value("i32", -8375792)],
);

// ./test/core/float_exprs.wast:2338
assert_return(
  () => invoke($87, `f32.nonarithmetic_nan_bitpattern`, [-8375792]),
  [value("i32", 2139107856)],
);

// ./test/core/float_exprs.wast:2339
assert_return(
  () =>
    invoke($87, `f64.arithmetic_nan_bitpattern`, [
      9218868437227418128n,
      9218868437227418128n,
    ]),
  [value("i64", 9221120237041090560n)],
);

// ./test/core/float_exprs.wast:2340
assert_return(() => invoke($87, `f64.canonical_nan_bitpattern`, [0n, 0n]), [
  value("i64", 9221120237041090560n),
]);

// ./test/core/float_exprs.wast:2341
assert_return(
  () =>
    invoke($87, `f64.canonical_nan_bitpattern`, [
      9221120237041090560n,
      9221120237041090560n,
    ]),
  [value("i64", 9221120237041090560n)],
);

// ./test/core/float_exprs.wast:2342
assert_return(
  () =>
    invoke($87, `f64.canonical_nan_bitpattern`, [
      -2251799813685248n,
      9221120237041090560n,
    ]),
  [value("i64", 9221120237041090560n)],
);

// ./test/core/float_exprs.wast:2343
assert_return(
  () =>
    invoke($87, `f64.canonical_nan_bitpattern`, [
      9221120237041090560n,
      -2251799813685248n,
    ]),
  [value("i64", 9221120237041090560n)],
);

// ./test/core/float_exprs.wast:2344
assert_return(
  () =>
    invoke($87, `f64.canonical_nan_bitpattern`, [
      -2251799813685248n,
      -2251799813685248n,
    ]),
  [value("i64", 9221120237041090560n)],
);

// ./test/core/float_exprs.wast:2345
assert_return(
  () => invoke($87, `f64.nonarithmetic_nan_bitpattern`, [9221120237041103376n]),
  [value("i64", -2251799813672432n)],
);

// ./test/core/float_exprs.wast:2346
assert_return(
  () => invoke($87, `f64.nonarithmetic_nan_bitpattern`, [-2251799813672432n]),
  [value("i64", 9221120237041103376n)],
);

// ./test/core/float_exprs.wast:2347
assert_return(
  () => invoke($87, `f64.nonarithmetic_nan_bitpattern`, [9218868437227418128n]),
  [value("i64", -4503599627357680n)],
);

// ./test/core/float_exprs.wast:2348
assert_return(
  () => invoke($87, `f64.nonarithmetic_nan_bitpattern`, [-4503599627357680n]),
  [value("i64", 9218868437227418128n)],
);

// ./test/core/float_exprs.wast:2349
assert_return(() => invoke($87, `f32.no_fold_sub_zero`, [2141192192]), [
  value("i32", 2143289344),
]);

// ./test/core/float_exprs.wast:2350
assert_return(() => invoke($87, `f32.no_fold_neg0_sub`, [2141192192]), [
  value("i32", 2143289344),
]);

// ./test/core/float_exprs.wast:2351
assert_return(() => invoke($87, `f32.no_fold_mul_one`, [2141192192]), [
  value("i32", 2143289344),
]);

// ./test/core/float_exprs.wast:2352
assert_return(() => invoke($87, `f32.no_fold_neg1_mul`, [2141192192]), [
  value("i32", 2143289344),
]);

// ./test/core/float_exprs.wast:2353
assert_return(() => invoke($87, `f32.no_fold_div_one`, [2141192192]), [
  value("i32", 2143289344),
]);

// ./test/core/float_exprs.wast:2354
assert_return(() => invoke($87, `f32.no_fold_div_neg1`, [2141192192]), [
  value("i32", 2143289344),
]);

// ./test/core/float_exprs.wast:2355
assert_return(
  () => invoke($87, `f64.no_fold_sub_zero`, [9219994337134247936n]),
  [value("i64", 9221120237041090560n)],
);

// ./test/core/float_exprs.wast:2356
assert_return(
  () => invoke($87, `f64.no_fold_neg0_sub`, [9219994337134247936n]),
  [value("i64", 9221120237041090560n)],
);

// ./test/core/float_exprs.wast:2357
assert_return(
  () => invoke($87, `f64.no_fold_mul_one`, [9219994337134247936n]),
  [value("i64", 9221120237041090560n)],
);

// ./test/core/float_exprs.wast:2358
assert_return(
  () => invoke($87, `f64.no_fold_neg1_mul`, [9219994337134247936n]),
  [value("i64", 9221120237041090560n)],
);

// ./test/core/float_exprs.wast:2359
assert_return(
  () => invoke($87, `f64.no_fold_div_one`, [9219994337134247936n]),
  [value("i64", 9221120237041090560n)],
);

// ./test/core/float_exprs.wast:2360
assert_return(
  () => invoke($87, `f64.no_fold_div_neg1`, [9219994337134247936n]),
  [value("i64", 9221120237041090560n)],
);

// ./test/core/float_exprs.wast:2361
assert_return(() => invoke($87, `no_fold_promote_demote`, [2141192192]), [
  value("i32", 2143289344),
]);

// ./test/core/float_exprs.wast:2366
let $88 = instantiate(`(module
  (func (export "dot_product_example")
        (param $$x0 f64) (param $$x1 f64) (param $$x2 f64) (param $$x3 f64)
        (param $$y0 f64) (param $$y1 f64) (param $$y2 f64) (param $$y3 f64)
        (result f64)
    (f64.add (f64.add (f64.add
      (f64.mul (local.get $$x0) (local.get $$y0))
      (f64.mul (local.get $$x1) (local.get $$y1)))
      (f64.mul (local.get $$x2) (local.get $$y2)))
      (f64.mul (local.get $$x3) (local.get $$y3)))
  )

  (func (export "with_binary_sum_collapse")
        (param $$x0 f64) (param $$x1 f64) (param $$x2 f64) (param $$x3 f64)
        (param $$y0 f64) (param $$y1 f64) (param $$y2 f64) (param $$y3 f64)
        (result f64)
      (f64.add (f64.add (f64.mul (local.get $$x0) (local.get $$y0))
                        (f64.mul (local.get $$x1) (local.get $$y1)))
               (f64.add (f64.mul (local.get $$x2) (local.get $$y2))
                        (f64.mul (local.get $$x3) (local.get $$y3))))
  )
)`);

// ./test/core/float_exprs.wast:2389
assert_return(
  () =>
    invoke($88, `dot_product_example`, [
      value("f64", 32000000),
      value("f64", 1),
      value("f64", -1),
      value("f64", 80000000),
      value("f64", 40000000),
      value("f64", 1),
      value("f64", -1),
      value("f64", -16000000),
    ]),
  [value("f64", 2)],
);

// ./test/core/float_exprs.wast:2393
assert_return(
  () =>
    invoke($88, `with_binary_sum_collapse`, [
      value("f64", 32000000),
      value("f64", 1),
      value("f64", -1),
      value("f64", 80000000),
      value("f64", 40000000),
      value("f64", 1),
      value("f64", -1),
      value("f64", -16000000),
    ]),
  [value("f64", 2)],
);

// ./test/core/float_exprs.wast:2400
let $89 = instantiate(`(module
  (func (export "f32.contract2fma")
        (param $$x f32) (param $$y f32) (result f32)
    (f32.sqrt (f32.sub (f32.mul (local.get $$x) (local.get $$x))
                       (f32.mul (local.get $$y) (local.get $$y)))))
  (func (export "f64.contract2fma")
        (param $$x f64) (param $$y f64) (result f64)
    (f64.sqrt (f64.sub (f64.mul (local.get $$x) (local.get $$x))
                       (f64.mul (local.get $$y) (local.get $$y)))))
)`);

// ./test/core/float_exprs.wast:2411
assert_return(
  () => invoke($89, `f32.contract2fma`, [value("f32", 1), value("f32", 1)]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:2412
assert_return(
  () => invoke($89, `f32.contract2fma`, [value("f32", 1.1), value("f32", 1.1)]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:2413
assert_return(
  () =>
    invoke($89, `f32.contract2fma`, [
      value("f32", 1.1999999),
      value("f32", 1.1999999),
    ]),
  [value("f32", 0)],
);

// ./test/core/float_exprs.wast:2414
assert_return(
  () => invoke($89, `f64.contract2fma`, [value("f64", 1), value("f64", 1)]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:2415
assert_return(
  () => invoke($89, `f64.contract2fma`, [value("f64", 1.1), value("f64", 1.1)]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:2416
assert_return(
  () => invoke($89, `f64.contract2fma`, [value("f64", 1.2), value("f64", 1.2)]),
  [value("f64", 0)],
);

// ./test/core/float_exprs.wast:2421
let $90 = instantiate(`(module
  (func (export "f32.division_by_small_number")
        (param $$a f32) (param $$b f32) (param $$c f32) (result f32)
    (f32.sub (local.get $$a) (f32.div (local.get $$b) (local.get $$c))))
  (func (export "f64.division_by_small_number")
        (param $$a f64) (param $$b f64) (param $$c f64) (result f64)
    (f64.sub (local.get $$a) (f64.div (local.get $$b) (local.get $$c))))
)`);

// ./test/core/float_exprs.wast:2430
assert_return(
  () =>
    invoke($90, `f32.division_by_small_number`, [
      value("f32", 112000000),
      value("f32", 100000),
      value("f32", 0.0009),
    ]),
  [value("f32", 888888)],
);

// ./test/core/float_exprs.wast:2431
assert_return(
  () =>
    invoke($90, `f64.division_by_small_number`, [
      value("f64", 112000000),
      value("f64", 100000),
      value("f64", 0.0009),
    ]),
  [value("f64", 888888.8888888806)],
);

// ./test/core/float_exprs.wast:2436
let $91 = instantiate(`(module
  (func (export "f32.golden_ratio") (param $$a f32) (param $$b f32) (param $$c f32) (result f32)
    (f32.mul (local.get 0) (f32.add (local.get 1) (f32.sqrt (local.get 2)))))
  (func (export "f64.golden_ratio") (param $$a f64) (param $$b f64) (param $$c f64) (result f64)
    (f64.mul (local.get 0) (f64.add (local.get 1) (f64.sqrt (local.get 2)))))
)`);

// ./test/core/float_exprs.wast:2443
assert_return(
  () =>
    invoke($91, `f32.golden_ratio`, [
      value("f32", 0.5),
      value("f32", 1),
      value("f32", 5),
    ]),
  [value("f32", 1.618034)],
);

// ./test/core/float_exprs.wast:2444
assert_return(
  () =>
    invoke($91, `f64.golden_ratio`, [
      value("f64", 0.5),
      value("f64", 1),
      value("f64", 5),
    ]),
  [value("f64", 1.618033988749895)],
);

// ./test/core/float_exprs.wast:2449
let $92 = instantiate(`(module
  (func (export "f32.silver_means") (param $$n f32) (result f32)
    (f32.mul (f32.const 0.5)
             (f32.add (local.get $$n)
                      (f32.sqrt (f32.add (f32.mul (local.get $$n) (local.get $$n))
                                         (f32.const 4.0))))))
  (func (export "f64.silver_means") (param $$n f64) (result f64)
    (f64.mul (f64.const 0.5)
             (f64.add (local.get $$n)
                      (f64.sqrt (f64.add (f64.mul (local.get $$n) (local.get $$n))
                                         (f64.const 4.0))))))
)`);

// ./test/core/float_exprs.wast:2462
assert_return(() => invoke($92, `f32.silver_means`, [value("f32", 0)]), [
  value("f32", 1),
]);

// ./test/core/float_exprs.wast:2463
assert_return(() => invoke($92, `f32.silver_means`, [value("f32", 1)]), [
  value("f32", 1.618034),
]);

// ./test/core/float_exprs.wast:2464
assert_return(() => invoke($92, `f32.silver_means`, [value("f32", 2)]), [
  value("f32", 2.4142137),
]);

// ./test/core/float_exprs.wast:2465
assert_return(() => invoke($92, `f32.silver_means`, [value("f32", 3)]), [
  value("f32", 3.3027756),
]);

// ./test/core/float_exprs.wast:2466
assert_return(() => invoke($92, `f32.silver_means`, [value("f32", 4)]), [
  value("f32", 4.236068),
]);

// ./test/core/float_exprs.wast:2467
assert_return(() => invoke($92, `f32.silver_means`, [value("f32", 5)]), [
  value("f32", 5.192582),
]);

// ./test/core/float_exprs.wast:2468
assert_return(() => invoke($92, `f64.silver_means`, [value("f64", 0)]), [
  value("f64", 1),
]);

// ./test/core/float_exprs.wast:2469
assert_return(() => invoke($92, `f64.silver_means`, [value("f64", 1)]), [
  value("f64", 1.618033988749895),
]);

// ./test/core/float_exprs.wast:2470
assert_return(() => invoke($92, `f64.silver_means`, [value("f64", 2)]), [
  value("f64", 2.414213562373095),
]);

// ./test/core/float_exprs.wast:2471
assert_return(() => invoke($92, `f64.silver_means`, [value("f64", 3)]), [
  value("f64", 3.302775637731995),
]);

// ./test/core/float_exprs.wast:2472
assert_return(() => invoke($92, `f64.silver_means`, [value("f64", 4)]), [
  value("f64", 4.23606797749979),
]);

// ./test/core/float_exprs.wast:2473
assert_return(() => invoke($92, `f64.silver_means`, [value("f64", 5)]), [
  value("f64", 5.192582403567252),
]);

// ./test/core/float_exprs.wast:2478
let $93 = instantiate(`(module
  (func (export "point_four") (param $$four f64) (param $$ten f64) (result i32)
    (f64.lt (f64.div (local.get $$four) (local.get $$ten)) (f64.const 0.4)))
)`);

// ./test/core/float_exprs.wast:2483
assert_return(
  () => invoke($93, `point_four`, [value("f64", 4), value("f64", 10)]),
  [value("i32", 0)],
);

// ./test/core/float_exprs.wast:2488
let $94 = instantiate(`(module
  (func (export "tau") (param i32) (result f64)
    (local f64 f64 f64 f64)
    f64.const 0x0p+0
    local.set 1
    block
      local.get 0
      i32.const 1
      i32.lt_s
      br_if 0
      f64.const 0x1p+0
      local.set 2
      f64.const 0x0p+0
      local.set 3
      loop
        local.get 1
        local.get 2
        f64.const 0x1p+3
        local.get 3
        f64.const 0x1p+3
        f64.mul
        local.tee 4
        f64.const 0x1p+0
        f64.add
        f64.div
        f64.const 0x1p+2
        local.get 4
        f64.const 0x1p+2
        f64.add
        f64.div
        f64.sub
        f64.const 0x1p+1
        local.get 4
        f64.const 0x1.4p+2
        f64.add
        f64.div
        f64.sub
        f64.const 0x1p+1
        local.get 4
        f64.const 0x1.8p+2
        f64.add
        f64.div
        f64.sub
        f64.mul
        f64.add
        local.set 1
        local.get 3
        f64.const 0x1p+0
        f64.add
        local.set 3
        local.get 2
        f64.const 0x1p-4
        f64.mul
        local.set 2
        local.get 0
        i32.const -1
        i32.add
        local.tee 0
        br_if 0
      end
    end
    local.get 1
  )
)`);

// ./test/core/float_exprs.wast:2553
assert_return(() => invoke($94, `tau`, [10]), [
  value("f64", 6.283185307179583),
]);

// ./test/core/float_exprs.wast:2554
assert_return(() => invoke($94, `tau`, [11]), [
  value("f64", 6.283185307179586),
]);

// ./test/core/float_exprs.wast:2558
let $95 = instantiate(`(module
  (func (export "f32.no_fold_conditional_inc") (param $$x f32) (param $$y f32) (result f32)
    (select (local.get $$x)
            (f32.add (local.get $$x) (f32.const 1.0))
            (f32.lt (local.get $$y) (f32.const 0.0))))
  (func (export "f64.no_fold_conditional_inc") (param $$x f64) (param $$y f64) (result f64)
    (select (local.get $$x)
            (f64.add (local.get $$x) (f64.const 1.0))
            (f64.lt (local.get $$y) (f64.const 0.0))))
)`);

// ./test/core/float_exprs.wast:2569
assert_return(
  () =>
    invoke($95, `f32.no_fold_conditional_inc`, [
      value("f32", -0),
      value("f32", -1),
    ]),
  [value("f32", -0)],
);

// ./test/core/float_exprs.wast:2570
assert_return(
  () =>
    invoke($95, `f64.no_fold_conditional_inc`, [
      value("f64", -0),
      value("f64", -1),
    ]),
  [value("f64", -0)],
);
