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

// ./test/core/multi-memory/exports0.wast

// ./test/core/multi-memory/exports0.wast:3
let $0 = instantiate(`(module (memory 0) (export "a" (memory 0)))`);

// ./test/core/multi-memory/exports0.wast:4
let $1 = instantiate(`(module (memory 0) (export "a" (memory 0)) (export "b" (memory 0)))`);

// ./test/core/multi-memory/exports0.wast:5
let $2 = instantiate(`(module (memory 0) (memory 0) (export "a" (memory 0)) (export "b" (memory 1)))`);

// ./test/core/multi-memory/exports0.wast:6
let $3 = instantiate(`(module
  (memory $$mem0 0)
  (memory $$mem1 0)
  (memory $$mem2 0)
  (memory $$mem3 0)
  (memory $$mem4 0)
  (memory $$mem5 0)
  (memory $$mem6 0)
  
  (export "a" (memory $$mem0))
  (export "b" (memory $$mem1))
  (export "ac" (memory $$mem2))
  (export "bc" (memory $$mem3))
  (export "ad" (memory $$mem4))
  (export "bd" (memory $$mem5))
  (export "be" (memory $$mem6))
  
  (export "za" (memory $$mem0))
  (export "zb" (memory $$mem1))
  (export "zac" (memory $$mem2))
  (export "zbc" (memory $$mem3))
  (export "zad" (memory $$mem4))
  (export "zbd" (memory $$mem5))
  (export "zbe" (memory $$mem6))
)`);

// ./test/core/multi-memory/exports0.wast:32
let $4 = instantiate(`(module
  (export "a" (memory 0))
  (memory 6)

  (export "b" (memory 1))
  (memory 3)
)`);

// ./test/core/multi-memory/exports0.wast:40
let $5 = instantiate(`(module
  (export "a" (memory 0))
  (memory 0 1)
  (memory 0 1)
  (memory 0 1)
  (memory 0 1)

  (export "b" (memory 3))
)`);

// ./test/core/multi-memory/exports0.wast:49
let $6 = instantiate(`(module (export "a" (memory $$a)) (memory $$a 0))`);

// ./test/core/multi-memory/exports0.wast:50
let $7 = instantiate(`(module (export "a" (memory $$a)) (memory $$a 0 1))`);
