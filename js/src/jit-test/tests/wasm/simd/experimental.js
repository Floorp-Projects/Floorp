// |jit-test| skip-if: !wasmSimdExperimentalEnabled()

// Experimental opcodes.  We have no text parsing support for these yet.  The
// tests will be cleaned up and moved into ad-hack.js if the opcodes are
// adopted.

load(libdir + "wasm-binary.js");

function wasmEval(bytes, imports) {
    return new WebAssembly.Instance(new WebAssembly.Module(bytes), imports);
}

function get(arr, loc, len) {
    let res = [];
    for ( let i=0; i < len; i++ ) {
        res.push(arr[loc+i]);
    }
    return res;
}

function set(arr, loc, vals) {
    for ( let i=0; i < vals.length; i++ ) {
        if (arr instanceof BigInt64Array) {
            arr[loc+i] = BigInt(vals[i]);
        } else {
            arr[loc+i] = vals[i];
        }
    }
}

function assertSame(got, expected) {
    assertEq(got.length, expected.length);
    for ( let i=0; i < got.length; i++ ) {
        let g = got[i];
        let e = expected[i];
        if (typeof g != typeof e) {
            if (typeof g == "bigint")
                e = BigInt(e);
            else if (typeof e == "bigint")
                g = BigInt(g);
        }
        assertEq(g, e);
    }
}

function iota(len) {
    let xs = [];
    for ( let i=0 ; i < len ; i++ )
        xs.push(i);
    return xs;
}

const v2vSig = {args:[], ret:VoidCode};

function V128Load(addr) {
    return [I32ConstCode, varS32(addr),
            SimdPrefix, V128LoadCode, 4, varU32(0)]
}

function V128StoreExpr(addr, v) {
    return [I32ConstCode, varS32(addr),
            ...v,
            SimdPrefix, V128StoreCode, 4, varU32(0)];
}

// Zero-extending SIMD load, https://github.com/WebAssembly/simd/pull/237

for ( let [opcode, k, log2align, cons, cast] of [[V128Load32ZeroCode, 4, 2, Int32Array, Number],
                                                 [V128Load64ZeroCode, 2, 3, BigInt64Array, BigInt]] ) {
    var ins = wasmEval(moduleWithSections([
        sigSection([v2vSig]),
        declSection([0]),
        memorySection(1),
        exportSection([{funcIndex: 0, name: "run"},
                       {memIndex: 0, name: "mem"}]),
        bodySection([
            funcBody({locals:[],
                      body: [...V128StoreExpr(0, [I32ConstCode, varU32(16),
                                                  SimdPrefix, varU32(opcode), log2align, varU32(0)])]})])]));

    var mem = new cons(ins.exports.mem.buffer);
    mem[k] = cast(37);
    ins.exports.run();
    var result = get(mem, 0, k);
    assertSame(result, iota(k).map((v) => v == 0 ? 37 : 0));
}

