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

// ./test/core/memory_grow.wast

// ./test/core/memory_grow.wast:1
let $0 = instantiate(`(module
  (memory 0)
  (func (export "grow") (param i32) (result i32) (memory.grow (local.get 0)))
)`);

// ./test/core/memory_grow.wast:6
assert_return(() => invoke($0, `grow`, [0]), [value("i32", 0)]);

// ./test/core/memory_grow.wast:7
assert_return(() => invoke($0, `grow`, [1]), [value("i32", 0)]);

// ./test/core/memory_grow.wast:8
assert_return(() => invoke($0, `grow`, [0]), [value("i32", 1)]);

// ./test/core/memory_grow.wast:9
assert_return(() => invoke($0, `grow`, [2]), [value("i32", 1)]);

// ./test/core/memory_grow.wast:10
assert_return(() => invoke($0, `grow`, [800]), [value("i32", 3)]);

// ./test/core/memory_grow.wast:11
assert_return(() => invoke($0, `grow`, [65536]), [value("i32", -1)]);

// ./test/core/memory_grow.wast:12
assert_return(() => invoke($0, `grow`, [64736]), [value("i32", -1)]);

// ./test/core/memory_grow.wast:13
assert_return(() => invoke($0, `grow`, [1]), [value("i32", 803)]);

// ./test/core/memory_grow.wast:15
let $1 = instantiate(`(module
  (memory 0 10)
  (func (export "grow") (param i32) (result i32) (memory.grow (local.get 0)))
)`);

// ./test/core/memory_grow.wast:20
assert_return(() => invoke($1, `grow`, [0]), [value("i32", 0)]);

// ./test/core/memory_grow.wast:21
assert_return(() => invoke($1, `grow`, [1]), [value("i32", 0)]);

// ./test/core/memory_grow.wast:22
assert_return(() => invoke($1, `grow`, [1]), [value("i32", 1)]);

// ./test/core/memory_grow.wast:23
assert_return(() => invoke($1, `grow`, [2]), [value("i32", 2)]);

// ./test/core/memory_grow.wast:24
assert_return(() => invoke($1, `grow`, [6]), [value("i32", 4)]);

// ./test/core/memory_grow.wast:25
assert_return(() => invoke($1, `grow`, [0]), [value("i32", 10)]);

// ./test/core/memory_grow.wast:26
assert_return(() => invoke($1, `grow`, [1]), [value("i32", -1)]);

// ./test/core/memory_grow.wast:27
assert_return(() => invoke($1, `grow`, [65536]), [value("i32", -1)]);

// ./test/core/memory_grow.wast:32
let $2 = instantiate(`(module
  (memory 1)
  (func (export "grow") (param i32) (result i32)
    (memory.grow (local.get 0))
  )
  (func (export "check-memory-zero") (param i32 i32) (result i32)
    (local i32)
    (local.set 2 (i32.const 1))
    (block
      (loop
        (local.set 2 (i32.load8_u (local.get 0)))
        (br_if 1 (i32.ne (local.get 2) (i32.const 0)))
        (br_if 1 (i32.ge_u (local.get 0) (local.get 1)))
        (local.set 0 (i32.add (local.get 0) (i32.const 1)))
        (br_if 0 (i32.le_u (local.get 0) (local.get 1)))
      )
    )
    (local.get 2)
  )
)`);

// ./test/core/memory_grow.wast:53
assert_return(() => invoke($2, `check-memory-zero`, [0, 65535]), [value("i32", 0)]);

// ./test/core/memory_grow.wast:54
assert_return(() => invoke($2, `grow`, [1]), [value("i32", 1)]);

// ./test/core/memory_grow.wast:55
assert_return(() => invoke($2, `check-memory-zero`, [65536, 131071]), [value("i32", 0)]);

// ./test/core/memory_grow.wast:56
assert_return(() => invoke($2, `grow`, [1]), [value("i32", 2)]);

// ./test/core/memory_grow.wast:57
assert_return(() => invoke($2, `check-memory-zero`, [131072, 196607]), [value("i32", 0)]);

// ./test/core/memory_grow.wast:58
assert_return(() => invoke($2, `grow`, [1]), [value("i32", 3)]);

// ./test/core/memory_grow.wast:59
assert_return(() => invoke($2, `check-memory-zero`, [196608, 262143]), [value("i32", 0)]);

// ./test/core/memory_grow.wast:60
assert_return(() => invoke($2, `grow`, [1]), [value("i32", 4)]);

// ./test/core/memory_grow.wast:61
assert_return(() => invoke($2, `check-memory-zero`, [262144, 327679]), [value("i32", 0)]);

// ./test/core/memory_grow.wast:62
assert_return(() => invoke($2, `grow`, [1]), [value("i32", 5)]);

// ./test/core/memory_grow.wast:63
assert_return(() => invoke($2, `check-memory-zero`, [327680, 393215]), [value("i32", 0)]);

// ./test/core/memory_grow.wast:68
let $3 = instantiate(`(module
  (memory 0)

  (func (export "load_at_zero") (result i32) (i32.load (i32.const 0)))
  (func (export "store_at_zero") (i32.store (i32.const 0) (i32.const 2)))

  (func (export "load_at_page_size") (result i32)
    (i32.load (i32.const 0x10000))
  )
  (func (export "store_at_page_size")
    (i32.store (i32.const 0x10000) (i32.const 3))
  )

  (func (export "grow") (param i32) (result i32) (memory.grow (local.get 0)))
  (func (export "size") (result i32) (memory.size))
)`);

// ./test/core/memory_grow.wast:85
assert_return(() => invoke($3, `size`, []), [value("i32", 0)]);

// ./test/core/memory_grow.wast:86
assert_trap(() => invoke($3, `store_at_zero`, []), `out of bounds memory access`);

// ./test/core/memory_grow.wast:87
assert_trap(() => invoke($3, `load_at_zero`, []), `out of bounds memory access`);

// ./test/core/memory_grow.wast:88
assert_trap(() => invoke($3, `store_at_page_size`, []), `out of bounds memory access`);

// ./test/core/memory_grow.wast:89
assert_trap(() => invoke($3, `load_at_page_size`, []), `out of bounds memory access`);

// ./test/core/memory_grow.wast:90
assert_return(() => invoke($3, `grow`, [1]), [value("i32", 0)]);

// ./test/core/memory_grow.wast:91
assert_return(() => invoke($3, `size`, []), [value("i32", 1)]);

// ./test/core/memory_grow.wast:92
assert_return(() => invoke($3, `load_at_zero`, []), [value("i32", 0)]);

// ./test/core/memory_grow.wast:93
assert_return(() => invoke($3, `store_at_zero`, []), []);

// ./test/core/memory_grow.wast:94
assert_return(() => invoke($3, `load_at_zero`, []), [value("i32", 2)]);

// ./test/core/memory_grow.wast:95
assert_trap(() => invoke($3, `store_at_page_size`, []), `out of bounds memory access`);

// ./test/core/memory_grow.wast:96
assert_trap(() => invoke($3, `load_at_page_size`, []), `out of bounds memory access`);

// ./test/core/memory_grow.wast:97
assert_return(() => invoke($3, `grow`, [4]), [value("i32", 1)]);

// ./test/core/memory_grow.wast:98
assert_return(() => invoke($3, `size`, []), [value("i32", 5)]);

// ./test/core/memory_grow.wast:99
assert_return(() => invoke($3, `load_at_zero`, []), [value("i32", 2)]);

// ./test/core/memory_grow.wast:100
assert_return(() => invoke($3, `store_at_zero`, []), []);

// ./test/core/memory_grow.wast:101
assert_return(() => invoke($3, `load_at_zero`, []), [value("i32", 2)]);

// ./test/core/memory_grow.wast:102
assert_return(() => invoke($3, `load_at_page_size`, []), [value("i32", 0)]);

// ./test/core/memory_grow.wast:103
assert_return(() => invoke($3, `store_at_page_size`, []), []);

// ./test/core/memory_grow.wast:104
assert_return(() => invoke($3, `load_at_page_size`, []), [value("i32", 3)]);

// ./test/core/memory_grow.wast:109
let $4 = instantiate(`(module
  (memory (export "mem1") 2 5)
  (memory (export "mem2") 0)
)`);

// ./test/core/memory_grow.wast:113
register($4, `M`);

// ./test/core/memory_grow.wast:115
let $5 = instantiate(`(module
  (memory $$mem1 (import "M" "mem1") 1 6)
  (memory $$mem2 (import "M" "mem2") 0)
  (memory $$mem3 3)
  (memory $$mem4 4 5)

  (func (export "size1") (result i32) (memory.size $$mem1))
  (func (export "size2") (result i32) (memory.size $$mem2))
  (func (export "size3") (result i32) (memory.size $$mem3))
  (func (export "size4") (result i32) (memory.size $$mem4))

  (func (export "grow1") (param i32) (result i32)
    (memory.grow $$mem1 (local.get 0))
  )
  (func (export "grow2") (param i32) (result i32)
    (memory.grow $$mem2 (local.get 0))
  )
  (func (export "grow3") (param i32) (result i32)
    (memory.grow $$mem3 (local.get 0))
  )
  (func (export "grow4") (param i32) (result i32)
    (memory.grow $$mem4 (local.get 0))
  )
)`);

// ./test/core/memory_grow.wast:140
assert_return(() => invoke($5, `size1`, []), [value("i32", 2)]);

// ./test/core/memory_grow.wast:141
assert_return(() => invoke($5, `size2`, []), [value("i32", 0)]);

// ./test/core/memory_grow.wast:142
assert_return(() => invoke($5, `size3`, []), [value("i32", 3)]);

// ./test/core/memory_grow.wast:143
assert_return(() => invoke($5, `size4`, []), [value("i32", 4)]);

// ./test/core/memory_grow.wast:145
assert_return(() => invoke($5, `grow1`, [1]), [value("i32", 2)]);

// ./test/core/memory_grow.wast:146
assert_return(() => invoke($5, `size1`, []), [value("i32", 3)]);

// ./test/core/memory_grow.wast:147
assert_return(() => invoke($5, `size2`, []), [value("i32", 0)]);

// ./test/core/memory_grow.wast:148
assert_return(() => invoke($5, `size3`, []), [value("i32", 3)]);

// ./test/core/memory_grow.wast:149
assert_return(() => invoke($5, `size4`, []), [value("i32", 4)]);

// ./test/core/memory_grow.wast:151
assert_return(() => invoke($5, `grow1`, [2]), [value("i32", 3)]);

// ./test/core/memory_grow.wast:152
assert_return(() => invoke($5, `size1`, []), [value("i32", 5)]);

// ./test/core/memory_grow.wast:153
assert_return(() => invoke($5, `size2`, []), [value("i32", 0)]);

// ./test/core/memory_grow.wast:154
assert_return(() => invoke($5, `size3`, []), [value("i32", 3)]);

// ./test/core/memory_grow.wast:155
assert_return(() => invoke($5, `size4`, []), [value("i32", 4)]);

// ./test/core/memory_grow.wast:157
assert_return(() => invoke($5, `grow1`, [1]), [value("i32", -1)]);

// ./test/core/memory_grow.wast:158
assert_return(() => invoke($5, `size1`, []), [value("i32", 5)]);

// ./test/core/memory_grow.wast:159
assert_return(() => invoke($5, `size2`, []), [value("i32", 0)]);

// ./test/core/memory_grow.wast:160
assert_return(() => invoke($5, `size3`, []), [value("i32", 3)]);

// ./test/core/memory_grow.wast:161
assert_return(() => invoke($5, `size4`, []), [value("i32", 4)]);

// ./test/core/memory_grow.wast:163
assert_return(() => invoke($5, `grow2`, [10]), [value("i32", 0)]);

// ./test/core/memory_grow.wast:164
assert_return(() => invoke($5, `size1`, []), [value("i32", 5)]);

// ./test/core/memory_grow.wast:165
assert_return(() => invoke($5, `size2`, []), [value("i32", 10)]);

// ./test/core/memory_grow.wast:166
assert_return(() => invoke($5, `size3`, []), [value("i32", 3)]);

// ./test/core/memory_grow.wast:167
assert_return(() => invoke($5, `size4`, []), [value("i32", 4)]);

// ./test/core/memory_grow.wast:169
assert_return(() => invoke($5, `grow3`, [268435456]), [value("i32", -1)]);

// ./test/core/memory_grow.wast:170
assert_return(() => invoke($5, `size1`, []), [value("i32", 5)]);

// ./test/core/memory_grow.wast:171
assert_return(() => invoke($5, `size2`, []), [value("i32", 10)]);

// ./test/core/memory_grow.wast:172
assert_return(() => invoke($5, `size3`, []), [value("i32", 3)]);

// ./test/core/memory_grow.wast:173
assert_return(() => invoke($5, `size4`, []), [value("i32", 4)]);

// ./test/core/memory_grow.wast:175
assert_return(() => invoke($5, `grow3`, [3]), [value("i32", 3)]);

// ./test/core/memory_grow.wast:176
assert_return(() => invoke($5, `size1`, []), [value("i32", 5)]);

// ./test/core/memory_grow.wast:177
assert_return(() => invoke($5, `size2`, []), [value("i32", 10)]);

// ./test/core/memory_grow.wast:178
assert_return(() => invoke($5, `size3`, []), [value("i32", 6)]);

// ./test/core/memory_grow.wast:179
assert_return(() => invoke($5, `size4`, []), [value("i32", 4)]);

// ./test/core/memory_grow.wast:181
assert_return(() => invoke($5, `grow4`, [1]), [value("i32", 4)]);

// ./test/core/memory_grow.wast:182
assert_return(() => invoke($5, `grow4`, [1]), [value("i32", -1)]);

// ./test/core/memory_grow.wast:183
assert_return(() => invoke($5, `size1`, []), [value("i32", 5)]);

// ./test/core/memory_grow.wast:184
assert_return(() => invoke($5, `size2`, []), [value("i32", 10)]);

// ./test/core/memory_grow.wast:185
assert_return(() => invoke($5, `size3`, []), [value("i32", 6)]);

// ./test/core/memory_grow.wast:186
assert_return(() => invoke($5, `size4`, []), [value("i32", 5)]);

// ./test/core/memory_grow.wast:191
let $6 = instantiate(`(module
  (memory 1)

  (func (export "as-br-value") (result i32)
    (block (result i32) (br 0 (memory.grow (i32.const 0))))
  )

  (func (export "as-br_if-cond")
    (block (br_if 0 (memory.grow (i32.const 0))))
  )
  (func (export "as-br_if-value") (result i32)
    (block (result i32)
      (drop (br_if 0 (memory.grow (i32.const 0)) (i32.const 1))) (i32.const 7)
    )
  )
  (func (export "as-br_if-value-cond") (result i32)
    (block (result i32)
      (drop (br_if 0 (i32.const 6) (memory.grow (i32.const 0)))) (i32.const 7)
    )
  )

  (func (export "as-br_table-index")
    (block (br_table 0 0 0 (memory.grow (i32.const 0))))
  )
  (func (export "as-br_table-value") (result i32)
    (block (result i32)
      (br_table 0 0 0 (memory.grow (i32.const 0)) (i32.const 1)) (i32.const 7)
    )
  )
  (func (export "as-br_table-value-index") (result i32)
    (block (result i32)
      (br_table 0 0 (i32.const 6) (memory.grow (i32.const 0))) (i32.const 7)
    )
  )

  (func (export "as-return-value") (result i32)
    (return (memory.grow (i32.const 0)))
  )

  (func (export "as-if-cond") (result i32)
    (if (result i32) (memory.grow (i32.const 0))
      (then (i32.const 0)) (else (i32.const 1))
    )
  )
  (func (export "as-if-then") (result i32)
    (if (result i32) (i32.const 1)
      (then (memory.grow (i32.const 0))) (else (i32.const 0))
    )
  )
  (func (export "as-if-else") (result i32)
    (if (result i32) (i32.const 0)
      (then (i32.const 0)) (else (memory.grow (i32.const 0)))
    )
  )

  (func (export "as-select-first") (param i32 i32) (result i32)
    (select (memory.grow (i32.const 0)) (local.get 0) (local.get 1))
  )
  (func (export "as-select-second") (param i32 i32) (result i32)
    (select (local.get 0) (memory.grow (i32.const 0)) (local.get 1))
  )
  (func (export "as-select-cond") (result i32)
    (select (i32.const 0) (i32.const 1) (memory.grow (i32.const 0)))
  )

  (func $$f (param i32 i32 i32) (result i32) (i32.const -1))
  (func (export "as-call-first") (result i32)
    (call $$f (memory.grow (i32.const 0)) (i32.const 2) (i32.const 3))
  )
  (func (export "as-call-mid") (result i32)
    (call $$f (i32.const 1) (memory.grow (i32.const 0)) (i32.const 3))
  )
  (func (export "as-call-last") (result i32)
    (call $$f (i32.const 1) (i32.const 2) (memory.grow (i32.const 0)))
  )

  (type $$sig (func (param i32 i32 i32) (result i32)))
  (table funcref (elem $$f))
  (func (export "as-call_indirect-first") (result i32)
    (call_indirect (type $$sig)
      (memory.grow (i32.const 0)) (i32.const 2) (i32.const 3) (i32.const 0)
    )
  )
  (func (export "as-call_indirect-mid") (result i32)
    (call_indirect (type $$sig)
      (i32.const 1) (memory.grow (i32.const 0)) (i32.const 3) (i32.const 0)
    )
  )
  (func (export "as-call_indirect-last") (result i32)
    (call_indirect (type $$sig)
      (i32.const 1) (i32.const 2) (memory.grow (i32.const 0)) (i32.const 0)
    )
  )
  (func (export "as-call_indirect-index") (result i32)
    (call_indirect (type $$sig)
      (i32.const 1) (i32.const 2) (i32.const 3) (memory.grow (i32.const 0))
    )
  )

  (func (export "as-local.set-value") (local i32)
    (local.set 0 (memory.grow (i32.const 0)))
  )
  (func (export "as-local.tee-value") (result i32) (local i32)
    (local.tee 0 (memory.grow (i32.const 0)))
  )
  (global $$g (mut i32) (i32.const 0))
  (func (export "as-global.set-value") (local i32)
    (global.set $$g (memory.grow (i32.const 0)))
  )

  (func (export "as-load-address") (result i32)
    (i32.load (memory.grow (i32.const 0)))
  )
  (func (export "as-loadN-address") (result i32)
    (i32.load8_s (memory.grow (i32.const 0)))
  )

  (func (export "as-store-address")
    (i32.store (memory.grow (i32.const 0)) (i32.const 7))
  )
  (func (export "as-store-value")
    (i32.store (i32.const 2) (memory.grow (i32.const 0)))
  )

  (func (export "as-storeN-address")
    (i32.store8 (memory.grow (i32.const 0)) (i32.const 7))
  )
  (func (export "as-storeN-value")
    (i32.store16 (i32.const 2) (memory.grow (i32.const 0)))
  )

  (func (export "as-unary-operand") (result i32)
    (i32.clz (memory.grow (i32.const 0)))
  )

  (func (export "as-binary-left") (result i32)
    (i32.add (memory.grow (i32.const 0)) (i32.const 10))
  )
  (func (export "as-binary-right") (result i32)
    (i32.sub (i32.const 10) (memory.grow (i32.const 0)))
  )

  (func (export "as-test-operand") (result i32)
    (i32.eqz (memory.grow (i32.const 0)))
  )

  (func (export "as-compare-left") (result i32)
    (i32.le_s (memory.grow (i32.const 0)) (i32.const 10))
  )
  (func (export "as-compare-right") (result i32)
    (i32.ne (i32.const 10) (memory.grow (i32.const 0)))
  )

  (func (export "as-memory.grow-size") (result i32)
    (memory.grow (memory.grow (i32.const 0)))
  )
)`);

// ./test/core/memory_grow.wast:349
assert_return(() => invoke($6, `as-br-value`, []), [value("i32", 1)]);

// ./test/core/memory_grow.wast:351
assert_return(() => invoke($6, `as-br_if-cond`, []), []);

// ./test/core/memory_grow.wast:352
assert_return(() => invoke($6, `as-br_if-value`, []), [value("i32", 1)]);

// ./test/core/memory_grow.wast:353
assert_return(() => invoke($6, `as-br_if-value-cond`, []), [value("i32", 6)]);

// ./test/core/memory_grow.wast:355
assert_return(() => invoke($6, `as-br_table-index`, []), []);

// ./test/core/memory_grow.wast:356
assert_return(() => invoke($6, `as-br_table-value`, []), [value("i32", 1)]);

// ./test/core/memory_grow.wast:357
assert_return(() => invoke($6, `as-br_table-value-index`, []), [value("i32", 6)]);

// ./test/core/memory_grow.wast:359
assert_return(() => invoke($6, `as-return-value`, []), [value("i32", 1)]);

// ./test/core/memory_grow.wast:361
assert_return(() => invoke($6, `as-if-cond`, []), [value("i32", 0)]);

// ./test/core/memory_grow.wast:362
assert_return(() => invoke($6, `as-if-then`, []), [value("i32", 1)]);

// ./test/core/memory_grow.wast:363
assert_return(() => invoke($6, `as-if-else`, []), [value("i32", 1)]);

// ./test/core/memory_grow.wast:365
assert_return(() => invoke($6, `as-select-first`, [0, 1]), [value("i32", 1)]);

// ./test/core/memory_grow.wast:366
assert_return(() => invoke($6, `as-select-second`, [0, 0]), [value("i32", 1)]);

// ./test/core/memory_grow.wast:367
assert_return(() => invoke($6, `as-select-cond`, []), [value("i32", 0)]);

// ./test/core/memory_grow.wast:369
assert_return(() => invoke($6, `as-call-first`, []), [value("i32", -1)]);

// ./test/core/memory_grow.wast:370
assert_return(() => invoke($6, `as-call-mid`, []), [value("i32", -1)]);

// ./test/core/memory_grow.wast:371
assert_return(() => invoke($6, `as-call-last`, []), [value("i32", -1)]);

// ./test/core/memory_grow.wast:373
assert_return(() => invoke($6, `as-call_indirect-first`, []), [value("i32", -1)]);

// ./test/core/memory_grow.wast:374
assert_return(() => invoke($6, `as-call_indirect-mid`, []), [value("i32", -1)]);

// ./test/core/memory_grow.wast:375
assert_return(() => invoke($6, `as-call_indirect-last`, []), [value("i32", -1)]);

// ./test/core/memory_grow.wast:376
assert_trap(() => invoke($6, `as-call_indirect-index`, []), `undefined element`);

// ./test/core/memory_grow.wast:378
assert_return(() => invoke($6, `as-local.set-value`, []), []);

// ./test/core/memory_grow.wast:379
assert_return(() => invoke($6, `as-local.tee-value`, []), [value("i32", 1)]);

// ./test/core/memory_grow.wast:380
assert_return(() => invoke($6, `as-global.set-value`, []), []);

// ./test/core/memory_grow.wast:382
assert_return(() => invoke($6, `as-load-address`, []), [value("i32", 0)]);

// ./test/core/memory_grow.wast:383
assert_return(() => invoke($6, `as-loadN-address`, []), [value("i32", 0)]);

// ./test/core/memory_grow.wast:384
assert_return(() => invoke($6, `as-store-address`, []), []);

// ./test/core/memory_grow.wast:385
assert_return(() => invoke($6, `as-store-value`, []), []);

// ./test/core/memory_grow.wast:386
assert_return(() => invoke($6, `as-storeN-address`, []), []);

// ./test/core/memory_grow.wast:387
assert_return(() => invoke($6, `as-storeN-value`, []), []);

// ./test/core/memory_grow.wast:389
assert_return(() => invoke($6, `as-unary-operand`, []), [value("i32", 31)]);

// ./test/core/memory_grow.wast:391
assert_return(() => invoke($6, `as-binary-left`, []), [value("i32", 11)]);

// ./test/core/memory_grow.wast:392
assert_return(() => invoke($6, `as-binary-right`, []), [value("i32", 9)]);

// ./test/core/memory_grow.wast:394
assert_return(() => invoke($6, `as-test-operand`, []), [value("i32", 0)]);

// ./test/core/memory_grow.wast:396
assert_return(() => invoke($6, `as-compare-left`, []), [value("i32", 1)]);

// ./test/core/memory_grow.wast:397
assert_return(() => invoke($6, `as-compare-right`, []), [value("i32", 1)]);

// ./test/core/memory_grow.wast:399
assert_return(() => invoke($6, `as-memory.grow-size`, []), [value("i32", 1)]);

// ./test/core/memory_grow.wast:404
let $7 = instantiate(`(module
  (memory $$mem1 1)
  (memory $$mem2 2)

  (func (export "grow1") (param i32) (result i32)
    (memory.grow $$mem1 (local.get 0))
  )
  (func (export "grow2") (param i32) (result i32)
    (memory.grow $$mem2 (local.get 0))
  )

  (func (export "size1") (result i32) (memory.size $$mem1))
  (func (export "size2") (result i32) (memory.size $$mem2))
)`);

// ./test/core/memory_grow.wast:419
assert_return(() => invoke($7, `size1`, []), [value("i32", 1)]);

// ./test/core/memory_grow.wast:420
assert_return(() => invoke($7, `size2`, []), [value("i32", 2)]);

// ./test/core/memory_grow.wast:421
assert_return(() => invoke($7, `grow1`, [3]), [value("i32", 1)]);

// ./test/core/memory_grow.wast:422
assert_return(() => invoke($7, `grow1`, [4]), [value("i32", 4)]);

// ./test/core/memory_grow.wast:423
assert_return(() => invoke($7, `grow1`, [1]), [value("i32", 8)]);

// ./test/core/memory_grow.wast:424
assert_return(() => invoke($7, `grow2`, [1]), [value("i32", 2)]);

// ./test/core/memory_grow.wast:425
assert_return(() => invoke($7, `grow2`, [1]), [value("i32", 3)]);

// ./test/core/memory_grow.wast:430
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-i32-vs-f32 (result i32)
      (memory.grow (f32.const 0))
    )
  )`),
  `type mismatch`,
);

// ./test/core/memory_grow.wast:439
assert_invalid(
  () => instantiate(`(module
    (memory 0)
    (func $$type-size-empty-vs-i32 (result i32)
      (memory.grow)
    )
  )`),
  `type mismatch`,
);

// ./test/core/memory_grow.wast:448
assert_invalid(
  () => instantiate(`(module
    (memory 0)
    (func $$type-size-empty-vs-i32-in-block (result i32)
      (i32.const 0)
      (block (result i32) (memory.grow))
    )
  )`),
  `type mismatch`,
);

// ./test/core/memory_grow.wast:458
assert_invalid(
  () => instantiate(`(module
    (memory 0)
    (func $$type-size-empty-vs-i32-in-loop (result i32)
      (i32.const 0)
      (loop (result i32) (memory.grow))
    )
  )`),
  `type mismatch`,
);

// ./test/core/memory_grow.wast:468
assert_invalid(
  () => instantiate(`(module
    (memory 0)
    (func $$type-size-empty-vs-i32-in-then (result i32)
      (i32.const 0) (i32.const 0)
      (if (result i32) (then (memory.grow)))
    )
  )`),
  `type mismatch`,
);

// ./test/core/memory_grow.wast:479
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-result-i32-vs-empty
      (memory.grow (i32.const 1))
    )
  )`),
  `type mismatch`,
);

// ./test/core/memory_grow.wast:488
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-size-f32-vs-i32 (result i32)
      (memory.grow (f32.const 0))
    )
  )`),
  `type mismatch`,
);

// ./test/core/memory_grow.wast:498
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-result-i32-vs-empty
      (memory.grow (i32.const 0))
    )
  )`),
  `type mismatch`,
);

// ./test/core/memory_grow.wast:507
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (func $$type-result-i32-vs-f32 (result f32)
      (memory.grow (i32.const 0))
    )
  )`),
  `type mismatch`,
);
