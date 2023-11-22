// |jit-test| skip-if: !wasmTailCallsEnabled()

// Tail-call litmus test with multiple results
//
// Mutually recursive functions implement a multi-entry loop using tail calls.
// The functions do not have the same signatures, so if all arguments are stack
// arguments then these use different amounts of stack space.

var ins = wasmEvalText(`
(module
  (func $odd (export "odd") (param $n i32) (param $dummy i32) (result i32 i32 i32)
    (if (result i32 i32 i32) (i32.eqz (local.get $n))
        (then (return (i32.const 0) (i32.const 32769) (i32.const -37)))
        (else (return_call $even (i32.sub (local.get $n) (i32.const 1))))))

  (func $even (export "even") (param $n i32) (result i32 i32 i32)
    (if (result i32 i32 i32) (i32.eqz (local.get $n))
        (then (return (i32.const 1) (i32.const -17) (i32.const 44021)))
        (else (return_call $odd (i32.sub (local.get $n) (i32.const 1)) (i32.const 33)))))
)
`);

assertSame(ins.exports.even(TailCallIterations), [1, -17, 44021]);
assertSame(ins.exports.odd(TailCallIterations, 33), [0, 32769, -37]);
