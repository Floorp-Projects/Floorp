setJitCompilerOption('baseline.warmup.trigger', 4);
oomTest((function () {
    gcslice(0);
}))
