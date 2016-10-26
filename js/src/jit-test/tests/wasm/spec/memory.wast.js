// |jit-test| test-also-wasm-baseline
// TODO initializer expression can reference global module-defined variables?
// TODO data segment fitting must be done at instantiation time, not compile time
// TODO module with 0-sized memory can accept empty segments at any offsets?
quit();
var importedArgs = ['memory.wast']; load(scriptdir + '../spec.js');
