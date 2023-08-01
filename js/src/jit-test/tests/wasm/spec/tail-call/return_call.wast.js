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

// ./test/core/return_call.wast

// ./test/core/return_call.wast:3
let $0 = instantiate(`(module
  ;; Auxiliary definitions
  (func $$const-i32 (result i32) (i32.const 0x132))
  (func $$const-i64 (result i64) (i64.const 0x164))
  (func $$const-f32 (result f32) (f32.const 0xf32))
  (func $$const-f64 (result f64) (f64.const 0xf64))

  (func $$id-i32 (param i32) (result i32) (local.get 0))
  (func $$id-i64 (param i64) (result i64) (local.get 0))
  (func $$id-f32 (param f32) (result f32) (local.get 0))
  (func $$id-f64 (param f64) (result f64) (local.get 0))

  (func $$f32-i32 (param f32 i32) (result i32) (local.get 1))
  (func $$i32-i64 (param i32 i64) (result i64) (local.get 1))
  (func $$f64-f32 (param f64 f32) (result f32) (local.get 1))
  (func $$i64-f64 (param i64 f64) (result f64) (local.get 1))

  ;; Typing

  (func (export "type-i32") (result i32) (return_call $$const-i32))
  (func (export "type-i64") (result i64) (return_call $$const-i64))
  (func (export "type-f32") (result f32) (return_call $$const-f32))
  (func (export "type-f64") (result f64) (return_call $$const-f64))

  (func (export "type-first-i32") (result i32) (return_call $$id-i32 (i32.const 32)))
  (func (export "type-first-i64") (result i64) (return_call $$id-i64 (i64.const 64)))
  (func (export "type-first-f32") (result f32) (return_call $$id-f32 (f32.const 1.32)))
  (func (export "type-first-f64") (result f64) (return_call $$id-f64 (f64.const 1.64)))

  (func (export "type-second-i32") (result i32)
    (return_call $$f32-i32 (f32.const 32.1) (i32.const 32))
  )
  (func (export "type-second-i64") (result i64)
    (return_call $$i32-i64 (i32.const 32) (i64.const 64))
  )
  (func (export "type-second-f32") (result f32)
    (return_call $$f64-f32 (f64.const 64) (f32.const 32))
  )
  (func (export "type-second-f64") (result f64)
    (return_call $$i64-f64 (i64.const 64) (f64.const 64.1))
  )

  ;; Recursion

  (func $$fac-acc (export "fac-acc") (param i64 i64) (result i64)
    (if (result i64) (i64.eqz (local.get 0))
      (then (local.get 1))
      (else
        (return_call $$fac-acc
          (i64.sub (local.get 0) (i64.const 1))
          (i64.mul (local.get 0) (local.get 1))
        )
      )
    )
  )

  (func $$count (export "count") (param i64) (result i64)
    (if (result i64) (i64.eqz (local.get 0))
      (then (local.get 0))
      (else (return_call $$count (i64.sub (local.get 0) (i64.const 1))))
    )
  )

  (func $$even (export "even") (param i64) (result i32)
    (if (result i32) (i64.eqz (local.get 0))
      (then (i32.const 44))
      (else (return_call $$odd (i64.sub (local.get 0) (i64.const 1))))
    )
  )
  (func $$odd (export "odd") (param i64) (result i32)
    (if (result i32) (i64.eqz (local.get 0))
      (then (i32.const 99))
      (else (return_call $$even (i64.sub (local.get 0) (i64.const 1))))
    )
  )
)`);

// ./test/core/return_call.wast:80
assert_return(() => invoke($0, `type-i32`, []), [value("i32", 306)]);

// ./test/core/return_call.wast:81
assert_return(() => invoke($0, `type-i64`, []), [value("i64", 356n)]);

// ./test/core/return_call.wast:82
assert_return(() => invoke($0, `type-f32`, []), [value("f32", 3890)]);

// ./test/core/return_call.wast:83
assert_return(() => invoke($0, `type-f64`, []), [value("f64", 3940)]);

// ./test/core/return_call.wast:85
assert_return(() => invoke($0, `type-first-i32`, []), [value("i32", 32)]);

// ./test/core/return_call.wast:86
assert_return(() => invoke($0, `type-first-i64`, []), [value("i64", 64n)]);

// ./test/core/return_call.wast:87
assert_return(() => invoke($0, `type-first-f32`, []), [value("f32", 1.32)]);

// ./test/core/return_call.wast:88
assert_return(() => invoke($0, `type-first-f64`, []), [value("f64", 1.64)]);

// ./test/core/return_call.wast:90
assert_return(() => invoke($0, `type-second-i32`, []), [value("i32", 32)]);

// ./test/core/return_call.wast:91
assert_return(() => invoke($0, `type-second-i64`, []), [value("i64", 64n)]);

// ./test/core/return_call.wast:92
assert_return(() => invoke($0, `type-second-f32`, []), [value("f32", 32)]);

// ./test/core/return_call.wast:93
assert_return(() => invoke($0, `type-second-f64`, []), [value("f64", 64.1)]);

// ./test/core/return_call.wast:95
assert_return(() => invoke($0, `fac-acc`, [0n, 1n]), [value("i64", 1n)]);

// ./test/core/return_call.wast:96
assert_return(() => invoke($0, `fac-acc`, [1n, 1n]), [value("i64", 1n)]);

// ./test/core/return_call.wast:97
assert_return(() => invoke($0, `fac-acc`, [5n, 1n]), [value("i64", 120n)]);

// ./test/core/return_call.wast:98
assert_return(() => invoke($0, `fac-acc`, [25n, 1n]), [value("i64", 7034535277573963776n)]);

// ./test/core/return_call.wast:103
assert_return(() => invoke($0, `count`, [0n]), [value("i64", 0n)]);

// ./test/core/return_call.wast:104
assert_return(() => invoke($0, `count`, [1000n]), [value("i64", 0n)]);

// ./test/core/return_call.wast:105
assert_return(() => invoke($0, `count`, [1000000n]), [value("i64", 0n)]);

// ./test/core/return_call.wast:107
assert_return(() => invoke($0, `even`, [0n]), [value("i32", 44)]);

// ./test/core/return_call.wast:108
assert_return(() => invoke($0, `even`, [1n]), [value("i32", 99)]);

// ./test/core/return_call.wast:109
assert_return(() => invoke($0, `even`, [100n]), [value("i32", 44)]);

// ./test/core/return_call.wast:110
assert_return(() => invoke($0, `even`, [77n]), [value("i32", 99)]);

// ./test/core/return_call.wast:111
assert_return(() => invoke($0, `even`, [1000000n]), [value("i32", 44)]);

// ./test/core/return_call.wast:112
assert_return(() => invoke($0, `even`, [1000001n]), [value("i32", 99)]);

// ./test/core/return_call.wast:113
assert_return(() => invoke($0, `odd`, [0n]), [value("i32", 99)]);

// ./test/core/return_call.wast:114
assert_return(() => invoke($0, `odd`, [1n]), [value("i32", 44)]);

// ./test/core/return_call.wast:115
assert_return(() => invoke($0, `odd`, [200n]), [value("i32", 99)]);

// ./test/core/return_call.wast:116
assert_return(() => invoke($0, `odd`, [77n]), [value("i32", 44)]);

// ./test/core/return_call.wast:117
assert_return(() => invoke($0, `odd`, [1000000n]), [value("i32", 99)]);

// ./test/core/return_call.wast:118
assert_return(() => invoke($0, `odd`, [999999n]), [value("i32", 44)]);

// ./test/core/return_call.wast:123
assert_invalid(
  () => instantiate(`(module
    (func $$type-void-vs-num (result i32) (return_call 1) (i32.const 0))
    (func)
  )`),
  `type mismatch`,
);

// ./test/core/return_call.wast:130
assert_invalid(
  () => instantiate(`(module
    (func $$type-num-vs-num (result i32) (return_call 1) (i32.const 0))
    (func (result i64) (i64.const 1))
  )`),
  `type mismatch`,
);

// ./test/core/return_call.wast:138
assert_invalid(
  () => instantiate(`(module
    (func $$arity-0-vs-1 (return_call 1))
    (func (param i32))
  )`),
  `type mismatch`,
);

// ./test/core/return_call.wast:145
assert_invalid(
  () => instantiate(`(module
    (func $$arity-0-vs-2 (return_call 1))
    (func (param f64 i32))
  )`),
  `type mismatch`,
);

// ./test/core/return_call.wast:153
let $1 = instantiate(`(module
  (func $$arity-1-vs-0 (i32.const 1) (return_call 1))
  (func)
)`);

// ./test/core/return_call.wast:158
let $2 = instantiate(`(module
  (func $$arity-2-vs-0 (f64.const 2) (i32.const 1) (return_call 1))
  (func)
)`);

// ./test/core/return_call.wast:163
assert_invalid(
  () => instantiate(`(module
    (func $$type-first-void-vs-num (return_call 1 (nop) (i32.const 1)))
    (func (param i32 i32))
  )`),
  `type mismatch`,
);

// ./test/core/return_call.wast:170
assert_invalid(
  () => instantiate(`(module
    (func $$type-second-void-vs-num (return_call 1 (i32.const 1) (nop)))
    (func (param i32 i32))
  )`),
  `type mismatch`,
);

// ./test/core/return_call.wast:177
assert_invalid(
  () => instantiate(`(module
    (func $$type-first-num-vs-num (return_call 1 (f64.const 1) (i32.const 1)))
    (func (param i32 f64))
  )`),
  `type mismatch`,
);

// ./test/core/return_call.wast:184
assert_invalid(
  () => instantiate(`(module
    (func $$type-second-num-vs-num (return_call 1 (i32.const 1) (f64.const 1)))
    (func (param f64 i32))
  )`),
  `type mismatch`,
);

// ./test/core/return_call.wast:195
assert_invalid(
  () => instantiate(`(module (func $$unbound-func (return_call 1)))`),
  `unknown function`,
);

// ./test/core/return_call.wast:199
assert_invalid(
  () => instantiate(`(module (func $$large-func (return_call 1012321300)))`),
  `unknown function`,
);
