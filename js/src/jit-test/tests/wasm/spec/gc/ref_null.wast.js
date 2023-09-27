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

// ./test/core/ref_null.wast

// ./test/core/ref_null.wast:1
let $0 = instantiate(`(module
  (type $$t (func))
  (func (export "anyref") (result anyref) (ref.null any))
  (func (export "funcref") (result funcref) (ref.null func))
  (func (export "ref") (result (ref null $$t)) (ref.null $$t))

  (global anyref (ref.null any))
  (global funcref (ref.null func))
  (global (ref null $$t) (ref.null $$t))
)`);

// ./test/core/ref_null.wast:12
assert_return(() => invoke($0, `anyref`, []), [value('anyref', null)]);

// ./test/core/ref_null.wast:13
assert_return(() => invoke($0, `funcref`, []), [value('anyfunc', null)]);

// ./test/core/ref_null.wast:14
assert_return(() => invoke($0, `ref`, []), [null]);

// ./test/core/ref_null.wast:17
let $1 = instantiate(`(module
  (type $$t (func))
  (global $$null nullref (ref.null none))
  (global $$nullfunc nullfuncref (ref.null nofunc))
  (global $$nullextern nullexternref (ref.null noextern))
  (func (export "anyref") (result anyref) (global.get $$null))
  (func (export "nullref") (result nullref) (global.get $$null))
  (func (export "funcref") (result funcref) (global.get $$nullfunc))
  (func (export "nullfuncref") (result nullfuncref) (global.get $$nullfunc))
  (func (export "externref") (result externref) (global.get $$nullextern))
  (func (export "nullexternref") (result nullexternref) (global.get $$nullextern))
  (func (export "ref") (result (ref null $$t)) (global.get $$nullfunc))

  (global anyref (ref.null any))
  (global anyref (ref.null none))
  (global funcref (ref.null func))
  (global funcref (ref.null nofunc))
  (global externref (ref.null extern))
  (global externref (ref.null noextern))
  (global nullref (ref.null none))
  (global nullfuncref (ref.null nofunc))
  (global nullexternref (ref.null noextern))
  (global (ref null $$t) (ref.null $$t))
  (global (ref null $$t) (ref.null nofunc))
)`);

// ./test/core/ref_null.wast:43
assert_return(() => invoke($1, `anyref`, []), [value('anyref', null)]);

// ./test/core/ref_null.wast:44
assert_return(() => invoke($1, `anyref`, []), [value('nullref', null)]);

// ./test/core/ref_null.wast:45
assert_return(() => invoke($1, `anyref`, []), [null]);

// ./test/core/ref_null.wast:46
assert_return(() => invoke($1, `nullref`, []), [value('anyref', null)]);

// ./test/core/ref_null.wast:47
assert_return(() => invoke($1, `nullref`, []), [value('nullref', null)]);

// ./test/core/ref_null.wast:48
assert_return(() => invoke($1, `nullref`, []), [null]);

// ./test/core/ref_null.wast:49
assert_return(() => invoke($1, `funcref`, []), [value('anyfunc', null)]);

// ./test/core/ref_null.wast:50
assert_return(() => invoke($1, `funcref`, []), [value('nullfuncref', null)]);

// ./test/core/ref_null.wast:51
assert_return(() => invoke($1, `funcref`, []), [null]);

// ./test/core/ref_null.wast:52
assert_return(() => invoke($1, `nullfuncref`, []), [value('anyfunc', null)]);

// ./test/core/ref_null.wast:53
assert_return(() => invoke($1, `nullfuncref`, []), [value('nullfuncref', null)]);

// ./test/core/ref_null.wast:54
assert_return(() => invoke($1, `nullfuncref`, []), [null]);

// ./test/core/ref_null.wast:55
assert_return(() => invoke($1, `externref`, []), [value('externref', null)]);

// ./test/core/ref_null.wast:56
assert_return(() => invoke($1, `externref`, []), [value('nullexternref', null)]);

// ./test/core/ref_null.wast:57
assert_return(() => invoke($1, `externref`, []), [null]);

// ./test/core/ref_null.wast:58
assert_return(() => invoke($1, `nullexternref`, []), [value('externref', null)]);

// ./test/core/ref_null.wast:59
assert_return(() => invoke($1, `nullexternref`, []), [value('nullexternref', null)]);

// ./test/core/ref_null.wast:60
assert_return(() => invoke($1, `nullexternref`, []), [null]);

// ./test/core/ref_null.wast:61
assert_return(() => invoke($1, `ref`, []), [value('anyfunc', null)]);

// ./test/core/ref_null.wast:62
assert_return(() => invoke($1, `ref`, []), [value('nullfuncref', null)]);

// ./test/core/ref_null.wast:63
assert_return(() => invoke($1, `ref`, []), [null]);
