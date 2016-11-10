load(libdir + "wasm.js");

// Test instructions with no return value interposed between pushes and pops.
wasmFullPass(` (module
   (memory 1)

   (func (result i32)
     (local $local f64)
     i32.const 0
     i32.const 16
     i32.store
     i32.const 0
     i32.load
     i32.const 0
     if
       i32.const 0
       call $returnVoid
       i32.const 21
       i32.store
       i32.const 22
       drop
     else
       i32.const 0
       i32.const 17
       i32.store
     end
     block i32
     block i32
     i32.const 2
     if i32
       i32.const 500
     else
       i32.const 501
     end
     end
     end
     drop
     i32.const 0
     i32.load
     f64.const 5.0
     set_local $local
     f64.const 5.0
     tee_local $local
     drop
     block
       i32.const 0
       i32.const 18
       i32.store
       nop
       i32.const 0
       i32.const 19
       call $returnVoid
       i32.store
       loop
         i32.const 1
         if
           i32.const 0
           i32.const 20
           i32.store
         end
       end
     end
     i32.add
   )

   (func $returnVoid)

   (export "run" 0)
)`, 33);
