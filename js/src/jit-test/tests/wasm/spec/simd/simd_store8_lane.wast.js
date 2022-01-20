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

// ./test/core/simd/simd_store8_lane.wast

// ./test/core/simd/simd_store8_lane.wast:4
let $0 = instantiate(`(module
  (memory 1)
  (global $$zero (mut v128) (v128.const i32x4 0 0 0 0))
  (func (export "v128.store8_lane_0")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane 0 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store (local.get $$address) (global.get $$zero))    (local.get $$ret))
  (func (export "v128.store8_lane_1")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane 1 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store (local.get $$address) (global.get $$zero))    (local.get $$ret))
  (func (export "v128.store8_lane_2")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane 2 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store (local.get $$address) (global.get $$zero))    (local.get $$ret))
  (func (export "v128.store8_lane_3")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane 3 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store (local.get $$address) (global.get $$zero))    (local.get $$ret))
  (func (export "v128.store8_lane_4")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane 4 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store (local.get $$address) (global.get $$zero))    (local.get $$ret))
  (func (export "v128.store8_lane_5")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane 5 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store (local.get $$address) (global.get $$zero))    (local.get $$ret))
  (func (export "v128.store8_lane_6")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane 6 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store (local.get $$address) (global.get $$zero))    (local.get $$ret))
  (func (export "v128.store8_lane_7")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane 7 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store (local.get $$address) (global.get $$zero))    (local.get $$ret))
  (func (export "v128.store8_lane_8")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane 8 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store (local.get $$address) (global.get $$zero))    (local.get $$ret))
  (func (export "v128.store8_lane_9")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane 9 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store (local.get $$address) (global.get $$zero))    (local.get $$ret))
  (func (export "v128.store8_lane_10")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane 10 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store (local.get $$address) (global.get $$zero))    (local.get $$ret))
  (func (export "v128.store8_lane_11")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane 11 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store (local.get $$address) (global.get $$zero))    (local.get $$ret))
  (func (export "v128.store8_lane_12")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane 12 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store (local.get $$address) (global.get $$zero))    (local.get $$ret))
  (func (export "v128.store8_lane_13")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane 13 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store (local.get $$address) (global.get $$zero))    (local.get $$ret))
  (func (export "v128.store8_lane_14")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane 14 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store (local.get $$address) (global.get $$zero))    (local.get $$ret))
  (func (export "v128.store8_lane_15")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane 15 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store (local.get $$address) (global.get $$zero))    (local.get $$ret))
  (func (export "v128.store8_lane_0_offset_0")
    (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane offset=0 0 (i32.const 0) (local.get $$x))
    (local.set $$ret (i64.load offset=0 (i32.const 0)))
    (v128.store offset=0 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_1_offset_1")
    (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane offset=1 1 (i32.const 0) (local.get $$x))
    (local.set $$ret (i64.load offset=1 (i32.const 0)))
    (v128.store offset=1 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_2_offset_2")
    (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane offset=2 2 (i32.const 0) (local.get $$x))
    (local.set $$ret (i64.load offset=2 (i32.const 0)))
    (v128.store offset=2 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_3_offset_3")
    (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane offset=3 3 (i32.const 0) (local.get $$x))
    (local.set $$ret (i64.load offset=3 (i32.const 0)))
    (v128.store offset=3 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_4_offset_4")
    (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane offset=4 4 (i32.const 0) (local.get $$x))
    (local.set $$ret (i64.load offset=4 (i32.const 0)))
    (v128.store offset=4 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_5_offset_5")
    (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane offset=5 5 (i32.const 0) (local.get $$x))
    (local.set $$ret (i64.load offset=5 (i32.const 0)))
    (v128.store offset=5 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_6_offset_6")
    (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane offset=6 6 (i32.const 0) (local.get $$x))
    (local.set $$ret (i64.load offset=6 (i32.const 0)))
    (v128.store offset=6 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_7_offset_7")
    (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane offset=7 7 (i32.const 0) (local.get $$x))
    (local.set $$ret (i64.load offset=7 (i32.const 0)))
    (v128.store offset=7 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_8_offset_8")
    (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane offset=8 8 (i32.const 0) (local.get $$x))
    (local.set $$ret (i64.load offset=8 (i32.const 0)))
    (v128.store offset=8 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_9_offset_9")
    (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane offset=9 9 (i32.const 0) (local.get $$x))
    (local.set $$ret (i64.load offset=9 (i32.const 0)))
    (v128.store offset=9 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_10_offset_10")
    (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane offset=10 10 (i32.const 0) (local.get $$x))
    (local.set $$ret (i64.load offset=10 (i32.const 0)))
    (v128.store offset=10 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_11_offset_11")
    (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane offset=11 11 (i32.const 0) (local.get $$x))
    (local.set $$ret (i64.load offset=11 (i32.const 0)))
    (v128.store offset=11 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_12_offset_12")
    (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane offset=12 12 (i32.const 0) (local.get $$x))
    (local.set $$ret (i64.load offset=12 (i32.const 0)))
    (v128.store offset=12 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_13_offset_13")
    (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane offset=13 13 (i32.const 0) (local.get $$x))
    (local.set $$ret (i64.load offset=13 (i32.const 0)))
    (v128.store offset=13 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_14_offset_14")
    (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane offset=14 14 (i32.const 0) (local.get $$x))
    (local.set $$ret (i64.load offset=14 (i32.const 0)))
    (v128.store offset=14 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_15_offset_15")
    (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane offset=15 15 (i32.const 0) (local.get $$x))
    (local.set $$ret (i64.load offset=15 (i32.const 0)))
    (v128.store offset=15 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_0_align_1")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane align=1 0 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=0 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_1_align_1")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane align=1 1 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=1 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_2_align_1")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane align=1 2 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=2 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_3_align_1")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane align=1 3 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=3 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_4_align_1")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane align=1 4 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=4 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_5_align_1")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane align=1 5 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=5 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_6_align_1")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane align=1 6 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=6 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_7_align_1")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane align=1 7 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=7 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_8_align_1")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane align=1 8 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=8 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_9_align_1")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane align=1 9 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=9 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_10_align_1")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane align=1 10 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=10 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_11_align_1")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane align=1 11 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=11 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_12_align_1")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane align=1 12 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=12 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_13_align_1")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane align=1 13 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=13 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_14_align_1")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane align=1 14 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=14 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
  (func (export "v128.store8_lane_15_align_1")
    (param $$address i32) (param $$x v128) (result i64) (local $$ret i64)
    (v128.store8_lane align=1 15 (local.get $$address) (local.get $$x))
    (local.set $$ret (i64.load (local.get $$address)))
    (v128.store offset=15 (i32.const 0) (global.get $$zero))
    (local.get $$ret))
)`);

// ./test/core/simd/simd_store8_lane.wast:281
assert_return(
  () =>
    invoke($0, `v128.store8_lane_0`, [
      0,
      i8x16([
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
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i64", 0n)],
);

// ./test/core/simd/simd_store8_lane.wast:284
assert_return(
  () =>
    invoke($0, `v128.store8_lane_1`, [
      1,
      i8x16([
        0x0,
        0x1,
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
        0x0,
        0x0,
      ]),
    ]),
  [value("i64", 1n)],
);

// ./test/core/simd/simd_store8_lane.wast:287
assert_return(
  () =>
    invoke($0, `v128.store8_lane_2`, [
      2,
      i8x16([
        0x0,
        0x0,
        0x2,
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
        0x0,
      ]),
    ]),
  [value("i64", 2n)],
);

// ./test/core/simd/simd_store8_lane.wast:290
assert_return(
  () =>
    invoke($0, `v128.store8_lane_3`, [
      3,
      i8x16([
        0x0,
        0x0,
        0x0,
        0x3,
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
    ]),
  [value("i64", 3n)],
);

// ./test/core/simd/simd_store8_lane.wast:293
assert_return(
  () =>
    invoke($0, `v128.store8_lane_4`, [
      4,
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x4,
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
    ]),
  [value("i64", 4n)],
);

// ./test/core/simd/simd_store8_lane.wast:296
assert_return(
  () =>
    invoke($0, `v128.store8_lane_5`, [
      5,
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x5,
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
    ]),
  [value("i64", 5n)],
);

// ./test/core/simd/simd_store8_lane.wast:299
assert_return(
  () =>
    invoke($0, `v128.store8_lane_6`, [
      6,
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x6,
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
    ]),
  [value("i64", 6n)],
);

// ./test/core/simd/simd_store8_lane.wast:302
assert_return(
  () =>
    invoke($0, `v128.store8_lane_7`, [
      7,
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x7,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i64", 7n)],
);

// ./test/core/simd/simd_store8_lane.wast:305
assert_return(
  () =>
    invoke($0, `v128.store8_lane_8`, [
      8,
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x8,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i64", 8n)],
);

// ./test/core/simd/simd_store8_lane.wast:308
assert_return(
  () =>
    invoke($0, `v128.store8_lane_9`, [
      9,
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x9,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i64", 9n)],
);

// ./test/core/simd/simd_store8_lane.wast:311
assert_return(
  () =>
    invoke($0, `v128.store8_lane_10`, [
      10,
      i8x16([
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
        0xa,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i64", 10n)],
);

// ./test/core/simd/simd_store8_lane.wast:314
assert_return(
  () =>
    invoke($0, `v128.store8_lane_11`, [
      11,
      i8x16([
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
        0xb,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i64", 11n)],
);

// ./test/core/simd/simd_store8_lane.wast:317
assert_return(
  () =>
    invoke($0, `v128.store8_lane_12`, [
      12,
      i8x16([
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
        0xc,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i64", 12n)],
);

// ./test/core/simd/simd_store8_lane.wast:320
assert_return(
  () =>
    invoke($0, `v128.store8_lane_13`, [
      13,
      i8x16([
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
        0x0,
        0xd,
        0x0,
        0x0,
      ]),
    ]),
  [value("i64", 13n)],
);

// ./test/core/simd/simd_store8_lane.wast:323
assert_return(
  () =>
    invoke($0, `v128.store8_lane_14`, [
      14,
      i8x16([
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
        0x0,
        0x0,
        0xe,
        0x0,
      ]),
    ]),
  [value("i64", 14n)],
);

// ./test/core/simd/simd_store8_lane.wast:326
assert_return(
  () =>
    invoke($0, `v128.store8_lane_15`, [
      15,
      i8x16([
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
        0x0,
        0x0,
        0x0,
        0xf,
      ]),
    ]),
  [value("i64", 15n)],
);

// ./test/core/simd/simd_store8_lane.wast:329
assert_return(
  () =>
    invoke($0, `v128.store8_lane_0_offset_0`, [
      i8x16([
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
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i64", 0n)],
);

// ./test/core/simd/simd_store8_lane.wast:331
assert_return(
  () =>
    invoke($0, `v128.store8_lane_1_offset_1`, [
      i8x16([
        0x0,
        0x1,
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
        0x0,
        0x0,
      ]),
    ]),
  [value("i64", 1n)],
);

// ./test/core/simd/simd_store8_lane.wast:333
assert_return(
  () =>
    invoke($0, `v128.store8_lane_2_offset_2`, [
      i8x16([
        0x0,
        0x0,
        0x2,
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
        0x0,
      ]),
    ]),
  [value("i64", 2n)],
);

// ./test/core/simd/simd_store8_lane.wast:335
assert_return(
  () =>
    invoke($0, `v128.store8_lane_3_offset_3`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x3,
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
    ]),
  [value("i64", 3n)],
);

// ./test/core/simd/simd_store8_lane.wast:337
assert_return(
  () =>
    invoke($0, `v128.store8_lane_4_offset_4`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x4,
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
    ]),
  [value("i64", 4n)],
);

// ./test/core/simd/simd_store8_lane.wast:339
assert_return(
  () =>
    invoke($0, `v128.store8_lane_5_offset_5`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x5,
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
    ]),
  [value("i64", 5n)],
);

// ./test/core/simd/simd_store8_lane.wast:341
assert_return(
  () =>
    invoke($0, `v128.store8_lane_6_offset_6`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x6,
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
    ]),
  [value("i64", 6n)],
);

// ./test/core/simd/simd_store8_lane.wast:343
assert_return(
  () =>
    invoke($0, `v128.store8_lane_7_offset_7`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x7,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i64", 7n)],
);

// ./test/core/simd/simd_store8_lane.wast:345
assert_return(
  () =>
    invoke($0, `v128.store8_lane_8_offset_8`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x8,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i64", 8n)],
);

// ./test/core/simd/simd_store8_lane.wast:347
assert_return(
  () =>
    invoke($0, `v128.store8_lane_9_offset_9`, [
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x9,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i64", 9n)],
);

// ./test/core/simd/simd_store8_lane.wast:349
assert_return(
  () =>
    invoke($0, `v128.store8_lane_10_offset_10`, [
      i8x16([
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
        0xa,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i64", 10n)],
);

// ./test/core/simd/simd_store8_lane.wast:351
assert_return(
  () =>
    invoke($0, `v128.store8_lane_11_offset_11`, [
      i8x16([
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
        0xb,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i64", 11n)],
);

// ./test/core/simd/simd_store8_lane.wast:353
assert_return(
  () =>
    invoke($0, `v128.store8_lane_12_offset_12`, [
      i8x16([
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
        0xc,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i64", 12n)],
);

// ./test/core/simd/simd_store8_lane.wast:355
assert_return(
  () =>
    invoke($0, `v128.store8_lane_13_offset_13`, [
      i8x16([
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
        0x0,
        0xd,
        0x0,
        0x0,
      ]),
    ]),
  [value("i64", 13n)],
);

// ./test/core/simd/simd_store8_lane.wast:357
assert_return(
  () =>
    invoke($0, `v128.store8_lane_14_offset_14`, [
      i8x16([
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
        0x0,
        0x0,
        0xe,
        0x0,
      ]),
    ]),
  [value("i64", 14n)],
);

// ./test/core/simd/simd_store8_lane.wast:359
assert_return(
  () =>
    invoke($0, `v128.store8_lane_15_offset_15`, [
      i8x16([
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
        0x0,
        0x0,
        0x0,
        0xf,
      ]),
    ]),
  [value("i64", 15n)],
);

// ./test/core/simd/simd_store8_lane.wast:361
assert_return(
  () =>
    invoke($0, `v128.store8_lane_0_align_1`, [
      0,
      i8x16([
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
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i64", 0n)],
);

// ./test/core/simd/simd_store8_lane.wast:364
assert_return(
  () =>
    invoke($0, `v128.store8_lane_1_align_1`, [
      1,
      i8x16([
        0x0,
        0x1,
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
        0x0,
        0x0,
      ]),
    ]),
  [value("i64", 1n)],
);

// ./test/core/simd/simd_store8_lane.wast:367
assert_return(
  () =>
    invoke($0, `v128.store8_lane_2_align_1`, [
      2,
      i8x16([
        0x0,
        0x0,
        0x2,
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
        0x0,
      ]),
    ]),
  [value("i64", 2n)],
);

// ./test/core/simd/simd_store8_lane.wast:370
assert_return(
  () =>
    invoke($0, `v128.store8_lane_3_align_1`, [
      3,
      i8x16([
        0x0,
        0x0,
        0x0,
        0x3,
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
    ]),
  [value("i64", 3n)],
);

// ./test/core/simd/simd_store8_lane.wast:373
assert_return(
  () =>
    invoke($0, `v128.store8_lane_4_align_1`, [
      4,
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x4,
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
    ]),
  [value("i64", 4n)],
);

// ./test/core/simd/simd_store8_lane.wast:376
assert_return(
  () =>
    invoke($0, `v128.store8_lane_5_align_1`, [
      5,
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x5,
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
    ]),
  [value("i64", 5n)],
);

// ./test/core/simd/simd_store8_lane.wast:379
assert_return(
  () =>
    invoke($0, `v128.store8_lane_6_align_1`, [
      6,
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x6,
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
    ]),
  [value("i64", 6n)],
);

// ./test/core/simd/simd_store8_lane.wast:382
assert_return(
  () =>
    invoke($0, `v128.store8_lane_7_align_1`, [
      7,
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x7,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i64", 7n)],
);

// ./test/core/simd/simd_store8_lane.wast:385
assert_return(
  () =>
    invoke($0, `v128.store8_lane_8_align_1`, [
      8,
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x8,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i64", 8n)],
);

// ./test/core/simd/simd_store8_lane.wast:388
assert_return(
  () =>
    invoke($0, `v128.store8_lane_9_align_1`, [
      9,
      i8x16([
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x9,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i64", 9n)],
);

// ./test/core/simd/simd_store8_lane.wast:391
assert_return(
  () =>
    invoke($0, `v128.store8_lane_10_align_1`, [
      10,
      i8x16([
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
        0xa,
        0x0,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i64", 10n)],
);

// ./test/core/simd/simd_store8_lane.wast:394
assert_return(
  () =>
    invoke($0, `v128.store8_lane_11_align_1`, [
      11,
      i8x16([
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
        0xb,
        0x0,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i64", 11n)],
);

// ./test/core/simd/simd_store8_lane.wast:397
assert_return(
  () =>
    invoke($0, `v128.store8_lane_12_align_1`, [
      12,
      i8x16([
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
        0xc,
        0x0,
        0x0,
        0x0,
      ]),
    ]),
  [value("i64", 12n)],
);

// ./test/core/simd/simd_store8_lane.wast:400
assert_return(
  () =>
    invoke($0, `v128.store8_lane_13_align_1`, [
      13,
      i8x16([
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
        0x0,
        0xd,
        0x0,
        0x0,
      ]),
    ]),
  [value("i64", 13n)],
);

// ./test/core/simd/simd_store8_lane.wast:403
assert_return(
  () =>
    invoke($0, `v128.store8_lane_14_align_1`, [
      14,
      i8x16([
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
        0x0,
        0x0,
        0xe,
        0x0,
      ]),
    ]),
  [value("i64", 14n)],
);

// ./test/core/simd/simd_store8_lane.wast:406
assert_return(
  () =>
    invoke($0, `v128.store8_lane_15_align_1`, [
      15,
      i8x16([
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
        0x0,
        0x0,
        0x0,
        0xf,
      ]),
    ]),
  [value("i64", 15n)],
);

// ./test/core/simd/simd_store8_lane.wast:411
assert_invalid(
  () =>
    instantiate(`(module (memory 1)
          (func (param $$x v128) (result v128)
            (v128.store8_lane 0 (local.get $$x) (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/simd/simd_store8_lane.wast:417
assert_invalid(
  () =>
    instantiate(`(module (memory 1)
          (func (param $$x v128) (result v128)
            (v128.store8_lane 16 (i32.const 0) (local.get $$x))))`),
  `invalid lane index`,
);

// ./test/core/simd/simd_store8_lane.wast:423
assert_invalid(
  () =>
    instantiate(`(module (memory 1)
          (func (param $$x v128) (result v128)
          (v128.store8_lane align=2 0 (i32.const 0) (local.get $$x))))`),
  `alignment must not be larger than natural`,
);
