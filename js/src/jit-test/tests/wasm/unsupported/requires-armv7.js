// |jit-test| skip-if: !getBuildConfiguration("arm"); --arm-hwcap=vfp

// Wasm should be unavailable in this configuration: floating point without
// armv7, and armv7 is required for atomics and unaligned accesses.
assertEq(typeof WebAssembly, "undefined");
