// |jit-test| --wasm-exceptions; skip-if: !wasmExceptionsEnabled()

// Simple test with return_call.
var ins = wasmEvalText(`(module
    (tag $exn)
    (func $f0 (result i32)
       throw $exn
    )
    (func $f1 (result i32)
      try
        return_call $f0
      catch $exn
        i32.const 3
        return
      end
      i32.const 4
    )
    (func (export "main") (result i32)
      call $f1
    )
)`);
assertErrorMessage(() => ins.exports.main(), WebAssembly.Exception, /.*/);

// Test if return stub, created by introducing the slow path,
// not interfering with exception propagation.
var ins0 = wasmEvalText(`(module
    (tag $exn)
    (func $f0 (result i32)
       throw $exn
    )
    (func (export "f") (result i32)
      call $f0
    )
    (export "t" (tag $exn))
)`);
var ins = wasmEvalText(`(module
    (import "" "t" (tag $exn))
    (import "" "f" (func $f0 (result i32)))
    (func $f1 (result i32)
      try
        return_call $f0
      catch $exn
        i32.const 3
        return
      end
      i32.const 4
    )
    (func (export "main") (result i32)
      call $f1
    )
  )`, {"":ins0.exports,});
  
assertErrorMessage(() => ins.exports.main(), WebAssembly.Exception, /.*/);  
  