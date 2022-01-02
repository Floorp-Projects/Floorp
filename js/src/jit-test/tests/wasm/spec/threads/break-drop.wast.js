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

// ./test/core/break-drop.wast

// ./test/core/break-drop.wast:1
let $0 = instantiate(`(module
  (func (export "br") (block (br 0)))
  (func (export "br_if") (block (br_if 0 (i32.const 1))))
  (func (export "br_table") (block (br_table 0 (i32.const 0))))
)`);

// ./test/core/break-drop.wast:7
assert_return(() => invoke($0, `br`, []), []);

// ./test/core/break-drop.wast:8
assert_return(() => invoke($0, `br_if`, []), []);

// ./test/core/break-drop.wast:9
assert_return(() => invoke($0, `br_table`, []), []);
