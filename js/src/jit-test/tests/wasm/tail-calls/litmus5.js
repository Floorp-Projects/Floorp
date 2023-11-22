// |jit-test| skip-if: !wasmTailCallsEnabled()

// Mutually recursive functions implement a multi-entry loop using indirect
// cross-module tail calls.  The functions do not have the same signatures, so
// if all arguments are stack arguments then these use different amounts of
// stack space.
//
// Cross-module mutual recursion must be set up by JS, which is a bit of hair.
// But this should not destroy the ability to tail-call.
//
// The variable ballast is intended to test that we handle various combinations
// of stack and register arguments properly.
//
// The mutable globals accessed after the call are there to attempt to ensure
// that the correct instance is restored after the chain of tail calls.

for ( let ballast=1; ballast < TailCallBallast; ballast++ ) {
    let vals = iota(ballast,1);
    let ps = vals.map(_ => 'i32').join(' ')
    let es = vals.map(i => `(local.get ${i})`).join(' ')
    let sum = vals.reduceRight((p,c) => `(i32.add (local.get ${c}) ${p})`, `(i32.const 0)`)
    let sumv = vals.reduce((p,c) => p+c);

    let odd_cookie = 24680246;
    let oddtext = `
(module
  (table (import "" "table") 2 2 funcref)
  (type $even_t (func (param i32 ${ps}) (result i32)))
  (global $glob (export "g") (mut i32) (i32.const ${odd_cookie}))

  (func $odd_entry (export "odd_entry") (param $n i32) (param ${ps}) (result i32)
    (call $odd (local.get $n) ${es} (i32.const 86))
    (global.get $glob)
    i32.add)

  (func $odd (export "odd") (param $n i32) (param ${ps}) (param $dummy i32) (result i32)
    (if (result i32) (i32.eqz (local.get $n))
        (then (return (i32.or (i32.shl ${sum} (i32.const 1)) (i32.const 0))))
        (else (return_call_indirect (type $even_t) (i32.sub (local.get $n) (i32.const 1)) ${es} (i32.const 0))))))`

    let even_cookie = 12345678;
    let eventext = `
(module
  (table (import "" "table") 2 2 funcref)
  (type $odd_t (func (param i32 ${ps} i32) (result i32)))
  (global $glob (export "g") (mut i32) (i32.const ${even_cookie}))

  (func $even_entry (export "even_entry") (param $n i32) (param ${ps}) (result i32)
    (call $even (local.get $n) ${es})
    (global.get $glob)
    i32.add)

  (func $even (export "even") (param $n i32) (param ${ps}) (result i32)
    (if (result i32) (i32.eqz (local.get $n))
        (then (return (i32.or (i32.shl ${sum} (i32.const 1)) (i32.const 1))))
        (else (return_call_indirect (type $odd_t) (i32.sub (local.get $n) (i32.const 1)) ${es} (i32.const 33) (i32.const 1))))))`

    let table = new WebAssembly.Table({initial:2, maximum:2, element:"funcref"})

    let oddins = wasmEvalText(oddtext, {"":{table}});
    let evenins = wasmEvalText(eventext, {"":{table}});

    table.set(0, evenins.exports.even);
    table.set(1, oddins.exports.odd);

    assertEq(evenins.exports.even_entry(TailCallIterations, ...vals), ((sumv*2) + 1) + even_cookie);
    assertEq(oddins.exports.odd_entry(TailCallIterations, ...vals, 33), ((sumv*2) + 0) + odd_cookie);
}
