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

// ./test/core/simd/simd_i16x8_extadd_pairwise_i8x16.wast

// ./test/core/simd/simd_i16x8_extadd_pairwise_i8x16.wast:4
let $0 = instantiate(`(module
  (func (export "i16x8.extadd_pairwise_i8x16_s") (param v128) (result v128) (i16x8.extadd_pairwise_i8x16_s (local.get 0)))
  (func (export "i16x8.extadd_pairwise_i8x16_u") (param v128) (result v128) (i16x8.extadd_pairwise_i8x16_u (local.get 0)))
)`);

// ./test/core/simd/simd_i16x8_extadd_pairwise_i8x16.wast:11
assert_return(
  () =>
    invoke($0, `i16x8.extadd_pairwise_i8x16_s`, [
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
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i16x8_extadd_pairwise_i8x16.wast:13
assert_return(
  () =>
    invoke($0, `i16x8.extadd_pairwise_i8x16_s`, [
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
      ]),
    ]),
  [i16x8([0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2])],
);

// ./test/core/simd/simd_i16x8_extadd_pairwise_i8x16.wast:15
assert_return(
  () =>
    invoke($0, `i16x8.extadd_pairwise_i8x16_s`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
    ]),
  [i16x8([0xfffe, 0xfffe, 0xfffe, 0xfffe, 0xfffe, 0xfffe, 0xfffe, 0xfffe])],
);

// ./test/core/simd/simd_i16x8_extadd_pairwise_i8x16.wast:17
assert_return(
  () =>
    invoke($0, `i16x8.extadd_pairwise_i8x16_s`, [
      i8x16([
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
      ]),
    ]),
  [i16x8([0xfc, 0xfc, 0xfc, 0xfc, 0xfc, 0xfc, 0xfc, 0xfc])],
);

// ./test/core/simd/simd_i16x8_extadd_pairwise_i8x16.wast:19
assert_return(
  () =>
    invoke($0, `i16x8.extadd_pairwise_i8x16_s`, [
      i8x16([
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
      ]),
    ]),
  [i16x8([0xff02, 0xff02, 0xff02, 0xff02, 0xff02, 0xff02, 0xff02, 0xff02])],
);

// ./test/core/simd/simd_i16x8_extadd_pairwise_i8x16.wast:21
assert_return(
  () =>
    invoke($0, `i16x8.extadd_pairwise_i8x16_s`, [
      i8x16([
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
      ]),
    ]),
  [i16x8([0xff00, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00, 0xff00])],
);

// ./test/core/simd/simd_i16x8_extadd_pairwise_i8x16.wast:23
assert_return(
  () =>
    invoke($0, `i16x8.extadd_pairwise_i8x16_s`, [
      i8x16([
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
      ]),
    ]),
  [i16x8([0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe])],
);

// ./test/core/simd/simd_i16x8_extadd_pairwise_i8x16.wast:25
assert_return(
  () =>
    invoke($0, `i16x8.extadd_pairwise_i8x16_s`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
    ]),
  [i16x8([0xfffe, 0xfffe, 0xfffe, 0xfffe, 0xfffe, 0xfffe, 0xfffe, 0xfffe])],
);

// ./test/core/simd/simd_i16x8_extadd_pairwise_i8x16.wast:29
assert_return(
  () =>
    invoke($0, `i16x8.extadd_pairwise_i8x16_u`, [
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
  [i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i16x8_extadd_pairwise_i8x16.wast:31
assert_return(
  () =>
    invoke($0, `i16x8.extadd_pairwise_i8x16_u`, [
      i8x16([
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
        0x1,
      ]),
    ]),
  [i16x8([0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2])],
);

// ./test/core/simd/simd_i16x8_extadd_pairwise_i8x16.wast:33
assert_return(
  () =>
    invoke($0, `i16x8.extadd_pairwise_i8x16_u`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
    ]),
  [i16x8([0x1fe, 0x1fe, 0x1fe, 0x1fe, 0x1fe, 0x1fe, 0x1fe, 0x1fe])],
);

// ./test/core/simd/simd_i16x8_extadd_pairwise_i8x16.wast:35
assert_return(
  () =>
    invoke($0, `i16x8.extadd_pairwise_i8x16_u`, [
      i8x16([
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
        0x7e,
      ]),
    ]),
  [i16x8([0xfc, 0xfc, 0xfc, 0xfc, 0xfc, 0xfc, 0xfc, 0xfc])],
);

// ./test/core/simd/simd_i16x8_extadd_pairwise_i8x16.wast:37
assert_return(
  () =>
    invoke($0, `i16x8.extadd_pairwise_i8x16_u`, [
      i8x16([
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
        0x81,
      ]),
    ]),
  [i16x8([0x102, 0x102, 0x102, 0x102, 0x102, 0x102, 0x102, 0x102])],
);

// ./test/core/simd/simd_i16x8_extadd_pairwise_i8x16.wast:39
assert_return(
  () =>
    invoke($0, `i16x8.extadd_pairwise_i8x16_u`, [
      i8x16([
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
        0x80,
      ]),
    ]),
  [i16x8([0x100, 0x100, 0x100, 0x100, 0x100, 0x100, 0x100, 0x100])],
);

// ./test/core/simd/simd_i16x8_extadd_pairwise_i8x16.wast:41
assert_return(
  () =>
    invoke($0, `i16x8.extadd_pairwise_i8x16_u`, [
      i8x16([
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x7f,
      ]),
    ]),
  [i16x8([0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe])],
);

// ./test/core/simd/simd_i16x8_extadd_pairwise_i8x16.wast:43
assert_return(
  () =>
    invoke($0, `i16x8.extadd_pairwise_i8x16_u`, [
      i8x16([
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
    ]),
  [i16x8([0x1fe, 0x1fe, 0x1fe, 0x1fe, 0x1fe, 0x1fe, 0x1fe, 0x1fe])],
);

// ./test/core/simd/simd_i16x8_extadd_pairwise_i8x16.wast:47
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i16x8.extadd_pairwise_i8x16_s (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i16x8_extadd_pairwise_i8x16.wast:48
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i16x8.extadd_pairwise_i8x16_u (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i16x8_extadd_pairwise_i8x16.wast:52
assert_invalid(() =>
  instantiate(`(module
    (func $$i16x8.extadd_pairwise_i8x16_s-arg-empty (result v128)
      (i16x8.extadd_pairwise_i8x16_s)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i16x8_extadd_pairwise_i8x16.wast:60
assert_invalid(() =>
  instantiate(`(module
    (func $$i16x8.extadd_pairwise_i8x16_u-arg-empty (result v128)
      (i16x8.extadd_pairwise_i8x16_u)
    )
  )`), `type mismatch`);
