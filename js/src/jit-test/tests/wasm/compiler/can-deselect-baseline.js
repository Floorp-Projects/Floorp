// |jit-test| skip-if: !wasmCompilersPresent().match("ion") || wasmIonDisabledByFeatures(); --wasm-compiler=optimizing

assertEq(wasmCompileMode(), "ion");
