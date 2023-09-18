// |jit-test| skip-if: !wasmSimdEnabled()
// Tests if combination of comparsion and bitselect produces correct result.
// On x86/64 platforms, it is expected to replace slow bitselect emulation,
// with its faster laneselect equivalent (pblendvb).
// See bug 1751488 for more information.

let verifyCodegen = _method => {};
if (hasDisassembler() && wasmCompileMode() == "ion" &&
    getBuildConfiguration("x64") && !getBuildConfiguration("simulator")) {
  if (isAvxPresent()) {
    verifyCodegen = method => {
        assertEq(wasmDis(method, {asString: true}).includes('vpblendvb'), true);
    };
  } else {
    verifyCodegen = method => {
        assertEq(wasmDis(method, {asString: true}).includes("pblendvb"), true);
    };
  }
}

const checkOps = {
  eq(a, b) { return a == b; },
  ne(a, b) { return a != b; },
  lt(a, b) { return a < b; },
  le(a, b) { return a <= b; },
  gt(a, b) { return a > b; },
  ge(a, b) { return a >= b; },
};
const checkPattern = new Uint8Array(Array(32).fill(null).map((_, i) => i));

for (let [laneSize, aty_s, aty_u] of [
    [8, Int8Array, Uint8Array], [16, Int16Array, Uint16Array],
    [32, Int32Array, Uint32Array], [64, BigInt64Array, BigUint64Array]]) {
    const laneCount = 128 / laneSize; 
    const ty = `i${laneSize}x${laneCount}`;
    for (let op of ['eq', 'ne', 'lt_s', 'le_s', 'gt_s', 'ge_s', 'lt_u', 'le_u', 'gt_u', 'ge_u']) {
        if (laneSize == 64 && op.includes('_u')) continue;
        const wrap = laneSize < 64 ? x => x : x => BigInt(x);
        const aty = op.includes('_u') ? aty_u : aty_s;
        const check = checkOps[op.replace(/_[us]$/, "")];
        // Items to test: 0, 1, all 1s, top half 1s, low half 1s, top bit 1
        const testData = new aty([wrap(0), wrap(1), ~wrap(0), ~wrap(0) << wrap(laneSize / 2),
                                  ~((~wrap(0)) << wrap(laneSize / 2)), wrap(1) << wrap(laneSize - 1)]);
        const ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`(module
            (memory (export "memory") 1)
            (func (export "run")
              (v128.store (i32.const 32)
                (v128.bitselect (v128.load (i32.const 64)) (v128.load (i32.const 80)) (${ty}.${op} (v128.load (i32.const 0)) (v128.load (i32.const 16))))) ))`)));
        const mem = new aty(ins.exports.memory.buffer);
        const memI8 = new Uint8Array(ins.exports.memory.buffer);
        memI8.subarray(64, 96).set(checkPattern);
        verifyCodegen(ins.exports.run);
        for (let i = 0; i < testData.length; i++) {
            for (let j = 0; j < testData.length; j++) {
                for (let q = 0; q < laneCount; q++) {
                    mem[q] = testData[(i + q) % testData.length];
                    mem[q + laneCount] = testData[(j + q) % testData.length];
                }
                ins.exports.run();
                for (let q = 0; q < laneCount; q++) {
                    const val = check(mem[q], mem[q + laneCount]);
                    const n = laneSize >> 3;
                    for (let k = 0; k < n; k++) {
                        assertEq(checkPattern[q * n + k + (val ? 0 : 16)],
                                 memI8[32 + q * n + k]);
                    }
                }
            }
        }
    }
}

for (let [laneSize, aty] of [[32, Float32Array], [64, Float64Array]]) {
    const laneCount = 128 / laneSize; 
    const ty = `f${laneSize}x${laneCount}`;
    for (let op of ['eq', 'ne', 'lt', 'le', 'gt', 'ge']) {
        const check = checkOps[op];
        // Items to test: 0, 1, -1, PI, NaN, Inf, -0, -Inf
        const testData = new aty([0, 1, -1, Math.PI, NaN, Infinity, 0/-Infinity, -Infinity]);
        const ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`(module
            (memory (export "memory") 1)
            (func (export "run")
              (v128.store (i32.const 32)
                (v128.bitselect (v128.load (i32.const 64)) (v128.load (i32.const 80)) (${ty}.${op} (v128.load (i32.const 0)) (v128.load (i32.const 16))))) ))`)));
        const mem = new aty(ins.exports.memory.buffer);
        const memI8 = new Uint8Array(ins.exports.memory.buffer);
        memI8.subarray(64, 96).set(checkPattern);
        verifyCodegen(ins.exports.run);        
        for (let i = 0; i < testData.length; i++) {
            for (let j = 0; j < testData.length; j++) {
                for (let q = 0; q < laneCount; q++) {
                    mem[q] = testData[(i + q) % testData.length];
                    mem[q + laneCount] = testData[(j + q) % testData.length];
                }
                ins.exports.run();
                for (let q = 0; q < laneCount; q++) {
                    const val = check(mem[q], mem[q + laneCount]);
                    const n = laneSize >> 3;
                    for (let k = 0; k < n; k++) {
                        assertEq(checkPattern[q * n + k + (val ? 0 : 16)],
                                 memI8[32 + q * n + k]);
                    }
                }
            }
        }
    }
}
