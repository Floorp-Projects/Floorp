// |jit-test| test-also-wasm-baseline
// TODO: ion compile bug with loop return values
quit();
var importedArgs = ['labels.wast']; load(scriptdir + '../spec.js');
