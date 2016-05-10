load(libdir + "wasm.js");

assertEq(wasmEvalText(`(module
   (func (result i32) (param i32)
     (loop (if (i32.const 0) (br 0)) (get_local 0)))
   (export "" 0)
)`)(42), 42);

wasmEvalText(`(module (func $func$0
      (block (if (i32.const 1) (loop (br_table 0 (br 0)))))
  )
)`);

wasmEvalText(`(module (func
      (loop $out $in (br_table $out $out $in (i32.const 0)))
  )
)`);

wasmEvalText(`(module (func
  (select
    (block
      (block
        (br_table
         1
         0
         (i32.const 1)
        )
      )
      (i32.const 2)
    )
    (i32.const 3)
    (i32.const 4)
  )
))
`);

wasmEvalText(`(module
  (func (result i32) (param i32) (param i32) (i32.const 0))
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

 (export "" 1)
)
`)();

wasmEvalText(`
(module
    (import "check" "one" (param i32))
    (import "check" "two" (param i32) (param i32))
    (func (param i32) (call_import 0 (get_local 0)))
    (func (param i32) (param i32) (call_import 1 (get_local 0) (get_local 1)))
    (func
        (call 1
            (i32.const 43)
            (block $b
                (if (i32.const 1)
                    (call 0
                        (block
                            (call 0 (i32.const 42))
                            (br $b (i32.const 10)))))
                (i32.const 44))))
    (export "foo" 2))
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
}).foo();

wasmEvalText(`(module (func
 (return (i32.const 0))
 (select
  (loop (i32.const 1))
  (loop (i32.const 2))
  (i32.const 3)
 )
))`);

wasmEvalText(`(module (func
 (block $return
  (block $beforeReturn
   (loop $out $in
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
))`);

wasmEvalText(`
(module
  (func $func$0
   (select
    (if
     (i32.const 0)
     (f32.const 0)
     (i32.const 0)
    )
    (if
     (i32.const 0)
     (f32.const 0)
     (i32.const 0)
    )
    (i32.const 0)
   )
  )
)
`);

wasmEvalText(`
(module
 (func
  (i32.add
   (block $outer
    (block $middle
     (block $inner
      (br_table $middle $outer $inner (i32.const 42) (i32.const 1))
     )
     (nop)
    )
    (i32.const 0)
   )
   (i32.const 13)
  )
 )
)
`);
