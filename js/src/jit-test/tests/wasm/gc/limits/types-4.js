// |jit-test| --setpref=wasm_gc; include:wasm.js;

loadRelativeToScript("load-mod.js");

// Limit of 1 million types (across all recursion groups)
wasmFailValidateBinary(loadMod("wasm-gc-limits-r2-t500K1.wasm"), /too many/);
