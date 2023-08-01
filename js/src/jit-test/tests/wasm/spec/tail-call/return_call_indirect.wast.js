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

// ./test/core/return_call_indirect.wast

// ./test/core/return_call_indirect.wast:3
let $0 = instantiate(`(module
  ;; Auxiliary definitions
  (type $$proc (func))
  (type $$out-i32 (func (result i32)))
  (type $$out-i64 (func (result i64)))
  (type $$out-f32 (func (result f32)))
  (type $$out-f64 (func (result f64)))
  (type $$over-i32 (func (param i32) (result i32)))
  (type $$over-i64 (func (param i64) (result i64)))
  (type $$over-f32 (func (param f32) (result f32)))
  (type $$over-f64 (func (param f64) (result f64)))
  (type $$f32-i32 (func (param f32 i32) (result i32)))
  (type $$i32-i64 (func (param i32 i64) (result i64)))
  (type $$f64-f32 (func (param f64 f32) (result f32)))
  (type $$i64-f64 (func (param i64 f64) (result f64)))
  (type $$over-i32-duplicate (func (param i32) (result i32)))
  (type $$over-i64-duplicate (func (param i64) (result i64)))
  (type $$over-f32-duplicate (func (param f32) (result f32)))
  (type $$over-f64-duplicate (func (param f64) (result f64)))

  (func $$const-i32 (type $$out-i32) (i32.const 0x132))
  (func $$const-i64 (type $$out-i64) (i64.const 0x164))
  (func $$const-f32 (type $$out-f32) (f32.const 0xf32))
  (func $$const-f64 (type $$out-f64) (f64.const 0xf64))

  (func $$id-i32 (type $$over-i32) (local.get 0))
  (func $$id-i64 (type $$over-i64) (local.get 0))
  (func $$id-f32 (type $$over-f32) (local.get 0))
  (func $$id-f64 (type $$over-f64) (local.get 0))

  (func $$i32-i64 (type $$i32-i64) (local.get 1))
  (func $$i64-f64 (type $$i64-f64) (local.get 1))
  (func $$f32-i32 (type $$f32-i32) (local.get 1))
  (func $$f64-f32 (type $$f64-f32) (local.get 1))

  (func $$over-i32-duplicate (type $$over-i32-duplicate) (local.get 0))
  (func $$over-i64-duplicate (type $$over-i64-duplicate) (local.get 0))
  (func $$over-f32-duplicate (type $$over-f32-duplicate) (local.get 0))
  (func $$over-f64-duplicate (type $$over-f64-duplicate) (local.get 0))

  (table funcref
    (elem
      $$const-i32 $$const-i64 $$const-f32 $$const-f64
      $$id-i32 $$id-i64 $$id-f32 $$id-f64
      $$f32-i32 $$i32-i64 $$f64-f32 $$i64-f64
      $$fac $$fac-acc $$even $$odd
      $$over-i32-duplicate $$over-i64-duplicate
      $$over-f32-duplicate $$over-f64-duplicate
    )
  )

  ;; Syntax

  (func
    (return_call_indirect (i32.const 0))
    (return_call_indirect (param i64) (i64.const 0) (i32.const 0))
    (return_call_indirect (param i64) (param) (param f64 i32 i64)
      (i64.const 0) (f64.const 0) (i32.const 0) (i64.const 0) (i32.const 0)
    )
    (return_call_indirect (result) (i32.const 0))
  )

  (func (result i32)
    (return_call_indirect (result i32) (i32.const 0))
    (return_call_indirect (result i32) (result) (i32.const 0))
    (return_call_indirect (param i64) (result i32) (i64.const 0) (i32.const 0))
    (return_call_indirect
      (param) (param i64) (param) (param f64 i32 i64) (param) (param)
      (result) (result i32) (result) (result)
      (i64.const 0) (f64.const 0) (i32.const 0) (i64.const 0) (i32.const 0)
    )
  )

  (func (result i64)
    (return_call_indirect (type $$over-i64) (param i64) (result i64)
      (i64.const 0) (i32.const 0)
    )
  )

  ;; Typing

  (func (export "type-i32") (result i32)
    (return_call_indirect (type $$out-i32) (i32.const 0))
  )
  (func (export "type-i64") (result i64)
    (return_call_indirect (type $$out-i64) (i32.const 1))
  )
  (func (export "type-f32") (result f32)
    (return_call_indirect (type $$out-f32) (i32.const 2))
  )
  (func (export "type-f64") (result f64)
    (return_call_indirect (type $$out-f64) (i32.const 3))
  )

  (func (export "type-index") (result i64)
    (return_call_indirect (type $$over-i64) (i64.const 100) (i32.const 5))
  )

  (func (export "type-first-i32") (result i32)
    (return_call_indirect (type $$over-i32) (i32.const 32) (i32.const 4))
  )
  (func (export "type-first-i64") (result i64)
    (return_call_indirect (type $$over-i64) (i64.const 64) (i32.const 5))
  )
  (func (export "type-first-f32") (result f32)
    (return_call_indirect (type $$over-f32) (f32.const 1.32) (i32.const 6))
  )
  (func (export "type-first-f64") (result f64)
    (return_call_indirect (type $$over-f64) (f64.const 1.64) (i32.const 7))
  )

  (func (export "type-second-i32") (result i32)
    (return_call_indirect (type $$f32-i32)
      (f32.const 32.1) (i32.const 32) (i32.const 8)
    )
  )
  (func (export "type-second-i64") (result i64)
    (return_call_indirect (type $$i32-i64)
      (i32.const 32) (i64.const 64) (i32.const 9)
    )
  )
  (func (export "type-second-f32") (result f32)
    (return_call_indirect (type $$f64-f32)
      (f64.const 64) (f32.const 32) (i32.const 10)
    )
  )
  (func (export "type-second-f64") (result f64)
    (return_call_indirect (type $$i64-f64)
      (i64.const 64) (f64.const 64.1) (i32.const 11)
    )
  )

  ;; Dispatch

  (func (export "dispatch") (param i32 i64) (result i64)
    (return_call_indirect (type $$over-i64) (local.get 1) (local.get 0))
  )

  (func (export "dispatch-structural") (param i32) (result i64)
    (return_call_indirect (type $$over-i64-duplicate)
      (i64.const 9) (local.get 0)
    )
  )

  ;; Multiple tables

  (table $$tab2 funcref (elem $$tab-f1))
  (table $$tab3 funcref (elem $$tab-f2))

  (func $$tab-f1 (result i32) (i32.const 0x133))
  (func $$tab-f2 (result i32) (i32.const 0x134))

  (func (export "call-tab") (param $$i i32) (result i32)
    (if (i32.eq (local.get $$i) (i32.const 0))
      (then (return_call_indirect (type $$out-i32) (i32.const 0)))
    )
    (if (i32.eq (local.get $$i) (i32.const 1))
      (then (return_call_indirect 1 (type $$out-i32) (i32.const 0)))
    )
    (if (i32.eq (local.get $$i) (i32.const 2))
      (then (return_call_indirect $$tab3 (type $$out-i32) (i32.const 0)))
    )
    (i32.const 0)
  )

  ;; Recursion

  (func $$fac (export "fac") (type $$over-i64)
    (return_call_indirect (param i64 i64) (result i64)
      (local.get 0) (i64.const 1) (i32.const 13)
    )
  )

  (func $$fac-acc (param i64 i64) (result i64)
    (if (result i64) (i64.eqz (local.get 0))
      (then (local.get 1))
      (else
        (return_call_indirect (param i64 i64) (result i64)
          (i64.sub (local.get 0) (i64.const 1))
          (i64.mul (local.get 0) (local.get 1))
          (i32.const 13)
        )
      )
    )
  )

  (func $$even (export "even") (param i32) (result i32)
    (if (result i32) (i32.eqz (local.get 0))
      (then (i32.const 44))
      (else
        (return_call_indirect (type $$over-i32)
          (i32.sub (local.get 0) (i32.const 1))
          (i32.const 15)
        )
      )
    )
  )
  (func $$odd (export "odd") (param i32) (result i32)
    (if (result i32) (i32.eqz (local.get 0))
      (then (i32.const 99))
      (else
        (return_call_indirect (type $$over-i32)
          (i32.sub (local.get 0) (i32.const 1))
          (i32.const 14)
        )
      )
    )
  )
)`);

// ./test/core/return_call_indirect.wast:213
assert_return(() => invoke($0, `type-i32`, []), [value("i32", 306)]);

// ./test/core/return_call_indirect.wast:214
assert_return(() => invoke($0, `type-i64`, []), [value("i64", 356n)]);

// ./test/core/return_call_indirect.wast:215
assert_return(() => invoke($0, `type-f32`, []), [value("f32", 3890)]);

// ./test/core/return_call_indirect.wast:216
assert_return(() => invoke($0, `type-f64`, []), [value("f64", 3940)]);

// ./test/core/return_call_indirect.wast:218
assert_return(() => invoke($0, `type-index`, []), [value("i64", 100n)]);

// ./test/core/return_call_indirect.wast:220
assert_return(() => invoke($0, `type-first-i32`, []), [value("i32", 32)]);

// ./test/core/return_call_indirect.wast:221
assert_return(() => invoke($0, `type-first-i64`, []), [value("i64", 64n)]);

// ./test/core/return_call_indirect.wast:222
assert_return(() => invoke($0, `type-first-f32`, []), [value("f32", 1.32)]);

// ./test/core/return_call_indirect.wast:223
assert_return(() => invoke($0, `type-first-f64`, []), [value("f64", 1.64)]);

// ./test/core/return_call_indirect.wast:225
assert_return(() => invoke($0, `type-second-i32`, []), [value("i32", 32)]);

// ./test/core/return_call_indirect.wast:226
assert_return(() => invoke($0, `type-second-i64`, []), [value("i64", 64n)]);

// ./test/core/return_call_indirect.wast:227
assert_return(() => invoke($0, `type-second-f32`, []), [value("f32", 32)]);

// ./test/core/return_call_indirect.wast:228
assert_return(() => invoke($0, `type-second-f64`, []), [value("f64", 64.1)]);

// ./test/core/return_call_indirect.wast:230
assert_return(() => invoke($0, `dispatch`, [5, 2n]), [value("i64", 2n)]);

// ./test/core/return_call_indirect.wast:231
assert_return(() => invoke($0, `dispatch`, [5, 5n]), [value("i64", 5n)]);

// ./test/core/return_call_indirect.wast:232
assert_return(() => invoke($0, `dispatch`, [12, 5n]), [value("i64", 120n)]);

// ./test/core/return_call_indirect.wast:233
assert_return(() => invoke($0, `dispatch`, [17, 2n]), [value("i64", 2n)]);

// ./test/core/return_call_indirect.wast:234
assert_trap(() => invoke($0, `dispatch`, [0, 2n]), `indirect call type mismatch`);

// ./test/core/return_call_indirect.wast:235
assert_trap(() => invoke($0, `dispatch`, [15, 2n]), `indirect call type mismatch`);

// ./test/core/return_call_indirect.wast:236
assert_trap(() => invoke($0, `dispatch`, [20, 2n]), `undefined element`);

// ./test/core/return_call_indirect.wast:237
assert_trap(() => invoke($0, `dispatch`, [-1, 2n]), `undefined element`);

// ./test/core/return_call_indirect.wast:238
assert_trap(() => invoke($0, `dispatch`, [1213432423, 2n]), `undefined element`);

// ./test/core/return_call_indirect.wast:240
assert_return(() => invoke($0, `dispatch-structural`, [5]), [value("i64", 9n)]);

// ./test/core/return_call_indirect.wast:241
assert_return(() => invoke($0, `dispatch-structural`, [5]), [value("i64", 9n)]);

// ./test/core/return_call_indirect.wast:242
assert_return(() => invoke($0, `dispatch-structural`, [12]), [value("i64", 362880n)]);

// ./test/core/return_call_indirect.wast:243
assert_return(() => invoke($0, `dispatch-structural`, [17]), [value("i64", 9n)]);

// ./test/core/return_call_indirect.wast:244
assert_trap(() => invoke($0, `dispatch-structural`, [11]), `indirect call type mismatch`);

// ./test/core/return_call_indirect.wast:245
assert_trap(() => invoke($0, `dispatch-structural`, [16]), `indirect call type mismatch`);

// ./test/core/return_call_indirect.wast:247
assert_return(() => invoke($0, `call-tab`, [0]), [value("i32", 306)]);

// ./test/core/return_call_indirect.wast:248
assert_return(() => invoke($0, `call-tab`, [1]), [value("i32", 307)]);

// ./test/core/return_call_indirect.wast:249
assert_return(() => invoke($0, `call-tab`, [2]), [value("i32", 308)]);

// ./test/core/return_call_indirect.wast:251
assert_return(() => invoke($0, `fac`, [0n]), [value("i64", 1n)]);

// ./test/core/return_call_indirect.wast:252
assert_return(() => invoke($0, `fac`, [1n]), [value("i64", 1n)]);

// ./test/core/return_call_indirect.wast:253
assert_return(() => invoke($0, `fac`, [5n]), [value("i64", 120n)]);

// ./test/core/return_call_indirect.wast:254
assert_return(() => invoke($0, `fac`, [25n]), [value("i64", 7034535277573963776n)]);

// ./test/core/return_call_indirect.wast:256
assert_return(() => invoke($0, `even`, [0]), [value("i32", 44)]);

// ./test/core/return_call_indirect.wast:257
assert_return(() => invoke($0, `even`, [1]), [value("i32", 99)]);

// ./test/core/return_call_indirect.wast:258
assert_return(() => invoke($0, `even`, [100]), [value("i32", 44)]);

// ./test/core/return_call_indirect.wast:259
assert_return(() => invoke($0, `even`, [77]), [value("i32", 99)]);

// ./test/core/return_call_indirect.wast:260
assert_return(() => invoke($0, `even`, [100000]), [value("i32", 44)]);

// ./test/core/return_call_indirect.wast:261
assert_return(() => invoke($0, `even`, [111111]), [value("i32", 99)]);

// ./test/core/return_call_indirect.wast:262
assert_return(() => invoke($0, `odd`, [0]), [value("i32", 99)]);

// ./test/core/return_call_indirect.wast:263
assert_return(() => invoke($0, `odd`, [1]), [value("i32", 44)]);

// ./test/core/return_call_indirect.wast:264
assert_return(() => invoke($0, `odd`, [200]), [value("i32", 99)]);

// ./test/core/return_call_indirect.wast:265
assert_return(() => invoke($0, `odd`, [77]), [value("i32", 44)]);

// ./test/core/return_call_indirect.wast:266
assert_return(() => invoke($0, `odd`, [200002]), [value("i32", 99)]);

// ./test/core/return_call_indirect.wast:267
assert_return(() => invoke($0, `odd`, [300003]), [value("i32", 44)]);

// ./test/core/return_call_indirect.wast:272
assert_malformed(
  () => instantiate(`(type $$sig (func (param i32) (result i32))) (table 0 funcref) (func (result i32)   (return_call_indirect (type $$sig) (result i32) (param i32)     (i32.const 0) (i32.const 0)   ) ) `),
  `unexpected token`,
);

// ./test/core/return_call_indirect.wast:284
assert_malformed(
  () => instantiate(`(type $$sig (func (param i32) (result i32))) (table 0 funcref) (func (result i32)   (return_call_indirect (param i32) (type $$sig) (result i32)     (i32.const 0) (i32.const 0)   ) ) `),
  `unexpected token`,
);

// ./test/core/return_call_indirect.wast:296
assert_malformed(
  () => instantiate(`(type $$sig (func (param i32) (result i32))) (table 0 funcref) (func (result i32)   (return_call_indirect (param i32) (result i32) (type $$sig)     (i32.const 0) (i32.const 0)   ) ) `),
  `unexpected token`,
);

// ./test/core/return_call_indirect.wast:308
assert_malformed(
  () => instantiate(`(type $$sig (func (param i32) (result i32))) (table 0 funcref) (func (result i32)   (return_call_indirect (result i32) (type $$sig) (param i32)     (i32.const 0) (i32.const 0)   ) ) `),
  `unexpected token`,
);

// ./test/core/return_call_indirect.wast:320
assert_malformed(
  () => instantiate(`(type $$sig (func (param i32) (result i32))) (table 0 funcref) (func (result i32)   (return_call_indirect (result i32) (param i32) (type $$sig)     (i32.const 0) (i32.const 0)   ) ) `),
  `unexpected token`,
);

// ./test/core/return_call_indirect.wast:332
assert_malformed(
  () => instantiate(`(table 0 funcref) (func (result i32)   (return_call_indirect (result i32) (param i32)     (i32.const 0) (i32.const 0)   ) ) `),
  `unexpected token`,
);

// ./test/core/return_call_indirect.wast:344
assert_malformed(
  () => instantiate(`(table 0 funcref) (func (return_call_indirect (param $$x i32) (i32.const 0) (i32.const 0))) `),
  `unexpected token`,
);

// ./test/core/return_call_indirect.wast:351
assert_malformed(
  () => instantiate(`(type $$sig (func)) (table 0 funcref) (func (result i32)   (return_call_indirect (type $$sig) (result i32) (i32.const 0)) ) `),
  `inline function type`,
);

// ./test/core/return_call_indirect.wast:361
assert_malformed(
  () => instantiate(`(type $$sig (func (param i32) (result i32))) (table 0 funcref) (func (result i32)   (return_call_indirect (type $$sig) (result i32) (i32.const 0)) ) `),
  `inline function type`,
);

// ./test/core/return_call_indirect.wast:371
assert_malformed(
  () => instantiate(`(type $$sig (func (param i32) (result i32))) (table 0 funcref) (func   (return_call_indirect (type $$sig) (param i32)     (i32.const 0) (i32.const 0)   ) ) `),
  `inline function type`,
);

// ./test/core/return_call_indirect.wast:383
assert_malformed(
  () => instantiate(`(type $$sig (func (param i32 i32) (result i32))) (table 0 funcref) (func (result i32)   (return_call_indirect (type $$sig) (param i32) (result i32)     (i32.const 0) (i32.const 0)   ) ) `),
  `inline function type`,
);

// ./test/core/return_call_indirect.wast:398
assert_invalid(
  () => instantiate(`(module
    (type (func))
    (func $$no-table (return_call_indirect (type 0) (i32.const 0)))
  )`),
  `unknown table`,
);

// ./test/core/return_call_indirect.wast:406
assert_invalid(
  () => instantiate(`(module
    (type (func))
    (table 0 funcref)
    (func $$type-void-vs-num (i32.eqz (return_call_indirect (type 0) (i32.const 0))))
  )`),
  `type mismatch`,
);

// ./test/core/return_call_indirect.wast:414
assert_invalid(
  () => instantiate(`(module
    (type (func (result i64)))
    (table 0 funcref)
    (func $$type-num-vs-num (i32.eqz (return_call_indirect (type 0) (i32.const 0))))
  )`),
  `type mismatch`,
);

// ./test/core/return_call_indirect.wast:423
assert_invalid(
  () => instantiate(`(module
    (type (func (param i32)))
    (table 0 funcref)
    (func $$arity-0-vs-1 (return_call_indirect (type 0) (i32.const 0)))
  )`),
  `type mismatch`,
);

// ./test/core/return_call_indirect.wast:431
assert_invalid(
  () => instantiate(`(module
    (type (func (param f64 i32)))
    (table 0 funcref)
    (func $$arity-0-vs-2 (return_call_indirect (type 0) (i32.const 0)))
  )`),
  `type mismatch`,
);

// ./test/core/return_call_indirect.wast:440
let $1 = instantiate(`(module
  (type (func))
  (table 0 funcref)
  (func $$arity-1-vs-0 (return_call_indirect (type 0) (i32.const 1) (i32.const 0)))
)`);

// ./test/core/return_call_indirect.wast:446
let $2 = instantiate(`(module
  (type (func))
  (table 0 funcref)
  (func $$arity-2-vs-0
    (return_call_indirect (type 0) (f64.const 2) (i32.const 1) (i32.const 0))
  )
)`);

// ./test/core/return_call_indirect.wast:454
assert_invalid(
  () => instantiate(`(module
    (type (func (param i32)))
    (table 0 funcref)
    (func $$type-func-void-vs-i32 (return_call_indirect (type 0) (i32.const 1) (nop)))
  )`),
  `type mismatch`,
);

// ./test/core/return_call_indirect.wast:462
assert_invalid(
  () => instantiate(`(module
    (type (func (param i32)))
    (table 0 funcref)
    (func $$type-func-num-vs-i32 (return_call_indirect (type 0) (i32.const 0) (i64.const 1)))
  )`),
  `type mismatch`,
);

// ./test/core/return_call_indirect.wast:471
assert_invalid(
  () => instantiate(`(module
    (type (func (param i32 i32)))
    (table 0 funcref)
    (func $$type-first-void-vs-num
      (return_call_indirect (type 0) (nop) (i32.const 1) (i32.const 0))
    )
  )`),
  `type mismatch`,
);

// ./test/core/return_call_indirect.wast:481
assert_invalid(
  () => instantiate(`(module
    (type (func (param i32 i32)))
    (table 0 funcref)
    (func $$type-second-void-vs-num
      (return_call_indirect (type 0) (i32.const 1) (nop) (i32.const 0))
    )
  )`),
  `type mismatch`,
);

// ./test/core/return_call_indirect.wast:491
assert_invalid(
  () => instantiate(`(module
    (type (func (param i32 f64)))
    (table 0 funcref)
    (func $$type-first-num-vs-num
      (return_call_indirect (type 0) (f64.const 1) (i32.const 1) (i32.const 0))
    )
  )`),
  `type mismatch`,
);

// ./test/core/return_call_indirect.wast:501
assert_invalid(
  () => instantiate(`(module
    (type (func (param f64 i32)))
    (table 0 funcref)
    (func $$type-second-num-vs-num
      (return_call_indirect (type 0) (i32.const 1) (f64.const 1) (i32.const 0))
    )
  )`),
  `type mismatch`,
);

// ./test/core/return_call_indirect.wast:515
assert_invalid(
  () => instantiate(`(module
    (table 0 funcref)
    (func $$unbound-type (return_call_indirect (type 1) (i32.const 0)))
  )`),
  `unknown type`,
);

// ./test/core/return_call_indirect.wast:522
assert_invalid(
  () => instantiate(`(module
    (table 0 funcref)
    (func $$large-type (return_call_indirect (type 1012321300) (i32.const 0)))
  )`),
  `unknown type`,
);

// ./test/core/return_call_indirect.wast:533
assert_invalid(
  () => instantiate(`(module (table funcref (elem 0 0)))`),
  `unknown function 0`,
);
