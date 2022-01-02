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

// ./test/core/stack.wast

// ./test/core/stack.wast:1
let $0 = instantiate(`(module
  (func (export "fac-expr") (param $$n i64) (result i64)
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

  (func (export "fac-stack") (param $$n i64) (result i64)
    (local $$i i64)
    (local $$res i64)
    (local.get $$n)
    (local.set $$i)
    (i64.const 1)
    (local.set $$res)
    (block $$done
      (loop $$loop
        (local.get $$i)
        (i64.const 0)
        (i64.eq)
        (if
          (then (br $$done))
          (else
            (local.get $$i)
            (local.get $$res)
            (i64.mul)
            (local.set $$res)
            (local.get $$i)
            (i64.const 1)
            (i64.sub)
            (local.set $$i)
          )
        )
        (br $$loop)
      )
    )
    (local.get $$res)
  )

  (func (export "fac-stack-raw") (param $$n i64) (result i64)
    (local $$i i64)
    (local $$res i64)
    local.get $$n
    local.set $$i
    i64.const 1
    local.set $$res
    block $$done
      loop $$loop
        local.get $$i
        i64.const 0
        i64.eq
        if $$body
          br $$done
        else $$body
          local.get $$i
          local.get $$res
          i64.mul
          local.set $$res
          local.get $$i
          i64.const 1
          i64.sub
          local.set $$i
        end $$body
        br $$loop
      end $$loop
    end $$done
    local.get $$res
  )

  (func (export "fac-mixed") (param $$n i64) (result i64)
    (local $$i i64)
    (local $$res i64)
    (local.set $$i (local.get $$n))
    (local.set $$res (i64.const 1))
    (block $$done
      (loop $$loop
        (i64.eq (local.get $$i) (i64.const 0))
        (if
          (then (br $$done))
          (else
            (i64.mul (local.get $$i) (local.get $$res))
            (local.set $$res)
            (i64.sub (local.get $$i) (i64.const 1))
            (local.set $$i)
          )
        )
        (br $$loop)
      )
    )
    (local.get $$res)
  )

  (func (export "fac-mixed-raw") (param $$n i64) (result i64)
    (local $$i i64)
    (local $$res i64)
    (local.set $$i (local.get $$n))
    (local.set $$res (i64.const 1))
    block $$done
      loop $$loop
        (i64.eq (local.get $$i) (i64.const 0))
        if
          br $$done
        else
          (i64.mul (local.get $$i) (local.get $$res))
          local.set $$res
          (i64.sub (local.get $$i) (i64.const 1))
          local.set $$i
        end
        br $$loop
      end
    end
    local.get $$res
  )

  (global $$temp (mut i32) (i32.const 0))
  (func $$add_one_to_global (result i32)
    (local i32)
    (global.set $$temp (i32.add (i32.const 1) (global.get $$temp)))
    (global.get $$temp)
  )
  (func $$add_one_to_global_and_drop
    (drop (call $$add_one_to_global))
  )
  (func (export "not-quite-a-tree") (result i32)
    call $$add_one_to_global
    call $$add_one_to_global
    call $$add_one_to_global_and_drop
    i32.add
  )
)`);

// ./test/core/stack.wast:146
assert_return(() => invoke($0, `fac-expr`, [25n]), [
  value("i64", 7034535277573963776n),
]);

// ./test/core/stack.wast:147
assert_return(() => invoke($0, `fac-stack`, [25n]), [
  value("i64", 7034535277573963776n),
]);

// ./test/core/stack.wast:148
assert_return(() => invoke($0, `fac-mixed`, [25n]), [
  value("i64", 7034535277573963776n),
]);

// ./test/core/stack.wast:150
assert_return(() => invoke($0, `not-quite-a-tree`, []), [value("i32", 3)]);

// ./test/core/stack.wast:151
assert_return(() => invoke($0, `not-quite-a-tree`, []), [value("i32", 9)]);

// ./test/core/stack.wast:156
let $1 = instantiate(`(module
  (type $$proc (func))
  (table 1 funcref)

  (func
    (block i32.const 0 call_indirect)
    (loop i32.const 0 call_indirect)
    (if (i32.const 0) (then i32.const 0 call_indirect))
    (if (i32.const 0)
      (then i32.const 0 call_indirect)
      (else i32.const 0 call_indirect)
    )
    (block i32.const 0 call_indirect (type $$proc))
    (loop i32.const 0 call_indirect (type $$proc))
    (if (i32.const 0) (then i32.const 0 call_indirect (type $$proc)))
    (if (i32.const 0)
      (then i32.const 0 call_indirect (type $$proc))
      (else i32.const 0 call_indirect (type $$proc))
    )
    (block i32.const 0 i32.const 0 call_indirect (param i32))
    (loop i32.const 0 i32.const 0 call_indirect (param i32))
    (if (i32.const 0) (then i32.const 0 i32.const 0 call_indirect (param i32)))
    (if (i32.const 0)
      (then i32.const 0 i32.const 0 call_indirect (param i32))
      (else i32.const 0 i32.const 0 call_indirect (param i32))
    )
    (block (result i32) i32.const 0 call_indirect (result i32)) (drop)
    (loop (result i32) i32.const 0 call_indirect (result i32)) (drop)
    (if (result i32) (i32.const 0)
      (then i32.const 0 call_indirect (result i32))
      (else i32.const 0 call_indirect (result i32))
    ) (drop)
    (block i32.const 0 call_indirect (type $$proc) (param) (result))
    (loop i32.const 0 call_indirect (type $$proc) (param) (result))
    (if (i32.const 0)
      (then i32.const 0 call_indirect (type $$proc) (param) (result))
    )
    (if (i32.const 0)
      (then i32.const 0 call_indirect (type $$proc) (param) (param) (result))
      (else i32.const 0 call_indirect (type $$proc) (param) (result) (result))
    )

    block i32.const 0 call_indirect end
    loop i32.const 0 call_indirect end
    i32.const 0 if i32.const 0 call_indirect end
    i32.const 0 if i32.const 0 call_indirect else i32.const 0 call_indirect end
    block i32.const 0 call_indirect (type $$proc) end
    loop i32.const 0 call_indirect (type $$proc) end
    i32.const 0 if i32.const 0 call_indirect (type $$proc) end
    i32.const 0
    if
      i32.const 0 call_indirect (type $$proc)
    else
      i32.const 0 call_indirect (type $$proc)
    end
    block i32.const 0 i32.const 0 call_indirect (param i32) end
    loop i32.const 0 i32.const 0 call_indirect (param i32) end
    i32.const 0 if i32.const 0 i32.const 0 call_indirect (param i32) end
    i32.const 0
    if
      i32.const 0 i32.const 0 call_indirect (param i32)
    else
      i32.const 0 i32.const 0 call_indirect (param i32)
    end
    block (result i32) i32.const 0 call_indirect (result i32) end drop
    loop (result i32) i32.const 0 call_indirect (result i32) end drop
    i32.const 0
    if (result i32)
      i32.const 0 call_indirect (result i32)
    else
      i32.const 0 call_indirect (result i32)
    end drop
    block i32.const 0 call_indirect (type $$proc) (param) (result) end
    loop i32.const 0 call_indirect (type $$proc) (param) (result) end
    i32.const 0 if i32.const 0 call_indirect (type $$proc) (param) (result) end
    i32.const 0
    if
      i32.const 0 call_indirect (type $$proc) (param) (result)
    else
      i32.const 0 call_indirect (type $$proc) (param) (param) (result) (result)
    end
    i32.const 0 call_indirect
  )
)`);
