// |jit-test| skip-if: !wasmTailCallsEnabled()

// Tail-call litmus test with multiple results
//
// Mutually recursive functions implement a multi-entry loop using indirect
// cross-module tail calls.  The functions do not have the same signatures, so
// if all arguments are stack arguments then these use different amounts of
// stack space.
//
// Cross-module mutual recursion must be set up by JS, which is a bit of hair.
// But this should not destroy the ability to tail-call.
//
// The mutable globals accessed after the call are there to attempt to ensure
// that the correct instance is restored after the chain of tail calls.

var table = new WebAssembly.Table({initial:2, maximum:2, element:"funcref"})

var odd_cookie = 24680246;
var oddins = wasmEvalText(`
(module
  (table (import "" "table") 2 2 funcref)
  (type $even_t (func (param i32) (result i32 i32 i32)))
  (global $glob (export "g") (mut i32) (i32.const ${odd_cookie}))

  (func $odd_entry (export "odd_entry") (param $n i32) (result i32 i32 i32 i32)
    (call $odd (local.get $n) (i32.const 86))
    (global.get $glob))

  (func $odd (export "odd") (param $n i32) (param $dummy i32) (result i32 i32 i32)
    (if (result i32 i32 i32) (i32.eqz (local.get $n))
        (then (return (i32.const 0) (i32.const 32769) (i32.const -37)))
        (else (return_call_indirect (type $even_t) (i32.sub (local.get $n) (i32.const 1)) (i32.const 0))))))`,
                                      {"":{table}});

var even_cookie = 12345678;
var evenins = wasmEvalText(`
(module
  (table (import "" "table") 2 2 funcref)
  (type $odd_t (func (param i32 i32) (result i32 i32 i32)))
  (global $glob (export "g") (mut i32) (i32.const ${even_cookie}))

  (func $even_entry (export "even_entry") (param $n i32) (result i32 i32 i32 i32)
    (call $even (local.get $n))
    (global.get $glob))

  (func $even (export "even") (param $n i32) (result i32 i32 i32)
    (if (result i32 i32 i32) (i32.eqz (local.get $n))
        (then (return (i32.const 1) (i32.const -17) (i32.const 44021)))
        (else (return_call_indirect (type $odd_t) (i32.sub (local.get $n) (i32.const 1)) (i32.const 33) (i32.const 1))))))`,
                                       {"":{table}});


table.set(0, evenins.exports.even);
table.set(1, oddins.exports.odd);

assertSame(evenins.exports.even_entry(TailCallIterations), [1, -17, 44021, even_cookie]);
assertSame(oddins.exports.odd_entry(TailCallIterations+1, 33), [1, -17, 44021, odd_cookie]);
assertSame(evenins.exports.even_entry(TailCallIterations+1), [0, 32769, -37, even_cookie]);
assertSame(oddins.exports.odd_entry(TailCallIterations, 33), [0, 32769, -37, odd_cookie]);
