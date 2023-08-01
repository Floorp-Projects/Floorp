// |jit-test| skip-if: !wasmTailCallsEnabled() || !wasmExceptionsEnabled()

// Tail-call litmus test with multiple results
//
// Mutually recursive functions implement a multi-entry loop using tail calls,
// with exception handling.
//
// The functions do not have the same signatures, so if all arguments are stack
// arguments then these use different amounts of stack space.
//
// The "even" function will throw when the input is zero, so when an exception
// bubbles out of return_call it must not be caught by the exception handler in
// "odd", but must instead be delegated to the caller, which is JS code.

var ins = wasmEvalText(`
(module
  (tag $t)
  (func $odd (export "odd") (param $n i32) (param $dummy i32) (result i32 i32 i32)
    try (result i32 i32 i32)
    (if (result i32 i32 i32) (i32.eqz (local.get $n))
        (return (i32.const 0) (i32.const 32769) (i32.const -37))
        (return_call $even (i32.sub (local.get $n) (i32.const 1))))
    catch_all
      unreachable
    end)

  (func $even (export "even") (param $n i32) (result i32 i32 i32)
    (if (result i32 i32 i32) (i32.eqz (local.get $n))
        (throw $t)
        (return_call $odd (i32.sub (local.get $n) (i32.const 1)) (i32.const 33))))
)
`);

assertErrorMessage(() => ins.exports.even(TailCallIterations), WebAssembly.Exception, /.*/);
assertSame(ins.exports.odd(TailCallIterations, 33), [0, 32769, -37]);

assertErrorMessage(() => ins.exports.odd(TailCallIterations+1, 33), WebAssembly.Exception, /.*/);
assertSame(ins.exports.even(TailCallIterations+1), [0, 32769, -37]);
