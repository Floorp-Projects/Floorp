// |jit-test| exitstatus: 6;
load(libdir + "wasm.js");

setJitCompilerOption("signals.enable", 0);
timeout(1);
wasmEvalText('(module (func (loop (br 0))) (export "" 0))')();
assertEq(true, false);
