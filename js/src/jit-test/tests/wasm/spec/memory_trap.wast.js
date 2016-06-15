// |jit-test| test-also-wasm-baseline
// TODO current_memory opcode + traps on OOB
quit();
var importedArgs = ['memory_trap.wast']; load(scriptdir + '../spec.js');
