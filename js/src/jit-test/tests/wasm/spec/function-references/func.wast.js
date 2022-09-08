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

// ./test/core/func.wast

// ./test/core/func.wast:3
let $0 = instantiate(`(module
  ;; Auxiliary definition
  (type $$sig (func))
  (func $$dummy)

  ;; Syntax

  (func)
  (func (export "f"))
  (func $$f)
  (func $$h (export "g"))

  (func (local))
  (func (local) (local))
  (func (local i32))
  (func (local $$x i32))
  (func (local i32 f64 i64))
  (func (local i32) (local f64))
  (func (local i32 f32) (local $$x i64) (local) (local i32 f64))

  (func (param))
  (func (param) (param))
  (func (param i32))
  (func (param $$x i32))
  (func (param i32 f64 i64))
  (func (param i32) (param f64))
  (func (param i32 f32) (param $$x i64) (param) (param i32 f64))

  (func (result))
  (func (result) (result))
  (func (result i32) (unreachable))
  (func (result i32 f64 f32) (unreachable))
  (func (result i32) (result f64) (unreachable))
  (func (result i32 f32) (result i64) (result) (result i32 f64) (unreachable))

  (type $$sig-1 (func))
  (type $$sig-2 (func (result i32)))
  (type $$sig-3 (func (param $$x i32)))
  (type $$sig-4 (func (param i32 f64 i32) (result i32)))

  (func (export "type-use-1") (type $$sig-1))
  (func (export "type-use-2") (type $$sig-2) (i32.const 0))
  (func (export "type-use-3") (type $$sig-3))
  (func (export "type-use-4") (type $$sig-4) (i32.const 0))
  (func (export "type-use-5") (type $$sig-2) (result i32) (i32.const 0))
  (func (export "type-use-6") (type $$sig-3) (param i32))
  (func (export "type-use-7")
    (type $$sig-4) (param i32) (param f64 i32) (result i32) (i32.const 0)
  )

  (func (type $$sig))
  (func (type $$forward))  ;; forward reference

  (func $$complex
    (param i32 f32) (param $$x i64) (param) (param i32)
    (result) (result i32) (result) (result i64 i32)
    (local f32) (local $$y i32) (local i64 i32) (local) (local f64 i32)
    (unreachable) (unreachable)
  )
  (func $$complex-sig
    (type $$sig)
    (local f32) (local $$y i32) (local i64 i32) (local) (local f64 i32)
    (unreachable) (unreachable)
  )

  (type $$forward (func))

  ;; Typing of locals

  (func (export "local-first-i32") (result i32) (local i32 i32) (local.get 0))
  (func (export "local-first-i64") (result i64) (local i64 i64) (local.get 0))
  (func (export "local-first-f32") (result f32) (local f32 f32) (local.get 0))
  (func (export "local-first-f64") (result f64) (local f64 f64) (local.get 0))
  (func (export "local-second-i32") (result i32) (local i32 i32) (local.get 1))
  (func (export "local-second-i64") (result i64) (local i64 i64) (local.get 1))
  (func (export "local-second-f32") (result f32) (local f32 f32) (local.get 1))
  (func (export "local-second-f64") (result f64) (local f64 f64) (local.get 1))
  (func (export "local-mixed") (result f64)
    (local f32) (local $$x i32) (local i64 i32) (local) (local f64 i32)
    (drop (f32.neg (local.get 0)))
    (drop (i32.eqz (local.get 1)))
    (drop (i64.eqz (local.get 2)))
    (drop (i32.eqz (local.get 3)))
    (drop (f64.neg (local.get 4)))
    (drop (i32.eqz (local.get 5)))
    (local.get 4)
  )

  ;; Typing of parameters

  (func (export "param-first-i32") (param i32 i32) (result i32) (local.get 0))
  (func (export "param-first-i64") (param i64 i64) (result i64) (local.get 0))
  (func (export "param-first-f32") (param f32 f32) (result f32) (local.get 0))
  (func (export "param-first-f64") (param f64 f64) (result f64) (local.get 0))
  (func (export "param-second-i32") (param i32 i32) (result i32) (local.get 1))
  (func (export "param-second-i64") (param i64 i64) (result i64) (local.get 1))
  (func (export "param-second-f32") (param f32 f32) (result f32) (local.get 1))
  (func (export "param-second-f64") (param f64 f64) (result f64) (local.get 1))
  (func (export "param-mixed") (param f32 i32) (param) (param $$x i64) (param i32 f64 i32)
    (result f64)
    (drop (f32.neg (local.get 0)))
    (drop (i32.eqz (local.get 1)))
    (drop (i64.eqz (local.get 2)))
    (drop (i32.eqz (local.get 3)))
    (drop (f64.neg (local.get 4)))
    (drop (i32.eqz (local.get 5)))
    (local.get 4)
  )

  ;; Typing of results

  (func (export "empty"))
  (func (export "value-void") (call $$dummy))
  (func (export "value-i32") (result i32) (i32.const 77))
  (func (export "value-i64") (result i64) (i64.const 7777))
  (func (export "value-f32") (result f32) (f32.const 77.7))
  (func (export "value-f64") (result f64) (f64.const 77.77))
  (func (export "value-i32-f64") (result i32 f64) (i32.const 77) (f64.const 7))
  (func (export "value-i32-i32-i32") (result i32 i32 i32)
    (i32.const 1) (i32.const 2) (i32.const 3)
  )
  (func (export "value-block-void") (block (call $$dummy) (call $$dummy)))
  (func (export "value-block-i32") (result i32)
    (block (result i32) (call $$dummy) (i32.const 77))
  )
  (func (export "value-block-i32-i64") (result i32 i64)
    (block (result i32 i64) (call $$dummy) (i32.const 1) (i64.const 2))
  )

  (func (export "return-empty") (return))
  (func (export "return-i32") (result i32) (return (i32.const 78)))
  (func (export "return-i64") (result i64) (return (i64.const 7878)))
  (func (export "return-f32") (result f32) (return (f32.const 78.7)))
  (func (export "return-f64") (result f64) (return (f64.const 78.78)))
  (func (export "return-i32-f64") (result i32 f64)
    (return (i32.const 78) (f64.const 78.78))
  )
  (func (export "return-i32-i32-i32") (result i32 i32 i32)
    (return (i32.const 1) (i32.const 2) (i32.const 3))
  )
  (func (export "return-block-i32") (result i32)
    (return (block (result i32) (call $$dummy) (i32.const 77)))
  )
  (func (export "return-block-i32-i64") (result i32 i64)
    (return (block (result i32 i64) (call $$dummy) (i32.const 1) (i64.const 2)))
  )

  (func (export "break-empty") (br 0))
  (func (export "break-i32") (result i32) (br 0 (i32.const 79)))
  (func (export "break-i64") (result i64) (br 0 (i64.const 7979)))
  (func (export "break-f32") (result f32) (br 0 (f32.const 79.9)))
  (func (export "break-f64") (result f64) (br 0 (f64.const 79.79)))
  (func (export "break-i32-f64") (result i32 f64)
    (br 0 (i32.const 79) (f64.const 79.79))
  )
  (func (export "break-i32-i32-i32") (result i32 i32 i32)
    (br 0 (i32.const 1) (i32.const 2) (i32.const 3))
  )
  (func (export "break-block-i32") (result i32)
    (br 0 (block (result i32) (call $$dummy) (i32.const 77)))
  )
  (func (export "break-block-i32-i64") (result i32 i64)
    (br 0 (block (result i32 i64) (call $$dummy) (i32.const 1) (i64.const 2)))
  )

  (func (export "break-br_if-empty") (param i32)
    (br_if 0 (local.get 0))
  )
  (func (export "break-br_if-num") (param i32) (result i32)
    (drop (br_if 0 (i32.const 50) (local.get 0))) (i32.const 51)
  )
  (func (export "break-br_if-num-num") (param i32) (result i32 i64)
    (drop (drop (br_if 0 (i32.const 50) (i64.const 51) (local.get 0))))
    (i32.const 51) (i64.const 52)
  )

  (func (export "break-br_table-empty") (param i32)
    (br_table 0 0 0 (local.get 0))
  )
  (func (export "break-br_table-num") (param i32) (result i32)
    (br_table 0 0 (i32.const 50) (local.get 0)) (i32.const 51)
  )
  (func (export "break-br_table-num-num") (param i32) (result f32 i64)
    (br_table 0 0 (f32.const 50) (i64.const 51) (local.get 0))
    (f32.const 51) (i64.const 52)
  )
  (func (export "break-br_table-nested-empty") (param i32)
    (block (br_table 0 1 0 (local.get 0)))
  )
  (func (export "break-br_table-nested-num") (param i32) (result i32)
    (i32.add
      (block (result i32)
        (br_table 0 1 0 (i32.const 50) (local.get 0)) (i32.const 51)
      )
      (i32.const 2)
    )
  )
  (func (export "break-br_table-nested-num-num") (param i32) (result i32 i32)
    (i32.add
      (block (result i32 i32)
        (br_table 0 1 0 (i32.const 50) (i32.const 51) (local.get 0))
        (i32.const 51) (i32.const -3)
      )
    )
    (i32.const 52)
  )

  ;; Large signatures

  (func (export "large-sig")
    (param i32 i64 f32 f32 i32 f64 f32 i32 i32 i32 f32 f64 f64 f64 i32 i32 f32)
    (result f64 f32 i32 i32 i32 i64 f32 i32 i32 f32 f64 f64 i32 f32 i32 f64)
    (local.get 5)
    (local.get 2)
    (local.get 0)
    (local.get 8)
    (local.get 7)
    (local.get 1)
    (local.get 3)
    (local.get 9)
    (local.get 4)
    (local.get 6)
    (local.get 13)
    (local.get 11)
    (local.get 15)
    (local.get 16)
    (local.get 14)
    (local.get 12)
  )

  ;; Default initialization of locals

  (func (export "init-local-i32") (result i32) (local i32) (local.get 0))
  (func (export "init-local-i64") (result i64) (local i64) (local.get 0))
  (func (export "init-local-f32") (result f32) (local f32) (local.get 0))
  (func (export "init-local-f64") (result f64) (local f64) (local.get 0))
)`);

// ./test/core/func.wast:241
assert_return(() => invoke($0, `type-use-1`, []), []);

// ./test/core/func.wast:242
assert_return(() => invoke($0, `type-use-2`, []), [value("i32", 0)]);

// ./test/core/func.wast:243
assert_return(() => invoke($0, `type-use-3`, [1]), []);

// ./test/core/func.wast:244
assert_return(() => invoke($0, `type-use-4`, [1, value("f64", 1), 1]), [
  value("i32", 0),
]);

// ./test/core/func.wast:248
assert_return(() => invoke($0, `type-use-5`, []), [value("i32", 0)]);

// ./test/core/func.wast:249
assert_return(() => invoke($0, `type-use-6`, [1]), []);

// ./test/core/func.wast:250
assert_return(() => invoke($0, `type-use-7`, [1, value("f64", 1), 1]), [
  value("i32", 0),
]);

// ./test/core/func.wast:255
assert_return(() => invoke($0, `local-first-i32`, []), [value("i32", 0)]);

// ./test/core/func.wast:256
assert_return(() => invoke($0, `local-first-i64`, []), [value("i64", 0n)]);

// ./test/core/func.wast:257
assert_return(() => invoke($0, `local-first-f32`, []), [value("f32", 0)]);

// ./test/core/func.wast:258
assert_return(() => invoke($0, `local-first-f64`, []), [value("f64", 0)]);

// ./test/core/func.wast:259
assert_return(() => invoke($0, `local-second-i32`, []), [value("i32", 0)]);

// ./test/core/func.wast:260
assert_return(() => invoke($0, `local-second-i64`, []), [value("i64", 0n)]);

// ./test/core/func.wast:261
assert_return(() => invoke($0, `local-second-f32`, []), [value("f32", 0)]);

// ./test/core/func.wast:262
assert_return(() => invoke($0, `local-second-f64`, []), [value("f64", 0)]);

// ./test/core/func.wast:263
assert_return(() => invoke($0, `local-mixed`, []), [value("f64", 0)]);

// ./test/core/func.wast:265
assert_return(() => invoke($0, `param-first-i32`, [2, 3]), [value("i32", 2)]);

// ./test/core/func.wast:268
assert_return(() => invoke($0, `param-first-i64`, [2n, 3n]), [
  value("i64", 2n),
]);

// ./test/core/func.wast:271
assert_return(
  () => invoke($0, `param-first-f32`, [value("f32", 2), value("f32", 3)]),
  [value("f32", 2)],
);

// ./test/core/func.wast:274
assert_return(
  () => invoke($0, `param-first-f64`, [value("f64", 2), value("f64", 3)]),
  [value("f64", 2)],
);

// ./test/core/func.wast:277
assert_return(() => invoke($0, `param-second-i32`, [2, 3]), [value("i32", 3)]);

// ./test/core/func.wast:280
assert_return(() => invoke($0, `param-second-i64`, [2n, 3n]), [
  value("i64", 3n),
]);

// ./test/core/func.wast:283
assert_return(
  () => invoke($0, `param-second-f32`, [value("f32", 2), value("f32", 3)]),
  [value("f32", 3)],
);

// ./test/core/func.wast:286
assert_return(
  () => invoke($0, `param-second-f64`, [value("f64", 2), value("f64", 3)]),
  [value("f64", 3)],
);

// ./test/core/func.wast:290
assert_return(
  () =>
    invoke($0, `param-mixed`, [
      value("f32", 1),
      2,
      3n,
      4,
      value("f64", 5.5),
      6,
    ]),
  [value("f64", 5.5)],
);

// ./test/core/func.wast:298
assert_return(() => invoke($0, `empty`, []), []);

// ./test/core/func.wast:299
assert_return(() => invoke($0, `value-void`, []), []);

// ./test/core/func.wast:300
assert_return(() => invoke($0, `value-i32`, []), [value("i32", 77)]);

// ./test/core/func.wast:301
assert_return(() => invoke($0, `value-i64`, []), [value("i64", 7777n)]);

// ./test/core/func.wast:302
assert_return(() => invoke($0, `value-f32`, []), [value("f32", 77.7)]);

// ./test/core/func.wast:303
assert_return(() => invoke($0, `value-f64`, []), [value("f64", 77.77)]);

// ./test/core/func.wast:304
assert_return(() => invoke($0, `value-i32-f64`, []), [
  value("i32", 77),
  value("f64", 7),
]);

// ./test/core/func.wast:305
assert_return(() => invoke($0, `value-i32-i32-i32`, []), [
  value("i32", 1),
  value("i32", 2),
  value("i32", 3),
]);

// ./test/core/func.wast:308
assert_return(() => invoke($0, `value-block-void`, []), []);

// ./test/core/func.wast:309
assert_return(() => invoke($0, `value-block-i32`, []), [value("i32", 77)]);

// ./test/core/func.wast:310
assert_return(() => invoke($0, `value-block-i32-i64`, []), [
  value("i32", 1),
  value("i64", 2n),
]);

// ./test/core/func.wast:312
assert_return(() => invoke($0, `return-empty`, []), []);

// ./test/core/func.wast:313
assert_return(() => invoke($0, `return-i32`, []), [value("i32", 78)]);

// ./test/core/func.wast:314
assert_return(() => invoke($0, `return-i64`, []), [value("i64", 7878n)]);

// ./test/core/func.wast:315
assert_return(() => invoke($0, `return-f32`, []), [value("f32", 78.7)]);

// ./test/core/func.wast:316
assert_return(() => invoke($0, `return-f64`, []), [value("f64", 78.78)]);

// ./test/core/func.wast:317
assert_return(() => invoke($0, `return-i32-f64`, []), [
  value("i32", 78),
  value("f64", 78.78),
]);

// ./test/core/func.wast:318
assert_return(() => invoke($0, `return-i32-i32-i32`, []), [
  value("i32", 1),
  value("i32", 2),
  value("i32", 3),
]);

// ./test/core/func.wast:321
assert_return(() => invoke($0, `return-block-i32`, []), [value("i32", 77)]);

// ./test/core/func.wast:322
assert_return(() => invoke($0, `return-block-i32-i64`, []), [
  value("i32", 1),
  value("i64", 2n),
]);

// ./test/core/func.wast:324
assert_return(() => invoke($0, `break-empty`, []), []);

// ./test/core/func.wast:325
assert_return(() => invoke($0, `break-i32`, []), [value("i32", 79)]);

// ./test/core/func.wast:326
assert_return(() => invoke($0, `break-i64`, []), [value("i64", 7979n)]);

// ./test/core/func.wast:327
assert_return(() => invoke($0, `break-f32`, []), [value("f32", 79.9)]);

// ./test/core/func.wast:328
assert_return(() => invoke($0, `break-f64`, []), [value("f64", 79.79)]);

// ./test/core/func.wast:329
assert_return(() => invoke($0, `break-i32-f64`, []), [
  value("i32", 79),
  value("f64", 79.79),
]);

// ./test/core/func.wast:330
assert_return(() => invoke($0, `break-i32-i32-i32`, []), [
  value("i32", 1),
  value("i32", 2),
  value("i32", 3),
]);

// ./test/core/func.wast:333
assert_return(() => invoke($0, `break-block-i32`, []), [value("i32", 77)]);

// ./test/core/func.wast:334
assert_return(() => invoke($0, `break-block-i32-i64`, []), [
  value("i32", 1),
  value("i64", 2n),
]);

// ./test/core/func.wast:336
assert_return(() => invoke($0, `break-br_if-empty`, [0]), []);

// ./test/core/func.wast:337
assert_return(() => invoke($0, `break-br_if-empty`, [2]), []);

// ./test/core/func.wast:338
assert_return(() => invoke($0, `break-br_if-num`, [0]), [value("i32", 51)]);

// ./test/core/func.wast:339
assert_return(() => invoke($0, `break-br_if-num`, [1]), [value("i32", 50)]);

// ./test/core/func.wast:340
assert_return(() => invoke($0, `break-br_if-num-num`, [0]), [
  value("i32", 51),
  value("i64", 52n),
]);

// ./test/core/func.wast:343
assert_return(() => invoke($0, `break-br_if-num-num`, [1]), [
  value("i32", 50),
  value("i64", 51n),
]);

// ./test/core/func.wast:347
assert_return(() => invoke($0, `break-br_table-empty`, [0]), []);

// ./test/core/func.wast:348
assert_return(() => invoke($0, `break-br_table-empty`, [1]), []);

// ./test/core/func.wast:349
assert_return(() => invoke($0, `break-br_table-empty`, [5]), []);

// ./test/core/func.wast:350
assert_return(() => invoke($0, `break-br_table-empty`, [-1]), []);

// ./test/core/func.wast:351
assert_return(() => invoke($0, `break-br_table-num`, [0]), [value("i32", 50)]);

// ./test/core/func.wast:352
assert_return(() => invoke($0, `break-br_table-num`, [1]), [value("i32", 50)]);

// ./test/core/func.wast:353
assert_return(() => invoke($0, `break-br_table-num`, [10]), [value("i32", 50)]);

// ./test/core/func.wast:354
assert_return(() => invoke($0, `break-br_table-num`, [-100]), [
  value("i32", 50),
]);

// ./test/core/func.wast:355
assert_return(() => invoke($0, `break-br_table-num-num`, [0]), [
  value("f32", 50),
  value("i64", 51n),
]);

// ./test/core/func.wast:358
assert_return(() => invoke($0, `break-br_table-num-num`, [1]), [
  value("f32", 50),
  value("i64", 51n),
]);

// ./test/core/func.wast:361
assert_return(() => invoke($0, `break-br_table-num-num`, [10]), [
  value("f32", 50),
  value("i64", 51n),
]);

// ./test/core/func.wast:364
assert_return(() => invoke($0, `break-br_table-num-num`, [-100]), [
  value("f32", 50),
  value("i64", 51n),
]);

// ./test/core/func.wast:367
assert_return(() => invoke($0, `break-br_table-nested-empty`, [0]), []);

// ./test/core/func.wast:368
assert_return(() => invoke($0, `break-br_table-nested-empty`, [1]), []);

// ./test/core/func.wast:369
assert_return(() => invoke($0, `break-br_table-nested-empty`, [3]), []);

// ./test/core/func.wast:370
assert_return(() => invoke($0, `break-br_table-nested-empty`, [-2]), []);

// ./test/core/func.wast:371
assert_return(() => invoke($0, `break-br_table-nested-num`, [0]), [
  value("i32", 52),
]);

// ./test/core/func.wast:374
assert_return(() => invoke($0, `break-br_table-nested-num`, [1]), [
  value("i32", 50),
]);

// ./test/core/func.wast:377
assert_return(() => invoke($0, `break-br_table-nested-num`, [2]), [
  value("i32", 52),
]);

// ./test/core/func.wast:380
assert_return(() => invoke($0, `break-br_table-nested-num`, [-3]), [
  value("i32", 52),
]);

// ./test/core/func.wast:383
assert_return(() => invoke($0, `break-br_table-nested-num-num`, [0]), [
  value("i32", 101),
  value("i32", 52),
]);

// ./test/core/func.wast:387
assert_return(() => invoke($0, `break-br_table-nested-num-num`, [1]), [
  value("i32", 50),
  value("i32", 51),
]);

// ./test/core/func.wast:391
assert_return(() => invoke($0, `break-br_table-nested-num-num`, [2]), [
  value("i32", 101),
  value("i32", 52),
]);

// ./test/core/func.wast:395
assert_return(() => invoke($0, `break-br_table-nested-num-num`, [-3]), [
  value("i32", 101),
  value("i32", 52),
]);

// ./test/core/func.wast:400
assert_return(
  () =>
    invoke($0, `large-sig`, [
      0,
      1n,
      value("f32", 2),
      value("f32", 3),
      4,
      value("f64", 5),
      value("f32", 6),
      7,
      8,
      9,
      value("f32", 10),
      value("f64", 11),
      value("f64", 12),
      value("f64", 13),
      14,
      15,
      value("f32", 16),
    ]),
  [
    value("f64", 5),
    value("f32", 2),
    value("i32", 0),
    value("i32", 8),
    value("i32", 7),
    value("i64", 1n),
    value("f32", 3),
    value("i32", 9),
    value("i32", 4),
    value("f32", 6),
    value("f64", 13),
    value("f64", 11),
    value("i32", 15),
    value("f32", 16),
    value("i32", 14),
    value("f64", 12),
  ],
);

// ./test/core/func.wast:414
assert_return(() => invoke($0, `init-local-i32`, []), [value("i32", 0)]);

// ./test/core/func.wast:415
assert_return(() => invoke($0, `init-local-i64`, []), [value("i64", 0n)]);

// ./test/core/func.wast:416
assert_return(() => invoke($0, `init-local-f32`, []), [value("f32", 0)]);

// ./test/core/func.wast:417
assert_return(() => invoke($0, `init-local-f64`, []), [value("f64", 0)]);

// ./test/core/func.wast:422
let $1 = instantiate(`(module
  (func $$f (result f64) (f64.const 0))  ;; adds implicit type definition
  (func $$g (param i32))                 ;; reuses explicit type definition
  (type $$t (func (param i32)))

  (func $$i32->void (type 0))                ;; (param i32)
  (func $$void->f64 (type 1) (f64.const 0))  ;; (result f64)
  (func $$check
    (call $$i32->void (i32.const 0))
    (drop (call $$void->f64))
  )
)`);

// ./test/core/func.wast:435
assert_invalid(() =>
  instantiate(`(module
    (func $$f (result f64) (f64.const 0))  ;; adds implicit type definition
    (func $$g (param i32))                 ;; reuses explicit type definition
    (func $$h (result f64) (f64.const 1))  ;; reuses implicit type definition
    (type $$t (func (param i32)))

    (func (type 2))  ;; does not exist
  )`), `unknown type`);

// ./test/core/func.wast:447
assert_malformed(
  () =>
    instantiate(
      `(func $$f (result f64) (f64.const 0)) (func $$g (param i32)) (func $$h (result f64) (f64.const 1)) (type $$t (func (param i32))) (func (type 2) (param i32)) `,
    ),
  `unknown type`,
);

// ./test/core/func.wast:459
let $2 = instantiate(`(module
  (type $$proc (func (result i32)))
  (type $$sig (func (param i32) (result i32)))

  (func (export "f") (type $$sig)
    (local $$var i32)
    (local.get $$var)
  )

  (func $$g (type $$sig)
    (local $$var i32)
    (local.get $$var)
  )
  (func (export "g") (type $$sig)
    (call $$g (local.get 0))
  )

  (func (export "p") (type $$proc)
    (local $$var i32)
    (local.set 0 (i32.const 42))
    (local.get $$var)
  )
)`);

// ./test/core/func.wast:483
assert_return(() => invoke($2, `f`, [42]), [value("i32", 0)]);

// ./test/core/func.wast:484
assert_return(() => invoke($2, `g`, [42]), [value("i32", 0)]);

// ./test/core/func.wast:485
assert_return(() => invoke($2, `p`, []), [value("i32", 42)]);

// ./test/core/func.wast:488
let $3 = instantiate(`(module
  (type $$sig (func))

  (func $$empty-sig-1)  ;; should be assigned type $$sig
  (func $$complex-sig-1 (param f64 i64 f64 i64 f64 i64 f32 i32))
  (func $$empty-sig-2)  ;; should be assigned type $$sig
  (func $$complex-sig-2 (param f64 i64 f64 i64 f64 i64 f32 i32))
  (func $$complex-sig-3 (param f64 i64 f64 i64 f64 i64 f32 i32))
  (func $$complex-sig-4 (param i64 i64 f64 i64 f64 i64 f32 i32))
  (func $$complex-sig-5 (param i64 i64 f64 i64 f64 i64 f32 i32))

  (type $$empty-sig-duplicate (func))
  (type $$complex-sig-duplicate (func (param i64 i64 f64 i64 f64 i64 f32 i32)))
  (table funcref
    (elem
      $$complex-sig-3 $$empty-sig-2 $$complex-sig-1 $$complex-sig-3 $$empty-sig-1
      $$complex-sig-4 $$complex-sig-5
    )
  )

  (func (export "signature-explicit-reused")
    (call_indirect (type $$sig) (i32.const 1))
    (call_indirect (type $$sig) (i32.const 4))
  )

  (func (export "signature-implicit-reused")
    ;; The implicit index 3 in this test depends on the function and
    ;; type definitions, and may need adapting if they change.
    (call_indirect (type 3)
      (f64.const 0) (i64.const 0) (f64.const 0) (i64.const 0)
      (f64.const 0) (i64.const 0) (f32.const 0) (i32.const 0)
      (i32.const 0)
    )
    (call_indirect (type 3)
      (f64.const 0) (i64.const 0) (f64.const 0) (i64.const 0)
      (f64.const 0) (i64.const 0) (f32.const 0) (i32.const 0)
      (i32.const 2)
    )
    (call_indirect (type 3)
      (f64.const 0) (i64.const 0) (f64.const 0) (i64.const 0)
      (f64.const 0) (i64.const 0) (f32.const 0) (i32.const 0)
      (i32.const 3)
    )
  )

  (func (export "signature-explicit-duplicate")
    (call_indirect (type $$empty-sig-duplicate) (i32.const 1))
  )

  (func (export "signature-implicit-duplicate")
    (call_indirect (type $$complex-sig-duplicate)
      (i64.const 0) (i64.const 0) (f64.const 0) (i64.const 0)
      (f64.const 0) (i64.const 0) (f32.const 0) (i32.const 0)
      (i32.const 5)
    )
    (call_indirect (type $$complex-sig-duplicate)
      (i64.const 0) (i64.const 0) (f64.const 0) (i64.const 0)
      (f64.const 0) (i64.const 0) (f32.const 0) (i32.const 0)
      (i32.const 6)
    )
  )
)`);

// ./test/core/func.wast:551
assert_return(() => invoke($3, `signature-explicit-reused`, []), []);

// ./test/core/func.wast:552
assert_return(() => invoke($3, `signature-implicit-reused`, []), []);

// ./test/core/func.wast:553
assert_return(() => invoke($3, `signature-explicit-duplicate`, []), []);

// ./test/core/func.wast:554
assert_return(() => invoke($3, `signature-implicit-duplicate`, []), []);

// ./test/core/func.wast:559
assert_malformed(
  () =>
    instantiate(
      `(type $$sig (func (param i32) (result i32))) (func (type $$sig) (result i32) (param i32) (i32.const 0)) `,
    ),
  `unexpected token`,
);

// ./test/core/func.wast:566
assert_malformed(
  () =>
    instantiate(
      `(type $$sig (func (param i32) (result i32))) (func (param i32) (type $$sig) (result i32) (i32.const 0)) `,
    ),
  `unexpected token`,
);

// ./test/core/func.wast:573
assert_malformed(
  () =>
    instantiate(
      `(type $$sig (func (param i32) (result i32))) (func (param i32) (result i32) (type $$sig) (i32.const 0)) `,
    ),
  `unexpected token`,
);

// ./test/core/func.wast:580
assert_malformed(
  () =>
    instantiate(
      `(type $$sig (func (param i32) (result i32))) (func (result i32) (type $$sig) (param i32) (i32.const 0)) `,
    ),
  `unexpected token`,
);

// ./test/core/func.wast:587
assert_malformed(
  () =>
    instantiate(
      `(type $$sig (func (param i32) (result i32))) (func (result i32) (param i32) (type $$sig) (i32.const 0)) `,
    ),
  `unexpected token`,
);

// ./test/core/func.wast:594
assert_malformed(
  () => instantiate(`(func (result i32) (param i32) (i32.const 0)) `),
  `unexpected token`,
);

// ./test/core/func.wast:601
assert_malformed(
  () =>
    instantiate(
      `(type $$sig (func)) (func (type $$sig) (result i32) (i32.const 0)) `,
    ),
  `inline function type`,
);

// ./test/core/func.wast:608
assert_malformed(
  () =>
    instantiate(
      `(type $$sig (func (param i32) (result i32))) (func (type $$sig) (result i32) (i32.const 0)) `,
    ),
  `inline function type`,
);

// ./test/core/func.wast:615
assert_malformed(
  () =>
    instantiate(
      `(type $$sig (func (param i32) (result i32))) (func (type $$sig) (param i32) (i32.const 0)) `,
    ),
  `inline function type`,
);

// ./test/core/func.wast:622
assert_malformed(
  () =>
    instantiate(
      `(type $$sig (func (param i32 i32) (result i32))) (func (type $$sig) (param i32) (result i32) (unreachable)) `,
    ),
  `inline function type`,
);

// ./test/core/func.wast:630
assert_invalid(
  () => instantiate(`(module (func $$g (type 4)))`),
  `unknown type`,
);

// ./test/core/func.wast:634
assert_invalid(() =>
  instantiate(`(module
    (func $$f (drop (ref.func $$g)))
    (func $$g (type 4))
    (elem declare func $$g)
  )`), `unknown type`);

// ./test/core/func.wast:646
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-local-num-vs-num (result i64) (local i32) (local.get 0)))`,
    ),
  `type mismatch`,
);

// ./test/core/func.wast:650
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-local-num-vs-num (local f32) (i32.eqz (local.get 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/func.wast:654
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-local-num-vs-num (local f64 i64) (f64.neg (local.get 1))))`,
    ),
  `type mismatch`,
);

// ./test/core/func.wast:659
assert_invalid(() =>
  instantiate(`(module
    (type $$t (func))
    (func $$type-local-uninitialized (local $$x (ref $$t)) (drop (local.get $$x)))
  )`), `uninitialized local`);

// ./test/core/func.wast:670
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-param-num-vs-num (param i32) (result i64) (local.get 0)))`,
    ),
  `type mismatch`,
);

// ./test/core/func.wast:674
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-param-num-vs-num (param f32) (i32.eqz (local.get 0))))`,
    ),
  `type mismatch`,
);

// ./test/core/func.wast:678
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-param-num-vs-num (param f64 i64) (f64.neg (local.get 1))))`,
    ),
  `type mismatch`,
);

// ./test/core/func.wast:686
assert_invalid(
  () => instantiate(`(module (func $$type-empty-i32 (result i32)))`),
  `type mismatch`,
);

// ./test/core/func.wast:690
assert_invalid(
  () => instantiate(`(module (func $$type-empty-i64 (result i64)))`),
  `type mismatch`,
);

// ./test/core/func.wast:694
assert_invalid(
  () => instantiate(`(module (func $$type-empty-f32 (result f32)))`),
  `type mismatch`,
);

// ./test/core/func.wast:698
assert_invalid(
  () => instantiate(`(module (func $$type-empty-f64 (result f64)))`),
  `type mismatch`,
);

// ./test/core/func.wast:702
assert_invalid(
  () => instantiate(`(module (func $$type-empty-f64-i32 (result f64 i32)))`),
  `type mismatch`,
);

// ./test/core/func.wast:707
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-void-vs-num (result i32)
    (nop)
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:713
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-void-vs-nums (result i32 i32)
    (nop)
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:719
assert_invalid(() =>
  instantiate(`(module (func $$type-value-num-vs-void
    (i32.const 0)
  ))`), `type mismatch`);

// ./test/core/func.wast:725
assert_invalid(() =>
  instantiate(`(module (func $$type-value-nums-vs-void
    (i32.const 0) (i64.const 0)
  ))`), `type mismatch`);

// ./test/core/func.wast:731
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-num-vs-num (result i32)
    (f32.const 0)
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:737
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-num-vs-nums (result f32 f32)
    (f32.const 0)
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:743
assert_invalid(
  () =>
    instantiate(`(module (func $$type-value-nums-vs-num (result f32)
    (f32.const 0) (f32.const 0)
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:750
assert_invalid(
  () =>
    instantiate(`(module (func $$type-return-last-empty-vs-num (result i32)
    (return)
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:756
assert_invalid(
  () =>
    instantiate(`(module (func $$type-return-last-empty-vs-nums (result i32 i32)
    (return)
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:762
assert_invalid(
  () =>
    instantiate(`(module (func $$type-return-last-void-vs-num (result i32)
    (return (nop))
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:768
assert_invalid(
  () =>
    instantiate(`(module (func $$type-return-last-void-vs-nums (result i32 i64)
    (return (nop))
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:774
assert_invalid(
  () =>
    instantiate(`(module (func $$type-return-last-num-vs-num (result i32)
    (return (i64.const 0))
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:780
assert_invalid(
  () =>
    instantiate(`(module (func $$type-return-last-num-vs-nums (result i64 i64)
    (return (i64.const 0))
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:787
assert_invalid(
  () =>
    instantiate(`(module (func $$type-return-empty-vs-num (result i32)
    (return) (i32.const 1)
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:793
assert_invalid(
  () =>
    instantiate(`(module (func $$type-return-empty-vs-nums (result i32 i32)
    (return) (i32.const 1) (i32.const 2)
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:799
assert_invalid(
  () =>
    instantiate(`(module (func $$type-return-partial-vs-nums (result i32 i32)
    (i32.const 1) (return) (i32.const 2)
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:805
assert_invalid(
  () =>
    instantiate(`(module (func $$type-return-void-vs-num (result i32)
    (return (nop)) (i32.const 1)
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:811
assert_invalid(
  () =>
    instantiate(`(module (func $$type-return-void-vs-nums (result i32 i32)
    (return (nop)) (i32.const 1)
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:817
assert_invalid(
  () =>
    instantiate(`(module (func $$type-return-num-vs-num (result i32)
    (return (i64.const 1)) (i32.const 1)
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:823
assert_invalid(
  () =>
    instantiate(`(module (func $$type-return-num-vs-nums (result i32 i32)
    (return (i64.const 1)) (i32.const 1) (i32.const 2)
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:829
assert_invalid(
  () =>
    instantiate(`(module (func $$type-return-first-num-vs-num (result i32)
    (return (i64.const 1)) (return (i32.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:835
assert_invalid(
  () =>
    instantiate(`(module (func $$type-return-first-num-vs-nums (result i32 i32)
    (return (i32.const 1)) (return (i32.const 1) (i32.const 2))
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:842
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-last-void-vs-num (result i32)
    (br 0)
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:848
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-last-void-vs-nums (result i32 i32)
    (br 0)
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:854
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-last-num-vs-num (result i32)
    (br 0 (f32.const 0))
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:860
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-last-num-vs-nums (result i32 i32)
    (br 0 (i32.const 0))
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:866
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-void-vs-num (result i32)
    (br 0) (i32.const 1)
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:872
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-void-vs-nums (result i32 i32)
    (br 0) (i32.const 1) (i32.const 2)
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:878
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-num-vs-num (result i32)
    (br 0 (i64.const 1)) (i32.const 1)
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:884
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-num-vs-nums (result i32 i32)
    (br 0 (i32.const 1)) (i32.const 1) (i32.const 2)
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:890
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-first-num-vs-num (result i32)
    (br 0 (i64.const 1)) (br 0 (i32.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:897
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-empty-vs-num (result i32)
    (block (br 1)) (br 0 (i32.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:903
assert_invalid(
  () =>
    instantiate(
      `(module (func $$type-break-nested-empty-vs-nums (result i32 i32)
    (block (br 1)) (br 0 (i32.const 1) (i32.const 2))
  ))`,
    ),
  `type mismatch`,
);

// ./test/core/func.wast:909
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-void-vs-num (result i32)
    (block (br 1 (nop))) (br 0 (i32.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:915
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-void-vs-nums (result i32 i32)
    (block (br 1 (nop))) (br 0 (i32.const 1) (i32.const 2))
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:921
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-num-vs-num (result i32)
    (block (br 1 (i64.const 1))) (br 0 (i32.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:927
assert_invalid(
  () =>
    instantiate(`(module (func $$type-break-nested-num-vs-nums (result i32 i32)
    (block (result i32) (br 1 (i32.const 1))) (br 0 (i32.const 1) (i32.const 2))
  ))`),
  `type mismatch`,
);

// ./test/core/func.wast:937
assert_malformed(
  () => instantiate(`(func (nop) (local i32)) `),
  `unexpected token`,
);

// ./test/core/func.wast:941
assert_malformed(
  () => instantiate(`(func (nop) (param i32)) `),
  `unexpected token`,
);

// ./test/core/func.wast:945
assert_malformed(
  () => instantiate(`(func (nop) (result i32)) `),
  `unexpected token`,
);

// ./test/core/func.wast:949
assert_malformed(
  () => instantiate(`(func (local i32) (param i32)) `),
  `unexpected token`,
);

// ./test/core/func.wast:953
assert_malformed(
  () => instantiate(`(func (local i32) (result i32) (local.get 0)) `),
  `unexpected token`,
);

// ./test/core/func.wast:957
assert_malformed(
  () => instantiate(`(func (result i32) (param i32) (local.get 0)) `),
  `unexpected token`,
);

// ./test/core/func.wast:964
assert_malformed(
  () => instantiate(`(func $$foo) (func $$foo) `),
  `duplicate func`,
);

// ./test/core/func.wast:968
assert_malformed(
  () => instantiate(`(import "" "" (func $$foo)) (func $$foo) `),
  `duplicate func`,
);

// ./test/core/func.wast:972
assert_malformed(
  () => instantiate(`(import "" "" (func $$foo)) (import "" "" (func $$foo)) `),
  `duplicate func`,
);

// ./test/core/func.wast:977
assert_malformed(
  () => instantiate(`(func (param $$foo i32) (param $$foo i32)) `),
  `duplicate local`,
);

// ./test/core/func.wast:981
assert_malformed(
  () => instantiate(`(func (param $$foo i32) (local $$foo i32)) `),
  `duplicate local`,
);

// ./test/core/func.wast:985
assert_malformed(
  () => instantiate(`(func (local $$foo i32) (local $$foo i32)) `),
  `duplicate local`,
);
