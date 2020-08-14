// |jit-test| skip-if: !wasmSimdSupported()

// Experimental opcodes.  We have no text parsing support for these yet.  The
// tests will be cleaned up and moved into ad-hack.js if the opcodes are
// adopted.

// When simd is enabled by default in release builds we will flip the value of
// SimdExperimentalEnabled to false in RELEASE_OR_BETA builds.  At that point,
// these tests will start failing in release or beta builds, and a guard
// asserting !RELEASE_OR_BETA will have to be added above.  That is how it
// should be.

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

function pmin(x, y) { return y < x ? y : x }
function pmax(x, y) { return x < y ? y : x }

function ffloor(x) { return Math.fround(Math.floor(x)) }
function fceil(x) { return Math.fround(Math.ceil(x)) }
function ftrunc(x) { return Math.fround(Math.sign(x)*Math.floor(Math.abs(x))) }
function fnearest(x) { return Math.fround(Math.round(x)) }

function dfloor(x) { return Math.floor(x) }
function dceil(x) { return Math.ceil(x) }
function dtrunc(x) { return Math.sign(x)*Math.floor(Math.abs(x)) }
function dnearest(x) { return Math.round(x) }

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

// Pseudo-min/max, https://github.com/WebAssembly/simd/pull/122
var fxs = [5, 1, -4, 2];
var fys = [6, 0, -7, 3];
var dxs = [5, 1];
var dys = [6, 0];

for ( let [opcode, xs, ys, operator] of [[F32x4PMinCode, fxs, fys, pmin],
                                         [F32x4PMaxCode, fxs, fys, pmax],
                                         [F64x2PMinCode, dxs, dys, pmin],
                                         [F64x2PMaxCode, dxs, dys, pmax]] ) {
    var k = xs.length;
    var ans = iota(k).map((i) => operator(xs[i], ys[i]))

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
                                                  SimdPrefix, varU32(opcode)])]})])]));

    var mem = new (k == 4 ? Float32Array : Float64Array)(ins.exports.mem.buffer);
    set(mem, k, xs);
    set(mem, 2*k, ys);
    ins.exports.run();
    var result = get(mem, 0, k);
    assertSame(result, ans);
}

// Widening integer dot product, https://github.com/WebAssembly/simd/pull/127

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
                                              SimdPrefix, varU32(I32x4DotSI16x8Code)])]})])]));

var xs = [5, 1, -4, 2, 20, -15, 12, 3];
var ys = [6, 0, -7, 3, 8, -1, -3, 7];
var ans = [xs[0]*ys[0] + xs[1]*ys[1],
           xs[2]*ys[2] + xs[3]*ys[3],
           xs[4]*ys[4] + xs[5]*ys[5],
           xs[6]*ys[6] + xs[7]*ys[7]];

var mem16 = new Int16Array(ins.exports.mem.buffer);
var mem32 = new Int32Array(ins.exports.mem.buffer);
set(mem16, 8, xs);
set(mem16, 16, ys);
ins.exports.run();
var result = get(mem32, 0, 4);
assertSame(result, ans);

// Rounding, https://github.com/WebAssembly/simd/pull/232

var fxs = [5.1, -1.1, -4.3, 0];
var dxs = [5.1, -1.1];

for ( let [opcode, xs, operator] of [[F32x4CeilCode, fxs, fceil],
                                     [F32x4FloorCode, fxs, ffloor],
                                     [F32x4TruncCode, fxs, ftrunc],
                                     [F32x4NearestCode, fxs, fnearest],
                                     [F64x2CeilCode, dxs, dceil],
                                     [F64x2FloorCode, dxs, dfloor],
                                     [F64x2TruncCode, dxs, dtrunc],
                                     [F64x2NearestCode, dxs, dnearest]] ) {
    var k = xs.length;
    var ans = xs.map(operator);

    var ins = wasmEval(moduleWithSections([
        sigSection([v2vSig]),
        declSection([0]),
        memorySection(1),
        exportSection([{funcIndex: 0, name: "run"},
                       {memIndex: 0, name: "mem"}]),
        bodySection([
            funcBody({locals:[],
                      body: [...V128StoreExpr(0, [...V128Load(16),
                                                  SimdPrefix, varU32(opcode)])]})])]));

    var mem = new (k == 4 ? Float32Array : Float64Array)(ins.exports.mem.buffer);
    set(mem, k, xs);
    ins.exports.run();
    var result = get(mem, 0, k);
    assertSame(result, ans);
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

