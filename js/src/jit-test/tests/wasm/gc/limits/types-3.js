// |jit-test| --setpref=wasm_gc; include:wasm.js;

loadRelativeToScript("load-mod.js");

// Limit of 1 million types (across all recursion groups)
wasmFailValidateBinary(loadMod("wasm-gc-limits-r1-t1M1.wasm"), /too many/);
