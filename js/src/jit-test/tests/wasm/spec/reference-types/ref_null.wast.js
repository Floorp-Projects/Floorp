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
  (func (export "externref") (result externref) (ref.null extern))
  (func (export "funcref") (result funcref) (ref.null func))

  (global externref (ref.null extern))
  (global funcref (ref.null func))
)`);

// ./test/core/ref_null.wast:9
assert_return(() => invoke($0, `externref`, []), [value("externref", null)]);

// ./test/core/ref_null.wast:10
assert_return(() => invoke($0, `funcref`, []), [value("funcref", null)]);
