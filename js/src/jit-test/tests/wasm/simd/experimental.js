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

// FMA/FNMA, https://github.com/WebAssembly/relaxed-simd/issues/27 and
// https://github.com/WebAssembly/relaxed-simd/pull/81

function fma(x, y, a) { return (x * y) + a; }
function fnma(x, y, a) { return - (x * y) + a; }

var fxs = [10, 20, 30, 40];
var fys = [-2, -3, -4, -5];
var fas = [0, 100, 500, 700];
var dxs = [10, 20];
var dys = [-2, -3];
var das = [0, 100];

for ( let [opcode, xs, ys, as, operator] of [[F32x4RelaxedMaddCode, fxs, fys, fas, fma],
                                             [F32x4RelaxedNmaddCode, fxs, fys, fas, fnma],
                                             [F64x2RelaxedMaddCode, dxs, dys, das, fma],
                                             [F64x2RelaxedNmaddCode, dxs, dys, das, fnma]] ) {
    var k = xs.length;
    var ans = iota(k).map((i) => operator(xs[i], ys[i], as[i]))

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
    set(mem, k, xs);
    set(mem, 2*k, ys);
    set(mem, 3*k, as);
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
                                              SimdPrefix, varU32(I8x16RelaxedSwizzleCode)])]})])]));
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
                                        SimdPrefix, varU32(I8x16RelaxedSwizzleCode)])]})])])));


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
    const minOpcode = k == 4 ? F32x4RelaxedMinCode : F64x2RelaxedMinCode;
    const maxOpcode = k == 4 ? F32x4RelaxedMaxCode : F64x2RelaxedMaxCode;

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
                                              SimdPrefix, varU32(I32x4RelaxedTruncSSatF32x4Code)])]}),
        funcBody({locals:[],
                  body: [...V128StoreExpr(0, [...V128Load(16),
                                              SimdPrefix, varU32(I32x4RelaxedTruncUSatF32x4Code)])]}),
        funcBody({locals:[],
                  body: [...V128StoreExpr(0, [...V128Load(16),
                                              SimdPrefix, varU32(I32x4RelaxedTruncSatF64x2SZeroCode)])]}),
        funcBody({locals:[],
                  body: [...V128StoreExpr(0, [...V128Load(16),
                                              SimdPrefix, varU32(I32x4RelaxedTruncSatF64x2UZeroCode)])]})])]));

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
var result = get(new Int32Array(mem), 0, 4);
assertSame(result, [200000, -3, 0, 0]);
set(new Float64Array(mem), 2, [0x90000000 + 0.1, 0]);
ins.exports.from64u();
var result = get(new Uint32Array(mem), 0, 4);
assertSame(result, [0x90000000, 0, 0, 0]);

for (let op of [I32x4RelaxedTruncSSatF32x4Code, I32x4RelaxedTruncUSatF32x4Code,
                I32x4RelaxedTruncSatF64x2SZeroCode, I32x4RelaxedTruncSatF64x2UZeroCode]) {
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

for (let [k, opcode, AT] of [[1, I8x16RelaxedLaneSelectCode, Int8Array],
                             [2, I16x8RelaxedLaneSelectCode, Int16Array],
                             [4, I32x4RelaxedLaneSelectCode, Int32Array],
                             [8, I64x2RelaxedLaneSelectCode, BigInt64Array]]) {

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


// Relaxed rounding q-format multiplication.
var ins = wasmValidateAndEval(moduleWithSections([
    sigSection([v2vSig]),
    declSection([0]),
    memorySection(1),
    exportSection([{funcIndex: 0, name: "relaxed_q15mulr_s"},
                    {memIndex: 0, name: "mem"}]),
    bodySection([
        funcBody({locals:[],
                    body: [...V128StoreExpr(0, [...V128Load(16),
                                                ...V128Load(32),
                                                SimdPrefix, varU32(I16x8RelaxedQ15MulrSCode)])]})])]));

var mem16 = new Int16Array(ins.exports.mem.buffer);
for (let [as, bs] of cross([
        [1, -3, 5, -7, 11, -13, -17, 19],
        [-1, 0, 16, -32, 64, 128, -1024, 0, 1],
        [1,2,-32768,32767,1,4,-32768,32767]]) ) {
    set(mem16, 8, as);
    set(mem16, 16, bs);
    ins.exports.relaxed_q15mulr_s();
    const result = get(mem16, 0, 8);
    for (let i = 0; i < 8; i++) {
        const expected = (as[i] * bs[i] + 0x4000) >> 15;
        if (as[i] == -32768 && bs[i] == -32768) continue;
        assertEq(expected, result[i], `result of ${as[i]} * ${bs[i]}`);
    }
}


// Check relaxed dot product results.
var ins = wasmValidateAndEval(moduleWithSections([
    sigSection([v2vSig]),
    declSection([0]),
    memorySection(1),
    exportSection([{funcIndex: 0, name: "dot_i8x16_i7x16_s"},
                    {memIndex: 0, name: "mem"}]),
    bodySection([
        funcBody({locals:[],
                    body: [...V128StoreExpr(0, [...V128Load(16),
                                                ...V128Load(32),
                                                SimdPrefix, varU32(I16x8DotI8x16I7x16SCode)])]})])]));
var mem8 = new Int8Array(ins.exports.mem.buffer);
var mem16 = new Int16Array(ins.exports.mem.buffer);
var test7bit = [1, 2, 3, 4, 5, 64, 65, 127, 127, 0, 0,
                1, 65, 64, 2, 3, 0, 0, 127, 127, 5, 4];
var testNeg = test7bit.concat(test7bit.map(i => ~i));
for (let ai = 0; ai < testNeg.length - 15; ai++)
    for (let bi = 0; bi < test7bit.length - 15; bi++) {
        set(mem8, 16, testNeg.slice(ai, ai + 16));
        set(mem8, 32, test7bit.slice(bi, bi + 16));
        ins.exports.dot_i8x16_i7x16_s();
        const result = get(mem16, 0, 8);
        for (let i = 0; i < 8; i++) {
            const expected = ((testNeg[ai + i * 2] * test7bit[bi + i * 2]) +
                            (testNeg[ai + i * 2 + 1] * test7bit[bi + i * 2 + 1])) | 0;
            assertEq(expected, result[i]);
        }
    }

var ins = wasmValidateAndEval(moduleWithSections([
    sigSection([v2vSig]),
    declSection([0]),
    memorySection(1),
    exportSection([{funcIndex: 0, name: "dot_i8x16_i7x16_add_s"},
                    {memIndex: 0, name: "mem"}]),
    bodySection([
        funcBody({locals:[],
                    body: [...V128StoreExpr(0, [...V128Load(16),
                                                ...V128Load(32),
                                                ...V128Load(48),
                                                SimdPrefix, varU32(I32x4DotI8x16I7x16AddSCode)])]})])]));
var mem8 = new Int8Array(ins.exports.mem.buffer);
var mem32 = new Int32Array(ins.exports.mem.buffer);
var test7bit = [1, 2, 3, 4, 5, 64, 65, 127, 127, 0, 0,
                1, 65, 64, 2, 3, 0, 0, 127, 127, 5, 4];
var testNeg = test7bit.concat(test7bit.map(i => ~i));
var testAcc = [0, 12, 65336, -1, 0x10000000, -0xffffff];
for (let ai = 0; ai < testNeg.length - 15; ai++)
    for (let bi = 0; bi < test7bit.length - 15; bi++)
        for (let ci = 0; ci < testAcc.length - 3; ci++) {
            set(mem8, 16, testNeg.slice(ai, ai + 16));
            set(mem8, 32, test7bit.slice(bi, bi + 16));
            set(mem32, 48/4, testAcc.slice(ci, ci + 4));
            ins.exports.dot_i8x16_i7x16_add_s();
            const result = get(mem32, 0, 4);
            for (let i = 0; i < 4; i++) {
                const a1 = (testNeg[ai + i * 4] * test7bit[bi + i * 4]) +
                           (testNeg[ai + i * 4 + 1] * test7bit[bi + i * 4 + 1]);
                const a2 = (testNeg[ai + i * 4 + 2] * test7bit[bi + i * 4 + 2]) +
                           (testNeg[ai + i * 4 + 3] * test7bit[bi + i * 4 + 3]);
                const expected = (testAcc[ci + i] + a1 + a2) | 0;
                assertEq(expected, result[i]);
            }
        }
