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

// ./test/core/multi-memory/memory_size3.wast

// ./test/core/multi-memory/memory_size3.wast:3
assert_invalid(
  () => instantiate(`(module
    (memory 0)
    (memory $$m 1)
    (memory 0)
    (func $$type-result-i32-vs-empty
      (memory.size $$m)
    )
  )`),
  `type mismatch`,
);

// ./test/core/multi-memory/memory_size3.wast:14
assert_invalid(
  () => instantiate(`(module
    (memory 0)
    (memory 0)
    (memory 0)
    (memory $$m 1)
    (func $$type-result-i32-vs-f32 (result f32)
      (memory.size $$m)
    )
  )`),
  `type mismatch`,
);
