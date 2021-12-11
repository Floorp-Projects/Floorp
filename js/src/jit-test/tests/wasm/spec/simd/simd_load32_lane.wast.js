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

// ./test/core/simd/simd_load32_lane.wast

// ./test/core/simd/simd_load32_lane.wast:4
let $0 = instantiate(`(module
  (memory 1)
  (data (i32.const 0) "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0A\\0B\\0C\\0D\\0E\\0F")
  (func (export "v128.load32_lane_0")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load32_lane 0 (local.get $$address) (local.get $$x)))
  (func (export "v128.load32_lane_1")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load32_lane 1 (local.get $$address) (local.get $$x)))
  (func (export "v128.load32_lane_2")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load32_lane 2 (local.get $$address) (local.get $$x)))
  (func (export "v128.load32_lane_3")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load32_lane 3 (local.get $$address) (local.get $$x)))
  (func (export "v128.load32_lane_0_offset_0")
    (param $$x v128) (result v128)
    (v128.load32_lane offset=0 0 (i32.const 0) (local.get $$x)))
  (func (export "v128.load32_lane_1_offset_1")
    (param $$x v128) (result v128)
    (v128.load32_lane offset=1 1 (i32.const 0) (local.get $$x)))
  (func (export "v128.load32_lane_2_offset_2")
    (param $$x v128) (result v128)
    (v128.load32_lane offset=2 2 (i32.const 0) (local.get $$x)))
  (func (export "v128.load32_lane_3_offset_3")
    (param $$x v128) (result v128)
    (v128.load32_lane offset=3 3 (i32.const 0) (local.get $$x)))
  (func (export "v128.load32_lane_0_align_1")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load32_lane align=1 0 (local.get $$address) (local.get $$x)))
  (func (export "v128.load32_lane_0_align_2")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load32_lane align=2 0 (local.get $$address) (local.get $$x)))
  (func (export "v128.load32_lane_0_align_4")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load32_lane align=4 0 (local.get $$address) (local.get $$x)))
  (func (export "v128.load32_lane_1_align_1")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load32_lane align=1 1 (local.get $$address) (local.get $$x)))
  (func (export "v128.load32_lane_1_align_2")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load32_lane align=2 1 (local.get $$address) (local.get $$x)))
  (func (export "v128.load32_lane_1_align_4")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load32_lane align=4 1 (local.get $$address) (local.get $$x)))
  (func (export "v128.load32_lane_2_align_1")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load32_lane align=1 2 (local.get $$address) (local.get $$x)))
  (func (export "v128.load32_lane_2_align_2")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load32_lane align=2 2 (local.get $$address) (local.get $$x)))
  (func (export "v128.load32_lane_2_align_4")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load32_lane align=4 2 (local.get $$address) (local.get $$x)))
  (func (export "v128.load32_lane_3_align_1")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load32_lane align=1 3 (local.get $$address) (local.get $$x)))
  (func (export "v128.load32_lane_3_align_2")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load32_lane align=2 3 (local.get $$address) (local.get $$x)))
  (func (export "v128.load32_lane_3_align_4")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load32_lane align=4 3 (local.get $$address) (local.get $$x)))
)`);

// ./test/core/simd/simd_load32_lane.wast:69
assert_return(
  () => invoke($0, `v128.load32_lane_0`, [0, i32x4([0x0, 0x0, 0x0, 0x0])]),
  [i32x4([0x3020100, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_load32_lane.wast:72
assert_return(
  () => invoke($0, `v128.load32_lane_1`, [1, i32x4([0x0, 0x0, 0x0, 0x0])]),
  [i32x4([0x0, 0x4030201, 0x0, 0x0])],
);

// ./test/core/simd/simd_load32_lane.wast:75
assert_return(
  () => invoke($0, `v128.load32_lane_2`, [2, i32x4([0x0, 0x0, 0x0, 0x0])]),
  [i32x4([0x0, 0x0, 0x5040302, 0x0])],
);

// ./test/core/simd/simd_load32_lane.wast:78
assert_return(
  () => invoke($0, `v128.load32_lane_3`, [3, i32x4([0x0, 0x0, 0x0, 0x0])]),
  [i32x4([0x0, 0x0, 0x0, 0x6050403])],
);

// ./test/core/simd/simd_load32_lane.wast:81
assert_return(
  () =>
    invoke($0, `v128.load32_lane_0_offset_0`, [i32x4([0x0, 0x0, 0x0, 0x0])]),
  [i32x4([0x3020100, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_load32_lane.wast:83
assert_return(
  () =>
    invoke($0, `v128.load32_lane_1_offset_1`, [i32x4([0x0, 0x0, 0x0, 0x0])]),
  [i32x4([0x0, 0x4030201, 0x0, 0x0])],
);

// ./test/core/simd/simd_load32_lane.wast:85
assert_return(
  () =>
    invoke($0, `v128.load32_lane_2_offset_2`, [i32x4([0x0, 0x0, 0x0, 0x0])]),
  [i32x4([0x0, 0x0, 0x5040302, 0x0])],
);

// ./test/core/simd/simd_load32_lane.wast:87
assert_return(
  () =>
    invoke($0, `v128.load32_lane_3_offset_3`, [i32x4([0x0, 0x0, 0x0, 0x0])]),
  [i32x4([0x0, 0x0, 0x0, 0x6050403])],
);

// ./test/core/simd/simd_load32_lane.wast:89
assert_return(
  () =>
    invoke($0, `v128.load32_lane_0_align_1`, [0, i32x4([0x0, 0x0, 0x0, 0x0])]),
  [i32x4([0x3020100, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_load32_lane.wast:92
assert_return(
  () =>
    invoke($0, `v128.load32_lane_0_align_2`, [0, i32x4([0x0, 0x0, 0x0, 0x0])]),
  [i32x4([0x3020100, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_load32_lane.wast:95
assert_return(
  () =>
    invoke($0, `v128.load32_lane_0_align_4`, [0, i32x4([0x0, 0x0, 0x0, 0x0])]),
  [i32x4([0x3020100, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_load32_lane.wast:98
assert_return(
  () =>
    invoke($0, `v128.load32_lane_1_align_1`, [1, i32x4([0x0, 0x0, 0x0, 0x0])]),
  [i32x4([0x0, 0x4030201, 0x0, 0x0])],
);

// ./test/core/simd/simd_load32_lane.wast:101
assert_return(
  () =>
    invoke($0, `v128.load32_lane_1_align_2`, [1, i32x4([0x0, 0x0, 0x0, 0x0])]),
  [i32x4([0x0, 0x4030201, 0x0, 0x0])],
);

// ./test/core/simd/simd_load32_lane.wast:104
assert_return(
  () =>
    invoke($0, `v128.load32_lane_1_align_4`, [1, i32x4([0x0, 0x0, 0x0, 0x0])]),
  [i32x4([0x0, 0x4030201, 0x0, 0x0])],
);

// ./test/core/simd/simd_load32_lane.wast:107
assert_return(
  () =>
    invoke($0, `v128.load32_lane_2_align_1`, [2, i32x4([0x0, 0x0, 0x0, 0x0])]),
  [i32x4([0x0, 0x0, 0x5040302, 0x0])],
);

// ./test/core/simd/simd_load32_lane.wast:110
assert_return(
  () =>
    invoke($0, `v128.load32_lane_2_align_2`, [2, i32x4([0x0, 0x0, 0x0, 0x0])]),
  [i32x4([0x0, 0x0, 0x5040302, 0x0])],
);

// ./test/core/simd/simd_load32_lane.wast:113
assert_return(
  () =>
    invoke($0, `v128.load32_lane_2_align_4`, [2, i32x4([0x0, 0x0, 0x0, 0x0])]),
  [i32x4([0x0, 0x0, 0x5040302, 0x0])],
);

// ./test/core/simd/simd_load32_lane.wast:116
assert_return(
  () =>
    invoke($0, `v128.load32_lane_3_align_1`, [3, i32x4([0x0, 0x0, 0x0, 0x0])]),
  [i32x4([0x0, 0x0, 0x0, 0x6050403])],
);

// ./test/core/simd/simd_load32_lane.wast:119
assert_return(
  () =>
    invoke($0, `v128.load32_lane_3_align_2`, [3, i32x4([0x0, 0x0, 0x0, 0x0])]),
  [i32x4([0x0, 0x0, 0x0, 0x6050403])],
);

// ./test/core/simd/simd_load32_lane.wast:122
assert_return(
  () =>
    invoke($0, `v128.load32_lane_3_align_4`, [3, i32x4([0x0, 0x0, 0x0, 0x0])]),
  [i32x4([0x0, 0x0, 0x0, 0x6050403])],
);

// ./test/core/simd/simd_load32_lane.wast:127
assert_invalid(
  () =>
    instantiate(`(module (memory 1)
          (func (param $$x v128) (result v128)
            (v128.load32_lane 0 (local.get $$x) (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/simd/simd_load32_lane.wast:133
assert_invalid(
  () =>
    instantiate(`(module (memory 1)
          (func (param $$x v128) (result v128)
            (v128.load32_lane 4 (i32.const 0) (local.get $$x))))`),
  `invalid lane index`,
);

// ./test/core/simd/simd_load32_lane.wast:139
assert_invalid(
  () =>
    instantiate(`(module (memory 1)
          (func (param $$x v128) (result v128)
          (v128.load32_lane align=8 0 (i32.const 0) (local.get $$x))))`),
  `alignment must not be larger than natural`,
);
