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

// ./test/core/simd/simd_bitwise.wast

// ./test/core/simd/simd_bitwise.wast:3
let $0 = instantiate(`(module
  (func (export "not") (param $$0 v128) (result v128) (v128.not (local.get $$0)))
  (func (export "and") (param $$0 v128) (param $$1 v128) (result v128) (v128.and (local.get $$0) (local.get $$1)))
  (func (export "or") (param $$0 v128) (param $$1 v128) (result v128) (v128.or (local.get $$0) (local.get $$1)))
  (func (export "xor") (param $$0 v128) (param $$1 v128) (result v128) (v128.xor (local.get $$0) (local.get $$1)))
  (func (export "bitselect") (param $$0 v128) (param $$1 v128) (param $$2 v128) (result v128)
    (v128.bitselect (local.get $$0) (local.get $$1) (local.get $$2))
  )
  (func (export "andnot") (param $$0 v128) (param $$1 v128) (result v128) (v128.andnot (local.get $$0) (local.get $$1)))
)`);

// ./test/core/simd/simd_bitwise.wast:15
assert_return(() => invoke($0, `not`, [i32x4([0x0, 0x0, 0x0, 0x0])]), [
  i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
]);

// ./test/core/simd/simd_bitwise.wast:17
assert_return(
  () =>
    invoke($0, `not`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_bitwise.wast:19
assert_return(
  () => invoke($0, `not`, [i32x4([0xffffffff, 0x0, 0xffffffff, 0x0])]),
  [i32x4([0x0, 0xffffffff, 0x0, 0xffffffff])],
);

// ./test/core/simd/simd_bitwise.wast:21
assert_return(
  () => invoke($0, `not`, [i32x4([0x0, 0xffffffff, 0x0, 0xffffffff])]),
  [i32x4([0xffffffff, 0x0, 0xffffffff, 0x0])],
);

// ./test/core/simd/simd_bitwise.wast:23
assert_return(
  () =>
    invoke($0, `not`, [
      i32x4([0x55555555, 0x55555555, 0x55555555, 0x55555555]),
    ]),
  [i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa])],
);

// ./test/core/simd/simd_bitwise.wast:25
assert_return(
  () =>
    invoke($0, `not`, [
      i32x4([0xcccccccc, 0xcccccccc, 0xcccccccc, 0xcccccccc]),
    ]),
  [i32x4([0x33333333, 0x33333333, 0x33333333, 0x33333333])],
);

// ./test/core/simd/simd_bitwise.wast:27
assert_return(
  () =>
    invoke($0, `not`, [
      i32x4([0x499602d2, 0x499602d2, 0x499602d2, 0x499602d2]),
    ]),
  [i32x4([0xb669fd2d, 0xb669fd2d, 0xb669fd2d, 0xb669fd2d])],
);

// ./test/core/simd/simd_bitwise.wast:29
assert_return(
  () =>
    invoke($0, `not`, [
      i32x4([0x12345678, 0x12345678, 0x12345678, 0x12345678]),
    ]),
  [i32x4([0xedcba987, 0xedcba987, 0xedcba987, 0xedcba987])],
);

// ./test/core/simd/simd_bitwise.wast:31
assert_return(
  () =>
    invoke($0, `and`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0xffffffff, 0x0, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0xffffffff])],
);

// ./test/core/simd/simd_bitwise.wast:34
assert_return(
  () =>
    invoke($0, `and`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_bitwise.wast:37
assert_return(
  () =>
    invoke($0, `and`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_bitwise.wast:40
assert_return(
  () =>
    invoke($0, `and`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_bitwise.wast:43
assert_return(
  () =>
    invoke($0, `and`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_bitwise.wast:46
assert_return(
  () =>
    invoke($0, `and`, [
      i32x4([0xff, 0xff, 0xff, 0xff]),
      i32x4([0x55, 0x55, 0x55, 0x55]),
    ]),
  [i32x4([0x55, 0x55, 0x55, 0x55])],
);

// ./test/core/simd/simd_bitwise.wast:49
assert_return(
  () =>
    invoke($0, `and`, [
      i32x4([0xff, 0xff, 0xff, 0xff]),
      i32x4([0x80, 0x80, 0x80, 0x80]),
    ]),
  [i32x4([0x80, 0x80, 0x80, 0x80])],
);

// ./test/core/simd/simd_bitwise.wast:52
assert_return(
  () =>
    invoke($0, `and`, [
      i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa]),
      i32x4([0xa, 0x80, 0x5, 0xa5]),
    ]),
  [i32x4([0xa, 0x80, 0x0, 0xa0])],
);

// ./test/core/simd/simd_bitwise.wast:55
assert_return(
  () =>
    invoke($0, `and`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x55555555, 0x55555555, 0x55555555, 0x55555555]),
    ]),
  [i32x4([0x55555555, 0x55555555, 0x55555555, 0x55555555])],
);

// ./test/core/simd/simd_bitwise.wast:58
assert_return(
  () =>
    invoke($0, `and`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa]),
    ]),
  [i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa])],
);

// ./test/core/simd/simd_bitwise.wast:61
assert_return(
  () =>
    invoke($0, `and`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_bitwise.wast:64
assert_return(
  () =>
    invoke($0, `and`, [
      i32x4([0x55555555, 0x55555555, 0x55555555, 0x55555555]),
      i32x4([0x5555, 0xffff, 0x55ff, 0x5fff]),
    ]),
  [i32x4([0x5555, 0x5555, 0x5555, 0x5555])],
);

// ./test/core/simd/simd_bitwise.wast:67
assert_return(
  () =>
    invoke($0, `and`, [
      i32x4([0x499602d2, 0x499602d2, 0x499602d2, 0x499602d2]),
      i32x4([0x499602d2, 0x499602d2, 0x499602d2, 0x499602d2]),
    ]),
  [i32x4([0x499602d2, 0x499602d2, 0x499602d2, 0x499602d2])],
);

// ./test/core/simd/simd_bitwise.wast:70
assert_return(
  () =>
    invoke($0, `and`, [
      i32x4([0x12345678, 0x12345678, 0x12345678, 0x12345678]),
      i32x4([0x90abcdef, 0x90abcdef, 0x90abcdef, 0x90abcdef]),
    ]),
  [i32x4([0x10204468, 0x10204468, 0x10204468, 0x10204468])],
);

// ./test/core/simd/simd_bitwise.wast:73
assert_return(
  () =>
    invoke($0, `or`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0xffffffff, 0x0, 0xffffffff]),
    ]),
  [i32x4([0x0, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_bitwise.wast:76
assert_return(
  () =>
    invoke($0, `or`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_bitwise.wast:79
assert_return(
  () =>
    invoke($0, `or`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_bitwise.wast:82
assert_return(
  () =>
    invoke($0, `or`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_bitwise.wast:85
assert_return(
  () =>
    invoke($0, `or`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x1, 0x1, 0x1, 0x1])],
);

// ./test/core/simd/simd_bitwise.wast:88
assert_return(
  () =>
    invoke($0, `or`, [
      i32x4([0xff, 0xff, 0xff, 0xff]),
      i32x4([0x55, 0x55, 0x55, 0x55]),
    ]),
  [i32x4([0xff, 0xff, 0xff, 0xff])],
);

// ./test/core/simd/simd_bitwise.wast:91
assert_return(
  () =>
    invoke($0, `or`, [
      i32x4([0xff, 0xff, 0xff, 0xff]),
      i32x4([0x80, 0x80, 0x80, 0x80]),
    ]),
  [i32x4([0xff, 0xff, 0xff, 0xff])],
);

// ./test/core/simd/simd_bitwise.wast:94
assert_return(
  () =>
    invoke($0, `or`, [
      i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa]),
      i32x4([0xa, 0x80, 0x5, 0xa5]),
    ]),
  [i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaf, 0xaaaaaaaf])],
);

// ./test/core/simd/simd_bitwise.wast:97
assert_return(
  () =>
    invoke($0, `or`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x55555555, 0x55555555, 0x55555555, 0x55555555]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_bitwise.wast:100
assert_return(
  () =>
    invoke($0, `or`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_bitwise.wast:103
assert_return(
  () =>
    invoke($0, `or`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_bitwise.wast:106
assert_return(
  () =>
    invoke($0, `or`, [
      i32x4([0x55555555, 0x55555555, 0x55555555, 0x55555555]),
      i32x4([0x5555, 0xffff, 0x55ff, 0x5fff]),
    ]),
  [i32x4([0x55555555, 0x5555ffff, 0x555555ff, 0x55555fff])],
);

// ./test/core/simd/simd_bitwise.wast:109
assert_return(
  () =>
    invoke($0, `or`, [
      i32x4([0x499602d2, 0x499602d2, 0x499602d2, 0x499602d2]),
      i32x4([0x499602d2, 0x499602d2, 0x499602d2, 0x499602d2]),
    ]),
  [i32x4([0x499602d2, 0x499602d2, 0x499602d2, 0x499602d2])],
);

// ./test/core/simd/simd_bitwise.wast:112
assert_return(
  () =>
    invoke($0, `or`, [
      i32x4([0x12345678, 0x12345678, 0x12345678, 0x12345678]),
      i32x4([0x90abcdef, 0x90abcdef, 0x90abcdef, 0x90abcdef]),
    ]),
  [i32x4([0x92bfdfff, 0x92bfdfff, 0x92bfdfff, 0x92bfdfff])],
);

// ./test/core/simd/simd_bitwise.wast:115
assert_return(
  () =>
    invoke($0, `xor`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0xffffffff, 0x0, 0xffffffff]),
    ]),
  [i32x4([0x0, 0xffffffff, 0xffffffff, 0x0])],
);

// ./test/core/simd/simd_bitwise.wast:118
assert_return(
  () =>
    invoke($0, `xor`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_bitwise.wast:121
assert_return(
  () =>
    invoke($0, `xor`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_bitwise.wast:124
assert_return(
  () =>
    invoke($0, `xor`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_bitwise.wast:127
assert_return(
  () =>
    invoke($0, `xor`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_bitwise.wast:130
assert_return(
  () =>
    invoke($0, `xor`, [
      i32x4([0xff, 0xff, 0xff, 0xff]),
      i32x4([0x55, 0x55, 0x55, 0x55]),
    ]),
  [i32x4([0xaa, 0xaa, 0xaa, 0xaa])],
);

// ./test/core/simd/simd_bitwise.wast:133
assert_return(
  () =>
    invoke($0, `xor`, [
      i32x4([0xff, 0xff, 0xff, 0xff]),
      i32x4([0x80, 0x80, 0x80, 0x80]),
    ]),
  [i32x4([0x7f, 0x7f, 0x7f, 0x7f])],
);

// ./test/core/simd/simd_bitwise.wast:136
assert_return(
  () =>
    invoke($0, `xor`, [
      i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa]),
      i32x4([0xa, 0x80, 0x5, 0xa5]),
    ]),
  [i32x4([0xaaaaaaa0, 0xaaaaaa2a, 0xaaaaaaaf, 0xaaaaaa0f])],
);

// ./test/core/simd/simd_bitwise.wast:139
assert_return(
  () =>
    invoke($0, `xor`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x55555555, 0x55555555, 0x55555555, 0x55555555]),
    ]),
  [i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa])],
);

// ./test/core/simd/simd_bitwise.wast:142
assert_return(
  () =>
    invoke($0, `xor`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa]),
    ]),
  [i32x4([0x55555555, 0x55555555, 0x55555555, 0x55555555])],
);

// ./test/core/simd/simd_bitwise.wast:145
assert_return(
  () =>
    invoke($0, `xor`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_bitwise.wast:148
assert_return(
  () =>
    invoke($0, `xor`, [
      i32x4([0x55555555, 0x55555555, 0x55555555, 0x55555555]),
      i32x4([0x5555, 0xffff, 0x55ff, 0x5fff]),
    ]),
  [i32x4([0x55550000, 0x5555aaaa, 0x555500aa, 0x55550aaa])],
);

// ./test/core/simd/simd_bitwise.wast:151
assert_return(
  () =>
    invoke($0, `xor`, [
      i32x4([0x499602d2, 0x499602d2, 0x499602d2, 0x499602d2]),
      i32x4([0x499602d2, 0x499602d2, 0x499602d2, 0x499602d2]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_bitwise.wast:154
assert_return(
  () =>
    invoke($0, `xor`, [
      i32x4([0x12345678, 0x12345678, 0x12345678, 0x12345678]),
      i32x4([0x90abcdef, 0x90abcdef, 0x90abcdef, 0x90abcdef]),
    ]),
  [i32x4([0x829f9b97, 0x829f9b97, 0x829f9b97, 0x829f9b97])],
);

// ./test/core/simd/simd_bitwise.wast:157
assert_return(
  () =>
    invoke($0, `bitselect`, [
      i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa]),
      i32x4([0xbbbbbbbb, 0xbbbbbbbb, 0xbbbbbbbb, 0xbbbbbbbb]),
      i32x4([0x112345, 0xf00fffff, 0x10112021, 0xbbaabbaa]),
    ]),
  [i32x4([0xbbaababa, 0xabbaaaaa, 0xabaabbba, 0xaabbaabb])],
);

// ./test/core/simd/simd_bitwise.wast:161
assert_return(
  () =>
    invoke($0, `bitselect`, [
      i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa]),
      i32x4([0xbbbbbbbb, 0xbbbbbbbb, 0xbbbbbbbb, 0xbbbbbbbb]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0xbbbbbbbb, 0xbbbbbbbb, 0xbbbbbbbb, 0xbbbbbbbb])],
);

// ./test/core/simd/simd_bitwise.wast:165
assert_return(
  () =>
    invoke($0, `bitselect`, [
      i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa]),
      i32x4([0xbbbbbbbb, 0xbbbbbbbb, 0xbbbbbbbb, 0xbbbbbbbb]),
      i32x4([0x11111111, 0x11111111, 0x11111111, 0x11111111]),
    ]),
  [i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa])],
);

// ./test/core/simd/simd_bitwise.wast:169
assert_return(
  () =>
    invoke($0, `bitselect`, [
      i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa]),
      i32x4([0xbbbbbbbb, 0xbbbbbbbb, 0xbbbbbbbb, 0xbbbbbbbb]),
      i32x4([0x1234567, 0x89abcdef, 0xfedcba98, 0x76543210]),
    ]),
  [i32x4([0xbabababa, 0xbabababa, 0xabababab, 0xabababab])],
);

// ./test/core/simd/simd_bitwise.wast:173
assert_return(
  () =>
    invoke($0, `bitselect`, [
      i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa]),
      i32x4([0x55555555, 0x55555555, 0x55555555, 0x55555555]),
      i32x4([0x1234567, 0x89abcdef, 0xfedcba98, 0x76543210]),
    ]),
  [i32x4([0x54761032, 0xdcfe98ba, 0xab89efcd, 0x23016745])],
);

// ./test/core/simd/simd_bitwise.wast:177
assert_return(
  () =>
    invoke($0, `bitselect`, [
      i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa]),
      i32x4([0x55555555, 0x55555555, 0x55555555, 0x55555555]),
      i32x4([0x55555555, 0xaaaaaaaa, 0x0, 0xffffffff]),
    ]),
  [i32x4([0x0, 0xffffffff, 0x55555555, 0xaaaaaaaa])],
);

// ./test/core/simd/simd_bitwise.wast:181
assert_return(
  () =>
    invoke($0, `bitselect`, [
      i32x4([0x499602d2, 0x499602d2, 0x499602d2, 0x499602d2]),
      i32x4([0xb669fd2e, 0xb669fd2e, 0xb669fd2e, 0xb669fd2e]),
      i32x4([0xcdefcdef, 0xcdefcdef, 0xcdefcdef, 0xcdefcdef]),
    ]),
  [i32x4([0x7b8630c2, 0x7b8630c2, 0x7b8630c2, 0x7b8630c2])],
);

// ./test/core/simd/simd_bitwise.wast:185
assert_return(
  () =>
    invoke($0, `bitselect`, [
      i32x4([0x12345678, 0x12345678, 0x12345678, 0x12345678]),
      i32x4([0x90abcdef, 0x90abcdef, 0x90abcdef, 0x90abcdef]),
      i32x4([0xcdefcdef, 0xcdefcdef, 0xcdefcdef, 0xcdefcdef]),
    ]),
  [i32x4([0x10244468, 0x10244468, 0x10244468, 0x10244468])],
);

// ./test/core/simd/simd_bitwise.wast:189
assert_return(
  () =>
    invoke($0, `andnot`, [
      i32x4([0x0, 0x0, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0xffffffff, 0x0, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0xffffffff, 0x0])],
);

// ./test/core/simd/simd_bitwise.wast:192
assert_return(
  () =>
    invoke($0, `andnot`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_bitwise.wast:195
assert_return(
  () =>
    invoke($0, `andnot`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_bitwise.wast:198
assert_return(
  () =>
    invoke($0, `andnot`, [
      i32x4([0x0, 0x0, 0x0, 0x0]),
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_bitwise.wast:201
assert_return(
  () =>
    invoke($0, `andnot`, [
      i32x4([0x1, 0x1, 0x1, 0x1]),
      i32x4([0x1, 0x1, 0x1, 0x1]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_bitwise.wast:204
assert_return(
  () =>
    invoke($0, `andnot`, [
      i32x4([0xff, 0xff, 0xff, 0xff]),
      i32x4([0x55, 0x55, 0x55, 0x55]),
    ]),
  [i32x4([0xaa, 0xaa, 0xaa, 0xaa])],
);

// ./test/core/simd/simd_bitwise.wast:207
assert_return(
  () =>
    invoke($0, `andnot`, [
      i32x4([0xff, 0xff, 0xff, 0xff]),
      i32x4([0x80, 0x80, 0x80, 0x80]),
    ]),
  [i32x4([0x7f, 0x7f, 0x7f, 0x7f])],
);

// ./test/core/simd/simd_bitwise.wast:210
assert_return(
  () =>
    invoke($0, `andnot`, [
      i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa]),
      i32x4([0xa, 0x80, 0x5, 0xa5]),
    ]),
  [i32x4([0xaaaaaaa0, 0xaaaaaa2a, 0xaaaaaaaa, 0xaaaaaa0a])],
);

// ./test/core/simd/simd_bitwise.wast:213
assert_return(
  () =>
    invoke($0, `andnot`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x55555555, 0x55555555, 0x55555555, 0x55555555]),
    ]),
  [i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa])],
);

// ./test/core/simd/simd_bitwise.wast:216
assert_return(
  () =>
    invoke($0, `andnot`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa]),
    ]),
  [i32x4([0x55555555, 0x55555555, 0x55555555, 0x55555555])],
);

// ./test/core/simd/simd_bitwise.wast:219
assert_return(
  () =>
    invoke($0, `andnot`, [
      i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff]),
      i32x4([0x0, 0x0, 0x0, 0x0]),
    ]),
  [i32x4([0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff])],
);

// ./test/core/simd/simd_bitwise.wast:222
assert_return(
  () =>
    invoke($0, `andnot`, [
      i32x4([0x55555555, 0x55555555, 0x55555555, 0x55555555]),
      i32x4([0x5555, 0xffff, 0x55ff, 0x5fff]),
    ]),
  [i32x4([0x55550000, 0x55550000, 0x55550000, 0x55550000])],
);

// ./test/core/simd/simd_bitwise.wast:225
assert_return(
  () =>
    invoke($0, `andnot`, [
      i32x4([0x499602d2, 0x499602d2, 0x499602d2, 0x499602d2]),
      i32x4([0x499602d2, 0x499602d2, 0x499602d2, 0x499602d2]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_bitwise.wast:228
assert_return(
  () =>
    invoke($0, `andnot`, [
      i32x4([0x12345678, 0x12345678, 0x12345678, 0x12345678]),
      i32x4([0x90abcdef, 0x90abcdef, 0x90abcdef, 0x90abcdef]),
    ]),
  [i32x4([0x2141210, 0x2141210, 0x2141210, 0x2141210])],
);

// ./test/core/simd/simd_bitwise.wast:233
assert_return(
  () =>
    invoke($0, `not`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0.00000000000000000000000000000000000000587747),
      value("f32", 0.00000000000000000000000000000000000000587747),
      value("f32", 0.00000000000000000000000000000000000000587747),
      value("f32", 0.00000000000000000000000000000000000000587747),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:235
assert_return(
  () =>
    invoke($0, `not`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -0.00000000000000000000000000000000000000587747),
      value("f32", -0.00000000000000000000000000000000000000587747),
      value("f32", -0.00000000000000000000000000000000000000587747),
      value("f32", -0.00000000000000000000000000000000000000587747),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:237
assert_return(
  () =>
    invoke($0, `not`, [f32x4([-Infinity, -Infinity, -Infinity, -Infinity])]),
  [i32x4([0x7fffff, 0x7fffff, 0x7fffff, 0x7fffff])],
);

// ./test/core/simd/simd_bitwise.wast:239
assert_return(
  () => invoke($0, `not`, [f32x4([Infinity, Infinity, Infinity, Infinity])]),
  [i32x4([0x807fffff, 0x807fffff, 0x807fffff, 0x807fffff])],
);

// ./test/core/simd/simd_bitwise.wast:241
assert_return(
  () =>
    invoke($0, `and`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
    ]),
  [i32x4([0xffc00000, 0xffc00000, 0xffc00000, 0xffc00000])],
);

// ./test/core/simd/simd_bitwise.wast:244
assert_return(
  () =>
    invoke($0, `and`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
    ]),
  [
    new F32x4Pattern(
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:247
assert_return(
  () =>
    invoke($0, `and`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -Infinity),
      value("f32", -Infinity),
      value("f32", -Infinity),
      value("f32", -Infinity),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:250
assert_return(
  () =>
    invoke($0, `and`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:253
assert_return(
  () =>
    invoke($0, `and`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
    ]),
  [
    new F32x4Pattern(
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:256
assert_return(
  () =>
    invoke($0, `and`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:259
assert_return(
  () =>
    invoke($0, `and`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:262
assert_return(
  () =>
    invoke($0, `and`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -Infinity),
      value("f32", -Infinity),
      value("f32", -Infinity),
      value("f32", -Infinity),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:265
assert_return(
  () =>
    invoke($0, `and`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:268
assert_return(
  () =>
    invoke($0, `and`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:271
assert_return(
  () =>
    invoke($0, `or`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
    ]),
  [i32x4([0xffc00000, 0xffc00000, 0xffc00000, 0xffc00000])],
);

// ./test/core/simd/simd_bitwise.wast:274
assert_return(
  () =>
    invoke($0, `or`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
    ]),
  [i32x4([0xffc00000, 0xffc00000, 0xffc00000, 0xffc00000])],
);

// ./test/core/simd/simd_bitwise.wast:277
assert_return(
  () =>
    invoke($0, `or`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [i32x4([0xffc00000, 0xffc00000, 0xffc00000, 0xffc00000])],
);

// ./test/core/simd/simd_bitwise.wast:280
assert_return(
  () =>
    invoke($0, `or`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [i32x4([0xffc00000, 0xffc00000, 0xffc00000, 0xffc00000])],
);

// ./test/core/simd/simd_bitwise.wast:283
assert_return(
  () =>
    invoke($0, `or`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
    ]),
  [
    new F32x4Pattern(
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:286
assert_return(
  () =>
    invoke($0, `or`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [i32x4([0xffc00000, 0xffc00000, 0xffc00000, 0xffc00000])],
);

// ./test/core/simd/simd_bitwise.wast:289
assert_return(
  () =>
    invoke($0, `or`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [
    new F32x4Pattern(
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:292
assert_return(
  () =>
    invoke($0, `or`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -Infinity),
      value("f32", -Infinity),
      value("f32", -Infinity),
      value("f32", -Infinity),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:295
assert_return(
  () =>
    invoke($0, `or`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -Infinity),
      value("f32", -Infinity),
      value("f32", -Infinity),
      value("f32", -Infinity),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:298
assert_return(
  () =>
    invoke($0, `or`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:301
assert_return(
  () =>
    invoke($0, `xor`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:304
assert_return(
  () =>
    invoke($0, `xor`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
    ]),
  [
    new F32x4Pattern(
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:307
assert_return(
  () =>
    invoke($0, `xor`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [i32x4([0x400000, 0x400000, 0x400000, 0x400000])],
);

// ./test/core/simd/simd_bitwise.wast:310
assert_return(
  () =>
    invoke($0, `xor`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [i32x4([0x80400000, 0x80400000, 0x80400000, 0x80400000])],
);

// ./test/core/simd/simd_bitwise.wast:313
assert_return(
  () =>
    invoke($0, `xor`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:316
assert_return(
  () =>
    invoke($0, `xor`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [i32x4([0x80400000, 0x80400000, 0x80400000, 0x80400000])],
);

// ./test/core/simd/simd_bitwise.wast:319
assert_return(
  () =>
    invoke($0, `xor`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [i32x4([0x400000, 0x400000, 0x400000, 0x400000])],
);

// ./test/core/simd/simd_bitwise.wast:322
assert_return(
  () =>
    invoke($0, `xor`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:325
assert_return(
  () =>
    invoke($0, `xor`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_bitwise.wast:328
assert_return(
  () =>
    invoke($0, `xor`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:331
assert_return(
  () =>
    invoke($0, `bitselect`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
      f32x4([2779096600, 2779096600, 2779096600, 2779096600]),
    ]),
  [i32x4([0xffc00000, 0xffc00000, 0xffc00000, 0xffc00000])],
);

// ./test/core/simd/simd_bitwise.wast:335
assert_return(
  () =>
    invoke($0, `bitselect`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
      f32x4([2779096600, 2779096600, 2779096600, 2779096600]),
    ]),
  [
    new F32x4Pattern(
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:339
assert_return(
  () =>
    invoke($0, `bitselect`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([2779096600, 2779096600, 2779096600, 2779096600]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -Infinity),
      value("f32", -Infinity),
      value("f32", -Infinity),
      value("f32", -Infinity),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:343
assert_return(
  () =>
    invoke($0, `bitselect`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([2779096600, 2779096600, 2779096600, 2779096600]),
    ]),
  [
    new F32x4Pattern(
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:347
assert_return(
  () =>
    invoke($0, `bitselect`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
      f32x4([2779096600, 2779096600, 2779096600, 2779096600]),
    ]),
  [
    new F32x4Pattern(
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
      bytes("f32", [0x0, 0x0, 0xc0, 0x7f]),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:351
assert_return(
  () =>
    invoke($0, `bitselect`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([2779096600, 2779096600, 2779096600, 2779096600]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -Infinity),
      value("f32", -Infinity),
      value("f32", -Infinity),
      value("f32", -Infinity),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:355
assert_return(
  () =>
    invoke($0, `bitselect`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([2779096600, 2779096600, 2779096600, 2779096600]),
    ]),
  [
    new F32x4Pattern(
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:359
assert_return(
  () =>
    invoke($0, `bitselect`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([2779096600, 2779096600, 2779096600, 2779096600]),
    ]),
  [
    new F32x4Pattern(
      value("f32", -Infinity),
      value("f32", -Infinity),
      value("f32", -Infinity),
      value("f32", -Infinity),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:363
assert_return(
  () =>
    invoke($0, `bitselect`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([2779096600, 2779096600, 2779096600, 2779096600]),
    ]),
  [
    new F32x4Pattern(
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:367
assert_return(
  () =>
    invoke($0, `bitselect`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([2779096600, 2779096600, 2779096600, 2779096600]),
    ]),
  [
    new F32x4Pattern(
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
      value("f32", Infinity),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:371
assert_return(
  () =>
    invoke($0, `andnot`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_bitwise.wast:374
assert_return(
  () =>
    invoke($0, `andnot`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
    ]),
  [
    new F32x4Pattern(
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
      bytes("f32", [0x0, 0x0, 0x0, 0x80]),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:377
assert_return(
  () =>
    invoke($0, `andnot`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [i32x4([0x400000, 0x400000, 0x400000, 0x400000])],
);

// ./test/core/simd/simd_bitwise.wast:380
assert_return(
  () =>
    invoke($0, `andnot`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
        0x0,
        0x0,
        0xc0,
        0xff,
      ]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [i32x4([0x80400000, 0x80400000, 0x80400000, 0x80400000])],
);

// ./test/core/simd/simd_bitwise.wast:383
assert_return(
  () =>
    invoke($0, `andnot`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:386
assert_return(
  () =>
    invoke($0, `andnot`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [i32x4([0x400000, 0x400000, 0x400000, 0x400000])],
);

// ./test/core/simd/simd_bitwise.wast:389
assert_return(
  () =>
    invoke($0, `andnot`, [
      bytes("v128", [
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
        0x0,
        0x0,
        0xc0,
        0x7f,
      ]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [i32x4([0x400000, 0x400000, 0x400000, 0x400000])],
);

// ./test/core/simd/simd_bitwise.wast:392
assert_return(
  () =>
    invoke($0, `andnot`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
    ]),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
      value("f32", 0),
    ),
  ],
);

// ./test/core/simd/simd_bitwise.wast:395
assert_return(
  () =>
    invoke($0, `andnot`, [
      f32x4([-Infinity, -Infinity, -Infinity, -Infinity]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [i32x4([0x80000000, 0x80000000, 0x80000000, 0x80000000])],
);

// ./test/core/simd/simd_bitwise.wast:398
assert_return(
  () =>
    invoke($0, `andnot`, [
      f32x4([Infinity, Infinity, Infinity, Infinity]),
      f32x4([Infinity, Infinity, Infinity, Infinity]),
    ]),
  [i32x4([0x0, 0x0, 0x0, 0x0])],
);

// ./test/core/simd/simd_bitwise.wast:405
assert_invalid(
  () => instantiate(`(module (func (result v128) (v128.not (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/simd/simd_bitwise.wast:407
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (v128.and (i32.const 0) (v128.const i32x4 0 0 0 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_bitwise.wast:408
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (v128.and (v128.const i32x4 0 0 0 0) (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_bitwise.wast:409
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (v128.and (i32.const 0) (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_bitwise.wast:411
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (v128.or (i32.const 0) (v128.const i32x4 0 0 0 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_bitwise.wast:412
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (v128.or (v128.const i32x4 0 0 0 0) (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_bitwise.wast:413
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (v128.or (i32.const 0) (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_bitwise.wast:415
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (v128.xor (i32.const 0) (v128.const i32x4 0 0 0 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_bitwise.wast:416
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (v128.xor (v128.const i32x4 0 0 0 0) (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_bitwise.wast:417
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (v128.xor (i32.const 0) (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_bitwise.wast:419
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (v128.bitselect (i32.const 0) (v128.const i32x4 0 0 0 0) (v128.const i32x4 0 0 0 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_bitwise.wast:420
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (v128.bitselect (v128.const i32x4 0 0 0 0) (v128.const i32x4 0 0 0 0) (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_bitwise.wast:421
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (v128.bitselect (i32.const 0) (i32.const 0) (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_bitwise.wast:423
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (v128.andnot (i32.const 0) (v128.const i32x4 0 0 0 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_bitwise.wast:424
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (v128.andnot (v128.const i32x4 0 0 0 0) (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_bitwise.wast:425
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (v128.andnot (i32.const 0) (i32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_bitwise.wast:429
let $1 = instantiate(`(module (memory 1)
  (func (export "v128.not-in-block")
    (block
      (drop
        (block (result v128)
          (v128.not
            (block (result v128) (v128.load (i32.const 0)))
          )
        )
      )
    )
  )
  (func (export "v128.and-in-block")
    (block
      (drop
        (block (result v128)
          (v128.and
            (block (result v128) (v128.load (i32.const 0)))
            (block (result v128) (v128.load (i32.const 1)))
          )
        )
      )
    )
  )
  (func (export "v128.or-in-block")
    (block
      (drop
        (block (result v128)
          (v128.or
            (block (result v128) (v128.load (i32.const 0)))
            (block (result v128) (v128.load (i32.const 1)))
          )
        )
      )
    )
  )
  (func (export "v128.xor-in-block")
    (block
      (drop
        (block (result v128)
          (v128.xor
            (block (result v128) (v128.load (i32.const 0)))
            (block (result v128) (v128.load (i32.const 1)))
          )
        )
      )
    )
  )
  (func (export "v128.bitselect-in-block")
    (block
      (drop
        (block (result v128)
          (v128.bitselect
            (block (result v128) (v128.load (i32.const 0)))
            (block (result v128) (v128.load (i32.const 1)))
            (block (result v128) (v128.load (i32.const 2)))
          )
        )
      )
    )
  )
  (func (export "v128.andnot-in-block")
    (block
      (drop
        (block (result v128)
          (v128.andnot
            (block (result v128) (v128.load (i32.const 0)))
            (block (result v128) (v128.load (i32.const 1)))
          )
        )
      )
    )
  )
  (func (export "nested-v128.not")
    (drop
      (v128.not
        (v128.not
          (v128.not
            (v128.load (i32.const 0))
          )
        )
      )
    )
  )
  (func (export "nested-v128.and")
    (drop
      (v128.and
        (v128.and
          (v128.and
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
          (v128.and
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
        )
        (v128.and
          (v128.and
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
          (v128.and
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
        )
      )
    )
  )
  (func (export "nested-v128.or")
    (drop
      (v128.or
        (v128.or
          (v128.or
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
          (v128.or
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
        )
        (v128.or
          (v128.or
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
          (v128.or
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
        )
      )
    )
  )
  (func (export "nested-v128.xor")
    (drop
      (v128.xor
        (v128.xor
          (v128.xor
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
          (v128.xor
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
        )
        (v128.xor
          (v128.xor
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
          (v128.xor
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
        )
      )
    )
  )
  (func (export "nested-v128.bitselect")
    (drop
      (v128.bitselect
        (v128.bitselect
          (v128.bitselect
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
            (v128.load (i32.const 2))
          )
          (v128.bitselect
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
            (v128.load (i32.const 2))
          )
          (v128.bitselect
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
            (v128.load (i32.const 2))
          )
        )
        (v128.bitselect
          (v128.bitselect
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
            (v128.load (i32.const 2))
          )
          (v128.bitselect
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
            (v128.load (i32.const 2))
          )
          (v128.bitselect
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
            (v128.load (i32.const 2))
          )
        )
        (v128.bitselect
          (v128.bitselect
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
            (v128.load (i32.const 2))
          )
          (v128.bitselect
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
            (v128.load (i32.const 2))
          )
          (v128.bitselect
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
            (v128.load (i32.const 2))
          )
        )
      )
    )
  )
  (func (export "nested-v128.andnot")
    (drop
      (v128.andnot
        (v128.andnot
          (v128.andnot
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
          (v128.andnot
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
        )
        (v128.andnot
          (v128.andnot
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
          (v128.andnot
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
        )
      )
    )
  )
  (func (export "as-param")
    (drop
      (v128.or
        (v128.and
          (v128.not
            (v128.load (i32.const 0))
          )
          (v128.not
            (v128.load (i32.const 1))
          )
        )
        (v128.xor
          (v128.bitselect
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
            (v128.load (i32.const 2))
          )
          (v128.andnot
            (v128.load (i32.const 0))
            (v128.load (i32.const 1))
          )
        )
      )
    )
  )
)`);

// ./test/core/simd/simd_bitwise.wast:700
assert_return(() => invoke($1, `v128.not-in-block`, []), []);

// ./test/core/simd/simd_bitwise.wast:701
assert_return(() => invoke($1, `v128.and-in-block`, []), []);

// ./test/core/simd/simd_bitwise.wast:702
assert_return(() => invoke($1, `v128.or-in-block`, []), []);

// ./test/core/simd/simd_bitwise.wast:703
assert_return(() => invoke($1, `v128.xor-in-block`, []), []);

// ./test/core/simd/simd_bitwise.wast:704
assert_return(() => invoke($1, `v128.bitselect-in-block`, []), []);

// ./test/core/simd/simd_bitwise.wast:705
assert_return(() => invoke($1, `v128.andnot-in-block`, []), []);

// ./test/core/simd/simd_bitwise.wast:706
assert_return(() => invoke($1, `nested-v128.not`, []), []);

// ./test/core/simd/simd_bitwise.wast:707
assert_return(() => invoke($1, `nested-v128.and`, []), []);

// ./test/core/simd/simd_bitwise.wast:708
assert_return(() => invoke($1, `nested-v128.or`, []), []);

// ./test/core/simd/simd_bitwise.wast:709
assert_return(() => invoke($1, `nested-v128.xor`, []), []);

// ./test/core/simd/simd_bitwise.wast:710
assert_return(() => invoke($1, `nested-v128.bitselect`, []), []);

// ./test/core/simd/simd_bitwise.wast:711
assert_return(() => invoke($1, `nested-v128.andnot`, []), []);

// ./test/core/simd/simd_bitwise.wast:712
assert_return(() => invoke($1, `as-param`, []), []);

// ./test/core/simd/simd_bitwise.wast:717
assert_invalid(() =>
  instantiate(`(module
    (func $$v128.not-arg-empty (result v128)
      (v128.not)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_bitwise.wast:725
assert_invalid(() =>
  instantiate(`(module
    (func $$v128.and-1st-arg-empty (result v128)
      (v128.and (v128.const i32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_bitwise.wast:733
assert_invalid(() =>
  instantiate(`(module
    (func $$v128.and-arg-empty (result v128)
      (v128.and)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_bitwise.wast:741
assert_invalid(() =>
  instantiate(`(module
    (func $$v128.or-1st-arg-empty (result v128)
      (v128.or (v128.const i32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_bitwise.wast:749
assert_invalid(() =>
  instantiate(`(module
    (func $$v128.or-arg-empty (result v128)
      (v128.or)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_bitwise.wast:757
assert_invalid(() =>
  instantiate(`(module
    (func $$v128.xor-1st-arg-empty (result v128)
      (v128.xor (v128.const i32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_bitwise.wast:765
assert_invalid(() =>
  instantiate(`(module
    (func $$v128.xor-arg-empty (result v128)
      (v128.xor)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_bitwise.wast:773
assert_invalid(() =>
  instantiate(`(module
    (func $$v128.andnot-1st-arg-empty (result v128)
      (v128.andnot (v128.const i32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_bitwise.wast:781
assert_invalid(() =>
  instantiate(`(module
    (func $$v128.andnot-arg-empty (result v128)
      (v128.andnot)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_bitwise.wast:789
assert_invalid(() =>
  instantiate(`(module
    (func $$v128.bitselect-1st-arg-empty (result v128)
      (v128.bitselect (v128.const i32x4 0 0 0 0) (v128.const i32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_bitwise.wast:797
assert_invalid(() =>
  instantiate(`(module
    (func $$v128.bitselect-two-args-empty (result v128)
      (v128.bitselect (v128.const i32x4 0 0 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_bitwise.wast:805
assert_invalid(() =>
  instantiate(`(module
    (func $$v128.bitselect-arg-empty (result v128)
      (v128.bitselect)
    )
  )`), `type mismatch`);
