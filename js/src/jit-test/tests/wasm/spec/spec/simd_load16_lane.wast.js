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

// ./test/core/simd/simd_load16_lane.wast

// ./test/core/simd/simd_load16_lane.wast:4
let $0 = instantiate(`(module
  (memory 1)
  (data (i32.const 0) "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0A\\0B\\0C\\0D\\0E\\0F")
  (func (export "v128.load16_lane_0")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load16_lane 0 (local.get $$address) (local.get $$x)))
  (func (export "v128.load16_lane_1")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load16_lane 1 (local.get $$address) (local.get $$x)))
  (func (export "v128.load16_lane_2")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load16_lane 2 (local.get $$address) (local.get $$x)))
  (func (export "v128.load16_lane_3")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load16_lane 3 (local.get $$address) (local.get $$x)))
  (func (export "v128.load16_lane_4")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load16_lane 4 (local.get $$address) (local.get $$x)))
  (func (export "v128.load16_lane_5")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load16_lane 5 (local.get $$address) (local.get $$x)))
  (func (export "v128.load16_lane_6")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load16_lane 6 (local.get $$address) (local.get $$x)))
  (func (export "v128.load16_lane_7")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load16_lane 7 (local.get $$address) (local.get $$x)))
  (func (export "v128.load16_lane_0_offset_0")
    (param $$x v128) (result v128)
    (v128.load16_lane offset=0 0 (i32.const 0) (local.get $$x)))
  (func (export "v128.load16_lane_1_offset_1")
    (param $$x v128) (result v128)
    (v128.load16_lane offset=1 1 (i32.const 0) (local.get $$x)))
  (func (export "v128.load16_lane_2_offset_2")
    (param $$x v128) (result v128)
    (v128.load16_lane offset=2 2 (i32.const 0) (local.get $$x)))
  (func (export "v128.load16_lane_3_offset_3")
    (param $$x v128) (result v128)
    (v128.load16_lane offset=3 3 (i32.const 0) (local.get $$x)))
  (func (export "v128.load16_lane_4_offset_4")
    (param $$x v128) (result v128)
    (v128.load16_lane offset=4 4 (i32.const 0) (local.get $$x)))
  (func (export "v128.load16_lane_5_offset_5")
    (param $$x v128) (result v128)
    (v128.load16_lane offset=5 5 (i32.const 0) (local.get $$x)))
  (func (export "v128.load16_lane_6_offset_6")
    (param $$x v128) (result v128)
    (v128.load16_lane offset=6 6 (i32.const 0) (local.get $$x)))
  (func (export "v128.load16_lane_7_offset_7")
    (param $$x v128) (result v128)
    (v128.load16_lane offset=7 7 (i32.const 0) (local.get $$x)))
  (func (export "v128.load16_lane_0_align_1")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load16_lane align=1 0 (local.get $$address) (local.get $$x)))
  (func (export "v128.load16_lane_0_align_2")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load16_lane align=2 0 (local.get $$address) (local.get $$x)))
  (func (export "v128.load16_lane_1_align_1")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load16_lane align=1 1 (local.get $$address) (local.get $$x)))
  (func (export "v128.load16_lane_1_align_2")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load16_lane align=2 1 (local.get $$address) (local.get $$x)))
  (func (export "v128.load16_lane_2_align_1")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load16_lane align=1 2 (local.get $$address) (local.get $$x)))
  (func (export "v128.load16_lane_2_align_2")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load16_lane align=2 2 (local.get $$address) (local.get $$x)))
  (func (export "v128.load16_lane_3_align_1")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load16_lane align=1 3 (local.get $$address) (local.get $$x)))
  (func (export "v128.load16_lane_3_align_2")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load16_lane align=2 3 (local.get $$address) (local.get $$x)))
  (func (export "v128.load16_lane_4_align_1")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load16_lane align=1 4 (local.get $$address) (local.get $$x)))
  (func (export "v128.load16_lane_4_align_2")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load16_lane align=2 4 (local.get $$address) (local.get $$x)))
  (func (export "v128.load16_lane_5_align_1")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load16_lane align=1 5 (local.get $$address) (local.get $$x)))
  (func (export "v128.load16_lane_5_align_2")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load16_lane align=2 5 (local.get $$address) (local.get $$x)))
  (func (export "v128.load16_lane_6_align_1")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load16_lane align=1 6 (local.get $$address) (local.get $$x)))
  (func (export "v128.load16_lane_6_align_2")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load16_lane align=2 6 (local.get $$address) (local.get $$x)))
  (func (export "v128.load16_lane_7_align_1")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load16_lane align=1 7 (local.get $$address) (local.get $$x)))
  (func (export "v128.load16_lane_7_align_2")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load16_lane align=2 7 (local.get $$address) (local.get $$x)))
)`);

// ./test/core/simd/simd_load16_lane.wast:105
assert_return(
  () =>
    invoke($0, `v128.load16_lane_0`, [
      0,
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x100, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_load16_lane.wast:108
assert_return(
  () =>
    invoke($0, `v128.load16_lane_1`, [
      1,
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x0, 0x201, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_load16_lane.wast:111
assert_return(
  () =>
    invoke($0, `v128.load16_lane_2`, [
      2,
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x0, 0x0, 0x302, 0x0, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_load16_lane.wast:114
assert_return(
  () =>
    invoke($0, `v128.load16_lane_3`, [
      3,
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x0, 0x0, 0x0, 0x403, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_load16_lane.wast:117
assert_return(
  () =>
    invoke($0, `v128.load16_lane_4`, [
      4,
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x504, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_load16_lane.wast:120
assert_return(
  () =>
    invoke($0, `v128.load16_lane_5`, [
      5,
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x605, 0x0, 0x0])],
);

// ./test/core/simd/simd_load16_lane.wast:123
assert_return(
  () =>
    invoke($0, `v128.load16_lane_6`, [
      6,
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x706, 0x0])],
);

// ./test/core/simd/simd_load16_lane.wast:126
assert_return(
  () =>
    invoke($0, `v128.load16_lane_7`, [
      7,
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x807])],
);

// ./test/core/simd/simd_load16_lane.wast:129
assert_return(
  () =>
    invoke($0, `v128.load16_lane_0_offset_0`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x100, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_load16_lane.wast:131
assert_return(
  () =>
    invoke($0, `v128.load16_lane_1_offset_1`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x0, 0x201, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_load16_lane.wast:133
assert_return(
  () =>
    invoke($0, `v128.load16_lane_2_offset_2`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x0, 0x0, 0x302, 0x0, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_load16_lane.wast:135
assert_return(
  () =>
    invoke($0, `v128.load16_lane_3_offset_3`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x0, 0x0, 0x0, 0x403, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_load16_lane.wast:137
assert_return(
  () =>
    invoke($0, `v128.load16_lane_4_offset_4`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x504, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_load16_lane.wast:139
assert_return(
  () =>
    invoke($0, `v128.load16_lane_5_offset_5`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x605, 0x0, 0x0])],
);

// ./test/core/simd/simd_load16_lane.wast:141
assert_return(
  () =>
    invoke($0, `v128.load16_lane_6_offset_6`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x706, 0x0])],
);

// ./test/core/simd/simd_load16_lane.wast:143
assert_return(
  () =>
    invoke($0, `v128.load16_lane_7_offset_7`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x807])],
);

// ./test/core/simd/simd_load16_lane.wast:145
assert_return(
  () =>
    invoke($0, `v128.load16_lane_0_align_1`, [
      0,
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x100, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_load16_lane.wast:148
assert_return(
  () =>
    invoke($0, `v128.load16_lane_0_align_2`, [
      0,
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x100, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_load16_lane.wast:151
assert_return(
  () =>
    invoke($0, `v128.load16_lane_1_align_1`, [
      1,
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x0, 0x201, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_load16_lane.wast:154
assert_return(
  () =>
    invoke($0, `v128.load16_lane_1_align_2`, [
      1,
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x0, 0x201, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_load16_lane.wast:157
assert_return(
  () =>
    invoke($0, `v128.load16_lane_2_align_1`, [
      2,
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x0, 0x0, 0x302, 0x0, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_load16_lane.wast:160
assert_return(
  () =>
    invoke($0, `v128.load16_lane_2_align_2`, [
      2,
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x0, 0x0, 0x302, 0x0, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_load16_lane.wast:163
assert_return(
  () =>
    invoke($0, `v128.load16_lane_3_align_1`, [
      3,
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x0, 0x0, 0x0, 0x403, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_load16_lane.wast:166
assert_return(
  () =>
    invoke($0, `v128.load16_lane_3_align_2`, [
      3,
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x0, 0x0, 0x0, 0x403, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_load16_lane.wast:169
assert_return(
  () =>
    invoke($0, `v128.load16_lane_4_align_1`, [
      4,
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x504, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_load16_lane.wast:172
assert_return(
  () =>
    invoke($0, `v128.load16_lane_4_align_2`, [
      4,
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x504, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_load16_lane.wast:175
assert_return(
  () =>
    invoke($0, `v128.load16_lane_5_align_1`, [
      5,
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x605, 0x0, 0x0])],
);

// ./test/core/simd/simd_load16_lane.wast:178
assert_return(
  () =>
    invoke($0, `v128.load16_lane_5_align_2`, [
      5,
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x605, 0x0, 0x0])],
);

// ./test/core/simd/simd_load16_lane.wast:181
assert_return(
  () =>
    invoke($0, `v128.load16_lane_6_align_1`, [
      6,
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x706, 0x0])],
);

// ./test/core/simd/simd_load16_lane.wast:184
assert_return(
  () =>
    invoke($0, `v128.load16_lane_6_align_2`, [
      6,
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x706, 0x0])],
);

// ./test/core/simd/simd_load16_lane.wast:187
assert_return(
  () =>
    invoke($0, `v128.load16_lane_7_align_1`, [
      7,
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x807])],
);

// ./test/core/simd/simd_load16_lane.wast:190
assert_return(
  () =>
    invoke($0, `v128.load16_lane_7_align_2`, [
      7,
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x807])],
);

// ./test/core/simd/simd_load16_lane.wast:195
assert_invalid(
  () =>
    instantiate(`(module (memory 1)
          (func (param $$x v128) (result v128)
            (v128.load16_lane 0 (local.get $$x) (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/simd/simd_load16_lane.wast:201
assert_invalid(
  () =>
    instantiate(`(module (memory 1)
          (func (param $$x v128) (result v128)
            (v128.load16_lane 8 (i32.const 0) (local.get $$x))))`),
  `invalid lane index`,
);

// ./test/core/simd/simd_load16_lane.wast:207
assert_invalid(
  () =>
    instantiate(`(module (memory 1)
          (func (param $$x v128) (result v128)
          (v128.load16_lane align=4 0 (i32.const 0) (local.get $$x))))`),
  `alignment must not be larger than natural`,
);
