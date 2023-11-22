// |jit-test| skip-if: !wasmTailCallsEnabled()

// Once we exhaust the register arguments this will alternately grow and then
// shrink the stack frame across tail call boundaries because the increment of
// stack allocation is 16 bytes and our variability exceeds that.
//
// (This is not redundant with eg litmus0, because in that case all the
// functions have the same ballast.  Here we have different ballast, so we get growing and
// shrinking.)
//
// See litmus13 for the same-module call_indirect case.
// See litmus16 for the cross-module call_indirect case.

function ntimes(n, v) {
    if (typeof v == "function")
        return iota(n).map(v).join(' ');
    return iota(n).map(_ => v).join(' ');
}

function get_local(n) {
    return `(local.get ${n})`
}

function compute(ballast) {
    return iota(ballast).reduce((p,g,n) => `(i32.or ${p} (local.get ${n}))`,
                                `(i32.const ${1 << ballast})`)
}

function build(n, ballast) {
    switch (n) {
    case 0:
        return `
(func $f0 (export "f") (result i32)
  (return_call $f1 (i32.const ${1 << n})))
`;
    case ballast:
        return `
(func $f${ballast} (param ${ntimes(ballast, 'i32')}) (result i32)
    (if (result i32) (i32.eqz (global.get $glob))
        (then (return ${compute(ballast)}))
        (else (block (result i32)
          (global.set $glob (i32.sub (global.get $glob) (i32.const 1)))
          (return_call $f0)))))
`;
    default:
        return `
(func $f${n} (param ${ntimes(n, 'i32')}) (result i32)
  (return_call $f${n+1} (i32.const ${1 << n}) ${ntimes(n, get_local)}))
`
    }
}

for ( let ballast=1; ballast < TailCallBallast; ballast++ ) {

    let vals = iota(ballast+1).map(v => 1 << v);
    let sumv = vals.reduce((p,c) => p|c);
    let text = `
(module
  (global $glob (mut i32) (i32.const ${TailCallIterations}))
  ${ntimes(ballast, n => build(n, ballast))}
  ${build(ballast, ballast)})
`;

    let ins = wasmEvalText(text);
    assertEq(ins.exports.f(), sumv);
}
