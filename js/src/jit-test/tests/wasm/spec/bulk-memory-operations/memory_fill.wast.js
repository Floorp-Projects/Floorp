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

// ./test/core/memory_fill.wast

// ./test/core/memory_fill.wast:5
let $0 = instantiate(`(module
  (memory 1 1)
  
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "test")
    (memory.fill (i32.const 0xFF00) (i32.const 0x55) (i32.const 256))))`);

// ./test/core/memory_fill.wast:21
invoke($0, `test`, []);

// ./test/core/memory_fill.wast:23
assert_return(() => invoke($0, `checkRange`, [0, 65280, 0]), [
  value("i32", -1),
]);

// ./test/core/memory_fill.wast:25
assert_return(() => invoke($0, `checkRange`, [65280, 65536, 85]), [
  value("i32", -1),
]);

// ./test/core/memory_fill.wast:27
let $1 = instantiate(`(module
  (memory 1 1)
  
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "test")
    (memory.fill (i32.const 0xFF00) (i32.const 0x55) (i32.const 257))))`);

// ./test/core/memory_fill.wast:43
assert_trap(() => invoke($1, `test`, []), `out of bounds memory access`);

// ./test/core/memory_fill.wast:45
let $2 = instantiate(`(module
  (memory 1 1)
  
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "test")
    (memory.fill (i32.const 0xFFFFFF00) (i32.const 0x55) (i32.const 257))))`);

// ./test/core/memory_fill.wast:61
assert_trap(() => invoke($2, `test`, []), `out of bounds memory access`);

// ./test/core/memory_fill.wast:63
let $3 = instantiate(`(module
  (memory 1 1)
  
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "test")
    (memory.fill (i32.const 0x12) (i32.const 0x55) (i32.const 0))))`);

// ./test/core/memory_fill.wast:79
invoke($3, `test`, []);

// ./test/core/memory_fill.wast:81
assert_return(() => invoke($3, `checkRange`, [0, 65536, 0]), [
  value("i32", -1),
]);

// ./test/core/memory_fill.wast:83
let $4 = instantiate(`(module
  (memory 1 1)
  
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "test")
    (memory.fill (i32.const 0x10000) (i32.const 0x55) (i32.const 0))))`);

// ./test/core/memory_fill.wast:99
invoke($4, `test`, []);

// ./test/core/memory_fill.wast:101
let $5 = instantiate(`(module
  (memory 1 1)
  
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "test")
    (memory.fill (i32.const 0x20000) (i32.const 0x55) (i32.const 0))))`);

// ./test/core/memory_fill.wast:117
assert_trap(() => invoke($5, `test`, []), `out of bounds`);

// ./test/core/memory_fill.wast:119
let $6 = instantiate(`(module
  (memory 1 1)
  
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "test")
    (memory.fill (i32.const 0x1) (i32.const 0xAA) (i32.const 0xFFFE))))`);

// ./test/core/memory_fill.wast:135
invoke($6, `test`, []);

// ./test/core/memory_fill.wast:137
assert_return(() => invoke($6, `checkRange`, [0, 1, 0]), [value("i32", -1)]);

// ./test/core/memory_fill.wast:139
assert_return(() => invoke($6, `checkRange`, [1, 65535, 170]), [
  value("i32", -1),
]);

// ./test/core/memory_fill.wast:141
assert_return(() => invoke($6, `checkRange`, [65535, 65536, 0]), [
  value("i32", -1),
]);

// ./test/core/memory_fill.wast:144
let $7 = instantiate(`(module
  (memory 1 1)
  
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "test")
     (memory.fill (i32.const 0x12) (i32.const 0x55) (i32.const 10))
     (memory.fill (i32.const 0x15) (i32.const 0xAA) (i32.const 4))))`);

// ./test/core/memory_fill.wast:161
invoke($7, `test`, []);

// ./test/core/memory_fill.wast:163
assert_return(() => invoke($7, `checkRange`, [0, 18, 0]), [value("i32", -1)]);

// ./test/core/memory_fill.wast:165
assert_return(() => invoke($7, `checkRange`, [18, 21, 85]), [value("i32", -1)]);

// ./test/core/memory_fill.wast:167
assert_return(() => invoke($7, `checkRange`, [21, 25, 170]), [
  value("i32", -1),
]);

// ./test/core/memory_fill.wast:169
assert_return(() => invoke($7, `checkRange`, [25, 28, 85]), [value("i32", -1)]);

// ./test/core/memory_fill.wast:171
assert_return(() => invoke($7, `checkRange`, [28, 65536, 0]), [
  value("i32", -1),
]);

// ./test/core/memory_fill.wast:173
assert_invalid(
  () =>
    instantiate(`(module
    (func (export "testfn")
      (memory.fill (i32.const 10) (i32.const 20) (i32.const 30))))`),
  `unknown memory 0`,
);

// ./test/core/memory_fill.wast:179
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (i32.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:186
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (i32.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:193
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (i32.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:200
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (f32.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:207
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (f32.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:214
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (f32.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:221
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (f32.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:228
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (i64.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:235
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (i64.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:242
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (i64.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:249
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (i64.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:256
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (f64.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:263
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (f64.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:270
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (f64.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:277
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (f64.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:284
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (i32.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:291
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (i32.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:298
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (i32.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:305
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (i32.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:312
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (f32.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:319
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (f32.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:326
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (f32.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:333
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (f32.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:340
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (i64.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:347
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (i64.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:354
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (i64.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:361
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (i64.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:368
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (f64.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:375
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (f64.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:382
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (f64.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:389
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (f64.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:396
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (i32.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:403
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (i32.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:410
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (i32.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:417
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (i32.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:424
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (f32.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:431
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (f32.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:438
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (f32.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:445
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (f32.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:452
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (i64.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:459
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (i64.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:466
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (i64.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:473
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (i64.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:480
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (f64.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:487
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (f64.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:494
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (f64.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:501
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (f64.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:508
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (i32.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:515
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (i32.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:522
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (i32.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:529
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (i32.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:536
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (f32.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:543
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (f32.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:550
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (f32.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:557
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (f32.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:564
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (i64.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:571
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (i64.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:578
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (i64.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:585
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (i64.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:592
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (f64.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:599
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (f64.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:606
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (f64.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:613
assert_invalid(
  () =>
    instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (f64.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:620
let $8 = instantiate(`(module
  (memory 1 1 )
  
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "run") (param $$offs i32) (param $$val i32) (param $$len i32)
    (memory.fill (local.get $$offs) (local.get $$val) (local.get $$len))))`);

// ./test/core/memory_fill.wast:637
assert_trap(() => invoke($8, `run`, [65280, 37, 512]), `out of bounds`);

// ./test/core/memory_fill.wast:640
assert_return(() => invoke($8, `checkRange`, [0, 1, 0]), [value("i32", -1)]);

// ./test/core/memory_fill.wast:642
let $9 = instantiate(`(module
  (memory 1 1 )
  
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "run") (param $$offs i32) (param $$val i32) (param $$len i32)
    (memory.fill (local.get $$offs) (local.get $$val) (local.get $$len))))`);

// ./test/core/memory_fill.wast:659
assert_trap(() => invoke($9, `run`, [65279, 37, 514]), `out of bounds`);

// ./test/core/memory_fill.wast:662
assert_return(() => invoke($9, `checkRange`, [0, 1, 0]), [value("i32", -1)]);

// ./test/core/memory_fill.wast:664
let $10 = instantiate(`(module
  (memory 1 1 )
  
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "run") (param $$offs i32) (param $$val i32) (param $$len i32)
    (memory.fill (local.get $$offs) (local.get $$val) (local.get $$len))))`);

// ./test/core/memory_fill.wast:681
assert_trap(() => invoke($10, `run`, [65279, 37, -1]), `out of bounds`);

// ./test/core/memory_fill.wast:684
assert_return(() => invoke($10, `checkRange`, [0, 1, 0]), [value("i32", -1)]);
