// |jit-test| test-also-wasm-baseline
// TODO: maximum memory size too big
// TODO: spec still requires initializers be disjoint and ordered
quit();
var importedArgs = ['memory.wast']; load(scriptdir + '../spec.js');
