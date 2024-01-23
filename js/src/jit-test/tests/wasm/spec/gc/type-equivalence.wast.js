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

// ./test/core/type-equivalence.wast

// ./test/core/type-equivalence.wast:5
let $0 = instantiate(`(module
  (type $$t1 (func (param f32 f32) (result f32)))
  (type $$t2 (func (param $$x f32) (param $$y f32) (result f32)))

  (func $$f1 (param $$r (ref $$t1)) (call $$f2 (local.get $$r)))
  (func $$f2 (param $$r (ref $$t2)) (call $$f1 (local.get $$r)))
)`);

// ./test/core/type-equivalence.wast:16
let $1 = instantiate(`(module
  (type $$s0 (func (param i32) (result f32)))
  (type $$s1 (func (param i32 (ref $$s0)) (result (ref $$s0))))
  (type $$s2 (func (param i32 (ref $$s0)) (result (ref $$s0))))
  (type $$t1 (func (param (ref $$s1)) (result (ref $$s2))))
  (type $$t2 (func (param (ref $$s2)) (result (ref $$s1))))

  (func $$f1 (param $$r (ref $$t1)) (call $$f2 (local.get $$r)))
  (func $$f2 (param $$r (ref $$t2)) (call $$f1 (local.get $$r)))
)`);

// ./test/core/type-equivalence.wast:30
let $2 = instantiate(`(module
  (rec (type $$t1 (func (param i32 (ref $$t1)))))
  (rec (type $$t2 (func (param i32 (ref $$t2)))))

  (func $$f1 (param $$r (ref $$t1)) (call $$f2 (local.get $$r)))
  (func $$f2 (param $$r (ref $$t2)) (call $$f1 (local.get $$r)))
)`);

// ./test/core/type-equivalence.wast:38
let $3 = instantiate(`(module
  (type $$t1 (func (param i32 (ref $$t1))))
  (type $$t2 (func (param i32 (ref $$t2))))

  (func $$f1 (param $$r (ref $$t1)) (call $$f2 (local.get $$r)))
  (func $$f2 (param $$r (ref $$t2)) (call $$f1 (local.get $$r)))
)`);

// ./test/core/type-equivalence.wast:49
let $4 = instantiate(`(module
  (rec
    (type $$t0 (func (param i32 (ref $$t1))))
    (type $$t1 (func (param i32 (ref $$t0))))
  )
  (rec
    (type $$t2 (func (param i32 (ref $$t3))))
    (type $$t3 (func (param i32 (ref $$t2))))
  )

  (func $$f0 (param $$r (ref $$t0))
    (call $$f2 (local.get $$r))
  )
  (func $$f1 (param $$r (ref $$t1))
    (call $$f3 (local.get $$r))
  )
  (func $$f2 (param $$r (ref $$t2))
    (call $$f0 (local.get $$r))
  )
  (func $$f3 (param $$r (ref $$t3))
    (call $$f1 (local.get $$r))
  )
)`);

// ./test/core/type-equivalence.wast:76
assert_invalid(
  () => instantiate(`(module
    (type $$t1 (func (param (ref $$t2))))
    (type $$t2 (func (param (ref $$t1))))
  )`),
  `unknown type`,
);

// ./test/core/type-equivalence.wast:89
let $5 = instantiate(`(module
  (type $$t1 (func (param f32 f32)))
  (type $$t2 (func (param $$x f32) (param $$y f32)))

  (func $$f1 (type $$t1))
  (func $$f2 (type $$t2))
  (table funcref (elem $$f1 $$f2))

  (func (export "run")
    (call_indirect (type $$t1) (f32.const 1) (f32.const 2) (i32.const 1))
    (call_indirect (type $$t2) (f32.const 1) (f32.const 2) (i32.const 0))
  )
)`);

// ./test/core/type-equivalence.wast:102
assert_return(() => invoke($5, `run`, []), []);

// ./test/core/type-equivalence.wast:107
let $6 = instantiate(`(module
  (type $$s0 (func (param i32)))
  (type $$s1 (func (param i32 (ref $$s0))))
  (type $$s2 (func (param i32 (ref $$s0))))
  (type $$t1 (func (param (ref $$s1))))
  (type $$t2 (func (param (ref $$s2))))

  (func $$s1 (type $$s1))
  (func $$s2 (type $$s2))
  (func $$f1 (type $$t1))
  (func $$f2 (type $$t2))
  (table funcref (elem $$f1 $$f2 $$s1 $$s2))

  (func (export "run")
    (call_indirect (type $$t1) (ref.func $$s1) (i32.const 0))
    (call_indirect (type $$t1) (ref.func $$s1) (i32.const 1))
    (call_indirect (type $$t1) (ref.func $$s2) (i32.const 0))
    (call_indirect (type $$t1) (ref.func $$s2) (i32.const 1))
    (call_indirect (type $$t2) (ref.func $$s1) (i32.const 0))
    (call_indirect (type $$t2) (ref.func $$s1) (i32.const 1))
    (call_indirect (type $$t2) (ref.func $$s2) (i32.const 0))
    (call_indirect (type $$t2) (ref.func $$s2) (i32.const 1))
  )
)`);

// ./test/core/type-equivalence.wast:131
assert_return(() => invoke($6, `run`, []), []);

// ./test/core/type-equivalence.wast:136
let $7 = instantiate(`(module
  (rec (type $$t1 (func (result (ref null $$t1)))))
  (rec (type $$t2 (func (result (ref null $$t2)))))

  (func $$f1 (type $$t1) (ref.null $$t1))
  (func $$f2 (type $$t2) (ref.null $$t2))
  (table funcref (elem $$f1 $$f2))

  (func (export "run")
    (block (result (ref null $$t1)) (call_indirect (type $$t1) (i32.const 0)))
    (block (result (ref null $$t1)) (call_indirect (type $$t2) (i32.const 0)))
    (block (result (ref null $$t2)) (call_indirect (type $$t1) (i32.const 0)))
    (block (result (ref null $$t2)) (call_indirect (type $$t2) (i32.const 0)))
    (block (result (ref null $$t1)) (call_indirect (type $$t1) (i32.const 1)))
    (block (result (ref null $$t1)) (call_indirect (type $$t2) (i32.const 1)))
    (block (result (ref null $$t2)) (call_indirect (type $$t1) (i32.const 1)))
    (block (result (ref null $$t2)) (call_indirect (type $$t2) (i32.const 1)))
    (br 0)
  )
)`);

// ./test/core/type-equivalence.wast:156
assert_return(() => invoke($7, `run`, []), []);

// ./test/core/type-equivalence.wast:161
let $8 = instantiate(`(module
  (rec
    (type $$t1 (func (param i32 (ref $$t1))))
    (type $$t2 (func (param i32 (ref $$t3))))
    (type $$t3 (func (param i32 (ref $$t2))))
  )

  (rec
    (type $$u1 (func (param i32 (ref $$u1))))
    (type $$u2 (func (param i32 (ref $$u3))))
    (type $$u3 (func (param i32 (ref $$u2))))
  )

  (func $$f1 (type $$t1))
  (func $$f2 (type $$t2))
  (func $$f3 (type $$t3))
  (table funcref (elem $$f1 $$f2 $$f3))

  (func (export "run")
    (call_indirect (type $$t1) (i32.const 1) (ref.func $$f1) (i32.const 0))
    (call_indirect (type $$t2) (i32.const 1) (ref.func $$f3) (i32.const 1))
    (call_indirect (type $$t3) (i32.const 1) (ref.func $$f2) (i32.const 2))
    (call_indirect (type $$u1) (i32.const 1) (ref.func $$f1) (i32.const 0))
    (call_indirect (type $$u2) (i32.const 1) (ref.func $$f3) (i32.const 1))
    (call_indirect (type $$u3) (i32.const 1) (ref.func $$f2) (i32.const 2))
  )
)`);

// ./test/core/type-equivalence.wast:188
assert_return(() => invoke($8, `run`, []), []);

// ./test/core/type-equivalence.wast:195
let $9 = instantiate(`(module
  (type $$t1 (func (param f32 f32) (result f32)))
  (func (export "f") (param (ref $$t1)))
)`);

// ./test/core/type-equivalence.wast:199
register($9, `M`);

// ./test/core/type-equivalence.wast:200
let $10 = instantiate(`(module
  (type $$t2 (func (param $$x f32) (param $$y f32) (result f32)))
  (func (import "M" "f") (param (ref $$t2)))
)`);

// ./test/core/type-equivalence.wast:208
let $11 = instantiate(`(module
  (type $$s0 (func (param i32) (result f32)))
  (type $$s1 (func (param i32 (ref $$s0)) (result (ref $$s0))))
  (type $$s2 (func (param i32 (ref $$s0)) (result (ref $$s0))))
  (type $$t1 (func (param (ref $$s1)) (result (ref $$s2))))
  (type $$t2 (func (param (ref $$s2)) (result (ref $$s1))))
  (func (export "f1") (param (ref $$t1)))
  (func (export "f2") (param (ref $$t1)))
)`);

// ./test/core/type-equivalence.wast:217
register($11, `N`);

// ./test/core/type-equivalence.wast:218
let $12 = instantiate(`(module
  (type $$s0 (func (param i32) (result f32)))
  (type $$s1 (func (param i32 (ref $$s0)) (result (ref $$s0))))
  (type $$s2 (func (param i32 (ref $$s0)) (result (ref $$s0))))
  (type $$t1 (func (param (ref $$s1)) (result (ref $$s2))))
  (type $$t2 (func (param (ref $$s2)) (result (ref $$s1))))
  (func (import "N" "f1") (param (ref $$t1)))
  (func (import "N" "f1") (param (ref $$t2)))
  (func (import "N" "f2") (param (ref $$t1)))
  (func (import "N" "f2") (param (ref $$t1)))
)`);

// ./test/core/type-equivalence.wast:233
let $13 = instantiate(`(module
  (rec (type $$t1 (func (param i32 (ref $$t1)))))
  (func (export "f") (param (ref $$t1)))
)`);

// ./test/core/type-equivalence.wast:237
register($13, `M`);

// ./test/core/type-equivalence.wast:238
let $14 = instantiate(`(module
  (rec (type $$t2 (func (param i32 (ref $$t2)))))
  (func (import "M" "f") (param (ref $$t2)))
)`);

// ./test/core/type-equivalence.wast:246
let $15 = instantiate(`(module
  (rec
    (type $$t1 (func (param i32 (ref $$t1))))
    (type $$t2 (func (param i32 (ref $$t3))))
    (type $$t3 (func (param i32 (ref $$t2))))
  )
  (func (export "f1") (param (ref $$t1)))
  (func (export "f2") (param (ref $$t2)))
  (func (export "f3") (param (ref $$t3)))
)`);

// ./test/core/type-equivalence.wast:256
register($15, `M`);

// ./test/core/type-equivalence.wast:257
let $16 = instantiate(`(module
  (rec
    (type $$t1 (func (param i32 (ref $$t1))))
    (type $$t2 (func (param i32 (ref $$t3))))
    (type $$t3 (func (param i32 (ref $$t2))))
  )
  (func (import "M" "f1") (param (ref $$t1)))
  (func (import "M" "f2") (param (ref $$t2)))
  (func (import "M" "f3") (param (ref $$t3)))
)`);

// ./test/core/type-equivalence.wast:268
let $17 = instantiate(`(module
  (rec
    (type $$t1 (func (param i32 (ref $$t3))))
    (type $$t2 (func (param i32 (ref $$t1))))
    (type $$t3 (func (param i32 (ref $$t2))))
  )
  (func (export "f1") (param (ref $$t1)))
  (func (export "f2") (param (ref $$t2)))
  (func (export "f3") (param (ref $$t3)))
)`);

// ./test/core/type-equivalence.wast:278
register($17, `M`);

// ./test/core/type-equivalence.wast:279
let $18 = instantiate(`(module
  (rec
    (type $$t1 (func (param i32 (ref $$t3))))
    (type $$t2 (func (param i32 (ref $$t1))))
    (type $$t3 (func (param i32 (ref $$t2))))
  )
  (func (import "M" "f1") (param (ref $$t1)))
  (func (import "M" "f2") (param (ref $$t2)))
  (func (import "M" "f3") (param (ref $$t3)))
)`);

// ./test/core/type-equivalence.wast:290
let $19 = instantiate(`(module
  (rec
    (type $$t1 (func (param i32 (ref $$u1))))
    (type $$u1 (func (param f32 (ref $$t1))))
  )

  (rec
    (type $$t2 (func (param i32 (ref $$u3))))
    (type $$u2 (func (param f32 (ref $$t3))))
    (type $$t3 (func (param i32 (ref $$u2))))
    (type $$u3 (func (param f32 (ref $$t2))))
  )

  (func (export "f1") (param (ref $$t1)))
  (func (export "f2") (param (ref $$t2)))
  (func (export "f3") (param (ref $$t3)))
)`);

// ./test/core/type-equivalence.wast:307
register($19, `M`);

// ./test/core/type-equivalence.wast:308
let $20 = instantiate(`(module
  (rec
    (type $$t1 (func (param i32 (ref $$u1))))
    (type $$u1 (func (param f32 (ref $$t1))))
  )

  (rec
    (type $$t2 (func (param i32 (ref $$u3))))
    (type $$u2 (func (param f32 (ref $$t3))))
    (type $$t3 (func (param i32 (ref $$u2))))
    (type $$u3 (func (param f32 (ref $$t2))))
  )

  (func (import "M" "f1") (param (ref $$t1)))
  (func (import "M" "f2") (param (ref $$t2)))
  (func (import "M" "f3") (param (ref $$t3)))
)`);
