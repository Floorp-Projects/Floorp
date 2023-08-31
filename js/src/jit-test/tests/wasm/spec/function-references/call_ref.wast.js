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

// ./test/core/call_ref.wast

// ./test/core/call_ref.wast:1
let $0 = instantiate(`(module
  (type $$ii (func (param i32) (result i32)))

  (func $$apply (param $$f (ref $$ii)) (param $$x i32) (result i32)
    (call_ref $$ii (local.get $$x) (local.get $$f))
  )

  (func $$f (type $$ii) (i32.mul (local.get 0) (local.get 0)))
  (func $$g (type $$ii) (i32.sub (i32.const 0) (local.get 0)))

  (elem declare func $$f $$g)

  (func (export "run") (param $$x i32) (result i32)
    (local $$rf (ref null $$ii))
    (local $$rg (ref null $$ii))
    (local.set $$rf (ref.func $$f))
    (local.set $$rg (ref.func $$g))
    (call_ref $$ii (call_ref $$ii (local.get $$x) (local.get $$rf)) (local.get $$rg))
  )

  (func (export "null") (result i32)
    (call_ref $$ii (i32.const 1) (ref.null $$ii))
  )

  ;; Recursion

  (type $$ll (func (param i64) (result i64)))
  (type $$lll (func (param i64 i64) (result i64)))

  (elem declare func $$fac)
  (global $$fac (ref $$ll) (ref.func $$fac))

  (func $$fac (export "fac") (type $$ll)
    (if (result i64) (i64.eqz (local.get 0))
      (then (i64.const 1))
      (else
        (i64.mul
          (local.get 0)
          (call_ref $$ll (i64.sub (local.get 0) (i64.const 1)) (global.get $$fac))
        )
      )
    )
  )

  (elem declare func $$fac-acc)
  (global $$fac-acc (ref $$lll) (ref.func $$fac-acc))

  (func $$fac-acc (export "fac-acc") (type $$lll)
    (if (result i64) (i64.eqz (local.get 0))
      (then (local.get 1))
      (else
        (call_ref $$lll
          (i64.sub (local.get 0) (i64.const 1))
          (i64.mul (local.get 0) (local.get 1))
          (global.get $$fac-acc)
        )
      )
    )
  )

  (elem declare func $$fib)
  (global $$fib (ref $$ll) (ref.func $$fib))

  (func $$fib (export "fib") (type $$ll)
    (if (result i64) (i64.le_u (local.get 0) (i64.const 1))
      (then (i64.const 1))
      (else
        (i64.add
          (call_ref $$ll (i64.sub (local.get 0) (i64.const 2)) (global.get $$fib))
          (call_ref $$ll (i64.sub (local.get 0) (i64.const 1)) (global.get $$fib))
        )
      )
    )
  )

  (elem declare func $$even $$odd)
  (global $$even (ref $$ll) (ref.func $$even))
  (global $$odd (ref $$ll) (ref.func $$odd))

  (func $$even (export "even") (type $$ll)
    (if (result i64) (i64.eqz (local.get 0))
      (then (i64.const 44))
      (else (call_ref $$ll (i64.sub (local.get 0) (i64.const 1)) (global.get $$odd)))
    )
  )
  (func $$odd (export "odd") (type $$ll)
    (if (result i64) (i64.eqz (local.get 0))
      (then (i64.const 99))
      (else (call_ref $$ll (i64.sub (local.get 0) (i64.const 1)) (global.get $$even)))
    )
  )
)`);

// ./test/core/call_ref.wast:94
assert_return(() => invoke($0, `run`, [0]), [value("i32", 0)]);

// ./test/core/call_ref.wast:95
assert_return(() => invoke($0, `run`, [3]), [value("i32", -9)]);

// ./test/core/call_ref.wast:97
assert_trap(() => invoke($0, `null`, []), `null function`);

// ./test/core/call_ref.wast:99
assert_return(() => invoke($0, `fac`, [0n]), [value("i64", 1n)]);

// ./test/core/call_ref.wast:100
assert_return(() => invoke($0, `fac`, [1n]), [value("i64", 1n)]);

// ./test/core/call_ref.wast:101
assert_return(() => invoke($0, `fac`, [5n]), [value("i64", 120n)]);

// ./test/core/call_ref.wast:102
assert_return(() => invoke($0, `fac`, [25n]), [value("i64", 7034535277573963776n)]);

// ./test/core/call_ref.wast:103
assert_return(() => invoke($0, `fac-acc`, [0n, 1n]), [value("i64", 1n)]);

// ./test/core/call_ref.wast:104
assert_return(() => invoke($0, `fac-acc`, [1n, 1n]), [value("i64", 1n)]);

// ./test/core/call_ref.wast:105
assert_return(() => invoke($0, `fac-acc`, [5n, 1n]), [value("i64", 120n)]);

// ./test/core/call_ref.wast:106
assert_return(() => invoke($0, `fac-acc`, [25n, 1n]), [value("i64", 7034535277573963776n)]);

// ./test/core/call_ref.wast:111
assert_return(() => invoke($0, `fib`, [0n]), [value("i64", 1n)]);

// ./test/core/call_ref.wast:112
assert_return(() => invoke($0, `fib`, [1n]), [value("i64", 1n)]);

// ./test/core/call_ref.wast:113
assert_return(() => invoke($0, `fib`, [2n]), [value("i64", 2n)]);

// ./test/core/call_ref.wast:114
assert_return(() => invoke($0, `fib`, [5n]), [value("i64", 8n)]);

// ./test/core/call_ref.wast:115
assert_return(() => invoke($0, `fib`, [20n]), [value("i64", 10946n)]);

// ./test/core/call_ref.wast:117
assert_return(() => invoke($0, `even`, [0n]), [value("i64", 44n)]);

// ./test/core/call_ref.wast:118
assert_return(() => invoke($0, `even`, [1n]), [value("i64", 99n)]);

// ./test/core/call_ref.wast:119
assert_return(() => invoke($0, `even`, [100n]), [value("i64", 44n)]);

// ./test/core/call_ref.wast:120
assert_return(() => invoke($0, `even`, [77n]), [value("i64", 99n)]);

// ./test/core/call_ref.wast:121
assert_return(() => invoke($0, `odd`, [0n]), [value("i64", 99n)]);

// ./test/core/call_ref.wast:122
assert_return(() => invoke($0, `odd`, [1n]), [value("i64", 44n)]);

// ./test/core/call_ref.wast:123
assert_return(() => invoke($0, `odd`, [200n]), [value("i64", 99n)]);

// ./test/core/call_ref.wast:124
assert_return(() => invoke($0, `odd`, [77n]), [value("i64", 44n)]);

// ./test/core/call_ref.wast:129
let $1 = instantiate(`(module
  (type $$t (func))
  (func (export "unreachable") (result i32)
    (unreachable)
    (call_ref $$t)
  )
)`);

// ./test/core/call_ref.wast:136
assert_trap(() => invoke($1, `unreachable`, []), `unreachable`);

// ./test/core/call_ref.wast:138
let $2 = instantiate(`(module
  (elem declare func $$f)
  (type $$t (func (param i32) (result i32)))
  (func $$f (param i32) (result i32) (local.get 0))

  (func (export "unreachable") (result i32)
    (unreachable)
    (ref.func $$f)
    (call_ref $$t)
  )
)`);

// ./test/core/call_ref.wast:149
assert_trap(() => invoke($2, `unreachable`, []), `unreachable`);

// ./test/core/call_ref.wast:151
let $3 = instantiate(`(module
  (elem declare func $$f)
  (type $$t (func (param i32) (result i32)))
  (func $$f (param i32) (result i32) (local.get 0))

  (func (export "unreachable") (result i32)
    (unreachable)
    (i32.const 0)
    (ref.func $$f)
    (call_ref $$t)
    (drop)
    (i32.const 0)
  )
)`);

// ./test/core/call_ref.wast:165
assert_trap(() => invoke($3, `unreachable`, []), `unreachable`);

// ./test/core/call_ref.wast:167
assert_invalid(
  () => instantiate(`(module
    (elem declare func $$f)
    (type $$t (func (param i32) (result i32)))
    (func $$f (param i32) (result i32) (local.get 0))

    (func (export "unreachable") (result i32)
      (unreachable)
      (i64.const 0)
      (ref.func $$f)
      (call_ref $$t)
    )
  )`),
  `type mismatch`,
);

// ./test/core/call_ref.wast:183
assert_invalid(
  () => instantiate(`(module
    (elem declare func $$f)
    (type $$t (func (param i32) (result i32)))
    (func $$f (param i32) (result i32) (local.get 0))

    (func (export "unreachable") (result i32)
      (unreachable)
      (ref.func $$f)
      (call_ref $$t)
      (drop)
      (i64.const 0)
    )
  )`),
  `type mismatch`,
);

// ./test/core/call_ref.wast:200
assert_invalid(
  () => instantiate(`(module
    (type $$t (func))
    (func $$f (param $$r externref)
      (call_ref $$t (local.get $$r))
    )
  )`),
  `type mismatch`,
);
