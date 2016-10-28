// |jit-test| test-also-wasm-baseline
// TODO initializer expression can reference global mutable (module-defined) variables?
quit();
var importedArgs = ['memory.wast']; load(scriptdir + '../spec.js');
