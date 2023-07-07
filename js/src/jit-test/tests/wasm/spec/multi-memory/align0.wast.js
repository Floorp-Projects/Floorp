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

// ./test/core/multi-memory/align0.wast

// ./test/core/multi-memory/align0.wast:3
let $0 = instantiate(`(module
  (memory $$mem0 0)
  (memory $$mem1 1)
  (memory $$mem2 0)

  ;; $$default: natural alignment, $$1: align=1, $$2: align=2, $$4: align=4, $$8: align=8

  (func (export "f32_align_switch") (param i32) (result f32)
    (local f32 f32)
    (local.set 1 (f32.const 10.0))
    (block $$4
      (block $$2
        (block $$1
          (block $$default
            (block $$0
              (br_table $$0 $$default $$1 $$2 $$4 (local.get 0))
            ) ;; 0
            (f32.store $$mem1 (i32.const 0) (local.get 1))
            (local.set 2 (f32.load $$mem1 (i32.const 0)))
            (br $$4)
          ) ;; default
          (f32.store $$mem1 align=1 (i32.const 0) (local.get 1))
          (local.set 2 (f32.load $$mem1 align=1 (i32.const 0)))
          (br $$4)
        ) ;; 1
        (f32.store $$mem1 align=2 (i32.const 0) (local.get 1))
        (local.set 2 (f32.load $$mem1 align=2 (i32.const 0)))
        (br $$4)
      ) ;; 2
      (f32.store $$mem1 align=4 (i32.const 0) (local.get 1))
      (local.set 2 (f32.load $$mem1 align=4 (i32.const 0)))
    ) ;; 4
    (local.get 2)
  )
)`);

// ./test/core/multi-memory/align0.wast:39
assert_return(() => invoke($0, `f32_align_switch`, [0]), [value("f32", 10)]);

// ./test/core/multi-memory/align0.wast:40
assert_return(() => invoke($0, `f32_align_switch`, [1]), [value("f32", 10)]);

// ./test/core/multi-memory/align0.wast:41
assert_return(() => invoke($0, `f32_align_switch`, [2]), [value("f32", 10)]);

// ./test/core/multi-memory/align0.wast:42
assert_return(() => invoke($0, `f32_align_switch`, [3]), [value("f32", 10)]);
