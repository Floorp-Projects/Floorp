// |jit-test| skip-if: !wasmTailCallsEnabled()

// This alternately grows and then shrinks the stack frame across tail call boundaries.  
// Here we go cross-module as well.  There is enough ballast that the stack frame will
// alternately have to grow and shrink across some of the calls.
//
// We generate one module+instance per function so that every call is cross-instance.
// Each module has an "up" function (calling the next higher index) and a "down" function
// (calling the next lower index).  The last "up" function decrements the global counter
// and optionally returns a result $down0 just calls $up0.
//
// TODO: Test that the proper instance is being restored?  Or have we done that elsewhere?

function ntimes(n, v) {
    if (typeof v == "function")
        return iota(n).map(v).join(' ');
    return iota(n).map(_ => v).join(' ');
}

function get_local(n) {
    return `(local.get ${n})`
}

function compute(ballast) {
    return iota(ballast).reduce((p,_,n) => `(i32.or ${p} (local.get ${n}))`,
                                `(i32.const ${1 << ballast})`)
}

function code(n, ballast) {
    switch (n) {
    case 0:
        return `
(func $up0 (export "f") (result i32)
  (return_call_indirect (type $ty1) (i32.const ${1 << n}) (i32.const 1)))
(func $down0 (result i32)
  (return_call $up0))`;
    case ballast:
        return `
(func $up${ballast} (param ${ntimes(ballast, 'i32')}) (result i32)
    (if (result i32) (i32.eqz (global.get $glob))
        (then (return ${compute(ballast)}))
        (else
            (block (result i32)
                (global.set $glob (i32.sub (global.get $glob) (i32.const 1)))
                (return_call_indirect (type $ty${ballast-1}) ${ntimes(ballast-1,get_local)} (i32.const ${ballast+1}))))))`;
    default:
        return `
(func $up${n} (param ${ntimes(n, 'i32')}) (result i32)
  (return_call_indirect (type $ty${n+1}) (i32.const ${1 << n}) ${ntimes(n, get_local)} (i32.const ${n+1})))
(func $down${n} (param ${ntimes(n, 'i32')}) (result i32)
  (return_call_indirect (type $ty${n-1}) ${ntimes(n-1, get_local)} (i32.const ${2*ballast-n+1})))`;
    }
}

function types(n, ballast) {
    var tys = '';
    if (n > 0) 
        tys += `
(type $ty${n-1} (func (param ${ntimes(n-1, 'i32')}) (result i32)))`;
    if (n < ballast)
        tys += `
(type $ty${n+1} (func (param ${ntimes(n+1, 'i32')}) (result i32)))`
    return tys;
}

function inits(n, ballast) {
    var inits = `
(elem (i32.const ${n}) $up${n})`
    if (n < ballast) 
        inits += `
(elem (i32.const ${2*ballast-n}) $down${n})`;
    return inits
}

for (let ballast = 1; ballast < TailCallBallast; ballast++) {
    let counter = new WebAssembly.Global({ mutable: true, value: "i32" }, TailCallIterations/10);
    let table = new WebAssembly.Table({ initial: ballast * 2 + 1, maximum: ballast * 2 + 1, element: "anyfunc" });
    let tys = ntimes(ballast + 1, n => types(n));
    let vals = iota(ballast + 1).map(v => 1 << v);
    let sumv = vals.reduce((p, c) => p | c);
    let ins = [];
    let imp = { "": { table, counter } };
    for (let i = 0; i <= ballast; i++) {
        let txt = `
(module
    ${types(i, ballast)}
    (import "" "table" (table $t ${ballast * 2 - 1} funcref))
    (import "" "counter" (global $glob (mut i32)))
    ${inits(i, ballast)}
    ${code(i, ballast)})`;
        ins[i] = wasmEvalText(txt, imp);
    }
    assertEq(ins[0].exports.f(), sumv)
}
