// |jit-test| exitstatus:3
// TODO: wrapping offsets. Fails in WasmIonCompile, which only way to signal an
// error is OOMing, thus we signal a fake oom here.
var importedArgs = ['address.wast']; load(scriptdir + '../spec.js');
