// |jit-test| error:RangeError;  skip-if: largeArrayBufferEnabled()
setJitCompilerOption("ion.warmup.trigger", 50);
setJitCompilerOption("offthread-compilation.enable", 0);
gcPreserveCode();

var i = 0;
do {
    i++;
    var ta = new Int32Array(inIon() ? 0x20000001 : 1);
} while (!inIon());
