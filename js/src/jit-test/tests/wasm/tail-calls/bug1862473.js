// |jit-test| --wasm-gc; skip-if: !wasmGcEnabled()

var ins = wasmEvalText(`(module
  (func $func1)
  (func $func2 (param i32 arrayref arrayref i32 i32 i32 i32)
    return_call $func1
  )
  (func (export "test")
    i32.const 1
    ref.null array
    ref.null array
    i32.const -2
    i32.const -3
    i32.const -4
    i32.const -5
    call $func2

    ref.null array
    ref.cast (ref array)
    drop
  )
)`);

assertErrorMessage(() => ins.exports.test(), WebAssembly.RuntimeError, /bad cast/);
