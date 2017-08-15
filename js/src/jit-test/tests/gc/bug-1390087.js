if (helperThreadCount() === 0)
    quit();

setGCCallback({
    action: "majorGC"
});
gcparam("allocationThreshold", 1);
offThreadCompileScript("");
for (let i = 0; i < 40000; i++)
    Symbol.for(i);
eval(0);
