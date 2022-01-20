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

// ./test/core/simd/simd_i64x2_arith2.wast

// ./test/core/simd/simd_i64x2_arith2.wast:3
let $0 = instantiate(`(module
  (func (export "i64x2.abs") (param v128) (result v128) (i64x2.abs (local.get 0)))
  (func (export "i64x2.abs_with_const_0") (result v128) (i64x2.abs (v128.const i64x2 -9223372036854775808 9223372036854775807)))
)`);

// ./test/core/simd/simd_i64x2_arith2.wast:8
assert_return(() => invoke($0, `i64x2.abs`, [i64x2([0x1n, 0x1n])]), [
  i64x2([0x1n, 0x1n]),
]);

// ./test/core/simd/simd_i64x2_arith2.wast:10
assert_return(
  () =>
    invoke($0, `i64x2.abs`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_arith2.wast:12
assert_return(
  () =>
    invoke($0, `i64x2.abs`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_arith2.wast:14
assert_return(
  () =>
    invoke($0, `i64x2.abs`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x1n, 0x1n])],
);

// ./test/core/simd/simd_i64x2_arith2.wast:16
assert_return(
  () =>
    invoke($0, `i64x2.abs`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
    ]),
  [i64x2([0x8000000000000000n, 0x8000000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith2.wast:18
assert_return(
  () =>
    invoke($0, `i64x2.abs`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
    ]),
  [i64x2([0x8000000000000000n, 0x8000000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith2.wast:20
assert_return(
  () =>
    invoke($0, `i64x2.abs`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
    ]),
  [i64x2([0x8000000000000000n, 0x8000000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith2.wast:22
assert_return(
  () =>
    invoke($0, `i64x2.abs`, [
      i64x2([0x8000000000000000n, 0x8000000000000000n]),
    ]),
  [i64x2([0x8000000000000000n, 0x8000000000000000n])],
);

// ./test/core/simd/simd_i64x2_arith2.wast:24
assert_return(() => invoke($0, `i64x2.abs`, [i64x2([0x7bn, 0x7bn])]), [
  i64x2([0x7bn, 0x7bn]),
]);

// ./test/core/simd/simd_i64x2_arith2.wast:26
assert_return(
  () =>
    invoke($0, `i64x2.abs`, [
      i64x2([0xffffffffffffff85n, 0xffffffffffffff85n]),
    ]),
  [i64x2([0x7bn, 0x7bn])],
);

// ./test/core/simd/simd_i64x2_arith2.wast:28
assert_return(() => invoke($0, `i64x2.abs`, [i64x2([0x80n, 0x80n])]), [
  i64x2([0x80n, 0x80n]),
]);

// ./test/core/simd/simd_i64x2_arith2.wast:30
assert_return(
  () =>
    invoke($0, `i64x2.abs`, [
      i64x2([0xffffffffffffff80n, 0xffffffffffffff80n]),
    ]),
  [i64x2([0x80n, 0x80n])],
);

// ./test/core/simd/simd_i64x2_arith2.wast:32
assert_return(() => invoke($0, `i64x2.abs`, [i64x2([0x80n, 0x80n])]), [
  i64x2([0x80n, 0x80n]),
]);

// ./test/core/simd/simd_i64x2_arith2.wast:34
assert_return(
  () =>
    invoke($0, `i64x2.abs`, [
      i64x2([0xffffffffffffff80n, 0xffffffffffffff80n]),
    ]),
  [i64x2([0x80n, 0x80n])],
);

// ./test/core/simd/simd_i64x2_arith2.wast:38
assert_return(() => invoke($0, `i64x2.abs_with_const_0`, []), [
  i64x2([0x8000000000000000n, 0x7fffffffffffffffn]),
]);

// ./test/core/simd/simd_i64x2_arith2.wast:43
assert_return(
  () =>
    invoke($0, `i64x2.abs`, [
      i64x2([0x8000000000000000n, 0x7fffffffffffffffn]),
    ]),
  [i64x2([0x8000000000000000n, 0x7fffffffffffffffn])],
);

// ./test/core/simd/simd_i64x2_arith2.wast:47
assert_return(() => invoke($0, `i64x2.abs`, [i64x2([0x0n, 0x0n])]), [
  i64x2([0x0n, 0x0n]),
]);

// ./test/core/simd/simd_i64x2_arith2.wast:49
assert_return(() => invoke($0, `i64x2.abs`, [i64x2([0x0n, 0x0n])]), [
  i64x2([0x0n, 0x0n]),
]);

// ./test/core/simd/simd_i64x2_arith2.wast:51
assert_return(() => invoke($0, `i64x2.abs`, [i64x2([0x0n, 0x0n])]), [
  i64x2([0x0n, 0x0n]),
]);

// ./test/core/simd/simd_i64x2_arith2.wast:53
assert_return(() => invoke($0, `i64x2.abs`, [i64x2([0x0n, 0x0n])]), [
  i64x2([0x0n, 0x0n]),
]);

// ./test/core/simd/simd_i64x2_arith2.wast:59
assert_invalid(
  () =>
    instantiate(`(module (func (result v128) (i64x2.abs (f32.const 0.0))))`),
  `type mismatch`,
);

// ./test/core/simd/simd_i64x2_arith2.wast:63
assert_invalid(() =>
  instantiate(`(module
    (func $$i64x2.abs-arg-empty (result v128)
      (i64x2.abs)
    )
  )`), `type mismatch`);

// ./test/core/simd/simd_i64x2_arith2.wast:73
let $1 = instantiate(`(module
  (func (export "i64x2.abs-i64x2.abs") (param v128) (result v128) (i64x2.abs (i64x2.abs (local.get 0))))
)`);

// ./test/core/simd/simd_i64x2_arith2.wast:77
assert_return(
  () =>
    invoke($1, `i64x2.abs-i64x2.abs`, [
      i64x2([0xffffffffffffffffn, 0xffffffffffffffffn]),
    ]),
  [i64x2([0x1n, 0x1n])],
);
