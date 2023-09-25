// |jit-test| skip-if: !getBuildConfiguration("arm"); --arm-hwcap=armv7

// Wasm should be unavailable in this configuration: armv7 without floating point.
assertEq(typeof WebAssembly, "undefined");
