// |jit-test| --wasm-relaxed-simd; skip-if: !wasmSimdEnabled()

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

// FMA/FMS, https://github.com/WebAssembly/relaxed-simd/issues/27

function fma(a, x, y) { return a + (x * y) }
function fms(a, x, y) { return a - (x * y) }

var fas = [0, 100, 500, 700];
var fxs = [10, 20, 30, 40];
var fys = [-2, -3, -4, -5];
var das = [0, 100];
var dxs = [10, 20];
var dys = [-2, -3];

for ( let [opcode, as, xs, ys, operator] of [[F32x4RelaxedFmaCode, fas, fxs, fys, fma],
                                             [F32x4RelaxedFmsCode, fas, fxs, fys, fms],
                                             [F64x2RelaxedFmaCode, das, dxs, dys, fma],
                                             [F64x2RelaxedFmsCode, das, dxs, dys, fms]] ) {
    var k = xs.length;
    var ans = iota(k).map((i) => operator(as[i], xs[i], ys[i]))

    var ins = wasmEval(moduleWithSections([
        sigSection([v2vSig]),
        declSection([0]),
        memorySection(1),
        exportSection([{funcIndex: 0, name: "run"},
                       {memIndex: 0, name: "mem"}]),
        bodySection([
            funcBody({locals:[],
                      body: [...V128StoreExpr(0, [...V128Load(16),
                                                  ...V128Load(32),
                                                  ...V128Load(48),
                                                  SimdPrefix, varU32(opcode)])]})])]));

    var mem = new (k == 4 ? Float32Array : Float64Array)(ins.exports.mem.buffer);
    set(mem, k, as);
    set(mem, 2*k, xs);
    set(mem, 3*k, ys);
    ins.exports.run();
    var result = get(mem, 0, k);
    assertSame(result, ans);
}
