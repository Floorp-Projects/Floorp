// |jit-test| skip-if: helperThreadCount() === 0

setGCCallback({
    action: "majorGC"
});
gcparam("allocationThreshold", 1);
offThreadCompileToStencil("");
for (let i = 0; i < 40000; i++)
    Symbol.for(i);
eval(0);
