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

// ./test/core/unreached-valid.wast

// ./test/core/unreached-valid.wast:1
let $0 = instantiate(`(module

  ;; Check that both sides of the select are evaluated
  (func (export "select-trap-left") (param $$cond i32) (result i32)
    (select (unreachable) (i32.const 0) (local.get $$cond))
  )
  (func (export "select-trap-right") (param $$cond i32) (result i32)
    (select (i32.const 0) (unreachable) (local.get $$cond))
  )

  (func (export "select-unreached")
    (unreachable) (select)
    (unreachable) (i32.const 0) (select)
    (unreachable) (i32.const 0) (i32.const 0) (select)
    (unreachable) (i32.const 0) (i32.const 0) (i32.const 0) (select)
    (unreachable) (f32.const 0) (i32.const 0) (select)
    (unreachable)
  )

  (func (export "select-unreached-result1") (result i32)
    (unreachable) (i32.add (select))
  )

  (func (export "select-unreached-result2") (result i64)
    (unreachable) (i64.add (select (i64.const 0) (i32.const 0)))
  )

  (func (export "select-unreached-num")
    (unreachable)
    (select)
    (i32.eqz)
    (drop)
  )
  (func (export "select-unreached-ref")
    (unreachable)
    (select)
    (ref.is_null)
    (drop)
  )

  (type $$t (func (param i32) (result i32)))
  (func (export "call_ref-unreached") (result i32)
    (unreachable)
    (call_ref $$t)
  )
)`);

// ./test/core/unreached-valid.wast:48
assert_trap(() => invoke($0, `select-trap-left`, [1]), `unreachable`);

// ./test/core/unreached-valid.wast:49
assert_trap(() => invoke($0, `select-trap-left`, [0]), `unreachable`);

// ./test/core/unreached-valid.wast:50
assert_trap(() => invoke($0, `select-trap-right`, [1]), `unreachable`);

// ./test/core/unreached-valid.wast:51
assert_trap(() => invoke($0, `select-trap-right`, [0]), `unreachable`);

// ./test/core/unreached-valid.wast:53
assert_trap(() => invoke($0, `select-unreached-result1`, []), `unreachable`);

// ./test/core/unreached-valid.wast:54
assert_trap(() => invoke($0, `select-unreached-result2`, []), `unreachable`);

// ./test/core/unreached-valid.wast:55
assert_trap(() => invoke($0, `select-unreached-num`, []), `unreachable`);

// ./test/core/unreached-valid.wast:56
assert_trap(() => invoke($0, `select-unreached-ref`, []), `unreachable`);

// ./test/core/unreached-valid.wast:58
assert_trap(() => invoke($0, `call_ref-unreached`, []), `unreachable`);

// ./test/core/unreached-valid.wast:63
let $1 = instantiate(`(module
  (func (export "meet-bottom")
    (block (result f64)
      (block (result f32)
        (unreachable)
        (br_table 0 1 1 (i32.const 1))
      )
      (drop)
      (f64.const 0)
    )
    (drop)
  )
)`);

// ./test/core/unreached-valid.wast:77
assert_trap(() => invoke($1, `meet-bottom`, []), `unreachable`);

// ./test/core/unreached-valid.wast:82
let $2 = instantiate(`(module
  (func (result (ref func))
    (unreachable)
    (ref.as_non_null)
  )
  (func (result (ref extern))
    (unreachable)
    (ref.as_non_null)
  )

  (func (result (ref func))
    (block (result funcref)
      (unreachable)
      (br_on_null 0)
      (return)
    )
    (unreachable)
  )
  (func (result (ref extern))
    (block (result externref)
      (unreachable)
      (br_on_null 0)
      (return)
    )
    (unreachable)
  )
)`);
