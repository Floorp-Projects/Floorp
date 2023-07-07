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

// ./test/core/store.wast

// ./test/core/store.wast:3
let $0 = instantiate(`(module
  (memory $$mem1 1)
  (memory $$mem2 1)

  (func (export "load1") (param i32) (result i64)
    (i64.load $$mem1 (local.get 0))
  )
  (func (export "load2") (param i32) (result i64)
    (i64.load $$mem2 (local.get 0))
  )

  (func (export "store1") (param i32 i64)
    (i64.store $$mem1 (local.get 0) (local.get 1))
  )
  (func (export "store2") (param i32 i64)
    (i64.store $$mem2 (local.get 0) (local.get 1))
  )
)`);

// ./test/core/store.wast:22
invoke($0, `store1`, [0, 1n]);

// ./test/core/store.wast:23
invoke($0, `store2`, [0, 2n]);

// ./test/core/store.wast:24
assert_return(() => invoke($0, `load1`, [0]), [value("i64", 1n)]);

// ./test/core/store.wast:25
assert_return(() => invoke($0, `load2`, [0]), [value("i64", 2n)]);

// ./test/core/store.wast:28
let $1 = instantiate(`(module $$M1
  (memory (export "mem") 1)

  (func (export "load") (param i32) (result i64)
    (i64.load (local.get 0))
  )
  (func (export "store") (param i32 i64)
    (i64.store (local.get 0) (local.get 1))
  )
)`);
register($1, `M1`);

// ./test/core/store.wast:38
register($1, `M1`);

// ./test/core/store.wast:40
let $2 = instantiate(`(module $$M2
  (memory (export "mem") 1)

  (func (export "load") (param i32) (result i64)
    (i64.load (local.get 0))
  )
  (func (export "store") (param i32 i64)
    (i64.store (local.get 0) (local.get 1))
  )
)`);
register($2, `M2`);

// ./test/core/store.wast:50
register($2, `M2`);

// ./test/core/store.wast:52
invoke(`M1`, `store`, [0, 1n]);

// ./test/core/store.wast:53
invoke(`M2`, `store`, [0, 2n]);

// ./test/core/store.wast:54
assert_return(() => invoke(`M1`, `load`, [0]), [value("i64", 1n)]);

// ./test/core/store.wast:55
assert_return(() => invoke(`M2`, `load`, [0]), [value("i64", 2n)]);

// ./test/core/store.wast:57
let $3 = instantiate(`(module
  (memory $$mem1 (import "M1" "mem") 1)
  (memory $$mem2 (import "M2" "mem") 1)

  (func (export "load1") (param i32) (result i64)
    (i64.load $$mem1 (local.get 0))
  )
  (func (export "load2") (param i32) (result i64)
    (i64.load $$mem2 (local.get 0))
  )

  (func (export "store1") (param i32 i64)
    (i64.store $$mem1 (local.get 0) (local.get 1))
  )
  (func (export "store2") (param i32 i64)
    (i64.store $$mem2 (local.get 0) (local.get 1))
  )
)`);

// ./test/core/store.wast:76
invoke($3, `store1`, [0, 1n]);

// ./test/core/store.wast:77
invoke($3, `store2`, [0, 2n]);

// ./test/core/store.wast:78
assert_return(() => invoke($3, `load1`, [0]), [value("i64", 1n)]);

// ./test/core/store.wast:79
assert_return(() => invoke($3, `load2`, [0]), [value("i64", 2n)]);

// ./test/core/store.wast:82
let $4 = instantiate(`(module
  (memory (export "mem") 2)
)`);

// ./test/core/store.wast:85
register($4, `M`);

// ./test/core/store.wast:87
let $5 = instantiate(`(module
  (memory $$mem1 (import "M" "mem") 2)
  (memory $$mem2 3)

  (data (memory $$mem1) (i32.const 20) "\\01\\02\\03\\04\\05")
  (data (memory $$mem2) (i32.const 50) "\\0A\\0B\\0C\\0D\\0E")

  (func (export "read1") (param i32) (result i32)
    (i32.load8_u $$mem1 (local.get 0))
  )
  (func (export "read2") (param i32) (result i32)
    (i32.load8_u $$mem2 (local.get 0))
  )

  (func (export "copy-1-to-2")
    (local $$i i32)
    (local.set $$i (i32.const 20))
    (loop $$cont
      (br_if 1 (i32.eq (local.get $$i) (i32.const 23)))
      (i32.store8 $$mem2 (local.get $$i) (i32.load8_u $$mem1 (local.get $$i)))
      (local.set $$i (i32.add (local.get $$i) (i32.const 1)))
      (br $$cont)
    )
  )

  (func (export "copy-2-to-1")
    (local $$i i32)
    (local.set $$i (i32.const 50))
    (loop $$cont
      (br_if 1 (i32.eq (local.get $$i) (i32.const 54)))
      (i32.store8 $$mem1 (local.get $$i) (i32.load8_u $$mem2 (local.get $$i)))
      (local.set $$i (i32.add (local.get $$i) (i32.const 1)))
      (br $$cont)
    )
  )
)`);

// ./test/core/store.wast:124
assert_return(() => invoke($5, `read2`, [20]), [value("i32", 0)]);

// ./test/core/store.wast:125
assert_return(() => invoke($5, `read2`, [21]), [value("i32", 0)]);

// ./test/core/store.wast:126
assert_return(() => invoke($5, `read2`, [22]), [value("i32", 0)]);

// ./test/core/store.wast:127
assert_return(() => invoke($5, `read2`, [23]), [value("i32", 0)]);

// ./test/core/store.wast:128
assert_return(() => invoke($5, `read2`, [24]), [value("i32", 0)]);

// ./test/core/store.wast:129
invoke($5, `copy-1-to-2`, []);

// ./test/core/store.wast:130
assert_return(() => invoke($5, `read2`, [20]), [value("i32", 1)]);

// ./test/core/store.wast:131
assert_return(() => invoke($5, `read2`, [21]), [value("i32", 2)]);

// ./test/core/store.wast:132
assert_return(() => invoke($5, `read2`, [22]), [value("i32", 3)]);

// ./test/core/store.wast:133
assert_return(() => invoke($5, `read2`, [23]), [value("i32", 0)]);

// ./test/core/store.wast:134
assert_return(() => invoke($5, `read2`, [24]), [value("i32", 0)]);

// ./test/core/store.wast:136
assert_return(() => invoke($5, `read1`, [50]), [value("i32", 0)]);

// ./test/core/store.wast:137
assert_return(() => invoke($5, `read1`, [51]), [value("i32", 0)]);

// ./test/core/store.wast:138
assert_return(() => invoke($5, `read1`, [52]), [value("i32", 0)]);

// ./test/core/store.wast:139
assert_return(() => invoke($5, `read1`, [53]), [value("i32", 0)]);

// ./test/core/store.wast:140
assert_return(() => invoke($5, `read1`, [54]), [value("i32", 0)]);

// ./test/core/store.wast:141
invoke($5, `copy-2-to-1`, []);

// ./test/core/store.wast:142
assert_return(() => invoke($5, `read1`, [50]), [value("i32", 10)]);

// ./test/core/store.wast:143
assert_return(() => invoke($5, `read1`, [51]), [value("i32", 11)]);

// ./test/core/store.wast:144
assert_return(() => invoke($5, `read1`, [52]), [value("i32", 12)]);

// ./test/core/store.wast:145
assert_return(() => invoke($5, `read1`, [53]), [value("i32", 13)]);

// ./test/core/store.wast:146
assert_return(() => invoke($5, `read1`, [54]), [value("i32", 0)]);

// ./test/core/store.wast:151
let $6 = instantiate(`(module
  (memory 1)

  (func (export "as-block-value")
    (block (i32.store (i32.const 0) (i32.const 1)))
  )
  (func (export "as-loop-value")
    (loop (i32.store (i32.const 0) (i32.const 1)))
  )

  (func (export "as-br-value")
    (block (br 0 (i32.store (i32.const 0) (i32.const 1))))
  )
  (func (export "as-br_if-value")
    (block
      (br_if 0 (i32.store (i32.const 0) (i32.const 1)) (i32.const 1))
    )
  )
  (func (export "as-br_if-value-cond")
    (block
      (br_if 0 (i32.const 6) (i32.store (i32.const 0) (i32.const 1)))
    )
  )
  (func (export "as-br_table-value")
    (block
      (br_table 0 (i32.store (i32.const 0) (i32.const 1)) (i32.const 1))
    )
  )

  (func (export "as-return-value")
    (return (i32.store (i32.const 0) (i32.const 1)))
  )

  (func (export "as-if-then")
    (if (i32.const 1) (then (i32.store (i32.const 0) (i32.const 1))))
  )
  (func (export "as-if-else")
    (if (i32.const 0) (then) (else (i32.store (i32.const 0) (i32.const 1))))
  )
)`);

// ./test/core/store.wast:192
assert_return(() => invoke($6, `as-block-value`, []), []);

// ./test/core/store.wast:193
assert_return(() => invoke($6, `as-loop-value`, []), []);

// ./test/core/store.wast:195
assert_return(() => invoke($6, `as-br-value`, []), []);

// ./test/core/store.wast:196
assert_return(() => invoke($6, `as-br_if-value`, []), []);

// ./test/core/store.wast:197
assert_return(() => invoke($6, `as-br_if-value-cond`, []), []);

// ./test/core/store.wast:198
assert_return(() => invoke($6, `as-br_table-value`, []), []);

// ./test/core/store.wast:200
assert_return(() => invoke($6, `as-return-value`, []), []);

// ./test/core/store.wast:202
assert_return(() => invoke($6, `as-if-then`, []), []);

// ./test/core/store.wast:203
assert_return(() => invoke($6, `as-if-else`, []), []);

// ./test/core/store.wast:205
assert_malformed(
  () => instantiate(`(memory 1) (func (param i32) (i32.store32 (local.get 0) (i32.const 0))) `),
  `unknown operator`,
);

// ./test/core/store.wast:212
assert_malformed(
  () => instantiate(`(memory 1) (func (param i32) (i32.store64 (local.get 0) (i64.const 0))) `),
  `unknown operator`,
);

// ./test/core/store.wast:220
assert_malformed(
  () => instantiate(`(memory 1) (func (param i32) (i64.store64 (local.get 0) (i64.const 0))) `),
  `unknown operator`,
);

// ./test/core/store.wast:228
assert_malformed(
  () => instantiate(`(memory 1) (func (param i32) (f32.store32 (local.get 0) (f32.const 0))) `),
  `unknown operator`,
);

// ./test/core/store.wast:235
assert_malformed(
  () => instantiate(`(memory 1) (func (param i32) (f32.store64 (local.get 0) (f64.const 0))) `),
  `unknown operator`,
);

// ./test/core/store.wast:243
assert_malformed(
  () => instantiate(`(memory 1) (func (param i32) (f64.store32 (local.get 0) (f32.const 0))) `),
  `unknown operator`,
);

// ./test/core/store.wast:250
assert_malformed(
  () => instantiate(`(memory 1) (func (param i32) (f64.store64 (local.get 0) (f64.const 0))) `),
  `unknown operator`,
);

// ./test/core/store.wast:259
assert_invalid(
  () => instantiate(`(module (memory 1) (func (param i32) (result i32) (i32.store (i32.const 0) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/store.wast:263
assert_invalid(
  () => instantiate(`(module (memory 1) (func (param i64) (result i64) (i64.store (i32.const 0) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/store.wast:267
assert_invalid(
  () => instantiate(`(module (memory 1) (func (param f32) (result f32) (f32.store (i32.const 0) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/store.wast:271
assert_invalid(
  () => instantiate(`(module (memory 1) (func (param f64) (result f64) (f64.store (i32.const 0) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/store.wast:275
assert_invalid(
  () => instantiate(`(module (memory 1) (func (param i32) (result i32) (i32.store8 (i32.const 0) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/store.wast:279
assert_invalid(
  () => instantiate(`(module (memory 1) (func (param i32) (result i32) (i32.store16 (i32.const 0) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/store.wast:283
assert_invalid(
  () => instantiate(`(module (memory 1) (func (param i64) (result i64) (i64.store8 (i32.const 0) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/store.wast:287
assert_invalid(
  () => instantiate(`(module (memory 1) (func (param i64) (result i64) (i64.store16 (i32.const 0) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/store.wast:291
assert_invalid(
  () => instantiate(`(module (memory 1) (func (param i64) (result i64) (i64.store32 (i32.const 0) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/store.wast:297
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-address-empty
      (i32.store)
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:306
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-value-empty
     (i32.const 0) (i32.store)
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:315
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-address-empty-in-block
      (i32.const 0) (i32.const 0)
      (block (i32.store))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:325
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-value-empty-in-block
      (i32.const 0)
      (block (i32.const 0) (i32.store))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:335
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-address-empty-in-loop
      (i32.const 0) (i32.const 0)
      (loop (i32.store))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:345
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-value-empty-in-loop
      (i32.const 0)
      (loop (i32.const 0) (i32.store))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:355
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-address-empty-in-then
      (i32.const 0) (i32.const 0)
      (if (then (i32.store)))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:365
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-value-empty-in-then
      (i32.const 0)
      (if (then (i32.const 0) (i32.store)))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:375
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-address-empty-in-else
      (i32.const 0) (i32.const 0)
      (if (result i32) (then (i32.const 0)) (else (i32.store)))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:385
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-value-empty-in-else
      (i32.const 0)
      (if (result i32) (then (i32.const 0)) (else (i32.const 0) (i32.store)))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:395
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-address-empty-in-br
      (i32.const 0) (i32.const 0)
      (block (br 0 (i32.store)))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:405
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-value-empty-in-br
      (i32.const 0)
      (block (br 0 (i32.const 0) (i32.store)))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:415
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-address-empty-in-br_if
      (i32.const 0) (i32.const 0)
      (block (br_if 0 (i32.store) (i32.const 1)) )
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:425
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-value-empty-in-br_if
      (i32.const 0)
      (block (br_if 0 (i32.const 0) (i32.store) (i32.const 1)) )
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:435
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-address-empty-in-br_table
      (i32.const 0) (i32.const 0)
      (block (br_table 0 (i32.store)))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:445
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-value-empty-in-br_table
      (i32.const 0)
      (block (br_table 0 (i32.const 0) (i32.store)))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:455
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-address-empty-in-return
      (return (i32.store))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:464
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-value-empty-in-return
      (return (i32.const 0) (i32.store))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:473
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-address-empty-in-select
      (select (i32.store) (i32.const 1) (i32.const 2))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:482
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-value-empty-in-select
      (select (i32.const 0) (i32.store) (i32.const 1) (i32.const 2))
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:491
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-address-empty-in-call
      (call 1 (i32.store))
    )
    (func (param i32) (result i32) (local.get 0))
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:501
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-value-empty-in-call
      (call 1 (i32.const 0) (i32.store))
    )
    (func (param i32) (result i32) (local.get 0))
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:511
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$f (param i32) (result i32) (local.get 0))
    (type $$sig (func (param i32) (result i32)))
    (table funcref (elem $$f))
    (func $$type-address-empty-in-call_indirect
      (block (result i32)
        (call_indirect (type $$sig)
          (i32.store) (i32.const 0)
        )
      )
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:527
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$f (param i32) (result i32) (local.get 0))
    (type $$sig (func (param i32) (result i32)))
    (table funcref (elem $$f))
    (func $$type-value-empty-in-call_indirect
      (block (result i32)
        (call_indirect (type $$sig)
          (i32.const 0) (i32.store) (i32.const 0)
        )
      )
    )
  )`),
  `type mismatch`,
);

// ./test/core/store.wast:547
assert_invalid(
  () => instantiate(`(module (memory 1) (func (i32.store (f32.const 0) (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:548
assert_invalid(
  () => instantiate(`(module (memory 1) (func (i32.store8 (f32.const 0) (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:549
assert_invalid(
  () => instantiate(`(module (memory 1) (func (i32.store16 (f32.const 0) (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:550
assert_invalid(
  () => instantiate(`(module (memory 1) (func (i64.store (f32.const 0) (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:551
assert_invalid(
  () => instantiate(`(module (memory 1) (func (i64.store8 (f32.const 0) (i64.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:552
assert_invalid(
  () => instantiate(`(module (memory 1) (func (i64.store16 (f32.const 0) (i64.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:553
assert_invalid(
  () => instantiate(`(module (memory 1) (func (i64.store32 (f32.const 0) (i64.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:554
assert_invalid(
  () => instantiate(`(module (memory 1) (func (f32.store (f32.const 0) (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:555
assert_invalid(
  () => instantiate(`(module (memory 1) (func (f64.store (f32.const 0) (f64.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:557
assert_invalid(
  () => instantiate(`(module (memory 1) (func (i32.store (i32.const 0) (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:558
assert_invalid(
  () => instantiate(`(module (memory 1) (func (i32.store8 (i32.const 0) (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:559
assert_invalid(
  () => instantiate(`(module (memory 1) (func (i32.store16 (i32.const 0) (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:560
assert_invalid(
  () => instantiate(`(module (memory 1) (func (i64.store (i32.const 0) (f32.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:561
assert_invalid(
  () => instantiate(`(module (memory 1) (func (i64.store8 (i32.const 0) (f64.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:562
assert_invalid(
  () => instantiate(`(module (memory 1) (func (i64.store16 (i32.const 0) (f64.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:563
assert_invalid(
  () => instantiate(`(module (memory 1) (func (i64.store32 (i32.const 0) (f64.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:564
assert_invalid(
  () => instantiate(`(module (memory 1) (func (f32.store (i32.const 0) (i32.const 0))))`),
  `type mismatch`,
);

// ./test/core/store.wast:565
assert_invalid(
  () => instantiate(`(module (memory 1) (func (f64.store (i32.const 0) (i64.const 0))))`),
  `type mismatch`,
);
