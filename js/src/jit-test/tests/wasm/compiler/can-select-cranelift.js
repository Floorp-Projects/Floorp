// |jit-test| --wasm-compiler=optimizing; skip-if: !wasmCompilersPresent().match("cranelift") || wasmCraneliftDisabledByFeatures()

assertEq(wasmCompileMode(), "cranelift");
