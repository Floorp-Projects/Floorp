// |jit-test| --wasm-relaxed-simd; skip-if: !wasmRelaxedSimdEnabled()

// Experimental opcodes.  We have no text parsing support for these yet.  The
// tests will be cleaned up and moved into ad-hack.js if the opcodes are
// adopted.

load(libdir + "wasm-binary.js");

function wasmEval(bytes, imports) {
    return new WebAssembly.Instance(new WebAssembly.Module(bytes), imports);
}

function wasmValidateAndEval(bytes, imports) {
    assertEq(WebAssembly.validate(bytes), true, "test of WasmValidate.cpp");
    return wasmEval(bytes, imports);
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

    var ins = wasmValidateAndEval(moduleWithSections([
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

    assertEq(false, WebAssembly.validate(moduleWithSections([
        sigSection([v2vSig]),
        declSection([0]),
        memorySection(1),
        exportSection([{funcIndex: 0, name: "run"},
                       {memIndex: 0, name: "mem"}]),
        bodySection([
            funcBody({locals:[],
                      body: [...V128StoreExpr(0, [...V128Load(0),
                                                  ...V128Load(0),
                                                  SimdPrefix, varU32(opcode)])]})])])));    
}


// Relaxed swizzle, https://github.com/WebAssembly/relaxed-simd/issues/22

var ins = wasmValidateAndEval(moduleWithSections([
    sigSection([v2vSig]),
    declSection([0]),
    memorySection(1),
    exportSection([{funcIndex: 0, name: "run"},
                   {memIndex: 0, name: "mem"}]),
    bodySection([
        funcBody({locals:[],
                  body: [...V128StoreExpr(0, [...V128Load(16),
                                              ...V128Load(32),
                                              SimdPrefix, varU32(I8x16RelaxedSwizzle)])]})])]));
var mem = new Uint8Array(ins.exports.mem.buffer);
var test = [1, 4, 3, 7, 123, 0, 8, 222];
set(mem, 16, test);
for (let [i, s] of [[0, 0], [0, 1], [1,1], [1, 3], [7,5]]) {
    var ans = new Uint8Array(16);
    for (let j = 0; j < 16; j++) {
        mem[32 + j] = (j * s + i) & 15;
        ans[j] = test[(j * s + i) & 15];
    }
    ins.exports.run();
    var result = get(mem, 0, 16);
    assertSame(result, ans);
}

assertEq(false, WebAssembly.validate(moduleWithSections([
    sigSection([v2vSig]),
    declSection([0]),
    memorySection(1),
    bodySection([
        funcBody({locals:[],
            body: [...V128StoreExpr(0, [...V128Load(16),
                                        SimdPrefix, varU32(I8x16RelaxedSwizzle)])]})])])));


// Relaxed MIN/MAX, https://github.com/WebAssembly/relaxed-simd/issues/33

const Neg0 = -1/Infinity;
var minMaxTests = [
    {a: 0, b: 0, min: 0, max: 0, },
    {a: Neg0, b: Neg0, min: Neg0, max: Neg0, },
    {a: 1/3, b: 2/3, min: 1/3, max: 2/3, },
    {a: -1/3, b: -2/3, min: -2/3, max: -1/3, },
    {a: -1000, b: 1, min: -1000, max: 1, },
    {a: 10, b: -2, min: -2, max: 10, },
];

for (let k of [4, 2]) {
    const minOpcode = k == 4 ? F32x4RelaxedMin : F64x2RelaxedMin;
    const maxOpcode = k == 4 ? F32x4RelaxedMax : F64x2RelaxedMax;

    var ins = wasmValidateAndEval(moduleWithSections([
        sigSection([v2vSig]),
        declSection([0, 0]),
        memorySection(1),
        exportSection([{funcIndex: 0, name: "min"},
                       {funcIndex: 1, name: "max"},
                       {memIndex: 0, name: "mem"}]),
        bodySection([
            funcBody({locals:[],
                      body: [...V128StoreExpr(0, [...V128Load(16),
                                                  ...V128Load(32),
                                                  SimdPrefix, varU32(minOpcode)])]}),
            funcBody({locals:[],
                      body: [...V128StoreExpr(0, [...V128Load(16),
                                                  ...V128Load(32),
                                                  SimdPrefix, varU32(maxOpcode)])]})])]));
    for (let i = 0; i < minMaxTests.length; i++) {
        var Ty = k == 4 ? Float32Array : Float64Array;
        var mem = new Ty(ins.exports.mem.buffer);
        var minResult = new Ty(k);
        var maxResult = new Ty(k);
        for (let j = 0; j < k; j++) {
            const {a, b, min, max } = minMaxTests[(j + i) % minMaxTests.length];
            mem[j + k] = a;
            mem[j + k * 2] = b;
            minResult[j] = min;
            maxResult[j] = max;
        }
        ins.exports.min();
        var result = get(mem, 0, k);
        assertSame(result, minResult);
        ins.exports.max();
        var result = get(mem, 0, k);
        assertSame(result, maxResult);
    }

    for (let op of [minOpcode, maxOpcode]) {
        assertEq(false, WebAssembly.validate(moduleWithSections([
            sigSection([v2vSig]),
            declSection([0, 0]),
            memorySection(1),
            exportSection([]),
            bodySection([
                funcBody({locals:[],
                          body: [...V128StoreExpr(0, [...V128Load(0),
                                                      SimdPrefix, varU32(op)])]})])])));
    }
}

// Relaxed I32x4.TruncFXXX, https://github.com/WebAssembly/relaxed-simd/issues/21

var ins = wasmValidateAndEval(moduleWithSections([
    sigSection([v2vSig]),
    declSection([0, 0, 0, 0]),
    memorySection(1),
    exportSection([{funcIndex: 0, name: "from32s"},
                   {funcIndex: 1, name: "from32u"},
                   {funcIndex: 2, name: "from64s"},
                   {funcIndex: 3, name: "from64u"},
                   {memIndex: 0, name: "mem"}]),
    bodySection([
        funcBody({locals:[],
                  body: [...V128StoreExpr(0, [...V128Load(16),
                                              SimdPrefix, varU32(I32x4RelaxedTruncSSatF32x4)])]}),
        funcBody({locals:[],
                  body: [...V128StoreExpr(0, [...V128Load(16),
                                              SimdPrefix, varU32(I32x4RelaxedTruncUSatF32x4)])]}),
        funcBody({locals:[],
                  body: [...V128StoreExpr(0, [...V128Load(16),
                                              SimdPrefix, varU32(I32x4RelaxedTruncSatF64x2SZero)])]}),
        funcBody({locals:[],
                  body: [...V128StoreExpr(0, [...V128Load(16),
                                              SimdPrefix, varU32(I32x4RelaxedTruncSatF64x2UZero)])]})])]));

var mem = ins.exports.mem.buffer;
set(new Float32Array(mem), 4, [0, 2.3, -3.4, 100000]);
ins.exports.from32s();
var result = get(new Int32Array(mem), 0, 4);
assertSame(result, [0, 2, -3, 100000]);

set(new Float32Array(mem), 4, [0, 3.3, 0x80000000, 200000]);
ins.exports.from32u();
var result = get(new Uint32Array(mem), 0, 4);
assertSame(result, [0, 3, 0x80000000, 200000]);
set(new Float32Array(mem), 4, [0, 0x80000100, 0x80000101, 0xFFFFFF00]);
ins.exports.from32u();
var result = get(new Uint32Array(mem), 0, 4);
assertSame(result, [0, 0x80000100, 0x80000100, 0xFFFFFF00]);

set(new Float64Array(mem), 2, [200000.3, -3.4]);
ins.exports.from64s();
var result = get(new Int32Array(mem), 0, 2);
assertSame(result, [200000, -3]);
set(new Float64Array(mem), 2, [0x90000000 + 0.1, 0]);
ins.exports.from64u();
var result = get(new Uint32Array(mem), 0, 2);
assertSame(result, [0x90000000, 0]);

for (let op of [I32x4RelaxedTruncSSatF32x4, I32x4RelaxedTruncUSatF32x4,
                I32x4RelaxedTruncSatF64x2SZero, I32x4RelaxedTruncSatF64x2UZero]) {
    assertEq(false, WebAssembly.validate(moduleWithSections([
        sigSection([v2vSig]),
        declSection([0]),
        memorySection(1),
        exportSection([]),
        bodySection([
            funcBody({locals:[],
                      body: [...V128StoreExpr(0, [SimdPrefix, varU32(op)])]})])])));
}

// Relaxed blend / laneselect, https://github.com/WebAssembly/relaxed-simd/issues/17

for (let [k, opcode, AT] of [[1, I8x16LaneSelect, Int8Array],
                             [2, I16x8LaneSelect, Int16Array],
                             [4, I32x4LaneSelect, Int32Array],
                             [8, I64x2LaneSelect, BigInt64Array]]) {

    var ins = wasmValidateAndEval(moduleWithSections([
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

    var mem = ins.exports.mem.buffer;
    var mem8 = new Uint8Array(mem);
    set(mem8, 16, [1,2,3,4,0,0,0,0,100,0,102,0,0,250,251,252,253]);
    set(mem8, 32, [0,0,0,0,5,6,7,8,0,101,0,103,0,254,255,0,1]);
    var c = new AT(mem, 48, 16 / k);
    for (let i = 0; i < c.length; i++) {
        // Use popcnt to randomize 0 and ~0 
        const popcnt_i = i.toString(2).replace(/0/g, "").length;
        const v = popcnt_i & 1 ? -1 : 0
        c[i] = k == 8 ? BigInt(v) : v;
    }
    ins.exports.run();
    for (let i = 0; i < 16; i++) {
        const r = c[(i / k) | 0] ? mem8[16 + i] : mem8[32 + i];
        assertEq(r, mem8[i]);
    }

    assertEq(false, WebAssembly.validate(moduleWithSections([
        sigSection([v2vSig]),
        declSection([0]),
        memorySection(1),
        exportSection([{funcIndex: 0, name: "run"},
                       {memIndex: 0, name: "mem"}]),
        bodySection([
            funcBody({locals:[],
                      body: [...V128StoreExpr(0, [...V128Load(0),
                                                  ...V128Load(0),
                                                  SimdPrefix, varU32(opcode)])]})])])));    
}
