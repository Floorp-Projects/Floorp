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
