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

// ./test/core/f32.wast

// ./test/core/f32.wast:5
let $0 = instantiate(`(module
  (func (export "add") (param $$x f32) (param $$y f32) (result f32) (f32.add (local.get $$x) (local.get $$y)))
  (func (export "sub") (param $$x f32) (param $$y f32) (result f32) (f32.sub (local.get $$x) (local.get $$y)))
  (func (export "mul") (param $$x f32) (param $$y f32) (result f32) (f32.mul (local.get $$x) (local.get $$y)))
  (func (export "div") (param $$x f32) (param $$y f32) (result f32) (f32.div (local.get $$x) (local.get $$y)))
  (func (export "sqrt") (param $$x f32) (result f32) (f32.sqrt (local.get $$x)))
  (func (export "min") (param $$x f32) (param $$y f32) (result f32) (f32.min (local.get $$x) (local.get $$y)))
  (func (export "max") (param $$x f32) (param $$y f32) (result f32) (f32.max (local.get $$x) (local.get $$y)))
  (func (export "ceil") (param $$x f32) (result f32) (f32.ceil (local.get $$x)))
  (func (export "floor") (param $$x f32) (result f32) (f32.floor (local.get $$x)))
  (func (export "trunc") (param $$x f32) (result f32) (f32.trunc (local.get $$x)))
  (func (export "nearest") (param $$x f32) (result f32) (f32.nearest (local.get $$x)))
)`);

// ./test/core/f32.wast:19
assert_return(() => invoke($0, `add`, [value("f32", -0), value("f32", -0)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:20
assert_return(() => invoke($0, `add`, [value("f32", -0), value("f32", 0)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:21
assert_return(() => invoke($0, `add`, [value("f32", 0), value("f32", -0)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:22
assert_return(() => invoke($0, `add`, [value("f32", 0), value("f32", 0)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:23
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:24
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:25
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:26
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:27
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:28
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:29
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:30
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:31
assert_return(() => invoke($0, `add`, [value("f32", -0), value("f32", -0.5)]), [
  value("f32", -0.5),
]);

// ./test/core/f32.wast:32
assert_return(() => invoke($0, `add`, [value("f32", -0), value("f32", 0.5)]), [
  value("f32", 0.5),
]);

// ./test/core/f32.wast:33
assert_return(() => invoke($0, `add`, [value("f32", 0), value("f32", -0.5)]), [
  value("f32", -0.5),
]);

// ./test/core/f32.wast:34
assert_return(() => invoke($0, `add`, [value("f32", 0), value("f32", 0.5)]), [
  value("f32", 0.5),
]);

// ./test/core/f32.wast:35
assert_return(() => invoke($0, `add`, [value("f32", -0), value("f32", -1)]), [
  value("f32", -1),
]);

// ./test/core/f32.wast:36
assert_return(() => invoke($0, `add`, [value("f32", -0), value("f32", 1)]), [
  value("f32", 1),
]);

// ./test/core/f32.wast:37
assert_return(() => invoke($0, `add`, [value("f32", 0), value("f32", -1)]), [
  value("f32", -1),
]);

// ./test/core/f32.wast:38
assert_return(() => invoke($0, `add`, [value("f32", 0), value("f32", 1)]), [
  value("f32", 1),
]);

// ./test/core/f32.wast:39
assert_return(
  () => invoke($0, `add`, [value("f32", -0), value("f32", -6.2831855)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:40
assert_return(
  () => invoke($0, `add`, [value("f32", -0), value("f32", 6.2831855)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:41
assert_return(
  () => invoke($0, `add`, [value("f32", 0), value("f32", -6.2831855)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:42
assert_return(
  () => invoke($0, `add`, [value("f32", 0), value("f32", 6.2831855)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:43
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:44
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:45
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:46
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:47
assert_return(
  () => invoke($0, `add`, [value("f32", -0), value("f32", -Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:48
assert_return(
  () => invoke($0, `add`, [value("f32", -0), value("f32", Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:49
assert_return(
  () => invoke($0, `add`, [value("f32", 0), value("f32", -Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:50
assert_return(
  () => invoke($0, `add`, [value("f32", 0), value("f32", Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:51
assert_return(
  () =>
    invoke($0, `add`, [value("f32", -0), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:52
assert_return(
  () =>
    invoke($0, `add`, [value("f32", -0), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:53
assert_return(
  () =>
    invoke($0, `add`, [value("f32", -0), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:54
assert_return(
  () =>
    invoke($0, `add`, [value("f32", -0), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:55
assert_return(
  () =>
    invoke($0, `add`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:56
assert_return(
  () =>
    invoke($0, `add`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:57
assert_return(
  () =>
    invoke($0, `add`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:58
assert_return(
  () =>
    invoke($0, `add`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:59
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:60
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:61
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:62
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:63
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000003)],
);

// ./test/core/f32.wast:64
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:65
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:66
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000003)],
);

// ./test/core/f32.wast:67
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754945)],
);

// ./test/core/f32.wast:68
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754942)],
);

// ./test/core/f32.wast:69
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754942)],
);

// ./test/core/f32.wast:70
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754945)],
);

// ./test/core/f32.wast:71
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.5),
    ]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:72
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.5),
    ]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:73
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.5),
    ]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:74
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.5),
    ]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:75
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -1),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:76
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 1),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:77
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -1),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:78
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 1),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:79
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -6.2831855),
    ]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:80
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 6.2831855),
    ]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:81
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -6.2831855),
    ]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:82
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 6.2831855),
    ]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:83
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:84
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:85
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:86
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:87
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -Infinity),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:88
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", Infinity),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:89
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -Infinity),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:90
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", Infinity),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:91
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:92
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:93
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:94
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:95
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:96
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:97
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:98
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:99
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:100
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:101
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:102
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:103
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754945)],
);

// ./test/core/f32.wast:104
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754942)],
);

// ./test/core/f32.wast:105
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754942)],
);

// ./test/core/f32.wast:106
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754945)],
);

// ./test/core/f32.wast:107
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000023509887)],
);

// ./test/core/f32.wast:108
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:109
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:110
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000023509887)],
);

// ./test/core/f32.wast:111
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.5),
    ]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:112
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.5),
    ]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:113
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.5),
    ]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:114
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.5),
    ]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:115
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -1),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:116
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 1),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:117
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -1),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:118
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 1),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:119
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -6.2831855),
    ]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:120
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 6.2831855),
    ]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:121
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -6.2831855),
    ]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:122
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 6.2831855),
    ]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:123
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:124
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:125
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:126
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:127
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -Infinity),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:128
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", Infinity),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:129
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -Infinity),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:130
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", Infinity),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:131
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:132
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:133
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:134
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:135
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:136
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:137
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:138
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:139
assert_return(() => invoke($0, `add`, [value("f32", -0.5), value("f32", -0)]), [
  value("f32", -0.5),
]);

// ./test/core/f32.wast:140
assert_return(() => invoke($0, `add`, [value("f32", -0.5), value("f32", 0)]), [
  value("f32", -0.5),
]);

// ./test/core/f32.wast:141
assert_return(() => invoke($0, `add`, [value("f32", 0.5), value("f32", -0)]), [
  value("f32", 0.5),
]);

// ./test/core/f32.wast:142
assert_return(() => invoke($0, `add`, [value("f32", 0.5), value("f32", 0)]), [
  value("f32", 0.5),
]);

// ./test/core/f32.wast:143
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.5),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:144
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.5),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:145
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.5),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:146
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.5),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:147
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.5),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:148
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.5),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:149
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.5),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:150
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.5),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:151
assert_return(
  () => invoke($0, `add`, [value("f32", -0.5), value("f32", -0.5)]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:152
assert_return(
  () => invoke($0, `add`, [value("f32", -0.5), value("f32", 0.5)]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:153
assert_return(
  () => invoke($0, `add`, [value("f32", 0.5), value("f32", -0.5)]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:154
assert_return(() => invoke($0, `add`, [value("f32", 0.5), value("f32", 0.5)]), [
  value("f32", 1),
]);

// ./test/core/f32.wast:155
assert_return(() => invoke($0, `add`, [value("f32", -0.5), value("f32", -1)]), [
  value("f32", -1.5),
]);

// ./test/core/f32.wast:156
assert_return(() => invoke($0, `add`, [value("f32", -0.5), value("f32", 1)]), [
  value("f32", 0.5),
]);

// ./test/core/f32.wast:157
assert_return(() => invoke($0, `add`, [value("f32", 0.5), value("f32", -1)]), [
  value("f32", -0.5),
]);

// ./test/core/f32.wast:158
assert_return(() => invoke($0, `add`, [value("f32", 0.5), value("f32", 1)]), [
  value("f32", 1.5),
]);

// ./test/core/f32.wast:159
assert_return(
  () => invoke($0, `add`, [value("f32", -0.5), value("f32", -6.2831855)]),
  [value("f32", -6.7831855)],
);

// ./test/core/f32.wast:160
assert_return(
  () => invoke($0, `add`, [value("f32", -0.5), value("f32", 6.2831855)]),
  [value("f32", 5.7831855)],
);

// ./test/core/f32.wast:161
assert_return(
  () => invoke($0, `add`, [value("f32", 0.5), value("f32", -6.2831855)]),
  [value("f32", -5.7831855)],
);

// ./test/core/f32.wast:162
assert_return(
  () => invoke($0, `add`, [value("f32", 0.5), value("f32", 6.2831855)]),
  [value("f32", 6.7831855)],
);

// ./test/core/f32.wast:163
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.5),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:164
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.5),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:165
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.5),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:166
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.5),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:167
assert_return(
  () => invoke($0, `add`, [value("f32", -0.5), value("f32", -Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:168
assert_return(
  () => invoke($0, `add`, [value("f32", -0.5), value("f32", Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:169
assert_return(
  () => invoke($0, `add`, [value("f32", 0.5), value("f32", -Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:170
assert_return(
  () => invoke($0, `add`, [value("f32", 0.5), value("f32", Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:171
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:172
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:173
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:174
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:175
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:176
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:177
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:178
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:179
assert_return(() => invoke($0, `add`, [value("f32", -1), value("f32", -0)]), [
  value("f32", -1),
]);

// ./test/core/f32.wast:180
assert_return(() => invoke($0, `add`, [value("f32", -1), value("f32", 0)]), [
  value("f32", -1),
]);

// ./test/core/f32.wast:181
assert_return(() => invoke($0, `add`, [value("f32", 1), value("f32", -0)]), [
  value("f32", 1),
]);

// ./test/core/f32.wast:182
assert_return(() => invoke($0, `add`, [value("f32", 1), value("f32", 0)]), [
  value("f32", 1),
]);

// ./test/core/f32.wast:183
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -1),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:184
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -1),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:185
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 1),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:186
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 1),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:187
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -1),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:188
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -1),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:189
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 1),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:190
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 1),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:191
assert_return(() => invoke($0, `add`, [value("f32", -1), value("f32", -0.5)]), [
  value("f32", -1.5),
]);

// ./test/core/f32.wast:192
assert_return(() => invoke($0, `add`, [value("f32", -1), value("f32", 0.5)]), [
  value("f32", -0.5),
]);

// ./test/core/f32.wast:193
assert_return(() => invoke($0, `add`, [value("f32", 1), value("f32", -0.5)]), [
  value("f32", 0.5),
]);

// ./test/core/f32.wast:194
assert_return(() => invoke($0, `add`, [value("f32", 1), value("f32", 0.5)]), [
  value("f32", 1.5),
]);

// ./test/core/f32.wast:195
assert_return(() => invoke($0, `add`, [value("f32", -1), value("f32", -1)]), [
  value("f32", -2),
]);

// ./test/core/f32.wast:196
assert_return(() => invoke($0, `add`, [value("f32", -1), value("f32", 1)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:197
assert_return(() => invoke($0, `add`, [value("f32", 1), value("f32", -1)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:198
assert_return(() => invoke($0, `add`, [value("f32", 1), value("f32", 1)]), [
  value("f32", 2),
]);

// ./test/core/f32.wast:199
assert_return(
  () => invoke($0, `add`, [value("f32", -1), value("f32", -6.2831855)]),
  [value("f32", -7.2831855)],
);

// ./test/core/f32.wast:200
assert_return(
  () => invoke($0, `add`, [value("f32", -1), value("f32", 6.2831855)]),
  [value("f32", 5.2831855)],
);

// ./test/core/f32.wast:201
assert_return(
  () => invoke($0, `add`, [value("f32", 1), value("f32", -6.2831855)]),
  [value("f32", -5.2831855)],
);

// ./test/core/f32.wast:202
assert_return(
  () => invoke($0, `add`, [value("f32", 1), value("f32", 6.2831855)]),
  [value("f32", 7.2831855)],
);

// ./test/core/f32.wast:203
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -1),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:204
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -1),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:205
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 1),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:206
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 1),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:207
assert_return(
  () => invoke($0, `add`, [value("f32", -1), value("f32", -Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:208
assert_return(
  () => invoke($0, `add`, [value("f32", -1), value("f32", Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:209
assert_return(
  () => invoke($0, `add`, [value("f32", 1), value("f32", -Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:210
assert_return(
  () => invoke($0, `add`, [value("f32", 1), value("f32", Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:211
assert_return(
  () =>
    invoke($0, `add`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:212
assert_return(
  () =>
    invoke($0, `add`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:213
assert_return(
  () =>
    invoke($0, `add`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:214
assert_return(
  () =>
    invoke($0, `add`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:215
assert_return(
  () =>
    invoke($0, `add`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:216
assert_return(
  () =>
    invoke($0, `add`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:217
assert_return(
  () =>
    invoke($0, `add`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:218
assert_return(
  () =>
    invoke($0, `add`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:219
assert_return(
  () => invoke($0, `add`, [value("f32", -6.2831855), value("f32", -0)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:220
assert_return(
  () => invoke($0, `add`, [value("f32", -6.2831855), value("f32", 0)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:221
assert_return(
  () => invoke($0, `add`, [value("f32", 6.2831855), value("f32", -0)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:222
assert_return(
  () => invoke($0, `add`, [value("f32", 6.2831855), value("f32", 0)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:223
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -6.2831855),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:224
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -6.2831855),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:225
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 6.2831855),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:226
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 6.2831855),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:227
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -6.2831855),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:228
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -6.2831855),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:229
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 6.2831855),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:230
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 6.2831855),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:231
assert_return(
  () => invoke($0, `add`, [value("f32", -6.2831855), value("f32", -0.5)]),
  [value("f32", -6.7831855)],
);

// ./test/core/f32.wast:232
assert_return(
  () => invoke($0, `add`, [value("f32", -6.2831855), value("f32", 0.5)]),
  [value("f32", -5.7831855)],
);

// ./test/core/f32.wast:233
assert_return(
  () => invoke($0, `add`, [value("f32", 6.2831855), value("f32", -0.5)]),
  [value("f32", 5.7831855)],
);

// ./test/core/f32.wast:234
assert_return(
  () => invoke($0, `add`, [value("f32", 6.2831855), value("f32", 0.5)]),
  [value("f32", 6.7831855)],
);

// ./test/core/f32.wast:235
assert_return(
  () => invoke($0, `add`, [value("f32", -6.2831855), value("f32", -1)]),
  [value("f32", -7.2831855)],
);

// ./test/core/f32.wast:236
assert_return(
  () => invoke($0, `add`, [value("f32", -6.2831855), value("f32", 1)]),
  [value("f32", -5.2831855)],
);

// ./test/core/f32.wast:237
assert_return(
  () => invoke($0, `add`, [value("f32", 6.2831855), value("f32", -1)]),
  [value("f32", 5.2831855)],
);

// ./test/core/f32.wast:238
assert_return(
  () => invoke($0, `add`, [value("f32", 6.2831855), value("f32", 1)]),
  [value("f32", 7.2831855)],
);

// ./test/core/f32.wast:239
assert_return(
  () => invoke($0, `add`, [value("f32", -6.2831855), value("f32", -6.2831855)]),
  [value("f32", -12.566371)],
);

// ./test/core/f32.wast:240
assert_return(
  () => invoke($0, `add`, [value("f32", -6.2831855), value("f32", 6.2831855)]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:241
assert_return(
  () => invoke($0, `add`, [value("f32", 6.2831855), value("f32", -6.2831855)]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:242
assert_return(
  () => invoke($0, `add`, [value("f32", 6.2831855), value("f32", 6.2831855)]),
  [value("f32", 12.566371)],
);

// ./test/core/f32.wast:243
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -6.2831855),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:244
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -6.2831855),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:245
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 6.2831855),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:246
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 6.2831855),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:247
assert_return(
  () => invoke($0, `add`, [value("f32", -6.2831855), value("f32", -Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:248
assert_return(
  () => invoke($0, `add`, [value("f32", -6.2831855), value("f32", Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:249
assert_return(
  () => invoke($0, `add`, [value("f32", 6.2831855), value("f32", -Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:250
assert_return(
  () => invoke($0, `add`, [value("f32", 6.2831855), value("f32", Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:251
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:252
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:253
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:254
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:255
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:256
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:257
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:258
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:259
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:260
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:261
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:262
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:263
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:264
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:265
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:266
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:267
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:268
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:269
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:270
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:271
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.5),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:272
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.5),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:273
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.5),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:274
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.5),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:275
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -1),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:276
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 1),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:277
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -1),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:278
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 1),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:279
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -6.2831855),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:280
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 6.2831855),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:281
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -6.2831855),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:282
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 6.2831855),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:283
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:284
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:285
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:286
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:287
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -Infinity),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:288
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", Infinity),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:289
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -Infinity),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:290
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", Infinity),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:291
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:292
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:293
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:294
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:295
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:296
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:297
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:298
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:299
assert_return(
  () => invoke($0, `add`, [value("f32", -Infinity), value("f32", -0)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:300
assert_return(
  () => invoke($0, `add`, [value("f32", -Infinity), value("f32", 0)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:301
assert_return(
  () => invoke($0, `add`, [value("f32", Infinity), value("f32", -0)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:302
assert_return(
  () => invoke($0, `add`, [value("f32", Infinity), value("f32", 0)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:303
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -Infinity),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:304
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -Infinity),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:305
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", Infinity),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:306
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", Infinity),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:307
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -Infinity),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:308
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -Infinity),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:309
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", Infinity),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:310
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", Infinity),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:311
assert_return(
  () => invoke($0, `add`, [value("f32", -Infinity), value("f32", -0.5)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:312
assert_return(
  () => invoke($0, `add`, [value("f32", -Infinity), value("f32", 0.5)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:313
assert_return(
  () => invoke($0, `add`, [value("f32", Infinity), value("f32", -0.5)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:314
assert_return(
  () => invoke($0, `add`, [value("f32", Infinity), value("f32", 0.5)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:315
assert_return(
  () => invoke($0, `add`, [value("f32", -Infinity), value("f32", -1)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:316
assert_return(
  () => invoke($0, `add`, [value("f32", -Infinity), value("f32", 1)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:317
assert_return(
  () => invoke($0, `add`, [value("f32", Infinity), value("f32", -1)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:318
assert_return(
  () => invoke($0, `add`, [value("f32", Infinity), value("f32", 1)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:319
assert_return(
  () => invoke($0, `add`, [value("f32", -Infinity), value("f32", -6.2831855)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:320
assert_return(
  () => invoke($0, `add`, [value("f32", -Infinity), value("f32", 6.2831855)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:321
assert_return(
  () => invoke($0, `add`, [value("f32", Infinity), value("f32", -6.2831855)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:322
assert_return(
  () => invoke($0, `add`, [value("f32", Infinity), value("f32", 6.2831855)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:323
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -Infinity),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:324
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -Infinity),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:325
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", Infinity),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:326
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", Infinity),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:327
assert_return(
  () => invoke($0, `add`, [value("f32", -Infinity), value("f32", -Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:328
assert_return(
  () => invoke($0, `add`, [value("f32", -Infinity), value("f32", Infinity)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:329
assert_return(
  () => invoke($0, `add`, [value("f32", Infinity), value("f32", -Infinity)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:330
assert_return(
  () => invoke($0, `add`, [value("f32", Infinity), value("f32", Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:331
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:332
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:333
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:334
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:335
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:336
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:337
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:338
assert_return(
  () =>
    invoke($0, `add`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:339
assert_return(
  () =>
    invoke($0, `add`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", -0)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:340
assert_return(
  () =>
    invoke($0, `add`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", -0)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:341
assert_return(
  () =>
    invoke($0, `add`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 0)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:342
assert_return(
  () =>
    invoke($0, `add`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 0)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:343
assert_return(
  () =>
    invoke($0, `add`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", -0)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:344
assert_return(
  () =>
    invoke($0, `add`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", -0)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:345
assert_return(
  () =>
    invoke($0, `add`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 0)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:346
assert_return(
  () =>
    invoke($0, `add`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 0)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:347
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:348
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:349
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:350
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:351
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:352
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:353
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:354
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:355
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:356
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:357
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:358
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:359
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:360
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:361
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:362
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:363
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.5),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:364
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.5),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:365
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.5),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:366
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.5),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:367
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.5),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:368
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.5),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:369
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.5),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:370
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.5),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:371
assert_return(
  () =>
    invoke($0, `add`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", -1)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:372
assert_return(
  () =>
    invoke($0, `add`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", -1)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:373
assert_return(
  () =>
    invoke($0, `add`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 1)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:374
assert_return(
  () =>
    invoke($0, `add`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 1)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:375
assert_return(
  () =>
    invoke($0, `add`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", -1)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:376
assert_return(
  () =>
    invoke($0, `add`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", -1)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:377
assert_return(
  () =>
    invoke($0, `add`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 1)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:378
assert_return(
  () =>
    invoke($0, `add`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 1)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:379
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -6.2831855),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:380
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -6.2831855),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:381
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 6.2831855),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:382
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 6.2831855),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:383
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -6.2831855),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:384
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -6.2831855),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:385
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 6.2831855),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:386
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 6.2831855),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:387
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:388
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:389
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:390
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:391
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:392
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:393
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:394
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:395
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -Infinity),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:396
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -Infinity),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:397
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", Infinity),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:398
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", Infinity),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:399
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -Infinity),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:400
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -Infinity),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:401
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", Infinity),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:402
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", Infinity),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:403
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:404
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:405
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:406
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:407
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:408
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:409
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:410
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:411
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:412
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:413
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:414
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:415
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:416
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:417
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:418
assert_return(
  () =>
    invoke($0, `add`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:419
assert_return(() => invoke($0, `sub`, [value("f32", -0), value("f32", -0)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:420
assert_return(() => invoke($0, `sub`, [value("f32", -0), value("f32", 0)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:421
assert_return(() => invoke($0, `sub`, [value("f32", 0), value("f32", -0)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:422
assert_return(() => invoke($0, `sub`, [value("f32", 0), value("f32", 0)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:423
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:424
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:425
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:426
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:427
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:428
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:429
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:430
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:431
assert_return(() => invoke($0, `sub`, [value("f32", -0), value("f32", -0.5)]), [
  value("f32", 0.5),
]);

// ./test/core/f32.wast:432
assert_return(() => invoke($0, `sub`, [value("f32", -0), value("f32", 0.5)]), [
  value("f32", -0.5),
]);

// ./test/core/f32.wast:433
assert_return(() => invoke($0, `sub`, [value("f32", 0), value("f32", -0.5)]), [
  value("f32", 0.5),
]);

// ./test/core/f32.wast:434
assert_return(() => invoke($0, `sub`, [value("f32", 0), value("f32", 0.5)]), [
  value("f32", -0.5),
]);

// ./test/core/f32.wast:435
assert_return(() => invoke($0, `sub`, [value("f32", -0), value("f32", -1)]), [
  value("f32", 1),
]);

// ./test/core/f32.wast:436
assert_return(() => invoke($0, `sub`, [value("f32", -0), value("f32", 1)]), [
  value("f32", -1),
]);

// ./test/core/f32.wast:437
assert_return(() => invoke($0, `sub`, [value("f32", 0), value("f32", -1)]), [
  value("f32", 1),
]);

// ./test/core/f32.wast:438
assert_return(() => invoke($0, `sub`, [value("f32", 0), value("f32", 1)]), [
  value("f32", -1),
]);

// ./test/core/f32.wast:439
assert_return(
  () => invoke($0, `sub`, [value("f32", -0), value("f32", -6.2831855)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:440
assert_return(
  () => invoke($0, `sub`, [value("f32", -0), value("f32", 6.2831855)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:441
assert_return(
  () => invoke($0, `sub`, [value("f32", 0), value("f32", -6.2831855)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:442
assert_return(
  () => invoke($0, `sub`, [value("f32", 0), value("f32", 6.2831855)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:443
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:444
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:445
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:446
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:447
assert_return(
  () => invoke($0, `sub`, [value("f32", -0), value("f32", -Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:448
assert_return(
  () => invoke($0, `sub`, [value("f32", -0), value("f32", Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:449
assert_return(
  () => invoke($0, `sub`, [value("f32", 0), value("f32", -Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:450
assert_return(
  () => invoke($0, `sub`, [value("f32", 0), value("f32", Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:451
assert_return(
  () =>
    invoke($0, `sub`, [value("f32", -0), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:452
assert_return(
  () =>
    invoke($0, `sub`, [value("f32", -0), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:453
assert_return(
  () =>
    invoke($0, `sub`, [value("f32", -0), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:454
assert_return(
  () =>
    invoke($0, `sub`, [value("f32", -0), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:455
assert_return(
  () =>
    invoke($0, `sub`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:456
assert_return(
  () =>
    invoke($0, `sub`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:457
assert_return(
  () =>
    invoke($0, `sub`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:458
assert_return(
  () =>
    invoke($0, `sub`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:459
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:460
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:461
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:462
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:463
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:464
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000003)],
);

// ./test/core/f32.wast:465
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000003)],
);

// ./test/core/f32.wast:466
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:467
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754942)],
);

// ./test/core/f32.wast:468
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754945)],
);

// ./test/core/f32.wast:469
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754945)],
);

// ./test/core/f32.wast:470
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754942)],
);

// ./test/core/f32.wast:471
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.5),
    ]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:472
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.5),
    ]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:473
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.5),
    ]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:474
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.5),
    ]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:475
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -1),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:476
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 1),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:477
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -1),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:478
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 1),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:479
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -6.2831855),
    ]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:480
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 6.2831855),
    ]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:481
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -6.2831855),
    ]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:482
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 6.2831855),
    ]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:483
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:484
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:485
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:486
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:487
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -Infinity),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:488
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", Infinity),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:489
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -Infinity),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:490
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", Infinity),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:491
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:492
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:493
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:494
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:495
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:496
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:497
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:498
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:499
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:500
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:501
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:502
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:503
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754942)],
);

// ./test/core/f32.wast:504
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754945)],
);

// ./test/core/f32.wast:505
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754945)],
);

// ./test/core/f32.wast:506
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754942)],
);

// ./test/core/f32.wast:507
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:508
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000023509887)],
);

// ./test/core/f32.wast:509
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000023509887)],
);

// ./test/core/f32.wast:510
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:511
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.5),
    ]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:512
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.5),
    ]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:513
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.5),
    ]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:514
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.5),
    ]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:515
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -1),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:516
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 1),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:517
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -1),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:518
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 1),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:519
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -6.2831855),
    ]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:520
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 6.2831855),
    ]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:521
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -6.2831855),
    ]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:522
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 6.2831855),
    ]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:523
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:524
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:525
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:526
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:527
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -Infinity),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:528
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", Infinity),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:529
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -Infinity),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:530
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", Infinity),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:531
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:532
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:533
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:534
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:535
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:536
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:537
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:538
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:539
assert_return(() => invoke($0, `sub`, [value("f32", -0.5), value("f32", -0)]), [
  value("f32", -0.5),
]);

// ./test/core/f32.wast:540
assert_return(() => invoke($0, `sub`, [value("f32", -0.5), value("f32", 0)]), [
  value("f32", -0.5),
]);

// ./test/core/f32.wast:541
assert_return(() => invoke($0, `sub`, [value("f32", 0.5), value("f32", -0)]), [
  value("f32", 0.5),
]);

// ./test/core/f32.wast:542
assert_return(() => invoke($0, `sub`, [value("f32", 0.5), value("f32", 0)]), [
  value("f32", 0.5),
]);

// ./test/core/f32.wast:543
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.5),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:544
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.5),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:545
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.5),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:546
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.5),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:547
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.5),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:548
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.5),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:549
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.5),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:550
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.5),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:551
assert_return(
  () => invoke($0, `sub`, [value("f32", -0.5), value("f32", -0.5)]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:552
assert_return(
  () => invoke($0, `sub`, [value("f32", -0.5), value("f32", 0.5)]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:553
assert_return(
  () => invoke($0, `sub`, [value("f32", 0.5), value("f32", -0.5)]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:554
assert_return(() => invoke($0, `sub`, [value("f32", 0.5), value("f32", 0.5)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:555
assert_return(() => invoke($0, `sub`, [value("f32", -0.5), value("f32", -1)]), [
  value("f32", 0.5),
]);

// ./test/core/f32.wast:556
assert_return(() => invoke($0, `sub`, [value("f32", -0.5), value("f32", 1)]), [
  value("f32", -1.5),
]);

// ./test/core/f32.wast:557
assert_return(() => invoke($0, `sub`, [value("f32", 0.5), value("f32", -1)]), [
  value("f32", 1.5),
]);

// ./test/core/f32.wast:558
assert_return(() => invoke($0, `sub`, [value("f32", 0.5), value("f32", 1)]), [
  value("f32", -0.5),
]);

// ./test/core/f32.wast:559
assert_return(
  () => invoke($0, `sub`, [value("f32", -0.5), value("f32", -6.2831855)]),
  [value("f32", 5.7831855)],
);

// ./test/core/f32.wast:560
assert_return(
  () => invoke($0, `sub`, [value("f32", -0.5), value("f32", 6.2831855)]),
  [value("f32", -6.7831855)],
);

// ./test/core/f32.wast:561
assert_return(
  () => invoke($0, `sub`, [value("f32", 0.5), value("f32", -6.2831855)]),
  [value("f32", 6.7831855)],
);

// ./test/core/f32.wast:562
assert_return(
  () => invoke($0, `sub`, [value("f32", 0.5), value("f32", 6.2831855)]),
  [value("f32", -5.7831855)],
);

// ./test/core/f32.wast:563
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.5),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:564
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.5),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:565
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.5),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:566
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.5),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:567
assert_return(
  () => invoke($0, `sub`, [value("f32", -0.5), value("f32", -Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:568
assert_return(
  () => invoke($0, `sub`, [value("f32", -0.5), value("f32", Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:569
assert_return(
  () => invoke($0, `sub`, [value("f32", 0.5), value("f32", -Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:570
assert_return(
  () => invoke($0, `sub`, [value("f32", 0.5), value("f32", Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:571
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:572
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:573
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:574
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:575
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:576
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:577
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:578
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:579
assert_return(() => invoke($0, `sub`, [value("f32", -1), value("f32", -0)]), [
  value("f32", -1),
]);

// ./test/core/f32.wast:580
assert_return(() => invoke($0, `sub`, [value("f32", -1), value("f32", 0)]), [
  value("f32", -1),
]);

// ./test/core/f32.wast:581
assert_return(() => invoke($0, `sub`, [value("f32", 1), value("f32", -0)]), [
  value("f32", 1),
]);

// ./test/core/f32.wast:582
assert_return(() => invoke($0, `sub`, [value("f32", 1), value("f32", 0)]), [
  value("f32", 1),
]);

// ./test/core/f32.wast:583
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -1),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:584
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -1),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:585
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 1),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:586
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 1),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:587
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -1),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:588
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -1),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:589
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 1),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:590
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 1),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:591
assert_return(() => invoke($0, `sub`, [value("f32", -1), value("f32", -0.5)]), [
  value("f32", -0.5),
]);

// ./test/core/f32.wast:592
assert_return(() => invoke($0, `sub`, [value("f32", -1), value("f32", 0.5)]), [
  value("f32", -1.5),
]);

// ./test/core/f32.wast:593
assert_return(() => invoke($0, `sub`, [value("f32", 1), value("f32", -0.5)]), [
  value("f32", 1.5),
]);

// ./test/core/f32.wast:594
assert_return(() => invoke($0, `sub`, [value("f32", 1), value("f32", 0.5)]), [
  value("f32", 0.5),
]);

// ./test/core/f32.wast:595
assert_return(() => invoke($0, `sub`, [value("f32", -1), value("f32", -1)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:596
assert_return(() => invoke($0, `sub`, [value("f32", -1), value("f32", 1)]), [
  value("f32", -2),
]);

// ./test/core/f32.wast:597
assert_return(() => invoke($0, `sub`, [value("f32", 1), value("f32", -1)]), [
  value("f32", 2),
]);

// ./test/core/f32.wast:598
assert_return(() => invoke($0, `sub`, [value("f32", 1), value("f32", 1)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:599
assert_return(
  () => invoke($0, `sub`, [value("f32", -1), value("f32", -6.2831855)]),
  [value("f32", 5.2831855)],
);

// ./test/core/f32.wast:600
assert_return(
  () => invoke($0, `sub`, [value("f32", -1), value("f32", 6.2831855)]),
  [value("f32", -7.2831855)],
);

// ./test/core/f32.wast:601
assert_return(
  () => invoke($0, `sub`, [value("f32", 1), value("f32", -6.2831855)]),
  [value("f32", 7.2831855)],
);

// ./test/core/f32.wast:602
assert_return(
  () => invoke($0, `sub`, [value("f32", 1), value("f32", 6.2831855)]),
  [value("f32", -5.2831855)],
);

// ./test/core/f32.wast:603
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -1),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:604
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -1),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:605
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 1),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:606
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 1),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:607
assert_return(
  () => invoke($0, `sub`, [value("f32", -1), value("f32", -Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:608
assert_return(
  () => invoke($0, `sub`, [value("f32", -1), value("f32", Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:609
assert_return(
  () => invoke($0, `sub`, [value("f32", 1), value("f32", -Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:610
assert_return(
  () => invoke($0, `sub`, [value("f32", 1), value("f32", Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:611
assert_return(
  () =>
    invoke($0, `sub`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:612
assert_return(
  () =>
    invoke($0, `sub`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:613
assert_return(
  () =>
    invoke($0, `sub`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:614
assert_return(
  () =>
    invoke($0, `sub`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:615
assert_return(
  () =>
    invoke($0, `sub`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:616
assert_return(
  () =>
    invoke($0, `sub`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:617
assert_return(
  () =>
    invoke($0, `sub`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:618
assert_return(
  () =>
    invoke($0, `sub`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:619
assert_return(
  () => invoke($0, `sub`, [value("f32", -6.2831855), value("f32", -0)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:620
assert_return(
  () => invoke($0, `sub`, [value("f32", -6.2831855), value("f32", 0)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:621
assert_return(
  () => invoke($0, `sub`, [value("f32", 6.2831855), value("f32", -0)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:622
assert_return(
  () => invoke($0, `sub`, [value("f32", 6.2831855), value("f32", 0)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:623
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -6.2831855),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:624
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -6.2831855),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:625
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 6.2831855),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:626
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 6.2831855),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:627
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -6.2831855),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:628
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -6.2831855),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:629
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 6.2831855),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:630
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 6.2831855),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:631
assert_return(
  () => invoke($0, `sub`, [value("f32", -6.2831855), value("f32", -0.5)]),
  [value("f32", -5.7831855)],
);

// ./test/core/f32.wast:632
assert_return(
  () => invoke($0, `sub`, [value("f32", -6.2831855), value("f32", 0.5)]),
  [value("f32", -6.7831855)],
);

// ./test/core/f32.wast:633
assert_return(
  () => invoke($0, `sub`, [value("f32", 6.2831855), value("f32", -0.5)]),
  [value("f32", 6.7831855)],
);

// ./test/core/f32.wast:634
assert_return(
  () => invoke($0, `sub`, [value("f32", 6.2831855), value("f32", 0.5)]),
  [value("f32", 5.7831855)],
);

// ./test/core/f32.wast:635
assert_return(
  () => invoke($0, `sub`, [value("f32", -6.2831855), value("f32", -1)]),
  [value("f32", -5.2831855)],
);

// ./test/core/f32.wast:636
assert_return(
  () => invoke($0, `sub`, [value("f32", -6.2831855), value("f32", 1)]),
  [value("f32", -7.2831855)],
);

// ./test/core/f32.wast:637
assert_return(
  () => invoke($0, `sub`, [value("f32", 6.2831855), value("f32", -1)]),
  [value("f32", 7.2831855)],
);

// ./test/core/f32.wast:638
assert_return(
  () => invoke($0, `sub`, [value("f32", 6.2831855), value("f32", 1)]),
  [value("f32", 5.2831855)],
);

// ./test/core/f32.wast:639
assert_return(
  () => invoke($0, `sub`, [value("f32", -6.2831855), value("f32", -6.2831855)]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:640
assert_return(
  () => invoke($0, `sub`, [value("f32", -6.2831855), value("f32", 6.2831855)]),
  [value("f32", -12.566371)],
);

// ./test/core/f32.wast:641
assert_return(
  () => invoke($0, `sub`, [value("f32", 6.2831855), value("f32", -6.2831855)]),
  [value("f32", 12.566371)],
);

// ./test/core/f32.wast:642
assert_return(
  () => invoke($0, `sub`, [value("f32", 6.2831855), value("f32", 6.2831855)]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:643
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -6.2831855),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:644
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -6.2831855),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:645
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 6.2831855),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:646
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 6.2831855),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:647
assert_return(
  () => invoke($0, `sub`, [value("f32", -6.2831855), value("f32", -Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:648
assert_return(
  () => invoke($0, `sub`, [value("f32", -6.2831855), value("f32", Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:649
assert_return(
  () => invoke($0, `sub`, [value("f32", 6.2831855), value("f32", -Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:650
assert_return(
  () => invoke($0, `sub`, [value("f32", 6.2831855), value("f32", Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:651
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:652
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:653
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:654
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:655
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:656
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:657
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:658
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:659
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:660
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:661
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:662
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:663
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:664
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:665
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:666
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:667
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:668
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:669
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:670
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:671
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.5),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:672
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.5),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:673
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.5),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:674
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.5),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:675
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -1),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:676
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 1),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:677
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -1),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:678
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 1),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:679
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -6.2831855),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:680
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 6.2831855),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:681
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -6.2831855),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:682
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 6.2831855),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:683
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:684
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:685
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:686
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:687
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -Infinity),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:688
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", Infinity),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:689
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -Infinity),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:690
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", Infinity),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:691
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:692
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:693
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:694
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:695
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:696
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:697
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:698
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:699
assert_return(
  () => invoke($0, `sub`, [value("f32", -Infinity), value("f32", -0)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:700
assert_return(
  () => invoke($0, `sub`, [value("f32", -Infinity), value("f32", 0)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:701
assert_return(
  () => invoke($0, `sub`, [value("f32", Infinity), value("f32", -0)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:702
assert_return(
  () => invoke($0, `sub`, [value("f32", Infinity), value("f32", 0)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:703
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -Infinity),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:704
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -Infinity),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:705
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", Infinity),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:706
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", Infinity),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:707
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -Infinity),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:708
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -Infinity),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:709
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", Infinity),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:710
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", Infinity),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:711
assert_return(
  () => invoke($0, `sub`, [value("f32", -Infinity), value("f32", -0.5)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:712
assert_return(
  () => invoke($0, `sub`, [value("f32", -Infinity), value("f32", 0.5)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:713
assert_return(
  () => invoke($0, `sub`, [value("f32", Infinity), value("f32", -0.5)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:714
assert_return(
  () => invoke($0, `sub`, [value("f32", Infinity), value("f32", 0.5)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:715
assert_return(
  () => invoke($0, `sub`, [value("f32", -Infinity), value("f32", -1)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:716
assert_return(
  () => invoke($0, `sub`, [value("f32", -Infinity), value("f32", 1)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:717
assert_return(
  () => invoke($0, `sub`, [value("f32", Infinity), value("f32", -1)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:718
assert_return(
  () => invoke($0, `sub`, [value("f32", Infinity), value("f32", 1)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:719
assert_return(
  () => invoke($0, `sub`, [value("f32", -Infinity), value("f32", -6.2831855)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:720
assert_return(
  () => invoke($0, `sub`, [value("f32", -Infinity), value("f32", 6.2831855)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:721
assert_return(
  () => invoke($0, `sub`, [value("f32", Infinity), value("f32", -6.2831855)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:722
assert_return(
  () => invoke($0, `sub`, [value("f32", Infinity), value("f32", 6.2831855)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:723
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -Infinity),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:724
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -Infinity),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:725
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", Infinity),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:726
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", Infinity),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:727
assert_return(
  () => invoke($0, `sub`, [value("f32", -Infinity), value("f32", -Infinity)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:728
assert_return(
  () => invoke($0, `sub`, [value("f32", -Infinity), value("f32", Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:729
assert_return(
  () => invoke($0, `sub`, [value("f32", Infinity), value("f32", -Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:730
assert_return(
  () => invoke($0, `sub`, [value("f32", Infinity), value("f32", Infinity)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:731
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:732
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:733
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:734
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:735
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:736
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:737
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:738
assert_return(
  () =>
    invoke($0, `sub`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:739
assert_return(
  () =>
    invoke($0, `sub`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", -0)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:740
assert_return(
  () =>
    invoke($0, `sub`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", -0)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:741
assert_return(
  () =>
    invoke($0, `sub`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 0)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:742
assert_return(
  () =>
    invoke($0, `sub`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 0)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:743
assert_return(
  () =>
    invoke($0, `sub`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", -0)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:744
assert_return(
  () =>
    invoke($0, `sub`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", -0)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:745
assert_return(
  () =>
    invoke($0, `sub`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 0)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:746
assert_return(
  () =>
    invoke($0, `sub`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 0)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:747
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:748
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:749
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:750
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:751
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:752
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:753
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:754
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:755
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:756
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:757
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:758
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:759
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:760
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:761
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:762
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:763
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.5),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:764
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.5),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:765
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.5),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:766
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.5),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:767
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.5),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:768
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.5),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:769
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.5),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:770
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.5),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:771
assert_return(
  () =>
    invoke($0, `sub`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", -1)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:772
assert_return(
  () =>
    invoke($0, `sub`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", -1)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:773
assert_return(
  () =>
    invoke($0, `sub`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 1)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:774
assert_return(
  () =>
    invoke($0, `sub`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 1)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:775
assert_return(
  () =>
    invoke($0, `sub`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", -1)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:776
assert_return(
  () =>
    invoke($0, `sub`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", -1)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:777
assert_return(
  () =>
    invoke($0, `sub`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 1)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:778
assert_return(
  () =>
    invoke($0, `sub`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 1)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:779
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -6.2831855),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:780
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -6.2831855),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:781
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 6.2831855),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:782
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 6.2831855),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:783
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -6.2831855),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:784
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -6.2831855),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:785
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 6.2831855),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:786
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 6.2831855),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:787
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:788
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:789
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:790
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:791
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:792
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:793
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:794
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:795
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -Infinity),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:796
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -Infinity),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:797
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", Infinity),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:798
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", Infinity),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:799
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -Infinity),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:800
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -Infinity),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:801
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", Infinity),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:802
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", Infinity),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:803
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:804
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:805
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:806
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:807
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:808
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:809
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:810
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:811
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:812
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:813
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:814
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:815
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:816
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:817
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:818
assert_return(
  () =>
    invoke($0, `sub`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:819
assert_return(() => invoke($0, `mul`, [value("f32", -0), value("f32", -0)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:820
assert_return(() => invoke($0, `mul`, [value("f32", -0), value("f32", 0)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:821
assert_return(() => invoke($0, `mul`, [value("f32", 0), value("f32", -0)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:822
assert_return(() => invoke($0, `mul`, [value("f32", 0), value("f32", 0)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:823
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:824
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:825
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:826
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:827
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:828
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:829
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:830
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:831
assert_return(() => invoke($0, `mul`, [value("f32", -0), value("f32", -0.5)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:832
assert_return(() => invoke($0, `mul`, [value("f32", -0), value("f32", 0.5)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:833
assert_return(() => invoke($0, `mul`, [value("f32", 0), value("f32", -0.5)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:834
assert_return(() => invoke($0, `mul`, [value("f32", 0), value("f32", 0.5)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:835
assert_return(() => invoke($0, `mul`, [value("f32", -0), value("f32", -1)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:836
assert_return(() => invoke($0, `mul`, [value("f32", -0), value("f32", 1)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:837
assert_return(() => invoke($0, `mul`, [value("f32", 0), value("f32", -1)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:838
assert_return(() => invoke($0, `mul`, [value("f32", 0), value("f32", 1)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:839
assert_return(
  () => invoke($0, `mul`, [value("f32", -0), value("f32", -6.2831855)]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:840
assert_return(
  () => invoke($0, `mul`, [value("f32", -0), value("f32", 6.2831855)]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:841
assert_return(
  () => invoke($0, `mul`, [value("f32", 0), value("f32", -6.2831855)]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:842
assert_return(
  () => invoke($0, `mul`, [value("f32", 0), value("f32", 6.2831855)]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:843
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:844
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:845
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:846
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:847
assert_return(
  () => invoke($0, `mul`, [value("f32", -0), value("f32", -Infinity)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:848
assert_return(
  () => invoke($0, `mul`, [value("f32", -0), value("f32", Infinity)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:849
assert_return(
  () => invoke($0, `mul`, [value("f32", 0), value("f32", -Infinity)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:850
assert_return(
  () => invoke($0, `mul`, [value("f32", 0), value("f32", Infinity)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:851
assert_return(
  () =>
    invoke($0, `mul`, [value("f32", -0), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:852
assert_return(
  () =>
    invoke($0, `mul`, [value("f32", -0), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:853
assert_return(
  () =>
    invoke($0, `mul`, [value("f32", -0), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:854
assert_return(
  () =>
    invoke($0, `mul`, [value("f32", -0), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:855
assert_return(
  () =>
    invoke($0, `mul`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:856
assert_return(
  () =>
    invoke($0, `mul`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:857
assert_return(
  () =>
    invoke($0, `mul`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:858
assert_return(
  () =>
    invoke($0, `mul`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:859
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:860
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:861
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:862
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:863
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:864
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:865
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:866
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:867
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:868
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:869
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:870
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:871
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.5),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:872
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.5),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:873
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.5),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:874
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.5),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:875
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -1),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:876
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 1),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:877
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -1),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:878
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 1),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:879
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -6.2831855),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000008)],
);

// ./test/core/f32.wast:880
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 6.2831855),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000008)],
);

// ./test/core/f32.wast:881
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -6.2831855),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000008)],
);

// ./test/core/f32.wast:882
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 6.2831855),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000008)],
);

// ./test/core/f32.wast:883
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 0.00000047683713)],
);

// ./test/core/f32.wast:884
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -0.00000047683713)],
);

// ./test/core/f32.wast:885
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -0.00000047683713)],
);

// ./test/core/f32.wast:886
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 0.00000047683713)],
);

// ./test/core/f32.wast:887
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -Infinity),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:888
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", Infinity),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:889
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -Infinity),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:890
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", Infinity),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:891
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:892
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:893
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:894
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:895
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:896
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:897
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:898
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:899
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:900
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:901
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:902
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:903
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:904
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:905
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:906
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:907
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:908
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:909
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:910
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:911
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.5),
    ]),
  [value("f32", 0.000000000000000000000000000000000000005877472)],
);

// ./test/core/f32.wast:912
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.5),
    ]),
  [value("f32", -0.000000000000000000000000000000000000005877472)],
);

// ./test/core/f32.wast:913
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.5),
    ]),
  [value("f32", -0.000000000000000000000000000000000000005877472)],
);

// ./test/core/f32.wast:914
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.5),
    ]),
  [value("f32", 0.000000000000000000000000000000000000005877472)],
);

// ./test/core/f32.wast:915
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -1),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:916
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 1),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:917
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -1),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:918
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 1),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:919
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -6.2831855),
    ]),
  [value("f32", 0.00000000000000000000000000000000000007385849)],
);

// ./test/core/f32.wast:920
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 6.2831855),
    ]),
  [value("f32", -0.00000000000000000000000000000000000007385849)],
);

// ./test/core/f32.wast:921
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -6.2831855),
    ]),
  [value("f32", -0.00000000000000000000000000000000000007385849)],
);

// ./test/core/f32.wast:922
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 6.2831855),
    ]),
  [value("f32", 0.00000000000000000000000000000000000007385849)],
);

// ./test/core/f32.wast:923
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 3.9999998)],
);

// ./test/core/f32.wast:924
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -3.9999998)],
);

// ./test/core/f32.wast:925
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -3.9999998)],
);

// ./test/core/f32.wast:926
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 3.9999998)],
);

// ./test/core/f32.wast:927
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -Infinity),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:928
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", Infinity),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:929
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -Infinity),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:930
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", Infinity),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:931
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:932
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:933
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:934
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:935
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:936
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:937
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:938
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:939
assert_return(() => invoke($0, `mul`, [value("f32", -0.5), value("f32", -0)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:940
assert_return(() => invoke($0, `mul`, [value("f32", -0.5), value("f32", 0)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:941
assert_return(() => invoke($0, `mul`, [value("f32", 0.5), value("f32", -0)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:942
assert_return(() => invoke($0, `mul`, [value("f32", 0.5), value("f32", 0)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:943
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.5),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:944
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.5),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:945
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.5),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:946
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.5),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:947
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.5),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000005877472)],
);

// ./test/core/f32.wast:948
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.5),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000005877472)],
);

// ./test/core/f32.wast:949
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.5),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000005877472)],
);

// ./test/core/f32.wast:950
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.5),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000005877472)],
);

// ./test/core/f32.wast:951
assert_return(
  () => invoke($0, `mul`, [value("f32", -0.5), value("f32", -0.5)]),
  [value("f32", 0.25)],
);

// ./test/core/f32.wast:952
assert_return(
  () => invoke($0, `mul`, [value("f32", -0.5), value("f32", 0.5)]),
  [value("f32", -0.25)],
);

// ./test/core/f32.wast:953
assert_return(
  () => invoke($0, `mul`, [value("f32", 0.5), value("f32", -0.5)]),
  [value("f32", -0.25)],
);

// ./test/core/f32.wast:954
assert_return(() => invoke($0, `mul`, [value("f32", 0.5), value("f32", 0.5)]), [
  value("f32", 0.25),
]);

// ./test/core/f32.wast:955
assert_return(() => invoke($0, `mul`, [value("f32", -0.5), value("f32", -1)]), [
  value("f32", 0.5),
]);

// ./test/core/f32.wast:956
assert_return(() => invoke($0, `mul`, [value("f32", -0.5), value("f32", 1)]), [
  value("f32", -0.5),
]);

// ./test/core/f32.wast:957
assert_return(() => invoke($0, `mul`, [value("f32", 0.5), value("f32", -1)]), [
  value("f32", -0.5),
]);

// ./test/core/f32.wast:958
assert_return(() => invoke($0, `mul`, [value("f32", 0.5), value("f32", 1)]), [
  value("f32", 0.5),
]);

// ./test/core/f32.wast:959
assert_return(
  () => invoke($0, `mul`, [value("f32", -0.5), value("f32", -6.2831855)]),
  [value("f32", 3.1415927)],
);

// ./test/core/f32.wast:960
assert_return(
  () => invoke($0, `mul`, [value("f32", -0.5), value("f32", 6.2831855)]),
  [value("f32", -3.1415927)],
);

// ./test/core/f32.wast:961
assert_return(
  () => invoke($0, `mul`, [value("f32", 0.5), value("f32", -6.2831855)]),
  [value("f32", -3.1415927)],
);

// ./test/core/f32.wast:962
assert_return(
  () => invoke($0, `mul`, [value("f32", 0.5), value("f32", 6.2831855)]),
  [value("f32", 3.1415927)],
);

// ./test/core/f32.wast:963
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.5),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 170141170000000000000000000000000000000)],
);

// ./test/core/f32.wast:964
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.5),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -170141170000000000000000000000000000000)],
);

// ./test/core/f32.wast:965
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.5),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -170141170000000000000000000000000000000)],
);

// ./test/core/f32.wast:966
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.5),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 170141170000000000000000000000000000000)],
);

// ./test/core/f32.wast:967
assert_return(
  () => invoke($0, `mul`, [value("f32", -0.5), value("f32", -Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:968
assert_return(
  () => invoke($0, `mul`, [value("f32", -0.5), value("f32", Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:969
assert_return(
  () => invoke($0, `mul`, [value("f32", 0.5), value("f32", -Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:970
assert_return(
  () => invoke($0, `mul`, [value("f32", 0.5), value("f32", Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:971
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:972
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:973
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:974
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:975
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:976
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:977
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:978
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:979
assert_return(() => invoke($0, `mul`, [value("f32", -1), value("f32", -0)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:980
assert_return(() => invoke($0, `mul`, [value("f32", -1), value("f32", 0)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:981
assert_return(() => invoke($0, `mul`, [value("f32", 1), value("f32", -0)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:982
assert_return(() => invoke($0, `mul`, [value("f32", 1), value("f32", 0)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:983
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -1),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:984
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -1),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:985
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 1),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:986
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 1),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:987
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -1),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:988
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -1),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:989
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 1),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:990
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 1),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:991
assert_return(() => invoke($0, `mul`, [value("f32", -1), value("f32", -0.5)]), [
  value("f32", 0.5),
]);

// ./test/core/f32.wast:992
assert_return(() => invoke($0, `mul`, [value("f32", -1), value("f32", 0.5)]), [
  value("f32", -0.5),
]);

// ./test/core/f32.wast:993
assert_return(() => invoke($0, `mul`, [value("f32", 1), value("f32", -0.5)]), [
  value("f32", -0.5),
]);

// ./test/core/f32.wast:994
assert_return(() => invoke($0, `mul`, [value("f32", 1), value("f32", 0.5)]), [
  value("f32", 0.5),
]);

// ./test/core/f32.wast:995
assert_return(() => invoke($0, `mul`, [value("f32", -1), value("f32", -1)]), [
  value("f32", 1),
]);

// ./test/core/f32.wast:996
assert_return(() => invoke($0, `mul`, [value("f32", -1), value("f32", 1)]), [
  value("f32", -1),
]);

// ./test/core/f32.wast:997
assert_return(() => invoke($0, `mul`, [value("f32", 1), value("f32", -1)]), [
  value("f32", -1),
]);

// ./test/core/f32.wast:998
assert_return(() => invoke($0, `mul`, [value("f32", 1), value("f32", 1)]), [
  value("f32", 1),
]);

// ./test/core/f32.wast:999
assert_return(
  () => invoke($0, `mul`, [value("f32", -1), value("f32", -6.2831855)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:1000
assert_return(
  () => invoke($0, `mul`, [value("f32", -1), value("f32", 6.2831855)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1001
assert_return(
  () => invoke($0, `mul`, [value("f32", 1), value("f32", -6.2831855)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1002
assert_return(
  () => invoke($0, `mul`, [value("f32", 1), value("f32", 6.2831855)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:1003
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -1),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1004
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -1),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1005
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 1),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1006
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 1),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1007
assert_return(
  () => invoke($0, `mul`, [value("f32", -1), value("f32", -Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1008
assert_return(
  () => invoke($0, `mul`, [value("f32", -1), value("f32", Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1009
assert_return(
  () => invoke($0, `mul`, [value("f32", 1), value("f32", -Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1010
assert_return(
  () => invoke($0, `mul`, [value("f32", 1), value("f32", Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1011
assert_return(
  () =>
    invoke($0, `mul`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1012
assert_return(
  () =>
    invoke($0, `mul`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1013
assert_return(
  () =>
    invoke($0, `mul`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1014
assert_return(
  () =>
    invoke($0, `mul`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1015
assert_return(
  () =>
    invoke($0, `mul`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1016
assert_return(
  () =>
    invoke($0, `mul`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1017
assert_return(
  () =>
    invoke($0, `mul`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1018
assert_return(
  () =>
    invoke($0, `mul`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1019
assert_return(
  () => invoke($0, `mul`, [value("f32", -6.2831855), value("f32", -0)]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1020
assert_return(
  () => invoke($0, `mul`, [value("f32", -6.2831855), value("f32", 0)]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1021
assert_return(
  () => invoke($0, `mul`, [value("f32", 6.2831855), value("f32", -0)]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1022
assert_return(
  () => invoke($0, `mul`, [value("f32", 6.2831855), value("f32", 0)]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1023
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -6.2831855),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000008)],
);

// ./test/core/f32.wast:1024
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -6.2831855),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000008)],
);

// ./test/core/f32.wast:1025
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 6.2831855),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000008)],
);

// ./test/core/f32.wast:1026
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 6.2831855),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000008)],
);

// ./test/core/f32.wast:1027
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -6.2831855),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.00000000000000000000000000000000000007385849)],
);

// ./test/core/f32.wast:1028
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -6.2831855),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.00000000000000000000000000000000000007385849)],
);

// ./test/core/f32.wast:1029
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 6.2831855),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.00000000000000000000000000000000000007385849)],
);

// ./test/core/f32.wast:1030
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 6.2831855),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.00000000000000000000000000000000000007385849)],
);

// ./test/core/f32.wast:1031
assert_return(
  () => invoke($0, `mul`, [value("f32", -6.2831855), value("f32", -0.5)]),
  [value("f32", 3.1415927)],
);

// ./test/core/f32.wast:1032
assert_return(
  () => invoke($0, `mul`, [value("f32", -6.2831855), value("f32", 0.5)]),
  [value("f32", -3.1415927)],
);

// ./test/core/f32.wast:1033
assert_return(
  () => invoke($0, `mul`, [value("f32", 6.2831855), value("f32", -0.5)]),
  [value("f32", -3.1415927)],
);

// ./test/core/f32.wast:1034
assert_return(
  () => invoke($0, `mul`, [value("f32", 6.2831855), value("f32", 0.5)]),
  [value("f32", 3.1415927)],
);

// ./test/core/f32.wast:1035
assert_return(
  () => invoke($0, `mul`, [value("f32", -6.2831855), value("f32", -1)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:1036
assert_return(
  () => invoke($0, `mul`, [value("f32", -6.2831855), value("f32", 1)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1037
assert_return(
  () => invoke($0, `mul`, [value("f32", 6.2831855), value("f32", -1)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1038
assert_return(
  () => invoke($0, `mul`, [value("f32", 6.2831855), value("f32", 1)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:1039
assert_return(
  () => invoke($0, `mul`, [value("f32", -6.2831855), value("f32", -6.2831855)]),
  [value("f32", 39.47842)],
);

// ./test/core/f32.wast:1040
assert_return(
  () => invoke($0, `mul`, [value("f32", -6.2831855), value("f32", 6.2831855)]),
  [value("f32", -39.47842)],
);

// ./test/core/f32.wast:1041
assert_return(
  () => invoke($0, `mul`, [value("f32", 6.2831855), value("f32", -6.2831855)]),
  [value("f32", -39.47842)],
);

// ./test/core/f32.wast:1042
assert_return(
  () => invoke($0, `mul`, [value("f32", 6.2831855), value("f32", 6.2831855)]),
  [value("f32", 39.47842)],
);

// ./test/core/f32.wast:1043
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -6.2831855),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1044
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -6.2831855),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1045
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 6.2831855),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1046
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 6.2831855),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1047
assert_return(
  () => invoke($0, `mul`, [value("f32", -6.2831855), value("f32", -Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1048
assert_return(
  () => invoke($0, `mul`, [value("f32", -6.2831855), value("f32", Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1049
assert_return(
  () => invoke($0, `mul`, [value("f32", 6.2831855), value("f32", -Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1050
assert_return(
  () => invoke($0, `mul`, [value("f32", 6.2831855), value("f32", Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1051
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1052
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1053
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1054
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1055
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1056
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1057
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1058
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1059
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1060
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1061
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1062
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1063
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.00000047683713)],
);

// ./test/core/f32.wast:1064
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.00000047683713)],
);

// ./test/core/f32.wast:1065
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.00000047683713)],
);

// ./test/core/f32.wast:1066
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.00000047683713)],
);

// ./test/core/f32.wast:1067
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 3.9999998)],
);

// ./test/core/f32.wast:1068
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -3.9999998)],
);

// ./test/core/f32.wast:1069
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -3.9999998)],
);

// ./test/core/f32.wast:1070
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 3.9999998)],
);

// ./test/core/f32.wast:1071
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.5),
    ]),
  [value("f32", 170141170000000000000000000000000000000)],
);

// ./test/core/f32.wast:1072
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.5),
    ]),
  [value("f32", -170141170000000000000000000000000000000)],
);

// ./test/core/f32.wast:1073
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.5),
    ]),
  [value("f32", -170141170000000000000000000000000000000)],
);

// ./test/core/f32.wast:1074
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.5),
    ]),
  [value("f32", 170141170000000000000000000000000000000)],
);

// ./test/core/f32.wast:1075
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -1),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1076
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 1),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1077
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -1),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1078
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 1),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1079
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -6.2831855),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1080
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 6.2831855),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1081
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -6.2831855),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1082
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 6.2831855),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1083
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1084
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1085
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1086
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1087
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -Infinity),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1088
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", Infinity),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1089
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -Infinity),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1090
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", Infinity),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1091
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1092
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1093
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1094
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1095
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1096
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1097
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1098
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1099
assert_return(
  () => invoke($0, `mul`, [value("f32", -Infinity), value("f32", -0)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1100
assert_return(
  () => invoke($0, `mul`, [value("f32", -Infinity), value("f32", 0)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1101
assert_return(
  () => invoke($0, `mul`, [value("f32", Infinity), value("f32", -0)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1102
assert_return(
  () => invoke($0, `mul`, [value("f32", Infinity), value("f32", 0)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1103
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -Infinity),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1104
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -Infinity),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1105
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", Infinity),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1106
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", Infinity),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1107
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -Infinity),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1108
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -Infinity),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1109
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", Infinity),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1110
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", Infinity),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1111
assert_return(
  () => invoke($0, `mul`, [value("f32", -Infinity), value("f32", -0.5)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1112
assert_return(
  () => invoke($0, `mul`, [value("f32", -Infinity), value("f32", 0.5)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1113
assert_return(
  () => invoke($0, `mul`, [value("f32", Infinity), value("f32", -0.5)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1114
assert_return(
  () => invoke($0, `mul`, [value("f32", Infinity), value("f32", 0.5)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1115
assert_return(
  () => invoke($0, `mul`, [value("f32", -Infinity), value("f32", -1)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1116
assert_return(
  () => invoke($0, `mul`, [value("f32", -Infinity), value("f32", 1)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1117
assert_return(
  () => invoke($0, `mul`, [value("f32", Infinity), value("f32", -1)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1118
assert_return(
  () => invoke($0, `mul`, [value("f32", Infinity), value("f32", 1)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1119
assert_return(
  () => invoke($0, `mul`, [value("f32", -Infinity), value("f32", -6.2831855)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1120
assert_return(
  () => invoke($0, `mul`, [value("f32", -Infinity), value("f32", 6.2831855)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1121
assert_return(
  () => invoke($0, `mul`, [value("f32", Infinity), value("f32", -6.2831855)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1122
assert_return(
  () => invoke($0, `mul`, [value("f32", Infinity), value("f32", 6.2831855)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1123
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -Infinity),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1124
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -Infinity),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1125
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", Infinity),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1126
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", Infinity),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1127
assert_return(
  () => invoke($0, `mul`, [value("f32", -Infinity), value("f32", -Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1128
assert_return(
  () => invoke($0, `mul`, [value("f32", -Infinity), value("f32", Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1129
assert_return(
  () => invoke($0, `mul`, [value("f32", Infinity), value("f32", -Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1130
assert_return(
  () => invoke($0, `mul`, [value("f32", Infinity), value("f32", Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1131
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1132
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1133
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1134
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1135
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1136
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1137
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1138
assert_return(
  () =>
    invoke($0, `mul`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1139
assert_return(
  () =>
    invoke($0, `mul`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", -0)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1140
assert_return(
  () =>
    invoke($0, `mul`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", -0)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1141
assert_return(
  () =>
    invoke($0, `mul`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 0)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1142
assert_return(
  () =>
    invoke($0, `mul`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 0)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1143
assert_return(
  () =>
    invoke($0, `mul`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", -0)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1144
assert_return(
  () =>
    invoke($0, `mul`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", -0)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1145
assert_return(
  () =>
    invoke($0, `mul`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 0)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1146
assert_return(
  () =>
    invoke($0, `mul`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 0)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1147
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1148
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1149
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1150
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1151
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1152
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1153
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1154
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1155
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1156
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1157
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1158
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1159
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1160
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1161
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1162
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1163
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.5),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1164
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.5),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1165
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.5),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1166
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.5),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1167
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.5),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1168
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.5),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1169
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.5),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1170
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.5),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1171
assert_return(
  () =>
    invoke($0, `mul`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", -1)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1172
assert_return(
  () =>
    invoke($0, `mul`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", -1)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1173
assert_return(
  () =>
    invoke($0, `mul`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 1)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1174
assert_return(
  () =>
    invoke($0, `mul`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 1)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1175
assert_return(
  () =>
    invoke($0, `mul`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", -1)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1176
assert_return(
  () =>
    invoke($0, `mul`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", -1)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1177
assert_return(
  () =>
    invoke($0, `mul`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 1)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1178
assert_return(
  () =>
    invoke($0, `mul`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 1)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1179
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -6.2831855),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1180
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -6.2831855),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1181
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 6.2831855),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1182
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 6.2831855),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1183
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -6.2831855),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1184
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -6.2831855),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1185
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 6.2831855),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1186
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 6.2831855),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1187
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1188
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1189
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1190
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1191
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1192
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1193
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1194
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1195
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -Infinity),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1196
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -Infinity),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1197
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", Infinity),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1198
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", Infinity),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1199
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -Infinity),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1200
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -Infinity),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1201
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", Infinity),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1202
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", Infinity),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1203
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1204
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1205
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1206
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1207
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1208
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1209
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1210
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1211
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1212
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1213
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1214
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1215
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1216
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1217
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1218
assert_return(
  () =>
    invoke($0, `mul`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1219
assert_return(() => invoke($0, `div`, [value("f32", -0), value("f32", -0)]), [
  `canonical_nan`,
]);

// ./test/core/f32.wast:1220
assert_return(() => invoke($0, `div`, [value("f32", -0), value("f32", 0)]), [
  `canonical_nan`,
]);

// ./test/core/f32.wast:1221
assert_return(() => invoke($0, `div`, [value("f32", 0), value("f32", -0)]), [
  `canonical_nan`,
]);

// ./test/core/f32.wast:1222
assert_return(() => invoke($0, `div`, [value("f32", 0), value("f32", 0)]), [
  `canonical_nan`,
]);

// ./test/core/f32.wast:1223
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1224
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1225
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1226
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1227
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1228
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1229
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1230
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1231
assert_return(() => invoke($0, `div`, [value("f32", -0), value("f32", -0.5)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:1232
assert_return(() => invoke($0, `div`, [value("f32", -0), value("f32", 0.5)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:1233
assert_return(() => invoke($0, `div`, [value("f32", 0), value("f32", -0.5)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:1234
assert_return(() => invoke($0, `div`, [value("f32", 0), value("f32", 0.5)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:1235
assert_return(() => invoke($0, `div`, [value("f32", -0), value("f32", -1)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:1236
assert_return(() => invoke($0, `div`, [value("f32", -0), value("f32", 1)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:1237
assert_return(() => invoke($0, `div`, [value("f32", 0), value("f32", -1)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:1238
assert_return(() => invoke($0, `div`, [value("f32", 0), value("f32", 1)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:1239
assert_return(
  () => invoke($0, `div`, [value("f32", -0), value("f32", -6.2831855)]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1240
assert_return(
  () => invoke($0, `div`, [value("f32", -0), value("f32", 6.2831855)]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1241
assert_return(
  () => invoke($0, `div`, [value("f32", 0), value("f32", -6.2831855)]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1242
assert_return(
  () => invoke($0, `div`, [value("f32", 0), value("f32", 6.2831855)]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1243
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1244
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1245
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1246
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1247
assert_return(
  () => invoke($0, `div`, [value("f32", -0), value("f32", -Infinity)]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1248
assert_return(
  () => invoke($0, `div`, [value("f32", -0), value("f32", Infinity)]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1249
assert_return(
  () => invoke($0, `div`, [value("f32", 0), value("f32", -Infinity)]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1250
assert_return(
  () => invoke($0, `div`, [value("f32", 0), value("f32", Infinity)]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1251
assert_return(
  () =>
    invoke($0, `div`, [value("f32", -0), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1252
assert_return(
  () =>
    invoke($0, `div`, [value("f32", -0), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1253
assert_return(
  () =>
    invoke($0, `div`, [value("f32", -0), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1254
assert_return(
  () =>
    invoke($0, `div`, [value("f32", -0), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1255
assert_return(
  () =>
    invoke($0, `div`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1256
assert_return(
  () =>
    invoke($0, `div`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1257
assert_return(
  () =>
    invoke($0, `div`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1258
assert_return(
  () =>
    invoke($0, `div`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1259
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1260
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1261
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1262
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1263
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:1264
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:1265
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:1266
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:1267
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.00000011920929)],
);

// ./test/core/f32.wast:1268
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.00000011920929)],
);

// ./test/core/f32.wast:1269
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.00000011920929)],
);

// ./test/core/f32.wast:1270
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.00000011920929)],
);

// ./test/core/f32.wast:1271
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.5),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000003)],
);

// ./test/core/f32.wast:1272
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.5),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000003)],
);

// ./test/core/f32.wast:1273
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.5),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000003)],
);

// ./test/core/f32.wast:1274
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.5),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000003)],
);

// ./test/core/f32.wast:1275
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -1),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1276
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 1),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1277
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -1),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1278
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 1),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1279
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -6.2831855),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1280
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 6.2831855),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1281
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -6.2831855),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1282
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 6.2831855),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1283
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1284
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1285
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1286
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1287
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -Infinity),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1288
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", Infinity),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1289
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -Infinity),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1290
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", Infinity),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1291
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1292
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1293
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1294
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1295
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1296
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1297
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1298
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1299
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1300
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1301
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1302
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1303
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 8388608)],
);

// ./test/core/f32.wast:1304
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -8388608)],
);

// ./test/core/f32.wast:1305
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -8388608)],
);

// ./test/core/f32.wast:1306
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 8388608)],
);

// ./test/core/f32.wast:1307
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:1308
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:1309
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:1310
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:1311
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.5),
    ]),
  [value("f32", 0.000000000000000000000000000000000000023509887)],
);

// ./test/core/f32.wast:1312
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.5),
    ]),
  [value("f32", -0.000000000000000000000000000000000000023509887)],
);

// ./test/core/f32.wast:1313
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.5),
    ]),
  [value("f32", -0.000000000000000000000000000000000000023509887)],
);

// ./test/core/f32.wast:1314
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.5),
    ]),
  [value("f32", 0.000000000000000000000000000000000000023509887)],
);

// ./test/core/f32.wast:1315
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -1),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1316
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 1),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1317
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -1),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1318
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 1),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1319
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -6.2831855),
    ]),
  [value("f32", 0.000000000000000000000000000000000000001870857)],
);

// ./test/core/f32.wast:1320
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 6.2831855),
    ]),
  [value("f32", -0.000000000000000000000000000000000000001870857)],
);

// ./test/core/f32.wast:1321
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -6.2831855),
    ]),
  [value("f32", -0.000000000000000000000000000000000000001870857)],
);

// ./test/core/f32.wast:1322
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 6.2831855),
    ]),
  [value("f32", 0.000000000000000000000000000000000000001870857)],
);

// ./test/core/f32.wast:1323
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1324
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1325
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1326
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1327
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -Infinity),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1328
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", Infinity),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1329
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -Infinity),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1330
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", Infinity),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1331
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1332
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1333
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1334
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1335
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1336
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1337
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1338
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1339
assert_return(() => invoke($0, `div`, [value("f32", -0.5), value("f32", -0)]), [
  value("f32", Infinity),
]);

// ./test/core/f32.wast:1340
assert_return(() => invoke($0, `div`, [value("f32", -0.5), value("f32", 0)]), [
  value("f32", -Infinity),
]);

// ./test/core/f32.wast:1341
assert_return(() => invoke($0, `div`, [value("f32", 0.5), value("f32", -0)]), [
  value("f32", -Infinity),
]);

// ./test/core/f32.wast:1342
assert_return(() => invoke($0, `div`, [value("f32", 0.5), value("f32", 0)]), [
  value("f32", Infinity),
]);

// ./test/core/f32.wast:1343
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.5),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1344
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.5),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1345
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.5),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1346
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.5),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1347
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.5),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 42535296000000000000000000000000000000)],
);

// ./test/core/f32.wast:1348
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.5),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -42535296000000000000000000000000000000)],
);

// ./test/core/f32.wast:1349
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.5),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -42535296000000000000000000000000000000)],
);

// ./test/core/f32.wast:1350
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.5),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 42535296000000000000000000000000000000)],
);

// ./test/core/f32.wast:1351
assert_return(
  () => invoke($0, `div`, [value("f32", -0.5), value("f32", -0.5)]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:1352
assert_return(
  () => invoke($0, `div`, [value("f32", -0.5), value("f32", 0.5)]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:1353
assert_return(
  () => invoke($0, `div`, [value("f32", 0.5), value("f32", -0.5)]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:1354
assert_return(() => invoke($0, `div`, [value("f32", 0.5), value("f32", 0.5)]), [
  value("f32", 1),
]);

// ./test/core/f32.wast:1355
assert_return(() => invoke($0, `div`, [value("f32", -0.5), value("f32", -1)]), [
  value("f32", 0.5),
]);

// ./test/core/f32.wast:1356
assert_return(() => invoke($0, `div`, [value("f32", -0.5), value("f32", 1)]), [
  value("f32", -0.5),
]);

// ./test/core/f32.wast:1357
assert_return(() => invoke($0, `div`, [value("f32", 0.5), value("f32", -1)]), [
  value("f32", -0.5),
]);

// ./test/core/f32.wast:1358
assert_return(() => invoke($0, `div`, [value("f32", 0.5), value("f32", 1)]), [
  value("f32", 0.5),
]);

// ./test/core/f32.wast:1359
assert_return(
  () => invoke($0, `div`, [value("f32", -0.5), value("f32", -6.2831855)]),
  [value("f32", 0.07957747)],
);

// ./test/core/f32.wast:1360
assert_return(
  () => invoke($0, `div`, [value("f32", -0.5), value("f32", 6.2831855)]),
  [value("f32", -0.07957747)],
);

// ./test/core/f32.wast:1361
assert_return(
  () => invoke($0, `div`, [value("f32", 0.5), value("f32", -6.2831855)]),
  [value("f32", -0.07957747)],
);

// ./test/core/f32.wast:1362
assert_return(
  () => invoke($0, `div`, [value("f32", 0.5), value("f32", 6.2831855)]),
  [value("f32", 0.07957747)],
);

// ./test/core/f32.wast:1363
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.5),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 0.000000000000000000000000000000000000001469368)],
);

// ./test/core/f32.wast:1364
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.5),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -0.000000000000000000000000000000000000001469368)],
);

// ./test/core/f32.wast:1365
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.5),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -0.000000000000000000000000000000000000001469368)],
);

// ./test/core/f32.wast:1366
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.5),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 0.000000000000000000000000000000000000001469368)],
);

// ./test/core/f32.wast:1367
assert_return(
  () => invoke($0, `div`, [value("f32", -0.5), value("f32", -Infinity)]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1368
assert_return(
  () => invoke($0, `div`, [value("f32", -0.5), value("f32", Infinity)]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1369
assert_return(
  () => invoke($0, `div`, [value("f32", 0.5), value("f32", -Infinity)]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1370
assert_return(
  () => invoke($0, `div`, [value("f32", 0.5), value("f32", Infinity)]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1371
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1372
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1373
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1374
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1375
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1376
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1377
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1378
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1379
assert_return(() => invoke($0, `div`, [value("f32", -1), value("f32", -0)]), [
  value("f32", Infinity),
]);

// ./test/core/f32.wast:1380
assert_return(() => invoke($0, `div`, [value("f32", -1), value("f32", 0)]), [
  value("f32", -Infinity),
]);

// ./test/core/f32.wast:1381
assert_return(() => invoke($0, `div`, [value("f32", 1), value("f32", -0)]), [
  value("f32", -Infinity),
]);

// ./test/core/f32.wast:1382
assert_return(() => invoke($0, `div`, [value("f32", 1), value("f32", 0)]), [
  value("f32", Infinity),
]);

// ./test/core/f32.wast:1383
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -1),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1384
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -1),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1385
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 1),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1386
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 1),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1387
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -1),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 85070590000000000000000000000000000000)],
);

// ./test/core/f32.wast:1388
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -1),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -85070590000000000000000000000000000000)],
);

// ./test/core/f32.wast:1389
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 1),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -85070590000000000000000000000000000000)],
);

// ./test/core/f32.wast:1390
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 1),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 85070590000000000000000000000000000000)],
);

// ./test/core/f32.wast:1391
assert_return(() => invoke($0, `div`, [value("f32", -1), value("f32", -0.5)]), [
  value("f32", 2),
]);

// ./test/core/f32.wast:1392
assert_return(() => invoke($0, `div`, [value("f32", -1), value("f32", 0.5)]), [
  value("f32", -2),
]);

// ./test/core/f32.wast:1393
assert_return(() => invoke($0, `div`, [value("f32", 1), value("f32", -0.5)]), [
  value("f32", -2),
]);

// ./test/core/f32.wast:1394
assert_return(() => invoke($0, `div`, [value("f32", 1), value("f32", 0.5)]), [
  value("f32", 2),
]);

// ./test/core/f32.wast:1395
assert_return(() => invoke($0, `div`, [value("f32", -1), value("f32", -1)]), [
  value("f32", 1),
]);

// ./test/core/f32.wast:1396
assert_return(() => invoke($0, `div`, [value("f32", -1), value("f32", 1)]), [
  value("f32", -1),
]);

// ./test/core/f32.wast:1397
assert_return(() => invoke($0, `div`, [value("f32", 1), value("f32", -1)]), [
  value("f32", -1),
]);

// ./test/core/f32.wast:1398
assert_return(() => invoke($0, `div`, [value("f32", 1), value("f32", 1)]), [
  value("f32", 1),
]);

// ./test/core/f32.wast:1399
assert_return(
  () => invoke($0, `div`, [value("f32", -1), value("f32", -6.2831855)]),
  [value("f32", 0.15915494)],
);

// ./test/core/f32.wast:1400
assert_return(
  () => invoke($0, `div`, [value("f32", -1), value("f32", 6.2831855)]),
  [value("f32", -0.15915494)],
);

// ./test/core/f32.wast:1401
assert_return(
  () => invoke($0, `div`, [value("f32", 1), value("f32", -6.2831855)]),
  [value("f32", -0.15915494)],
);

// ./test/core/f32.wast:1402
assert_return(
  () => invoke($0, `div`, [value("f32", 1), value("f32", 6.2831855)]),
  [value("f32", 0.15915494)],
);

// ./test/core/f32.wast:1403
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -1),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 0.000000000000000000000000000000000000002938736)],
);

// ./test/core/f32.wast:1404
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -1),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -0.000000000000000000000000000000000000002938736)],
);

// ./test/core/f32.wast:1405
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 1),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -0.000000000000000000000000000000000000002938736)],
);

// ./test/core/f32.wast:1406
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 1),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 0.000000000000000000000000000000000000002938736)],
);

// ./test/core/f32.wast:1407
assert_return(
  () => invoke($0, `div`, [value("f32", -1), value("f32", -Infinity)]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1408
assert_return(
  () => invoke($0, `div`, [value("f32", -1), value("f32", Infinity)]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1409
assert_return(
  () => invoke($0, `div`, [value("f32", 1), value("f32", -Infinity)]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1410
assert_return(
  () => invoke($0, `div`, [value("f32", 1), value("f32", Infinity)]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1411
assert_return(
  () =>
    invoke($0, `div`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1412
assert_return(
  () =>
    invoke($0, `div`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1413
assert_return(
  () =>
    invoke($0, `div`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1414
assert_return(
  () =>
    invoke($0, `div`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1415
assert_return(
  () =>
    invoke($0, `div`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1416
assert_return(
  () =>
    invoke($0, `div`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1417
assert_return(
  () =>
    invoke($0, `div`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1418
assert_return(
  () =>
    invoke($0, `div`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1419
assert_return(
  () => invoke($0, `div`, [value("f32", -6.2831855), value("f32", -0)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1420
assert_return(
  () => invoke($0, `div`, [value("f32", -6.2831855), value("f32", 0)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1421
assert_return(
  () => invoke($0, `div`, [value("f32", 6.2831855), value("f32", -0)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1422
assert_return(
  () => invoke($0, `div`, [value("f32", 6.2831855), value("f32", 0)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1423
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -6.2831855),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1424
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -6.2831855),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1425
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 6.2831855),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1426
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 6.2831855),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1427
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -6.2831855),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1428
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -6.2831855),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1429
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 6.2831855),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1430
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 6.2831855),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1431
assert_return(
  () => invoke($0, `div`, [value("f32", -6.2831855), value("f32", -0.5)]),
  [value("f32", 12.566371)],
);

// ./test/core/f32.wast:1432
assert_return(
  () => invoke($0, `div`, [value("f32", -6.2831855), value("f32", 0.5)]),
  [value("f32", -12.566371)],
);

// ./test/core/f32.wast:1433
assert_return(
  () => invoke($0, `div`, [value("f32", 6.2831855), value("f32", -0.5)]),
  [value("f32", -12.566371)],
);

// ./test/core/f32.wast:1434
assert_return(
  () => invoke($0, `div`, [value("f32", 6.2831855), value("f32", 0.5)]),
  [value("f32", 12.566371)],
);

// ./test/core/f32.wast:1435
assert_return(
  () => invoke($0, `div`, [value("f32", -6.2831855), value("f32", -1)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:1436
assert_return(
  () => invoke($0, `div`, [value("f32", -6.2831855), value("f32", 1)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1437
assert_return(
  () => invoke($0, `div`, [value("f32", 6.2831855), value("f32", -1)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1438
assert_return(
  () => invoke($0, `div`, [value("f32", 6.2831855), value("f32", 1)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:1439
assert_return(
  () => invoke($0, `div`, [value("f32", -6.2831855), value("f32", -6.2831855)]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:1440
assert_return(
  () => invoke($0, `div`, [value("f32", -6.2831855), value("f32", 6.2831855)]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:1441
assert_return(
  () => invoke($0, `div`, [value("f32", 6.2831855), value("f32", -6.2831855)]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:1442
assert_return(
  () => invoke($0, `div`, [value("f32", 6.2831855), value("f32", 6.2831855)]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:1443
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -6.2831855),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 0.000000000000000000000000000000000000018464624)],
);

// ./test/core/f32.wast:1444
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -6.2831855),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -0.000000000000000000000000000000000000018464624)],
);

// ./test/core/f32.wast:1445
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 6.2831855),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -0.000000000000000000000000000000000000018464624)],
);

// ./test/core/f32.wast:1446
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 6.2831855),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 0.000000000000000000000000000000000000018464624)],
);

// ./test/core/f32.wast:1447
assert_return(
  () => invoke($0, `div`, [value("f32", -6.2831855), value("f32", -Infinity)]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1448
assert_return(
  () => invoke($0, `div`, [value("f32", -6.2831855), value("f32", Infinity)]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1449
assert_return(
  () => invoke($0, `div`, [value("f32", 6.2831855), value("f32", -Infinity)]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1450
assert_return(
  () => invoke($0, `div`, [value("f32", 6.2831855), value("f32", Infinity)]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1451
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1452
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1453
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1454
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1455
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1456
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1457
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1458
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1459
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1460
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1461
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1462
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1463
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1464
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1465
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1466
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1467
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1468
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1469
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1470
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1471
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.5),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1472
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.5),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1473
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.5),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1474
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.5),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1475
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -1),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1476
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 1),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1477
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -1),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1478
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 1),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1479
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -6.2831855),
    ]),
  [value("f32", 54157613000000000000000000000000000000)],
);

// ./test/core/f32.wast:1480
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 6.2831855),
    ]),
  [value("f32", -54157613000000000000000000000000000000)],
);

// ./test/core/f32.wast:1481
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -6.2831855),
    ]),
  [value("f32", -54157613000000000000000000000000000000)],
);

// ./test/core/f32.wast:1482
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 6.2831855),
    ]),
  [value("f32", 54157613000000000000000000000000000000)],
);

// ./test/core/f32.wast:1483
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:1484
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:1485
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:1486
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:1487
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -Infinity),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1488
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", Infinity),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1489
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -Infinity),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1490
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", Infinity),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1491
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1492
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1493
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1494
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1495
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1496
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1497
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1498
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1499
assert_return(
  () => invoke($0, `div`, [value("f32", -Infinity), value("f32", -0)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1500
assert_return(
  () => invoke($0, `div`, [value("f32", -Infinity), value("f32", 0)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1501
assert_return(
  () => invoke($0, `div`, [value("f32", Infinity), value("f32", -0)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1502
assert_return(
  () => invoke($0, `div`, [value("f32", Infinity), value("f32", 0)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1503
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -Infinity),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1504
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -Infinity),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1505
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", Infinity),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1506
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", Infinity),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1507
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -Infinity),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1508
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -Infinity),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1509
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", Infinity),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1510
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", Infinity),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1511
assert_return(
  () => invoke($0, `div`, [value("f32", -Infinity), value("f32", -0.5)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1512
assert_return(
  () => invoke($0, `div`, [value("f32", -Infinity), value("f32", 0.5)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1513
assert_return(
  () => invoke($0, `div`, [value("f32", Infinity), value("f32", -0.5)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1514
assert_return(
  () => invoke($0, `div`, [value("f32", Infinity), value("f32", 0.5)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1515
assert_return(
  () => invoke($0, `div`, [value("f32", -Infinity), value("f32", -1)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1516
assert_return(
  () => invoke($0, `div`, [value("f32", -Infinity), value("f32", 1)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1517
assert_return(
  () => invoke($0, `div`, [value("f32", Infinity), value("f32", -1)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1518
assert_return(
  () => invoke($0, `div`, [value("f32", Infinity), value("f32", 1)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1519
assert_return(
  () => invoke($0, `div`, [value("f32", -Infinity), value("f32", -6.2831855)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1520
assert_return(
  () => invoke($0, `div`, [value("f32", -Infinity), value("f32", 6.2831855)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1521
assert_return(
  () => invoke($0, `div`, [value("f32", Infinity), value("f32", -6.2831855)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1522
assert_return(
  () => invoke($0, `div`, [value("f32", Infinity), value("f32", 6.2831855)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1523
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -Infinity),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1524
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -Infinity),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1525
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", Infinity),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1526
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", Infinity),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1527
assert_return(
  () => invoke($0, `div`, [value("f32", -Infinity), value("f32", -Infinity)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1528
assert_return(
  () => invoke($0, `div`, [value("f32", -Infinity), value("f32", Infinity)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1529
assert_return(
  () => invoke($0, `div`, [value("f32", Infinity), value("f32", -Infinity)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1530
assert_return(
  () => invoke($0, `div`, [value("f32", Infinity), value("f32", Infinity)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1531
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1532
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1533
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1534
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1535
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1536
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1537
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1538
assert_return(
  () =>
    invoke($0, `div`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1539
assert_return(
  () =>
    invoke($0, `div`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", -0)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1540
assert_return(
  () =>
    invoke($0, `div`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", -0)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1541
assert_return(
  () =>
    invoke($0, `div`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 0)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1542
assert_return(
  () =>
    invoke($0, `div`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 0)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1543
assert_return(
  () =>
    invoke($0, `div`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", -0)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1544
assert_return(
  () =>
    invoke($0, `div`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", -0)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1545
assert_return(
  () =>
    invoke($0, `div`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 0)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1546
assert_return(
  () =>
    invoke($0, `div`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 0)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1547
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1548
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1549
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1550
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1551
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1552
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1553
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1554
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1555
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1556
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1557
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1558
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1559
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1560
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1561
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1562
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1563
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.5),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1564
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.5),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1565
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.5),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1566
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.5),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1567
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.5),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1568
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.5),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1569
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.5),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1570
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.5),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1571
assert_return(
  () =>
    invoke($0, `div`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", -1)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1572
assert_return(
  () =>
    invoke($0, `div`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", -1)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1573
assert_return(
  () =>
    invoke($0, `div`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 1)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1574
assert_return(
  () =>
    invoke($0, `div`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 1)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1575
assert_return(
  () =>
    invoke($0, `div`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", -1)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1576
assert_return(
  () =>
    invoke($0, `div`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", -1)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1577
assert_return(
  () =>
    invoke($0, `div`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 1)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1578
assert_return(
  () =>
    invoke($0, `div`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 1)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1579
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -6.2831855),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1580
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -6.2831855),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1581
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 6.2831855),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1582
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 6.2831855),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1583
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -6.2831855),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1584
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -6.2831855),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1585
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 6.2831855),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1586
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 6.2831855),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1587
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1588
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1589
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1590
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1591
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1592
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1593
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1594
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1595
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -Infinity),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1596
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -Infinity),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1597
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", Infinity),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1598
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", Infinity),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1599
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -Infinity),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1600
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -Infinity),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1601
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", Infinity),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1602
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", Infinity),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1603
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1604
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1605
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1606
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1607
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1608
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1609
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1610
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1611
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1612
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1613
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1614
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1615
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1616
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1617
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1618
assert_return(
  () =>
    invoke($0, `div`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1619
assert_return(() => invoke($0, `min`, [value("f32", -0), value("f32", -0)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:1620
assert_return(() => invoke($0, `min`, [value("f32", -0), value("f32", 0)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:1621
assert_return(() => invoke($0, `min`, [value("f32", 0), value("f32", -0)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:1622
assert_return(() => invoke($0, `min`, [value("f32", 0), value("f32", 0)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:1623
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1624
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1625
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1626
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1627
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1628
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1629
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1630
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1631
assert_return(() => invoke($0, `min`, [value("f32", -0), value("f32", -0.5)]), [
  value("f32", -0.5),
]);

// ./test/core/f32.wast:1632
assert_return(() => invoke($0, `min`, [value("f32", -0), value("f32", 0.5)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:1633
assert_return(() => invoke($0, `min`, [value("f32", 0), value("f32", -0.5)]), [
  value("f32", -0.5),
]);

// ./test/core/f32.wast:1634
assert_return(() => invoke($0, `min`, [value("f32", 0), value("f32", 0.5)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:1635
assert_return(() => invoke($0, `min`, [value("f32", -0), value("f32", -1)]), [
  value("f32", -1),
]);

// ./test/core/f32.wast:1636
assert_return(() => invoke($0, `min`, [value("f32", -0), value("f32", 1)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:1637
assert_return(() => invoke($0, `min`, [value("f32", 0), value("f32", -1)]), [
  value("f32", -1),
]);

// ./test/core/f32.wast:1638
assert_return(() => invoke($0, `min`, [value("f32", 0), value("f32", 1)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:1639
assert_return(
  () => invoke($0, `min`, [value("f32", -0), value("f32", -6.2831855)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1640
assert_return(
  () => invoke($0, `min`, [value("f32", -0), value("f32", 6.2831855)]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1641
assert_return(
  () => invoke($0, `min`, [value("f32", 0), value("f32", -6.2831855)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1642
assert_return(
  () => invoke($0, `min`, [value("f32", 0), value("f32", 6.2831855)]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1643
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1644
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1645
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1646
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1647
assert_return(
  () => invoke($0, `min`, [value("f32", -0), value("f32", -Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1648
assert_return(
  () => invoke($0, `min`, [value("f32", -0), value("f32", Infinity)]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1649
assert_return(
  () => invoke($0, `min`, [value("f32", 0), value("f32", -Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1650
assert_return(
  () => invoke($0, `min`, [value("f32", 0), value("f32", Infinity)]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1651
assert_return(
  () =>
    invoke($0, `min`, [value("f32", -0), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1652
assert_return(
  () =>
    invoke($0, `min`, [value("f32", -0), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1653
assert_return(
  () =>
    invoke($0, `min`, [value("f32", -0), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1654
assert_return(
  () =>
    invoke($0, `min`, [value("f32", -0), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1655
assert_return(
  () =>
    invoke($0, `min`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1656
assert_return(
  () =>
    invoke($0, `min`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1657
assert_return(
  () =>
    invoke($0, `min`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1658
assert_return(
  () =>
    invoke($0, `min`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1659
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1660
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1661
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1662
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1663
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1664
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1665
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1666
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1667
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1668
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1669
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1670
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1671
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.5),
    ]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:1672
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.5),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1673
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.5),
    ]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:1674
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.5),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1675
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -1),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:1676
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 1),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1677
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -1),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:1678
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 1),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1679
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -6.2831855),
    ]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1680
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 6.2831855),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1681
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -6.2831855),
    ]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1682
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 6.2831855),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1683
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1684
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1685
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1686
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1687
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -Infinity),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1688
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", Infinity),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1689
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -Infinity),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1690
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", Infinity),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1691
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1692
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1693
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1694
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1695
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1696
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1697
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1698
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1699
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1700
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1701
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1702
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1703
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1704
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1705
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1706
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1707
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1708
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1709
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1710
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1711
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.5),
    ]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:1712
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.5),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1713
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.5),
    ]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:1714
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.5),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1715
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -1),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:1716
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 1),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1717
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -1),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:1718
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 1),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1719
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -6.2831855),
    ]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1720
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 6.2831855),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1721
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -6.2831855),
    ]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1722
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 6.2831855),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1723
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1724
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1725
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1726
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1727
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -Infinity),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1728
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", Infinity),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1729
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -Infinity),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1730
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", Infinity),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1731
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1732
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1733
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1734
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1735
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1736
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1737
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1738
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1739
assert_return(() => invoke($0, `min`, [value("f32", -0.5), value("f32", -0)]), [
  value("f32", -0.5),
]);

// ./test/core/f32.wast:1740
assert_return(() => invoke($0, `min`, [value("f32", -0.5), value("f32", 0)]), [
  value("f32", -0.5),
]);

// ./test/core/f32.wast:1741
assert_return(() => invoke($0, `min`, [value("f32", 0.5), value("f32", -0)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:1742
assert_return(() => invoke($0, `min`, [value("f32", 0.5), value("f32", 0)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:1743
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.5),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:1744
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.5),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:1745
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.5),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1746
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.5),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1747
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.5),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:1748
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.5),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:1749
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.5),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1750
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.5),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1751
assert_return(
  () => invoke($0, `min`, [value("f32", -0.5), value("f32", -0.5)]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:1752
assert_return(
  () => invoke($0, `min`, [value("f32", -0.5), value("f32", 0.5)]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:1753
assert_return(
  () => invoke($0, `min`, [value("f32", 0.5), value("f32", -0.5)]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:1754
assert_return(() => invoke($0, `min`, [value("f32", 0.5), value("f32", 0.5)]), [
  value("f32", 0.5),
]);

// ./test/core/f32.wast:1755
assert_return(() => invoke($0, `min`, [value("f32", -0.5), value("f32", -1)]), [
  value("f32", -1),
]);

// ./test/core/f32.wast:1756
assert_return(() => invoke($0, `min`, [value("f32", -0.5), value("f32", 1)]), [
  value("f32", -0.5),
]);

// ./test/core/f32.wast:1757
assert_return(() => invoke($0, `min`, [value("f32", 0.5), value("f32", -1)]), [
  value("f32", -1),
]);

// ./test/core/f32.wast:1758
assert_return(() => invoke($0, `min`, [value("f32", 0.5), value("f32", 1)]), [
  value("f32", 0.5),
]);

// ./test/core/f32.wast:1759
assert_return(
  () => invoke($0, `min`, [value("f32", -0.5), value("f32", -6.2831855)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1760
assert_return(
  () => invoke($0, `min`, [value("f32", -0.5), value("f32", 6.2831855)]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:1761
assert_return(
  () => invoke($0, `min`, [value("f32", 0.5), value("f32", -6.2831855)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1762
assert_return(
  () => invoke($0, `min`, [value("f32", 0.5), value("f32", 6.2831855)]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:1763
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.5),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1764
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.5),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:1765
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.5),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1766
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.5),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:1767
assert_return(
  () => invoke($0, `min`, [value("f32", -0.5), value("f32", -Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1768
assert_return(
  () => invoke($0, `min`, [value("f32", -0.5), value("f32", Infinity)]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:1769
assert_return(
  () => invoke($0, `min`, [value("f32", 0.5), value("f32", -Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1770
assert_return(
  () => invoke($0, `min`, [value("f32", 0.5), value("f32", Infinity)]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:1771
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1772
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1773
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1774
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1775
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1776
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1777
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1778
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1779
assert_return(() => invoke($0, `min`, [value("f32", -1), value("f32", -0)]), [
  value("f32", -1),
]);

// ./test/core/f32.wast:1780
assert_return(() => invoke($0, `min`, [value("f32", -1), value("f32", 0)]), [
  value("f32", -1),
]);

// ./test/core/f32.wast:1781
assert_return(() => invoke($0, `min`, [value("f32", 1), value("f32", -0)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:1782
assert_return(() => invoke($0, `min`, [value("f32", 1), value("f32", 0)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:1783
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -1),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:1784
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -1),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:1785
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 1),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1786
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 1),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1787
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -1),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:1788
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -1),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:1789
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 1),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1790
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 1),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1791
assert_return(() => invoke($0, `min`, [value("f32", -1), value("f32", -0.5)]), [
  value("f32", -1),
]);

// ./test/core/f32.wast:1792
assert_return(() => invoke($0, `min`, [value("f32", -1), value("f32", 0.5)]), [
  value("f32", -1),
]);

// ./test/core/f32.wast:1793
assert_return(() => invoke($0, `min`, [value("f32", 1), value("f32", -0.5)]), [
  value("f32", -0.5),
]);

// ./test/core/f32.wast:1794
assert_return(() => invoke($0, `min`, [value("f32", 1), value("f32", 0.5)]), [
  value("f32", 0.5),
]);

// ./test/core/f32.wast:1795
assert_return(() => invoke($0, `min`, [value("f32", -1), value("f32", -1)]), [
  value("f32", -1),
]);

// ./test/core/f32.wast:1796
assert_return(() => invoke($0, `min`, [value("f32", -1), value("f32", 1)]), [
  value("f32", -1),
]);

// ./test/core/f32.wast:1797
assert_return(() => invoke($0, `min`, [value("f32", 1), value("f32", -1)]), [
  value("f32", -1),
]);

// ./test/core/f32.wast:1798
assert_return(() => invoke($0, `min`, [value("f32", 1), value("f32", 1)]), [
  value("f32", 1),
]);

// ./test/core/f32.wast:1799
assert_return(
  () => invoke($0, `min`, [value("f32", -1), value("f32", -6.2831855)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1800
assert_return(
  () => invoke($0, `min`, [value("f32", -1), value("f32", 6.2831855)]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:1801
assert_return(
  () => invoke($0, `min`, [value("f32", 1), value("f32", -6.2831855)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1802
assert_return(
  () => invoke($0, `min`, [value("f32", 1), value("f32", 6.2831855)]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:1803
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -1),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1804
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -1),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:1805
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 1),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1806
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 1),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:1807
assert_return(
  () => invoke($0, `min`, [value("f32", -1), value("f32", -Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1808
assert_return(
  () => invoke($0, `min`, [value("f32", -1), value("f32", Infinity)]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:1809
assert_return(
  () => invoke($0, `min`, [value("f32", 1), value("f32", -Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1810
assert_return(
  () => invoke($0, `min`, [value("f32", 1), value("f32", Infinity)]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:1811
assert_return(
  () =>
    invoke($0, `min`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1812
assert_return(
  () =>
    invoke($0, `min`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1813
assert_return(
  () =>
    invoke($0, `min`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1814
assert_return(
  () =>
    invoke($0, `min`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1815
assert_return(
  () =>
    invoke($0, `min`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1816
assert_return(
  () =>
    invoke($0, `min`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1817
assert_return(
  () =>
    invoke($0, `min`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1818
assert_return(
  () =>
    invoke($0, `min`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1819
assert_return(
  () => invoke($0, `min`, [value("f32", -6.2831855), value("f32", -0)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1820
assert_return(
  () => invoke($0, `min`, [value("f32", -6.2831855), value("f32", 0)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1821
assert_return(
  () => invoke($0, `min`, [value("f32", 6.2831855), value("f32", -0)]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1822
assert_return(
  () => invoke($0, `min`, [value("f32", 6.2831855), value("f32", 0)]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1823
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -6.2831855),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1824
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -6.2831855),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1825
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 6.2831855),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1826
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 6.2831855),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1827
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -6.2831855),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1828
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -6.2831855),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1829
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 6.2831855),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1830
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 6.2831855),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1831
assert_return(
  () => invoke($0, `min`, [value("f32", -6.2831855), value("f32", -0.5)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1832
assert_return(
  () => invoke($0, `min`, [value("f32", -6.2831855), value("f32", 0.5)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1833
assert_return(
  () => invoke($0, `min`, [value("f32", 6.2831855), value("f32", -0.5)]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:1834
assert_return(
  () => invoke($0, `min`, [value("f32", 6.2831855), value("f32", 0.5)]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:1835
assert_return(
  () => invoke($0, `min`, [value("f32", -6.2831855), value("f32", -1)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1836
assert_return(
  () => invoke($0, `min`, [value("f32", -6.2831855), value("f32", 1)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1837
assert_return(
  () => invoke($0, `min`, [value("f32", 6.2831855), value("f32", -1)]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:1838
assert_return(
  () => invoke($0, `min`, [value("f32", 6.2831855), value("f32", 1)]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:1839
assert_return(
  () => invoke($0, `min`, [value("f32", -6.2831855), value("f32", -6.2831855)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1840
assert_return(
  () => invoke($0, `min`, [value("f32", -6.2831855), value("f32", 6.2831855)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1841
assert_return(
  () => invoke($0, `min`, [value("f32", 6.2831855), value("f32", -6.2831855)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1842
assert_return(
  () => invoke($0, `min`, [value("f32", 6.2831855), value("f32", 6.2831855)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:1843
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -6.2831855),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1844
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -6.2831855),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1845
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 6.2831855),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1846
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 6.2831855),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:1847
assert_return(
  () => invoke($0, `min`, [value("f32", -6.2831855), value("f32", -Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1848
assert_return(
  () => invoke($0, `min`, [value("f32", -6.2831855), value("f32", Infinity)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1849
assert_return(
  () => invoke($0, `min`, [value("f32", 6.2831855), value("f32", -Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1850
assert_return(
  () => invoke($0, `min`, [value("f32", 6.2831855), value("f32", Infinity)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:1851
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1852
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1853
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1854
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1855
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1856
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1857
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1858
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1859
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1860
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1861
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1862
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1863
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1864
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1865
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1866
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1867
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1868
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1869
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1870
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1871
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.5),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1872
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.5),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1873
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.5),
    ]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:1874
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.5),
    ]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:1875
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -1),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1876
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 1),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1877
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -1),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:1878
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 1),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:1879
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -6.2831855),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1880
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 6.2831855),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1881
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -6.2831855),
    ]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1882
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 6.2831855),
    ]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:1883
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1884
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1885
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1886
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1887
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -Infinity),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1888
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", Infinity),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1889
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -Infinity),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1890
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", Infinity),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1891
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1892
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1893
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1894
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1895
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1896
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1897
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1898
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1899
assert_return(
  () => invoke($0, `min`, [value("f32", -Infinity), value("f32", -0)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1900
assert_return(
  () => invoke($0, `min`, [value("f32", -Infinity), value("f32", 0)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1901
assert_return(
  () => invoke($0, `min`, [value("f32", Infinity), value("f32", -0)]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:1902
assert_return(
  () => invoke($0, `min`, [value("f32", Infinity), value("f32", 0)]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:1903
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -Infinity),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1904
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -Infinity),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1905
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", Infinity),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1906
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", Infinity),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:1907
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -Infinity),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1908
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -Infinity),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1909
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", Infinity),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1910
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", Infinity),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:1911
assert_return(
  () => invoke($0, `min`, [value("f32", -Infinity), value("f32", -0.5)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1912
assert_return(
  () => invoke($0, `min`, [value("f32", -Infinity), value("f32", 0.5)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1913
assert_return(
  () => invoke($0, `min`, [value("f32", Infinity), value("f32", -0.5)]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:1914
assert_return(
  () => invoke($0, `min`, [value("f32", Infinity), value("f32", 0.5)]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:1915
assert_return(
  () => invoke($0, `min`, [value("f32", -Infinity), value("f32", -1)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1916
assert_return(
  () => invoke($0, `min`, [value("f32", -Infinity), value("f32", 1)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1917
assert_return(
  () => invoke($0, `min`, [value("f32", Infinity), value("f32", -1)]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:1918
assert_return(
  () => invoke($0, `min`, [value("f32", Infinity), value("f32", 1)]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:1919
assert_return(
  () => invoke($0, `min`, [value("f32", -Infinity), value("f32", -6.2831855)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1920
assert_return(
  () => invoke($0, `min`, [value("f32", -Infinity), value("f32", 6.2831855)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1921
assert_return(
  () => invoke($0, `min`, [value("f32", Infinity), value("f32", -6.2831855)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:1922
assert_return(
  () => invoke($0, `min`, [value("f32", Infinity), value("f32", 6.2831855)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:1923
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -Infinity),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1924
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -Infinity),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1925
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", Infinity),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1926
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", Infinity),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:1927
assert_return(
  () => invoke($0, `min`, [value("f32", -Infinity), value("f32", -Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1928
assert_return(
  () => invoke($0, `min`, [value("f32", -Infinity), value("f32", Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1929
assert_return(
  () => invoke($0, `min`, [value("f32", Infinity), value("f32", -Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:1930
assert_return(
  () => invoke($0, `min`, [value("f32", Infinity), value("f32", Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:1931
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1932
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1933
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1934
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1935
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1936
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1937
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1938
assert_return(
  () =>
    invoke($0, `min`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1939
assert_return(
  () =>
    invoke($0, `min`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", -0)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1940
assert_return(
  () =>
    invoke($0, `min`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", -0)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1941
assert_return(
  () =>
    invoke($0, `min`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 0)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1942
assert_return(
  () =>
    invoke($0, `min`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 0)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1943
assert_return(
  () =>
    invoke($0, `min`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", -0)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1944
assert_return(
  () =>
    invoke($0, `min`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", -0)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1945
assert_return(
  () =>
    invoke($0, `min`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 0)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1946
assert_return(
  () =>
    invoke($0, `min`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 0)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1947
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1948
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1949
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1950
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1951
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1952
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1953
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1954
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1955
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1956
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1957
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1958
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1959
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1960
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1961
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1962
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1963
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.5),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1964
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.5),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1965
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.5),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1966
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.5),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1967
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.5),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1968
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.5),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1969
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.5),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1970
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.5),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1971
assert_return(
  () =>
    invoke($0, `min`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", -1)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1972
assert_return(
  () =>
    invoke($0, `min`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", -1)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1973
assert_return(
  () =>
    invoke($0, `min`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 1)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1974
assert_return(
  () =>
    invoke($0, `min`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 1)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1975
assert_return(
  () =>
    invoke($0, `min`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", -1)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1976
assert_return(
  () =>
    invoke($0, `min`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", -1)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1977
assert_return(
  () =>
    invoke($0, `min`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 1)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1978
assert_return(
  () =>
    invoke($0, `min`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 1)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1979
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -6.2831855),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1980
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -6.2831855),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1981
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 6.2831855),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1982
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 6.2831855),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1983
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -6.2831855),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1984
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -6.2831855),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1985
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 6.2831855),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1986
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 6.2831855),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1987
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1988
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1989
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1990
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1991
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1992
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1993
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1994
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1995
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -Infinity),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1996
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -Infinity),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1997
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", Infinity),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:1998
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", Infinity),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:1999
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -Infinity),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2000
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -Infinity),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2001
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", Infinity),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2002
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", Infinity),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2003
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2004
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2005
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2006
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2007
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2008
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2009
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2010
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2011
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2012
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2013
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2014
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2015
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2016
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2017
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2018
assert_return(
  () =>
    invoke($0, `min`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2019
assert_return(() => invoke($0, `max`, [value("f32", -0), value("f32", -0)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:2020
assert_return(() => invoke($0, `max`, [value("f32", -0), value("f32", 0)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:2021
assert_return(() => invoke($0, `max`, [value("f32", 0), value("f32", -0)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:2022
assert_return(() => invoke($0, `max`, [value("f32", 0), value("f32", 0)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:2023
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:2024
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2025
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:2026
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2027
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:2028
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2029
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:2030
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2031
assert_return(() => invoke($0, `max`, [value("f32", -0), value("f32", -0.5)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:2032
assert_return(() => invoke($0, `max`, [value("f32", -0), value("f32", 0.5)]), [
  value("f32", 0.5),
]);

// ./test/core/f32.wast:2033
assert_return(() => invoke($0, `max`, [value("f32", 0), value("f32", -0.5)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:2034
assert_return(() => invoke($0, `max`, [value("f32", 0), value("f32", 0.5)]), [
  value("f32", 0.5),
]);

// ./test/core/f32.wast:2035
assert_return(() => invoke($0, `max`, [value("f32", -0), value("f32", -1)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:2036
assert_return(() => invoke($0, `max`, [value("f32", -0), value("f32", 1)]), [
  value("f32", 1),
]);

// ./test/core/f32.wast:2037
assert_return(() => invoke($0, `max`, [value("f32", 0), value("f32", -1)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:2038
assert_return(() => invoke($0, `max`, [value("f32", 0), value("f32", 1)]), [
  value("f32", 1),
]);

// ./test/core/f32.wast:2039
assert_return(
  () => invoke($0, `max`, [value("f32", -0), value("f32", -6.2831855)]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:2040
assert_return(
  () => invoke($0, `max`, [value("f32", -0), value("f32", 6.2831855)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:2041
assert_return(
  () => invoke($0, `max`, [value("f32", 0), value("f32", -6.2831855)]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:2042
assert_return(
  () => invoke($0, `max`, [value("f32", 0), value("f32", 6.2831855)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:2043
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:2044
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2045
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:2046
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2047
assert_return(
  () => invoke($0, `max`, [value("f32", -0), value("f32", -Infinity)]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:2048
assert_return(
  () => invoke($0, `max`, [value("f32", -0), value("f32", Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2049
assert_return(
  () => invoke($0, `max`, [value("f32", 0), value("f32", -Infinity)]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:2050
assert_return(
  () => invoke($0, `max`, [value("f32", 0), value("f32", Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2051
assert_return(
  () =>
    invoke($0, `max`, [value("f32", -0), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2052
assert_return(
  () =>
    invoke($0, `max`, [value("f32", -0), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2053
assert_return(
  () =>
    invoke($0, `max`, [value("f32", -0), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2054
assert_return(
  () =>
    invoke($0, `max`, [value("f32", -0), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2055
assert_return(
  () =>
    invoke($0, `max`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2056
assert_return(
  () =>
    invoke($0, `max`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2057
assert_return(
  () =>
    invoke($0, `max`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2058
assert_return(
  () =>
    invoke($0, `max`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2059
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:2060
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:2061
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2062
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2063
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2064
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2065
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2066
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2067
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2068
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2069
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2070
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2071
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.5),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2072
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.5),
    ]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:2073
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.5),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2074
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.5),
    ]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:2075
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -1),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2076
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 1),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:2077
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -1),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2078
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 1),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:2079
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -6.2831855),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2080
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 6.2831855),
    ]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:2081
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -6.2831855),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2082
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 6.2831855),
    ]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:2083
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2084
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2085
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2086
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2087
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -Infinity),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2088
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", Infinity),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2089
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -Infinity),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2090
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", Infinity),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2091
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2092
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2093
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2094
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2095
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2096
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2097
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2098
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2099
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:2100
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:2101
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2102
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2103
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2104
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2105
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2106
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2107
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2108
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2109
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2110
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2111
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.5),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2112
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.5),
    ]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:2113
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.5),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2114
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.5),
    ]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:2115
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -1),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2116
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 1),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:2117
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -1),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2118
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 1),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:2119
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -6.2831855),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2120
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 6.2831855),
    ]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:2121
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -6.2831855),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2122
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 6.2831855),
    ]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:2123
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2124
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2125
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2126
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2127
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -Infinity),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2128
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", Infinity),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2129
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -Infinity),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2130
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", Infinity),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2131
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2132
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2133
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2134
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2135
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2136
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2137
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2138
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2139
assert_return(() => invoke($0, `max`, [value("f32", -0.5), value("f32", -0)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:2140
assert_return(() => invoke($0, `max`, [value("f32", -0.5), value("f32", 0)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:2141
assert_return(() => invoke($0, `max`, [value("f32", 0.5), value("f32", -0)]), [
  value("f32", 0.5),
]);

// ./test/core/f32.wast:2142
assert_return(() => invoke($0, `max`, [value("f32", 0.5), value("f32", 0)]), [
  value("f32", 0.5),
]);

// ./test/core/f32.wast:2143
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.5),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2144
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.5),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2145
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.5),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:2146
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.5),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:2147
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.5),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2148
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.5),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2149
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.5),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:2150
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.5),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:2151
assert_return(
  () => invoke($0, `max`, [value("f32", -0.5), value("f32", -0.5)]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:2152
assert_return(
  () => invoke($0, `max`, [value("f32", -0.5), value("f32", 0.5)]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:2153
assert_return(
  () => invoke($0, `max`, [value("f32", 0.5), value("f32", -0.5)]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:2154
assert_return(() => invoke($0, `max`, [value("f32", 0.5), value("f32", 0.5)]), [
  value("f32", 0.5),
]);

// ./test/core/f32.wast:2155
assert_return(() => invoke($0, `max`, [value("f32", -0.5), value("f32", -1)]), [
  value("f32", -0.5),
]);

// ./test/core/f32.wast:2156
assert_return(() => invoke($0, `max`, [value("f32", -0.5), value("f32", 1)]), [
  value("f32", 1),
]);

// ./test/core/f32.wast:2157
assert_return(() => invoke($0, `max`, [value("f32", 0.5), value("f32", -1)]), [
  value("f32", 0.5),
]);

// ./test/core/f32.wast:2158
assert_return(() => invoke($0, `max`, [value("f32", 0.5), value("f32", 1)]), [
  value("f32", 1),
]);

// ./test/core/f32.wast:2159
assert_return(
  () => invoke($0, `max`, [value("f32", -0.5), value("f32", -6.2831855)]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:2160
assert_return(
  () => invoke($0, `max`, [value("f32", -0.5), value("f32", 6.2831855)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:2161
assert_return(
  () => invoke($0, `max`, [value("f32", 0.5), value("f32", -6.2831855)]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:2162
assert_return(
  () => invoke($0, `max`, [value("f32", 0.5), value("f32", 6.2831855)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:2163
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.5),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:2164
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.5),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2165
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.5),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:2166
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.5),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2167
assert_return(
  () => invoke($0, `max`, [value("f32", -0.5), value("f32", -Infinity)]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:2168
assert_return(
  () => invoke($0, `max`, [value("f32", -0.5), value("f32", Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2169
assert_return(
  () => invoke($0, `max`, [value("f32", 0.5), value("f32", -Infinity)]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:2170
assert_return(
  () => invoke($0, `max`, [value("f32", 0.5), value("f32", Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2171
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2172
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2173
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2174
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2175
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2176
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2177
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2178
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2179
assert_return(() => invoke($0, `max`, [value("f32", -1), value("f32", -0)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:2180
assert_return(() => invoke($0, `max`, [value("f32", -1), value("f32", 0)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:2181
assert_return(() => invoke($0, `max`, [value("f32", 1), value("f32", -0)]), [
  value("f32", 1),
]);

// ./test/core/f32.wast:2182
assert_return(() => invoke($0, `max`, [value("f32", 1), value("f32", 0)]), [
  value("f32", 1),
]);

// ./test/core/f32.wast:2183
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -1),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2184
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -1),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2185
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 1),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:2186
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 1),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:2187
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -1),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2188
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -1),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2189
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 1),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:2190
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 1),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:2191
assert_return(() => invoke($0, `max`, [value("f32", -1), value("f32", -0.5)]), [
  value("f32", -0.5),
]);

// ./test/core/f32.wast:2192
assert_return(() => invoke($0, `max`, [value("f32", -1), value("f32", 0.5)]), [
  value("f32", 0.5),
]);

// ./test/core/f32.wast:2193
assert_return(() => invoke($0, `max`, [value("f32", 1), value("f32", -0.5)]), [
  value("f32", 1),
]);

// ./test/core/f32.wast:2194
assert_return(() => invoke($0, `max`, [value("f32", 1), value("f32", 0.5)]), [
  value("f32", 1),
]);

// ./test/core/f32.wast:2195
assert_return(() => invoke($0, `max`, [value("f32", -1), value("f32", -1)]), [
  value("f32", -1),
]);

// ./test/core/f32.wast:2196
assert_return(() => invoke($0, `max`, [value("f32", -1), value("f32", 1)]), [
  value("f32", 1),
]);

// ./test/core/f32.wast:2197
assert_return(() => invoke($0, `max`, [value("f32", 1), value("f32", -1)]), [
  value("f32", 1),
]);

// ./test/core/f32.wast:2198
assert_return(() => invoke($0, `max`, [value("f32", 1), value("f32", 1)]), [
  value("f32", 1),
]);

// ./test/core/f32.wast:2199
assert_return(
  () => invoke($0, `max`, [value("f32", -1), value("f32", -6.2831855)]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:2200
assert_return(
  () => invoke($0, `max`, [value("f32", -1), value("f32", 6.2831855)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:2201
assert_return(
  () => invoke($0, `max`, [value("f32", 1), value("f32", -6.2831855)]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:2202
assert_return(
  () => invoke($0, `max`, [value("f32", 1), value("f32", 6.2831855)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:2203
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -1),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:2204
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -1),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2205
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 1),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:2206
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 1),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2207
assert_return(
  () => invoke($0, `max`, [value("f32", -1), value("f32", -Infinity)]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:2208
assert_return(
  () => invoke($0, `max`, [value("f32", -1), value("f32", Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2209
assert_return(
  () => invoke($0, `max`, [value("f32", 1), value("f32", -Infinity)]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:2210
assert_return(
  () => invoke($0, `max`, [value("f32", 1), value("f32", Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2211
assert_return(
  () =>
    invoke($0, `max`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2212
assert_return(
  () =>
    invoke($0, `max`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2213
assert_return(
  () =>
    invoke($0, `max`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2214
assert_return(
  () =>
    invoke($0, `max`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2215
assert_return(
  () =>
    invoke($0, `max`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2216
assert_return(
  () =>
    invoke($0, `max`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2217
assert_return(
  () =>
    invoke($0, `max`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2218
assert_return(
  () =>
    invoke($0, `max`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2219
assert_return(
  () => invoke($0, `max`, [value("f32", -6.2831855), value("f32", -0)]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:2220
assert_return(
  () => invoke($0, `max`, [value("f32", -6.2831855), value("f32", 0)]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:2221
assert_return(
  () => invoke($0, `max`, [value("f32", 6.2831855), value("f32", -0)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:2222
assert_return(
  () => invoke($0, `max`, [value("f32", 6.2831855), value("f32", 0)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:2223
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -6.2831855),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2224
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -6.2831855),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2225
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 6.2831855),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:2226
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 6.2831855),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:2227
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -6.2831855),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2228
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -6.2831855),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2229
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 6.2831855),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:2230
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 6.2831855),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:2231
assert_return(
  () => invoke($0, `max`, [value("f32", -6.2831855), value("f32", -0.5)]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:2232
assert_return(
  () => invoke($0, `max`, [value("f32", -6.2831855), value("f32", 0.5)]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:2233
assert_return(
  () => invoke($0, `max`, [value("f32", 6.2831855), value("f32", -0.5)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:2234
assert_return(
  () => invoke($0, `max`, [value("f32", 6.2831855), value("f32", 0.5)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:2235
assert_return(
  () => invoke($0, `max`, [value("f32", -6.2831855), value("f32", -1)]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:2236
assert_return(
  () => invoke($0, `max`, [value("f32", -6.2831855), value("f32", 1)]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:2237
assert_return(
  () => invoke($0, `max`, [value("f32", 6.2831855), value("f32", -1)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:2238
assert_return(
  () => invoke($0, `max`, [value("f32", 6.2831855), value("f32", 1)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:2239
assert_return(
  () => invoke($0, `max`, [value("f32", -6.2831855), value("f32", -6.2831855)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:2240
assert_return(
  () => invoke($0, `max`, [value("f32", -6.2831855), value("f32", 6.2831855)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:2241
assert_return(
  () => invoke($0, `max`, [value("f32", 6.2831855), value("f32", -6.2831855)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:2242
assert_return(
  () => invoke($0, `max`, [value("f32", 6.2831855), value("f32", 6.2831855)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:2243
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -6.2831855),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:2244
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -6.2831855),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2245
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 6.2831855),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:2246
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 6.2831855),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2247
assert_return(
  () => invoke($0, `max`, [value("f32", -6.2831855), value("f32", -Infinity)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:2248
assert_return(
  () => invoke($0, `max`, [value("f32", -6.2831855), value("f32", Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2249
assert_return(
  () => invoke($0, `max`, [value("f32", 6.2831855), value("f32", -Infinity)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:2250
assert_return(
  () => invoke($0, `max`, [value("f32", 6.2831855), value("f32", Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2251
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2252
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2253
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2254
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2255
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2256
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2257
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2258
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2259
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:2260
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:2261
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2262
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2263
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2264
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2265
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2266
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2267
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2268
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2269
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2270
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2271
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.5),
    ]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:2272
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.5),
    ]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:2273
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.5),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2274
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.5),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2275
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -1),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:2276
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 1),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:2277
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -1),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2278
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 1),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2279
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -6.2831855),
    ]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:2280
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 6.2831855),
    ]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:2281
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -6.2831855),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2282
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 6.2831855),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2283
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2284
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2285
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2286
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2287
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -Infinity),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2288
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", Infinity),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2289
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -Infinity),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2290
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", Infinity),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2291
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2292
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2293
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2294
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2295
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2296
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2297
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2298
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2299
assert_return(
  () => invoke($0, `max`, [value("f32", -Infinity), value("f32", -0)]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:2300
assert_return(
  () => invoke($0, `max`, [value("f32", -Infinity), value("f32", 0)]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:2301
assert_return(
  () => invoke($0, `max`, [value("f32", Infinity), value("f32", -0)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2302
assert_return(
  () => invoke($0, `max`, [value("f32", Infinity), value("f32", 0)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2303
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -Infinity),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2304
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -Infinity),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.000000000000000000000000000000000000000000001)],
);

// ./test/core/f32.wast:2305
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", Infinity),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2306
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", Infinity),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2307
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -Infinity),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2308
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -Infinity),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.000000000000000000000000000000000000011754944)],
);

// ./test/core/f32.wast:2309
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", Infinity),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2310
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", Infinity),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2311
assert_return(
  () => invoke($0, `max`, [value("f32", -Infinity), value("f32", -0.5)]),
  [value("f32", -0.5)],
);

// ./test/core/f32.wast:2312
assert_return(
  () => invoke($0, `max`, [value("f32", -Infinity), value("f32", 0.5)]),
  [value("f32", 0.5)],
);

// ./test/core/f32.wast:2313
assert_return(
  () => invoke($0, `max`, [value("f32", Infinity), value("f32", -0.5)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2314
assert_return(
  () => invoke($0, `max`, [value("f32", Infinity), value("f32", 0.5)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2315
assert_return(
  () => invoke($0, `max`, [value("f32", -Infinity), value("f32", -1)]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:2316
assert_return(
  () => invoke($0, `max`, [value("f32", -Infinity), value("f32", 1)]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:2317
assert_return(
  () => invoke($0, `max`, [value("f32", Infinity), value("f32", -1)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2318
assert_return(
  () => invoke($0, `max`, [value("f32", Infinity), value("f32", 1)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2319
assert_return(
  () => invoke($0, `max`, [value("f32", -Infinity), value("f32", -6.2831855)]),
  [value("f32", -6.2831855)],
);

// ./test/core/f32.wast:2320
assert_return(
  () => invoke($0, `max`, [value("f32", -Infinity), value("f32", 6.2831855)]),
  [value("f32", 6.2831855)],
);

// ./test/core/f32.wast:2321
assert_return(
  () => invoke($0, `max`, [value("f32", Infinity), value("f32", -6.2831855)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2322
assert_return(
  () => invoke($0, `max`, [value("f32", Infinity), value("f32", 6.2831855)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2323
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -Infinity),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2324
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -Infinity),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2325
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", Infinity),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2326
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", Infinity),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2327
assert_return(
  () => invoke($0, `max`, [value("f32", -Infinity), value("f32", -Infinity)]),
  [value("f32", -Infinity)],
);

// ./test/core/f32.wast:2328
assert_return(
  () => invoke($0, `max`, [value("f32", -Infinity), value("f32", Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2329
assert_return(
  () => invoke($0, `max`, [value("f32", Infinity), value("f32", -Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2330
assert_return(
  () => invoke($0, `max`, [value("f32", Infinity), value("f32", Infinity)]),
  [value("f32", Infinity)],
);

// ./test/core/f32.wast:2331
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2332
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2333
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2334
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2335
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2336
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2337
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2338
assert_return(
  () =>
    invoke($0, `max`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2339
assert_return(
  () =>
    invoke($0, `max`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", -0)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2340
assert_return(
  () =>
    invoke($0, `max`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", -0)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2341
assert_return(
  () =>
    invoke($0, `max`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 0)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2342
assert_return(
  () =>
    invoke($0, `max`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 0)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2343
assert_return(
  () =>
    invoke($0, `max`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", -0)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2344
assert_return(
  () =>
    invoke($0, `max`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", -0)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2345
assert_return(
  () =>
    invoke($0, `max`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 0)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2346
assert_return(
  () =>
    invoke($0, `max`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 0)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2347
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2348
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2349
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2350
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2351
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2352
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2353
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2354
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2355
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2356
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2357
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2358
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2359
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2360
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2361
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2362
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2363
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.5),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2364
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.5),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2365
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.5),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2366
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.5),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2367
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.5),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2368
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.5),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2369
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.5),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2370
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.5),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2371
assert_return(
  () =>
    invoke($0, `max`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", -1)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2372
assert_return(
  () =>
    invoke($0, `max`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", -1)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2373
assert_return(
  () =>
    invoke($0, `max`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 1)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2374
assert_return(
  () =>
    invoke($0, `max`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 1)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2375
assert_return(
  () =>
    invoke($0, `max`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", -1)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2376
assert_return(
  () =>
    invoke($0, `max`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", -1)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2377
assert_return(
  () =>
    invoke($0, `max`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 1)]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2378
assert_return(
  () =>
    invoke($0, `max`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 1)]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2379
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -6.2831855),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2380
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -6.2831855),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2381
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 6.2831855),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2382
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 6.2831855),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2383
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -6.2831855),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2384
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -6.2831855),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2385
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 6.2831855),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2386
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 6.2831855),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2387
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2388
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2389
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2390
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2391
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2392
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2393
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2394
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2395
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -Infinity),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2396
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -Infinity),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2397
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", Infinity),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2398
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", Infinity),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2399
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -Infinity),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2400
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -Infinity),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2401
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", Infinity),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2402
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", Infinity),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2403
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2404
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2405
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2406
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2407
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2408
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2409
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2410
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2411
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2412
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2413
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2414
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2415
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2416
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2417
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2418
assert_return(
  () =>
    invoke($0, `max`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2419
assert_return(() => invoke($0, `sqrt`, [value("f32", -0)]), [value("f32", -0)]);

// ./test/core/f32.wast:2420
assert_return(() => invoke($0, `sqrt`, [value("f32", 0)]), [value("f32", 0)]);

// ./test/core/f32.wast:2421
assert_return(
  () =>
    invoke($0, `sqrt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2422
assert_return(
  () =>
    invoke($0, `sqrt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0.00000000000000000000003743392)],
);

// ./test/core/f32.wast:2423
assert_return(
  () =>
    invoke($0, `sqrt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2424
assert_return(
  () =>
    invoke($0, `sqrt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0.00000000000000000010842022)],
);

// ./test/core/f32.wast:2425
assert_return(() => invoke($0, `sqrt`, [value("f32", -0.5)]), [
  `canonical_nan`,
]);

// ./test/core/f32.wast:2426
assert_return(() => invoke($0, `sqrt`, [value("f32", 0.5)]), [
  value("f32", 0.70710677),
]);

// ./test/core/f32.wast:2427
assert_return(() => invoke($0, `sqrt`, [value("f32", -1)]), [`canonical_nan`]);

// ./test/core/f32.wast:2428
assert_return(() => invoke($0, `sqrt`, [value("f32", 1)]), [value("f32", 1)]);

// ./test/core/f32.wast:2429
assert_return(() => invoke($0, `sqrt`, [value("f32", -6.2831855)]), [
  `canonical_nan`,
]);

// ./test/core/f32.wast:2430
assert_return(() => invoke($0, `sqrt`, [value("f32", 6.2831855)]), [
  value("f32", 2.5066283),
]);

// ./test/core/f32.wast:2431
assert_return(
  () =>
    invoke($0, `sqrt`, [
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2432
assert_return(
  () =>
    invoke($0, `sqrt`, [value("f32", 340282350000000000000000000000000000000)]),
  [value("f32", 18446743000000000000)],
);

// ./test/core/f32.wast:2433
assert_return(() => invoke($0, `sqrt`, [value("f32", -Infinity)]), [
  `canonical_nan`,
]);

// ./test/core/f32.wast:2434
assert_return(() => invoke($0, `sqrt`, [value("f32", Infinity)]), [
  value("f32", Infinity),
]);

// ./test/core/f32.wast:2435
assert_return(
  () => invoke($0, `sqrt`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2436
assert_return(
  () => invoke($0, `sqrt`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2437
assert_return(
  () => invoke($0, `sqrt`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2438
assert_return(
  () => invoke($0, `sqrt`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2439
assert_return(() => invoke($0, `floor`, [value("f32", -0)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:2440
assert_return(() => invoke($0, `floor`, [value("f32", 0)]), [value("f32", 0)]);

// ./test/core/f32.wast:2441
assert_return(
  () =>
    invoke($0, `floor`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:2442
assert_return(
  () =>
    invoke($0, `floor`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:2443
assert_return(
  () =>
    invoke($0, `floor`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -1)],
);

// ./test/core/f32.wast:2444
assert_return(
  () =>
    invoke($0, `floor`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:2445
assert_return(() => invoke($0, `floor`, [value("f32", -0.5)]), [
  value("f32", -1),
]);

// ./test/core/f32.wast:2446
assert_return(() => invoke($0, `floor`, [value("f32", 0.5)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:2447
assert_return(() => invoke($0, `floor`, [value("f32", -1)]), [
  value("f32", -1),
]);

// ./test/core/f32.wast:2448
assert_return(() => invoke($0, `floor`, [value("f32", 1)]), [value("f32", 1)]);

// ./test/core/f32.wast:2449
assert_return(() => invoke($0, `floor`, [value("f32", -6.2831855)]), [
  value("f32", -7),
]);

// ./test/core/f32.wast:2450
assert_return(() => invoke($0, `floor`, [value("f32", 6.2831855)]), [
  value("f32", 6),
]);

// ./test/core/f32.wast:2451
assert_return(
  () =>
    invoke($0, `floor`, [
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2452
assert_return(
  () =>
    invoke($0, `floor`, [
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2453
assert_return(() => invoke($0, `floor`, [value("f32", -Infinity)]), [
  value("f32", -Infinity),
]);

// ./test/core/f32.wast:2454
assert_return(() => invoke($0, `floor`, [value("f32", Infinity)]), [
  value("f32", Infinity),
]);

// ./test/core/f32.wast:2455
assert_return(
  () => invoke($0, `floor`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2456
assert_return(
  () => invoke($0, `floor`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2457
assert_return(
  () => invoke($0, `floor`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2458
assert_return(
  () => invoke($0, `floor`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2459
assert_return(() => invoke($0, `ceil`, [value("f32", -0)]), [value("f32", -0)]);

// ./test/core/f32.wast:2460
assert_return(() => invoke($0, `ceil`, [value("f32", 0)]), [value("f32", 0)]);

// ./test/core/f32.wast:2461
assert_return(
  () =>
    invoke($0, `ceil`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:2462
assert_return(
  () =>
    invoke($0, `ceil`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:2463
assert_return(
  () =>
    invoke($0, `ceil`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:2464
assert_return(
  () =>
    invoke($0, `ceil`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 1)],
);

// ./test/core/f32.wast:2465
assert_return(() => invoke($0, `ceil`, [value("f32", -0.5)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:2466
assert_return(() => invoke($0, `ceil`, [value("f32", 0.5)]), [value("f32", 1)]);

// ./test/core/f32.wast:2467
assert_return(() => invoke($0, `ceil`, [value("f32", -1)]), [value("f32", -1)]);

// ./test/core/f32.wast:2468
assert_return(() => invoke($0, `ceil`, [value("f32", 1)]), [value("f32", 1)]);

// ./test/core/f32.wast:2469
assert_return(() => invoke($0, `ceil`, [value("f32", -6.2831855)]), [
  value("f32", -6),
]);

// ./test/core/f32.wast:2470
assert_return(() => invoke($0, `ceil`, [value("f32", 6.2831855)]), [
  value("f32", 7),
]);

// ./test/core/f32.wast:2471
assert_return(
  () =>
    invoke($0, `ceil`, [
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2472
assert_return(
  () =>
    invoke($0, `ceil`, [value("f32", 340282350000000000000000000000000000000)]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2473
assert_return(() => invoke($0, `ceil`, [value("f32", -Infinity)]), [
  value("f32", -Infinity),
]);

// ./test/core/f32.wast:2474
assert_return(() => invoke($0, `ceil`, [value("f32", Infinity)]), [
  value("f32", Infinity),
]);

// ./test/core/f32.wast:2475
assert_return(
  () => invoke($0, `ceil`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2476
assert_return(
  () => invoke($0, `ceil`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2477
assert_return(
  () => invoke($0, `ceil`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2478
assert_return(
  () => invoke($0, `ceil`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2479
assert_return(() => invoke($0, `trunc`, [value("f32", -0)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:2480
assert_return(() => invoke($0, `trunc`, [value("f32", 0)]), [value("f32", 0)]);

// ./test/core/f32.wast:2481
assert_return(
  () =>
    invoke($0, `trunc`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:2482
assert_return(
  () =>
    invoke($0, `trunc`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:2483
assert_return(
  () =>
    invoke($0, `trunc`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:2484
assert_return(
  () =>
    invoke($0, `trunc`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:2485
assert_return(() => invoke($0, `trunc`, [value("f32", -0.5)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:2486
assert_return(() => invoke($0, `trunc`, [value("f32", 0.5)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:2487
assert_return(() => invoke($0, `trunc`, [value("f32", -1)]), [
  value("f32", -1),
]);

// ./test/core/f32.wast:2488
assert_return(() => invoke($0, `trunc`, [value("f32", 1)]), [value("f32", 1)]);

// ./test/core/f32.wast:2489
assert_return(() => invoke($0, `trunc`, [value("f32", -6.2831855)]), [
  value("f32", -6),
]);

// ./test/core/f32.wast:2490
assert_return(() => invoke($0, `trunc`, [value("f32", 6.2831855)]), [
  value("f32", 6),
]);

// ./test/core/f32.wast:2491
assert_return(
  () =>
    invoke($0, `trunc`, [
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2492
assert_return(
  () =>
    invoke($0, `trunc`, [
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2493
assert_return(() => invoke($0, `trunc`, [value("f32", -Infinity)]), [
  value("f32", -Infinity),
]);

// ./test/core/f32.wast:2494
assert_return(() => invoke($0, `trunc`, [value("f32", Infinity)]), [
  value("f32", Infinity),
]);

// ./test/core/f32.wast:2495
assert_return(
  () => invoke($0, `trunc`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2496
assert_return(
  () => invoke($0, `trunc`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2497
assert_return(
  () => invoke($0, `trunc`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2498
assert_return(
  () => invoke($0, `trunc`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2499
assert_return(() => invoke($0, `nearest`, [value("f32", -0)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:2500
assert_return(() => invoke($0, `nearest`, [value("f32", 0)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:2501
assert_return(
  () =>
    invoke($0, `nearest`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:2502
assert_return(
  () =>
    invoke($0, `nearest`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:2503
assert_return(
  () =>
    invoke($0, `nearest`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", -0)],
);

// ./test/core/f32.wast:2504
assert_return(
  () =>
    invoke($0, `nearest`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("f32", 0)],
);

// ./test/core/f32.wast:2505
assert_return(() => invoke($0, `nearest`, [value("f32", -0.5)]), [
  value("f32", -0),
]);

// ./test/core/f32.wast:2506
assert_return(() => invoke($0, `nearest`, [value("f32", 0.5)]), [
  value("f32", 0),
]);

// ./test/core/f32.wast:2507
assert_return(() => invoke($0, `nearest`, [value("f32", -1)]), [
  value("f32", -1),
]);

// ./test/core/f32.wast:2508
assert_return(() => invoke($0, `nearest`, [value("f32", 1)]), [
  value("f32", 1),
]);

// ./test/core/f32.wast:2509
assert_return(() => invoke($0, `nearest`, [value("f32", -6.2831855)]), [
  value("f32", -6),
]);

// ./test/core/f32.wast:2510
assert_return(() => invoke($0, `nearest`, [value("f32", 6.2831855)]), [
  value("f32", 6),
]);

// ./test/core/f32.wast:2511
assert_return(
  () =>
    invoke($0, `nearest`, [
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("f32", -340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2512
assert_return(
  () =>
    invoke($0, `nearest`, [
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("f32", 340282350000000000000000000000000000000)],
);

// ./test/core/f32.wast:2513
assert_return(() => invoke($0, `nearest`, [value("f32", -Infinity)]), [
  value("f32", -Infinity),
]);

// ./test/core/f32.wast:2514
assert_return(() => invoke($0, `nearest`, [value("f32", Infinity)]), [
  value("f32", Infinity),
]);

// ./test/core/f32.wast:2515
assert_return(
  () => invoke($0, `nearest`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2516
assert_return(
  () => invoke($0, `nearest`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2517
assert_return(
  () => invoke($0, `nearest`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [`canonical_nan`],
);

// ./test/core/f32.wast:2518
assert_return(
  () => invoke($0, `nearest`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [`arithmetic_nan`],
);

// ./test/core/f32.wast:2523
assert_invalid(
  () =>
    instantiate(
      `(module (func (result f32) (f32.add (i64.const 0) (f64.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/f32.wast:2524
assert_invalid(
  () =>
    instantiate(
      `(module (func (result f32) (f32.div (i64.const 0) (f64.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/f32.wast:2525
assert_invalid(
  () =>
    instantiate(
      `(module (func (result f32) (f32.max (i64.const 0) (f64.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/f32.wast:2526
assert_invalid(
  () =>
    instantiate(
      `(module (func (result f32) (f32.min (i64.const 0) (f64.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/f32.wast:2527
assert_invalid(
  () =>
    instantiate(
      `(module (func (result f32) (f32.mul (i64.const 0) (f64.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/f32.wast:2528
assert_invalid(
  () =>
    instantiate(
      `(module (func (result f32) (f32.sub (i64.const 0) (f64.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/f32.wast:2529
assert_invalid(
  () => instantiate(`(module (func (result f32) (f32.ceil (i64.const 0))))`),
  `type mismatch`,
);

// ./test/core/f32.wast:2530
assert_invalid(
  () => instantiate(`(module (func (result f32) (f32.floor (i64.const 0))))`),
  `type mismatch`,
);

// ./test/core/f32.wast:2531
assert_invalid(
  () => instantiate(`(module (func (result f32) (f32.nearest (i64.const 0))))`),
  `type mismatch`,
);

// ./test/core/f32.wast:2532
assert_invalid(
  () => instantiate(`(module (func (result f32) (f32.sqrt (i64.const 0))))`),
  `type mismatch`,
);

// ./test/core/f32.wast:2533
assert_invalid(
  () => instantiate(`(module (func (result f32) (f32.trunc (i64.const 0))))`),
  `type mismatch`,
);

// ./test/core/f32.wast:2536
assert_malformed(
  () => instantiate(`(func (result f32) (f32.const nan:arithmetic)) `),
  `unexpected token`,
);

// ./test/core/f32.wast:2540
assert_malformed(
  () => instantiate(`(func (result f32) (f32.const nan:canonical)) `),
  `unexpected token`,
);
