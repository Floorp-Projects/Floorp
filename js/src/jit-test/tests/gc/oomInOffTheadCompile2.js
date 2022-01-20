// |jit-test| skip-if: !('oomTest' in this) || helperThreadCount() === 0

oomTest(() => {
    offThreadCompileScript("function a(x) {");
    runOffThreadScript();
});
