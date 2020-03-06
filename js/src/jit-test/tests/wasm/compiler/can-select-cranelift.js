// |jit-test| --wasm-compiler=cranelift; skip-if: !wasmCompilersPresent().match("cranelift")

assertEq(wasmCompileMode(), "cranelift");
