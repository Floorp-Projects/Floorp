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

// ./test/core/table-sub.wast

// ./test/core/table-sub.wast:1
assert_invalid(
  () => instantiate(`(module
    (table $$t1 10 funcref)
    (table $$t2 10 externref)
    (func $$f
      (table.copy $$t1 $$t2 (i32.const 0) (i32.const 1) (i32.const 2))
    )
  )`),
  `type mismatch`,
);

// ./test/core/table-sub.wast:12
assert_invalid(
  () => instantiate(`(module
    (table $$t 10 funcref)
    (elem $$el externref)
    (func $$f
      (table.init $$t $$el (i32.const 0) (i32.const 1) (i32.const 2))
    )
  )`),
  `type mismatch`,
);
