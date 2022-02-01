// |jit-test| skip-if: helperThreadCount() === 0
gczeal(4);
offThreadCompileToStencil("let x = 1;");
