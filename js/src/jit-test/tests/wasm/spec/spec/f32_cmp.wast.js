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

// ./test/core/f32_cmp.wast

// ./test/core/f32_cmp.wast:4
let $0 = instantiate(`(module
  (func (export "eq") (param $$x f32) (param $$y f32) (result i32) (f32.eq (local.get $$x) (local.get $$y)))
  (func (export "ne") (param $$x f32) (param $$y f32) (result i32) (f32.ne (local.get $$x) (local.get $$y)))
  (func (export "lt") (param $$x f32) (param $$y f32) (result i32) (f32.lt (local.get $$x) (local.get $$y)))
  (func (export "le") (param $$x f32) (param $$y f32) (result i32) (f32.le (local.get $$x) (local.get $$y)))
  (func (export "gt") (param $$x f32) (param $$y f32) (result i32) (f32.gt (local.get $$x) (local.get $$y)))
  (func (export "ge") (param $$x f32) (param $$y f32) (result i32) (f32.ge (local.get $$x) (local.get $$y)))
)`);

// ./test/core/f32_cmp.wast:13
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:14
assert_return(
  () =>
    invoke($0, `eq`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", 0)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:15
assert_return(
  () =>
    invoke($0, `eq`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:16
assert_return(() => invoke($0, `eq`, [value("f32", 0), value("f32", 0)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:17
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:18
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:19
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:20
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:21
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:22
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:23
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:24
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:25
assert_return(
  () =>
    invoke($0, `eq`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", -0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:26
assert_return(
  () =>
    invoke($0, `eq`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:27
assert_return(() => invoke($0, `eq`, [value("f32", 0), value("f32", -0.5)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:28
assert_return(() => invoke($0, `eq`, [value("f32", 0), value("f32", 0.5)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:29
assert_return(
  () =>
    invoke($0, `eq`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:30
assert_return(
  () =>
    invoke($0, `eq`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:31
assert_return(() => invoke($0, `eq`, [value("f32", 0), value("f32", -1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:32
assert_return(() => invoke($0, `eq`, [value("f32", 0), value("f32", 1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:33
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:34
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:35
assert_return(
  () => invoke($0, `eq`, [value("f32", 0), value("f32", -6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:36
assert_return(
  () => invoke($0, `eq`, [value("f32", 0), value("f32", 6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:37
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:38
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:39
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:40
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:41
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:42
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:43
assert_return(
  () => invoke($0, `eq`, [value("f32", 0), value("f32", -Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:44
assert_return(
  () => invoke($0, `eq`, [value("f32", 0), value("f32", Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:45
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:46
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:47
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:48
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:49
assert_return(
  () =>
    invoke($0, `eq`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:50
assert_return(
  () =>
    invoke($0, `eq`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:51
assert_return(
  () =>
    invoke($0, `eq`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:52
assert_return(
  () =>
    invoke($0, `eq`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:53
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:54
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:55
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:56
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:57
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:58
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:59
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:60
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:61
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:62
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:63
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:64
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:65
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:66
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:67
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:68
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:69
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:70
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:71
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:72
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:73
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:74
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:75
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:76
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:77
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:78
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:79
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:80
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:81
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:82
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:83
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:84
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:85
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:86
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:87
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:88
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:89
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:90
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:91
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:92
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:93
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:94
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:95
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:96
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:97
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:98
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:99
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:100
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:101
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:102
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:103
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:104
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:105
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:106
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:107
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:108
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:109
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:110
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:111
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:112
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:113
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:114
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:115
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:116
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:117
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:118
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:119
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:120
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:121
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:122
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:123
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:124
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:125
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:126
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:127
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:128
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:129
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:130
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:131
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:132
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:133
assert_return(
  () =>
    invoke($0, `eq`, [value("f32", -0.5), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:134
assert_return(() => invoke($0, `eq`, [value("f32", -0.5), value("f32", 0)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:135
assert_return(
  () =>
    invoke($0, `eq`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:136
assert_return(() => invoke($0, `eq`, [value("f32", 0.5), value("f32", 0)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:137
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.5),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:138
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.5),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:139
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.5),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:140
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.5),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:141
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.5),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:142
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.5),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:143
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.5),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:144
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.5),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:145
assert_return(
  () => invoke($0, `eq`, [value("f32", -0.5), value("f32", -0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:146
assert_return(() => invoke($0, `eq`, [value("f32", -0.5), value("f32", 0.5)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:147
assert_return(() => invoke($0, `eq`, [value("f32", 0.5), value("f32", -0.5)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:148
assert_return(() => invoke($0, `eq`, [value("f32", 0.5), value("f32", 0.5)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:149
assert_return(() => invoke($0, `eq`, [value("f32", -0.5), value("f32", -1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:150
assert_return(() => invoke($0, `eq`, [value("f32", -0.5), value("f32", 1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:151
assert_return(() => invoke($0, `eq`, [value("f32", 0.5), value("f32", -1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:152
assert_return(() => invoke($0, `eq`, [value("f32", 0.5), value("f32", 1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:153
assert_return(
  () => invoke($0, `eq`, [value("f32", -0.5), value("f32", -6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:154
assert_return(
  () => invoke($0, `eq`, [value("f32", -0.5), value("f32", 6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:155
assert_return(
  () => invoke($0, `eq`, [value("f32", 0.5), value("f32", -6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:156
assert_return(
  () => invoke($0, `eq`, [value("f32", 0.5), value("f32", 6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:157
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.5),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:158
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.5),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:159
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.5),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:160
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 0.5),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:161
assert_return(
  () => invoke($0, `eq`, [value("f32", -0.5), value("f32", -Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:162
assert_return(
  () => invoke($0, `eq`, [value("f32", -0.5), value("f32", Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:163
assert_return(
  () => invoke($0, `eq`, [value("f32", 0.5), value("f32", -Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:164
assert_return(
  () => invoke($0, `eq`, [value("f32", 0.5), value("f32", Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:165
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:166
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:167
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:168
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:169
assert_return(
  () =>
    invoke($0, `eq`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:170
assert_return(
  () =>
    invoke($0, `eq`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:171
assert_return(
  () =>
    invoke($0, `eq`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:172
assert_return(
  () =>
    invoke($0, `eq`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:173
assert_return(
  () =>
    invoke($0, `eq`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:174
assert_return(() => invoke($0, `eq`, [value("f32", -1), value("f32", 0)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:175
assert_return(
  () =>
    invoke($0, `eq`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:176
assert_return(() => invoke($0, `eq`, [value("f32", 1), value("f32", 0)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:177
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -1),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:178
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -1),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:179
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 1),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:180
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 1),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:181
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -1),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:182
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -1),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:183
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 1),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:184
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 1),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:185
assert_return(() => invoke($0, `eq`, [value("f32", -1), value("f32", -0.5)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:186
assert_return(() => invoke($0, `eq`, [value("f32", -1), value("f32", 0.5)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:187
assert_return(() => invoke($0, `eq`, [value("f32", 1), value("f32", -0.5)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:188
assert_return(() => invoke($0, `eq`, [value("f32", 1), value("f32", 0.5)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:189
assert_return(() => invoke($0, `eq`, [value("f32", -1), value("f32", -1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:190
assert_return(() => invoke($0, `eq`, [value("f32", -1), value("f32", 1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:191
assert_return(() => invoke($0, `eq`, [value("f32", 1), value("f32", -1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:192
assert_return(() => invoke($0, `eq`, [value("f32", 1), value("f32", 1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:193
assert_return(
  () => invoke($0, `eq`, [value("f32", -1), value("f32", -6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:194
assert_return(
  () => invoke($0, `eq`, [value("f32", -1), value("f32", 6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:195
assert_return(
  () => invoke($0, `eq`, [value("f32", 1), value("f32", -6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:196
assert_return(
  () => invoke($0, `eq`, [value("f32", 1), value("f32", 6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:197
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -1),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:198
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -1),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:199
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 1),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:200
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 1),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:201
assert_return(
  () => invoke($0, `eq`, [value("f32", -1), value("f32", -Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:202
assert_return(
  () => invoke($0, `eq`, [value("f32", -1), value("f32", Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:203
assert_return(
  () => invoke($0, `eq`, [value("f32", 1), value("f32", -Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:204
assert_return(
  () => invoke($0, `eq`, [value("f32", 1), value("f32", Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:205
assert_return(
  () =>
    invoke($0, `eq`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:206
assert_return(
  () =>
    invoke($0, `eq`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:207
assert_return(
  () =>
    invoke($0, `eq`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:208
assert_return(
  () =>
    invoke($0, `eq`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:209
assert_return(
  () =>
    invoke($0, `eq`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:210
assert_return(
  () =>
    invoke($0, `eq`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:211
assert_return(
  () =>
    invoke($0, `eq`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:212
assert_return(
  () =>
    invoke($0, `eq`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:213
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:214
assert_return(
  () => invoke($0, `eq`, [value("f32", -6.2831855), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:215
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:216
assert_return(
  () => invoke($0, `eq`, [value("f32", 6.2831855), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:217
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -6.2831855),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:218
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -6.2831855),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:219
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 6.2831855),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:220
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 6.2831855),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:221
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -6.2831855),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:222
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -6.2831855),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:223
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 6.2831855),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:224
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 6.2831855),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:225
assert_return(
  () => invoke($0, `eq`, [value("f32", -6.2831855), value("f32", -0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:226
assert_return(
  () => invoke($0, `eq`, [value("f32", -6.2831855), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:227
assert_return(
  () => invoke($0, `eq`, [value("f32", 6.2831855), value("f32", -0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:228
assert_return(
  () => invoke($0, `eq`, [value("f32", 6.2831855), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:229
assert_return(
  () => invoke($0, `eq`, [value("f32", -6.2831855), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:230
assert_return(
  () => invoke($0, `eq`, [value("f32", -6.2831855), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:231
assert_return(
  () => invoke($0, `eq`, [value("f32", 6.2831855), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:232
assert_return(
  () => invoke($0, `eq`, [value("f32", 6.2831855), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:233
assert_return(
  () => invoke($0, `eq`, [value("f32", -6.2831855), value("f32", -6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:234
assert_return(
  () => invoke($0, `eq`, [value("f32", -6.2831855), value("f32", 6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:235
assert_return(
  () => invoke($0, `eq`, [value("f32", 6.2831855), value("f32", -6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:236
assert_return(
  () => invoke($0, `eq`, [value("f32", 6.2831855), value("f32", 6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:237
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -6.2831855),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:238
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -6.2831855),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:239
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 6.2831855),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:240
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 6.2831855),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:241
assert_return(
  () => invoke($0, `eq`, [value("f32", -6.2831855), value("f32", -Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:242
assert_return(
  () => invoke($0, `eq`, [value("f32", -6.2831855), value("f32", Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:243
assert_return(
  () => invoke($0, `eq`, [value("f32", 6.2831855), value("f32", -Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:244
assert_return(
  () => invoke($0, `eq`, [value("f32", 6.2831855), value("f32", Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:245
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:246
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:247
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:248
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:249
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:250
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:251
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:252
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:253
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:254
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:255
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:256
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:257
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:258
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:259
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:260
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:261
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:262
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:263
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:264
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:265
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:266
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:267
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:268
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:269
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:270
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:271
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:272
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:273
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:274
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:275
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:276
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:277
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:278
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:279
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:280
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:281
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:282
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:283
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:284
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:285
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:286
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:287
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:288
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:289
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:290
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:291
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:292
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:293
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:294
assert_return(
  () => invoke($0, `eq`, [value("f32", -Infinity), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:295
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:296
assert_return(
  () => invoke($0, `eq`, [value("f32", Infinity), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:297
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -Infinity),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:298
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -Infinity),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:299
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", Infinity),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:300
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", Infinity),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:301
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -Infinity),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:302
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -Infinity),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:303
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", Infinity),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:304
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", Infinity),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:305
assert_return(
  () => invoke($0, `eq`, [value("f32", -Infinity), value("f32", -0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:306
assert_return(
  () => invoke($0, `eq`, [value("f32", -Infinity), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:307
assert_return(
  () => invoke($0, `eq`, [value("f32", Infinity), value("f32", -0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:308
assert_return(
  () => invoke($0, `eq`, [value("f32", Infinity), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:309
assert_return(
  () => invoke($0, `eq`, [value("f32", -Infinity), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:310
assert_return(
  () => invoke($0, `eq`, [value("f32", -Infinity), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:311
assert_return(
  () => invoke($0, `eq`, [value("f32", Infinity), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:312
assert_return(
  () => invoke($0, `eq`, [value("f32", Infinity), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:313
assert_return(
  () => invoke($0, `eq`, [value("f32", -Infinity), value("f32", -6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:314
assert_return(
  () => invoke($0, `eq`, [value("f32", -Infinity), value("f32", 6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:315
assert_return(
  () => invoke($0, `eq`, [value("f32", Infinity), value("f32", -6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:316
assert_return(
  () => invoke($0, `eq`, [value("f32", Infinity), value("f32", 6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:317
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -Infinity),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:318
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -Infinity),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:319
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", Infinity),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:320
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", Infinity),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:321
assert_return(
  () => invoke($0, `eq`, [value("f32", -Infinity), value("f32", -Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:322
assert_return(
  () => invoke($0, `eq`, [value("f32", -Infinity), value("f32", Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:323
assert_return(
  () => invoke($0, `eq`, [value("f32", Infinity), value("f32", -Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:324
assert_return(
  () => invoke($0, `eq`, [value("f32", Infinity), value("f32", Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:325
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:326
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:327
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:328
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:329
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:330
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:331
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:332
assert_return(
  () =>
    invoke($0, `eq`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:333
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:334
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:335
assert_return(
  () =>
    invoke($0, `eq`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:336
assert_return(
  () =>
    invoke($0, `eq`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:337
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:338
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:339
assert_return(
  () =>
    invoke($0, `eq`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:340
assert_return(
  () =>
    invoke($0, `eq`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:341
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:342
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:343
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:344
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:345
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:346
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:347
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:348
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:349
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:350
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:351
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:352
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:353
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:354
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:355
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:356
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:357
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:358
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:359
assert_return(
  () =>
    invoke($0, `eq`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:360
assert_return(
  () =>
    invoke($0, `eq`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:361
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:362
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:363
assert_return(
  () =>
    invoke($0, `eq`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:364
assert_return(
  () =>
    invoke($0, `eq`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:365
assert_return(
  () =>
    invoke($0, `eq`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:366
assert_return(
  () =>
    invoke($0, `eq`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:367
assert_return(
  () =>
    invoke($0, `eq`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:368
assert_return(
  () =>
    invoke($0, `eq`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:369
assert_return(
  () =>
    invoke($0, `eq`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:370
assert_return(
  () =>
    invoke($0, `eq`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:371
assert_return(
  () =>
    invoke($0, `eq`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:372
assert_return(
  () =>
    invoke($0, `eq`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:373
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:374
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:375
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:376
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:377
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:378
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:379
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:380
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:381
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:382
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:383
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:384
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:385
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:386
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:387
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:388
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:389
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:390
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:391
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:392
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:393
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:394
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:395
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:396
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:397
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:398
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:399
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:400
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:401
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:402
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:403
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:404
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:405
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:406
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:407
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:408
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:409
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:410
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:411
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:412
assert_return(
  () =>
    invoke($0, `eq`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:413
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:414
assert_return(
  () =>
    invoke($0, `ne`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:415
assert_return(
  () =>
    invoke($0, `ne`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:416
assert_return(() => invoke($0, `ne`, [value("f32", 0), value("f32", 0)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:417
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:418
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:419
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:420
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:421
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:422
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:423
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:424
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:425
assert_return(
  () =>
    invoke($0, `ne`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", -0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:426
assert_return(
  () =>
    invoke($0, `ne`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", 0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:427
assert_return(() => invoke($0, `ne`, [value("f32", 0), value("f32", -0.5)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:428
assert_return(() => invoke($0, `ne`, [value("f32", 0), value("f32", 0.5)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:429
assert_return(
  () =>
    invoke($0, `ne`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", -1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:430
assert_return(
  () =>
    invoke($0, `ne`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", 1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:431
assert_return(() => invoke($0, `ne`, [value("f32", 0), value("f32", -1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:432
assert_return(() => invoke($0, `ne`, [value("f32", 0), value("f32", 1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:433
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:434
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:435
assert_return(
  () => invoke($0, `ne`, [value("f32", 0), value("f32", -6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:436
assert_return(
  () => invoke($0, `ne`, [value("f32", 0), value("f32", 6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:437
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:438
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:439
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:440
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:441
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:442
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:443
assert_return(
  () => invoke($0, `ne`, [value("f32", 0), value("f32", -Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:444
assert_return(
  () => invoke($0, `ne`, [value("f32", 0), value("f32", Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:445
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:446
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:447
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:448
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:449
assert_return(
  () =>
    invoke($0, `ne`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:450
assert_return(
  () =>
    invoke($0, `ne`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:451
assert_return(
  () =>
    invoke($0, `ne`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:452
assert_return(
  () =>
    invoke($0, `ne`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:453
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:454
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:455
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:456
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:457
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:458
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:459
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:460
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:461
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:462
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:463
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:464
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:465
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:466
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:467
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:468
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:469
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:470
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:471
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:472
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:473
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:474
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:475
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:476
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:477
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:478
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:479
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:480
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:481
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:482
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:483
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:484
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:485
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:486
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:487
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:488
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:489
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:490
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:491
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:492
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:493
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:494
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:495
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:496
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:497
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:498
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:499
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:500
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:501
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:502
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:503
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:504
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:505
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:506
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:507
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:508
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:509
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:510
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:511
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:512
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:513
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:514
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:515
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:516
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:517
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:518
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:519
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:520
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:521
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:522
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:523
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:524
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:525
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:526
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:527
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:528
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:529
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:530
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:531
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:532
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:533
assert_return(
  () =>
    invoke($0, `ne`, [value("f32", -0.5), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:534
assert_return(() => invoke($0, `ne`, [value("f32", -0.5), value("f32", 0)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:535
assert_return(
  () =>
    invoke($0, `ne`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:536
assert_return(() => invoke($0, `ne`, [value("f32", 0.5), value("f32", 0)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:537
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.5),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:538
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.5),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:539
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.5),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:540
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.5),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:541
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.5),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:542
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.5),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:543
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.5),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:544
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.5),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:545
assert_return(
  () => invoke($0, `ne`, [value("f32", -0.5), value("f32", -0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:546
assert_return(() => invoke($0, `ne`, [value("f32", -0.5), value("f32", 0.5)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:547
assert_return(() => invoke($0, `ne`, [value("f32", 0.5), value("f32", -0.5)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:548
assert_return(() => invoke($0, `ne`, [value("f32", 0.5), value("f32", 0.5)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:549
assert_return(() => invoke($0, `ne`, [value("f32", -0.5), value("f32", -1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:550
assert_return(() => invoke($0, `ne`, [value("f32", -0.5), value("f32", 1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:551
assert_return(() => invoke($0, `ne`, [value("f32", 0.5), value("f32", -1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:552
assert_return(() => invoke($0, `ne`, [value("f32", 0.5), value("f32", 1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:553
assert_return(
  () => invoke($0, `ne`, [value("f32", -0.5), value("f32", -6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:554
assert_return(
  () => invoke($0, `ne`, [value("f32", -0.5), value("f32", 6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:555
assert_return(
  () => invoke($0, `ne`, [value("f32", 0.5), value("f32", -6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:556
assert_return(
  () => invoke($0, `ne`, [value("f32", 0.5), value("f32", 6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:557
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.5),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:558
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.5),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:559
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.5),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:560
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 0.5),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:561
assert_return(
  () => invoke($0, `ne`, [value("f32", -0.5), value("f32", -Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:562
assert_return(
  () => invoke($0, `ne`, [value("f32", -0.5), value("f32", Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:563
assert_return(
  () => invoke($0, `ne`, [value("f32", 0.5), value("f32", -Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:564
assert_return(
  () => invoke($0, `ne`, [value("f32", 0.5), value("f32", Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:565
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:566
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:567
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:568
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:569
assert_return(
  () =>
    invoke($0, `ne`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:570
assert_return(
  () =>
    invoke($0, `ne`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:571
assert_return(
  () =>
    invoke($0, `ne`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:572
assert_return(
  () =>
    invoke($0, `ne`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:573
assert_return(
  () =>
    invoke($0, `ne`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:574
assert_return(() => invoke($0, `ne`, [value("f32", -1), value("f32", 0)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:575
assert_return(
  () =>
    invoke($0, `ne`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:576
assert_return(() => invoke($0, `ne`, [value("f32", 1), value("f32", 0)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:577
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -1),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:578
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -1),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:579
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 1),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:580
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 1),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:581
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -1),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:582
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -1),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:583
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 1),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:584
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 1),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:585
assert_return(() => invoke($0, `ne`, [value("f32", -1), value("f32", -0.5)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:586
assert_return(() => invoke($0, `ne`, [value("f32", -1), value("f32", 0.5)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:587
assert_return(() => invoke($0, `ne`, [value("f32", 1), value("f32", -0.5)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:588
assert_return(() => invoke($0, `ne`, [value("f32", 1), value("f32", 0.5)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:589
assert_return(() => invoke($0, `ne`, [value("f32", -1), value("f32", -1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:590
assert_return(() => invoke($0, `ne`, [value("f32", -1), value("f32", 1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:591
assert_return(() => invoke($0, `ne`, [value("f32", 1), value("f32", -1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:592
assert_return(() => invoke($0, `ne`, [value("f32", 1), value("f32", 1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:593
assert_return(
  () => invoke($0, `ne`, [value("f32", -1), value("f32", -6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:594
assert_return(
  () => invoke($0, `ne`, [value("f32", -1), value("f32", 6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:595
assert_return(
  () => invoke($0, `ne`, [value("f32", 1), value("f32", -6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:596
assert_return(
  () => invoke($0, `ne`, [value("f32", 1), value("f32", 6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:597
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -1),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:598
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -1),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:599
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 1),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:600
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 1),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:601
assert_return(
  () => invoke($0, `ne`, [value("f32", -1), value("f32", -Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:602
assert_return(
  () => invoke($0, `ne`, [value("f32", -1), value("f32", Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:603
assert_return(
  () => invoke($0, `ne`, [value("f32", 1), value("f32", -Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:604
assert_return(
  () => invoke($0, `ne`, [value("f32", 1), value("f32", Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:605
assert_return(
  () =>
    invoke($0, `ne`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:606
assert_return(
  () =>
    invoke($0, `ne`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:607
assert_return(
  () =>
    invoke($0, `ne`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:608
assert_return(
  () =>
    invoke($0, `ne`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:609
assert_return(
  () =>
    invoke($0, `ne`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:610
assert_return(
  () =>
    invoke($0, `ne`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:611
assert_return(
  () =>
    invoke($0, `ne`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:612
assert_return(
  () =>
    invoke($0, `ne`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:613
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:614
assert_return(
  () => invoke($0, `ne`, [value("f32", -6.2831855), value("f32", 0)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:615
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:616
assert_return(
  () => invoke($0, `ne`, [value("f32", 6.2831855), value("f32", 0)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:617
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -6.2831855),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:618
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -6.2831855),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:619
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 6.2831855),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:620
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 6.2831855),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:621
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -6.2831855),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:622
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -6.2831855),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:623
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 6.2831855),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:624
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 6.2831855),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:625
assert_return(
  () => invoke($0, `ne`, [value("f32", -6.2831855), value("f32", -0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:626
assert_return(
  () => invoke($0, `ne`, [value("f32", -6.2831855), value("f32", 0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:627
assert_return(
  () => invoke($0, `ne`, [value("f32", 6.2831855), value("f32", -0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:628
assert_return(
  () => invoke($0, `ne`, [value("f32", 6.2831855), value("f32", 0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:629
assert_return(
  () => invoke($0, `ne`, [value("f32", -6.2831855), value("f32", -1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:630
assert_return(
  () => invoke($0, `ne`, [value("f32", -6.2831855), value("f32", 1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:631
assert_return(
  () => invoke($0, `ne`, [value("f32", 6.2831855), value("f32", -1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:632
assert_return(
  () => invoke($0, `ne`, [value("f32", 6.2831855), value("f32", 1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:633
assert_return(
  () => invoke($0, `ne`, [value("f32", -6.2831855), value("f32", -6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:634
assert_return(
  () => invoke($0, `ne`, [value("f32", -6.2831855), value("f32", 6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:635
assert_return(
  () => invoke($0, `ne`, [value("f32", 6.2831855), value("f32", -6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:636
assert_return(
  () => invoke($0, `ne`, [value("f32", 6.2831855), value("f32", 6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:637
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -6.2831855),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:638
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -6.2831855),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:639
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 6.2831855),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:640
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 6.2831855),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:641
assert_return(
  () => invoke($0, `ne`, [value("f32", -6.2831855), value("f32", -Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:642
assert_return(
  () => invoke($0, `ne`, [value("f32", -6.2831855), value("f32", Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:643
assert_return(
  () => invoke($0, `ne`, [value("f32", 6.2831855), value("f32", -Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:644
assert_return(
  () => invoke($0, `ne`, [value("f32", 6.2831855), value("f32", Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:645
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:646
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:647
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:648
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:649
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:650
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:651
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:652
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:653
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:654
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:655
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:656
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:657
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:658
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:659
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:660
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:661
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:662
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:663
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:664
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:665
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:666
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:667
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:668
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:669
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:670
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:671
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:672
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:673
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:674
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:675
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:676
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:677
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:678
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:679
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:680
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:681
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:682
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:683
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:684
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:685
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:686
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:687
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:688
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:689
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:690
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:691
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:692
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:693
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:694
assert_return(
  () => invoke($0, `ne`, [value("f32", -Infinity), value("f32", 0)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:695
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:696
assert_return(
  () => invoke($0, `ne`, [value("f32", Infinity), value("f32", 0)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:697
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -Infinity),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:698
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -Infinity),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:699
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", Infinity),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:700
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", Infinity),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:701
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -Infinity),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:702
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -Infinity),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:703
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", Infinity),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:704
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", Infinity),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:705
assert_return(
  () => invoke($0, `ne`, [value("f32", -Infinity), value("f32", -0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:706
assert_return(
  () => invoke($0, `ne`, [value("f32", -Infinity), value("f32", 0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:707
assert_return(
  () => invoke($0, `ne`, [value("f32", Infinity), value("f32", -0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:708
assert_return(
  () => invoke($0, `ne`, [value("f32", Infinity), value("f32", 0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:709
assert_return(
  () => invoke($0, `ne`, [value("f32", -Infinity), value("f32", -1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:710
assert_return(
  () => invoke($0, `ne`, [value("f32", -Infinity), value("f32", 1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:711
assert_return(
  () => invoke($0, `ne`, [value("f32", Infinity), value("f32", -1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:712
assert_return(
  () => invoke($0, `ne`, [value("f32", Infinity), value("f32", 1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:713
assert_return(
  () => invoke($0, `ne`, [value("f32", -Infinity), value("f32", -6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:714
assert_return(
  () => invoke($0, `ne`, [value("f32", -Infinity), value("f32", 6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:715
assert_return(
  () => invoke($0, `ne`, [value("f32", Infinity), value("f32", -6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:716
assert_return(
  () => invoke($0, `ne`, [value("f32", Infinity), value("f32", 6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:717
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -Infinity),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:718
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -Infinity),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:719
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", Infinity),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:720
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", Infinity),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:721
assert_return(
  () => invoke($0, `ne`, [value("f32", -Infinity), value("f32", -Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:722
assert_return(
  () => invoke($0, `ne`, [value("f32", -Infinity), value("f32", Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:723
assert_return(
  () => invoke($0, `ne`, [value("f32", Infinity), value("f32", -Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:724
assert_return(
  () => invoke($0, `ne`, [value("f32", Infinity), value("f32", Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:725
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:726
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:727
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:728
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:729
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:730
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:731
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:732
assert_return(
  () =>
    invoke($0, `ne`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:733
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:734
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:735
assert_return(
  () =>
    invoke($0, `ne`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 0)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:736
assert_return(
  () =>
    invoke($0, `ne`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 0)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:737
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:738
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:739
assert_return(
  () =>
    invoke($0, `ne`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 0)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:740
assert_return(
  () =>
    invoke($0, `ne`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 0)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:741
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:742
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:743
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:744
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:745
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:746
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:747
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:748
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:749
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:750
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:751
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:752
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:753
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:754
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:755
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:756
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:757
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:758
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:759
assert_return(
  () =>
    invoke($0, `ne`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:760
assert_return(
  () =>
    invoke($0, `ne`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:761
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:762
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:763
assert_return(
  () =>
    invoke($0, `ne`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:764
assert_return(
  () =>
    invoke($0, `ne`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:765
assert_return(
  () =>
    invoke($0, `ne`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", -1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:766
assert_return(
  () =>
    invoke($0, `ne`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", -1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:767
assert_return(
  () =>
    invoke($0, `ne`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:768
assert_return(
  () =>
    invoke($0, `ne`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:769
assert_return(
  () =>
    invoke($0, `ne`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", -1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:770
assert_return(
  () =>
    invoke($0, `ne`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", -1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:771
assert_return(
  () =>
    invoke($0, `ne`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:772
assert_return(
  () =>
    invoke($0, `ne`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:773
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:774
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:775
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:776
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:777
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:778
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:779
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:780
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:781
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:782
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:783
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:784
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:785
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:786
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:787
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:788
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:789
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:790
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:791
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:792
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:793
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:794
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:795
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:796
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:797
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:798
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:799
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:800
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:801
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:802
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:803
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:804
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:805
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:806
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:807
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:808
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:809
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:810
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:811
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:812
assert_return(
  () =>
    invoke($0, `ne`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:813
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:814
assert_return(
  () =>
    invoke($0, `lt`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:815
assert_return(
  () =>
    invoke($0, `lt`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:816
assert_return(() => invoke($0, `lt`, [value("f32", 0), value("f32", 0)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:817
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:818
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:819
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:820
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:821
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:822
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:823
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:824
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:825
assert_return(
  () =>
    invoke($0, `lt`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", -0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:826
assert_return(
  () =>
    invoke($0, `lt`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", 0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:827
assert_return(() => invoke($0, `lt`, [value("f32", 0), value("f32", -0.5)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:828
assert_return(() => invoke($0, `lt`, [value("f32", 0), value("f32", 0.5)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:829
assert_return(
  () =>
    invoke($0, `lt`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:830
assert_return(
  () =>
    invoke($0, `lt`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", 1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:831
assert_return(() => invoke($0, `lt`, [value("f32", 0), value("f32", -1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:832
assert_return(() => invoke($0, `lt`, [value("f32", 0), value("f32", 1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:833
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:834
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:835
assert_return(
  () => invoke($0, `lt`, [value("f32", 0), value("f32", -6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:836
assert_return(
  () => invoke($0, `lt`, [value("f32", 0), value("f32", 6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:837
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:838
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:839
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:840
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:841
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:842
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:843
assert_return(
  () => invoke($0, `lt`, [value("f32", 0), value("f32", -Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:844
assert_return(
  () => invoke($0, `lt`, [value("f32", 0), value("f32", Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:845
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:846
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:847
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:848
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:849
assert_return(
  () =>
    invoke($0, `lt`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:850
assert_return(
  () =>
    invoke($0, `lt`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:851
assert_return(
  () =>
    invoke($0, `lt`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:852
assert_return(
  () =>
    invoke($0, `lt`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:853
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:854
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:855
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:856
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:857
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:858
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:859
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:860
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:861
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:862
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:863
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:864
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:865
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:866
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:867
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:868
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:869
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:870
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:871
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:872
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:873
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:874
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:875
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:876
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:877
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:878
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:879
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:880
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:881
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:882
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:883
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:884
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:885
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:886
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:887
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:888
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:889
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:890
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:891
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:892
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:893
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:894
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:895
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:896
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:897
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:898
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:899
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:900
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:901
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:902
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:903
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:904
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:905
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:906
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:907
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:908
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:909
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:910
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:911
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:912
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:913
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:914
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:915
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:916
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:917
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:918
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:919
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:920
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:921
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:922
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:923
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:924
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:925
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:926
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:927
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:928
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:929
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:930
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:931
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:932
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:933
assert_return(
  () =>
    invoke($0, `lt`, [value("f32", -0.5), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:934
assert_return(() => invoke($0, `lt`, [value("f32", -0.5), value("f32", 0)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:935
assert_return(
  () =>
    invoke($0, `lt`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:936
assert_return(() => invoke($0, `lt`, [value("f32", 0.5), value("f32", 0)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:937
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.5),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:938
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.5),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:939
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.5),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:940
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.5),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:941
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.5),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:942
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.5),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:943
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.5),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:944
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.5),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:945
assert_return(
  () => invoke($0, `lt`, [value("f32", -0.5), value("f32", -0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:946
assert_return(() => invoke($0, `lt`, [value("f32", -0.5), value("f32", 0.5)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:947
assert_return(() => invoke($0, `lt`, [value("f32", 0.5), value("f32", -0.5)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:948
assert_return(() => invoke($0, `lt`, [value("f32", 0.5), value("f32", 0.5)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:949
assert_return(() => invoke($0, `lt`, [value("f32", -0.5), value("f32", -1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:950
assert_return(() => invoke($0, `lt`, [value("f32", -0.5), value("f32", 1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:951
assert_return(() => invoke($0, `lt`, [value("f32", 0.5), value("f32", -1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:952
assert_return(() => invoke($0, `lt`, [value("f32", 0.5), value("f32", 1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:953
assert_return(
  () => invoke($0, `lt`, [value("f32", -0.5), value("f32", -6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:954
assert_return(
  () => invoke($0, `lt`, [value("f32", -0.5), value("f32", 6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:955
assert_return(
  () => invoke($0, `lt`, [value("f32", 0.5), value("f32", -6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:956
assert_return(
  () => invoke($0, `lt`, [value("f32", 0.5), value("f32", 6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:957
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.5),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:958
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.5),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:959
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.5),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:960
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 0.5),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:961
assert_return(
  () => invoke($0, `lt`, [value("f32", -0.5), value("f32", -Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:962
assert_return(
  () => invoke($0, `lt`, [value("f32", -0.5), value("f32", Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:963
assert_return(
  () => invoke($0, `lt`, [value("f32", 0.5), value("f32", -Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:964
assert_return(
  () => invoke($0, `lt`, [value("f32", 0.5), value("f32", Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:965
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:966
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:967
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:968
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:969
assert_return(
  () =>
    invoke($0, `lt`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:970
assert_return(
  () =>
    invoke($0, `lt`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:971
assert_return(
  () =>
    invoke($0, `lt`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:972
assert_return(
  () =>
    invoke($0, `lt`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:973
assert_return(
  () =>
    invoke($0, `lt`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:974
assert_return(() => invoke($0, `lt`, [value("f32", -1), value("f32", 0)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:975
assert_return(
  () =>
    invoke($0, `lt`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:976
assert_return(() => invoke($0, `lt`, [value("f32", 1), value("f32", 0)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:977
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -1),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:978
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -1),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:979
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 1),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:980
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 1),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:981
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -1),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:982
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -1),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:983
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 1),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:984
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 1),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:985
assert_return(() => invoke($0, `lt`, [value("f32", -1), value("f32", -0.5)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:986
assert_return(() => invoke($0, `lt`, [value("f32", -1), value("f32", 0.5)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:987
assert_return(() => invoke($0, `lt`, [value("f32", 1), value("f32", -0.5)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:988
assert_return(() => invoke($0, `lt`, [value("f32", 1), value("f32", 0.5)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:989
assert_return(() => invoke($0, `lt`, [value("f32", -1), value("f32", -1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:990
assert_return(() => invoke($0, `lt`, [value("f32", -1), value("f32", 1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:991
assert_return(() => invoke($0, `lt`, [value("f32", 1), value("f32", -1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:992
assert_return(() => invoke($0, `lt`, [value("f32", 1), value("f32", 1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:993
assert_return(
  () => invoke($0, `lt`, [value("f32", -1), value("f32", -6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:994
assert_return(
  () => invoke($0, `lt`, [value("f32", -1), value("f32", 6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:995
assert_return(
  () => invoke($0, `lt`, [value("f32", 1), value("f32", -6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:996
assert_return(
  () => invoke($0, `lt`, [value("f32", 1), value("f32", 6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:997
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -1),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:998
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -1),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:999
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 1),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1000
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 1),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1001
assert_return(
  () => invoke($0, `lt`, [value("f32", -1), value("f32", -Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1002
assert_return(
  () => invoke($0, `lt`, [value("f32", -1), value("f32", Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1003
assert_return(
  () => invoke($0, `lt`, [value("f32", 1), value("f32", -Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1004
assert_return(
  () => invoke($0, `lt`, [value("f32", 1), value("f32", Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1005
assert_return(
  () =>
    invoke($0, `lt`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1006
assert_return(
  () =>
    invoke($0, `lt`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1007
assert_return(
  () =>
    invoke($0, `lt`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1008
assert_return(
  () =>
    invoke($0, `lt`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1009
assert_return(
  () =>
    invoke($0, `lt`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1010
assert_return(
  () =>
    invoke($0, `lt`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1011
assert_return(
  () =>
    invoke($0, `lt`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1012
assert_return(
  () =>
    invoke($0, `lt`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1013
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1014
assert_return(
  () => invoke($0, `lt`, [value("f32", -6.2831855), value("f32", 0)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1015
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1016
assert_return(
  () => invoke($0, `lt`, [value("f32", 6.2831855), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1017
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -6.2831855),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1018
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -6.2831855),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1019
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 6.2831855),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1020
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 6.2831855),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1021
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -6.2831855),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1022
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -6.2831855),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1023
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 6.2831855),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1024
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 6.2831855),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1025
assert_return(
  () => invoke($0, `lt`, [value("f32", -6.2831855), value("f32", -0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1026
assert_return(
  () => invoke($0, `lt`, [value("f32", -6.2831855), value("f32", 0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1027
assert_return(
  () => invoke($0, `lt`, [value("f32", 6.2831855), value("f32", -0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1028
assert_return(
  () => invoke($0, `lt`, [value("f32", 6.2831855), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1029
assert_return(
  () => invoke($0, `lt`, [value("f32", -6.2831855), value("f32", -1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1030
assert_return(
  () => invoke($0, `lt`, [value("f32", -6.2831855), value("f32", 1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1031
assert_return(
  () => invoke($0, `lt`, [value("f32", 6.2831855), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1032
assert_return(
  () => invoke($0, `lt`, [value("f32", 6.2831855), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1033
assert_return(
  () => invoke($0, `lt`, [value("f32", -6.2831855), value("f32", -6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1034
assert_return(
  () => invoke($0, `lt`, [value("f32", -6.2831855), value("f32", 6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1035
assert_return(
  () => invoke($0, `lt`, [value("f32", 6.2831855), value("f32", -6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1036
assert_return(
  () => invoke($0, `lt`, [value("f32", 6.2831855), value("f32", 6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1037
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -6.2831855),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1038
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -6.2831855),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1039
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 6.2831855),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1040
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 6.2831855),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1041
assert_return(
  () => invoke($0, `lt`, [value("f32", -6.2831855), value("f32", -Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1042
assert_return(
  () => invoke($0, `lt`, [value("f32", -6.2831855), value("f32", Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1043
assert_return(
  () => invoke($0, `lt`, [value("f32", 6.2831855), value("f32", -Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1044
assert_return(
  () => invoke($0, `lt`, [value("f32", 6.2831855), value("f32", Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1045
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1046
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1047
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1048
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1049
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1050
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1051
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1052
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1053
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1054
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1055
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1056
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1057
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1058
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1059
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1060
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1061
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1062
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1063
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1064
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1065
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1066
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1067
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1068
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1069
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1070
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1071
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1072
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1073
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1074
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1075
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1076
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1077
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1078
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1079
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1080
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1081
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1082
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1083
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1084
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1085
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1086
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1087
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1088
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1089
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1090
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1091
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1092
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1093
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1094
assert_return(
  () => invoke($0, `lt`, [value("f32", -Infinity), value("f32", 0)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1095
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1096
assert_return(
  () => invoke($0, `lt`, [value("f32", Infinity), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1097
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -Infinity),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1098
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -Infinity),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1099
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", Infinity),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1100
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", Infinity),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1101
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -Infinity),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1102
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -Infinity),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1103
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", Infinity),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1104
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", Infinity),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1105
assert_return(
  () => invoke($0, `lt`, [value("f32", -Infinity), value("f32", -0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1106
assert_return(
  () => invoke($0, `lt`, [value("f32", -Infinity), value("f32", 0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1107
assert_return(
  () => invoke($0, `lt`, [value("f32", Infinity), value("f32", -0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1108
assert_return(
  () => invoke($0, `lt`, [value("f32", Infinity), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1109
assert_return(
  () => invoke($0, `lt`, [value("f32", -Infinity), value("f32", -1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1110
assert_return(
  () => invoke($0, `lt`, [value("f32", -Infinity), value("f32", 1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1111
assert_return(
  () => invoke($0, `lt`, [value("f32", Infinity), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1112
assert_return(
  () => invoke($0, `lt`, [value("f32", Infinity), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1113
assert_return(
  () => invoke($0, `lt`, [value("f32", -Infinity), value("f32", -6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1114
assert_return(
  () => invoke($0, `lt`, [value("f32", -Infinity), value("f32", 6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1115
assert_return(
  () => invoke($0, `lt`, [value("f32", Infinity), value("f32", -6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1116
assert_return(
  () => invoke($0, `lt`, [value("f32", Infinity), value("f32", 6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1117
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -Infinity),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1118
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -Infinity),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1119
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", Infinity),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1120
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", Infinity),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1121
assert_return(
  () => invoke($0, `lt`, [value("f32", -Infinity), value("f32", -Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1122
assert_return(
  () => invoke($0, `lt`, [value("f32", -Infinity), value("f32", Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1123
assert_return(
  () => invoke($0, `lt`, [value("f32", Infinity), value("f32", -Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1124
assert_return(
  () => invoke($0, `lt`, [value("f32", Infinity), value("f32", Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1125
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1126
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1127
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1128
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1129
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1130
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1131
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1132
assert_return(
  () =>
    invoke($0, `lt`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1133
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1134
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1135
assert_return(
  () =>
    invoke($0, `lt`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1136
assert_return(
  () =>
    invoke($0, `lt`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1137
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1138
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1139
assert_return(
  () =>
    invoke($0, `lt`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1140
assert_return(
  () =>
    invoke($0, `lt`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1141
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1142
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1143
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1144
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1145
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1146
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1147
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1148
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1149
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1150
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1151
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1152
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1153
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1154
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1155
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1156
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1157
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1158
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1159
assert_return(
  () =>
    invoke($0, `lt`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1160
assert_return(
  () =>
    invoke($0, `lt`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1161
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1162
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1163
assert_return(
  () =>
    invoke($0, `lt`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1164
assert_return(
  () =>
    invoke($0, `lt`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1165
assert_return(
  () =>
    invoke($0, `lt`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1166
assert_return(
  () =>
    invoke($0, `lt`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1167
assert_return(
  () =>
    invoke($0, `lt`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1168
assert_return(
  () =>
    invoke($0, `lt`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1169
assert_return(
  () =>
    invoke($0, `lt`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1170
assert_return(
  () =>
    invoke($0, `lt`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1171
assert_return(
  () =>
    invoke($0, `lt`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1172
assert_return(
  () =>
    invoke($0, `lt`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1173
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1174
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1175
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1176
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1177
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1178
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1179
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1180
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1181
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1182
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1183
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1184
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1185
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1186
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1187
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1188
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1189
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1190
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1191
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1192
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1193
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1194
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1195
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1196
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1197
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1198
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1199
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1200
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1201
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1202
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1203
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1204
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1205
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1206
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1207
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1208
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1209
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1210
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1211
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1212
assert_return(
  () =>
    invoke($0, `lt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1213
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1214
assert_return(
  () =>
    invoke($0, `le`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", 0)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1215
assert_return(
  () =>
    invoke($0, `le`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1216
assert_return(() => invoke($0, `le`, [value("f32", 0), value("f32", 0)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:1217
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1218
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1219
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1220
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1221
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1222
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1223
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1224
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1225
assert_return(
  () =>
    invoke($0, `le`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", -0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1226
assert_return(
  () =>
    invoke($0, `le`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", 0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1227
assert_return(() => invoke($0, `le`, [value("f32", 0), value("f32", -0.5)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:1228
assert_return(() => invoke($0, `le`, [value("f32", 0), value("f32", 0.5)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:1229
assert_return(
  () =>
    invoke($0, `le`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1230
assert_return(
  () =>
    invoke($0, `le`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", 1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1231
assert_return(() => invoke($0, `le`, [value("f32", 0), value("f32", -1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:1232
assert_return(() => invoke($0, `le`, [value("f32", 0), value("f32", 1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:1233
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1234
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1235
assert_return(
  () => invoke($0, `le`, [value("f32", 0), value("f32", -6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1236
assert_return(
  () => invoke($0, `le`, [value("f32", 0), value("f32", 6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1237
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1238
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1239
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1240
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1241
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1242
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1243
assert_return(
  () => invoke($0, `le`, [value("f32", 0), value("f32", -Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1244
assert_return(
  () => invoke($0, `le`, [value("f32", 0), value("f32", Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1245
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1246
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1247
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1248
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1249
assert_return(
  () =>
    invoke($0, `le`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1250
assert_return(
  () =>
    invoke($0, `le`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1251
assert_return(
  () =>
    invoke($0, `le`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1252
assert_return(
  () =>
    invoke($0, `le`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1253
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1254
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1255
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1256
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1257
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1258
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1259
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1260
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1261
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1262
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1263
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1264
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1265
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1266
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1267
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1268
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1269
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1270
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1271
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1272
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1273
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1274
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1275
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1276
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1277
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1278
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1279
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1280
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1281
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1282
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1283
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1284
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1285
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1286
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1287
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1288
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1289
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1290
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1291
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1292
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1293
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1294
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1295
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1296
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1297
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1298
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1299
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1300
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1301
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1302
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1303
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1304
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1305
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1306
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1307
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1308
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1309
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1310
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1311
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1312
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1313
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1314
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1315
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1316
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1317
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1318
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1319
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1320
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1321
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1322
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1323
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1324
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1325
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1326
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1327
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1328
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1329
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1330
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1331
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1332
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1333
assert_return(
  () =>
    invoke($0, `le`, [value("f32", -0.5), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1334
assert_return(() => invoke($0, `le`, [value("f32", -0.5), value("f32", 0)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:1335
assert_return(
  () =>
    invoke($0, `le`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1336
assert_return(() => invoke($0, `le`, [value("f32", 0.5), value("f32", 0)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:1337
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.5),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1338
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.5),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1339
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.5),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1340
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.5),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1341
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.5),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1342
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.5),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1343
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.5),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1344
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.5),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1345
assert_return(
  () => invoke($0, `le`, [value("f32", -0.5), value("f32", -0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1346
assert_return(() => invoke($0, `le`, [value("f32", -0.5), value("f32", 0.5)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:1347
assert_return(() => invoke($0, `le`, [value("f32", 0.5), value("f32", -0.5)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:1348
assert_return(() => invoke($0, `le`, [value("f32", 0.5), value("f32", 0.5)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:1349
assert_return(() => invoke($0, `le`, [value("f32", -0.5), value("f32", -1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:1350
assert_return(() => invoke($0, `le`, [value("f32", -0.5), value("f32", 1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:1351
assert_return(() => invoke($0, `le`, [value("f32", 0.5), value("f32", -1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:1352
assert_return(() => invoke($0, `le`, [value("f32", 0.5), value("f32", 1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:1353
assert_return(
  () => invoke($0, `le`, [value("f32", -0.5), value("f32", -6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1354
assert_return(
  () => invoke($0, `le`, [value("f32", -0.5), value("f32", 6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1355
assert_return(
  () => invoke($0, `le`, [value("f32", 0.5), value("f32", -6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1356
assert_return(
  () => invoke($0, `le`, [value("f32", 0.5), value("f32", 6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1357
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.5),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1358
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.5),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1359
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.5),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1360
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 0.5),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1361
assert_return(
  () => invoke($0, `le`, [value("f32", -0.5), value("f32", -Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1362
assert_return(
  () => invoke($0, `le`, [value("f32", -0.5), value("f32", Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1363
assert_return(
  () => invoke($0, `le`, [value("f32", 0.5), value("f32", -Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1364
assert_return(
  () => invoke($0, `le`, [value("f32", 0.5), value("f32", Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1365
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1366
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1367
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1368
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1369
assert_return(
  () =>
    invoke($0, `le`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1370
assert_return(
  () =>
    invoke($0, `le`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1371
assert_return(
  () =>
    invoke($0, `le`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1372
assert_return(
  () =>
    invoke($0, `le`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1373
assert_return(
  () =>
    invoke($0, `le`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1374
assert_return(() => invoke($0, `le`, [value("f32", -1), value("f32", 0)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:1375
assert_return(
  () =>
    invoke($0, `le`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1376
assert_return(() => invoke($0, `le`, [value("f32", 1), value("f32", 0)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:1377
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -1),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1378
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -1),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1379
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 1),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1380
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 1),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1381
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -1),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1382
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -1),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1383
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 1),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1384
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 1),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1385
assert_return(() => invoke($0, `le`, [value("f32", -1), value("f32", -0.5)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:1386
assert_return(() => invoke($0, `le`, [value("f32", -1), value("f32", 0.5)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:1387
assert_return(() => invoke($0, `le`, [value("f32", 1), value("f32", -0.5)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:1388
assert_return(() => invoke($0, `le`, [value("f32", 1), value("f32", 0.5)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:1389
assert_return(() => invoke($0, `le`, [value("f32", -1), value("f32", -1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:1390
assert_return(() => invoke($0, `le`, [value("f32", -1), value("f32", 1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:1391
assert_return(() => invoke($0, `le`, [value("f32", 1), value("f32", -1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:1392
assert_return(() => invoke($0, `le`, [value("f32", 1), value("f32", 1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:1393
assert_return(
  () => invoke($0, `le`, [value("f32", -1), value("f32", -6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1394
assert_return(
  () => invoke($0, `le`, [value("f32", -1), value("f32", 6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1395
assert_return(
  () => invoke($0, `le`, [value("f32", 1), value("f32", -6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1396
assert_return(
  () => invoke($0, `le`, [value("f32", 1), value("f32", 6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1397
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -1),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1398
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -1),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1399
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 1),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1400
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 1),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1401
assert_return(
  () => invoke($0, `le`, [value("f32", -1), value("f32", -Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1402
assert_return(
  () => invoke($0, `le`, [value("f32", -1), value("f32", Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1403
assert_return(
  () => invoke($0, `le`, [value("f32", 1), value("f32", -Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1404
assert_return(
  () => invoke($0, `le`, [value("f32", 1), value("f32", Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1405
assert_return(
  () =>
    invoke($0, `le`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1406
assert_return(
  () =>
    invoke($0, `le`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1407
assert_return(
  () =>
    invoke($0, `le`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1408
assert_return(
  () =>
    invoke($0, `le`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1409
assert_return(
  () =>
    invoke($0, `le`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1410
assert_return(
  () =>
    invoke($0, `le`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1411
assert_return(
  () =>
    invoke($0, `le`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1412
assert_return(
  () =>
    invoke($0, `le`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1413
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1414
assert_return(
  () => invoke($0, `le`, [value("f32", -6.2831855), value("f32", 0)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1415
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1416
assert_return(
  () => invoke($0, `le`, [value("f32", 6.2831855), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1417
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -6.2831855),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1418
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -6.2831855),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1419
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 6.2831855),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1420
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 6.2831855),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1421
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -6.2831855),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1422
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -6.2831855),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1423
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 6.2831855),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1424
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 6.2831855),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1425
assert_return(
  () => invoke($0, `le`, [value("f32", -6.2831855), value("f32", -0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1426
assert_return(
  () => invoke($0, `le`, [value("f32", -6.2831855), value("f32", 0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1427
assert_return(
  () => invoke($0, `le`, [value("f32", 6.2831855), value("f32", -0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1428
assert_return(
  () => invoke($0, `le`, [value("f32", 6.2831855), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1429
assert_return(
  () => invoke($0, `le`, [value("f32", -6.2831855), value("f32", -1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1430
assert_return(
  () => invoke($0, `le`, [value("f32", -6.2831855), value("f32", 1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1431
assert_return(
  () => invoke($0, `le`, [value("f32", 6.2831855), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1432
assert_return(
  () => invoke($0, `le`, [value("f32", 6.2831855), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1433
assert_return(
  () => invoke($0, `le`, [value("f32", -6.2831855), value("f32", -6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1434
assert_return(
  () => invoke($0, `le`, [value("f32", -6.2831855), value("f32", 6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1435
assert_return(
  () => invoke($0, `le`, [value("f32", 6.2831855), value("f32", -6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1436
assert_return(
  () => invoke($0, `le`, [value("f32", 6.2831855), value("f32", 6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1437
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -6.2831855),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1438
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -6.2831855),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1439
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 6.2831855),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1440
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 6.2831855),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1441
assert_return(
  () => invoke($0, `le`, [value("f32", -6.2831855), value("f32", -Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1442
assert_return(
  () => invoke($0, `le`, [value("f32", -6.2831855), value("f32", Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1443
assert_return(
  () => invoke($0, `le`, [value("f32", 6.2831855), value("f32", -Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1444
assert_return(
  () => invoke($0, `le`, [value("f32", 6.2831855), value("f32", Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1445
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1446
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1447
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1448
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1449
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1450
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1451
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1452
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1453
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1454
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1455
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1456
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1457
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1458
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1459
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1460
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1461
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1462
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1463
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1464
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1465
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1466
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1467
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1468
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1469
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1470
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1471
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1472
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1473
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1474
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1475
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1476
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1477
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1478
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1479
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1480
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1481
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1482
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1483
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1484
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1485
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1486
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1487
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1488
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1489
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1490
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1491
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1492
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1493
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1494
assert_return(
  () => invoke($0, `le`, [value("f32", -Infinity), value("f32", 0)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1495
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1496
assert_return(
  () => invoke($0, `le`, [value("f32", Infinity), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1497
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -Infinity),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1498
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -Infinity),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1499
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", Infinity),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1500
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", Infinity),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1501
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -Infinity),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1502
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -Infinity),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1503
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", Infinity),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1504
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", Infinity),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1505
assert_return(
  () => invoke($0, `le`, [value("f32", -Infinity), value("f32", -0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1506
assert_return(
  () => invoke($0, `le`, [value("f32", -Infinity), value("f32", 0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1507
assert_return(
  () => invoke($0, `le`, [value("f32", Infinity), value("f32", -0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1508
assert_return(
  () => invoke($0, `le`, [value("f32", Infinity), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1509
assert_return(
  () => invoke($0, `le`, [value("f32", -Infinity), value("f32", -1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1510
assert_return(
  () => invoke($0, `le`, [value("f32", -Infinity), value("f32", 1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1511
assert_return(
  () => invoke($0, `le`, [value("f32", Infinity), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1512
assert_return(
  () => invoke($0, `le`, [value("f32", Infinity), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1513
assert_return(
  () => invoke($0, `le`, [value("f32", -Infinity), value("f32", -6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1514
assert_return(
  () => invoke($0, `le`, [value("f32", -Infinity), value("f32", 6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1515
assert_return(
  () => invoke($0, `le`, [value("f32", Infinity), value("f32", -6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1516
assert_return(
  () => invoke($0, `le`, [value("f32", Infinity), value("f32", 6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1517
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -Infinity),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1518
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -Infinity),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1519
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", Infinity),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1520
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", Infinity),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1521
assert_return(
  () => invoke($0, `le`, [value("f32", -Infinity), value("f32", -Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1522
assert_return(
  () => invoke($0, `le`, [value("f32", -Infinity), value("f32", Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1523
assert_return(
  () => invoke($0, `le`, [value("f32", Infinity), value("f32", -Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1524
assert_return(
  () => invoke($0, `le`, [value("f32", Infinity), value("f32", Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1525
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1526
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1527
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1528
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1529
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1530
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1531
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1532
assert_return(
  () =>
    invoke($0, `le`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1533
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1534
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1535
assert_return(
  () =>
    invoke($0, `le`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1536
assert_return(
  () =>
    invoke($0, `le`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1537
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1538
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1539
assert_return(
  () =>
    invoke($0, `le`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1540
assert_return(
  () =>
    invoke($0, `le`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1541
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1542
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1543
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1544
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1545
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1546
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1547
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1548
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1549
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1550
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1551
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1552
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1553
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1554
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1555
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1556
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1557
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1558
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1559
assert_return(
  () =>
    invoke($0, `le`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1560
assert_return(
  () =>
    invoke($0, `le`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1561
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1562
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1563
assert_return(
  () =>
    invoke($0, `le`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1564
assert_return(
  () =>
    invoke($0, `le`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1565
assert_return(
  () =>
    invoke($0, `le`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1566
assert_return(
  () =>
    invoke($0, `le`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1567
assert_return(
  () =>
    invoke($0, `le`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1568
assert_return(
  () =>
    invoke($0, `le`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1569
assert_return(
  () =>
    invoke($0, `le`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1570
assert_return(
  () =>
    invoke($0, `le`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1571
assert_return(
  () =>
    invoke($0, `le`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1572
assert_return(
  () =>
    invoke($0, `le`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1573
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1574
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1575
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1576
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1577
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1578
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1579
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1580
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1581
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1582
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1583
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1584
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1585
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1586
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1587
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1588
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1589
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1590
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1591
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1592
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1593
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1594
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1595
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1596
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1597
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1598
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1599
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1600
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1601
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1602
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1603
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1604
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1605
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1606
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1607
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1608
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1609
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1610
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1611
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1612
assert_return(
  () =>
    invoke($0, `le`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1613
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1614
assert_return(
  () =>
    invoke($0, `gt`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1615
assert_return(
  () =>
    invoke($0, `gt`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1616
assert_return(() => invoke($0, `gt`, [value("f32", 0), value("f32", 0)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:1617
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1618
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1619
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1620
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1621
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1622
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1623
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1624
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1625
assert_return(
  () =>
    invoke($0, `gt`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", -0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1626
assert_return(
  () =>
    invoke($0, `gt`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1627
assert_return(() => invoke($0, `gt`, [value("f32", 0), value("f32", -0.5)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:1628
assert_return(() => invoke($0, `gt`, [value("f32", 0), value("f32", 0.5)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:1629
assert_return(
  () =>
    invoke($0, `gt`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", -1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1630
assert_return(
  () =>
    invoke($0, `gt`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1631
assert_return(() => invoke($0, `gt`, [value("f32", 0), value("f32", -1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:1632
assert_return(() => invoke($0, `gt`, [value("f32", 0), value("f32", 1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:1633
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1634
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1635
assert_return(
  () => invoke($0, `gt`, [value("f32", 0), value("f32", -6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1636
assert_return(
  () => invoke($0, `gt`, [value("f32", 0), value("f32", 6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1637
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1638
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1639
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1640
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1641
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1642
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1643
assert_return(
  () => invoke($0, `gt`, [value("f32", 0), value("f32", -Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1644
assert_return(
  () => invoke($0, `gt`, [value("f32", 0), value("f32", Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1645
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1646
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1647
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1648
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1649
assert_return(
  () =>
    invoke($0, `gt`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1650
assert_return(
  () =>
    invoke($0, `gt`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1651
assert_return(
  () =>
    invoke($0, `gt`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1652
assert_return(
  () =>
    invoke($0, `gt`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1653
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1654
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1655
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1656
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1657
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1658
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1659
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1660
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1661
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1662
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1663
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1664
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1665
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1666
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1667
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1668
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1669
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1670
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1671
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1672
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1673
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1674
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1675
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1676
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1677
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1678
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1679
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1680
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1681
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1682
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1683
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1684
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1685
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1686
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1687
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1688
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1689
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1690
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1691
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1692
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1693
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1694
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1695
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1696
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1697
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1698
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1699
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1700
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1701
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1702
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1703
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1704
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1705
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1706
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1707
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1708
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1709
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1710
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1711
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1712
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1713
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1714
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1715
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1716
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1717
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1718
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1719
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1720
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1721
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1722
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1723
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1724
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1725
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1726
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1727
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1728
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1729
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1730
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1731
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1732
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1733
assert_return(
  () =>
    invoke($0, `gt`, [value("f32", -0.5), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1734
assert_return(() => invoke($0, `gt`, [value("f32", -0.5), value("f32", 0)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:1735
assert_return(
  () =>
    invoke($0, `gt`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1736
assert_return(() => invoke($0, `gt`, [value("f32", 0.5), value("f32", 0)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:1737
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.5),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1738
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.5),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1739
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.5),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1740
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.5),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1741
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.5),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1742
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.5),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1743
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.5),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1744
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.5),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1745
assert_return(
  () => invoke($0, `gt`, [value("f32", -0.5), value("f32", -0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1746
assert_return(() => invoke($0, `gt`, [value("f32", -0.5), value("f32", 0.5)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:1747
assert_return(() => invoke($0, `gt`, [value("f32", 0.5), value("f32", -0.5)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:1748
assert_return(() => invoke($0, `gt`, [value("f32", 0.5), value("f32", 0.5)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:1749
assert_return(() => invoke($0, `gt`, [value("f32", -0.5), value("f32", -1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:1750
assert_return(() => invoke($0, `gt`, [value("f32", -0.5), value("f32", 1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:1751
assert_return(() => invoke($0, `gt`, [value("f32", 0.5), value("f32", -1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:1752
assert_return(() => invoke($0, `gt`, [value("f32", 0.5), value("f32", 1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:1753
assert_return(
  () => invoke($0, `gt`, [value("f32", -0.5), value("f32", -6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1754
assert_return(
  () => invoke($0, `gt`, [value("f32", -0.5), value("f32", 6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1755
assert_return(
  () => invoke($0, `gt`, [value("f32", 0.5), value("f32", -6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1756
assert_return(
  () => invoke($0, `gt`, [value("f32", 0.5), value("f32", 6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1757
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.5),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1758
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.5),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1759
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.5),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1760
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 0.5),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1761
assert_return(
  () => invoke($0, `gt`, [value("f32", -0.5), value("f32", -Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1762
assert_return(
  () => invoke($0, `gt`, [value("f32", -0.5), value("f32", Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1763
assert_return(
  () => invoke($0, `gt`, [value("f32", 0.5), value("f32", -Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1764
assert_return(
  () => invoke($0, `gt`, [value("f32", 0.5), value("f32", Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1765
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1766
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1767
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1768
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1769
assert_return(
  () =>
    invoke($0, `gt`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1770
assert_return(
  () =>
    invoke($0, `gt`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1771
assert_return(
  () =>
    invoke($0, `gt`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1772
assert_return(
  () =>
    invoke($0, `gt`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1773
assert_return(
  () =>
    invoke($0, `gt`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1774
assert_return(() => invoke($0, `gt`, [value("f32", -1), value("f32", 0)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:1775
assert_return(
  () =>
    invoke($0, `gt`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1776
assert_return(() => invoke($0, `gt`, [value("f32", 1), value("f32", 0)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:1777
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -1),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1778
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -1),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1779
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 1),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1780
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 1),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1781
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -1),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1782
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -1),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1783
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 1),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1784
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 1),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1785
assert_return(() => invoke($0, `gt`, [value("f32", -1), value("f32", -0.5)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:1786
assert_return(() => invoke($0, `gt`, [value("f32", -1), value("f32", 0.5)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:1787
assert_return(() => invoke($0, `gt`, [value("f32", 1), value("f32", -0.5)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:1788
assert_return(() => invoke($0, `gt`, [value("f32", 1), value("f32", 0.5)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:1789
assert_return(() => invoke($0, `gt`, [value("f32", -1), value("f32", -1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:1790
assert_return(() => invoke($0, `gt`, [value("f32", -1), value("f32", 1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:1791
assert_return(() => invoke($0, `gt`, [value("f32", 1), value("f32", -1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:1792
assert_return(() => invoke($0, `gt`, [value("f32", 1), value("f32", 1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:1793
assert_return(
  () => invoke($0, `gt`, [value("f32", -1), value("f32", -6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1794
assert_return(
  () => invoke($0, `gt`, [value("f32", -1), value("f32", 6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1795
assert_return(
  () => invoke($0, `gt`, [value("f32", 1), value("f32", -6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1796
assert_return(
  () => invoke($0, `gt`, [value("f32", 1), value("f32", 6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1797
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -1),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1798
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -1),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1799
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 1),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1800
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 1),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1801
assert_return(
  () => invoke($0, `gt`, [value("f32", -1), value("f32", -Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1802
assert_return(
  () => invoke($0, `gt`, [value("f32", -1), value("f32", Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1803
assert_return(
  () => invoke($0, `gt`, [value("f32", 1), value("f32", -Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1804
assert_return(
  () => invoke($0, `gt`, [value("f32", 1), value("f32", Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1805
assert_return(
  () =>
    invoke($0, `gt`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1806
assert_return(
  () =>
    invoke($0, `gt`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1807
assert_return(
  () =>
    invoke($0, `gt`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1808
assert_return(
  () =>
    invoke($0, `gt`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1809
assert_return(
  () =>
    invoke($0, `gt`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1810
assert_return(
  () =>
    invoke($0, `gt`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1811
assert_return(
  () =>
    invoke($0, `gt`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1812
assert_return(
  () =>
    invoke($0, `gt`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1813
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1814
assert_return(
  () => invoke($0, `gt`, [value("f32", -6.2831855), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1815
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1816
assert_return(
  () => invoke($0, `gt`, [value("f32", 6.2831855), value("f32", 0)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1817
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -6.2831855),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1818
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -6.2831855),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1819
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 6.2831855),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1820
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 6.2831855),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1821
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -6.2831855),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1822
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -6.2831855),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1823
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 6.2831855),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1824
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 6.2831855),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1825
assert_return(
  () => invoke($0, `gt`, [value("f32", -6.2831855), value("f32", -0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1826
assert_return(
  () => invoke($0, `gt`, [value("f32", -6.2831855), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1827
assert_return(
  () => invoke($0, `gt`, [value("f32", 6.2831855), value("f32", -0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1828
assert_return(
  () => invoke($0, `gt`, [value("f32", 6.2831855), value("f32", 0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1829
assert_return(
  () => invoke($0, `gt`, [value("f32", -6.2831855), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1830
assert_return(
  () => invoke($0, `gt`, [value("f32", -6.2831855), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1831
assert_return(
  () => invoke($0, `gt`, [value("f32", 6.2831855), value("f32", -1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1832
assert_return(
  () => invoke($0, `gt`, [value("f32", 6.2831855), value("f32", 1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1833
assert_return(
  () => invoke($0, `gt`, [value("f32", -6.2831855), value("f32", -6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1834
assert_return(
  () => invoke($0, `gt`, [value("f32", -6.2831855), value("f32", 6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1835
assert_return(
  () => invoke($0, `gt`, [value("f32", 6.2831855), value("f32", -6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1836
assert_return(
  () => invoke($0, `gt`, [value("f32", 6.2831855), value("f32", 6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1837
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -6.2831855),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1838
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -6.2831855),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1839
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 6.2831855),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1840
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 6.2831855),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1841
assert_return(
  () => invoke($0, `gt`, [value("f32", -6.2831855), value("f32", -Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1842
assert_return(
  () => invoke($0, `gt`, [value("f32", -6.2831855), value("f32", Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1843
assert_return(
  () => invoke($0, `gt`, [value("f32", 6.2831855), value("f32", -Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1844
assert_return(
  () => invoke($0, `gt`, [value("f32", 6.2831855), value("f32", Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1845
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1846
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1847
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1848
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1849
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1850
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1851
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1852
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1853
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1854
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1855
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1856
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1857
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1858
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1859
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1860
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1861
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1862
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1863
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1864
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1865
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1866
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1867
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1868
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1869
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1870
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1871
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1872
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1873
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1874
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1875
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1876
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1877
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1878
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1879
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1880
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1881
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1882
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1883
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1884
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1885
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1886
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1887
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1888
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1889
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1890
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1891
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1892
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1893
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1894
assert_return(
  () => invoke($0, `gt`, [value("f32", -Infinity), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1895
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1896
assert_return(
  () => invoke($0, `gt`, [value("f32", Infinity), value("f32", 0)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1897
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -Infinity),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1898
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -Infinity),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1899
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", Infinity),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1900
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", Infinity),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1901
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -Infinity),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1902
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -Infinity),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1903
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", Infinity),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1904
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", Infinity),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1905
assert_return(
  () => invoke($0, `gt`, [value("f32", -Infinity), value("f32", -0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1906
assert_return(
  () => invoke($0, `gt`, [value("f32", -Infinity), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1907
assert_return(
  () => invoke($0, `gt`, [value("f32", Infinity), value("f32", -0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1908
assert_return(
  () => invoke($0, `gt`, [value("f32", Infinity), value("f32", 0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1909
assert_return(
  () => invoke($0, `gt`, [value("f32", -Infinity), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1910
assert_return(
  () => invoke($0, `gt`, [value("f32", -Infinity), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1911
assert_return(
  () => invoke($0, `gt`, [value("f32", Infinity), value("f32", -1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1912
assert_return(
  () => invoke($0, `gt`, [value("f32", Infinity), value("f32", 1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1913
assert_return(
  () => invoke($0, `gt`, [value("f32", -Infinity), value("f32", -6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1914
assert_return(
  () => invoke($0, `gt`, [value("f32", -Infinity), value("f32", 6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1915
assert_return(
  () => invoke($0, `gt`, [value("f32", Infinity), value("f32", -6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1916
assert_return(
  () => invoke($0, `gt`, [value("f32", Infinity), value("f32", 6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1917
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -Infinity),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1918
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -Infinity),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1919
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", Infinity),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1920
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", Infinity),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1921
assert_return(
  () => invoke($0, `gt`, [value("f32", -Infinity), value("f32", -Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1922
assert_return(
  () => invoke($0, `gt`, [value("f32", -Infinity), value("f32", Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1923
assert_return(
  () => invoke($0, `gt`, [value("f32", Infinity), value("f32", -Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:1924
assert_return(
  () => invoke($0, `gt`, [value("f32", Infinity), value("f32", Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1925
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1926
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1927
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1928
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1929
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1930
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1931
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1932
assert_return(
  () =>
    invoke($0, `gt`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1933
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1934
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1935
assert_return(
  () =>
    invoke($0, `gt`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1936
assert_return(
  () =>
    invoke($0, `gt`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1937
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1938
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1939
assert_return(
  () =>
    invoke($0, `gt`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1940
assert_return(
  () =>
    invoke($0, `gt`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1941
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1942
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1943
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1944
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1945
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1946
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1947
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1948
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1949
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1950
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1951
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1952
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1953
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1954
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1955
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1956
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1957
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1958
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1959
assert_return(
  () =>
    invoke($0, `gt`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1960
assert_return(
  () =>
    invoke($0, `gt`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1961
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1962
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1963
assert_return(
  () =>
    invoke($0, `gt`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1964
assert_return(
  () =>
    invoke($0, `gt`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1965
assert_return(
  () =>
    invoke($0, `gt`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1966
assert_return(
  () =>
    invoke($0, `gt`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1967
assert_return(
  () =>
    invoke($0, `gt`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1968
assert_return(
  () =>
    invoke($0, `gt`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1969
assert_return(
  () =>
    invoke($0, `gt`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1970
assert_return(
  () =>
    invoke($0, `gt`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1971
assert_return(
  () =>
    invoke($0, `gt`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1972
assert_return(
  () =>
    invoke($0, `gt`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1973
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1974
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1975
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1976
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1977
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1978
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1979
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1980
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1981
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1982
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1983
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1984
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1985
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1986
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1987
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1988
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1989
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1990
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1991
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1992
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1993
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1994
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1995
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1996
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1997
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1998
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:1999
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2000
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2001
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2002
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2003
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2004
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2005
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2006
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2007
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2008
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2009
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2010
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2011
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2012
assert_return(
  () =>
    invoke($0, `gt`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2013
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2014
assert_return(
  () =>
    invoke($0, `ge`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", 0)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2015
assert_return(
  () =>
    invoke($0, `ge`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2016
assert_return(() => invoke($0, `ge`, [value("f32", 0), value("f32", 0)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:2017
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2018
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2019
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2020
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2021
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2022
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2023
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2024
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2025
assert_return(
  () =>
    invoke($0, `ge`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", -0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2026
assert_return(
  () =>
    invoke($0, `ge`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2027
assert_return(() => invoke($0, `ge`, [value("f32", 0), value("f32", -0.5)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:2028
assert_return(() => invoke($0, `ge`, [value("f32", 0), value("f32", 0.5)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:2029
assert_return(
  () =>
    invoke($0, `ge`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", -1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2030
assert_return(
  () =>
    invoke($0, `ge`, [bytes("f32", [0x0, 0x0, 0x0, 0x80]), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2031
assert_return(() => invoke($0, `ge`, [value("f32", 0), value("f32", -1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:2032
assert_return(() => invoke($0, `ge`, [value("f32", 0), value("f32", 1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:2033
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2034
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2035
assert_return(
  () => invoke($0, `ge`, [value("f32", 0), value("f32", -6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2036
assert_return(
  () => invoke($0, `ge`, [value("f32", 0), value("f32", 6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2037
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2038
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2039
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2040
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2041
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", -Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2042
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2043
assert_return(
  () => invoke($0, `ge`, [value("f32", 0), value("f32", -Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2044
assert_return(
  () => invoke($0, `ge`, [value("f32", 0), value("f32", Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2045
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2046
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2047
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2048
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2049
assert_return(
  () =>
    invoke($0, `ge`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2050
assert_return(
  () =>
    invoke($0, `ge`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2051
assert_return(
  () =>
    invoke($0, `ge`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2052
assert_return(
  () =>
    invoke($0, `ge`, [value("f32", 0), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2053
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2054
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2055
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2056
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2057
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2058
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2059
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2060
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2061
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2062
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2063
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2064
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2065
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2066
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2067
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2068
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2069
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2070
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2071
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2072
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2073
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2074
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2075
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2076
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2077
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2078
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2079
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2080
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2081
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", -Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2082
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2083
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", -Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2084
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2085
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2086
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2087
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2088
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2089
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2090
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2091
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2092
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000000000001),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2093
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2094
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2095
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2096
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2097
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2098
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2099
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2100
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2101
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2102
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2103
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2104
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2105
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2106
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2107
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2108
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2109
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2110
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2111
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2112
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2113
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2114
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2115
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2116
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2117
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2118
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2119
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2120
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2121
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", -Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2122
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2123
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", -Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2124
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2125
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2126
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2127
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2128
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2129
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2130
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2131
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2132
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.000000000000000000000000000000000000011754944),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2133
assert_return(
  () =>
    invoke($0, `ge`, [value("f32", -0.5), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2134
assert_return(() => invoke($0, `ge`, [value("f32", -0.5), value("f32", 0)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:2135
assert_return(
  () =>
    invoke($0, `ge`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2136
assert_return(() => invoke($0, `ge`, [value("f32", 0.5), value("f32", 0)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:2137
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.5),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2138
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.5),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2139
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.5),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2140
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.5),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2141
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.5),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2142
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.5),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2143
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.5),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2144
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.5),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2145
assert_return(
  () => invoke($0, `ge`, [value("f32", -0.5), value("f32", -0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2146
assert_return(() => invoke($0, `ge`, [value("f32", -0.5), value("f32", 0.5)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:2147
assert_return(() => invoke($0, `ge`, [value("f32", 0.5), value("f32", -0.5)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:2148
assert_return(() => invoke($0, `ge`, [value("f32", 0.5), value("f32", 0.5)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:2149
assert_return(() => invoke($0, `ge`, [value("f32", -0.5), value("f32", -1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:2150
assert_return(() => invoke($0, `ge`, [value("f32", -0.5), value("f32", 1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:2151
assert_return(() => invoke($0, `ge`, [value("f32", 0.5), value("f32", -1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:2152
assert_return(() => invoke($0, `ge`, [value("f32", 0.5), value("f32", 1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:2153
assert_return(
  () => invoke($0, `ge`, [value("f32", -0.5), value("f32", -6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2154
assert_return(
  () => invoke($0, `ge`, [value("f32", -0.5), value("f32", 6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2155
assert_return(
  () => invoke($0, `ge`, [value("f32", 0.5), value("f32", -6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2156
assert_return(
  () => invoke($0, `ge`, [value("f32", 0.5), value("f32", 6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2157
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.5),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2158
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.5),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2159
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.5),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2160
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 0.5),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2161
assert_return(
  () => invoke($0, `ge`, [value("f32", -0.5), value("f32", -Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2162
assert_return(
  () => invoke($0, `ge`, [value("f32", -0.5), value("f32", Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2163
assert_return(
  () => invoke($0, `ge`, [value("f32", 0.5), value("f32", -Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2164
assert_return(
  () => invoke($0, `ge`, [value("f32", 0.5), value("f32", Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2165
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2166
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2167
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2168
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -0.5),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2169
assert_return(
  () =>
    invoke($0, `ge`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2170
assert_return(
  () =>
    invoke($0, `ge`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2171
assert_return(
  () =>
    invoke($0, `ge`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2172
assert_return(
  () =>
    invoke($0, `ge`, [value("f32", 0.5), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2173
assert_return(
  () =>
    invoke($0, `ge`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2174
assert_return(() => invoke($0, `ge`, [value("f32", -1), value("f32", 0)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:2175
assert_return(
  () =>
    invoke($0, `ge`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0x0, 0x80])]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2176
assert_return(() => invoke($0, `ge`, [value("f32", 1), value("f32", 0)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:2177
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -1),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2178
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -1),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2179
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 1),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2180
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 1),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2181
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -1),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2182
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -1),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2183
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 1),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2184
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 1),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2185
assert_return(() => invoke($0, `ge`, [value("f32", -1), value("f32", -0.5)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:2186
assert_return(() => invoke($0, `ge`, [value("f32", -1), value("f32", 0.5)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:2187
assert_return(() => invoke($0, `ge`, [value("f32", 1), value("f32", -0.5)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:2188
assert_return(() => invoke($0, `ge`, [value("f32", 1), value("f32", 0.5)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:2189
assert_return(() => invoke($0, `ge`, [value("f32", -1), value("f32", -1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:2190
assert_return(() => invoke($0, `ge`, [value("f32", -1), value("f32", 1)]), [
  value("i32", 0),
]);

// ./test/core/f32_cmp.wast:2191
assert_return(() => invoke($0, `ge`, [value("f32", 1), value("f32", -1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:2192
assert_return(() => invoke($0, `ge`, [value("f32", 1), value("f32", 1)]), [
  value("i32", 1),
]);

// ./test/core/f32_cmp.wast:2193
assert_return(
  () => invoke($0, `ge`, [value("f32", -1), value("f32", -6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2194
assert_return(
  () => invoke($0, `ge`, [value("f32", -1), value("f32", 6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2195
assert_return(
  () => invoke($0, `ge`, [value("f32", 1), value("f32", -6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2196
assert_return(
  () => invoke($0, `ge`, [value("f32", 1), value("f32", 6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2197
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -1),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2198
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -1),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2199
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 1),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2200
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 1),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2201
assert_return(
  () => invoke($0, `ge`, [value("f32", -1), value("f32", -Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2202
assert_return(
  () => invoke($0, `ge`, [value("f32", -1), value("f32", Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2203
assert_return(
  () => invoke($0, `ge`, [value("f32", 1), value("f32", -Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2204
assert_return(
  () => invoke($0, `ge`, [value("f32", 1), value("f32", Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2205
assert_return(
  () =>
    invoke($0, `ge`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2206
assert_return(
  () =>
    invoke($0, `ge`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2207
assert_return(
  () =>
    invoke($0, `ge`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2208
assert_return(
  () =>
    invoke($0, `ge`, [value("f32", -1), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2209
assert_return(
  () =>
    invoke($0, `ge`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xc0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2210
assert_return(
  () =>
    invoke($0, `ge`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xa0, 0xff])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2211
assert_return(
  () =>
    invoke($0, `ge`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xc0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2212
assert_return(
  () =>
    invoke($0, `ge`, [value("f32", 1), bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2213
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2214
assert_return(
  () => invoke($0, `ge`, [value("f32", -6.2831855), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2215
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2216
assert_return(
  () => invoke($0, `ge`, [value("f32", 6.2831855), value("f32", 0)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2217
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -6.2831855),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2218
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -6.2831855),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2219
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 6.2831855),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2220
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 6.2831855),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2221
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -6.2831855),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2222
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -6.2831855),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2223
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 6.2831855),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2224
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 6.2831855),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2225
assert_return(
  () => invoke($0, `ge`, [value("f32", -6.2831855), value("f32", -0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2226
assert_return(
  () => invoke($0, `ge`, [value("f32", -6.2831855), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2227
assert_return(
  () => invoke($0, `ge`, [value("f32", 6.2831855), value("f32", -0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2228
assert_return(
  () => invoke($0, `ge`, [value("f32", 6.2831855), value("f32", 0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2229
assert_return(
  () => invoke($0, `ge`, [value("f32", -6.2831855), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2230
assert_return(
  () => invoke($0, `ge`, [value("f32", -6.2831855), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2231
assert_return(
  () => invoke($0, `ge`, [value("f32", 6.2831855), value("f32", -1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2232
assert_return(
  () => invoke($0, `ge`, [value("f32", 6.2831855), value("f32", 1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2233
assert_return(
  () => invoke($0, `ge`, [value("f32", -6.2831855), value("f32", -6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2234
assert_return(
  () => invoke($0, `ge`, [value("f32", -6.2831855), value("f32", 6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2235
assert_return(
  () => invoke($0, `ge`, [value("f32", 6.2831855), value("f32", -6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2236
assert_return(
  () => invoke($0, `ge`, [value("f32", 6.2831855), value("f32", 6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2237
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -6.2831855),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2238
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -6.2831855),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2239
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 6.2831855),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2240
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 6.2831855),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2241
assert_return(
  () => invoke($0, `ge`, [value("f32", -6.2831855), value("f32", -Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2242
assert_return(
  () => invoke($0, `ge`, [value("f32", -6.2831855), value("f32", Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2243
assert_return(
  () => invoke($0, `ge`, [value("f32", 6.2831855), value("f32", -Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2244
assert_return(
  () => invoke($0, `ge`, [value("f32", 6.2831855), value("f32", Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2245
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2246
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2247
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2248
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2249
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2250
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2251
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2252
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 6.2831855),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2253
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2254
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2255
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2256
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2257
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2258
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2259
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2260
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2261
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2262
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2263
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2264
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2265
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2266
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2267
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2268
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 0.5),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2269
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2270
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 1),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2271
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2272
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 1),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2273
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2274
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2275
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2276
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 6.2831855),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2277
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2278
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2279
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2280
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2281
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", -Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2282
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -340282350000000000000000000000000000000),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2283
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", -Infinity),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2284
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 340282350000000000000000000000000000000),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2285
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2286
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2287
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2288
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2289
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2290
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2291
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2292
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", 340282350000000000000000000000000000000),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2293
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2294
assert_return(
  () => invoke($0, `ge`, [value("f32", -Infinity), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2295
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2296
assert_return(
  () => invoke($0, `ge`, [value("f32", Infinity), value("f32", 0)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2297
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -Infinity),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2298
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -Infinity),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2299
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", Infinity),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2300
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", Infinity),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2301
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -Infinity),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2302
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -Infinity),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2303
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", Infinity),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2304
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", Infinity),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2305
assert_return(
  () => invoke($0, `ge`, [value("f32", -Infinity), value("f32", -0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2306
assert_return(
  () => invoke($0, `ge`, [value("f32", -Infinity), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2307
assert_return(
  () => invoke($0, `ge`, [value("f32", Infinity), value("f32", -0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2308
assert_return(
  () => invoke($0, `ge`, [value("f32", Infinity), value("f32", 0.5)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2309
assert_return(
  () => invoke($0, `ge`, [value("f32", -Infinity), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2310
assert_return(
  () => invoke($0, `ge`, [value("f32", -Infinity), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2311
assert_return(
  () => invoke($0, `ge`, [value("f32", Infinity), value("f32", -1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2312
assert_return(
  () => invoke($0, `ge`, [value("f32", Infinity), value("f32", 1)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2313
assert_return(
  () => invoke($0, `ge`, [value("f32", -Infinity), value("f32", -6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2314
assert_return(
  () => invoke($0, `ge`, [value("f32", -Infinity), value("f32", 6.2831855)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2315
assert_return(
  () => invoke($0, `ge`, [value("f32", Infinity), value("f32", -6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2316
assert_return(
  () => invoke($0, `ge`, [value("f32", Infinity), value("f32", 6.2831855)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2317
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -Infinity),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2318
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -Infinity),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2319
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", Infinity),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2320
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", Infinity),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2321
assert_return(
  () => invoke($0, `ge`, [value("f32", -Infinity), value("f32", -Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2322
assert_return(
  () => invoke($0, `ge`, [value("f32", -Infinity), value("f32", Infinity)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2323
assert_return(
  () => invoke($0, `ge`, [value("f32", Infinity), value("f32", -Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2324
assert_return(
  () => invoke($0, `ge`, [value("f32", Infinity), value("f32", Infinity)]),
  [value("i32", 1)],
);

// ./test/core/f32_cmp.wast:2325
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2326
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2327
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2328
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", -Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2329
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2330
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2331
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2332
assert_return(
  () =>
    invoke($0, `ge`, [
      value("f32", Infinity),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2333
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2334
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2335
assert_return(
  () =>
    invoke($0, `ge`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2336
assert_return(
  () =>
    invoke($0, `ge`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2337
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2338
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2339
assert_return(
  () =>
    invoke($0, `ge`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2340
assert_return(
  () =>
    invoke($0, `ge`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 0)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2341
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2342
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2343
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2344
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2345
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2346
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2347
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2348
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000000000001),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2349
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2350
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2351
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2352
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2353
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2354
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2355
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2356
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 0.000000000000000000000000000000000000011754944),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2357
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2358
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2359
assert_return(
  () =>
    invoke($0, `ge`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2360
assert_return(
  () =>
    invoke($0, `ge`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2361
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2362
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -0.5),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2363
assert_return(
  () =>
    invoke($0, `ge`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2364
assert_return(
  () =>
    invoke($0, `ge`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 0.5)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2365
assert_return(
  () =>
    invoke($0, `ge`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2366
assert_return(
  () =>
    invoke($0, `ge`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2367
assert_return(
  () =>
    invoke($0, `ge`, [bytes("f32", [0x0, 0x0, 0xc0, 0xff]), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2368
assert_return(
  () =>
    invoke($0, `ge`, [bytes("f32", [0x0, 0x0, 0xa0, 0xff]), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2369
assert_return(
  () =>
    invoke($0, `ge`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2370
assert_return(
  () =>
    invoke($0, `ge`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", -1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2371
assert_return(
  () =>
    invoke($0, `ge`, [bytes("f32", [0x0, 0x0, 0xc0, 0x7f]), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2372
assert_return(
  () =>
    invoke($0, `ge`, [bytes("f32", [0x0, 0x0, 0xa0, 0x7f]), value("f32", 1)]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2373
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2374
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2375
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2376
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2377
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2378
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2379
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2380
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 6.2831855),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2381
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2382
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2383
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2384
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2385
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2386
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2387
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2388
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", 340282350000000000000000000000000000000),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2389
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2390
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2391
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2392
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2393
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2394
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", -Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2395
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2396
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      value("f32", Infinity),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2397
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2398
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2399
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2400
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2401
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2402
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2403
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2404
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2405
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2406
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2407
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2408
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0xff]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2409
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2410
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2411
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2412
assert_return(
  () =>
    invoke($0, `ge`, [
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xa0, 0x7f]),
    ]),
  [value("i32", 0)],
);

// ./test/core/f32_cmp.wast:2417
assert_invalid(
  () =>
    instantiate(
      `(module (func (result f32) (f32.eq (i64.const 0) (f64.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/f32_cmp.wast:2418
assert_invalid(
  () =>
    instantiate(
      `(module (func (result f32) (f32.ge (i64.const 0) (f64.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/f32_cmp.wast:2419
assert_invalid(
  () =>
    instantiate(
      `(module (func (result f32) (f32.gt (i64.const 0) (f64.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/f32_cmp.wast:2420
assert_invalid(
  () =>
    instantiate(
      `(module (func (result f32) (f32.le (i64.const 0) (f64.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/f32_cmp.wast:2421
assert_invalid(
  () =>
    instantiate(
      `(module (func (result f32) (f32.lt (i64.const 0) (f64.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/f32_cmp.wast:2422
assert_invalid(
  () =>
    instantiate(
      `(module (func (result f32) (f32.ne (i64.const 0) (f64.const 0))))`,
    ),
  `type mismatch`,
);
