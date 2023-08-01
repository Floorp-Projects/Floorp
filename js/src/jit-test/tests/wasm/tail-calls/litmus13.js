// |jit-test| skip-if: !wasmTailCallsEnabled()

// Once we exhaust the register arguments this will alternately grow and then
// shrink the stack frame across tail call boundaries because the increment of
// stack allocation is 16 bytes and our variability exceeds that.
//
// (This is not redundant with eg litmus2, because in that case all the
// functions have the same ballast.  Here we have different ballast, so we get
// growing and shrinking.)
//
// See litmus11 for the direct-call case.
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
  (return_call_indirect (type $ty1) (i32.const ${1 << n}) (i32.const 1)))
`;
    case ballast:
        return `
(func $f${ballast} (param ${ntimes(ballast, 'i32')}) (result i32)
    (if (result i32) (i32.eqz (global.get $glob))
        (return ${compute(ballast)})
        (block (result i32)
          (global.set $glob (i32.sub (global.get $glob) (i32.const 1)))
          (return_call_indirect (type $ty0) (i32.const 0)))))
`;
    default:
        return `
(func $f${n} (param ${ntimes(n, 'i32')}) (result i32)
  (return_call_indirect (type $ty${n+1}) (i32.const ${1 << n}) ${ntimes(n, get_local)} (i32.const ${n+1})))
`
    }
}

function types(n) {
    var ps = n == 0 ? '' : `(param ${ntimes(n, 'i32')})`;
    return `
(type $ty${n} (func ${ps} (result i32)))`;
}

function funcnames(n) {
    return ntimes(n, n => `$f${n}`)
}

for ( let ballast=1; ballast < TailCallBallast; ballast++ ) {

    let vals = iota(ballast+1).map(v => 1 << v);
    let sumv = vals.reduce((p,c) => p|c);
    let text = `
(module
  ${ntimes(ballast+1, n => types(n))}
  (table $t ${ballast+1} ${ballast+1} funcref)
  (elem (table $t) (i32.const 0) func ${funcnames(ballast+1)})
  (global $glob (mut i32) (i32.const ${TailCallIterations}))
  ${ntimes(ballast, n => build(n, ballast))}
  ${build(ballast, ballast)})
`;

    let ins = wasmEvalText(text);
    assertEq(ins.exports.f(), sumv);
}
