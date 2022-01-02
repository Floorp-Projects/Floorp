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

// ./test/core/func_ptrs.wast

// ./test/core/func_ptrs.wast:1
let $0 = instantiate(`(module
  (type    (func))                           ;; 0: void -> void
  (type $$S (func))                           ;; 1: void -> void
  (type    (func (param)))                   ;; 2: void -> void
  (type    (func (result i32)))              ;; 3: void -> i32
  (type    (func (param) (result i32)))      ;; 4: void -> i32
  (type $$T (func (param i32) (result i32)))  ;; 5: i32 -> i32
  (type $$U (func (param i32)))               ;; 6: i32 -> void

  (func $$print (import "spectest" "print_i32") (type 6))

  (func (type 0))
  (func (type $$S))

  (func (export "one") (type 4) (i32.const 13))
  (func (export "two") (type $$T) (i32.add (local.get 0) (i32.const 1)))

  ;; Both signature and parameters are allowed (and required to match)
  ;; since this allows the naming of parameters.
  (func (export "three") (type $$T) (param $$a i32) (result i32)
    (i32.sub (local.get 0) (i32.const 2))
  )

  (func (export "four") (type $$U) (call $$print (local.get 0)))
)`);

// ./test/core/func_ptrs.wast:27
assert_return(() => invoke($0, `one`, []), [value("i32", 13)]);

// ./test/core/func_ptrs.wast:28
assert_return(() => invoke($0, `two`, [13]), [value("i32", 14)]);

// ./test/core/func_ptrs.wast:29
assert_return(() => invoke($0, `three`, [13]), [value("i32", 11)]);

// ./test/core/func_ptrs.wast:30
invoke($0, `four`, [83]);

// ./test/core/func_ptrs.wast:32
assert_invalid(
  () => instantiate(`(module (elem (i32.const 0)))`),
  `unknown table`,
);

// ./test/core/func_ptrs.wast:33
assert_invalid(
  () => instantiate(`(module (elem (i32.const 0) 0) (func))`),
  `unknown table`,
);

// ./test/core/func_ptrs.wast:35
assert_invalid(
  () => instantiate(`(module (table 1 funcref) (elem (i64.const 0)))`),
  `type mismatch`,
);

// ./test/core/func_ptrs.wast:39
assert_invalid(
  () =>
    instantiate(`(module (table 1 funcref) (elem (i32.ctz (i32.const 0))))`),
  `constant expression required`,
);

// ./test/core/func_ptrs.wast:43
assert_invalid(
  () => instantiate(`(module (table 1 funcref) (elem (nop)))`),
  `constant expression required`,
);

// ./test/core/func_ptrs.wast:48
assert_invalid(() => instantiate(`(module (func (type 42)))`), `unknown type`);

// ./test/core/func_ptrs.wast:49
assert_invalid(
  () =>
    instantiate(`(module (import "spectest" "print_i32" (func (type 43))))`),
  `unknown type`,
);

// ./test/core/func_ptrs.wast:51
let $1 = instantiate(`(module
  (type $$T (func (param) (result i32)))
  (type $$U (func (param) (result i32)))
  (table funcref (elem $$t1 $$t2 $$t3 $$u1 $$u2 $$t1 $$t3))

  (func $$t1 (type $$T) (i32.const 1))
  (func $$t2 (type $$T) (i32.const 2))
  (func $$t3 (type $$T) (i32.const 3))
  (func $$u1 (type $$U) (i32.const 4))
  (func $$u2 (type $$U) (i32.const 5))

  (func (export "callt") (param $$i i32) (result i32)
    (call_indirect (type $$T) (local.get $$i))
  )

  (func (export "callu") (param $$i i32) (result i32)
    (call_indirect (type $$U) (local.get $$i))
  )
)`);

// ./test/core/func_ptrs.wast:71
assert_return(() => invoke($1, `callt`, [0]), [value("i32", 1)]);

// ./test/core/func_ptrs.wast:72
assert_return(() => invoke($1, `callt`, [1]), [value("i32", 2)]);

// ./test/core/func_ptrs.wast:73
assert_return(() => invoke($1, `callt`, [2]), [value("i32", 3)]);

// ./test/core/func_ptrs.wast:74
assert_return(() => invoke($1, `callt`, [3]), [value("i32", 4)]);

// ./test/core/func_ptrs.wast:75
assert_return(() => invoke($1, `callt`, [4]), [value("i32", 5)]);

// ./test/core/func_ptrs.wast:76
assert_return(() => invoke($1, `callt`, [5]), [value("i32", 1)]);

// ./test/core/func_ptrs.wast:77
assert_return(() => invoke($1, `callt`, [6]), [value("i32", 3)]);

// ./test/core/func_ptrs.wast:78
assert_trap(() => invoke($1, `callt`, [7]), `undefined element`);

// ./test/core/func_ptrs.wast:79
assert_trap(() => invoke($1, `callt`, [100]), `undefined element`);

// ./test/core/func_ptrs.wast:80
assert_trap(() => invoke($1, `callt`, [-1]), `undefined element`);

// ./test/core/func_ptrs.wast:82
assert_return(() => invoke($1, `callu`, [0]), [value("i32", 1)]);

// ./test/core/func_ptrs.wast:83
assert_return(() => invoke($1, `callu`, [1]), [value("i32", 2)]);

// ./test/core/func_ptrs.wast:84
assert_return(() => invoke($1, `callu`, [2]), [value("i32", 3)]);

// ./test/core/func_ptrs.wast:85
assert_return(() => invoke($1, `callu`, [3]), [value("i32", 4)]);

// ./test/core/func_ptrs.wast:86
assert_return(() => invoke($1, `callu`, [4]), [value("i32", 5)]);

// ./test/core/func_ptrs.wast:87
assert_return(() => invoke($1, `callu`, [5]), [value("i32", 1)]);

// ./test/core/func_ptrs.wast:88
assert_return(() => invoke($1, `callu`, [6]), [value("i32", 3)]);

// ./test/core/func_ptrs.wast:89
assert_trap(() => invoke($1, `callu`, [7]), `undefined element`);

// ./test/core/func_ptrs.wast:90
assert_trap(() => invoke($1, `callu`, [100]), `undefined element`);

// ./test/core/func_ptrs.wast:91
assert_trap(() => invoke($1, `callu`, [-1]), `undefined element`);

// ./test/core/func_ptrs.wast:93
let $2 = instantiate(`(module
  (type $$T (func (result i32)))
  (table funcref (elem 0 1))

  (func $$t1 (type $$T) (i32.const 1))
  (func $$t2 (type $$T) (i32.const 2))

  (func (export "callt") (param $$i i32) (result i32)
    (call_indirect (type $$T) (local.get $$i))
  )
)`);

// ./test/core/func_ptrs.wast:105
assert_return(() => invoke($2, `callt`, [0]), [value("i32", 1)]);

// ./test/core/func_ptrs.wast:106
assert_return(() => invoke($2, `callt`, [1]), [value("i32", 2)]);
