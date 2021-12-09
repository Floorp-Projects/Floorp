// |jit-test| skip-if: helperThreadCount() === 0
gczeal(10);
newGlobal();
offThreadCompileToStencil("let x = 1;");
abortgc();
