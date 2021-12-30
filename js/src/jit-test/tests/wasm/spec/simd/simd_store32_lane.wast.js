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

// ./test/core/simd/simd_store32_lane.wast

// ./test/core/simd/simd_store32_lane.wast:4
let $0 = instantiate(`(module
  (memory 1)
  (global $$zero (mut v128) (v128.const i32x4 0 0 0 0))
  (func (export "v128.store32_lane_0")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store32_lane 0 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store (local.get $$address) (global.get $$zero))    (local.get $$ret))
  (func (export "v128.store32_lane_1")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store32_lane 1 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store (local.get $$address) (global.get $$zero))    (local.get $$ret))
  (func (export "v128.store32_lane_2")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store32_lane 2 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store (local.get $$address) (global.get $$zero))    (local.get $$ret))
  (func (export "v128.store32_lane_3")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store32_lane 3 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store (local.get $$address) (global.get $$zero))    (local.get $$ret))
  (func (export "v128.store32_lane_0_offset_0")
    (param $$x v128) (result i64) (local $$ret i64)
    (v128.store32_lane offset=0 0 (i32.const 0) (local.get $$x))
    (local.set $$ret (i64.load offset=0 (i32.const 0)))
    (v128.store offset=0 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store32_lane_1_offset_1")
    (param $$x v128) (result i64) (local $$ret i64)
    (v128.store32_lane offset=1 1 (i32.const 0) (local.get $$x))
    (local.set $$ret (i64.load offset=1 (i32.const 0)))
    (v128.store offset=1 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store32_lane_2_offset_2")
    (param $$x v128) (result i64) (local $$ret i64)
    (v128.store32_lane offset=2 2 (i32.const 0) (local.get $$x))
    (local.set $$ret (i64.load offset=2 (i32.const 0)))
    (v128.store offset=2 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store32_lane_3_offset_3")
    (param $$x v128) (result i64) (local $$ret i64)
    (v128.store32_lane offset=3 3 (i32.const 0) (local.get $$x))
    (local.set $$ret (i64.load offset=3 (i32.const 0)))
    (v128.store offset=3 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store32_lane_0_align_1")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store32_lane align=1 0 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=0 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store32_lane_0_align_2")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store32_lane align=2 0 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=0 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store32_lane_0_align_4")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store32_lane align=4 0 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=0 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store32_lane_1_align_1")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store32_lane align=1 1 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=1 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store32_lane_1_align_2")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store32_lane align=2 1 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=1 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store32_lane_1_align_4")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store32_lane align=4 1 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=1 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store32_lane_2_align_1")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store32_lane align=1 2 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=2 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store32_lane_2_align_2")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store32_lane align=2 2 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=2 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store32_lane_2_align_4")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store32_lane align=4 2 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=2 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store32_lane_3_align_1")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store32_lane align=1 3 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=3 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store32_lane_3_align_2")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store32_lane align=2 3 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=3 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store32_lane_3_align_4")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store32_lane align=4 3 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=3 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
)`);

// ./test/core/simd/simd_store32_lane.wast:125
assert_return(
  () =>
    invoke($0, `v128.store32_lane_0`, [0, i32x4([0x3020100, 0x0, 0x0, 0x0])]),
  [value("i64", 50462976n)],
);

// ./test/core/simd/simd_store32_lane.wast:128
assert_return(
  () =>
    invoke($0, `v128.store32_lane_1`, [1, i32x4([0x0, 0x4030201, 0x0, 0x0])]),
  [value("i64", 67305985n)],
);

// ./test/core/simd/simd_store32_lane.wast:131
assert_return(
  () =>
    invoke($0, `v128.store32_lane_2`, [2, i32x4([0x0, 0x0, 0x5040302, 0x0])]),
  [value("i64", 84148994n)],
);

// ./test/core/simd/simd_store32_lane.wast:134
assert_return(
  () =>
    invoke($0, `v128.store32_lane_3`, [3, i32x4([0x0, 0x0, 0x0, 0x6050403])]),
  [value("i64", 100992003n)],
);

// ./test/core/simd/simd_store32_lane.wast:137
assert_return(
  () =>
    invoke($0, `v128.store32_lane_0_offset_0`, [
      i32x4([0x3020100, 0x0, 0x0, 0x0]),
    ]),
  [value("i64", 50462976n)],
);

// ./test/core/simd/simd_store32_lane.wast:139
assert_return(
  () =>
    invoke($0, `v128.store32_lane_1_offset_1`, [
      i32x4([0x0, 0x4030201, 0x0, 0x0]),
    ]),
  [value("i64", 67305985n)],
);

// ./test/core/simd/simd_store32_lane.wast:141
assert_return(
  () =>
    invoke($0, `v128.store32_lane_2_offset_2`, [
      i32x4([0x0, 0x0, 0x5040302, 0x0]),
    ]),
  [value("i64", 84148994n)],
);

// ./test/core/simd/simd_store32_lane.wast:143
assert_return(
  () =>
    invoke($0, `v128.store32_lane_3_offset_3`, [
      i32x4([0x0, 0x0, 0x0, 0x6050403]),
    ]),
  [value("i64", 100992003n)],
);

// ./test/core/simd/simd_store32_lane.wast:145
assert_return(
  () =>
    invoke($0, `v128.store32_lane_0_align_1`, [
      0,
      i32x4([0x3020100, 0x0, 0x0, 0x0]),
    ]),
  [value("i64", 50462976n)],
);

// ./test/core/simd/simd_store32_lane.wast:148
assert_return(
  () =>
    invoke($0, `v128.store32_lane_0_align_2`, [
      0,
      i32x4([0x3020100, 0x0, 0x0, 0x0]),
    ]),
  [value("i64", 50462976n)],
);

// ./test/core/simd/simd_store32_lane.wast:151
assert_return(
  () =>
    invoke($0, `v128.store32_lane_0_align_4`, [
      0,
      i32x4([0x3020100, 0x0, 0x0, 0x0]),
    ]),
  [value("i64", 50462976n)],
);

// ./test/core/simd/simd_store32_lane.wast:154
assert_return(
  () =>
    invoke($0, `v128.store32_lane_1_align_1`, [
      1,
      i32x4([0x0, 0x4030201, 0x0, 0x0]),
    ]),
  [value("i64", 67305985n)],
);

// ./test/core/simd/simd_store32_lane.wast:157
assert_return(
  () =>
    invoke($0, `v128.store32_lane_1_align_2`, [
      1,
      i32x4([0x0, 0x4030201, 0x0, 0x0]),
    ]),
  [value("i64", 67305985n)],
);

// ./test/core/simd/simd_store32_lane.wast:160
assert_return(
  () =>
    invoke($0, `v128.store32_lane_1_align_4`, [
      1,
      i32x4([0x0, 0x4030201, 0x0, 0x0]),
    ]),
  [value("i64", 67305985n)],
);

// ./test/core/simd/simd_store32_lane.wast:163
assert_return(
  () =>
    invoke($0, `v128.store32_lane_2_align_1`, [
      2,
      i32x4([0x0, 0x0, 0x5040302, 0x0]),
    ]),
  [value("i64", 84148994n)],
);

// ./test/core/simd/simd_store32_lane.wast:166
assert_return(
  () =>
    invoke($0, `v128.store32_lane_2_align_2`, [
      2,
      i32x4([0x0, 0x0, 0x5040302, 0x0]),
    ]),
  [value("i64", 84148994n)],
);

// ./test/core/simd/simd_store32_lane.wast:169
assert_return(
  () =>
    invoke($0, `v128.store32_lane_2_align_4`, [
      2,
      i32x4([0x0, 0x0, 0x5040302, 0x0]),
    ]),
  [value("i64", 84148994n)],
);

// ./test/core/simd/simd_store32_lane.wast:172
assert_return(
  () =>
    invoke($0, `v128.store32_lane_3_align_1`, [
      3,
      i32x4([0x0, 0x0, 0x0, 0x6050403]),
    ]),
  [value("i64", 100992003n)],
);

// ./test/core/simd/simd_store32_lane.wast:175
assert_return(
  () =>
    invoke($0, `v128.store32_lane_3_align_2`, [
      3,
      i32x4([0x0, 0x0, 0x0, 0x6050403]),
    ]),
  [value("i64", 100992003n)],
);

// ./test/core/simd/simd_store32_lane.wast:178
assert_return(
  () =>
    invoke($0, `v128.store32_lane_3_align_4`, [
      3,
      i32x4([0x0, 0x0, 0x0, 0x6050403]),
    ]),
  [value("i64", 100992003n)],
);

// ./test/core/simd/simd_store32_lane.wast:183
assert_invalid(
  () =>
    instantiate(`(module (memory 1)
          (func (param $$x v128) (result v128)
            (v128.store32_lane 0 (local.get $$x) (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/simd/simd_store32_lane.wast:189
assert_invalid(
  () =>
    instantiate(`(module (memory 1)
          (func (param $$x v128) (result v128)
            (v128.store32_lane 4 (i32.const 0) (local.get $$x))))`),
  `invalid lane index`,
);

// ./test/core/simd/simd_store32_lane.wast:195
assert_invalid(
  () =>
    instantiate(`(module (memory 1)
          (func (param $$x v128) (result v128)
          (v128.store32_lane align=8 0 (i32.const 0) (local.get $$x))))`),
  `alignment must not be larger than natural`,
);
