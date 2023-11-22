// |jit-test| skip-if: !wasmTailCallsEnabled()

// Loop implemented using tail calls - the call passes as many arguments as
// the function receives, of the same types.
//
// The variable ballast is intended to test that we handle various combinations
// of stack and register arguments properly.

for ( let ballast=1; ballast < TailCallBallast; ballast++ ) {
    let vals = iota(ballast,1);
    let ps = vals.map(_ => 'i32').join(' ')
    let es = vals.map(i => `(local.get ${1+i})`).join(' ')
    let sum = vals.reduceRight((p,c) => `(i32.add (local.get ${c+1}) ${p})`, `(i32.const 0)`)
    let sumv = vals.reduce((p,c) => p+c);
    let text = `
(module
  (func $loop (export "loop") (param $n i32) (param $q i32) (param ${ps}) (result i32)
    (if (result i32) (i32.eqz (local.get $n))
        (then (return (i32.add (local.get $q) ${sum})))
        (else (return_call $loop (i32.sub (local.get $n) (i32.const 1)) (i32.add (local.get $q) (i32.const 1)) ${es})))))
`;
    let ins = wasmEvalText(text);
    assertEq(ins.exports.loop(TailCallIterations, ...vals), TailCallIterations + sumv);
}
