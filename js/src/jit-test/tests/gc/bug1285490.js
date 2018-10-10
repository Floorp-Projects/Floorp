// |jit-test| skip-if: helperThreadCount() === 0
gczeal(4);
offThreadCompileScript("let x = 1;");
