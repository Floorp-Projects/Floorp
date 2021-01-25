// |jit-test| --wasm-simd-wormhole; skip-if: wasmCompileMode() != "cranelift"; include:wasm-binary.js

if (!(getBuildConfiguration().x64 || getBuildConfiguration().x86) || anySimulator())
    quit(0);

// Wormhole is not available for cranelift
assertEq(wasmSimdWormholeEnabled(), false);

function anySimulator() {
    let conf = getBuildConfiguration();
    for ( let n in conf ) {
        if (n.indexOf("simulator") >= 0 && conf[n])
            return true;
    }
    return false;
}
