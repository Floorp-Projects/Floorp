if (!this['SharedArrayBuffer'])
    quit();

setJitCompilerOption('asmjs.atomics.enable', 1);
new SharedArrayBuffer(65536);
setJitCompilerOption('asmjs.atomics.enable', 0)
