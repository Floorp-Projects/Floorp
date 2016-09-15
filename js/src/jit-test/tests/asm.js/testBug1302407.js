if (!this['SharedArrayBuffer'])
    quit();

setJitCompilerOption('wasm.test-mode', 1);
new SharedArrayBuffer(65536);
setJitCompilerOption('wasm.test-mode', 0)
