// |jit-test| skip-if: helperThreadCount() === 0

gczeal(0);
offThreadCompileScript("");
startgc(0);
runOffThreadScript();
