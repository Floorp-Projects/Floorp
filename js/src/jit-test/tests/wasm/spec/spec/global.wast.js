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

// ./test/core/global.wast

// ./test/core/global.wast:3
let $0 = instantiate(`(module
  (global $$a i32 (i32.const -2))
  (global (;1;) f32 (f32.const -3))
  (global (;2;) f64 (f64.const -4))
  (global $$b i64 (i64.const -5))

  (global $$x (mut i32) (i32.const -12))
  (global (;5;) (mut f32) (f32.const -13))
  (global (;6;) (mut f64) (f64.const -14))
  (global $$y (mut i64) (i64.const -15))

  (func (export "get-a") (result i32) (global.get $$a))
  (func (export "get-b") (result i64) (global.get $$b))
  (func (export "get-x") (result i32) (global.get $$x))
  (func (export "get-y") (result i64) (global.get $$y))
  (func (export "set-x") (param i32) (global.set $$x (local.get 0)))
  (func (export "set-y") (param i64) (global.set $$y (local.get 0)))

  (func (export "get-1") (result f32) (global.get 1))
  (func (export "get-2") (result f64) (global.get 2))
  (func (export "get-5") (result f32) (global.get 5))
  (func (export "get-6") (result f64) (global.get 6))
  (func (export "set-5") (param f32) (global.set 5 (local.get 0)))
  (func (export "set-6") (param f64) (global.set 6 (local.get 0)))

  ;; As the argument of control constructs and instructions

  (memory 1)

  (func $$dummy)

  (func (export "as-select-first") (result i32)
    (select (global.get $$x) (i32.const 2) (i32.const 3))
  )
  (func (export "as-select-mid") (result i32)
    (select (i32.const 2) (global.get $$x) (i32.const 3))
  )
  (func (export "as-select-last") (result i32)
    (select (i32.const 2) (i32.const 3) (global.get $$x))
  )

  (func (export "as-loop-first") (result i32)
    (loop (result i32)
      (global.get $$x) (call $$dummy) (call $$dummy)
    )
  )
  (func (export "as-loop-mid") (result i32)
    (loop (result i32)
      (call $$dummy) (global.get $$x) (call $$dummy)
    )
  )
  (func (export "as-loop-last") (result i32)
    (loop (result i32)
      (call $$dummy) (call $$dummy) (global.get $$x)
    )
  )

  (func (export "as-if-condition") (result i32)
    (if (result i32) (global.get $$x)
      (then (call $$dummy) (i32.const 2))
      (else (call $$dummy) (i32.const 3))
    )
  )
  (func (export "as-if-then") (result i32)
    (if (result i32) (i32.const 1)
      (then (global.get $$x)) (else (i32.const 2))
    )
  )
  (func (export "as-if-else") (result i32)
    (if (result i32) (i32.const 0)
      (then (i32.const 2)) (else (global.get $$x))
    )
  )

  (func (export "as-br_if-first") (result i32)
    (block (result i32)
      (br_if 0 (global.get $$x) (i32.const 2))
      (return (i32.const 3))
    )
  )
  (func (export "as-br_if-last") (result i32)
    (block (result i32)
      (br_if 0 (i32.const 2) (global.get $$x))
      (return (i32.const 3))
    )
  )

  (func (export "as-br_table-first") (result i32)
    (block (result i32)
      (global.get $$x) (i32.const 2) (br_table 0 0)
    )
  )
  (func (export "as-br_table-last") (result i32)
    (block (result i32)
      (i32.const 2) (global.get $$x) (br_table 0 0)
    )
  )

  (func $$func (param i32 i32) (result i32) (local.get 0))
  (type $$check (func (param i32 i32) (result i32)))
  (table funcref (elem $$func))
  (func (export "as-call_indirect-first") (result i32)
    (block (result i32)
      (call_indirect (type $$check)
        (global.get $$x) (i32.const 2) (i32.const 0)
      )
    )
  )
  (func (export "as-call_indirect-mid") (result i32)
    (block (result i32)
      (call_indirect (type $$check)
        (i32.const 2) (global.get $$x) (i32.const 0)
      )
    )
  )
 (func (export "as-call_indirect-last") (result i32)
    (block (result i32)
      (call_indirect (type $$check)
        (i32.const 2) (i32.const 0) (global.get $$x)
      )
    )
  )

  (func (export "as-store-first")
    (global.get $$x) (i32.const 1) (i32.store)
  )
  (func (export "as-store-last")
    (i32.const 0) (global.get $$x) (i32.store)
  )
  (func (export "as-load-operand") (result i32)
    (i32.load (global.get $$x))
  )
  (func (export "as-memory.grow-value") (result i32)
    (memory.grow (global.get $$x))
  )

  (func $$f (param i32) (result i32) (local.get 0))
  (func (export "as-call-value") (result i32)
    (call $$f (global.get $$x))
  )

  (func (export "as-return-value") (result i32)
    (global.get $$x) (return)
  )
  (func (export "as-drop-operand")
    (drop (global.get $$x))
  )
  (func (export "as-br-value") (result i32)
    (block (result i32) (br 0 (global.get $$x)))
  )

  (func (export "as-local.set-value") (param i32) (result i32)
    (local.set 0 (global.get $$x))
    (local.get 0)
  )
  (func (export "as-local.tee-value") (param i32) (result i32)
    (local.tee 0 (global.get $$x))
  )
  (func (export "as-global.set-value") (result i32)
    (global.set $$x (global.get $$x))
    (global.get $$x)
  )

  (func (export "as-unary-operand") (result i32)
    (i32.eqz (global.get $$x))
  )
  (func (export "as-binary-operand") (result i32)
    (i32.mul
      (global.get $$x) (global.get $$x)
    )
  )
  (func (export "as-compare-operand") (result i32)
    (i32.gt_u
      (global.get 0) (i32.const 1)
    )
  )
)`);

// ./test/core/global.wast:181
assert_return(() => invoke($0, `get-a`, []), [value("i32", -2)]);

// ./test/core/global.wast:182
assert_return(() => invoke($0, `get-b`, []), [value("i64", -5n)]);

// ./test/core/global.wast:183
assert_return(() => invoke($0, `get-x`, []), [value("i32", -12)]);

// ./test/core/global.wast:184
assert_return(() => invoke($0, `get-y`, []), [value("i64", -15n)]);

// ./test/core/global.wast:186
assert_return(() => invoke($0, `get-1`, []), [value("f32", -3)]);

// ./test/core/global.wast:187
assert_return(() => invoke($0, `get-2`, []), [value("f64", -4)]);

// ./test/core/global.wast:188
assert_return(() => invoke($0, `get-5`, []), [value("f32", -13)]);

// ./test/core/global.wast:189
assert_return(() => invoke($0, `get-6`, []), [value("f64", -14)]);

// ./test/core/global.wast:191
assert_return(() => invoke($0, `set-x`, [6]), []);

// ./test/core/global.wast:192
assert_return(() => invoke($0, `set-y`, [7n]), []);

// ./test/core/global.wast:193
assert_return(() => invoke($0, `set-5`, [value("f32", 8)]), []);

// ./test/core/global.wast:194
assert_return(() => invoke($0, `set-6`, [value("f64", 9)]), []);

// ./test/core/global.wast:196
assert_return(() => invoke($0, `get-x`, []), [value("i32", 6)]);

// ./test/core/global.wast:197
assert_return(() => invoke($0, `get-y`, []), [value("i64", 7n)]);

// ./test/core/global.wast:198
assert_return(() => invoke($0, `get-5`, []), [value("f32", 8)]);

// ./test/core/global.wast:199
assert_return(() => invoke($0, `get-6`, []), [value("f64", 9)]);

// ./test/core/global.wast:201
assert_return(() => invoke($0, `as-select-first`, []), [value("i32", 6)]);

// ./test/core/global.wast:202
assert_return(() => invoke($0, `as-select-mid`, []), [value("i32", 2)]);

// ./test/core/global.wast:203
assert_return(() => invoke($0, `as-select-last`, []), [value("i32", 2)]);

// ./test/core/global.wast:205
assert_return(() => invoke($0, `as-loop-first`, []), [value("i32", 6)]);

// ./test/core/global.wast:206
assert_return(() => invoke($0, `as-loop-mid`, []), [value("i32", 6)]);

// ./test/core/global.wast:207
assert_return(() => invoke($0, `as-loop-last`, []), [value("i32", 6)]);

// ./test/core/global.wast:209
assert_return(() => invoke($0, `as-if-condition`, []), [value("i32", 2)]);

// ./test/core/global.wast:210
assert_return(() => invoke($0, `as-if-then`, []), [value("i32", 6)]);

// ./test/core/global.wast:211
assert_return(() => invoke($0, `as-if-else`, []), [value("i32", 6)]);

// ./test/core/global.wast:213
assert_return(() => invoke($0, `as-br_if-first`, []), [value("i32", 6)]);

// ./test/core/global.wast:214
assert_return(() => invoke($0, `as-br_if-last`, []), [value("i32", 2)]);

// ./test/core/global.wast:216
assert_return(() => invoke($0, `as-br_table-first`, []), [value("i32", 6)]);

// ./test/core/global.wast:217
assert_return(() => invoke($0, `as-br_table-last`, []), [value("i32", 2)]);

// ./test/core/global.wast:219
assert_return(() => invoke($0, `as-call_indirect-first`, []), [
  value("i32", 6),
]);

// ./test/core/global.wast:220
assert_return(() => invoke($0, `as-call_indirect-mid`, []), [value("i32", 2)]);

// ./test/core/global.wast:221
assert_trap(() => invoke($0, `as-call_indirect-last`, []), `undefined element`);

// ./test/core/global.wast:223
assert_return(() => invoke($0, `as-store-first`, []), []);

// ./test/core/global.wast:224
assert_return(() => invoke($0, `as-store-last`, []), []);

// ./test/core/global.wast:225
assert_return(() => invoke($0, `as-load-operand`, []), [value("i32", 1)]);

// ./test/core/global.wast:226
assert_return(() => invoke($0, `as-memory.grow-value`, []), [value("i32", 1)]);

// ./test/core/global.wast:228
assert_return(() => invoke($0, `as-call-value`, []), [value("i32", 6)]);

// ./test/core/global.wast:230
assert_return(() => invoke($0, `as-return-value`, []), [value("i32", 6)]);

// ./test/core/global.wast:231
assert_return(() => invoke($0, `as-drop-operand`, []), []);

// ./test/core/global.wast:232
assert_return(() => invoke($0, `as-br-value`, []), [value("i32", 6)]);

// ./test/core/global.wast:234
assert_return(() => invoke($0, `as-local.set-value`, [1]), [value("i32", 6)]);

// ./test/core/global.wast:235
assert_return(() => invoke($0, `as-local.tee-value`, [1]), [value("i32", 6)]);

// ./test/core/global.wast:236
assert_return(() => invoke($0, `as-global.set-value`, []), [value("i32", 6)]);

// ./test/core/global.wast:238
assert_return(() => invoke($0, `as-unary-operand`, []), [value("i32", 0)]);

// ./test/core/global.wast:239
assert_return(() => invoke($0, `as-binary-operand`, []), [value("i32", 36)]);

// ./test/core/global.wast:240
assert_return(() => invoke($0, `as-compare-operand`, []), [value("i32", 1)]);

// ./test/core/global.wast:242
assert_invalid(
  () =>
    instantiate(
      `(module (global f32 (f32.const 0)) (func (global.set 0 (f32.const 1))))`,
    ),
  `global is immutable`,
);

// ./test/core/global.wast:248
let $1 = instantiate(
  `(module (global (mut f32) (f32.const 0)) (export "a" (global 0)))`,
);

// ./test/core/global.wast:249
let $2 = instantiate(`(module (global (export "a") (mut f32) (f32.const 0)))`);

// ./test/core/global.wast:251
assert_invalid(
  () => instantiate(`(module (global f32 (f32.neg (f32.const 0))))`),
  `constant expression required`,
);

// ./test/core/global.wast:256
assert_invalid(
  () => instantiate(`(module (global f32 (local.get 0)))`),
  `constant expression required`,
);

// ./test/core/global.wast:261
assert_invalid(
  () => instantiate(`(module (global f32 (f32.neg (f32.const 1))))`),
  `constant expression required`,
);

// ./test/core/global.wast:266
assert_invalid(
  () => instantiate(`(module (global i32 (i32.const 0) (nop)))`),
  `constant expression required`,
);

// ./test/core/global.wast:271
assert_invalid(
  () => instantiate(`(module (global i32 (nop)))`),
  `constant expression required`,
);

// ./test/core/global.wast:276
assert_invalid(
  () => instantiate(`(module (global i32 (f32.const 0)))`),
  `type mismatch`,
);

// ./test/core/global.wast:281
assert_invalid(
  () => instantiate(`(module (global i32 (i32.const 0) (i32.const 0)))`),
  `type mismatch`,
);

// ./test/core/global.wast:286
assert_invalid(
  () => instantiate(`(module (global i32 (;empty instruction sequence;)))`),
  `type mismatch`,
);

// ./test/core/global.wast:291
assert_invalid(
  () => instantiate(`(module (global i32 (global.get 0)))`),
  `unknown global`,
);

// ./test/core/global.wast:296
assert_invalid(
  () =>
    instantiate(
      `(module (global i32 (global.get 1)) (global i32 (i32.const 0)))`,
    ),
  `unknown global`,
);

// ./test/core/global.wast:301
let $3 = instantiate(`(module
  (import "spectest" "global_i32" (global i32))
)`);

// ./test/core/global.wast:304
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\98\\80\\80\\80\\00"             ;; import section
      "\\01"                          ;; length 1
      "\\08\\73\\70\\65\\63\\74\\65\\73\\74"  ;; "spectest"
      "\\0a\\67\\6c\\6f\\62\\61\\6c\\5f\\69\\33\\32" ;; "global_i32"
      "\\03"                          ;; GlobalImport
      "\\7f"                          ;; i32
      "\\02"                          ;; malformed mutability
  )`), `malformed mutability`);

// ./test/core/global.wast:317
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\02\\98\\80\\80\\80\\00"             ;; import section
      "\\01"                          ;; length 1
      "\\08\\73\\70\\65\\63\\74\\65\\73\\74"  ;; "spectest"
      "\\0a\\67\\6c\\6f\\62\\61\\6c\\5f\\69\\33\\32" ;; "global_i32"
      "\\03"                          ;; GlobalImport
      "\\7f"                          ;; i32
      "\\ff"                          ;; malformed mutability
  )`), `malformed mutability`);

// ./test/core/global.wast:331
let $4 = instantiate(`(module
  (global i32 (i32.const 0))
)`);

// ./test/core/global.wast:334
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\86\\80\\80\\80\\00"  ;; global section
      "\\01"               ;; length 1
      "\\7f"               ;; i32
      "\\02"               ;; malformed mutability
      "\\41\\00"            ;; i32.const 0
      "\\0b"               ;; end
  )`), `malformed mutability`);

// ./test/core/global.wast:346
assert_malformed(() =>
  instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\06\\86\\80\\80\\80\\00"  ;; global section
      "\\01"               ;; length 1
      "\\7f"               ;; i32
      "\\ff"               ;; malformed mutability
      "\\41\\00"            ;; i32.const 0
      "\\0b"               ;; end
  )`), `malformed mutability`);

// ./test/core/global.wast:360
assert_invalid(() =>
  instantiate(`(module
    (global $$x (mut i32) (i32.const 0))
    (func $$type-global.set-value-empty
      (global.set $$x)
    )
  )`), `type mismatch`);

// ./test/core/global.wast:369
assert_invalid(() =>
  instantiate(`(module
    (global $$x (mut i32) (i32.const 0))
    (func $$type-global.set-value-empty-in-block
      (i32.const 0)
      (block (global.set $$x))
    )
  )`), `type mismatch`);

// ./test/core/global.wast:379
assert_invalid(() =>
  instantiate(`(module
    (global $$x (mut i32) (i32.const 0))
    (func $$type-global.set-value-empty-in-loop
      (i32.const 0)
      (loop (global.set $$x))
    )
  )`), `type mismatch`);

// ./test/core/global.wast:389
assert_invalid(() =>
  instantiate(`(module
    (global $$x (mut i32) (i32.const 0))
    (func $$type-global.set-value-empty-in-then
      (i32.const 0) (i32.const 0)
      (if (then (global.set $$x)))
    )
  )`), `type mismatch`);

// ./test/core/global.wast:399
assert_invalid(() =>
  instantiate(`(module
    (global $$x (mut i32) (i32.const 0))
    (func $$type-global.set-value-empty-in-else
      (i32.const 0) (i32.const 0)
      (if (result i32) (then (i32.const 0)) (else (global.set $$x)))
    )
  )`), `type mismatch`);

// ./test/core/global.wast:409
assert_invalid(() =>
  instantiate(`(module
    (global $$x (mut i32) (i32.const 0))
    (func $$type-global.set-value-empty-in-br
      (i32.const 0)
      (block (br 0 (global.set $$x)))
    )
  )`), `type mismatch`);

// ./test/core/global.wast:419
assert_invalid(() =>
  instantiate(`(module
    (global $$x (mut i32) (i32.const 0))
    (func $$type-global.set-value-empty-in-br_if
      (i32.const 0)
      (block (br_if 0 (global.set $$x)))
    )
  )`), `type mismatch`);

// ./test/core/global.wast:429
assert_invalid(() =>
  instantiate(`(module
    (global $$x (mut i32) (i32.const 0))
    (func $$type-global.set-value-empty-in-br_table
      (i32.const 0)
      (block (br_table 0 (global.set $$x)))
    )
  )`), `type mismatch`);

// ./test/core/global.wast:439
assert_invalid(() =>
  instantiate(`(module
    (global $$x (mut i32) (i32.const 0))
    (func $$type-global.set-value-empty-in-return
      (return (global.set $$x))
    )
  )`), `type mismatch`);

// ./test/core/global.wast:448
assert_invalid(() =>
  instantiate(`(module
    (global $$x (mut i32) (i32.const 0))
    (func $$type-global.set-value-empty-in-select
      (select (global.set $$x) (i32.const 1) (i32.const 2))
    )
  )`), `type mismatch`);

// ./test/core/global.wast:457
assert_invalid(() =>
  instantiate(`(module
    (global $$x (mut i32) (i32.const 0))
    (func $$type-global.set-value-empty-in-call
      (call 1 (global.set $$x))
    )
    (func (param i32) (result i32) (local.get 0))
  )`), `type mismatch`);

// ./test/core/global.wast:467
assert_invalid(() =>
  instantiate(`(module
    (global $$x (mut i32) (i32.const 0))
    (func $$f (param i32) (result i32) (local.get 0))
    (type $$sig (func (param i32) (result i32)))
    (table funcref (elem $$f))
    (func $$type-global.set-value-empty-in-call_indirect
      (block (result i32)
        (call_indirect (type $$sig)
          (global.set $$x) (i32.const 0)
        )
      )
    )
  )`), `type mismatch`);

// ./test/core/global.wast:486
assert_malformed(
  () =>
    instantiate(
      `(global $$foo i32 (i32.const 0)) (global $$foo i32 (i32.const 0)) `,
    ),
  `duplicate global`,
);

// ./test/core/global.wast:490
assert_malformed(
  () =>
    instantiate(
      `(import "" "" (global $$foo i32)) (global $$foo i32 (i32.const 0)) `,
    ),
  `duplicate global`,
);

// ./test/core/global.wast:494
assert_malformed(
  () =>
    instantiate(
      `(import "" "" (global $$foo i32)) (import "" "" (global $$foo i32)) `,
    ),
  `duplicate global`,
);
