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

// ./test/core/simd/simd_i32x4_extadd_pairwise_i16x8.wast

// ./test/core/simd/simd_i32x4_extadd_pairwise_i16x8.wast:4
let $0 = instantiate(`(module
  (func (export "i32x4.extadd_pairwise_i16x8_s") (param v128) (result v128) (i32x4.extadd_pairwise_i16x8_s (local.get 0)))
  (func (export "i32x4.extadd_pairwise_i16x8_u") (param v128) (result v128) (i32x4.extadd_pairwise_i16x8_u (local.get 0)))
)`);

// ./test/core/simd/simd_i32x4_extadd_pairwise_i16x8.wast:11
assert_return(
  () =>
    invoke($0, `i32x4.extadd_pairwise_i16x8_s`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_extadd_pairwise_i16x8.wast:13
assert_return(
  () =>
    invoke($0, `i32x4.extadd_pairwise_i16x8_s`, [
      i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x2, 0x2, 0x2, 0x2])],
);

// ./test/core/simd/simd_i32x4_extadd_pairwise_i16x8.wast:15
assert_return(
  () =>
    invoke($0, `i32x4.extadd_pairwise_i16x8_s`, [
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0xfffffffe, 0xfffffffe, 0xfffffffe, 0xfffffffe])],
);

// ./test/core/simd/simd_i32x4_extadd_pairwise_i16x8.wast:17
assert_return(
  () =>
    invoke($0, `i32x4.extadd_pairwise_i16x8_s`, [
      i16x8([0x7ffe, 0x7ffe, 0x7ffe, 0x7ffe, 0x7ffe, 0x7ffe, 0x7ffe, 0x7ffe]),
    ]),
  [i32x4([0xfffc, 0xfffc, 0xfffc, 0xfffc])],
);

// ./test/core/simd/simd_i32x4_extadd_pairwise_i16x8.wast:19
assert_return(
  () =>
    invoke($0, `i32x4.extadd_pairwise_i16x8_s`, [
      i16x8([0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001]),
    ]),
  [i32x4([0xffff0002, 0xffff0002, 0xffff0002, 0xffff0002])],
);

// ./test/core/simd/simd_i32x4_extadd_pairwise_i16x8.wast:21
assert_return(
  () =>
    invoke($0, `i32x4.extadd_pairwise_i16x8_s`, [
      i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000]),
    ]),
  [i32x4([0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000])],
);

// ./test/core/simd/simd_i32x4_extadd_pairwise_i16x8.wast:23
assert_return(
  () =>
    invoke($0, `i32x4.extadd_pairwise_i16x8_s`, [
      i16x8([0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff]),
    ]),
  [i32x4([0xfffe, 0xfffe, 0xfffe, 0xfffe])],
);

// ./test/core/simd/simd_i32x4_extadd_pairwise_i16x8.wast:25
assert_return(
  () =>
    invoke($0, `i32x4.extadd_pairwise_i16x8_s`, [
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0xfffffffe, 0xfffffffe, 0xfffffffe, 0xfffffffe])],
);

// ./test/core/simd/simd_i32x4_extadd_pairwise_i16x8.wast:29
assert_return(
  () =>
    invoke($0, `i32x4.extadd_pairwise_i16x8_u`, [
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_extadd_pairwise_i16x8.wast:31
assert_return(
  () =>
    invoke($0, `i32x4.extadd_pairwise_i16x8_u`, [
      i16x8([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x2, 0x2, 0x2, 0x2])],
);

// ./test/core/simd/simd_i32x4_extadd_pairwise_i16x8.wast:33
assert_return(
  () =>
    invoke($0, `i32x4.extadd_pairwise_i16x8_u`, [
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0x1fffe, 0x1fffe, 0x1fffe, 0x1fffe])],
);

// ./test/core/simd/simd_i32x4_extadd_pairwise_i16x8.wast:35
assert_return(
  () =>
    invoke($0, `i32x4.extadd_pairwise_i16x8_u`, [
      i16x8([0x7ffe, 0x7ffe, 0x7ffe, 0x7ffe, 0x7ffe, 0x7ffe, 0x7ffe, 0x7ffe]),
    ]),
  [i32x4([0xfffc, 0xfffc, 0xfffc, 0xfffc])],
);

// ./test/core/simd/simd_i32x4_extadd_pairwise_i16x8.wast:37
assert_return(
  () =>
    invoke($0, `i32x4.extadd_pairwise_i16x8_u`, [
      i16x8([0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001]),
    ]),
  [i32x4([0x10002, 0x10002, 0x10002, 0x10002])],
);

// ./test/core/simd/simd_i32x4_extadd_pairwise_i16x8.wast:39
assert_return(
  () =>
    invoke($0, `i32x4.extadd_pairwise_i16x8_u`, [
      i16x8([0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000]),
    ]),
  [i32x4([0x10000, 0x10000, 0x10000, 0x10000])],
);

// ./test/core/simd/simd_i32x4_extadd_pairwise_i16x8.wast:41
assert_return(
  () =>
    invoke($0, `i32x4.extadd_pairwise_i16x8_u`, [
      i16x8([0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff, 0x7fff]),
    ]),
  [i32x4([0xfffe, 0xfffe, 0xfffe, 0xfffe])],
);

// ./test/core/simd/simd_i32x4_extadd_pairwise_i16x8.wast:43
assert_return(
  () =>
    invoke($0, `i32x4.extadd_pairwise_i16x8_u`, [
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0x1fffe, 0x1fffe, 0x1fffe, 0x1fffe])],
);

// ./test/core/simd/simd_i32x4_extadd_pairwise_i16x8.wast:47
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.extadd_pairwise_i16x8_s (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i32x4_extadd_pairwise_i16x8.wast:48
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.extadd_pairwise_i16x8_u (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i32x4_extadd_pairwise_i16x8.wast:52
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.extadd_pairwise_i16x8_s-arg-empty (result v128)
      (i32x4.extadd_pairwise_i16x8_s)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_extadd_pairwise_i16x8.wast:60
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.extadd_pairwise_i16x8_u-arg-empty (result v128)
      (i32x4.extadd_pairwise_i16x8_u)
    )
  )`), `type mismatch`);
