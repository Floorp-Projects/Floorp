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

// ./test/core/type-rec.wast

// ./test/core/type-rec.wast:3
let $0 = instantiate(`(module
  (rec (type $$f1 (func)) (type (struct (field (ref $$f1)))))
  (rec (type $$f2 (func)) (type (struct (field (ref $$f2)))))
  (func $$f (type $$f2))
  (global (ref $$f1) (ref.func $$f))
)`);

// ./test/core/type-rec.wast:10
let $1 = instantiate(`(module
  (rec (type $$f1 (func)) (type (struct (field (ref $$f1)))))
  (rec (type $$f2 (func)) (type (struct (field (ref $$f2)))))
  (rec
    (type $$g1 (func))
    (type (struct (field (ref $$f1) (ref $$f1) (ref $$f2) (ref $$f2) (ref $$g1))))
  )
  (rec
    (type $$g2 (func))
    (type (struct (field (ref $$f1) (ref $$f2) (ref $$f1) (ref $$f2) (ref $$g2))))
  )
  (func $$g (type $$g2))
  (global (ref $$g1) (ref.func $$g))
)`);

// ./test/core/type-rec.wast:25
assert_invalid(
  () => instantiate(`(module
    (rec (type $$f1 (func)) (type (struct (field (ref $$f1)))))
    (rec (type $$f2 (func)) (type (struct (field (ref $$f1)))))
    (func $$f (type $$f2))
    (global (ref $$f1) (ref.func $$f))
  )`),
  `type mismatch`,
);

// ./test/core/type-rec.wast:35
assert_invalid(
  () => instantiate(`(module
    (rec (type $$f0 (func)) (type (struct (field (ref $$f0)))))
    (rec (type $$f1 (func)) (type (struct (field (ref $$f0)))))
    (rec (type $$f2 (func)) (type (struct (field (ref $$f1)))))
    (func $$f (type $$f2))
    (global (ref $$f1) (ref.func $$f))
  )`),
  `type mismatch`,
);

// ./test/core/type-rec.wast:46
assert_invalid(
  () => instantiate(`(module
    (rec (type $$f1 (func)) (type (struct)))
    (rec (type (struct)) (type $$f2 (func)))
    (global (ref $$f1) (ref.func $$f))
    (func $$f (type $$f2))
  )`),
  `type mismatch`,
);

// ./test/core/type-rec.wast:56
assert_invalid(
  () => instantiate(`(module
    (rec (type $$f1 (func)) (type (struct)))
    (rec (type $$f2 (func)) (type (struct)) (type (func)))
    (global (ref $$f1) (ref.func $$f))
    (func $$f (type $$f2))
  )`),
  `type mismatch`,
);

// ./test/core/type-rec.wast:69
let $2 = instantiate(`(module $$M
  (rec (type $$f1 (func)) (type (struct)))
  (func (export "f") (type $$f1))
)`);
register($2, `M`);

// ./test/core/type-rec.wast:73
register(`M`, `M`);

// ./test/core/type-rec.wast:75
let $3 = instantiate(`(module
  (rec (type $$f2 (func)) (type (struct)))
  (func (import "M" "f") (type $$f2))
)`);

// ./test/core/type-rec.wast:80
assert_unlinkable(
  () => instantiate(`(module
    (rec (type (struct)) (type $$f2 (func)))
    (func (import "M" "f") (type $$f2))
  )`),
  `incompatible import type`,
);

// ./test/core/type-rec.wast:88
assert_unlinkable(
  () => instantiate(`(module
    (rec (type $$f2 (func)))
    (func (import "M" "f") (type $$f2))
  )`),
  `incompatible import type`,
);

// ./test/core/type-rec.wast:99
let $4 = instantiate(`(module
  (rec (type $$f1 (func)) (type (struct)))
  (rec (type $$f2 (func)) (type (struct)))
  (table funcref (elem $$f1))
  (func $$f1 (type $$f1))
  (func (export "run") (call_indirect (type $$f2) (i32.const 0)))
)`);

// ./test/core/type-rec.wast:106
assert_return(() => invoke($4, `run`, []), []);

// ./test/core/type-rec.wast:108
let $5 = instantiate(`(module
  (rec (type $$f1 (func)) (type (struct)))
  (rec (type (struct)) (type $$f2 (func)))
  (table funcref (elem $$f1))
  (func $$f1 (type $$f1))
  (func (export "run") (call_indirect (type $$f2) (i32.const 0)))
)`);

// ./test/core/type-rec.wast:115
assert_trap(() => invoke($5, `run`, []), `indirect call type mismatch`);

// ./test/core/type-rec.wast:117
let $6 = instantiate(`(module
  (rec (type $$f1 (func)) (type (struct)))
  (rec (type $$f2 (func)))
  (table funcref (elem $$f1))
  (func $$f1 (type $$f1))
  (func (export "run") (call_indirect (type $$f2) (i32.const 0)))
)`);

// ./test/core/type-rec.wast:124
assert_trap(() => invoke($6, `run`, []), `indirect call type mismatch`);

// ./test/core/type-rec.wast:129
let $7 = instantiate(`(module
  (rec (type $$s (struct)))
  (rec (type $$t (func (param (ref $$s)))))
  (func $$f (param (ref $$s)))  ;; okay, type is equivalent to $$t
  (global (ref $$t) (ref.func $$f))
)`);

// ./test/core/type-rec.wast:136
assert_invalid(
  () => instantiate(`(module
    (rec
      (type $$s (struct))
      (type $$t (func (param (ref $$s))))
    )
    (func $$f (param (ref $$s)))  ;; type is not equivalent to $$t
    (global (ref $$t) (ref.func $$f))
  )`),
  `type mismatch`,
);

// ./test/core/type-rec.wast:148
assert_invalid(
  () => instantiate(`(module
    (rec
      (type (struct))
      (type $$t (func))
    )
    (func $$f)  ;; type is not equivalent to $$t
    (global (ref $$t) (ref.func $$f))
  )`),
  `type mismatch`,
);
