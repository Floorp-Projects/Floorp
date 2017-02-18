load(libdir + "wasm.js");

// In unreachable code, the current design is that validation is disabled,
// meaning we have to have a special mode in the decoder for decoding code
// that won't actually run.

wasmFullPass(`(module
   (func (result i32)
     (return (i32.const 42))
     (i32.add (f64.const 1.0) (f32.const 0.0))
     (return (f64.const 2.0))
     (if (f32.const 3.0) (i64.const 2) (i32.const 1))
     (select (f64.const -5.0) (f32.const 2.3) (f64.const 8.9))
   )
   (export "run" 0)
)`, 42);

wasmFullPass(`(module
   (func (result i32) (param i32)
     (block
        (br_if 1 (i32.const 41) (get_local 0))
        (br 1 (i32.const 42))
     )
     (i32.add (f32.const 0.0) (f64.const 1.0))
     (return (f64.const 2.0))
     (if (f32.const 3.0) (i64.const 2) (i32.const 1))
     (select (f64.const -5.0) (f32.const 2.3) (f64.const 8.9))
   )
   (export "run" 0)
)`, 42, {}, 0);
