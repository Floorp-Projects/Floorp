
setJitCompilerOption("offthread-compilation.enable", 0);
setJitCompilerOption("ion.warmup.trigger", 0);

for (let j = 0; j < 2; ++j) {
    let z = j ? 0n : 1;
    if (z) {
        z = 0;
    } else {
        z = 0;
    }
}
