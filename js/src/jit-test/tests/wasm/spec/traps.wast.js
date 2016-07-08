// |jit-test| test-also-wasm-baseline
// TODO dce'd effectful instructions
quit();
var importedArgs = ['traps.wast']; load(scriptdir + '../spec.js');
