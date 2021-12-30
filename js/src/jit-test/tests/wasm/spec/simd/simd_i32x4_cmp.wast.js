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

// ./test/core/simd/simd_i32x4_cmp.wast

// ./test/core/simd/simd_i32x4_cmp.wast:4
let $0 = instantiate(`(module
  (func (export "eq") (param $$x v128) (param $$y v128) (result v128) (i32x4.eq (local.get $$x) (local.get $$y)))
  (func (export "ne") (param $$x v128) (param $$y v128) (result v128) (i32x4.ne (local.get $$x) (local.get $$y)))
  (func (export "lt_s") (param $$x v128) (param $$y v128) (result v128) (i32x4.lt_s (local.get $$x) (local.get $$y)))
  (func (export "lt_u") (param $$x v128) (param $$y v128) (result v128) (i32x4.lt_u (local.get $$x) (local.get $$y)))
  (func (export "le_s") (param $$x v128) (param $$y v128) (result v128) (i32x4.le_s (local.get $$x) (local.get $$y)))
  (func (export "le_u") (param $$x v128) (param $$y v128) (result v128) (i32x4.le_u (local.get $$x) (local.get $$y)))
  (func (export "gt_s") (param $$x v128) (param $$y v128) (result v128) (i32x4.gt_s (local.get $$x) (local.get $$y)))
  (func (export "gt_u") (param $$x v128) (param $$y v128) (result v128) (i32x4.gt_u (local.get $$x) (local.get $$y)))
  (func (export "ge_s") (param $$x v128) (param $$y v128) (result v128) (i32x4.ge_s (local.get $$x) (local.get $$y)))
  (func (export "ge_u") (param $$x v128) (param $$y v128) (result v128) (i32x4.ge_u (local.get $$x) (local.get $$y)))
)`);

// ./test/core/simd/simd_i32x4_cmp.wast:23
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:26
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:29
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:32
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:35
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:38
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:41
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0x3020100, 0x11100904, 0x1a0b0a12, 0xffabaa1b]),
      i32x4([0x3020100, 0x11100904, 0x1a0b0a12, 0xffabaa1b]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:46
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:49
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:52
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:55
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:58
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:63
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:66
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:69
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:72
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:75
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:78
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:81
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0x80000001, 0xffffffff, 0x0, 0xffffffff]),
      i32x4([0x80000001, 0xffffffff, 0x0, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:86
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0xc3000000, 0xc2fe0000, 0xbf800000, 0x0]),
      f32x4([-128, -127, -1, 0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:89
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0x3f800000, 0x42fe0000, 0x43000000, 0x437f0000]),
      f32x4([1, 127, 128, 255]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:94
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:97
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:100
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0x2030001, 0x10110409, 0xb1a120a, 0xabff1baa]),
      i32x4([0xaa1bffab, 0xa121a0b, 0x9041110, 0x1000302]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:103
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0x80018000, 0x80038002, 0x80058004, 0x80078006]),
      i32x4([0x80078006, 0x80058004, 0x80038002, 0x80018000]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:106
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0x80000000, 0x7fffffff, 0x0, 0xffffffff]),
      i32x4([0x80000000, 0x80000001, 0xffffffff, 0x0]),
    ]),
  [i32x4([0xffffffff, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:111
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
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
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:114
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
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
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:117
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
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
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:120
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0x3020100, 0x7060504, 0xb0a0908, 0xf0e0d0c]),
      i8x16([
        0x0,
        0x1,
        0x2,
        0x3,
        0x4,
        0x5,
        0x6,
        0x7,
        0x8,
        0x9,
        0xa,
        0xb,
        0xc,
        0xd,
        0xe,
        0xf,
      ]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:123
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i8x16([
        0x80,
        0x81,
        0x82,
        0x83,
        0xfd,
        0xfe,
        0xff,
        0x0,
        0x0,
        0x1,
        0x2,
        0x7f,
        0x80,
        0xfd,
        0xfe,
        0xff,
      ]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:126
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0xff80ff80, 0x0, 0x1, 0xffffffff]),
      i8x16([
        0x80,
        0x80,
        0x80,
        0x80,
        0x0,
        0x0,
        0x0,
        0x0,
        0x1,
        0x1,
        0x1,
        0x1,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
    ]),
  [i32x4([0x0, 0xffffffff, 0x0, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:129
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0x55555555, 0x55555555, 0x55555555, 0x55555555]),
      i8x16([
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:134
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:137
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:140
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:143
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0x3020100, 0x7060504, 0xb0a0908, 0xf0e0d0c]),
      i16x8([0x100, 0x302, 0x504, 0x706, 0x908, 0xb0a, 0xd0c, 0xf0e]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:146
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i16x8([0x8180, 0x8382, 0xfefd, 0xff, 0x100, 0x7f02, 0xfd80, 0xfffe]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:149
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0xffffffff, 0x0, 0x1, 0xffff]),
      i16x8([0xffff, 0xffff, 0x0, 0x0, 0x1, 0x0, 0xffff, 0xffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:152
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0x55555555, 0x55555555, 0x55555555, 0x55555555]),
      i16x8([0xaaaa, 0xaaaa, 0xaaaa, 0xaaaa, 0xaaaa, 0xaaaa, 0xaaaa, 0xaaaa]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:155
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0x75bcd15, 0x75bcd15, 0x75bcd15, 0x75bcd15]),
      i32x4([0x75bcd15, 0x75bcd15, 0x75bcd15, 0x75bcd15]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:158
assert_return(
  () =>
    invoke($0, `eq`, [
      i32x4([0x12345678, 0x12345678, 0x12345678, 0x12345678]),
      i32x4([0x12345678, 0x12345678, 0x12345678, 0x12345678]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:167
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:170
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:173
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:176
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:179
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:182
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:185
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0x3020100, 0x11100904, 0x1a0b0a12, 0xffabaa1b]),
      i32x4([0x3020100, 0x11100904, 0x1a0b0a12, 0xffabaa1b]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:190
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:193
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:196
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:199
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:202
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:207
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:210
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:213
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:216
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:219
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:222
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:225
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0x80000001, 0xffffffff, 0x0, 0xffffffff]),
      i32x4([0x80000001, 0xffffffff, 0x0, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:230
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0xc3000000, 0xc2fe0000, 0xbf800000, 0x0]),
      f32x4([-128, -127, -1, 0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:233
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0x3f800000, 0x42fe0000, 0x43000000, 0x437f0000]),
      f32x4([1, 127, 128, 255]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:238
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:241
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:244
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0x2030001, 0x10110409, 0xb1a120a, 0xabff1baa]),
      i32x4([0xaa1bffab, 0xa121a0b, 0x9041110, 0x1000302]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:247
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0x80018000, 0x80038002, 0x80058004, 0x80078006]),
      i32x4([0x80078006, 0x80058004, 0x80038002, 0x80018000]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:250
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0x80000000, 0x7fffffff, 0x0, 0xffffffff]),
      i32x4([0x80000000, 0x80000001, 0xffffffff, 0x0]),
    ]),
  [i32x4([0x0, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:255
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
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
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:258
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
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
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:261
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
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
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:264
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0x3020100, 0x7060504, 0xb0a0908, 0xf0e0d0c]),
      i8x16([
        0x0,
        0x1,
        0x2,
        0x3,
        0x4,
        0x5,
        0x6,
        0x7,
        0x8,
        0x9,
        0xa,
        0xb,
        0xc,
        0xd,
        0xe,
        0xf,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:267
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i8x16([
        0x80,
        0x81,
        0x82,
        0x83,
        0xfd,
        0xfe,
        0xff,
        0x0,
        0x0,
        0x1,
        0x2,
        0x7f,
        0x80,
        0xfd,
        0xfe,
        0xff,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:270
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0xff80ff80, 0x0, 0x1, 0xffffffff]),
      i8x16([
        0x80,
        0x80,
        0x80,
        0x80,
        0x0,
        0x0,
        0x0,
        0x0,
        0x1,
        0x1,
        0x1,
        0x1,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
    ]),
  [i32x4([0xffffffff, 0x0, 0xffffffff, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:273
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0x55555555, 0x55555555, 0x55555555, 0x55555555]),
      i8x16([
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
      ]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:278
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:281
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:284
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:287
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0x3020100, 0x7060504, 0xb0a0908, 0xf0e0d0c]),
      i16x8([0x100, 0x302, 0x504, 0x706, 0x908, 0xb0a, 0xd0c, 0xf0e]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:290
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i16x8([0x8180, 0x8382, 0xfefd, 0xff, 0x100, 0x7f02, 0xfd80, 0xfffe]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:293
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0xffffff80, 0x0, 0x1, 0xff]),
      i16x8([0xff80, 0xff80, 0x0, 0x0, 0x1, 0x1, 0xff, 0xff]),
    ]),
  [i32x4([0xffffffff, 0x0, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:296
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa]),
      i16x8([0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:299
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0x75bcd15, 0x75bcd15, 0x75bcd15, 0x75bcd15]),
      i32x4([0x75bcd15, 0x75bcd15, 0x75bcd15, 0x75bcd15]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:302
assert_return(
  () =>
    invoke($0, `ne`, [
      i32x4([0x12345678, 0x12345678, 0x12345678, 0x12345678]),
      i32x4([0x12345678, 0x12345678, 0x12345678, 0x12345678]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:311
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:314
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:317
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:320
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:323
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:326
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:329
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0x3020100, 0x11100904, 0x1a0b0a12, 0xffabaa1b]),
      i32x4([0x3020100, 0x11100904, 0x1a0b0a12, 0xffabaa1b]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:334
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:337
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:340
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:343
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:346
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:351
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:354
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:357
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:360
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:363
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:366
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:369
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0x80000001, 0xffffffff, 0x0, 0xffffffff]),
      i32x4([0x80000001, 0xffffffff, 0x0, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:374
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0xc3000000, 0xc2fe0000, 0xbf800000, 0x0]),
      f32x4([-128, -127, -1, 0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:377
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0x3f800000, 0x42fe0000, 0x43000000, 0x437f0000]),
      f32x4([1, 127, 128, 255]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:382
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:385
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:388
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0x2030001, 0x10110409, 0xb1a120a, 0xabff1baa]),
      i32x4([0xaa1bffab, 0xa121a0b, 0x9041110, 0x1000302]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:391
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0x80018000, 0x80038002, 0x80058004, 0x80078006]),
      i32x4([0x80078006, 0x80058004, 0x80038002, 0x80018000]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:394
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0x80000000, 0x7fffffff, 0x0, 0xffffffff]),
      i32x4([0x80000000, 0x80000001, 0xffffffff, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:399
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
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
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:402
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
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
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:405
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
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
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:408
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0x3020100, 0x7060504, 0xb0a0908, 0xf0e0d0c]),
      i8x16([
        0x0,
        0x1,
        0x2,
        0x3,
        0x4,
        0x5,
        0x6,
        0x7,
        0x8,
        0x9,
        0xa,
        0xb,
        0xc,
        0xd,
        0xe,
        0xf,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:411
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i8x16([
        0x80,
        0x81,
        0x82,
        0x83,
        0xfd,
        0xfe,
        0xff,
        0x0,
        0x0,
        0x1,
        0x2,
        0x7f,
        0x80,
        0xfd,
        0xfe,
        0xff,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:414
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0xff80ff80, 0x0, 0x1, 0xffffffff]),
      i8x16([
        0x80,
        0x80,
        0x80,
        0x80,
        0x0,
        0x0,
        0x0,
        0x0,
        0x1,
        0x1,
        0x1,
        0x1,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0xffffffff, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:417
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0x55555555, 0x55555555, 0x55555555, 0x55555555]),
      i8x16([
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:422
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:425
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:428
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:431
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0x3020100, 0x7060504, 0xb0a0908, 0xf0e0d0c]),
      i16x8([0x100, 0x302, 0x504, 0x706, 0x908, 0xb0a, 0xd0c, 0xf0e]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:434
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i16x8([0x8180, 0x8382, 0xfefd, 0xff, 0x100, 0x7f02, 0xfd80, 0xfffe]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:437
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0xffffff80, 0x0, 0x1, 0xff]),
      i16x8([0xff80, 0xff80, 0x0, 0x0, 0x1, 0x1, 0xff, 0xff]),
    ]),
  [i32x4([0x0, 0x0, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:440
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa]),
      i16x8([0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:443
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0x75bcd15, 0x75bcd15, 0x75bcd15, 0x75bcd15]),
      i32x4([0x75bcd15, 0x75bcd15, 0x75bcd15, 0x75bcd15]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:446
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i32x4([0x90abcdef, 0x90abcdef, 0x90abcdef, 0x90abcdef]),
      i32x4([0x90abcdf0, 0x90abcdf0, 0x90abcdf0, 0x90abcdf0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:455
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:458
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:461
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:464
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:467
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:470
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:473
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0x3020100, 0x11100904, 0x1a0b0a12, 0xffabaa1b]),
      i32x4([0x3020100, 0x11100904, 0x1a0b0a12, 0xffabaa1b]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:478
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:481
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:484
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:487
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:490
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:495
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:498
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:501
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:504
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:507
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:510
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:513
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0x80000001, 0xffffffff, 0x0, 0xffffffff]),
      i32x4([0x80000001, 0xffffffff, 0x0, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:518
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0xc3000000, 0xc2fe0000, 0xbf800000, 0x0]),
      f32x4([-128, -127, -1, 0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:521
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0x3f800000, 0x42fe0000, 0x43000000, 0x437f0000]),
      f32x4([1, 127, 128, 255]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:526
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:529
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:532
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0x2030001, 0x10110409, 0xb1a120a, 0xabff1baa]),
      i32x4([0xaa1bffab, 0xa121a0b, 0x9041110, 0x1000302]),
    ]),
  [i32x4([0xffffffff, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:535
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0x80018000, 0x80038002, 0x80058004, 0x80078006]),
      i32x4([0x80078006, 0x80058004, 0x80038002, 0x80018000]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:538
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0x80000000, 0x7fffffff, 0x0, 0xffffffff]),
      i32x4([0x80000000, 0x80000001, 0xffffffff, 0x0]),
    ]),
  [i32x4([0x0, 0xffffffff, 0xffffffff, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:543
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
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
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:546
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
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
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:549
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
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
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:552
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0x3020100, 0x7060504, 0xb0a0908, 0xf0e0d0c]),
      i8x16([
        0x0,
        0x1,
        0x2,
        0x3,
        0x4,
        0x5,
        0x6,
        0x7,
        0x8,
        0x9,
        0xa,
        0xb,
        0xc,
        0xd,
        0xe,
        0xf,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:555
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i8x16([
        0x80,
        0x81,
        0x82,
        0x83,
        0xfd,
        0xfe,
        0xff,
        0x0,
        0x0,
        0x1,
        0x2,
        0x7f,
        0x80,
        0xfd,
        0xfe,
        0xff,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:558
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0xff80ff80, 0x0, 0x1, 0xffffffff]),
      i8x16([
        0x80,
        0x80,
        0x80,
        0x80,
        0x0,
        0x0,
        0x0,
        0x0,
        0x1,
        0x1,
        0x1,
        0x1,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0xffffffff, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:561
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0x55555555, 0x55555555, 0x55555555, 0x55555555]),
      i8x16([
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
      ]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:566
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:569
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:572
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:575
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0x3020100, 0x7060504, 0xb0a0908, 0xf0e0d0c]),
      i16x8([0x100, 0x302, 0x504, 0x706, 0x908, 0xb0a, 0xd0c, 0xf0e]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:578
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i16x8([0x8180, 0x8382, 0xfefd, 0xff, 0x100, 0x7f02, 0xfd80, 0xfffe]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:581
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0xffffff80, 0x0, 0x1, 0xff]),
      i16x8([0xff80, 0xff80, 0x0, 0x0, 0x1, 0x1, 0xff, 0xff]),
    ]),
  [i32x4([0x0, 0x0, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:584
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa]),
      i16x8([0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:587
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0x75bcd15, 0x75bcd15, 0x75bcd15, 0x75bcd15]),
      i32x4([0x75bcd15, 0x75bcd15, 0x75bcd15, 0x75bcd15]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:590
assert_return(
  () =>
    invoke($0, `lt_u`, [
      i32x4([0x90abcdef, 0x90abcdef, 0x90abcdef, 0x90abcdef]),
      i32x4([0x90abcdf0, 0x90abcdf0, 0x90abcdf0, 0x90abcdf0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:599
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:602
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:605
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:608
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:611
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:614
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:617
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0x3020100, 0x11100904, 0x1a0b0a12, 0xffabaa1b]),
      i32x4([0x3020100, 0x11100904, 0x1a0b0a12, 0xffabaa1b]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:622
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:625
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:628
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:631
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:634
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:639
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:642
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:645
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:648
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:651
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:654
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:657
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0x80000001, 0xffffffff, 0x0, 0xffffffff]),
      i32x4([0x80000001, 0xffffffff, 0x0, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:662
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0xc3000000, 0xc2fe0000, 0xbf800000, 0x0]),
      f32x4([-128, -127, -1, 0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:665
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0x3f800000, 0x42fe0000, 0x43000000, 0x437f0000]),
      f32x4([1, 127, 128, 255]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:670
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:673
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:676
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0x2030001, 0x10110409, 0xb1a120a, 0xabff1baa]),
      i32x4([0xaa1bffab, 0xa121a0b, 0x9041110, 0x1000302]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:679
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0x80018000, 0x80038002, 0x80058004, 0x80078006]),
      i32x4([0x80078006, 0x80058004, 0x80038002, 0x80018000]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:682
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0x80000000, 0x7fffffff, 0x0, 0xffffffff]),
      i32x4([0x80000000, 0x80000001, 0xffffffff, 0x0]),
    ]),
  [i32x4([0xffffffff, 0x0, 0x0, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:687
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
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
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:690
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
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
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:693
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
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
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:696
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0x3020100, 0x7060504, 0xb0a0908, 0xf0e0d0c]),
      i8x16([
        0x0,
        0x1,
        0x2,
        0x3,
        0x4,
        0x5,
        0x6,
        0x7,
        0x8,
        0x9,
        0xa,
        0xb,
        0xc,
        0xd,
        0xe,
        0xf,
      ]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:699
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i8x16([
        0x80,
        0x81,
        0x82,
        0x83,
        0xfd,
        0xfe,
        0xff,
        0x0,
        0x0,
        0x1,
        0x2,
        0x7f,
        0x80,
        0xfd,
        0xfe,
        0xff,
      ]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:702
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0xff80ff80, 0x0, 0x1, 0xffffffff]),
      i8x16([
        0x80,
        0x80,
        0x80,
        0x80,
        0x0,
        0x0,
        0x0,
        0x0,
        0x1,
        0x1,
        0x1,
        0x1,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
    ]),
  [i32x4([0x0, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:705
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0x55555555, 0x55555555, 0x55555555, 0x55555555]),
      i8x16([
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:710
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:713
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:716
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:719
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0x3020100, 0x7060504, 0xb0a0908, 0xf0e0d0c]),
      i16x8([0x100, 0x302, 0x504, 0x706, 0x908, 0xb0a, 0xd0c, 0xf0e]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:722
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i16x8([0x8180, 0x8382, 0xfefd, 0xff, 0x100, 0x7f02, 0xfd80, 0xfffe]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:725
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0xffffff80, 0x0, 0x1, 0xff]),
      i16x8([0xff80, 0xff80, 0x0, 0x0, 0x1, 0x1, 0xff, 0xff]),
    ]),
  [i32x4([0x0, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:728
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa]),
      i16x8([0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:731
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0x75bcd15, 0x75bcd15, 0x75bcd15, 0x75bcd15]),
      i32x4([0x75bcd15, 0x75bcd15, 0x75bcd15, 0x75bcd15]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:734
assert_return(
  () =>
    invoke($0, `le_s`, [
      i32x4([0x12345678, 0x12345678, 0x12345678, 0x12345678]),
      i32x4([0x12345678, 0x12345678, 0x12345678, 0x12345678]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:743
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:746
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:749
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:752
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:755
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:758
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:761
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0x3020100, 0x11100904, 0x1a0b0a12, 0xffabaa1b]),
      i32x4([0x3020100, 0x11100904, 0x1a0b0a12, 0xffabaa1b]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:766
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:769
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:772
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:775
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:778
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:783
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:786
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:789
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:792
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:795
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:798
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:801
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0x80000001, 0xffffffff, 0x0, 0xffffffff]),
      i32x4([0x80000001, 0xffffffff, 0x0, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:806
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0xc3000000, 0xc2fe0000, 0xbf800000, 0x0]),
      f32x4([-128, -127, -1, 0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:809
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0x3f800000, 0x42fe0000, 0x43000000, 0x437f0000]),
      f32x4([1, 127, 128, 255]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:814
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:817
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:820
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0x2030001, 0x10110409, 0xb1a120a, 0xabff1baa]),
      i32x4([0xaa1bffab, 0xa121a0b, 0x9041110, 0x1000302]),
    ]),
  [i32x4([0xffffffff, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:823
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0x80018000, 0x80038002, 0x80058004, 0x80078006]),
      i32x4([0x80078006, 0x80058004, 0x80038002, 0x80018000]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:826
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0x80000000, 0x7fffffff, 0x0, 0xffffffff]),
      i32x4([0x80000000, 0x80000001, 0xffffffff, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:831
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
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
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:834
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
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
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:837
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
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
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:840
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0x3020100, 0x7060504, 0xb0a0908, 0xf0e0d0c]),
      i8x16([
        0x0,
        0x1,
        0x2,
        0x3,
        0x4,
        0x5,
        0x6,
        0x7,
        0x8,
        0x9,
        0xa,
        0xb,
        0xc,
        0xd,
        0xe,
        0xf,
      ]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:843
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i8x16([
        0x80,
        0x81,
        0x82,
        0x83,
        0xfd,
        0xfe,
        0xff,
        0x0,
        0x0,
        0x1,
        0x2,
        0x7f,
        0x80,
        0xfd,
        0xfe,
        0xff,
      ]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:846
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0xff80ff80, 0x0, 0x1, 0xffffffff]),
      i8x16([
        0x80,
        0x80,
        0x80,
        0x80,
        0x0,
        0x0,
        0x0,
        0x0,
        0x1,
        0x1,
        0x1,
        0x1,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
    ]),
  [i32x4([0x0, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:849
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0x55555555, 0x55555555, 0x55555555, 0x55555555]),
      i8x16([
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
      ]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:854
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:857
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:860
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:863
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0x3020100, 0x7060504, 0xb0a0908, 0xf0e0d0c]),
      i16x8([0x100, 0x302, 0x504, 0x706, 0x908, 0xb0a, 0xd0c, 0xf0e]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:866
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i16x8([0x8180, 0x8382, 0xfefd, 0xff, 0x100, 0x7f02, 0xfd80, 0xfffe]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:869
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0xffffff80, 0x0, 0x1, 0xff]),
      i16x8([0xff80, 0xff80, 0x0, 0x0, 0x1, 0x1, 0xff, 0xff]),
    ]),
  [i32x4([0x0, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:872
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa]),
      i16x8([0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:875
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0x75bcd15, 0x75bcd15, 0x75bcd15, 0x75bcd15]),
      i32x4([0x75bcd15, 0x75bcd15, 0x75bcd15, 0x75bcd15]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:878
assert_return(
  () =>
    invoke($0, `le_u`, [
      i32x4([0x90abcdef, 0x90abcdef, 0x90abcdef, 0x90abcdef]),
      i32x4([0x90abcdef, 0x90abcdef, 0x90abcdef, 0x90abcdef]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:887
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:890
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:893
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:896
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:899
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:902
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:905
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0x3020100, 0x11100904, 0x1a0b0a12, 0xffabaa1b]),
      i32x4([0x3020100, 0x11100904, 0x1a0b0a12, 0xffabaa1b]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:910
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:913
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:916
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:919
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:922
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:927
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:930
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:933
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:936
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:939
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:942
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:945
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0x80000001, 0xffffffff, 0x0, 0xffffffff]),
      i32x4([0x80000001, 0xffffffff, 0x0, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:950
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0xc3000000, 0xc2fe0000, 0xbf800000, 0x0]),
      f32x4([-128, -127, -1, 0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:953
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0x3f800000, 0x42fe0000, 0x43000000, 0x437f0000]),
      f32x4([1, 127, 128, 255]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:958
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:961
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:964
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0x2030001, 0x10110409, 0xb1a120a, 0xabff1baa]),
      i32x4([0xaa1bffab, 0xa121a0b, 0x9041110, 0x1000302]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:967
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0x80018000, 0x80038002, 0x80058004, 0x80078006]),
      i32x4([0x80078006, 0x80058004, 0x80038002, 0x80018000]),
    ]),
  [i32x4([0x0, 0x0, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:970
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0x80000000, 0x7fffffff, 0x0, 0xffffffff]),
      i32x4([0x80000000, 0x80000001, 0xffffffff, 0x0]),
    ]),
  [i32x4([0x0, 0xffffffff, 0xffffffff, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:975
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
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
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:978
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
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
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:981
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
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
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:984
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0x3020100, 0x7060504, 0xb0a0908, 0xf0e0d0c]),
      i8x16([
        0x0,
        0x1,
        0x2,
        0x3,
        0x4,
        0x5,
        0x6,
        0x7,
        0x8,
        0x9,
        0xa,
        0xb,
        0xc,
        0xd,
        0xe,
        0xf,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:987
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i8x16([
        0x80,
        0x81,
        0x82,
        0x83,
        0xfd,
        0xfe,
        0xff,
        0x0,
        0x0,
        0x1,
        0x2,
        0x7f,
        0x80,
        0xfd,
        0xfe,
        0xff,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:990
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0xff80ff80, 0x0, 0x1, 0xffffffff]),
      i8x16([
        0x80,
        0x80,
        0x80,
        0x80,
        0x0,
        0x0,
        0x0,
        0x0,
        0x1,
        0x1,
        0x1,
        0x1,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
    ]),
  [i32x4([0xffffffff, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:993
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0x55555555, 0x55555555, 0x55555555, 0x55555555]),
      i8x16([
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
      ]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:998
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1001
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1004
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1007
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0x3020100, 0x7060504, 0xb0a0908, 0xf0e0d0c]),
      i16x8([0x100, 0x302, 0x504, 0x706, 0x908, 0xb0a, 0xd0c, 0xf0e]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1010
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i16x8([0x8180, 0x8382, 0xfefd, 0xff, 0x100, 0x7f02, 0xfd80, 0xfffe]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1013
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0xffff, 0x0, 0x1, 0x8000]),
      i16x8([0xffff, 0xffff, 0x0, 0x0, 0x1, 0x1, 0x8000, 0x8000]),
    ]),
  [i32x4([0xffffffff, 0x0, 0x0, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1016
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa]),
      i16x8([0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1019
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0x75bcd15, 0x75bcd15, 0x75bcd15, 0x75bcd15]),
      i32x4([0x75bcd15, 0x75bcd15, 0x75bcd15, 0x75bcd15]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1022
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i32x4([0x90abcdef, 0x90abcdef, 0x90abcdef, 0x90abcdef]),
      i32x4([0x90abcdef, 0x90abcdef, 0x90abcdef, 0x90abcdef]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1031
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1034
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1037
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1040
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1043
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1046
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1049
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0x3020100, 0x11100904, 0x1a0b0a12, 0xffabaa1b]),
      i32x4([0x3020100, 0x11100904, 0x1a0b0a12, 0xffabaa1b]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1054
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1057
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1060
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1063
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1066
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1071
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1074
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1077
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1080
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1083
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1086
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1089
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0x80000001, 0xffffffff, 0x0, 0xffffffff]),
      i32x4([0x80000001, 0xffffffff, 0x0, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1094
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0xc3000000, 0xc2fe0000, 0xbf800000, 0x0]),
      f32x4([-128, -127, -1, 0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1097
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0x3f800000, 0x42fe0000, 0x43000000, 0x437f0000]),
      f32x4([1, 127, 128, 255]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1102
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1105
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1108
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0x2030001, 0x10110409, 0xb1a120a, 0xabff1baa]),
      i32x4([0xaa1bffab, 0xa121a0b, 0x9041110, 0x1000302]),
    ]),
  [i32x4([0x0, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1111
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0x80018000, 0x80038002, 0x80058004, 0x80078006]),
      i32x4([0x80078006, 0x80058004, 0x80038002, 0x80018000]),
    ]),
  [i32x4([0x0, 0x0, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1114
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0x80000000, 0x7fffffff, 0x0, 0xffffffff]),
      i32x4([0x80000000, 0x80000001, 0xffffffff, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1119
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
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
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1122
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
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
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1125
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
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
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1128
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0x3020100, 0x7060504, 0xb0a0908, 0xf0e0d0c]),
      i8x16([
        0x0,
        0x1,
        0x2,
        0x3,
        0x4,
        0x5,
        0x6,
        0x7,
        0x8,
        0x9,
        0xa,
        0xb,
        0xc,
        0xd,
        0xe,
        0xf,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1131
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i8x16([
        0x80,
        0x81,
        0x82,
        0x83,
        0xfd,
        0xfe,
        0xff,
        0x0,
        0x0,
        0x1,
        0x2,
        0x7f,
        0x80,
        0xfd,
        0xfe,
        0xff,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1134
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0xff80ff80, 0x0, 0x1, 0xffffffff]),
      i8x16([
        0x80,
        0x80,
        0x80,
        0x80,
        0x0,
        0x0,
        0x0,
        0x0,
        0x1,
        0x1,
        0x1,
        0x1,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
    ]),
  [i32x4([0xffffffff, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1137
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0x55555555, 0x55555555, 0x55555555, 0x55555555]),
      i8x16([
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
        0xaa,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1142
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1145
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1148
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1151
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0x3020100, 0x7060504, 0xb0a0908, 0xf0e0d0c]),
      i16x8([0x100, 0x302, 0x504, 0x706, 0x908, 0xb0a, 0xd0c, 0xf0e]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1154
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i16x8([0x8180, 0x8382, 0xfefd, 0xff, 0x100, 0x7f02, 0xfd80, 0xfffe]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1157
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0xffffff80, 0x0, 0x1, 0xff]),
      i16x8([0xff80, 0xff80, 0x0, 0x0, 0x1, 0x1, 0xff, 0xff]),
    ]),
  [i32x4([0xffffffff, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1160
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa]),
      i16x8([0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1163
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0x75bcd15, 0x75bcd15, 0x75bcd15, 0x75bcd15]),
      i32x4([0x75bcd15, 0x75bcd15, 0x75bcd15, 0x75bcd15]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1166
assert_return(
  () =>
    invoke($0, `gt_u`, [
      i32x4([0x12345678, 0x12345678, 0x12345678, 0x12345678]),
      i32x4([0x12345678, 0x12345678, 0x12345678, 0x12345678]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1175
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1178
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1181
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1184
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1187
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1190
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1193
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0x3020100, 0x11100904, 0x1a0b0a12, 0xffabaa1b]),
      i32x4([0x3020100, 0x11100904, 0x1a0b0a12, 0xffabaa1b]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1198
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1201
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1204
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1207
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1210
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1215
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1218
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1221
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1224
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1227
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1230
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1233
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0x80000001, 0xffffffff, 0x0, 0xffffffff]),
      i32x4([0x80000001, 0xffffffff, 0x0, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1238
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0xc3000000, 0xc2fe0000, 0xbf800000, 0x0]),
      f32x4([-128, -127, -1, 0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1241
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0x3f800000, 0x42fe0000, 0x43000000, 0x437f0000]),
      f32x4([1, 127, 128, 255]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1246
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1249
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1252
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0x2030001, 0x10110409, 0xb1a120a, 0xabff1baa]),
      i32x4([0xaa1bffab, 0xa121a0b, 0x9041110, 0x1000302]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1255
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0x80018000, 0x80038002, 0x80058004, 0x80078006]),
      i32x4([0x80078006, 0x80058004, 0x80038002, 0x80018000]),
    ]),
  [i32x4([0x0, 0x0, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1258
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0x80000000, 0x7fffffff, 0x0, 0xffffffff]),
      i32x4([0x80000000, 0x80000001, 0xffffffff, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1263
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
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
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1266
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
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
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1269
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
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
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1272
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0x3020100, 0x7060504, 0xb0a0908, 0xf0e0d0c]),
      i8x16([
        0x0,
        0x1,
        0x2,
        0x3,
        0x4,
        0x5,
        0x6,
        0x7,
        0x8,
        0x9,
        0xa,
        0xb,
        0xc,
        0xd,
        0xe,
        0xf,
      ]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1275
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i8x16([
        0x80,
        0x81,
        0x82,
        0x83,
        0xfd,
        0xfe,
        0xff,
        0x0,
        0x0,
        0x1,
        0x2,
        0x7f,
        0x80,
        0xfd,
        0xfe,
        0xff,
      ]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1278
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0xff80ff80, 0x0, 0x1, 0xffffffff]),
      i8x16([
        0x80,
        0x80,
        0x80,
        0x80,
        0x0,
        0x0,
        0x0,
        0x0,
        0x1,
        0x1,
        0x1,
        0x1,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0x0, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1281
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0x55555555, 0x55555555, 0x55555555, 0x55555555]),
      i8x16([
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
      ]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1286
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1289
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1292
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1295
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0x3020100, 0x7060504, 0xb0a0908, 0xf0e0d0c]),
      i16x8([0x100, 0x302, 0x504, 0x706, 0x908, 0xb0a, 0xd0c, 0xf0e]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1298
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i16x8([0x8180, 0x8382, 0xfefd, 0xff, 0x100, 0x7f02, 0xfd80, 0xfffe]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1301
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0xffff, 0x0, 0x1, 0x8000]),
      i16x8([0xffff, 0xffff, 0x0, 0x0, 0x1, 0x1, 0x8000, 0x8000]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0x0, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1304
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa]),
      i16x8([0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1307
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0x75bcd15, 0x75bcd15, 0x75bcd15, 0x75bcd15]),
      i32x4([0x75bcd15, 0x75bcd15, 0x75bcd15, 0x75bcd15]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1310
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i32x4([0x12345678, 0x12345678, 0x12345678, 0x12345678]),
      i32x4([0x12345678, 0x12345678, 0x12345678, 0x12345678]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1319
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1322
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1325
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1328
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1331
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1334
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1337
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0x3020100, 0x11100904, 0x1a0b0a12, 0xffabaa1b]),
      i32x4([0x3020100, 0x11100904, 0x1a0b0a12, 0xffabaa1b]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1342
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1345
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1348
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1351
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
      i32x4([0x80808080, 0x80808080, 0x80808080, 0x80808080]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1354
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1359
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1362
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1365
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1368
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1371
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1374
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1377
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0x80000001, 0xffffffff, 0x0, 0xffffffff]),
      i32x4([0x80000001, 0xffffffff, 0x0, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1382
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0xc3000000, 0xc2fe0000, 0xbf800000, 0x0]),
      f32x4([-128, -127, -1, 0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1385
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0x3f800000, 0x42fe0000, 0x43000000, 0x437f0000]),
      f32x4([1, 127, 128, 255]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1390
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f, 0xf0f0f0f]),
      i32x4([0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1393
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0xffffffff, 0xffffffff, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1396
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0x2030001, 0x10110409, 0xb1a120a, 0xabff1baa]),
      i32x4([0xaa1bffab, 0xa121a0b, 0x9041110, 0x1000302]),
    ]),
  [i32x4([0x0, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1399
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0x80018000, 0x80038002, 0x80058004, 0x80078006]),
      i32x4([0x80078006, 0x80058004, 0x80038002, 0x80018000]),
    ]),
  [i32x4([0x0, 0x0, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1402
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0x80000000, 0x7fffffff, 0x0, 0xffffffff]),
      i32x4([0x80000000, 0x80000001, 0xffffffff, 0x0]),
    ]),
  [i32x4([0xffffffff, 0x0, 0x0, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1407
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
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
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1410
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
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
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1413
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
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
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1416
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0x3020100, 0x7060504, 0xb0a0908, 0xf0e0d0c]),
      i8x16([
        0x0,
        0x1,
        0x2,
        0x3,
        0x4,
        0x5,
        0x6,
        0x7,
        0x8,
        0x9,
        0xa,
        0xb,
        0xc,
        0xd,
        0xe,
        0xf,
      ]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1419
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i8x16([
        0x80,
        0x81,
        0x82,
        0x83,
        0xfd,
        0xfe,
        0xff,
        0x0,
        0x0,
        0x1,
        0x2,
        0x7f,
        0x80,
        0xfd,
        0xfe,
        0xff,
      ]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1422
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0xff80ff80, 0x0, 0x1, 0xffffffff]),
      i8x16([
        0x80,
        0x80,
        0x80,
        0x80,
        0x0,
        0x0,
        0x0,
        0x0,
        0x1,
        0x1,
        0x1,
        0x1,
        0xff,
        0xff,
        0xff,
        0xff,
      ]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0x0, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1425
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa]),
      i8x16([
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
        0x55,
      ]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1430
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1433
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i16x8([0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1436
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i16x8([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1439
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0x3020100, 0x7060504, 0xb0a0908, 0xf0e0d0c]),
      i16x8([0x100, 0x302, 0x504, 0x706, 0x908, 0xb0a, 0xd0c, 0xf0e]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1442
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0x83828180, 0xfffefd, 0x7f020100, 0xfffefd80]),
      i16x8([0x8180, 0x8382, 0xfefd, 0xff, 0x100, 0x7f02, 0xfd80, 0xfffe]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1445
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0xffffff80, 0x0, 0x1, 0xff]),
      i16x8([0xffff, 0xffff, 0x0, 0x0, 0x1, 0x1, 0x8000, 0x8000]),
    ]),
  [i32x4([0x0, 0xffffffff, 0x0, 0x0])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1448
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa]),
      i16x8([0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555, 0x5555]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1451
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0x75bcd15, 0x75bcd15, 0x75bcd15, 0x75bcd15]),
      i32x4([0x75bcd15, 0x75bcd15, 0x75bcd15, 0x75bcd15]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1454
assert_return(
  () =>
    invoke($0, `ge_u`, [
      i32x4([0x12345678, 0x12345678, 0x12345678, 0x12345678]),
      i32x4([0x12345678, 0x12345678, 0x12345678, 0x12345678]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_i32x4_cmp.wast:1461
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.eq (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i32x4_cmp.wast:1462
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.ge_s (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i32x4_cmp.wast:1463
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.ge_u (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i32x4_cmp.wast:1464
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.gt_s (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i32x4_cmp.wast:1465
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.gt_u (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i32x4_cmp.wast:1466
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.le_s (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i32x4_cmp.wast:1467
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.le_u (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i32x4_cmp.wast:1468
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.lt_s (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i32x4_cmp.wast:1469
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.lt_u (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i32x4_cmp.wast:1470
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i32x4.ne (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i32x4_cmp.wast:1475
let $1 = instantiate(`(module (memory 1)
  (func (export "eq-in-block")
    (block
      (drop
        (block (result v128)
          (i32x4.eq
            (block (result v128) (v128.load (i32.const 0)))
            (block (result v128) (v128.load (i32.const 1)))
          )
        )
      )
    )
  )
  (func (export "ne-in-block")
    (block
      (drop
        (block (result v128)
          (i32x4.ne
            (block (result v128) (v128.load (i32.const 0)))
            (block (result v128) (v128.load (i32.const 1)))
          )
        )
      )
    )
  )
  (func (export "lt_s-in-block")
    (block
      (drop
        (block (result v128)
          (i32x4.lt_s
            (block (result v128) (v128.load (i32.const 0)))
            (block (result v128) (v128.load (i32.const 1)))
          )
        )
      )
    )
  )
  (func (export "le_u-in-block")
    (block
      (drop
        (block (result v128)
          (i32x4.le_u
            (block (result v128) (v128.load (i32.const 0)))
            (block (result v128) (v128.load (i32.const 1)))
          )
        )
      )
    )
  )
  (func (export "gt_u-in-block")
    (block
      (drop
        (block (result v128)
          (i32x4.gt_u
            (block (result v128) (v128.load (i32.const 0)))
            (block (result v128) (v128.load (i32.const 1)))
          )
        )
      )
    )
  )
  (func (export "ge_s-in-block")
    (block
      (drop
        (block (result v128)
          (i32x4.ge_s
            (block (result v128) (v128.load (i32.const 0)))
            (block (result v128) (v128.load (i32.const 1)))
          )
        )
      )
    )
  )
  (func (export "nested-eq")
    (drop
      (i32x4.eq
        (i32x4.eq
          (i32x4.eq
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
          (i32x4.eq
            (v128.load (i32.const 2))
            (v128.load (i32.const 3))
          )
        )
        (i32x4.eq
          (i32x4.eq
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
          (i32x4.eq
            (v128.load (i32.const 2))
            (v128.load (i32.const 3))
          )
        )
      )
    )
  )
  (func (export "nested-ne")
    (drop
      (i32x4.ne
        (i32x4.ne
          (i32x4.ne
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
          (i32x4.ne
            (v128.load (i32.const 2))
            (v128.load (i32.const 3))
          )
        )
        (i32x4.ne
          (i32x4.ne
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
          (i32x4.ne
            (v128.load (i32.const 2))
            (v128.load (i32.const 3))
          )
        )
      )
    )
  )
  (func (export "nested-lt_s")
    (drop
      (i32x4.lt_s
        (i32x4.lt_s
          (i32x4.lt_s
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
          (i32x4.lt_s
            (v128.load (i32.const 2))
            (v128.load (i32.const 3))
          )
        )
        (i32x4.lt_s
          (i32x4.lt_s
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
          (i32x4.lt_s
            (v128.load (i32.const 2))
            (v128.load (i32.const 3))
          )
        )
      )
    )
  )
  (func (export "nested-le_u")
    (drop
      (i32x4.le_u
        (i32x4.le_u
          (i32x4.le_u
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
          (i32x4.le_u
            (v128.load (i32.const 2))
            (v128.load (i32.const 3))
          )
        )
        (i32x4.le_u
          (i32x4.le_u
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
          (i32x4.le_u
            (v128.load (i32.const 2))
            (v128.load (i32.const 3))
          )
        )
      )
    )
  )
  (func (export "nested-gt_u")
    (drop
      (i32x4.gt_u
        (i32x4.gt_u
          (i32x4.gt_u
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
          (i32x4.gt_u
            (v128.load (i32.const 2))
            (v128.load (i32.const 3))
          )
        )
        (i32x4.gt_u
          (i32x4.gt_u
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
          (i32x4.gt_u
            (v128.load (i32.const 2))
            (v128.load (i32.const 3))
          )
        )
      )
    )
  )
  (func (export "nested-ge_s")
    (drop
      (i32x4.ge_s
        (i32x4.ge_s
          (i32x4.ge_s
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
          (i32x4.ge_s
            (v128.load (i32.const 2))
            (v128.load (i32.const 3))
          )
        )
        (i32x4.ge_s
          (i32x4.ge_s
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
          (i32x4.ge_s
            (v128.load (i32.const 2))
            (v128.load (i32.const 3))
          )
        )
      )
    )
  )
  (func (export "as-param")
    (drop
      (i32x4.ge_u
        (i32x4.eq
          (i32x4.lt_s
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
          (i32x4.le_u
            (v128.load (i32.const 2))
            (v128.load (i32.const 3))
          )
        )
        (i32x4.ne
          (i32x4.gt_s
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
          (i32x4.lt_u
            (v128.load (i32.const 2))
            (v128.load (i32.const 3))
          )
        )
      )
    )
  )
)`);

// ./test/core/simd/simd_i32x4_cmp.wast:1731
assert_return(() => invoke($1, `eq-in-block`, []), []);

// ./test/core/simd/simd_i32x4_cmp.wast:1732
assert_return(() => invoke($1, `ne-in-block`, []), []);

// ./test/core/simd/simd_i32x4_cmp.wast:1733
assert_return(() => invoke($1, `lt_s-in-block`, []), []);

// ./test/core/simd/simd_i32x4_cmp.wast:1734
assert_return(() => invoke($1, `le_u-in-block`, []), []);

// ./test/core/simd/simd_i32x4_cmp.wast:1735
assert_return(() => invoke($1, `gt_u-in-block`, []), []);

// ./test/core/simd/simd_i32x4_cmp.wast:1736
assert_return(() => invoke($1, `ge_s-in-block`, []), []);

// ./test/core/simd/simd_i32x4_cmp.wast:1737
assert_return(() => invoke($1, `nested-eq`, []), []);

// ./test/core/simd/simd_i32x4_cmp.wast:1738
assert_return(() => invoke($1, `nested-ne`, []), []);

// ./test/core/simd/simd_i32x4_cmp.wast:1739
assert_return(() => invoke($1, `nested-lt_s`, []), []);

// ./test/core/simd/simd_i32x4_cmp.wast:1740
assert_return(() => invoke($1, `nested-le_u`, []), []);

// ./test/core/simd/simd_i32x4_cmp.wast:1741
assert_return(() => invoke($1, `nested-gt_u`, []), []);

// ./test/core/simd/simd_i32x4_cmp.wast:1742
assert_return(() => invoke($1, `nested-ge_s`, []), []);

// ./test/core/simd/simd_i32x4_cmp.wast:1743
assert_return(() => invoke($1, `as-param`, []), []);

// ./test/core/simd/simd_i32x4_cmp.wast:1748
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.eq-1st-arg-empty (result v128)
      (i32x4.eq (v128.const i32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_cmp.wast:1756
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.eq-arg-empty (result v128)
      (i32x4.eq)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_cmp.wast:1764
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.ne-1st-arg-empty (result v128)
      (i32x4.ne (v128.const i32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_cmp.wast:1772
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.ne-arg-empty (result v128)
      (i32x4.ne)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_cmp.wast:1780
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.lt_s-1st-arg-empty (result v128)
      (i32x4.lt_s (v128.const i32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_cmp.wast:1788
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.lt_s-arg-empty (result v128)
      (i32x4.lt_s)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_cmp.wast:1796
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.lt_u-1st-arg-empty (result v128)
      (i32x4.lt_u (v128.const i32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_cmp.wast:1804
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.lt_u-arg-empty (result v128)
      (i32x4.lt_u)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_cmp.wast:1812
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.le_s-1st-arg-empty (result v128)
      (i32x4.le_s (v128.const i32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_cmp.wast:1820
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.le_s-arg-empty (result v128)
      (i32x4.le_s)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_cmp.wast:1828
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.le_u-1st-arg-empty (result v128)
      (i32x4.le_u (v128.const i32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_cmp.wast:1836
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.le_u-arg-empty (result v128)
      (i32x4.le_u)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_cmp.wast:1844
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.gt_s-1st-arg-empty (result v128)
      (i32x4.gt_s (v128.const i32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_cmp.wast:1852
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.gt_s-arg-empty (result v128)
      (i32x4.gt_s)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_cmp.wast:1860
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.gt_u-1st-arg-empty (result v128)
      (i32x4.gt_u (v128.const i32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_cmp.wast:1868
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.gt_u-arg-empty (result v128)
      (i32x4.gt_u)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_cmp.wast:1876
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.ge_s-1st-arg-empty (result v128)
      (i32x4.ge_s (v128.const i32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_cmp.wast:1884
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.ge_s-arg-empty (result v128)
      (i32x4.ge_s)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_cmp.wast:1892
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.ge_u-1st-arg-empty (result v128)
      (i32x4.ge_u (v128.const i32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_cmp.wast:1900
assert_invalid(() =>
  instantiate(`(module
    (func $$i32x4.ge_u-arg-empty (result v128)
      (i32x4.ge_u)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i32x4_cmp.wast:1910
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (param $$x v128) (param $$y v128) (result v128) (i4x32.eq (local.get $$x) (local.get $$y))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_i32x4_cmp.wast:1911
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (param $$x v128) (param $$y v128) (result v128) (i4x32.ne (local.get $$x) (local.get $$y))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_i32x4_cmp.wast:1912
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (param $$x v128) (param $$y v128) (result v128) (i4x32.lt_s (local.get $$x) (local.get $$y))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_i32x4_cmp.wast:1913
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (param $$x v128) (param $$y v128) (result v128) (i4x32.lt_u (local.get $$x) (local.get $$y))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_i32x4_cmp.wast:1914
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (param $$x v128) (param $$y v128) (result v128) (i4x32.le_s (local.get $$x) (local.get $$y))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_i32x4_cmp.wast:1915
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (param $$x v128) (param $$y v128) (result v128) (i4x32.le_u (local.get $$x) (local.get $$y))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_i32x4_cmp.wast:1916
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (param $$x v128) (param $$y v128) (result v128) (i4x32.gt_s (local.get $$x) (local.get $$y))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_i32x4_cmp.wast:1917
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (param $$x v128) (param $$y v128) (result v128) (i4x32.gt_u (local.get $$x) (local.get $$y))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_i32x4_cmp.wast:1918
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (param $$x v128) (param $$y v128) (result v128) (i4x32.ge_s (local.get $$x) (local.get $$y))) `,
    ),
  `unknown operator`,
);

// ./test/core/simd/simd_i32x4_cmp.wast:1919
assert_malformed(
  () =>
    instantiate(
      `(memory 1) (func (param $$x v128) (param $$y v128) (result v128) (i4x32.ge_u (local.get $$x) (local.get $$y))) `,
    ),
  `unknown operator`,
);
