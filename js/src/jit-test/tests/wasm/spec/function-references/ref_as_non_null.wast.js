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

// ./test/core/ref_as_non_null.wast

// ./test/core/ref_as_non_null.wast:1
let $0 = instantiate(`(module
  (type $$t (func (result i32)))

  (func $$nn (param $$r (ref $$t)) (result i32)
    (call_ref (ref.as_non_null (local.get $$r)))
  )
  (func $$n (param $$r (ref null $$t)) (result i32)
    (call_ref (ref.as_non_null (local.get $$r)))
  )

  (elem func $$f)
  (func $$f (result i32) (i32.const 7))

  (func (export "nullable-null") (result i32) (call $$n (ref.null $$t)))
  (func (export "nonnullable-f") (result i32) (call $$nn (ref.func $$f)))
  (func (export "nullable-f") (result i32) (call $$n (ref.func $$f)))

  (func (export "unreachable") (result i32)
    (unreachable)
    (ref.as_non_null)
    (call $$nn)
  )
)`);

// ./test/core/ref_as_non_null.wast:25
assert_trap(() => invoke($0, `unreachable`, []), `unreachable`);

// ./test/core/ref_as_non_null.wast:27
assert_trap(() => invoke($0, `nullable-null`, []), `null reference`);

// ./test/core/ref_as_non_null.wast:28
assert_return(() => invoke($0, `nonnullable-f`, []), [value("i32", 7)]);

// ./test/core/ref_as_non_null.wast:29
assert_return(() => invoke($0, `nullable-f`, []), [value("i32", 7)]);

// ./test/core/ref_as_non_null.wast:31
assert_invalid(() =>
  instantiate(`(module
    (type $$t (func (result i32)))
    (func $$g (param $$r (ref $$t)) (drop (ref.as_non_null (local.get $$r))))
    (func (call $$g (ref.null $$t)))
  )`), `type mismatch`);

// ./test/core/ref_as_non_null.wast:41
let $1 = instantiate(`(module
  (type $$t (func))
  (func (param $$r (ref $$t)) (drop (ref.as_non_null (local.get $$r))))
  (func (param $$r (ref func)) (drop (ref.as_non_null (local.get $$r))))
  (func (param $$r (ref extern)) (drop (ref.as_non_null (local.get $$r))))
)`);
