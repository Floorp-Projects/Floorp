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

// ./test/core/multi-memory/float_exprs0.wast

// ./test/core/multi-memory/float_exprs0.wast:1
let $0 = instantiate(`(module
  (memory 0 0)
  (memory $$m 1 1)
  (memory 0 0)
  (func (export "init") (param $$i i32) (param $$x f64)
    (f64.store $$m (local.get $$i) (local.get $$x)))

  (func (export "run") (param $$n i32) (param $$z f64)
    (local $$i i32)
    (block $$exit
      (loop $$cont
        (f64.store $$m
          (local.get $$i)
          (f64.div (f64.load $$m (local.get $$i)) (local.get $$z))
        )
        (local.set $$i (i32.add (local.get $$i) (i32.const 8)))
        (br_if $$cont (i32.lt_u (local.get $$i) (local.get $$n)))
      )
    )
  )

  (func (export "check") (param $$i i32) (result f64) (f64.load $$m (local.get $$i)))
)`);

// ./test/core/multi-memory/float_exprs0.wast:25
invoke($0, `init`, [0, value("f64", 15.1)]);

// ./test/core/multi-memory/float_exprs0.wast:26
invoke($0, `init`, [8, value("f64", 15.2)]);

// ./test/core/multi-memory/float_exprs0.wast:27
invoke($0, `init`, [16, value("f64", 15.3)]);

// ./test/core/multi-memory/float_exprs0.wast:28
invoke($0, `init`, [24, value("f64", 15.4)]);

// ./test/core/multi-memory/float_exprs0.wast:29
assert_return(() => invoke($0, `check`, [0]), [value("f64", 15.1)]);

// ./test/core/multi-memory/float_exprs0.wast:30
assert_return(() => invoke($0, `check`, [8]), [value("f64", 15.2)]);

// ./test/core/multi-memory/float_exprs0.wast:31
assert_return(() => invoke($0, `check`, [16]), [value("f64", 15.3)]);

// ./test/core/multi-memory/float_exprs0.wast:32
assert_return(() => invoke($0, `check`, [24]), [value("f64", 15.4)]);

// ./test/core/multi-memory/float_exprs0.wast:33
invoke($0, `run`, [32, value("f64", 3)]);

// ./test/core/multi-memory/float_exprs0.wast:34
assert_return(() => invoke($0, `check`, [0]), [value("f64", 5.033333333333333)]);

// ./test/core/multi-memory/float_exprs0.wast:35
assert_return(() => invoke($0, `check`, [8]), [value("f64", 5.066666666666666)]);

// ./test/core/multi-memory/float_exprs0.wast:36
assert_return(() => invoke($0, `check`, [16]), [value("f64", 5.1000000000000005)]);

// ./test/core/multi-memory/float_exprs0.wast:37
assert_return(() => invoke($0, `check`, [24]), [value("f64", 5.133333333333334)]);
