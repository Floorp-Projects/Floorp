// |jit-test| --wasm-simd-wormhole; skip-if: wasmCompileMode() != "ion"; include:wasm-binary.js

if (!nativeIntel())
    quit(0);

// Wormhole is available for ion on x64/x86
assertEq(wasmSimdWormholeEnabled(), true);

// True if running on Intel hardware and generating code for same.
function nativeIntel() {
    var conf = getBuildConfiguration();
    if (!(conf.x64 || conf.x86))
        return false;
    if (conf.arm64 || conf.arm || conf.mips32 || conf.mips64)
        return false;
    return true;
}
