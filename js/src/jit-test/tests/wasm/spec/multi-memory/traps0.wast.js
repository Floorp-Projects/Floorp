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

// ./test/core/multi-memory/traps0.wast

// ./test/core/multi-memory/traps0.wast:1
let $0 = instantiate(`(module
    (memory $$mem0 1)
    (memory $$mem1 1)
    (memory $$mem2 1)

    (func (export "no_dce.i32.load") (param $$i i32) (drop (i32.load $$mem1 (local.get $$i))))
    (func (export "no_dce.i32.load16_s") (param $$i i32) (drop (i32.load16_s $$mem1 (local.get $$i))))
    (func (export "no_dce.i32.load16_u") (param $$i i32) (drop (i32.load16_u $$mem1 (local.get $$i))))
    (func (export "no_dce.i32.load8_s") (param $$i i32) (drop (i32.load8_s $$mem1 (local.get $$i))))
    (func (export "no_dce.i32.load8_u") (param $$i i32) (drop (i32.load8_u $$mem1 (local.get $$i))))
    (func (export "no_dce.i64.load") (param $$i i32) (drop (i64.load $$mem1 (local.get $$i))))
    (func (export "no_dce.i64.load32_s") (param $$i i32) (drop (i64.load32_s $$mem1 (local.get $$i))))
    (func (export "no_dce.i64.load32_u") (param $$i i32) (drop (i64.load32_u $$mem2 (local.get $$i))))
    (func (export "no_dce.i64.load16_s") (param $$i i32) (drop (i64.load16_s $$mem2 (local.get $$i))))
    (func (export "no_dce.i64.load16_u") (param $$i i32) (drop (i64.load16_u $$mem2 (local.get $$i))))
    (func (export "no_dce.i64.load8_s") (param $$i i32) (drop (i64.load8_s $$mem2 (local.get $$i))))
    (func (export "no_dce.i64.load8_u") (param $$i i32) (drop (i64.load8_u $$mem2 (local.get $$i))))
    (func (export "no_dce.f32.load") (param $$i i32) (drop (f32.load $$mem2 (local.get $$i))))
    (func (export "no_dce.f64.load") (param $$i i32) (drop (f64.load $$mem2 (local.get $$i))))
)`);

// ./test/core/multi-memory/traps0.wast:22
assert_trap(() => invoke($0, `no_dce.i32.load`, [65536]), `out of bounds memory access`);

// ./test/core/multi-memory/traps0.wast:23
assert_trap(() => invoke($0, `no_dce.i32.load16_s`, [65536]), `out of bounds memory access`);

// ./test/core/multi-memory/traps0.wast:24
assert_trap(() => invoke($0, `no_dce.i32.load16_u`, [65536]), `out of bounds memory access`);

// ./test/core/multi-memory/traps0.wast:25
assert_trap(() => invoke($0, `no_dce.i32.load8_s`, [65536]), `out of bounds memory access`);

// ./test/core/multi-memory/traps0.wast:26
assert_trap(() => invoke($0, `no_dce.i32.load8_u`, [65536]), `out of bounds memory access`);

// ./test/core/multi-memory/traps0.wast:27
assert_trap(() => invoke($0, `no_dce.i64.load`, [65536]), `out of bounds memory access`);

// ./test/core/multi-memory/traps0.wast:28
assert_trap(() => invoke($0, `no_dce.i64.load32_s`, [65536]), `out of bounds memory access`);

// ./test/core/multi-memory/traps0.wast:29
assert_trap(() => invoke($0, `no_dce.i64.load32_u`, [65536]), `out of bounds memory access`);

// ./test/core/multi-memory/traps0.wast:30
assert_trap(() => invoke($0, `no_dce.i64.load16_s`, [65536]), `out of bounds memory access`);

// ./test/core/multi-memory/traps0.wast:31
assert_trap(() => invoke($0, `no_dce.i64.load16_u`, [65536]), `out of bounds memory access`);

// ./test/core/multi-memory/traps0.wast:32
assert_trap(() => invoke($0, `no_dce.i64.load8_s`, [65536]), `out of bounds memory access`);

// ./test/core/multi-memory/traps0.wast:33
assert_trap(() => invoke($0, `no_dce.i64.load8_u`, [65536]), `out of bounds memory access`);

// ./test/core/multi-memory/traps0.wast:34
assert_trap(() => invoke($0, `no_dce.f32.load`, [65536]), `out of bounds memory access`);

// ./test/core/multi-memory/traps0.wast:35
assert_trap(() => invoke($0, `no_dce.f64.load`, [65536]), `out of bounds memory access`);
