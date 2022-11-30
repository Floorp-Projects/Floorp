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

// ./test/core/tokens.wast

// ./test/core/tokens.wast:3
let $0 = instantiate(`(module
  (func(nop))
)`);

// ./test/core/tokens.wast:6
let $1 = instantiate(`(module
  (func (nop)nop)
)`);

// ./test/core/tokens.wast:9
let $2 = instantiate(`(module
  (func nop(nop))
)`);

// ./test/core/tokens.wast:12
let $3 = instantiate(`(module
  (func(nop)(nop))
)`);

// ./test/core/tokens.wast:15
let $4 = instantiate(`(module
  (func $$f(nop))
)`);

// ./test/core/tokens.wast:18
let $5 = instantiate(`(module
  (func br 0(nop))
)`);

// ./test/core/tokens.wast:21
let $6 = instantiate(`(module
  (table 1 funcref)
  (func)
  (elem (i32.const 0)0)
)`);

// ./test/core/tokens.wast:26
let $7 = instantiate(`(module
  (table 1 funcref)
  (func $$f)
  (elem (i32.const 0)$$f)
)`);

// ./test/core/tokens.wast:31
let $8 = instantiate(`(module
  (memory 1)
  (data (i32.const 0)"a")
)`);

// ./test/core/tokens.wast:35
let $9 = instantiate(`(module
  (import "spectest" "print"(func))
)`);

// ./test/core/tokens.wast:42
let $10 = instantiate(`(module
  (func;;bla
  )
)`);

// ./test/core/tokens.wast:46
let $11 = instantiate(`(module
  (func (nop);;bla
  )
)`);

// ./test/core/tokens.wast:50
let $12 = instantiate(`(module
  (func nop;;bla
  )
)`);

// ./test/core/tokens.wast:54
let $13 = instantiate(`(module
  (func $$f;;bla
  )
)`);

// ./test/core/tokens.wast:58
let $14 = instantiate(`(module
  (func br 0;;bla
  )
)`);

// ./test/core/tokens.wast:62
let $15 = instantiate(`(module
  (data "a";;bla
  )
)`);

// ./test/core/tokens.wast:70
let $16 = instantiate(`(module
  (func (block $$l (i32.const 0) (br_table 0 $$l)))
)`);

// ./test/core/tokens.wast:73
assert_malformed(
  () => instantiate(`(func (block $$l (i32.const 0) (br_table 0$$l))) `),
  `unknown operator`,
);

// ./test/core/tokens.wast:80
let $17 = instantiate(`(module
  (func (block $$l (i32.const 0) (br_table $$l 0)))
)`);

// ./test/core/tokens.wast:83
assert_malformed(
  () => instantiate(`(func (block $$l (i32.const 0) (br_table $$l0))) `),
  `unknown label`,
);

// ./test/core/tokens.wast:90
let $18 = instantiate(`(module
  (func (block $$l (i32.const 0) (br_table $$l $$l)))
)`);

// ./test/core/tokens.wast:93
assert_malformed(
  () => instantiate(`(func (block $$l (i32.const 0) (br_table $$l$$l))) `),
  `unknown label`,
);

// ./test/core/tokens.wast:100
let $19 = instantiate(`(module
  (func (block $$l0 (i32.const 0) (br_table $$l0)))
)`);

// ./test/core/tokens.wast:103
let $20 = instantiate(`(module
  (func (block $$l$$l (i32.const 0) (br_table $$l$$l)))
)`);

// ./test/core/tokens.wast:110
let $21 = instantiate(`(module
  (data "a")
)`);

// ./test/core/tokens.wast:113
assert_malformed(() => instantiate(`(data"a") `), `unknown operator`);

// ./test/core/tokens.wast:120
let $22 = instantiate(`(module
  (data $$l "a")
)`);

// ./test/core/tokens.wast:123
assert_malformed(() => instantiate(`(data $$l"a") `), `unknown operator`);

// ./test/core/tokens.wast:130
let $23 = instantiate(`(module
  (data $$l " a")
)`);

// ./test/core/tokens.wast:133
assert_malformed(() => instantiate(`(data $$l" a") `), `unknown operator`);

// ./test/core/tokens.wast:140
let $24 = instantiate(`(module
  (data $$l "a ")
)`);

// ./test/core/tokens.wast:143
assert_malformed(() => instantiate(`(data $$l"a ") `), `unknown operator`);

// ./test/core/tokens.wast:150
let $25 = instantiate(`(module
  (data $$l "a " "b")
)`);

// ./test/core/tokens.wast:153
assert_malformed(() => instantiate(`(data $$l"a ""b") `), `unknown operator`);

// ./test/core/tokens.wast:160
let $26 = instantiate(`(module
  (data $$l "\u{f61a}\u{f4a9}")
)`);

// ./test/core/tokens.wast:163
assert_malformed(() => instantiate(`(data $$l"\u{f61a}\u{f4a9}") `), `unknown operator`);

// ./test/core/tokens.wast:170
let $27 = instantiate(`(module
  (data $$l " \u{f61a}\u{f4a9}")
)`);

// ./test/core/tokens.wast:173
assert_malformed(() => instantiate(`(data $$l" \u{f61a}\u{f4a9}") `), `unknown operator`);

// ./test/core/tokens.wast:180
let $28 = instantiate(`(module
  (data $$l "\u{f61a}\u{f4a9} ")
)`);

// ./test/core/tokens.wast:183
assert_malformed(() => instantiate(`(data $$l"\u{f61a}\u{f4a9} ") `), `unknown operator`);

// ./test/core/tokens.wast:190
let $29 = instantiate(`(module
  (data "a" "b")
)`);

// ./test/core/tokens.wast:193
assert_malformed(() => instantiate(`(data "a""b") `), `unknown operator`);

// ./test/core/tokens.wast:200
let $30 = instantiate(`(module
  (data "a" " b")
)`);

// ./test/core/tokens.wast:203
assert_malformed(() => instantiate(`(data "a"" b") `), `unknown operator`);

// ./test/core/tokens.wast:210
let $31 = instantiate(`(module
  (data "a " "b")
)`);

// ./test/core/tokens.wast:213
assert_malformed(() => instantiate(`(data "a ""b") `), `unknown operator`);

// ./test/core/tokens.wast:220
let $32 = instantiate(`(module
  (data "\u{f61a}\u{f4a9}" "\u{f61a}\u{f4a9}")
)`);

// ./test/core/tokens.wast:223
assert_malformed(
  () => instantiate(`(data "\u{f61a}\u{f4a9}""\u{f61a}\u{f4a9}") `),
  `unknown operator`,
);

// ./test/core/tokens.wast:230
let $33 = instantiate(`(module
  (data "\u{f61a}\u{f4a9}" " \u{f61a}\u{f4a9}")
)`);

// ./test/core/tokens.wast:233
assert_malformed(
  () => instantiate(`(data "\u{f61a}\u{f4a9}"" \u{f61a}\u{f4a9}") `),
  `unknown operator`,
);

// ./test/core/tokens.wast:240
let $34 = instantiate(`(module
  (data "\u{f61a}\u{f4a9} " "\u{f61a}\u{f4a9}")
)`);

// ./test/core/tokens.wast:243
assert_malformed(
  () => instantiate(`(data "\u{f61a}\u{f4a9} ""\u{f61a}\u{f4a9}") `),
  `unknown operator`,
);

// ./test/core/tokens.wast:251
assert_malformed(() => instantiate(`(func "a"x) `), `unknown operator`);

// ./test/core/tokens.wast:257
assert_malformed(() => instantiate(`(func "a"0) `), `unknown operator`);

// ./test/core/tokens.wast:263
assert_malformed(() => instantiate(`(func 0"a") `), `unknown operator`);

// ./test/core/tokens.wast:269
assert_malformed(() => instantiate(`(func "a"$$x) `), `unknown operator`);
