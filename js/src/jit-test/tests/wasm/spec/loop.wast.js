// |jit-test| test-also-wasm-baseline
// TODO: bug in the test: https://github.com/WebAssembly/spec/issues/338
// TODO: ion compile bug with loop return values
quit();
var importedArgs = ['loop.wast']; load(scriptdir + '../spec.js');
