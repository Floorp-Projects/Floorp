// |jit-test| --setpref=wasm_gc; include:wasm.js;

loadRelativeToScript("load-mod.js");

// Limit of 1 million types (across all recursion groups)
wasmValidateBinary(loadMod("wasm-gc-limits-r2-t500K.wasm"));
