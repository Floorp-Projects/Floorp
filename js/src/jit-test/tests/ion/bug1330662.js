setJitCompilerOption("ion.warmup.trigger", 50);
setJitCompilerOption("offthread-compilation.enable", 0);
gcPreserveCode();

for (i=0;i<10000;++i) {
    a = inIon() ? 0 : 300;
    buf = new Uint8ClampedArray(a);
}
