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

// ./test/core/forward.wast

// ./test/core/forward.wast:1
let $0 = instantiate(`(module
  (func $$even (export "even") (param $$n i32) (result i32)
    (if (result i32) (i32.eq (local.get $$n) (i32.const 0))
      (then (i32.const 1))
      (else (call $$odd (i32.sub (local.get $$n) (i32.const 1))))
    )
  )

  (func $$odd (export "odd") (param $$n i32) (result i32)
    (if (result i32) (i32.eq (local.get $$n) (i32.const 0))
      (then (i32.const 0))
      (else (call $$even (i32.sub (local.get $$n) (i32.const 1))))
    )
  )
)`);

// ./test/core/forward.wast:17
assert_return(() => invoke($0, `even`, [13]), [value("i32", 0)]);

// ./test/core/forward.wast:18
assert_return(() => invoke($0, `even`, [20]), [value("i32", 1)]);

// ./test/core/forward.wast:19
assert_return(() => invoke($0, `odd`, [13]), [value("i32", 1)]);

// ./test/core/forward.wast:20
assert_return(() => invoke($0, `odd`, [20]), [value("i32", 0)]);
