// |jit-test| skip-if: helperThreadCount() === 0; --off-thread-parse-global
offThreadCompileModule("");
finishOffThreadModule();
