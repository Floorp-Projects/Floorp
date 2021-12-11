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

// ./test/core/traps.wast

// ./test/core/traps.wast:5
let $0 = instantiate(`(module
  (func (export "no_dce.i32.div_s") (param $$x i32) (param $$y i32)
    (drop (i32.div_s (local.get $$x) (local.get $$y))))
  (func (export "no_dce.i32.div_u") (param $$x i32) (param $$y i32)
    (drop (i32.div_u (local.get $$x) (local.get $$y))))
  (func (export "no_dce.i64.div_s") (param $$x i64) (param $$y i64)
    (drop (i64.div_s (local.get $$x) (local.get $$y))))
  (func (export "no_dce.i64.div_u") (param $$x i64) (param $$y i64)
    (drop (i64.div_u (local.get $$x) (local.get $$y))))
)`);

// ./test/core/traps.wast:16
assert_trap(
  () => invoke($0, `no_dce.i32.div_s`, [1, 0]),
  `integer divide by zero`,
);

// ./test/core/traps.wast:17
assert_trap(
  () => invoke($0, `no_dce.i32.div_u`, [1, 0]),
  `integer divide by zero`,
);

// ./test/core/traps.wast:18
assert_trap(
  () => invoke($0, `no_dce.i64.div_s`, [1n, 0n]),
  `integer divide by zero`,
);

// ./test/core/traps.wast:19
assert_trap(
  () => invoke($0, `no_dce.i64.div_u`, [1n, 0n]),
  `integer divide by zero`,
);

// ./test/core/traps.wast:20
assert_trap(
  () => invoke($0, `no_dce.i32.div_s`, [-2147483648, -1]),
  `integer overflow`,
);

// ./test/core/traps.wast:21
assert_trap(
  () => invoke($0, `no_dce.i64.div_s`, [-9223372036854775808n, -1n]),
  `integer overflow`,
);

// ./test/core/traps.wast:23
let $1 = instantiate(`(module
  (func (export "no_dce.i32.rem_s") (param $$x i32) (param $$y i32)
    (drop (i32.rem_s (local.get $$x) (local.get $$y))))
  (func (export "no_dce.i32.rem_u") (param $$x i32) (param $$y i32)
    (drop (i32.rem_u (local.get $$x) (local.get $$y))))
  (func (export "no_dce.i64.rem_s") (param $$x i64) (param $$y i64)
    (drop (i64.rem_s (local.get $$x) (local.get $$y))))
  (func (export "no_dce.i64.rem_u") (param $$x i64) (param $$y i64)
    (drop (i64.rem_u (local.get $$x) (local.get $$y))))
)`);

// ./test/core/traps.wast:34
assert_trap(
  () => invoke($1, `no_dce.i32.rem_s`, [1, 0]),
  `integer divide by zero`,
);

// ./test/core/traps.wast:35
assert_trap(
  () => invoke($1, `no_dce.i32.rem_u`, [1, 0]),
  `integer divide by zero`,
);

// ./test/core/traps.wast:36
assert_trap(
  () => invoke($1, `no_dce.i64.rem_s`, [1n, 0n]),
  `integer divide by zero`,
);

// ./test/core/traps.wast:37
assert_trap(
  () => invoke($1, `no_dce.i64.rem_u`, [1n, 0n]),
  `integer divide by zero`,
);

// ./test/core/traps.wast:39
let $2 = instantiate(`(module
  (func (export "no_dce.i32.trunc_f32_s") (param $$x f32) (drop (i32.trunc_f32_s (local.get $$x))))
  (func (export "no_dce.i32.trunc_f32_u") (param $$x f32) (drop (i32.trunc_f32_u (local.get $$x))))
  (func (export "no_dce.i32.trunc_f64_s") (param $$x f64) (drop (i32.trunc_f64_s (local.get $$x))))
  (func (export "no_dce.i32.trunc_f64_u") (param $$x f64) (drop (i32.trunc_f64_u (local.get $$x))))
  (func (export "no_dce.i64.trunc_f32_s") (param $$x f32) (drop (i64.trunc_f32_s (local.get $$x))))
  (func (export "no_dce.i64.trunc_f32_u") (param $$x f32) (drop (i64.trunc_f32_u (local.get $$x))))
  (func (export "no_dce.i64.trunc_f64_s") (param $$x f64) (drop (i64.trunc_f64_s (local.get $$x))))
  (func (export "no_dce.i64.trunc_f64_u") (param $$x f64) (drop (i64.trunc_f64_u (local.get $$x))))
)`);

// ./test/core/traps.wast:50
assert_trap(
  () =>
    invoke($2, `no_dce.i32.trunc_f32_s`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  `invalid conversion to integer`,
);

// ./test/core/traps.wast:51
assert_trap(
  () =>
    invoke($2, `no_dce.i32.trunc_f32_u`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  `invalid conversion to integer`,
);

// ./test/core/traps.wast:52
assert_trap(
  () =>
    invoke($2, `no_dce.i32.trunc_f64_s`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
    ]),
  `invalid conversion to integer`,
);

// ./test/core/traps.wast:53
assert_trap(
  () =>
    invoke($2, `no_dce.i32.trunc_f64_u`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
    ]),
  `invalid conversion to integer`,
);

// ./test/core/traps.wast:54
assert_trap(
  () =>
    invoke($2, `no_dce.i64.trunc_f32_s`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  `invalid conversion to integer`,
);

// ./test/core/traps.wast:55
assert_trap(
  () =>
    invoke($2, `no_dce.i64.trunc_f32_u`, [
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ]),
  `invalid conversion to integer`,
);

// ./test/core/traps.wast:56
assert_trap(
  () =>
    invoke($2, `no_dce.i64.trunc_f64_s`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
    ]),
  `invalid conversion to integer`,
);

// ./test/core/traps.wast:57
assert_trap(
  () =>
    invoke($2, `no_dce.i64.trunc_f64_u`, [
      bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf8, 0x7f]),
    ]),
  `invalid conversion to integer`,
);

// ./test/core/traps.wast:59
let $3 = instantiate(`(module
    (memory 1)

    (func (export "no_dce.i32.load") (param $$i i32) (drop (i32.load (local.get $$i))))
    (func (export "no_dce.i32.load16_s") (param $$i i32) (drop (i32.load16_s (local.get $$i))))
    (func (export "no_dce.i32.load16_u") (param $$i i32) (drop (i32.load16_u (local.get $$i))))
    (func (export "no_dce.i32.load8_s") (param $$i i32) (drop (i32.load8_s (local.get $$i))))
    (func (export "no_dce.i32.load8_u") (param $$i i32) (drop (i32.load8_u (local.get $$i))))
    (func (export "no_dce.i64.load") (param $$i i32) (drop (i64.load (local.get $$i))))
    (func (export "no_dce.i64.load32_s") (param $$i i32) (drop (i64.load32_s (local.get $$i))))
    (func (export "no_dce.i64.load32_u") (param $$i i32) (drop (i64.load32_u (local.get $$i))))
    (func (export "no_dce.i64.load16_s") (param $$i i32) (drop (i64.load16_s (local.get $$i))))
    (func (export "no_dce.i64.load16_u") (param $$i i32) (drop (i64.load16_u (local.get $$i))))
    (func (export "no_dce.i64.load8_s") (param $$i i32) (drop (i64.load8_s (local.get $$i))))
    (func (export "no_dce.i64.load8_u") (param $$i i32) (drop (i64.load8_u (local.get $$i))))
    (func (export "no_dce.f32.load") (param $$i i32) (drop (f32.load (local.get $$i))))
    (func (export "no_dce.f64.load") (param $$i i32) (drop (f64.load (local.get $$i))))
)`);

// ./test/core/traps.wast:78
assert_trap(
  () => invoke($3, `no_dce.i32.load`, [65536]),
  `out of bounds memory access`,
);

// ./test/core/traps.wast:79
assert_trap(
  () => invoke($3, `no_dce.i32.load16_s`, [65536]),
  `out of bounds memory access`,
);

// ./test/core/traps.wast:80
assert_trap(
  () => invoke($3, `no_dce.i32.load16_u`, [65536]),
  `out of bounds memory access`,
);

// ./test/core/traps.wast:81
assert_trap(
  () => invoke($3, `no_dce.i32.load8_s`, [65536]),
  `out of bounds memory access`,
);

// ./test/core/traps.wast:82
assert_trap(
  () => invoke($3, `no_dce.i32.load8_u`, [65536]),
  `out of bounds memory access`,
);

// ./test/core/traps.wast:83
assert_trap(
  () => invoke($3, `no_dce.i64.load`, [65536]),
  `out of bounds memory access`,
);

// ./test/core/traps.wast:84
assert_trap(
  () => invoke($3, `no_dce.i64.load32_s`, [65536]),
  `out of bounds memory access`,
);

// ./test/core/traps.wast:85
assert_trap(
  () => invoke($3, `no_dce.i64.load32_u`, [65536]),
  `out of bounds memory access`,
);

// ./test/core/traps.wast:86
assert_trap(
  () => invoke($3, `no_dce.i64.load16_s`, [65536]),
  `out of bounds memory access`,
);

// ./test/core/traps.wast:87
assert_trap(
  () => invoke($3, `no_dce.i64.load16_u`, [65536]),
  `out of bounds memory access`,
);

// ./test/core/traps.wast:88
assert_trap(
  () => invoke($3, `no_dce.i64.load8_s`, [65536]),
  `out of bounds memory access`,
);

// ./test/core/traps.wast:89
assert_trap(
  () => invoke($3, `no_dce.i64.load8_u`, [65536]),
  `out of bounds memory access`,
);

// ./test/core/traps.wast:90
assert_trap(
  () => invoke($3, `no_dce.f32.load`, [65536]),
  `out of bounds memory access`,
);

// ./test/core/traps.wast:91
assert_trap(
  () => invoke($3, `no_dce.f64.load`, [65536]),
  `out of bounds memory access`,
);
