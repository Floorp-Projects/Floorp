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

// ./test/core/simd/simd_i64x2_cmp.wast

// ./test/core/simd/simd_i64x2_cmp.wast:4
let $0 = instantiate(`(module
  (func (export "eq") (param $$x v128) (param $$y v128) (result v128) (i64x2.eq (local.get $$x) (local.get $$y)))
  (func (export "ne") (param $$x v128) (param $$y v128) (result v128) (i64x2.ne (local.get $$x) (local.get $$y)))
  (func (export "lt_s") (param $$x v128) (param $$y v128) (result v128) (i64x2.lt_s (local.get $$x) (local.get $$y)))
  (func (export "le_s") (param $$x v128) (param $$y v128) (result v128) (i64x2.le_s (local.get $$x) (local.get $$y)))
  (func (export "gt_s") (param $$x v128) (param $$y v128) (result v128) (i64x2.gt_s (local.get $$x) (local.get $$y)))
  (func (export "ge_s") (param $$x v128) (param $$y v128) (result v128) (i64x2.ge_s (local.get $$x) (local.get $$y)))
)`);

// ./test/core/simd/simd_i64x2_cmp.wast:17
assert_return(
  () =>
    invoke($0, `eq`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:20
assert_return(
  () => invoke($0, `eq`, [i64x2([0x0n, 0x0n]), i64x2([0x0n, 0x0n])]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:23
assert_return(
  () =>
    invoke($0, `eq`, [
      i64x2([0xf0f0f0f0f0f0f0f0n, 0xf0f0f0f0f0f0f0f0n]),
      i64x2([0xf0f0f0f0f0f0f0f0n, 0xf0f0f0f0f0f0f0f0n]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:26
assert_return(
  () =>
    invoke($0, `eq`, [
      i64x2([0xf0f0f0f0f0f0f0fn, 0xf0f0f0f0f0f0f0fn]),
      i64x2([0xf0f0f0f0f0f0f0fn, 0xf0f0f0f0f0f0f0fn]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:29
assert_return(
  () =>
    invoke($0, `eq`, [
      i64x2([0xffffffffffffffffn, 0x0n]),
      i64x2([0xffffffffffffffffn, 0x0n]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:32
assert_return(
  () =>
    invoke($0, `eq`, [
      i64x2([0x0n, 0xffffffffffffffffn]),
      i64x2([0x0n, 0xffffffffffffffffn]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:35
assert_return(
  () =>
    invoke($0, `eq`, [
      i64x2([0x3020100n, 0x11100904n]),
      i64x2([0x3020100n, 0x11100904n]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:38
assert_return(
  () =>
    invoke($0, `eq`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xfffffffffffffffn, 0xfffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:41
assert_return(
  () => invoke($0, `eq`, [i64x2([0x1n, 0x1n]), i64x2([0x2n, 0x2n])]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:50
assert_return(
  () =>
    invoke($0, `ne`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:53
assert_return(
  () => invoke($0, `ne`, [i64x2([0x0n, 0x0n]), i64x2([0x0n, 0x0n])]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:56
assert_return(
  () =>
    invoke($0, `ne`, [
      i64x2([0xf0f0f0f0f0f0f0f0n, 0xf0f0f0f0f0f0f0f0n]),
      i64x2([0xf0f0f0f0f0f0f0f0n, 0xf0f0f0f0f0f0f0f0n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:59
assert_return(
  () =>
    invoke($0, `ne`, [
      i64x2([0xf0f0f0f0f0f0f0fn, 0xf0f0f0f0f0f0f0fn]),
      i64x2([0xf0f0f0f0f0f0f0fn, 0xf0f0f0f0f0f0f0fn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:62
assert_return(
  () =>
    invoke($0, `ne`, [
      i64x2([0xffffffffffffffffn, 0x0n]),
      i64x2([0xffffffffffffffffn, 0x0n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:65
assert_return(
  () =>
    invoke($0, `ne`, [
      i64x2([0x0n, 0xffffffffffffffffn]),
      i64x2([0x0n, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:68
assert_return(
  () =>
    invoke($0, `ne`, [
      i64x2([0x3020100n, 0x11100904n]),
      i64x2([0x3020100n, 0x11100904n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:77
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:80
assert_return(
  () => invoke($0, `lt_s`, [i64x2([0x0n, 0x0n]), i64x2([0x0n, 0x0n])]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:83
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i64x2([0xf0f0f0f0f0f0f0f0n, 0xf0f0f0f0f0f0f0f0n]),
      i64x2([0xf0f0f0f0f0f0f0f0n, 0xf0f0f0f0f0f0f0f0n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:86
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i64x2([0xf0f0f0f0f0f0f0fn, 0xf0f0f0f0f0f0f0fn]),
      i64x2([0xf0f0f0f0f0f0f0fn, 0xf0f0f0f0f0f0f0fn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:89
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i64x2([0xffffffffffffffffn, 0x0n]),
      i64x2([0xffffffffffffffffn, 0x0n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:92
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i64x2([0x0n, 0xffffffffffffffffn]),
      i64x2([0x0n, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:95
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i64x2([0x302010011100904n, 0x1a0b0a12ffabaa1bn]),
      i64x2([0x302010011100904n, 0x1a0b0a12ffabaa1bn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:100
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:103
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:106
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i64x2([0x8080808080808080n, 0x8080808080808080n]),
      i64x2([0x8080808080808080n, 0x8080808080808080n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:109
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i64x2([0x8080808080808080n, 0x8080808080808080n]),
      i64x2([0x8080808080808080n, 0x8080808080808080n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:112
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i64x2([0x8382818000fffefdn, 0x7f020100fffefd80n]),
      i64x2([0x8382818000fffefdn, 0x7f020100fffefd80n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:117
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:120
assert_return(
  () => invoke($0, `lt_s`, [i64x2([0x0n, 0x0n]), i64x2([0x0n, 0x0n])]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:123
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:126
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:129
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i64x2([0xffffffffffffffffn, 0x0n]),
      i64x2([0xffffffffffffffffn, 0x0n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:132
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i64x2([0x0n, 0xffffffffffffffffn]),
      i64x2([0x0n, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:135
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i64x2([0x8000000000000001n, 0xffffffffffffffffn]),
      i64x2([0x8000000000000001n, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:140
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i64x2([0xc060000000000000n, 0xc05fc00000000000n]),
      f64x2([-128, -127]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:143
assert_return(
  () =>
    invoke($0, `lt_s`, [
      i64x2([0x3ff0000000000000n, 0x405fc00000000000n]),
      f64x2([1, 127]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:152
assert_return(
  () =>
    invoke($0, `le_s`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:155
assert_return(
  () => invoke($0, `le_s`, [i64x2([0x0n, 0x0n]), i64x2([0x0n, 0x0n])]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:158
assert_return(
  () =>
    invoke($0, `le_s`, [
      i64x2([0xf0f0f0f0f0f0f0f0n, 0xf0f0f0f0f0f0f0f0n]),
      i64x2([0xf0f0f0f0f0f0f0f0n, 0xf0f0f0f0f0f0f0f0n]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:161
assert_return(
  () =>
    invoke($0, `le_s`, [
      i64x2([0xf0f0f0f0f0f0f0fn, 0xf0f0f0f0f0f0f0fn]),
      i64x2([0xf0f0f0f0f0f0f0fn, 0xf0f0f0f0f0f0f0fn]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:164
assert_return(
  () =>
    invoke($0, `le_s`, [
      i64x2([0xffffffffffffffffn, 0x0n]),
      i64x2([0xffffffffffffffffn, 0x0n]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:167
assert_return(
  () =>
    invoke($0, `le_s`, [
      i64x2([0x0n, 0xffffffffffffffffn]),
      i64x2([0x0n, 0xffffffffffffffffn]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:170
assert_return(
  () =>
    invoke($0, `le_s`, [
      i64x2([0x302010011100904n, 0x1a0b0a12ffabaa1bn]),
      i64x2([0x302010011100904n, 0x1a0b0a12ffabaa1bn]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:175
assert_return(
  () =>
    invoke($0, `le_s`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:178
assert_return(
  () =>
    invoke($0, `le_s`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:181
assert_return(
  () =>
    invoke($0, `le_s`, [
      i64x2([0x8080808080808080n, 0x8080808080808080n]),
      i64x2([0x8080808080808080n, 0x8080808080808080n]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:184
assert_return(
  () =>
    invoke($0, `le_s`, [
      i64x2([0x8080808080808080n, 0x8080808080808080n]),
      i64x2([0x8080808080808080n, 0x8080808080808080n]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:187
assert_return(
  () =>
    invoke($0, `le_s`, [
      i64x2([0x8382818000fffefdn, 0x7f020100fffefd80n]),
      i64x2([0x8382818000fffefdn, 0x7f020100fffefd80n]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:192
assert_return(
  () =>
    invoke($0, `le_s`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:195
assert_return(
  () =>
    invoke($0, `le_s`, [
      i64x2([0x0n, 0x0n]),
      i64x2([0x0n, 0xffffffffffffffffn]),
    ]),
  [i64x2([0xffffffffffffffffn, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:198
assert_return(
  () => invoke($0, `le_s`, [i64x2([0x0n, 0x0n]), i64x2([0x0n, 0x0n])]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:201
assert_return(
  () =>
    invoke($0, `le_s`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:204
assert_return(
  () =>
    invoke($0, `le_s`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:207
assert_return(
  () =>
    invoke($0, `le_s`, [
      i64x2([0xffffffffffffffffn, 0x0n]),
      i64x2([0xffffffffffffffffn, 0x0n]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:210
assert_return(
  () =>
    invoke($0, `le_s`, [
      i64x2([0x0n, 0xffffffffffffffffn]),
      i64x2([0x0n, 0xffffffffffffffffn]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:213
assert_return(
  () =>
    invoke($0, `le_s`, [
      i64x2([0x8000000000000001n, 0xffffffffffffffffn]),
      i64x2([0x8000000000000001n, 0xffffffffffffffffn]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:218
assert_return(
  () =>
    invoke($0, `le_s`, [
      i64x2([0xc060000000000000n, 0xc05fc00000000000n]),
      f64x2([-128, -127]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:221
assert_return(
  () =>
    invoke($0, `le_s`, [
      i64x2([0x3ff0000000000000n, 0x405fc00000000000n]),
      f64x2([1, 127]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:230
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:233
assert_return(
  () => invoke($0, `gt_s`, [i64x2([0x0n, 0x0n]), i64x2([0x0n, 0x0n])]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:236
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i64x2([0xf0f0f0f0f0f0f0f0n, 0xf0f0f0f0f0f0f0f0n]),
      i64x2([0xf0f0f0f0f0f0f0f0n, 0xf0f0f0f0f0f0f0f0n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:239
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i64x2([0xf0f0f0f0f0f0f0fn, 0xf0f0f0f0f0f0f0fn]),
      i64x2([0xf0f0f0f0f0f0f0fn, 0xf0f0f0f0f0f0f0fn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:242
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i64x2([0xffffffffffffffffn, 0x0n]),
      i64x2([0xffffffffffffffffn, 0x0n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:245
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i64x2([0x0n, 0xffffffffffffffffn]),
      i64x2([0x0n, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:248
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i64x2([0x302010011100904n, 0x1a0b0a12ffabaa1bn]),
      i64x2([0x302010011100904n, 0x1a0b0a12ffabaa1bn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:253
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:256
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:259
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i64x2([0x8080808080808080n, 0x8080808080808080n]),
      i64x2([0x8080808080808080n, 0x8080808080808080n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:262
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i64x2([0x8080808080808080n, 0x8080808080808080n]),
      i64x2([0x8080808080808080n, 0x8080808080808080n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:265
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i64x2([0x8382818000fffefdn, 0x7f020100fffefd80n]),
      i64x2([0x8382818000fffefdn, 0x7f020100fffefd80n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:270
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:273
assert_return(
  () => invoke($0, `gt_s`, [i64x2([0x0n, 0x0n]), i64x2([0x0n, 0x0n])]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:276
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:279
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:282
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i64x2([0xffffffffffffffffn, 0x0n]),
      i64x2([0xffffffffffffffffn, 0x0n]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:285
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i64x2([0x0n, 0xffffffffffffffffn]),
      i64x2([0x0n, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:288
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i64x2([0x8000000000000001n, 0xffffffffffffffffn]),
      i64x2([0x8000000000000001n, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:293
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i64x2([0xc060000000000000n, 0xc05fc00000000000n]),
      f64x2([-128, -127]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:296
assert_return(
  () =>
    invoke($0, `gt_s`, [
      i64x2([0x3ff0000000000000n, 0x405fc00000000000n]),
      f64x2([1, 127]),
    ]),
  [i64x2([0x0n, 0x0n])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:305
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:308
assert_return(
  () => invoke($0, `ge_s`, [i64x2([0x0n, 0x0n]), i64x2([0x0n, 0x0n])]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:311
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i64x2([0xf0f0f0f0f0f0f0f0n, 0xf0f0f0f0f0f0f0f0n]),
      i64x2([0xf0f0f0f0f0f0f0f0n, 0xf0f0f0f0f0f0f0f0n]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:314
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i64x2([0xf0f0f0f0f0f0f0fn, 0xf0f0f0f0f0f0f0fn]),
      i64x2([0xf0f0f0f0f0f0f0fn, 0xf0f0f0f0f0f0f0fn]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:317
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i64x2([0xffffffffffffffffn, 0x0n]),
      i64x2([0xffffffffffffffffn, 0x0n]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:320
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i64x2([0x0n, 0xffffffffffffffffn]),
      i64x2([0x0n, 0xffffffffffffffffn]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:323
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i64x2([0x302010011100904n, 0x1a0b0a12ffabaa1bn]),
      i64x2([0x302010011100904n, 0x1a0b0a12ffabaa1bn]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:328
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:331
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:334
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i64x2([0x8080808080808080n, 0x8080808080808080n]),
      i64x2([0x8080808080808080n, 0x8080808080808080n]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:337
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i64x2([0x8080808080808080n, 0x8080808080808080n]),
      i64x2([0x8080808080808080n, 0x8080808080808080n]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:340
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i64x2([0x8382818000fffefdn, 0x7f020100fffefd80n]),
      i64x2([0x8382818000fffefdn, 0x7f020100fffefd80n]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:345
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:348
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0x0n, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x0n, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:351
assert_return(
  () => invoke($0, `ge_s`, [i64x2([0x0n, 0x0n]), i64x2([0x0n, 0x0n])]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:354
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:357
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:360
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i64x2([0xffffffffffffffffn, 0x0n]),
      i64x2([0xffffffffffffffffn, 0x0n]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:363
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i64x2([0x0n, 0xffffffffffffffffn]),
      i64x2([0x0n, 0xffffffffffffffffn]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:366
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i64x2([0x8000000000000001n, 0xffffffffffffffffn]),
      i64x2([0x8000000000000001n, 0xffffffffffffffffn]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:371
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i64x2([0xc060000000000000n, 0xc05fc00000000000n]),
      f64x2([-128, -127]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:374
assert_return(
  () =>
    invoke($0, `ge_s`, [
      i64x2([0x3ff0000000000000n, 0x405fc00000000000n]),
      f64x2([1, 127]),
    ]),
  [i64x2([0xffffffffffffffffn, 0xffffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_cmp.wast:380
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i64x2.eq (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i64x2_cmp.wast:381
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i64x2.ne (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i64x2_cmp.wast:382
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i64x2.ge_s (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i64x2_cmp.wast:383
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i64x2.gt_s (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i64x2_cmp.wast:384
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i64x2.le_s (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i64x2_cmp.wast:385
assert_invalid(
  () =>
    instantiate(
      `(module (func (result v128) (i64x2.lt_s (i32.const 0) (f32.const 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/simd/simd_i64x2_cmp.wast:389
assert_invalid(() =>
  instantiate(`(module
    (func $$i64x2.eq-1st-arg-empty (result v128)
      (i64x2.eq (v128.const i64x2 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i64x2_cmp.wast:397
assert_invalid(() =>
  instantiate(`(module
    (func $$i64x2.eq-arg-empty (result v128)
      (i64x2.eq)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i64x2_cmp.wast:405
assert_invalid(() =>
  instantiate(`(module
    (func $$i64x2.ne-1st-arg-empty (result v128)
      (i64x2.ne (v128.const i64x2 0 0))
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i64x2_cmp.wast:413
assert_invalid(() =>
  instantiate(`(module
    (func $$i64x2.ne-arg-empty (result v128)
      (i64x2.ne)
    )
  )`), `type mismatch`);
