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

// ./test/core/simd/simd_store64_lane.wast

// ./test/core/simd/simd_store64_lane.wast:4
let $0 = instantiate(`(module
  (memory 1)
  (global $$zero (mut v128) (v128.const i32x4 0 0 0 0))
  (func (export "v128.store64_lane_0")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store64_lane 0 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store (local.get $$address) (global.get $$zero))    (local.get $$ret))
  (func (export "v128.store64_lane_1")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store64_lane 1 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store (local.get $$address) (global.get $$zero))    (local.get $$ret))
  (func (export "v128.store64_lane_0_offset_0")
    (param $$x v128) (result i64) (local $$ret i64)
    (v128.store64_lane offset=0 0 (i32.const 0) (local.get $$x))
    (local.set $$ret (i64.load offset=0 (i32.const 0)))
    (v128.store offset=0 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store64_lane_1_offset_1")
    (param $$x v128) (result i64) (local $$ret i64)
    (v128.store64_lane offset=1 1 (i32.const 0) (local.get $$x))
    (local.set $$ret (i64.load offset=1 (i32.const 0)))
    (v128.store offset=1 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store64_lane_0_align_1")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store64_lane align=1 0 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=0 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store64_lane_0_align_2")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store64_lane align=2 0 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=0 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store64_lane_0_align_4")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store64_lane align=4 0 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=0 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store64_lane_0_align_8")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store64_lane align=8 0 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=0 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store64_lane_1_align_1")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store64_lane align=1 1 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=1 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store64_lane_1_align_2")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store64_lane align=2 1 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=1 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store64_lane_1_align_4")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store64_lane align=4 1 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=1 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store64_lane_1_align_8")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store64_lane align=8 1 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=1 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
)`);

// ./test/core/simd/simd_store64_lane.wast:79
assert_return(
  () =>
    invoke($0, `v128.store64_lane_0`, [0, i64x2([0x706050403020100n, 0x0n])]),
  [value("i64", 506097522914230528n)],
);

// ./test/core/simd/simd_store64_lane.wast:82
assert_return(
  () =>
    invoke($0, `v128.store64_lane_1`, [1, i64x2([0x0n, 0x807060504030201n])]),
  [value("i64", 578437695752307201n)],
);

// ./test/core/simd/simd_store64_lane.wast:85
assert_return(
  () =>
    invoke($0, `v128.store64_lane_0_offset_0`, [
      i64x2([0x706050403020100n, 0x0n]),
    ]),
  [value("i64", 506097522914230528n)],
);

// ./test/core/simd/simd_store64_lane.wast:87
assert_return(
  () =>
    invoke($0, `v128.store64_lane_1_offset_1`, [
      i64x2([0x0n, 0x807060504030201n]),
    ]),
  [value("i64", 578437695752307201n)],
);

// ./test/core/simd/simd_store64_lane.wast:89
assert_return(
  () =>
    invoke($0, `v128.store64_lane_0_align_1`, [
      0,
      i64x2([0x706050403020100n, 0x0n]),
    ]),
  [value("i64", 506097522914230528n)],
);

// ./test/core/simd/simd_store64_lane.wast:92
assert_return(
  () =>
    invoke($0, `v128.store64_lane_0_align_2`, [
      0,
      i64x2([0x706050403020100n, 0x0n]),
    ]),
  [value("i64", 506097522914230528n)],
);

// ./test/core/simd/simd_store64_lane.wast:95
assert_return(
  () =>
    invoke($0, `v128.store64_lane_0_align_4`, [
      0,
      i64x2([0x706050403020100n, 0x0n]),
    ]),
  [value("i64", 506097522914230528n)],
);

// ./test/core/simd/simd_store64_lane.wast:98
assert_return(
  () =>
    invoke($0, `v128.store64_lane_0_align_8`, [
      0,
      i64x2([0x706050403020100n, 0x0n]),
    ]),
  [value("i64", 506097522914230528n)],
);

// ./test/core/simd/simd_store64_lane.wast:101
assert_return(
  () =>
    invoke($0, `v128.store64_lane_1_align_1`, [
      1,
      i64x2([0x0n, 0x807060504030201n]),
    ]),
  [value("i64", 578437695752307201n)],
);

// ./test/core/simd/simd_store64_lane.wast:104
assert_return(
  () =>
    invoke($0, `v128.store64_lane_1_align_2`, [
      1,
      i64x2([0x0n, 0x807060504030201n]),
    ]),
  [value("i64", 578437695752307201n)],
);

// ./test/core/simd/simd_store64_lane.wast:107
assert_return(
  () =>
    invoke($0, `v128.store64_lane_1_align_4`, [
      1,
      i64x2([0x0n, 0x807060504030201n]),
    ]),
  [value("i64", 578437695752307201n)],
);

// ./test/core/simd/simd_store64_lane.wast:110
assert_return(
  () =>
    invoke($0, `v128.store64_lane_1_align_8`, [
      1,
      i64x2([0x0n, 0x807060504030201n]),
    ]),
  [value("i64", 578437695752307201n)],
);

// ./test/core/simd/simd_store64_lane.wast:115
assert_invalid(
  () =>
    instantiate(`(module (memory 1)
          (func (param $$x v128) (result v128)
            (v128.store64_lane 0 (local.get $$x) (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/simd/simd_store64_lane.wast:121
assert_invalid(
  () =>
    instantiate(`(module (memory 1)
          (func (param $$x v128) (result v128)
            (v128.store64_lane 2 (i32.const 0) (local.get $$x))))`),
  `invalid lane index`,
);

// ./test/core/simd/simd_store64_lane.wast:127
assert_invalid(
  () =>
    instantiate(`(module (memory 1)
          (func (param $$x v128) (result v128)
          (v128.store64_lane align=16 0 (i32.const 0) (local.get $$x))))`),
  `alignment must not be larger than natural`,
);
