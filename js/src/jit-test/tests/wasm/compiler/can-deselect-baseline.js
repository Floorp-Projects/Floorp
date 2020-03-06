// |jit-test| --wasm-compiler=ion

if (wasmCompilersPresent().match("ion")) {
    assertEq(wasmCompileMode(), "ion");
} else {
    // No ion on arm64, so asking for ion-only disables everything.
    assertEq(wasmCompileMode(), "none");
}
