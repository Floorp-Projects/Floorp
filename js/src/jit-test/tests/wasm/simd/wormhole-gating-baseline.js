// |jit-test| --wasm-simd-wormhole; skip-if: wasmCompileMode() != "baseline"; include:wasm-binary.js

if (!(getBuildConfiguration().x64 || getBuildConfiguration().x86) || anySimulator())
    quit(0);

// Wormhole is available for baseline on x64/x86
assertEq(wasmSimdWormholeEnabled(), true);

function anySimulator() {
    let conf = getBuildConfiguration();
    for ( let n in conf ) {
        if (n.indexOf("simulator") >= 0 && conf[n])
            return true;
    }
    return false;
}
