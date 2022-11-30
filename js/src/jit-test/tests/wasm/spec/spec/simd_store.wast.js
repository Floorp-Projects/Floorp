// |jit-test| skip-if: !wasmSimdEnabled()

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

// ./test/core/simd/simd_store.wast

// ./test/core/simd/simd_store.wast:3
let $0 = instantiate(`(module
  (memory 1)
  (func (export "v128.store_i8x16") (result v128)
    (v128.store (i32.const 0) (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))
    (v128.load (i32.const 0))
  )
  (func (export "v128.store_i16x8") (result v128)
    (v128.store (i32.const 0) (v128.const i16x8 0 1 2 3 4 5 6 7))
    (v128.load (i32.const 0))
  )
  (func (export "v128.store_i16x8_2") (result v128)
    (v128.store (i32.const 0) (v128.const i16x8 012_345 012_345 012_345 012_345 012_345 012_345 012_345 012_345))
    (v128.load (i32.const 0))
  )
  (func (export "v128.store_i16x8_3") (result v128)
    (v128.store (i32.const 0) (v128.const i16x8 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234))
    (v128.load (i32.const 0))
  )
  (func (export "v128.store_i32x4") (result v128)
    (v128.store (i32.const 0) (v128.const i32x4 0 1 2 3))
    (v128.load (i32.const 0))
  )
  (func (export "v128.store_i32x4_2") (result v128)
    (v128.store (i32.const 0) (v128.const i32x4 0_123_456_789 0_123_456_789 0_123_456_789 0_123_456_789))
    (v128.load (i32.const 0))
  )
  (func (export "v128.store_i32x4_3") (result v128)
    (v128.store (i32.const 0) (v128.const i32x4 0x0_1234_5678 0x0_1234_5678 0x0_1234_5678 0x0_1234_5678))
    (v128.load (i32.const 0))
  )

  (func (export "v128.store_f32x4") (result v128)
    (v128.store (i32.const 0) (v128.const f32x4 0 1 2 3))
    (v128.load (i32.const 0))
  )
)`);

// ./test/core/simd/simd_store.wast:40
assert_return(
  () => invoke($0, `v128.store_i8x16`, []),
  [
    i8x16([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf]),
  ],
);

// ./test/core/simd/simd_store.wast:41
assert_return(() => invoke($0, `v128.store_i16x8`, []), [i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7])]);

// ./test/core/simd/simd_store.wast:42
assert_return(
  () => invoke($0, `v128.store_i16x8_2`, []),
  [i16x8([0x3039, 0x3039, 0x3039, 0x3039, 0x3039, 0x3039, 0x3039, 0x3039])],
);

// ./test/core/simd/simd_store.wast:43
assert_return(
  () => invoke($0, `v128.store_i16x8_3`, []),
  [i16x8([0x1234, 0x1234, 0x1234, 0x1234, 0x1234, 0x1234, 0x1234, 0x1234])],
);

// ./test/core/simd/simd_store.wast:44
assert_return(() => invoke($0, `v128.store_i32x4`, []), [i32x4([0x0, 0x1, 0x2, 0x3])]);

// ./test/core/simd/simd_store.wast:45
assert_return(
  () => invoke($0, `v128.store_i32x4_2`, []),
  [i32x4([0x75bcd15, 0x75bcd15, 0x75bcd15, 0x75bcd15])],
);

// ./test/core/simd/simd_store.wast:46
assert_return(
  () => invoke($0, `v128.store_i32x4_3`, []),
  [i32x4([0x12345678, 0x12345678, 0x12345678, 0x12345678])],
);

// ./test/core/simd/simd_store.wast:47
assert_return(
  () => invoke($0, `v128.store_f32x4`, []),
  [
    new F32x4Pattern(
      value("f32", 0),
      value("f32", 1),
      value("f32", 2),
      value("f32", 3),
    ),
  ],
);

// ./test/core/simd/simd_store.wast:52
let $1 = instantiate(`(module
  (memory 1)
  (func (export "as-block-value")
    (block (v128.store (i32.const 0) (v128.const i32x4 0 0 0 0)))
  )
  (func (export "as-loop-value")
    (loop (v128.store (i32.const 0) (v128.const i32x4 0 0 0 0)))
  )
  (func (export "as-br-value")
    (block (br 0 (v128.store (i32.const 0) (v128.const i32x4 0 0 0 0))))
  )
  (func (export "as-br_if-value")
    (block
      (br_if 0 (v128.store (i32.const 0) (v128.const i32x4 0 0 0 0)) (i32.const 1))
    )
  )
  (func (export "as-br_if-value-cond")
    (block
      (br_if 0 (i32.const 6) (v128.store (i32.const 0) (v128.const i32x4 0 0 0 0)))
    )
  )
  (func (export "as-br_table-value")
    (block
      (br_table 0 (v128.store (i32.const 0) (v128.const i32x4 0 0 0 0)) (i32.const 1))
    )
  )
  (func (export "as-return-value")
    (return (v128.store (i32.const 0) (v128.const i32x4 0 0 0 0)))
  )
  (func (export "as-if-then")
    (if (i32.const 1) (then (v128.store (i32.const 0) (v128.const i32x4 0 0 0 0))))
  )
  (func (export "as-if-else")
    (if (i32.const 0) (then) (else (v128.store (i32.const 0) (v128.const i32x4 0 0 0 0))))
  )
)`);

// ./test/core/simd/simd_store.wast:89
assert_return(() => invoke($1, `as-block-value`, []), []);

// ./test/core/simd/simd_store.wast:90
assert_return(() => invoke($1, `as-loop-value`, []), []);

// ./test/core/simd/simd_store.wast:91
assert_return(() => invoke($1, `as-br-value`, []), []);

// ./test/core/simd/simd_store.wast:92
assert_return(() => invoke($1, `as-br_if-value`, []), []);

// ./test/core/simd/simd_store.wast:93
assert_return(() => invoke($1, `as-br_if-value-cond`, []), []);

// ./test/core/simd/simd_store.wast:94
assert_return(() => invoke($1, `as-br_table-value`, []), []);

// ./test/core/simd/simd_store.wast:95
assert_return(() => invoke($1, `as-return-value`, []), []);

// ./test/core/simd/simd_store.wast:96
assert_return(() => invoke($1, `as-if-then`, []), []);

// ./test/core/simd/simd_store.wast:97
assert_return(() => invoke($1, `as-if-else`, []), []);

// ./test/core/simd/simd_store.wast:102
assert_malformed(
  () => instantiate(`(memory 1) (func (v128.store8 (i32.const 0) (v128.const i32x4 0 0 0 0))) `),
  `unknown operator`,
);

// ./test/core/simd/simd_store.wast:109
assert_malformed(
  () => instantiate(`(memory 1) (func (v128.store16 (i32.const 0) (v128.const i32x4 0 0 0 0))) `),
  `unknown operator`,
);

// ./test/core/simd/simd_store.wast:116
assert_malformed(
  () => instantiate(`(memory 1) (func (v128.store32 (i32.const 0) (v128.const i32x4 0 0 0 0))) `),
  `unknown operator`,
);

// ./test/core/simd/simd_store.wast:127
assert_invalid(
  () => instantiate(`(module (memory 1) (func (v128.store (f32.const 0) (v128.const i32x4 0 0 0 0))))`),
  `type mismatch`,
);

// ./test/core/simd/simd_store.wast:131
assert_invalid(
  () => instantiate(`(module (memory 1) (func (local v128) (block (br_if 0 (v128.store)))))`),
  `type mismatch`,
);

// ./test/core/simd/simd_store.wast:135
assert_invalid(
  () => instantiate(`(module (memory 1) (func (result v128) (v128.store (i32.const 0) (v128.const i32x4 0 0 0 0))))`),
  `type mismatch`,
);

// ./test/core/simd/simd_store.wast:143
assert_invalid(
  () => instantiate(`(module (memory 0)
    (func $$v128.store-1st-arg-empty
      (v128.store (v128.const i32x4 0 0 0 0))
    )
  )`),
  `type mismatch`,
);

// ./test/core/simd/simd_store.wast:151
assert_invalid(
  () => instantiate(`(module (memory 0)
    (func $$v128.store-2nd-arg-empty
      (v128.store (i32.const 0))
    )
  )`),
  `type mismatch`,
);

// ./test/core/simd/simd_store.wast:159
assert_invalid(
  () => instantiate(`(module (memory 0)
    (func $$v128.store-arg-empty
      (v128.store)
    )
  )`),
  `type mismatch`,
);
