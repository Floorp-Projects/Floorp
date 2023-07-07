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

// ./test/core/multi-memory/data1.wast

// ./test/core/multi-memory/data1.wast:3
assert_trap(
  () => instantiate(`(module
    (memory 1)
    (memory 0)
    (memory 2)
    (data (memory 1) (i32.const 0) "a")
  )`),
  `out of bounds memory access`,
);

// ./test/core/multi-memory/data1.wast:13
assert_trap(
  () => instantiate(`(module
    (memory 1 1)
    (memory 1 1)
    (memory 0 0)
    (data (memory 2) (i32.const 0) "a")
  )`),
  `out of bounds memory access`,
);

// ./test/core/multi-memory/data1.wast:23
assert_trap(
  () => instantiate(`(module
    (memory 1 1)
    (memory 0 1)
    (memory 1 1)
    (data (memory 1) (i32.const 0) "a")
  )`),
  `out of bounds memory access`,
);

// ./test/core/multi-memory/data1.wast:32
assert_trap(
  () => instantiate(`(module
    (memory 1)
    (memory 1)
    (memory 0)
    (data (memory 2) (i32.const 1))
  )`),
  `out of bounds memory access`,
);

// ./test/core/multi-memory/data1.wast:41
assert_trap(
  () => instantiate(`(module
    (memory 1 1)
    (memory 1 1)
    (memory 0 1)
    (data (memory 2) (i32.const 1))
  )`),
  `out of bounds memory access`,
);

// ./test/core/multi-memory/data1.wast:60
assert_trap(
  () => instantiate(`(module
    (global (import "spectest" "global_i32") i32)
    (memory 3)
    (memory 0)
    (memory 3)
    (data (memory 1) (global.get 0) "a")
  )`),
  `out of bounds memory access`,
);

// ./test/core/multi-memory/data1.wast:71
assert_trap(
  () => instantiate(`(module
    (memory 2 2)
    (memory 1 2)
    (memory 2 2)
    (data (memory 1) (i32.const 0x1_0000) "a")
  )`),
  `out of bounds memory access`,
);

// ./test/core/multi-memory/data1.wast:80
assert_trap(
  () => instantiate(`(module
    (import "spectest" "memory" (memory 1))
    (data (i32.const 0x1_0000) "a")
  )`),
  `out of bounds memory access`,
);

// ./test/core/multi-memory/data1.wast:88
assert_trap(
  () => instantiate(`(module
    (memory 3)
    (memory 3)
    (memory 2)
    (data (memory 2) (i32.const 0x2_0000) "a")
  )`),
  `out of bounds memory access`,
);

// ./test/core/multi-memory/data1.wast:98
assert_trap(
  () => instantiate(`(module
    (memory 3 3)
    (memory 2 3)
    (memory 3 3)
    (data (memory 1) (i32.const 0x2_0000) "a")
  )`),
  `out of bounds memory access`,
);

// ./test/core/multi-memory/data1.wast:108
assert_trap(
  () => instantiate(`(module
    (memory 0)
    (memory 0)
    (memory 1)
    (data (memory 2) (i32.const -1) "a")
  )`),
  `out of bounds memory access`,
);

// ./test/core/multi-memory/data1.wast:117
assert_trap(
  () => instantiate(`(module
    (import "spectest" "memory" (memory 1))
    (import "spectest" "memory" (memory 1))
    (import "spectest" "memory" (memory 1))
    (data (memory 2) (i32.const -1) "a")
  )`),
  `out of bounds memory access`,
);

// ./test/core/multi-memory/data1.wast:127
assert_trap(
  () => instantiate(`(module
    (memory 2)
    (memory 2)
    (memory 2)
    (data (memory 2) (i32.const -100) "a")
  )`),
  `out of bounds memory access`,
);

// ./test/core/multi-memory/data1.wast:136
assert_trap(
  () => instantiate(`(module
    (import "spectest" "memory" (memory 1))
    (import "spectest" "memory" (memory 1))
    (import "spectest" "memory" (memory 1))
    (import "spectest" "memory" (memory 1))
    (data (memory 3) (i32.const -100) "a")
  )`),
  `out of bounds memory access`,
);
