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

// ./test/core/float_memory.wast

// ./test/core/float_memory.wast:5
let $0 = instantiate(`(module
  (memory (data "\\00\\00\\a0\\7f"))

  (func (export "f32.load") (result f32) (f32.load (i32.const 0)))
  (func (export "i32.load") (result i32) (i32.load (i32.const 0)))
  (func (export "f32.store") (f32.store (i32.const 0) (f32.const nan:0x200000)))
  (func (export "i32.store") (i32.store (i32.const 0) (i32.const 0x7fa00000)))
  (func (export "reset") (i32.store (i32.const 0) (i32.const 0)))
)`);

// ./test/core/float_memory.wast:15
assert_return(() => invoke($0, `i32.load`, []), [value("i32", 2141192192)]);

// ./test/core/float_memory.wast:16
assert_return(() => invoke($0, `f32.load`, []), [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]);

// ./test/core/float_memory.wast:17
invoke($0, `reset`, []);

// ./test/core/float_memory.wast:18
assert_return(() => invoke($0, `i32.load`, []), [value("i32", 0)]);

// ./test/core/float_memory.wast:19
assert_return(() => invoke($0, `f32.load`, []), [value("f32", 0)]);

// ./test/core/float_memory.wast:20
invoke($0, `f32.store`, []);

// ./test/core/float_memory.wast:21
assert_return(() => invoke($0, `i32.load`, []), [value("i32", 2141192192)]);

// ./test/core/float_memory.wast:22
assert_return(() => invoke($0, `f32.load`, []), [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]);

// ./test/core/float_memory.wast:23
invoke($0, `reset`, []);

// ./test/core/float_memory.wast:24
assert_return(() => invoke($0, `i32.load`, []), [value("i32", 0)]);

// ./test/core/float_memory.wast:25
assert_return(() => invoke($0, `f32.load`, []), [value("f32", 0)]);

// ./test/core/float_memory.wast:26
invoke($0, `i32.store`, []);

// ./test/core/float_memory.wast:27
assert_return(() => invoke($0, `i32.load`, []), [value("i32", 2141192192)]);

// ./test/core/float_memory.wast:28
assert_return(() => invoke($0, `f32.load`, []), [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]);

// ./test/core/float_memory.wast:30
let $1 = instantiate(`(module
  (memory (data "\\00\\00\\00\\00\\00\\00\\f4\\7f"))

  (func (export "f64.load") (result f64) (f64.load (i32.const 0)))
  (func (export "i64.load") (result i64) (i64.load (i32.const 0)))
  (func (export "f64.store") (f64.store (i32.const 0) (f64.const nan:0x4000000000000)))
  (func (export "i64.store") (i64.store (i32.const 0) (i64.const 0x7ff4000000000000)))
  (func (export "reset") (i64.store (i32.const 0) (i64.const 0)))
)`);

// ./test/core/float_memory.wast:40
assert_return(() => invoke($1, `i64.load`, []), [value("i64", 9219994337134247936n)]);

// ./test/core/float_memory.wast:41
assert_return(
  () => invoke($1, `f64.load`, []),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f])],
);

// ./test/core/float_memory.wast:42
invoke($1, `reset`, []);

// ./test/core/float_memory.wast:43
assert_return(() => invoke($1, `i64.load`, []), [value("i64", 0n)]);

// ./test/core/float_memory.wast:44
assert_return(() => invoke($1, `f64.load`, []), [value("f64", 0)]);

// ./test/core/float_memory.wast:45
invoke($1, `f64.store`, []);

// ./test/core/float_memory.wast:46
assert_return(() => invoke($1, `i64.load`, []), [value("i64", 9219994337134247936n)]);

// ./test/core/float_memory.wast:47
assert_return(
  () => invoke($1, `f64.load`, []),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f])],
);

// ./test/core/float_memory.wast:48
invoke($1, `reset`, []);

// ./test/core/float_memory.wast:49
assert_return(() => invoke($1, `i64.load`, []), [value("i64", 0n)]);

// ./test/core/float_memory.wast:50
assert_return(() => invoke($1, `f64.load`, []), [value("f64", 0)]);

// ./test/core/float_memory.wast:51
invoke($1, `i64.store`, []);

// ./test/core/float_memory.wast:52
assert_return(() => invoke($1, `i64.load`, []), [value("i64", 9219994337134247936n)]);

// ./test/core/float_memory.wast:53
assert_return(
  () => invoke($1, `f64.load`, []),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f])],
);

// ./test/core/float_memory.wast:57
let $2 = instantiate(`(module
  (memory (data "\\00\\00\\00\\a0\\7f"))

  (func (export "f32.load") (result f32) (f32.load (i32.const 1)))
  (func (export "i32.load") (result i32) (i32.load (i32.const 1)))
  (func (export "f32.store") (f32.store (i32.const 1) (f32.const nan:0x200000)))
  (func (export "i32.store") (i32.store (i32.const 1) (i32.const 0x7fa00000)))
  (func (export "reset") (i32.store (i32.const 1) (i32.const 0)))
)`);

// ./test/core/float_memory.wast:67
assert_return(() => invoke($2, `i32.load`, []), [value("i32", 2141192192)]);

// ./test/core/float_memory.wast:68
assert_return(() => invoke($2, `f32.load`, []), [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]);

// ./test/core/float_memory.wast:69
invoke($2, `reset`, []);

// ./test/core/float_memory.wast:70
assert_return(() => invoke($2, `i32.load`, []), [value("i32", 0)]);

// ./test/core/float_memory.wast:71
assert_return(() => invoke($2, `f32.load`, []), [value("f32", 0)]);

// ./test/core/float_memory.wast:72
invoke($2, `f32.store`, []);

// ./test/core/float_memory.wast:73
assert_return(() => invoke($2, `i32.load`, []), [value("i32", 2141192192)]);

// ./test/core/float_memory.wast:74
assert_return(() => invoke($2, `f32.load`, []), [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]);

// ./test/core/float_memory.wast:75
invoke($2, `reset`, []);

// ./test/core/float_memory.wast:76
assert_return(() => invoke($2, `i32.load`, []), [value("i32", 0)]);

// ./test/core/float_memory.wast:77
assert_return(() => invoke($2, `f32.load`, []), [value("f32", 0)]);

// ./test/core/float_memory.wast:78
invoke($2, `i32.store`, []);

// ./test/core/float_memory.wast:79
assert_return(() => invoke($2, `i32.load`, []), [value("i32", 2141192192)]);

// ./test/core/float_memory.wast:80
assert_return(() => invoke($2, `f32.load`, []), [bytes("f32", [0x0, 0x0, 0xa0, 0x7f])]);

// ./test/core/float_memory.wast:82
let $3 = instantiate(`(module
  (memory (data "\\00\\00\\00\\00\\00\\00\\00\\f4\\7f"))

  (func (export "f64.load") (result f64) (f64.load (i32.const 1)))
  (func (export "i64.load") (result i64) (i64.load (i32.const 1)))
  (func (export "f64.store") (f64.store (i32.const 1) (f64.const nan:0x4000000000000)))
  (func (export "i64.store") (i64.store (i32.const 1) (i64.const 0x7ff4000000000000)))
  (func (export "reset") (i64.store (i32.const 1) (i64.const 0)))
)`);

// ./test/core/float_memory.wast:92
assert_return(() => invoke($3, `i64.load`, []), [value("i64", 9219994337134247936n)]);

// ./test/core/float_memory.wast:93
assert_return(
  () => invoke($3, `f64.load`, []),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f])],
);

// ./test/core/float_memory.wast:94
invoke($3, `reset`, []);

// ./test/core/float_memory.wast:95
assert_return(() => invoke($3, `i64.load`, []), [value("i64", 0n)]);

// ./test/core/float_memory.wast:96
assert_return(() => invoke($3, `f64.load`, []), [value("f64", 0)]);

// ./test/core/float_memory.wast:97
invoke($3, `f64.store`, []);

// ./test/core/float_memory.wast:98
assert_return(() => invoke($3, `i64.load`, []), [value("i64", 9219994337134247936n)]);

// ./test/core/float_memory.wast:99
assert_return(
  () => invoke($3, `f64.load`, []),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f])],
);

// ./test/core/float_memory.wast:100
invoke($3, `reset`, []);

// ./test/core/float_memory.wast:101
assert_return(() => invoke($3, `i64.load`, []), [value("i64", 0n)]);

// ./test/core/float_memory.wast:102
assert_return(() => invoke($3, `f64.load`, []), [value("f64", 0)]);

// ./test/core/float_memory.wast:103
invoke($3, `i64.store`, []);

// ./test/core/float_memory.wast:104
assert_return(() => invoke($3, `i64.load`, []), [value("i64", 9219994337134247936n)]);

// ./test/core/float_memory.wast:105
assert_return(
  () => invoke($3, `f64.load`, []),
  [bytes("f64", [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf4, 0x7f])],
);

// ./test/core/float_memory.wast:109
let $4 = instantiate(`(module
  (memory (data "\\01\\00\\d0\\7f"))

  (func (export "f32.load") (result f32) (f32.load (i32.const 0)))
  (func (export "i32.load") (result i32) (i32.load (i32.const 0)))
  (func (export "f32.store") (f32.store (i32.const 0) (f32.const nan:0x500001)))
  (func (export "i32.store") (i32.store (i32.const 0) (i32.const 0x7fd00001)))
  (func (export "reset") (i32.store (i32.const 0) (i32.const 0)))
)`);

// ./test/core/float_memory.wast:119
assert_return(() => invoke($4, `i32.load`, []), [value("i32", 2144337921)]);

// ./test/core/float_memory.wast:120
assert_return(() => invoke($4, `f32.load`, []), [bytes("f32", [0x1, 0x0, 0xd0, 0x7f])]);

// ./test/core/float_memory.wast:121
invoke($4, `reset`, []);

// ./test/core/float_memory.wast:122
assert_return(() => invoke($4, `i32.load`, []), [value("i32", 0)]);

// ./test/core/float_memory.wast:123
assert_return(() => invoke($4, `f32.load`, []), [value("f32", 0)]);

// ./test/core/float_memory.wast:124
invoke($4, `f32.store`, []);

// ./test/core/float_memory.wast:125
assert_return(() => invoke($4, `i32.load`, []), [value("i32", 2144337921)]);

// ./test/core/float_memory.wast:126
assert_return(() => invoke($4, `f32.load`, []), [bytes("f32", [0x1, 0x0, 0xd0, 0x7f])]);

// ./test/core/float_memory.wast:127
invoke($4, `reset`, []);

// ./test/core/float_memory.wast:128
assert_return(() => invoke($4, `i32.load`, []), [value("i32", 0)]);

// ./test/core/float_memory.wast:129
assert_return(() => invoke($4, `f32.load`, []), [value("f32", 0)]);

// ./test/core/float_memory.wast:130
invoke($4, `i32.store`, []);

// ./test/core/float_memory.wast:131
assert_return(() => invoke($4, `i32.load`, []), [value("i32", 2144337921)]);

// ./test/core/float_memory.wast:132
assert_return(() => invoke($4, `f32.load`, []), [bytes("f32", [0x1, 0x0, 0xd0, 0x7f])]);

// ./test/core/float_memory.wast:134
let $5 = instantiate(`(module
  (memory (data "\\01\\00\\00\\00\\00\\00\\fc\\7f"))

  (func (export "f64.load") (result f64) (f64.load (i32.const 0)))
  (func (export "i64.load") (result i64) (i64.load (i32.const 0)))
  (func (export "f64.store") (f64.store (i32.const 0) (f64.const nan:0xc000000000001)))
  (func (export "i64.store") (i64.store (i32.const 0) (i64.const 0x7ffc000000000001)))
  (func (export "reset") (i64.store (i32.const 0) (i64.const 0)))
)`);

// ./test/core/float_memory.wast:144
assert_return(() => invoke($5, `i64.load`, []), [value("i64", 9222246136947933185n)]);

// ./test/core/float_memory.wast:145
assert_return(
  () => invoke($5, `f64.load`, []),
  [bytes("f64", [0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0xfc, 0x7f])],
);

// ./test/core/float_memory.wast:146
invoke($5, `reset`, []);

// ./test/core/float_memory.wast:147
assert_return(() => invoke($5, `i64.load`, []), [value("i64", 0n)]);

// ./test/core/float_memory.wast:148
assert_return(() => invoke($5, `f64.load`, []), [value("f64", 0)]);

// ./test/core/float_memory.wast:149
invoke($5, `f64.store`, []);

// ./test/core/float_memory.wast:150
assert_return(() => invoke($5, `i64.load`, []), [value("i64", 9222246136947933185n)]);

// ./test/core/float_memory.wast:151
assert_return(
  () => invoke($5, `f64.load`, []),
  [bytes("f64", [0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0xfc, 0x7f])],
);

// ./test/core/float_memory.wast:152
invoke($5, `reset`, []);

// ./test/core/float_memory.wast:153
assert_return(() => invoke($5, `i64.load`, []), [value("i64", 0n)]);

// ./test/core/float_memory.wast:154
assert_return(() => invoke($5, `f64.load`, []), [value("f64", 0)]);

// ./test/core/float_memory.wast:155
invoke($5, `i64.store`, []);

// ./test/core/float_memory.wast:156
assert_return(() => invoke($5, `i64.load`, []), [value("i64", 9222246136947933185n)]);

// ./test/core/float_memory.wast:157
assert_return(
  () => invoke($5, `f64.load`, []),
  [bytes("f64", [0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0xfc, 0x7f])],
);
