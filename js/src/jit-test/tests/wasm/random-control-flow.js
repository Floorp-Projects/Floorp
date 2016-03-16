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
