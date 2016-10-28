// |jit-test| test-also-wasm-baseline
// TODO syntax error (allow to reference table/memories)
quit();
var importedArgs = ['exports.wast']; load(scriptdir + '../spec.js');
