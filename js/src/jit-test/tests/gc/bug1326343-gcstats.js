if (!('oomTest' in this))
    quit();

setJitCompilerOption('baseline.warmup.trigger', 4);
oomTest((function () {
    gcslice(0);
}))
