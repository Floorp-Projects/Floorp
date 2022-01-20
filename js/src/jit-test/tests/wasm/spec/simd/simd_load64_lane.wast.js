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

// ./test/core/simd/simd_load64_lane.wast

// ./test/core/simd/simd_load64_lane.wast:4
let $0 = instantiate(`(module
  (memory 1)
  (data (i32.const 0) "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0A\\0B\\0C\\0D\\0E\\0F")
  (func (export "v128.load64_lane_0")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load64_lane 0 (local.get $$address) (local.get $$x)))
  (func (export "v128.load64_lane_1")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load64_lane 1 (local.get $$address) (local.get $$x)))
  (func (export "v128.load64_lane_0_offset_0")
    (param $$x v128) (result v128)
    (v128.load64_lane offset=0 0 (i32.const 0) (local.get $$x)))
  (func (export "v128.load64_lane_1_offset_1")
    (param $$x v128) (result v128)
    (v128.load64_lane offset=1 1 (i32.const 0) (local.get $$x)))
  (func (export "v128.load64_lane_0_align_1")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load64_lane align=1 0 (local.get $$address) (local.get $$x)))
  (func (export "v128.load64_lane_0_align_2")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load64_lane align=2 0 (local.get $$address) (local.get $$x)))
  (func (export "v128.load64_lane_0_align_4")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load64_lane align=4 0 (local.get $$address) (local.get $$x)))
  (func (export "v128.load64_lane_0_align_8")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load64_lane align=8 0 (local.get $$address) (local.get $$x)))
  (func (export "v128.load64_lane_1_align_1")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load64_lane align=1 1 (local.get $$address) (local.get $$x)))
  (func (export "v128.load64_lane_1_align_2")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load64_lane align=2 1 (local.get $$address) (local.get $$x)))
  (func (export "v128.load64_lane_1_align_4")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load64_lane align=4 1 (local.get $$address) (local.get $$x)))
  (func (export "v128.load64_lane_1_align_8")
    (param $$address i32) (param $$x v128) (result v128)
    (v128.load64_lane align=8 1 (local.get $$address) (local.get $$x)))
)`);

// ./test/core/simd/simd_load64_lane.wast:45
assert_return(
  () => invoke($0, `v128.load64_lane_0`, [0, i64x2([0x0n, 0x0n])]),
  [i64x2([0x706050403020100n, 0x0n])],
);

// ./test/core/simd/simd_load64_lane.wast:48
assert_return(
  () => invoke($0, `v128.load64_lane_1`, [1, i64x2([0x0n, 0x0n])]),
  [i64x2([0x0n, 0x807060504030201n])],
);

// ./test/core/simd/simd_load64_lane.wast:51
assert_return(
  () => invoke($0, `v128.load64_lane_0_offset_0`, [i64x2([0x0n, 0x0n])]),
  [i64x2([0x706050403020100n, 0x0n])],
);

// ./test/core/simd/simd_load64_lane.wast:53
assert_return(
  () => invoke($0, `v128.load64_lane_1_offset_1`, [i64x2([0x0n, 0x0n])]),
  [i64x2([0x0n, 0x807060504030201n])],
);

// ./test/core/simd/simd_load64_lane.wast:55
assert_return(
  () => invoke($0, `v128.load64_lane_0_align_1`, [0, i64x2([0x0n, 0x0n])]),
  [i64x2([0x706050403020100n, 0x0n])],
);

// ./test/core/simd/simd_load64_lane.wast:58
assert_return(
  () => invoke($0, `v128.load64_lane_0_align_2`, [0, i64x2([0x0n, 0x0n])]),
  [i64x2([0x706050403020100n, 0x0n])],
);

// ./test/core/simd/simd_load64_lane.wast:61
assert_return(
  () => invoke($0, `v128.load64_lane_0_align_4`, [0, i64x2([0x0n, 0x0n])]),
  [i64x2([0x706050403020100n, 0x0n])],
);

// ./test/core/simd/simd_load64_lane.wast:64
assert_return(
  () => invoke($0, `v128.load64_lane_0_align_8`, [0, i64x2([0x0n, 0x0n])]),
  [i64x2([0x706050403020100n, 0x0n])],
);

// ./test/core/simd/simd_load64_lane.wast:67
assert_return(
  () => invoke($0, `v128.load64_lane_1_align_1`, [1, i64x2([0x0n, 0x0n])]),
  [i64x2([0x0n, 0x807060504030201n])],
);

// ./test/core/simd/simd_load64_lane.wast:70
assert_return(
  () => invoke($0, `v128.load64_lane_1_align_2`, [1, i64x2([0x0n, 0x0n])]),
  [i64x2([0x0n, 0x807060504030201n])],
);

// ./test/core/simd/simd_load64_lane.wast:73
assert_return(
  () => invoke($0, `v128.load64_lane_1_align_4`, [1, i64x2([0x0n, 0x0n])]),
  [i64x2([0x0n, 0x807060504030201n])],
);

// ./test/core/simd/simd_load64_lane.wast:76
assert_return(
  () => invoke($0, `v128.load64_lane_1_align_8`, [1, i64x2([0x0n, 0x0n])]),
  [i64x2([0x0n, 0x807060504030201n])],
);

// ./test/core/simd/simd_load64_lane.wast:81
assert_invalid(
  () =>
    instantiate(`(module (memory 1)
          (func (param $$x v128) (result v128)
            (v128.load64_lane 0 (local.get $$x) (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/simd/simd_load64_lane.wast:87
assert_invalid(
  () =>
    instantiate(`(module (memory 1)
          (func (param $$x v128) (result v128)
            (v128.load64_lane 2 (i32.const 0) (local.get $$x))))`),
  `invalid lane index`,
);

// ./test/core/simd/simd_load64_lane.wast:93
assert_invalid(
  () =>
    instantiate(`(module (memory 1)
          (func (param $$x v128) (result v128)
          (v128.load64_lane align=16 0 (i32.const 0) (local.get $$x))))`),
  `alignment must not be larger than natural`,
);
