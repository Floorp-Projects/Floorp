// |jit-test| skip-if: !wasmTailCallsEnabled()

// Mutually recursive functions implement a multi-entry loop using tail calls.
// The functions do not have the same signatures, so if all arguments are stack
// arguments then these use different amounts of stack space.
//
// The variable ballast is intended to test that we handle various combinations
// of stack and register arguments properly.

for ( let ballast=1; ballast < TailCallBallast; ballast++ ) {
    let vals = iota(ballast,1);
    let ps = vals.map(_ => 'i32').join(' ')
    let es = vals.map(i => `(local.get ${i})`).join(' ')
    let sum = vals.reduceRight((p,c) => `(i32.add (local.get ${c}) ${p})`, `(i32.const 0)`)
    let sumv = vals.reduce((p,c) => p+c);
    let text = `
(module
  (func $odd (export "odd") (param $n i32) (param ${ps}) (param $dummy i32) (result i32)
    (if (result i32) (i32.eqz (local.get $n))
        (return (i32.or (i32.shl ${sum} (i32.const 1)) (i32.const 0)))
        (return_call $even (i32.sub (local.get $n) (i32.const 1)) ${es})))

  (func $even (export "even") (param $n i32) (param ${ps}) (result i32)
    (if (result i32) (i32.eqz (local.get $n))
        (return (i32.or (i32.shl ${sum} (i32.const 1)) (i32.const 1)))
        (return_call $odd (i32.sub (local.get $n) (i32.const 1)) ${es} (i32.const 33)))))`;

    let ins = wasmEvalText(text);
    assertEq(ins.exports.even(TailCallIterations, ...vals), (sumv*2) + 1);
    assertEq(ins.exports.odd(TailCallIterations, ...vals, 33), (sumv*2) + 0);
}

