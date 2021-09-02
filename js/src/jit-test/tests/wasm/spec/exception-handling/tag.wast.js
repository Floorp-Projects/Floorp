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

// ./test/core/tag.wast

// ./test/core/tag.wast:3
let $0 = instantiate(`(module
  (tag)
  (tag (param i32))
  (tag (export "t2") (param i32))
  (tag $$t3 (param i32 f32))
  (export "t3" (tag 3))
)`);

// ./test/core/tag.wast:11
register($0, `test`);

// ./test/core/tag.wast:13
let $1 = instantiate(`(module
  (tag $$t0 (import "test" "t2") (param i32))
  (import "test" "t3" (tag $$t1 (param i32 f32)))
)`);

// ./test/core/tag.wast:18
assert_invalid(
  () => instantiate(`(module (tag (result i32)))`),
  `non-empty tag result type`,
);
