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

// ./test/core/multi-memory/data0.wast

// ./test/core/multi-memory/data0.wast:5
let $0 = instantiate(`(module
  (memory $$mem0 1)
  (memory $$mem1 1)
  (memory $$mem2 1)
  
  (data (i32.const 0))
  (data (i32.const 1) "a" "" "bcd")
  (data (offset (i32.const 0)))
  (data (offset (i32.const 0)) "" "a" "bc" "")
  (data (memory 0) (i32.const 0))
  (data (memory 0x0) (i32.const 1) "a" "" "bcd")
  (data (memory 0x000) (offset (i32.const 0)))
  (data (memory 0) (offset (i32.const 0)) "" "a" "bc" "")
  (data (memory $$mem0) (i32.const 0))
  (data (memory $$mem1) (i32.const 1) "a" "" "bcd")
  (data (memory $$mem2) (offset (i32.const 0)))
  (data (memory $$mem0) (offset (i32.const 0)) "" "a" "bc" "")

  (data $$d1 (i32.const 0))
  (data $$d2 (i32.const 1) "a" "" "bcd")
  (data $$d3 (offset (i32.const 0)))
  (data $$d4 (offset (i32.const 0)) "" "a" "bc" "")
  (data $$d5 (memory 0) (i32.const 0))
  (data $$d6 (memory 0x0) (i32.const 1) "a" "" "bcd")
  (data $$d7 (memory 0x000) (offset (i32.const 0)))
  (data $$d8 (memory 0) (offset (i32.const 0)) "" "a" "bc" "")
  (data $$d9 (memory $$mem0) (i32.const 0))
  (data $$d10 (memory $$mem1) (i32.const 1) "a" "" "bcd")
  (data $$d11 (memory $$mem2) (offset (i32.const 0)))
  (data $$d12 (memory $$mem0) (offset (i32.const 0)) "" "a" "bc" "")
)`);

// ./test/core/multi-memory/data0.wast:39
let $1 = instantiate(`(module
  (memory 1)
  (data (i32.const 0) "a")
)`);

// ./test/core/multi-memory/data0.wast:43
let $2 = instantiate(`(module
  (import "spectest" "memory" (memory 1))
  (import "spectest" "memory" (memory 1))
  (import "spectest" "memory" (memory 1))
  (data (memory 0) (i32.const 0) "a")
  (data (memory 1) (i32.const 0) "a")
  (data (memory 2) (i32.const 0) "a")
)`);

// ./test/core/multi-memory/data0.wast:52
let $3 = instantiate(`(module
  (global (import "spectest" "global_i32") i32)
  (memory 1)
  (data (global.get 0) "a")
)`);

// ./test/core/multi-memory/data0.wast:57
let $4 = instantiate(`(module
  (global (import "spectest" "global_i32") i32)
  (import "spectest" "memory" (memory 1))
  (data (global.get 0) "a")
)`);

// ./test/core/multi-memory/data0.wast:63
let $5 = instantiate(`(module
  (global $$g (import "spectest" "global_i32") i32)
  (memory 1)
  (data (global.get $$g) "a")
)`);

// ./test/core/multi-memory/data0.wast:68
let $6 = instantiate(`(module
  (global $$g (import "spectest" "global_i32") i32)
  (import "spectest" "memory" (memory 1))
  (data (global.get $$g) "a")
)`);
