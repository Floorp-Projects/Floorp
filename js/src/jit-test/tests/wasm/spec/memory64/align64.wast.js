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

// ./test/core/align64.wast

// ./test/core/align64.wast:3
let $0 = instantiate(`(module (memory i64 0) (func (drop (i32.load8_s align=1 (i64.const 0)))))`);

// ./test/core/align64.wast:4
let $1 = instantiate(`(module (memory i64 0) (func (drop (i32.load8_u align=1 (i64.const 0)))))`);

// ./test/core/align64.wast:5
let $2 = instantiate(`(module (memory i64 0) (func (drop (i32.load16_s align=2 (i64.const 0)))))`);

// ./test/core/align64.wast:6
let $3 = instantiate(`(module (memory i64 0) (func (drop (i32.load16_u align=2 (i64.const 0)))))`);

// ./test/core/align64.wast:7
let $4 = instantiate(`(module (memory i64 0) (func (drop (i32.load align=4 (i64.const 0)))))`);

// ./test/core/align64.wast:8
let $5 = instantiate(`(module (memory i64 0) (func (drop (i64.load8_s align=1 (i64.const 0)))))`);

// ./test/core/align64.wast:9
let $6 = instantiate(`(module (memory i64 0) (func (drop (i64.load8_u align=1 (i64.const 0)))))`);

// ./test/core/align64.wast:10
let $7 = instantiate(`(module (memory i64 0) (func (drop (i64.load16_s align=2 (i64.const 0)))))`);

// ./test/core/align64.wast:11
let $8 = instantiate(`(module (memory i64 0) (func (drop (i64.load16_u align=2 (i64.const 0)))))`);

// ./test/core/align64.wast:12
let $9 = instantiate(`(module (memory i64 0) (func (drop (i64.load32_s align=4 (i64.const 0)))))`);

// ./test/core/align64.wast:13
let $10 = instantiate(`(module (memory i64 0) (func (drop (i64.load32_u align=4 (i64.const 0)))))`);

// ./test/core/align64.wast:14
let $11 = instantiate(`(module (memory i64 0) (func (drop (i64.load align=8 (i64.const 0)))))`);

// ./test/core/align64.wast:15
let $12 = instantiate(`(module (memory i64 0) (func (drop (f32.load align=4 (i64.const 0)))))`);

// ./test/core/align64.wast:16
let $13 = instantiate(`(module (memory i64 0) (func (drop (f64.load align=8 (i64.const 0)))))`);

// ./test/core/align64.wast:17
let $14 = instantiate(`(module (memory i64 0) (func (i32.store8 align=1 (i64.const 0) (i32.const 1))))`);

// ./test/core/align64.wast:18
let $15 = instantiate(`(module (memory i64 0) (func (i32.store16 align=2 (i64.const 0) (i32.const 1))))`);

// ./test/core/align64.wast:19
let $16 = instantiate(`(module (memory i64 0) (func (i32.store align=4 (i64.const 0) (i32.const 1))))`);

// ./test/core/align64.wast:20
let $17 = instantiate(`(module (memory i64 0) (func (i64.store8 align=1 (i64.const 0) (i64.const 1))))`);

// ./test/core/align64.wast:21
let $18 = instantiate(`(module (memory i64 0) (func (i64.store16 align=2 (i64.const 0) (i64.const 1))))`);

// ./test/core/align64.wast:22
let $19 = instantiate(`(module (memory i64 0) (func (i64.store32 align=4 (i64.const 0) (i64.const 1))))`);

// ./test/core/align64.wast:23
let $20 = instantiate(`(module (memory i64 0) (func (i64.store align=8 (i64.const 0) (i64.const 1))))`);

// ./test/core/align64.wast:24
let $21 = instantiate(`(module (memory i64 0) (func (f32.store align=4 (i64.const 0) (f32.const 1.0))))`);

// ./test/core/align64.wast:25
let $22 = instantiate(`(module (memory i64 0) (func (f64.store align=8 (i64.const 0) (f64.const 1.0))))`);

// ./test/core/align64.wast:27
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (drop (i32.load8_s align=0 (i64.const 0))))) `),
  `alignment`,
);

// ./test/core/align64.wast:33
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (drop (i32.load8_s align=7 (i64.const 0))))) `),
  `alignment`,
);

// ./test/core/align64.wast:39
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (drop (i32.load8_u align=0 (i64.const 0))))) `),
  `alignment`,
);

// ./test/core/align64.wast:45
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (drop (i32.load8_u align=7 (i64.const 0))))) `),
  `alignment`,
);

// ./test/core/align64.wast:51
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (drop (i32.load16_s align=0 (i64.const 0))))) `),
  `alignment`,
);

// ./test/core/align64.wast:57
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (drop (i32.load16_s align=7 (i64.const 0))))) `),
  `alignment`,
);

// ./test/core/align64.wast:63
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (drop (i32.load16_u align=0 (i64.const 0))))) `),
  `alignment`,
);

// ./test/core/align64.wast:69
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (drop (i32.load16_u align=7 (i64.const 0))))) `),
  `alignment`,
);

// ./test/core/align64.wast:75
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (drop (i32.load align=0 (i64.const 0))))) `),
  `alignment`,
);

// ./test/core/align64.wast:81
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (drop (i32.load align=7 (i64.const 0))))) `),
  `alignment`,
);

// ./test/core/align64.wast:87
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (drop (i64.load8_s align=0 (i64.const 0))))) `),
  `alignment`,
);

// ./test/core/align64.wast:93
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (drop (i64.load8_s align=7 (i64.const 0))))) `),
  `alignment`,
);

// ./test/core/align64.wast:99
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (drop (i64.load8_u align=0 (i64.const 0))))) `),
  `alignment`,
);

// ./test/core/align64.wast:105
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (drop (i64.load8_u align=7 (i64.const 0))))) `),
  `alignment`,
);

// ./test/core/align64.wast:111
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (drop (i64.load16_s align=0 (i64.const 0))))) `),
  `alignment`,
);

// ./test/core/align64.wast:117
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (drop (i64.load16_s align=7 (i64.const 0))))) `),
  `alignment`,
);

// ./test/core/align64.wast:123
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (drop (i64.load16_u align=0 (i64.const 0))))) `),
  `alignment`,
);

// ./test/core/align64.wast:129
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (drop (i64.load16_u align=7 (i64.const 0))))) `),
  `alignment`,
);

// ./test/core/align64.wast:135
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (drop (i64.load32_s align=0 (i64.const 0))))) `),
  `alignment`,
);

// ./test/core/align64.wast:141
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (drop (i64.load32_s align=7 (i64.const 0))))) `),
  `alignment`,
);

// ./test/core/align64.wast:147
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (drop (i64.load32_u align=0 (i64.const 0))))) `),
  `alignment`,
);

// ./test/core/align64.wast:153
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (drop (i64.load32_u align=7 (i64.const 0))))) `),
  `alignment`,
);

// ./test/core/align64.wast:159
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (drop (i64.load align=0 (i64.const 0))))) `),
  `alignment`,
);

// ./test/core/align64.wast:165
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (drop (i64.load align=7 (i64.const 0))))) `),
  `alignment`,
);

// ./test/core/align64.wast:171
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (drop (f32.load align=0 (i64.const 0))))) `),
  `alignment`,
);

// ./test/core/align64.wast:177
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (drop (f32.load align=7 (i64.const 0))))) `),
  `alignment`,
);

// ./test/core/align64.wast:183
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (drop (f64.load align=0 (i64.const 0))))) `),
  `alignment`,
);

// ./test/core/align64.wast:189
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (drop (f64.load align=7 (i64.const 0))))) `),
  `alignment`,
);

// ./test/core/align64.wast:196
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (i32.store8 align=0 (i64.const 0) (i32.const 0)))) `),
  `alignment`,
);

// ./test/core/align64.wast:202
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (i32.store8 align=7 (i64.const 0) (i32.const 0)))) `),
  `alignment`,
);

// ./test/core/align64.wast:208
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (i32.store16 align=0 (i64.const 0) (i32.const 0)))) `),
  `alignment`,
);

// ./test/core/align64.wast:214
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (i32.store16 align=7 (i64.const 0) (i32.const 0)))) `),
  `alignment`,
);

// ./test/core/align64.wast:220
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (i32.store align=0 (i64.const 0) (i32.const 0)))) `),
  `alignment`,
);

// ./test/core/align64.wast:226
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (i32.store align=7 (i64.const 0) (i32.const 0)))) `),
  `alignment`,
);

// ./test/core/align64.wast:232
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (i64.store8 align=0 (i64.const 0) (i64.const 0)))) `),
  `alignment`,
);

// ./test/core/align64.wast:238
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (i64.store8 align=7 (i64.const 0) (i64.const 0)))) `),
  `alignment`,
);

// ./test/core/align64.wast:244
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (i64.store16 align=0 (i64.const 0) (i64.const 0)))) `),
  `alignment`,
);

// ./test/core/align64.wast:250
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (i64.store16 align=7 (i64.const 0) (i64.const 0)))) `),
  `alignment`,
);

// ./test/core/align64.wast:256
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (i64.store32 align=0 (i64.const 0) (i64.const 0)))) `),
  `alignment`,
);

// ./test/core/align64.wast:262
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (i64.store32 align=7 (i64.const 0) (i64.const 0)))) `),
  `alignment`,
);

// ./test/core/align64.wast:268
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (i64.store align=0 (i64.const 0) (i64.const 0)))) `),
  `alignment`,
);

// ./test/core/align64.wast:274
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (i64.store align=7 (i64.const 0) (i64.const 0)))) `),
  `alignment`,
);

// ./test/core/align64.wast:280
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (f32.store align=0 (i64.const 0) (f32.const 0)))) `),
  `alignment`,
);

// ./test/core/align64.wast:286
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (f32.store align=7 (i64.const 0) (f32.const 0)))) `),
  `alignment`,
);

// ./test/core/align64.wast:292
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (f64.store align=0 (i64.const 0) (f32.const 0)))) `),
  `alignment`,
);

// ./test/core/align64.wast:298
assert_malformed(
  () => instantiate(`(module (memory i64 0) (func (f64.store align=7 (i64.const 0) (f32.const 0)))) `),
  `alignment`,
);

// ./test/core/align64.wast:305
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (drop (i32.load8_s align=2 (i64.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:309
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (drop (i32.load8_u align=2 (i64.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:313
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (drop (i32.load16_s align=4 (i64.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:317
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (drop (i32.load16_u align=4 (i64.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:321
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (drop (i32.load align=8 (i64.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:325
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (drop (i64.load8_s align=2 (i64.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:329
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (drop (i64.load8_u align=2 (i64.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:333
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (drop (i64.load16_s align=4 (i64.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:337
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (drop (i64.load16_u align=4 (i64.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:341
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (drop (i64.load32_s align=8 (i64.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:345
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (drop (i64.load32_u align=8 (i64.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:349
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (drop (i64.load align=16 (i64.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:353
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (drop (f32.load align=8 (i64.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:357
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (drop (f64.load align=16 (i64.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:362
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (drop (i32.load8_s align=2 (i64.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:366
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (drop (i32.load8_u align=2 (i64.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:370
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (drop (i32.load16_s align=4 (i64.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:374
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (drop (i32.load16_u align=4 (i64.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:378
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (drop (i32.load align=8 (i64.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:382
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (drop (i64.load8_s align=2 (i64.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:386
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (drop (i64.load8_u align=2 (i64.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:390
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (drop (i64.load16_s align=4 (i64.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:394
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (drop (i64.load16_u align=4 (i64.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:398
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (drop (i64.load32_s align=8 (i64.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:402
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (drop (i64.load32_u align=8 (i64.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:406
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (drop (i64.load align=16 (i64.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:410
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (drop (f32.load align=8 (i64.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:414
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (drop (f64.load align=16 (i64.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:419
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (i32.store8 align=2 (i64.const 0) (i32.const 0))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:423
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (i32.store16 align=4 (i64.const 0) (i32.const 0))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:427
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (i32.store align=8 (i64.const 0) (i32.const 0))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:431
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (i64.store8 align=2 (i64.const 0) (i64.const 0))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:435
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (i64.store16 align=4 (i64.const 0) (i64.const 0))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:439
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (i64.store32 align=8 (i64.const 0) (i64.const 0))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:443
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (i64.store align=16 (i64.const 0) (i64.const 0))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:447
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (f32.store align=8 (i64.const 0) (f32.const 0))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:451
assert_invalid(
  () => instantiate(`(module (memory i64 0) (func (f64.store align=16 (i64.const 0) (f64.const 0))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align64.wast:458
let $23 = instantiate(`(module
  (memory i64 1)

  ;; $$default: natural alignment, $$1: align=1, $$2: align=2, $$4: align=4, $$8: align=8

  (func (export "f32_align_switch") (param i32) (result f32)
    (local f32 f32)
    (local.set 1 (f32.const 10.0))
    (block $$4
      (block $$2
        (block $$1
          (block $$default
            (block $$0
              (br_table $$0 $$default $$1 $$2 $$4 (local.get 0))
            ) ;; 0
            (f32.store (i64.const 0) (local.get 1))
            (local.set 2 (f32.load (i64.const 0)))
            (br $$4)
          ) ;; default
          (f32.store align=1 (i64.const 0) (local.get 1))
          (local.set 2 (f32.load align=1 (i64.const 0)))
          (br $$4)
        ) ;; 1
        (f32.store align=2 (i64.const 0) (local.get 1))
        (local.set 2 (f32.load align=2 (i64.const 0)))
        (br $$4)
      ) ;; 2
      (f32.store align=4 (i64.const 0) (local.get 1))
      (local.set 2 (f32.load align=4 (i64.const 0)))
    ) ;; 4
    (local.get 2)
  )

  (func (export "f64_align_switch") (param i32) (result f64)
    (local f64 f64)
    (local.set 1 (f64.const 10.0))
    (block $$8
      (block $$4
        (block $$2
          (block $$1
            (block $$default
              (block $$0
                (br_table $$0 $$default $$1 $$2 $$4 $$8 (local.get 0))
              ) ;; 0
              (f64.store (i64.const 0) (local.get 1))
              (local.set 2 (f64.load (i64.const 0)))
              (br $$8)
            ) ;; default
            (f64.store align=1 (i64.const 0) (local.get 1))
            (local.set 2 (f64.load align=1 (i64.const 0)))
            (br $$8)
          ) ;; 1
          (f64.store align=2 (i64.const 0) (local.get 1))
          (local.set 2 (f64.load align=2 (i64.const 0)))
          (br $$8)
        ) ;; 2
        (f64.store align=4 (i64.const 0) (local.get 1))
        (local.set 2 (f64.load align=4 (i64.const 0)))
        (br $$8)
      ) ;; 4
      (f64.store align=8 (i64.const 0) (local.get 1))
      (local.set 2 (f64.load align=8 (i64.const 0)))
    ) ;; 8
    (local.get 2)
  )

  ;; $$8s: i32/i64.load8_s, $$8u: i32/i64.load8_u, $$16s: i32/i64.load16_s, $$16u: i32/i64.load16_u, $$32: i32.load
  ;; $$32s: i64.load32_s, $$32u: i64.load32_u, $$64: i64.load

  (func (export "i32_align_switch") (param i32 i32) (result i32)
    (local i32 i32)
    (local.set 2 (i32.const 10))
    (block $$32
      (block $$16u
        (block $$16s
          (block $$8u
            (block $$8s
              (block $$0
                (br_table $$0 $$8s $$8u $$16s $$16u $$32 (local.get 0))
              ) ;; 0
              (if (i32.eq (local.get 1) (i32.const 0))
                (then
                  (i32.store8 (i64.const 0) (local.get 2))
                  (local.set 3 (i32.load8_s (i64.const 0)))
                )
              )
              (if (i32.eq (local.get 1) (i32.const 1))
                (then
                  (i32.store8 align=1 (i64.const 0) (local.get 2))
                  (local.set 3 (i32.load8_s align=1 (i64.const 0)))
                )
              )
              (br $$32)
            ) ;; 8s
            (if (i32.eq (local.get 1) (i32.const 0))
              (then
                (i32.store8 (i64.const 0) (local.get 2))
                (local.set 3 (i32.load8_u (i64.const 0)))
              )
            )
            (if (i32.eq (local.get 1) (i32.const 1))
              (then
                (i32.store8 align=1 (i64.const 0) (local.get 2))
                (local.set 3 (i32.load8_u align=1 (i64.const 0)))
              )
            )
            (br $$32)
          ) ;; 8u
          (if (i32.eq (local.get 1) (i32.const 0))
            (then
              (i32.store16 (i64.const 0) (local.get 2))
              (local.set 3 (i32.load16_s (i64.const 0)))
            )
          )
          (if (i32.eq (local.get 1) (i32.const 1))
            (then
              (i32.store16 align=1 (i64.const 0) (local.get 2))
              (local.set 3 (i32.load16_s align=1 (i64.const 0)))
            )
          )
          (if (i32.eq (local.get 1) (i32.const 2))
            (then
              (i32.store16 align=2 (i64.const 0) (local.get 2))
              (local.set 3 (i32.load16_s align=2 (i64.const 0)))
            )
          )
          (br $$32)
        ) ;; 16s
        (if (i32.eq (local.get 1) (i32.const 0))
          (then
            (i32.store16 (i64.const 0) (local.get 2))
            (local.set 3 (i32.load16_u (i64.const 0)))
          )
        )
        (if (i32.eq (local.get 1) (i32.const 1))
          (then
            (i32.store16 align=1 (i64.const 0) (local.get 2))
            (local.set 3 (i32.load16_u align=1 (i64.const 0)))
          )
        )
        (if (i32.eq (local.get 1) (i32.const 2))
          (then
            (i32.store16 align=2 (i64.const 0) (local.get 2))
            (local.set 3 (i32.load16_u align=2 (i64.const 0)))
          )
        )
        (br $$32)
      ) ;; 16u
      (if (i32.eq (local.get 1) (i32.const 0))
        (then
          (i32.store (i64.const 0) (local.get 2))
          (local.set 3 (i32.load (i64.const 0)))
        )
      )
      (if (i32.eq (local.get 1) (i32.const 1))
        (then
          (i32.store align=1 (i64.const 0) (local.get 2))
          (local.set 3 (i32.load align=1 (i64.const 0)))
        )
      )
      (if (i32.eq (local.get 1) (i32.const 2))
        (then
          (i32.store align=2 (i64.const 0) (local.get 2))
          (local.set 3 (i32.load align=2 (i64.const 0)))
        )
      )
      (if (i32.eq (local.get 1) (i32.const 4))
        (then
          (i32.store align=4 (i64.const 0) (local.get 2))
          (local.set 3 (i32.load align=4 (i64.const 0)))
        )
      )
    ) ;; 32
    (local.get 3)
  )

  (func (export "i64_align_switch") (param i32 i32) (result i64)
    (local i64 i64)
    (local.set 2 (i64.const 10))
    (block $$64
      (block $$32u
        (block $$32s
          (block $$16u
            (block $$16s
              (block $$8u
                (block $$8s
                  (block $$0
                    (br_table $$0 $$8s $$8u $$16s $$16u $$32s $$32u $$64 (local.get 0))
                  ) ;; 0
                  (if (i32.eq (local.get 1) (i32.const 0))
                    (then
                      (i64.store8 (i64.const 0) (local.get 2))
                      (local.set 3 (i64.load8_s (i64.const 0)))
                    )
                  )
                  (if (i32.eq (local.get 1) (i32.const 1))
                    (then
                      (i64.store8 align=1 (i64.const 0) (local.get 2))
                      (local.set 3 (i64.load8_s align=1 (i64.const 0)))
                    )
                  )
                  (br $$64)
                ) ;; 8s
                (if (i32.eq (local.get 1) (i32.const 0))
                  (then
                    (i64.store8 (i64.const 0) (local.get 2))
                    (local.set 3 (i64.load8_u (i64.const 0)))
                  )
                )
                (if (i32.eq (local.get 1) (i32.const 1))
                  (then
                    (i64.store8 align=1 (i64.const 0) (local.get 2))
                    (local.set 3 (i64.load8_u align=1 (i64.const 0)))
                  )
                )
                (br $$64)
              ) ;; 8u
              (if (i32.eq (local.get 1) (i32.const 0))
                (then
                  (i64.store16 (i64.const 0) (local.get 2))
                  (local.set 3 (i64.load16_s (i64.const 0)))
                )
              )
              (if (i32.eq (local.get 1) (i32.const 1))
                (then
                  (i64.store16 align=1 (i64.const 0) (local.get 2))
                  (local.set 3 (i64.load16_s align=1 (i64.const 0)))
                )
              )
              (if (i32.eq (local.get 1) (i32.const 2))
                (then
                  (i64.store16 align=2 (i64.const 0) (local.get 2))
                  (local.set 3 (i64.load16_s align=2 (i64.const 0)))
                )
              )
              (br $$64)
            ) ;; 16s
            (if (i32.eq (local.get 1) (i32.const 0))
              (then
                (i64.store16 (i64.const 0) (local.get 2))
                (local.set 3 (i64.load16_u (i64.const 0)))
              )
            )
            (if (i32.eq (local.get 1) (i32.const 1))
              (then
                (i64.store16 align=1 (i64.const 0) (local.get 2))
                (local.set 3 (i64.load16_u align=1 (i64.const 0)))
              )
            )
            (if (i32.eq (local.get 1) (i32.const 2))
              (then
                (i64.store16 align=2 (i64.const 0) (local.get 2))
                (local.set 3 (i64.load16_u align=2 (i64.const 0)))
              )
            )
            (br $$64)
          ) ;; 16u
          (if (i32.eq (local.get 1) (i32.const 0))
            (then
              (i64.store32 (i64.const 0) (local.get 2))
              (local.set 3 (i64.load32_s (i64.const 0)))
            )
          )
          (if (i32.eq (local.get 1) (i32.const 1))
            (then
              (i64.store32 align=1 (i64.const 0) (local.get 2))
              (local.set 3 (i64.load32_s align=1 (i64.const 0)))
            )
          )
          (if (i32.eq (local.get 1) (i32.const 2))
            (then
              (i64.store32 align=2 (i64.const 0) (local.get 2))
              (local.set 3 (i64.load32_s align=2 (i64.const 0)))
            )
          )
          (if (i32.eq (local.get 1) (i32.const 4))
            (then
              (i64.store32 align=4 (i64.const 0) (local.get 2))
              (local.set 3 (i64.load32_s align=4 (i64.const 0)))
            )
          )
          (br $$64)
        ) ;; 32s
        (if (i32.eq (local.get 1) (i32.const 0))
          (then
            (i64.store32 (i64.const 0) (local.get 2))
            (local.set 3 (i64.load32_u (i64.const 0)))
          )
        )
        (if (i32.eq (local.get 1) (i32.const 1))
          (then
            (i64.store32 align=1 (i64.const 0) (local.get 2))
            (local.set 3 (i64.load32_u align=1 (i64.const 0)))
          )
        )
        (if (i32.eq (local.get 1) (i32.const 2))
          (then
            (i64.store32 align=2 (i64.const 0) (local.get 2))
            (local.set 3 (i64.load32_u align=2 (i64.const 0)))
          )
        )
        (if (i32.eq (local.get 1) (i32.const 4))
          (then
            (i64.store32 align=4 (i64.const 0) (local.get 2))
            (local.set 3 (i64.load32_u align=4 (i64.const 0)))
          )
        )
        (br $$64)
      ) ;; 32u
      (if (i32.eq (local.get 1) (i32.const 0))
        (then
          (i64.store (i64.const 0) (local.get 2))
          (local.set 3 (i64.load (i64.const 0)))
        )
      )
      (if (i32.eq (local.get 1) (i32.const 1))
        (then
          (i64.store align=1 (i64.const 0) (local.get 2))
          (local.set 3 (i64.load align=1 (i64.const 0)))
        )
      )
      (if (i32.eq (local.get 1) (i32.const 2))
        (then
          (i64.store align=2 (i64.const 0) (local.get 2))
          (local.set 3 (i64.load align=2 (i64.const 0)))
        )
      )
      (if (i32.eq (local.get 1) (i32.const 4))
        (then
          (i64.store align=4 (i64.const 0) (local.get 2))
          (local.set 3 (i64.load align=4 (i64.const 0)))
        )
      )
      (if (i32.eq (local.get 1) (i32.const 8))
        (then
          (i64.store align=8 (i64.const 0) (local.get 2))
          (local.set 3 (i64.load align=8 (i64.const 0)))
        )
      )
    ) ;; 64
    (local.get 3)
  )
)`);

// ./test/core/align64.wast:802
assert_return(() => invoke($23, `f32_align_switch`, [0]), [value("f32", 10)]);

// ./test/core/align64.wast:803
assert_return(() => invoke($23, `f32_align_switch`, [1]), [value("f32", 10)]);

// ./test/core/align64.wast:804
assert_return(() => invoke($23, `f32_align_switch`, [2]), [value("f32", 10)]);

// ./test/core/align64.wast:805
assert_return(() => invoke($23, `f32_align_switch`, [3]), [value("f32", 10)]);

// ./test/core/align64.wast:807
assert_return(() => invoke($23, `f64_align_switch`, [0]), [value("f64", 10)]);

// ./test/core/align64.wast:808
assert_return(() => invoke($23, `f64_align_switch`, [1]), [value("f64", 10)]);

// ./test/core/align64.wast:809
assert_return(() => invoke($23, `f64_align_switch`, [2]), [value("f64", 10)]);

// ./test/core/align64.wast:810
assert_return(() => invoke($23, `f64_align_switch`, [3]), [value("f64", 10)]);

// ./test/core/align64.wast:811
assert_return(() => invoke($23, `f64_align_switch`, [4]), [value("f64", 10)]);

// ./test/core/align64.wast:813
assert_return(() => invoke($23, `i32_align_switch`, [0, 0]), [value("i32", 10)]);

// ./test/core/align64.wast:814
assert_return(() => invoke($23, `i32_align_switch`, [0, 1]), [value("i32", 10)]);

// ./test/core/align64.wast:815
assert_return(() => invoke($23, `i32_align_switch`, [1, 0]), [value("i32", 10)]);

// ./test/core/align64.wast:816
assert_return(() => invoke($23, `i32_align_switch`, [1, 1]), [value("i32", 10)]);

// ./test/core/align64.wast:817
assert_return(() => invoke($23, `i32_align_switch`, [2, 0]), [value("i32", 10)]);

// ./test/core/align64.wast:818
assert_return(() => invoke($23, `i32_align_switch`, [2, 1]), [value("i32", 10)]);

// ./test/core/align64.wast:819
assert_return(() => invoke($23, `i32_align_switch`, [2, 2]), [value("i32", 10)]);

// ./test/core/align64.wast:820
assert_return(() => invoke($23, `i32_align_switch`, [3, 0]), [value("i32", 10)]);

// ./test/core/align64.wast:821
assert_return(() => invoke($23, `i32_align_switch`, [3, 1]), [value("i32", 10)]);

// ./test/core/align64.wast:822
assert_return(() => invoke($23, `i32_align_switch`, [3, 2]), [value("i32", 10)]);

// ./test/core/align64.wast:823
assert_return(() => invoke($23, `i32_align_switch`, [4, 0]), [value("i32", 10)]);

// ./test/core/align64.wast:824
assert_return(() => invoke($23, `i32_align_switch`, [4, 1]), [value("i32", 10)]);

// ./test/core/align64.wast:825
assert_return(() => invoke($23, `i32_align_switch`, [4, 2]), [value("i32", 10)]);

// ./test/core/align64.wast:826
assert_return(() => invoke($23, `i32_align_switch`, [4, 4]), [value("i32", 10)]);

// ./test/core/align64.wast:828
assert_return(() => invoke($23, `i64_align_switch`, [0, 0]), [value("i64", 10n)]);

// ./test/core/align64.wast:829
assert_return(() => invoke($23, `i64_align_switch`, [0, 1]), [value("i64", 10n)]);

// ./test/core/align64.wast:830
assert_return(() => invoke($23, `i64_align_switch`, [1, 0]), [value("i64", 10n)]);

// ./test/core/align64.wast:831
assert_return(() => invoke($23, `i64_align_switch`, [1, 1]), [value("i64", 10n)]);

// ./test/core/align64.wast:832
assert_return(() => invoke($23, `i64_align_switch`, [2, 0]), [value("i64", 10n)]);

// ./test/core/align64.wast:833
assert_return(() => invoke($23, `i64_align_switch`, [2, 1]), [value("i64", 10n)]);

// ./test/core/align64.wast:834
assert_return(() => invoke($23, `i64_align_switch`, [2, 2]), [value("i64", 10n)]);

// ./test/core/align64.wast:835
assert_return(() => invoke($23, `i64_align_switch`, [3, 0]), [value("i64", 10n)]);

// ./test/core/align64.wast:836
assert_return(() => invoke($23, `i64_align_switch`, [3, 1]), [value("i64", 10n)]);

// ./test/core/align64.wast:837
assert_return(() => invoke($23, `i64_align_switch`, [3, 2]), [value("i64", 10n)]);

// ./test/core/align64.wast:838
assert_return(() => invoke($23, `i64_align_switch`, [4, 0]), [value("i64", 10n)]);

// ./test/core/align64.wast:839
assert_return(() => invoke($23, `i64_align_switch`, [4, 1]), [value("i64", 10n)]);

// ./test/core/align64.wast:840
assert_return(() => invoke($23, `i64_align_switch`, [4, 2]), [value("i64", 10n)]);

// ./test/core/align64.wast:841
assert_return(() => invoke($23, `i64_align_switch`, [4, 4]), [value("i64", 10n)]);

// ./test/core/align64.wast:842
assert_return(() => invoke($23, `i64_align_switch`, [5, 0]), [value("i64", 10n)]);

// ./test/core/align64.wast:843
assert_return(() => invoke($23, `i64_align_switch`, [5, 1]), [value("i64", 10n)]);

// ./test/core/align64.wast:844
assert_return(() => invoke($23, `i64_align_switch`, [5, 2]), [value("i64", 10n)]);

// ./test/core/align64.wast:845
assert_return(() => invoke($23, `i64_align_switch`, [5, 4]), [value("i64", 10n)]);

// ./test/core/align64.wast:846
assert_return(() => invoke($23, `i64_align_switch`, [6, 0]), [value("i64", 10n)]);

// ./test/core/align64.wast:847
assert_return(() => invoke($23, `i64_align_switch`, [6, 1]), [value("i64", 10n)]);

// ./test/core/align64.wast:848
assert_return(() => invoke($23, `i64_align_switch`, [6, 2]), [value("i64", 10n)]);

// ./test/core/align64.wast:849
assert_return(() => invoke($23, `i64_align_switch`, [6, 4]), [value("i64", 10n)]);

// ./test/core/align64.wast:850
assert_return(() => invoke($23, `i64_align_switch`, [6, 8]), [value("i64", 10n)]);

// ./test/core/align64.wast:854
let $24 = instantiate(`(module
  (memory i64 1)
  (func (export "store") (param i64 i64)
    (i64.store align=4 (local.get 0) (local.get 1))
  )
  (func (export "load") (param i64) (result i32)
    (i32.load (local.get 0))
  )
)`);

// Bug 1737225 - do not observe the partial store caused by bug 1666747 on
// some native platforms.
if (!partialOobWriteMayWritePartialData()) {
    // ./test/core/align64.wast:864
    assert_trap(
        () => invoke($24, `store`, [65532n, -1n]),
        `out of bounds memory access`,
    );

    // ./test/core/align64.wast:866
    assert_return(() => invoke($24, `load`, [65532n]), [value("i32", 0)]);
}
