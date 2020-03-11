// |jit-test| --wasm-compiler=cranelift; skip-if: !wasmCompilersPresent().match("cranelift") || wasmCraneliftDisabledByFeatures()

assertEq(wasmCompileMode(), "cranelift");
