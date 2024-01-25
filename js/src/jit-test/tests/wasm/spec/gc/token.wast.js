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

// ./test/core/token.wast

// ./test/core/token.wast:3
assert_malformed(() => instantiate(`(func (drop (i32.const0))) `), `unknown operator`);

// ./test/core/token.wast:7
assert_malformed(() => instantiate(`(func br 0drop) `), `unknown operator`);

// ./test/core/token.wast:15
let $0 = instantiate(`(module
  (func(nop))
)`);

// ./test/core/token.wast:18
let $1 = instantiate(`(module
  (func (nop)nop)
)`);

// ./test/core/token.wast:21
let $2 = instantiate(`(module
  (func nop(nop))
)`);

// ./test/core/token.wast:24
let $3 = instantiate(`(module
  (func(nop)(nop))
)`);

// ./test/core/token.wast:27
let $4 = instantiate(`(module
  (func $$f(nop))
)`);

// ./test/core/token.wast:30
let $5 = instantiate(`(module
  (func br 0(nop))
)`);

// ./test/core/token.wast:33
let $6 = instantiate(`(module
  (table 1 funcref)
  (func)
  (elem (i32.const 0)0)
)`);

// ./test/core/token.wast:38
let $7 = instantiate(`(module
  (table 1 funcref)
  (func $$f)
  (elem (i32.const 0)$$f)
)`);

// ./test/core/token.wast:43
let $8 = instantiate(`(module
  (memory 1)
  (data (i32.const 0)"a")
)`);

// ./test/core/token.wast:47
let $9 = instantiate(`(module
  (import "spectest" "print"(func))
)`);

// ./test/core/token.wast:54
let $10 = instantiate(`(module
  (func;;bla
  )
)`);

// ./test/core/token.wast:58
let $11 = instantiate(`(module
  (func (nop);;bla
  )
)`);

// ./test/core/token.wast:62
let $12 = instantiate(`(module
  (func nop;;bla
  )
)`);

// ./test/core/token.wast:66
let $13 = instantiate(`(module
  (func $$f;;bla
  )
)`);

// ./test/core/token.wast:70
let $14 = instantiate(`(module
  (func br 0;;bla
  )
)`);

// ./test/core/token.wast:74
let $15 = instantiate(`(module
  (data "a";;bla
  )
)`);

// ./test/core/token.wast:82
let $16 = instantiate(`(module
  (func (block $$l (i32.const 0) (br_table 0 $$l)))
)`);

// ./test/core/token.wast:85
assert_malformed(
  () => instantiate(`(func (block $$l (i32.const 0) (br_table 0$$l))) `),
  `unknown operator`,
);

// ./test/core/token.wast:92
let $17 = instantiate(`(module
  (func (block $$l (i32.const 0) (br_table $$l 0)))
)`);

// ./test/core/token.wast:95
assert_malformed(
  () => instantiate(`(func (block $$l (i32.const 0) (br_table $$l0))) `),
  `unknown label`,
);

// ./test/core/token.wast:102
let $18 = instantiate(`(module
  (func (block $$l (i32.const 0) (br_table $$l $$l)))
)`);

// ./test/core/token.wast:105
assert_malformed(
  () => instantiate(`(func (block $$l (i32.const 0) (br_table $$l$$l))) `),
  `unknown label`,
);

// ./test/core/token.wast:112
let $19 = instantiate(`(module
  (func (block $$l0 (i32.const 0) (br_table $$l0)))
)`);

// ./test/core/token.wast:115
let $20 = instantiate(`(module
  (func (block $$l$$l (i32.const 0) (br_table $$l$$l)))
)`);

// ./test/core/token.wast:122
let $21 = instantiate(`(module
  (data "a")
)`);

// ./test/core/token.wast:125
assert_malformed(() => instantiate(`(data"a") `), `unknown operator`);

// ./test/core/token.wast:132
let $22 = instantiate(`(module
  (data $$l "a")
)`);

// ./test/core/token.wast:135
assert_malformed(() => instantiate(`(data $$l"a") `), `unknown operator`);

// ./test/core/token.wast:142
let $23 = instantiate(`(module
  (data $$l " a")
)`);

// ./test/core/token.wast:145
assert_malformed(() => instantiate(`(data $$l" a") `), `unknown operator`);

// ./test/core/token.wast:152
let $24 = instantiate(`(module
  (data $$l "a ")
)`);

// ./test/core/token.wast:155
assert_malformed(() => instantiate(`(data $$l"a ") `), `unknown operator`);

// ./test/core/token.wast:162
let $25 = instantiate(`(module
  (data $$l "a " "b")
)`);

// ./test/core/token.wast:165
assert_malformed(() => instantiate(`(data $$l"a ""b") `), `unknown operator`);

// ./test/core/token.wast:172
let $26 = instantiate(`(module
  (data $$l "\u{f61a}\u{f4a9}")
)`);

// ./test/core/token.wast:175
assert_malformed(() => instantiate(`(data $$l"\u{f61a}\u{f4a9}") `), `unknown operator`);

// ./test/core/token.wast:182
let $27 = instantiate(`(module
  (data $$l " \u{f61a}\u{f4a9}")
)`);

// ./test/core/token.wast:185
assert_malformed(() => instantiate(`(data $$l" \u{f61a}\u{f4a9}") `), `unknown operator`);

// ./test/core/token.wast:192
let $28 = instantiate(`(module
  (data $$l "\u{f61a}\u{f4a9} ")
)`);

// ./test/core/token.wast:195
assert_malformed(() => instantiate(`(data $$l"\u{f61a}\u{f4a9} ") `), `unknown operator`);

// ./test/core/token.wast:202
let $29 = instantiate(`(module
  (data "a" "b")
)`);

// ./test/core/token.wast:205
assert_malformed(() => instantiate(`(data "a""b") `), `unknown operator`);

// ./test/core/token.wast:212
let $30 = instantiate(`(module
  (data "a" " b")
)`);

// ./test/core/token.wast:215
assert_malformed(() => instantiate(`(data "a"" b") `), `unknown operator`);

// ./test/core/token.wast:222
let $31 = instantiate(`(module
  (data "a " "b")
)`);

// ./test/core/token.wast:225
assert_malformed(() => instantiate(`(data "a ""b") `), `unknown operator`);

// ./test/core/token.wast:232
let $32 = instantiate(`(module
  (data "\u{f61a}\u{f4a9}" "\u{f61a}\u{f4a9}")
)`);

// ./test/core/token.wast:235
assert_malformed(
  () => instantiate(`(data "\u{f61a}\u{f4a9}""\u{f61a}\u{f4a9}") `),
  `unknown operator`,
);

// ./test/core/token.wast:242
let $33 = instantiate(`(module
  (data "\u{f61a}\u{f4a9}" " \u{f61a}\u{f4a9}")
)`);

// ./test/core/token.wast:245
assert_malformed(
  () => instantiate(`(data "\u{f61a}\u{f4a9}"" \u{f61a}\u{f4a9}") `),
  `unknown operator`,
);

// ./test/core/token.wast:252
let $34 = instantiate(`(module
  (data "\u{f61a}\u{f4a9} " "\u{f61a}\u{f4a9}")
)`);

// ./test/core/token.wast:255
assert_malformed(
  () => instantiate(`(data "\u{f61a}\u{f4a9} ""\u{f61a}\u{f4a9}") `),
  `unknown operator`,
);

// ./test/core/token.wast:263
assert_malformed(() => instantiate(`(func "a"x) `), `unknown operator`);

// ./test/core/token.wast:269
assert_malformed(() => instantiate(`(func "a"0) `), `unknown operator`);

// ./test/core/token.wast:275
assert_malformed(() => instantiate(`(func 0"a") `), `unknown operator`);

// ./test/core/token.wast:281
assert_malformed(() => instantiate(`(func "a"$$x) `), `unknown operator`);
