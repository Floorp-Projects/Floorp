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

// ./test/core/gc/extern.wast

// ./test/core/gc/extern.wast:1
let $0 = instantiate(`(module
  (type $$ft (func))
  (type $$st (struct))
  (type $$at (array i8))

  (table 10 anyref)

  (elem declare func $$f)
  (func $$f)

  (func (export "init") (param $$x externref)
    (table.set (i32.const 0) (ref.null any))
    (table.set (i32.const 1) (ref.i31 (i32.const 7)))
    (table.set (i32.const 2) (struct.new_default $$st))
    (table.set (i32.const 3) (array.new_default $$at (i32.const 0)))
    (table.set (i32.const 4) (extern.internalize (local.get $$x)))
  )

  (func (export "internalize") (param externref) (result anyref)
    (extern.internalize (local.get 0))
  )
  (func (export "externalize") (param anyref) (result externref)
    (extern.externalize (local.get 0))
  )

  (func (export "externalize-i") (param i32) (result externref)
    (extern.externalize (table.get (local.get 0)))
  )
  (func (export "externalize-ii") (param i32) (result anyref)
    (extern.internalize (extern.externalize (table.get (local.get 0))))
  )
)`);

// ./test/core/gc/extern.wast:34
invoke($0, `init`, [externref(0)]);

// ./test/core/gc/extern.wast:36
assert_return(() => invoke($0, `internalize`, [externref(1)]), [new HostRefResult(1)]);

// ./test/core/gc/extern.wast:37
assert_return(() => invoke($0, `internalize`, [null]), [value('anyref', null)]);

// ./test/core/gc/extern.wast:39
assert_return(() => invoke($0, `externalize`, [hostref(2)]), [new ExternRefResult(2)]);

// ./test/core/gc/extern.wast:40
assert_return(() => invoke($0, `externalize`, [null]), [value('externref', null)]);

// ./test/core/gc/extern.wast:42
assert_return(() => invoke($0, `externalize-i`, [0]), [value('externref', null)]);

// ./test/core/gc/extern.wast:43
assert_return(() => invoke($0, `externalize-i`, [1]), [new RefWithType('externref')]);

// ./test/core/gc/extern.wast:44
assert_return(() => invoke($0, `externalize-i`, [2]), [new RefWithType('externref')]);

// ./test/core/gc/extern.wast:45
assert_return(() => invoke($0, `externalize-i`, [3]), [new RefWithType('externref')]);

// ./test/core/gc/extern.wast:46
assert_return(() => invoke($0, `externalize-i`, [4]), [new RefWithType('externref')]);

// ./test/core/gc/extern.wast:47
assert_return(() => invoke($0, `externalize-i`, [5]), [value('externref', null)]);

// ./test/core/gc/extern.wast:49
assert_return(() => invoke($0, `externalize-ii`, [0]), [value('anyref', null)]);

// ./test/core/gc/extern.wast:50
assert_return(() => invoke($0, `externalize-ii`, [1]), [new RefWithType('i31ref')]);

// ./test/core/gc/extern.wast:51
assert_return(() => invoke($0, `externalize-ii`, [2]), [new RefWithType('structref')]);

// ./test/core/gc/extern.wast:52
assert_return(() => invoke($0, `externalize-ii`, [3]), [new RefWithType('arrayref')]);

// ./test/core/gc/extern.wast:53
assert_return(() => invoke($0, `externalize-ii`, [4]), [new HostRefResult(0)]);

// ./test/core/gc/extern.wast:54
assert_return(() => invoke($0, `externalize-ii`, [5]), [value('anyref', null)]);
