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

// ./test/core/memory-multi.wast

// ./test/core/memory-multi.wast:5
let $0 = instantiate(`(module
  (memory $$mem1 1)
  (memory $$mem2 1)

  (func (export "init1") (result i32)
    (memory.init $$mem1 $$d (i32.const 1) (i32.const 0) (i32.const 4))
    (i32.load $$mem1 (i32.const 1))
  )

  (func (export "init2") (result i32)
    (memory.init $$mem2 $$d (i32.const 1) (i32.const 4) (i32.const 4))
    (i32.load $$mem2 (i32.const 1))
  )

  (data $$d "\\01\\00\\00\\00" "\\02\\00\\00\\00")
)`);

// ./test/core/memory-multi.wast:22
assert_return(() => invoke($0, `init1`, []), [value("i32", 1)]);

// ./test/core/memory-multi.wast:23
assert_return(() => invoke($0, `init2`, []), [value("i32", 2)]);

// ./test/core/memory-multi.wast:26
let $1 = instantiate(`(module
  (memory $$mem1 1)
  (memory $$mem2 1)

  (func (export "fill1") (result i32)
    (memory.fill $$mem1 (i32.const 1) (i32.const 0x01) (i32.const 4))
    (i32.load $$mem1 (i32.const 1))
  )

  (func (export "fill2") (result i32)
    (memory.fill $$mem2 (i32.const 1) (i32.const 0x02) (i32.const 2))
    (i32.load $$mem2 (i32.const 1))
  )
)`);

// ./test/core/memory-multi.wast:41
assert_return(() => invoke($1, `fill1`, []), [value("i32", 16843009)]);

// ./test/core/memory-multi.wast:42
assert_return(() => invoke($1, `fill2`, []), [value("i32", 514)]);
