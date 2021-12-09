// |jit-test| skip-if: helperThreadCount() === 0

gczeal(0);
startgc(45);
offThreadCompileToStencil("print(1)");
