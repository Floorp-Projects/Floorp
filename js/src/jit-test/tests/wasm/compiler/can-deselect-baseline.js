// |jit-test| skip-if: !wasmCompilersPresent().match("ion") || wasmIonDisabledByFeatures(); --wasm-compiler=optimizing

// When we land wasm-via-Ion/aarch64 phase 2, this can be changed back to
// testing only for Ion.
assertEq(true, wasmCompileMode() === "ion" || wasmCompileMode() === "cranelift");
