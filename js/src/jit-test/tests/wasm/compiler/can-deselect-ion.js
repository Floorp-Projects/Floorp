// |jit-test| skip-if: !wasmCompilersPresent().match("baseline"); --wasm-compiler=baseline

assertEq(wasmCompileMode(), "baseline");
