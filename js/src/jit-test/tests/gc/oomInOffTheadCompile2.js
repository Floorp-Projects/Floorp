if (!('oomTest' in this) || helperThreadCount() === 0)
    quit();

oomTest(() => {
    offThreadCompileScript("function a(x) {");
    runOffThreadScript();
});
