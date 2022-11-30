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

// ./test/core/memory64.wast

// ./test/core/memory64.wast:3
let $0 = instantiate(`(module (memory i64 0 0))`);

// ./test/core/memory64.wast:4
let $1 = instantiate(`(module (memory i64 0 1))`);

// ./test/core/memory64.wast:5
let $2 = instantiate(`(module (memory i64 1 256))`);

// ./test/core/memory64.wast:6
let $3 = instantiate(`(module (memory i64 0 65536))`);

// ./test/core/memory64.wast:8
assert_invalid(
  () => instantiate(`(module (memory i64 0) (memory i64 0))`),
  `multiple memories`,
);

// ./test/core/memory64.wast:9
assert_invalid(
  () => instantiate(`(module (memory (import "spectest" "memory") i64 0) (memory i64 0))`),
  `multiple memories`,
);

// ./test/core/memory64.wast:11
let $4 = instantiate(`(module (memory i64 (data)) (func (export "memsize") (result i64) (memory.size)))`);

// ./test/core/memory64.wast:12
assert_return(() => invoke($4, `memsize`, []), [value("i64", 0n)]);

// ./test/core/memory64.wast:13
let $5 = instantiate(`(module (memory i64 (data "")) (func (export "memsize") (result i64) (memory.size)))`);

// ./test/core/memory64.wast:14
assert_return(() => invoke($5, `memsize`, []), [value("i64", 0n)]);

// ./test/core/memory64.wast:15
let $6 = instantiate(`(module (memory i64 (data "x")) (func (export "memsize") (result i64) (memory.size)))`);

// ./test/core/memory64.wast:16
assert_return(() => invoke($6, `memsize`, []), [value("i64", 1n)]);

// ./test/core/memory64.wast:18
assert_invalid(() => instantiate(`(module (data (i64.const 0)))`), `unknown memory`);

// ./test/core/memory64.wast:19
assert_invalid(() => instantiate(`(module (data (i64.const 0) ""))`), `unknown memory`);

// ./test/core/memory64.wast:20
assert_invalid(() => instantiate(`(module (data (i64.const 0) "x"))`), `unknown memory`);

// ./test/core/memory64.wast:22
assert_invalid(
  () => instantiate(`(module (func (drop (f32.load (i64.const 0)))))`),
  `unknown memory`,
);

// ./test/core/memory64.wast:26
assert_invalid(
  () => instantiate(`(module (func (f32.store (i64.const 0) (f32.const 0))))`),
  `unknown memory`,
);

// ./test/core/memory64.wast:30
assert_invalid(
  () => instantiate(`(module (func (drop (i32.load8_s (i64.const 0)))))`),
  `unknown memory`,
);

// ./test/core/memory64.wast:34
assert_invalid(
  () => instantiate(`(module (func (i32.store8 (i64.const 0) (i32.const 0))))`),
  `unknown memory`,
);

// ./test/core/memory64.wast:38
assert_invalid(
  () => instantiate(`(module (func (drop (memory.size))))`),
  `unknown memory`,
);

// ./test/core/memory64.wast:42
assert_invalid(
  () => instantiate(`(module (func (drop (memory.grow (i64.const 0)))))`),
  `unknown memory`,
);

// ./test/core/memory64.wast:48
assert_invalid(
  () => instantiate(`(module (memory i64 1 0))`),
  `size minimum must not be greater than maximum`,
);

// ./test/core/memory64.wast:53
let $7 = instantiate(`(module
  (memory i64 1)
  (data (i64.const 0) "ABC\\a7D") (data (i64.const 20) "WASM")

  ;; Data section
  (func (export "data") (result i32)
    (i32.and
      (i32.and
        (i32.and
          (i32.eq (i32.load8_u (i64.const 0)) (i32.const 65))
          (i32.eq (i32.load8_u (i64.const 3)) (i32.const 167))
        )
        (i32.and
          (i32.eq (i32.load8_u (i64.const 6)) (i32.const 0))
          (i32.eq (i32.load8_u (i64.const 19)) (i32.const 0))
        )
      )
      (i32.and
        (i32.and
          (i32.eq (i32.load8_u (i64.const 20)) (i32.const 87))
          (i32.eq (i32.load8_u (i64.const 23)) (i32.const 77))
        )
        (i32.and
          (i32.eq (i32.load8_u (i64.const 24)) (i32.const 0))
          (i32.eq (i32.load8_u (i64.const 1023)) (i32.const 0))
        )
      )
    )
  )

  ;; Memory cast
  (func (export "cast") (result f64)
    (i64.store (i64.const 8) (i64.const -12345))
    (if
      (f64.eq
        (f64.load (i64.const 8))
        (f64.reinterpret_i64 (i64.const -12345))
      )
      (then (return (f64.const 0)))
    )
    (i64.store align=1 (i64.const 9) (i64.const 0))
    (i32.store16 align=1 (i64.const 15) (i32.const 16453))
    (f64.load align=1 (i64.const 9))
  )

  ;; Sign and zero extending memory loads
  (func (export "i32_load8_s") (param $$i i32) (result i32)
	(i32.store8 (i64.const 8) (local.get $$i))
	(i32.load8_s (i64.const 8))
  )
  (func (export "i32_load8_u") (param $$i i32) (result i32)
	(i32.store8 (i64.const 8) (local.get $$i))
	(i32.load8_u (i64.const 8))
  )
  (func (export "i32_load16_s") (param $$i i32) (result i32)
	(i32.store16 (i64.const 8) (local.get $$i))
	(i32.load16_s (i64.const 8))
  )
  (func (export "i32_load16_u") (param $$i i32) (result i32)
	(i32.store16 (i64.const 8) (local.get $$i))
	(i32.load16_u (i64.const 8))
  )
  (func (export "i64_load8_s") (param $$i i64) (result i64)
	(i64.store8 (i64.const 8) (local.get $$i))
	(i64.load8_s (i64.const 8))
  )
  (func (export "i64_load8_u") (param $$i i64) (result i64)
	(i64.store8 (i64.const 8) (local.get $$i))
	(i64.load8_u (i64.const 8))
  )
  (func (export "i64_load16_s") (param $$i i64) (result i64)
	(i64.store16 (i64.const 8) (local.get $$i))
	(i64.load16_s (i64.const 8))
  )
  (func (export "i64_load16_u") (param $$i i64) (result i64)
	(i64.store16 (i64.const 8) (local.get $$i))
	(i64.load16_u (i64.const 8))
  )
  (func (export "i64_load32_s") (param $$i i64) (result i64)
	(i64.store32 (i64.const 8) (local.get $$i))
	(i64.load32_s (i64.const 8))
  )
  (func (export "i64_load32_u") (param $$i i64) (result i64)
	(i64.store32 (i64.const 8) (local.get $$i))
	(i64.load32_u (i64.const 8))
  )
)`);

// ./test/core/memory64.wast:141
assert_return(() => invoke($7, `data`, []), [value("i32", 1)]);

// ./test/core/memory64.wast:142
assert_return(() => invoke($7, `cast`, []), [value("f64", 42)]);

// ./test/core/memory64.wast:144
assert_return(() => invoke($7, `i32_load8_s`, [-1]), [value("i32", -1)]);

// ./test/core/memory64.wast:145
assert_return(() => invoke($7, `i32_load8_u`, [-1]), [value("i32", 255)]);

// ./test/core/memory64.wast:146
assert_return(() => invoke($7, `i32_load16_s`, [-1]), [value("i32", -1)]);

// ./test/core/memory64.wast:147
assert_return(() => invoke($7, `i32_load16_u`, [-1]), [value("i32", 65535)]);

// ./test/core/memory64.wast:149
assert_return(() => invoke($7, `i32_load8_s`, [100]), [value("i32", 100)]);

// ./test/core/memory64.wast:150
assert_return(() => invoke($7, `i32_load8_u`, [200]), [value("i32", 200)]);

// ./test/core/memory64.wast:151
assert_return(() => invoke($7, `i32_load16_s`, [20000]), [value("i32", 20000)]);

// ./test/core/memory64.wast:152
assert_return(() => invoke($7, `i32_load16_u`, [40000]), [value("i32", 40000)]);

// ./test/core/memory64.wast:154
assert_return(() => invoke($7, `i32_load8_s`, [-19110589]), [value("i32", 67)]);

// ./test/core/memory64.wast:155
assert_return(() => invoke($7, `i32_load8_s`, [878104047]), [value("i32", -17)]);

// ./test/core/memory64.wast:156
assert_return(() => invoke($7, `i32_load8_u`, [-19110589]), [value("i32", 67)]);

// ./test/core/memory64.wast:157
assert_return(() => invoke($7, `i32_load8_u`, [878104047]), [value("i32", 239)]);

// ./test/core/memory64.wast:158
assert_return(() => invoke($7, `i32_load16_s`, [-19110589]), [value("i32", 25923)]);

// ./test/core/memory64.wast:159
assert_return(() => invoke($7, `i32_load16_s`, [878104047]), [value("i32", -12817)]);

// ./test/core/memory64.wast:160
assert_return(() => invoke($7, `i32_load16_u`, [-19110589]), [value("i32", 25923)]);

// ./test/core/memory64.wast:161
assert_return(() => invoke($7, `i32_load16_u`, [878104047]), [value("i32", 52719)]);

// ./test/core/memory64.wast:163
assert_return(() => invoke($7, `i64_load8_s`, [-1n]), [value("i64", -1n)]);

// ./test/core/memory64.wast:164
assert_return(() => invoke($7, `i64_load8_u`, [-1n]), [value("i64", 255n)]);

// ./test/core/memory64.wast:165
assert_return(() => invoke($7, `i64_load16_s`, [-1n]), [value("i64", -1n)]);

// ./test/core/memory64.wast:166
assert_return(() => invoke($7, `i64_load16_u`, [-1n]), [value("i64", 65535n)]);

// ./test/core/memory64.wast:167
assert_return(() => invoke($7, `i64_load32_s`, [-1n]), [value("i64", -1n)]);

// ./test/core/memory64.wast:168
assert_return(() => invoke($7, `i64_load32_u`, [-1n]), [value("i64", 4294967295n)]);

// ./test/core/memory64.wast:170
assert_return(() => invoke($7, `i64_load8_s`, [100n]), [value("i64", 100n)]);

// ./test/core/memory64.wast:171
assert_return(() => invoke($7, `i64_load8_u`, [200n]), [value("i64", 200n)]);

// ./test/core/memory64.wast:172
assert_return(() => invoke($7, `i64_load16_s`, [20000n]), [value("i64", 20000n)]);

// ./test/core/memory64.wast:173
assert_return(() => invoke($7, `i64_load16_u`, [40000n]), [value("i64", 40000n)]);

// ./test/core/memory64.wast:174
assert_return(() => invoke($7, `i64_load32_s`, [20000n]), [value("i64", 20000n)]);

// ./test/core/memory64.wast:175
assert_return(() => invoke($7, `i64_load32_u`, [40000n]), [value("i64", 40000n)]);

// ./test/core/memory64.wast:177
assert_return(() => invoke($7, `i64_load8_s`, [-81985529755441853n]), [value("i64", 67n)]);

// ./test/core/memory64.wast:178
assert_return(() => invoke($7, `i64_load8_s`, [3771275841602506223n]), [value("i64", -17n)]);

// ./test/core/memory64.wast:179
assert_return(() => invoke($7, `i64_load8_u`, [-81985529755441853n]), [value("i64", 67n)]);

// ./test/core/memory64.wast:180
assert_return(() => invoke($7, `i64_load8_u`, [3771275841602506223n]), [value("i64", 239n)]);

// ./test/core/memory64.wast:181
assert_return(() => invoke($7, `i64_load16_s`, [-81985529755441853n]), [value("i64", 25923n)]);

// ./test/core/memory64.wast:182
assert_return(() => invoke($7, `i64_load16_s`, [3771275841602506223n]), [value("i64", -12817n)]);

// ./test/core/memory64.wast:183
assert_return(() => invoke($7, `i64_load16_u`, [-81985529755441853n]), [value("i64", 25923n)]);

// ./test/core/memory64.wast:184
assert_return(() => invoke($7, `i64_load16_u`, [3771275841602506223n]), [value("i64", 52719n)]);

// ./test/core/memory64.wast:185
assert_return(() => invoke($7, `i64_load32_s`, [-81985529755441853n]), [value("i64", 1446274371n)]);

// ./test/core/memory64.wast:186
assert_return(() => invoke($7, `i64_load32_s`, [3771275841602506223n]), [value("i64", -1732588049n)]);

// ./test/core/memory64.wast:187
assert_return(() => invoke($7, `i64_load32_u`, [-81985529755441853n]), [value("i64", 1446274371n)]);

// ./test/core/memory64.wast:188
assert_return(() => invoke($7, `i64_load32_u`, [3771275841602506223n]), [value("i64", 2562379247n)]);
