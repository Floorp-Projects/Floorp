// |jit-test| test-join=--wasm-simd-wormhole; include:wasm-binary.js

// Make sure the wormhole is only available on x64
assertEq(!wasmSimdWormholeEnabled() || getBuildConfiguration().x64, true);

// Make sure the wormhole is only available with Ion
assertEq(!wasmSimdWormholeEnabled() || wasmCompileMode() == "ion", true);

function wormhole_op(opcode) {
    return `i8x16.shuffle 31 0 30 2 29 4 28 6 27 8 26 10 25 12 24 ${opcode} `
}

if (wasmSimdWormholeEnabled()) {
    let ins = wasmEvalText(`
(module
  (memory (export "mem") 1)
  (func (export "SELFTEST")
    (v128.store (i32.const 0) (${wormhole_op(WORMHOLE_SELFTEST)} (v128.const i32x4 0 0 0 0) (v128.const i32x4 0 0 0 0)))))`);
    ins.exports.SELFTEST();
    let mem = new Uint8Array(ins.exports.mem.buffer);
    let ans = [0xD, 0xE, 0xA, 0xD, 0xD, 0, 0, 0xD, 0xC, 0xA, 0xF, 0xE, 0xB, 0xA, 0xB, 0xE];
    for ( let i=0; i < 16; i++ )
        assertEq(mem[i], ans[i]);
}

