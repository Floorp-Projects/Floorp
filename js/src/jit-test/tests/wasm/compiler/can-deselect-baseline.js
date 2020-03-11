// |jit-test| skip-if: !wasmCompilersPresent().match("ion") || wasmIonDisabledByFeatures(); --wasm-compiler=ion

assertEq(wasmCompileMode(), "ion");
