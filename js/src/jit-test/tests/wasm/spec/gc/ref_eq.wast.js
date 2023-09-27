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

// ./test/core/gc/ref_eq.wast

// ./test/core/gc/ref_eq.wast:1
let $0 = instantiate(`(module
  (type $$st (sub (struct)))
  (type $$st' (sub (struct (field i32))))
  (type $$at (array i8))
  (type $$st-sub1 (sub $$st (struct)))
  (type $$st-sub2 (sub $$st (struct)))
  (type $$st'-sub1 (sub $$st' (struct (field i32))))
  (type $$st'-sub2 (sub $$st' (struct (field i32))))

  (table 20 (ref null eq))

  (func (export "init")
    (table.set (i32.const 0) (ref.null eq))
    (table.set (i32.const 1) (ref.null i31))
    (table.set (i32.const 2) (ref.i31 (i32.const 7)))
    (table.set (i32.const 3) (ref.i31 (i32.const 7)))
    (table.set (i32.const 4) (ref.i31 (i32.const 8)))
    (table.set (i32.const 5) (struct.new_default $$st))
    (table.set (i32.const 6) (struct.new_default $$st))
    (table.set (i32.const 7) (array.new_default $$at (i32.const 0)))
    (table.set (i32.const 8) (array.new_default $$at (i32.const 0)))
  )

  (func (export "eq") (param $$i i32) (param $$j i32) (result i32)
    (ref.eq (table.get (local.get $$i)) (table.get (local.get $$j)))
  )
)`);

// ./test/core/gc/ref_eq.wast:29
invoke($0, `init`, []);

// ./test/core/gc/ref_eq.wast:31
assert_return(() => invoke($0, `eq`, [0, 0]), [value("i32", 1)]);

// ./test/core/gc/ref_eq.wast:32
assert_return(() => invoke($0, `eq`, [0, 1]), [value("i32", 1)]);

// ./test/core/gc/ref_eq.wast:33
assert_return(() => invoke($0, `eq`, [0, 2]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:34
assert_return(() => invoke($0, `eq`, [0, 3]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:35
assert_return(() => invoke($0, `eq`, [0, 4]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:36
assert_return(() => invoke($0, `eq`, [0, 5]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:37
assert_return(() => invoke($0, `eq`, [0, 6]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:38
assert_return(() => invoke($0, `eq`, [0, 7]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:39
assert_return(() => invoke($0, `eq`, [0, 8]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:41
assert_return(() => invoke($0, `eq`, [1, 0]), [value("i32", 1)]);

// ./test/core/gc/ref_eq.wast:42
assert_return(() => invoke($0, `eq`, [1, 1]), [value("i32", 1)]);

// ./test/core/gc/ref_eq.wast:43
assert_return(() => invoke($0, `eq`, [1, 2]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:44
assert_return(() => invoke($0, `eq`, [1, 3]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:45
assert_return(() => invoke($0, `eq`, [1, 4]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:46
assert_return(() => invoke($0, `eq`, [1, 5]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:47
assert_return(() => invoke($0, `eq`, [1, 6]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:48
assert_return(() => invoke($0, `eq`, [1, 7]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:49
assert_return(() => invoke($0, `eq`, [1, 8]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:51
assert_return(() => invoke($0, `eq`, [2, 0]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:52
assert_return(() => invoke($0, `eq`, [2, 1]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:53
assert_return(() => invoke($0, `eq`, [2, 2]), [value("i32", 1)]);

// ./test/core/gc/ref_eq.wast:54
assert_return(() => invoke($0, `eq`, [2, 3]), [value("i32", 1)]);

// ./test/core/gc/ref_eq.wast:55
assert_return(() => invoke($0, `eq`, [2, 4]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:56
assert_return(() => invoke($0, `eq`, [2, 5]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:57
assert_return(() => invoke($0, `eq`, [2, 6]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:58
assert_return(() => invoke($0, `eq`, [2, 7]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:59
assert_return(() => invoke($0, `eq`, [2, 8]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:61
assert_return(() => invoke($0, `eq`, [3, 0]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:62
assert_return(() => invoke($0, `eq`, [3, 1]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:63
assert_return(() => invoke($0, `eq`, [3, 2]), [value("i32", 1)]);

// ./test/core/gc/ref_eq.wast:64
assert_return(() => invoke($0, `eq`, [3, 3]), [value("i32", 1)]);

// ./test/core/gc/ref_eq.wast:65
assert_return(() => invoke($0, `eq`, [3, 4]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:66
assert_return(() => invoke($0, `eq`, [3, 5]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:67
assert_return(() => invoke($0, `eq`, [3, 6]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:68
assert_return(() => invoke($0, `eq`, [3, 7]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:69
assert_return(() => invoke($0, `eq`, [3, 8]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:71
assert_return(() => invoke($0, `eq`, [4, 0]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:72
assert_return(() => invoke($0, `eq`, [4, 1]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:73
assert_return(() => invoke($0, `eq`, [4, 2]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:74
assert_return(() => invoke($0, `eq`, [4, 3]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:75
assert_return(() => invoke($0, `eq`, [4, 4]), [value("i32", 1)]);

// ./test/core/gc/ref_eq.wast:76
assert_return(() => invoke($0, `eq`, [4, 5]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:77
assert_return(() => invoke($0, `eq`, [4, 6]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:78
assert_return(() => invoke($0, `eq`, [4, 7]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:79
assert_return(() => invoke($0, `eq`, [4, 8]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:81
assert_return(() => invoke($0, `eq`, [5, 0]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:82
assert_return(() => invoke($0, `eq`, [5, 1]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:83
assert_return(() => invoke($0, `eq`, [5, 2]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:84
assert_return(() => invoke($0, `eq`, [5, 3]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:85
assert_return(() => invoke($0, `eq`, [5, 4]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:86
assert_return(() => invoke($0, `eq`, [5, 5]), [value("i32", 1)]);

// ./test/core/gc/ref_eq.wast:87
assert_return(() => invoke($0, `eq`, [5, 6]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:88
assert_return(() => invoke($0, `eq`, [5, 7]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:89
assert_return(() => invoke($0, `eq`, [5, 8]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:91
assert_return(() => invoke($0, `eq`, [6, 0]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:92
assert_return(() => invoke($0, `eq`, [6, 1]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:93
assert_return(() => invoke($0, `eq`, [6, 2]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:94
assert_return(() => invoke($0, `eq`, [6, 3]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:95
assert_return(() => invoke($0, `eq`, [6, 4]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:96
assert_return(() => invoke($0, `eq`, [6, 5]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:97
assert_return(() => invoke($0, `eq`, [6, 6]), [value("i32", 1)]);

// ./test/core/gc/ref_eq.wast:98
assert_return(() => invoke($0, `eq`, [6, 7]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:99
assert_return(() => invoke($0, `eq`, [6, 8]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:101
assert_return(() => invoke($0, `eq`, [7, 0]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:102
assert_return(() => invoke($0, `eq`, [7, 1]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:103
assert_return(() => invoke($0, `eq`, [7, 2]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:104
assert_return(() => invoke($0, `eq`, [7, 3]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:105
assert_return(() => invoke($0, `eq`, [7, 4]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:106
assert_return(() => invoke($0, `eq`, [7, 5]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:107
assert_return(() => invoke($0, `eq`, [7, 6]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:108
assert_return(() => invoke($0, `eq`, [7, 7]), [value("i32", 1)]);

// ./test/core/gc/ref_eq.wast:109
assert_return(() => invoke($0, `eq`, [7, 8]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:111
assert_return(() => invoke($0, `eq`, [8, 0]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:112
assert_return(() => invoke($0, `eq`, [8, 1]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:113
assert_return(() => invoke($0, `eq`, [8, 2]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:114
assert_return(() => invoke($0, `eq`, [8, 3]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:115
assert_return(() => invoke($0, `eq`, [8, 4]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:116
assert_return(() => invoke($0, `eq`, [8, 5]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:117
assert_return(() => invoke($0, `eq`, [8, 6]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:118
assert_return(() => invoke($0, `eq`, [8, 7]), [value("i32", 0)]);

// ./test/core/gc/ref_eq.wast:119
assert_return(() => invoke($0, `eq`, [8, 8]), [value("i32", 1)]);

// ./test/core/gc/ref_eq.wast:121
assert_invalid(
  () => instantiate(`(module
    (func (export "eq") (param $$r (ref any)) (result i32)
      (ref.eq (local.get $$r) (local.get $$r))
    )
  )`),
  `type mismatch`,
);

// ./test/core/gc/ref_eq.wast:129
assert_invalid(
  () => instantiate(`(module
    (func (export "eq") (param $$r (ref null any)) (result i32)
      (ref.eq (local.get $$r) (local.get $$r))
    )
  )`),
  `type mismatch`,
);

// ./test/core/gc/ref_eq.wast:137
assert_invalid(
  () => instantiate(`(module
    (func (export "eq") (param $$r (ref func)) (result i32)
      (ref.eq (local.get $$r) (local.get $$r))
    )
  )`),
  `type mismatch`,
);

// ./test/core/gc/ref_eq.wast:145
assert_invalid(
  () => instantiate(`(module
    (func (export "eq") (param $$r (ref null func)) (result i32)
      (ref.eq (local.get $$r) (local.get $$r))
    )
  )`),
  `type mismatch`,
);

// ./test/core/gc/ref_eq.wast:153
assert_invalid(
  () => instantiate(`(module
    (func (export "eq") (param $$r (ref extern)) (result i32)
      (ref.eq (local.get $$r) (local.get $$r))
    )
  )`),
  `type mismatch`,
);

// ./test/core/gc/ref_eq.wast:161
assert_invalid(
  () => instantiate(`(module
    (func (export "eq") (param $$r (ref null extern)) (result i32)
      (ref.eq (local.get $$r) (local.get $$r))
    )
  )`),
  `type mismatch`,
);
