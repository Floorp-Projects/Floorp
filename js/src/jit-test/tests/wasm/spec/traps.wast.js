// |jit-test| test-also-wasm-baseline
// TODO (baseline should trap on invalid conversion)
quit();
var importedArgs = ['traps.wast']; load(scriptdir + '../spec.js');
