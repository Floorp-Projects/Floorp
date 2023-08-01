// |jit-test| skip-if: !wasmTailCallsEnabled()

// Tail-call litmus test with multiple results
//
// Mutually recursive functions implement a multi-entry loop using indirect tail
// calls.  The functions do not have the same signatures, so if all arguments
// are stack arguments then these use different amounts of stack space.

var ins = wasmEvalText(`
(module
  (table 2 2 funcref)
  (elem (i32.const 0) $even $odd)
  (type $t (func (param i32) (result i32 i32 i32)))
  (type $q (func (param i32 i32) (result i32 i32 i32)))

  (func $odd (export "odd") (param $n i32) (param $dummy i32) (result i32 i32 i32)
    (if (result i32 i32 i32) (i32.eqz (local.get $n))
        (return (i32.const 0) (i32.const 32769) (i32.const -37))
        (return_call_indirect (type $t) (i32.sub (local.get $n) (i32.const 1)) (i32.const 0))))

  (func $even (export "even") (param $n i32) (result i32 i32 i32)
    (if (result i32 i32 i32) (i32.eqz (local.get $n))
        (return (i32.const 1) (i32.const -17) (i32.const 44021))
        (return_call_indirect (type $q) (i32.sub (local.get $n) (i32.const 1)) (i32.const 33) (i32.const 1))))
)
`);

assertSame(ins.exports.even(TailCallIterations), [1, -17, 44021]);
assertSame(ins.exports.odd(TailCallIterations, 33), [0, 32769, -37]);
