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

// ./test/core/fac.wast

// ./test/core/fac.wast:1
let $0 = instantiate(`(module
  ;; Recursive factorial
  (func (export "fac-rec") (param i64) (result i64)
    (if (result i64) (i64.eq (local.get 0) (i64.const 0))
      (then (i64.const 1))
      (else
        (i64.mul (local.get 0) (call 0 (i64.sub (local.get 0) (i64.const 1))))
      )
    )
  )

  ;; Recursive factorial named
  (func $$fac-rec-named (export "fac-rec-named") (param $$n i64) (result i64)
    (if (result i64) (i64.eq (local.get $$n) (i64.const 0))
      (then (i64.const 1))
      (else
        (i64.mul
          (local.get $$n)
          (call $$fac-rec-named (i64.sub (local.get $$n) (i64.const 1)))
        )
      )
    )
  )

  ;; Iterative factorial
  (func (export "fac-iter") (param i64) (result i64)
    (local i64 i64)
    (local.set 1 (local.get 0))
    (local.set 2 (i64.const 1))
    (block
      (loop
        (if
          (i64.eq (local.get 1) (i64.const 0))
          (then (br 2))
          (else
            (local.set 2 (i64.mul (local.get 1) (local.get 2)))
            (local.set 1 (i64.sub (local.get 1) (i64.const 1)))
          )
        )
        (br 0)
      )
    )
    (local.get 2)
  )

  ;; Iterative factorial named
  (func (export "fac-iter-named") (param $$n i64) (result i64)
    (local $$i i64)
    (local $$res i64)
    (local.set $$i (local.get $$n))
    (local.set $$res (i64.const 1))
    (block $$done
      (loop $$loop
        (if
          (i64.eq (local.get $$i) (i64.const 0))
          (then (br $$done))
          (else
            (local.set $$res (i64.mul (local.get $$i) (local.get $$res)))
            (local.set $$i (i64.sub (local.get $$i) (i64.const 1)))
          )
        )
        (br $$loop)
      )
    )
    (local.get $$res)
  )

  ;; Optimized factorial.
  (func (export "fac-opt") (param i64) (result i64)
    (local i64)
    (local.set 1 (i64.const 1))
    (block
      (br_if 0 (i64.lt_s (local.get 0) (i64.const 2)))
      (loop
        (local.set 1 (i64.mul (local.get 1) (local.get 0)))
        (local.set 0 (i64.add (local.get 0) (i64.const -1)))
        (br_if 0 (i64.gt_s (local.get 0) (i64.const 1)))
      )
    )
    (local.get 1)
  )
)`);

// ./test/core/fac.wast:84
assert_return(() => invoke($0, `fac-rec`, [25n]), [
  value("i64", 7034535277573963776n),
]);

// ./test/core/fac.wast:85
assert_return(() => invoke($0, `fac-iter`, [25n]), [
  value("i64", 7034535277573963776n),
]);

// ./test/core/fac.wast:86
assert_return(() => invoke($0, `fac-rec-named`, [25n]), [
  value("i64", 7034535277573963776n),
]);

// ./test/core/fac.wast:87
assert_return(() => invoke($0, `fac-iter-named`, [25n]), [
  value("i64", 7034535277573963776n),
]);

// ./test/core/fac.wast:88
assert_return(() => invoke($0, `fac-opt`, [25n]), [
  value("i64", 7034535277573963776n),
]);

// ./test/core/fac.wast:89
assert_exhaustion(
  () => invoke($0, `fac-rec`, [1073741824n]),
  `call stack exhausted`,
);
