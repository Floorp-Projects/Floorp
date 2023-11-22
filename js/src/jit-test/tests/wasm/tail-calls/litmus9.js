// |jit-test| skip-if: !wasmTailCallsEnabled() || !wasmExceptionsEnabled()

// Tail-call litmus test with multiple results
//
// Mutually recursive functions implement a multi-entry loop using indirect tail
// calls, with exception handling.
//
// The functions do not have the same signatures, so if all arguments are stack
// arguments then these use different amounts of stack space.
//
// The "even" function will throw when the input is zero, so when an exception
// bubbles out of return_call it must not be caught by the exception handler in
// "odd", but must instead be delegated to the caller, which is JS code.

var ins = wasmEvalText(`
(module
  (table 2 2 funcref)
  (elem (i32.const 0) $even $odd)
  (type $even_t (func (param i32) (result i32 i32 i32)))
  (type $odd_t (func (param i32 i32) (result i32 i32 i32)))
  (tag $t)

  (func $odd (export "odd") (param $n i32) (param $dummy i32) (result i32 i32 i32)
    try (result i32 i32 i32)
    (if (result i32 i32 i32) (i32.eqz (local.get $n))
        (then (return (i32.const 0) (i32.const 32769) (i32.const -37)))
        (else (return_call_indirect (type $even_t) (i32.sub (local.get $n) (i32.const 1))
                              (i32.const 0))))
    catch_all
      unreachable
    end)

  (func $even (export "even") (param $n i32) (result i32 i32 i32)
    (if (result i32 i32 i32) (i32.eqz (local.get $n))
        (then (throw $t))
        (else (return_call_indirect (type $odd_t) (i32.sub (local.get $n) (i32.const 1)) (i32.const 33)
                              (i32.const 1)))))
)
`);

assertErrorMessage(() => ins.exports.even(TailCallIterations), WebAssembly.Exception, /.*/);
assertSame(ins.exports.odd(TailCallIterations, 33), [0, 32769, -37]);
