wasmFailValidateText(`(module
   (func (param i32) (result i32)
     (loop (if (i32.const 0) (then (br 0))) (local.get 0)))
   (export "" (func 0))
)`, /(unused values not explicitly dropped by end of block)|(values remaining on stack at end of block)/);

wasmFailValidateText(`(module
   (func (param i32)
     (loop (if (i32.const 0) (then (br 0))) (local.get 0)))
   (export "" (func 0))
)`, /(unused values not explicitly dropped by end of block)|(values remaining on stack at end of block)/);

wasmFailValidateText(`(module
   (func (param i32) (result i32)
     (loop (if (i32.const 0) (then (br 0))) (drop (local.get 0))))
   (export "" (func 0))
)`, emptyStackError);

assertEq(wasmEvalText(`(module
   (func (param i32) (result i32)
     (loop (if (i32.const 0) (then (br 0)))) (local.get 0))
   (export "" (func 0))
)`).exports[""](42), 42);

wasmEvalText(`(module (func $func$0
      (block (if (i32.const 1) (then (loop (br_table 0 (br 0))))))
  )
)`);

wasmEvalText(`(module (func
      (block $out (loop $in (br_table $out $out $in (i32.const 0))))
  )
)`);

wasmEvalText(`(module (func (result i32)
  (select
    (block (result i32)
      (drop (block (result i32)
        (br_table
         1
         0
         (i32.const 1)
         (i32.const 0)
        )
      ))
      (i32.const 2)
    )
    (i32.const 3)
    (i32.const 4)
  )
))
`);

wasmEvalText(`(module
  (func (param i32) (param i32) (result i32) (i32.const 0))
  (func (result i32)
   (call 0 (i32.const 1) (call 0 (i32.const 2) (i32.const 3)))
   (call 0 (unreachable) (i32.const 4))
  )
)`);

wasmEvalText(`
(module

 (func
  (param i32) (param i32) (param i32) (param i32)
  (result i32)
  (i32.const 0)
 )

 (func (result i32)
  (call 0
   (i32.const 42)
   (i32.const 53)
   (call 0 (i32.const 100) (i32.const 13) (i32.const 37) (i32.const 128))
   (return (i32.const 42))
  )
 )

 (export "" (func 1))
)
`).exports[""]();

wasmEvalText(`
(module
    (import "check" "one" (func (param i32)))
    (import "check" "two" (func (param i32) (param i32)))
    (func (param i32) (call 0 (local.get 0)))
    (func (param i32) (param i32) (call 1 (local.get 0) (local.get 1)))
    (func
        (call 1
            (i32.const 43)
            (block $b (result i32)
                (if (i32.const 1)
                    (then (call 0
                        (block (result i32)
                            (call 0 (i32.const 42))
                            (br $b (i32.const 10))))))
                (i32.const 44))))
    (export "foo" (func 4)))
`, {
    check: {
        one(x) {
            assertEq(x, 42);
        },
        two(x, y) {
            assertEq(x, 43);
            assertEq(y, 10);
        }
    }
}).exports.foo();

assertEq(wasmEvalText(`(module (func
 return
 (select
  (loop (result i32) (i32.const 1))
  (loop (result i32) (i32.const 2))
  (i32.const 3)
 )
 drop
) (export "" (func 0)))`).exports[""](), undefined);

wasmEvalText(`(module (func (result i32)
 (return (i32.const 0))
 (select
  (loop (result i32) (i32.const 1))
  (loop (result i32) (i32.const 2))
  (i32.const 3)
 )
))`);

wasmEvalText(`(module (func
 (block $return
  (block $beforeReturn
    (block $out
      (loop $in
       (block $otherTable
         (br_table
          $return
          $return
          $otherTable
          $beforeReturn
          (i32.const 0)
         )
       )
       (block $backTop
        (br_table
         $backTop
         $backTop
         $beforeReturn
         (i32.const 0)
        )
       )
       (br $in)
      )
    )
  )
 )
))`);

wasmFailValidateText(
`(module
  (func $func$0
   (select
    (if (result f32)
     (i32.const 0)
     (then (f32.const 0))
     (else (i32.const 0))
    )
    (if (result f32)
     (i32.const 0)
     (then (f32.const 0))
     (else (i32.const 0))
    )
    (i32.const 0)
   )
  )
)`, mismatchError("i32", "f32"));

wasmEvalText(`
(module
 (func (result i32)
  (i32.add
   (block $outer (result i32)
    (drop (block $middle (result i32)
     (block $inner (result i32)
      (br_table $middle $outer $inner (i32.const 42) (i32.const 1))
     )
     (nop)
    ))
    (i32.const 0)
   )
   (i32.const 13)
  )
 )
)
`);

wasmFailValidateText(`
(module
    (func (result i32)
      (loop
        (i32.const 0)
        (br_table 1 0 (i32.const 15))
      )
    )
)`, /(br_table targets must all have the same arity)|(br_table target labels have different types)/);

wasmFailValidateText(`
(module
  (func (result i32)
    (loop (result i32)
      (i32.const 0)
      (br_table 1 0 (i32.const 15))
    )
  )
)`, /(br_table targets must all have the same arity)|(br_table target labels have different types)/);

wasmValidateText(`
(module
    (func
        (loop
          (i32.const 0)
          (br_table 1 0 (i32.const 15))
        )
    )
)`);
