// |jit-test| skip-if: wasmIonDisabledByFeatures() || (!wasmCompilersPresent().match("ion") && !wasmCompilersPresent().match("cranelift")); --wasm-compiler=optimizing

// Although we may build the system with both Cranelift and Ion present, only
// one may be used, hence:
//
// * If the system is built with Cranelift enabled, then Ion may not be used.
//
// * If the system is built without Cranelift enabled, then generally Ion is
//   enabled, and so only that may be used.

assertEq(true,
	 (wasmCompilersPresent().match("ion") && wasmCompileMode() === "ion") ||
	 (wasmCompilersPresent().match("cranelift") &&
	  wasmCompileMode() === "cranelift"));
