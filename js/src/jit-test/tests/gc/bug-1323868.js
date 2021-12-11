// |jit-test| allow-oom; skip-if: helperThreadCount() === 0

gczeal(0);
startgc(8301);
offThreadCompileScript("(({a,b,c}))");
gcparam("maxBytes", gcparam("gcBytes"));
