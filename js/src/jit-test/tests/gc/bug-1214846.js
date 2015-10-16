if (!('oomTest' in this) || helperThreadCount() === 0)
    quit();

enableSPSProfiling();
var s = newGlobal();
s.offThreadCompileScript('oomTest(() => {});');
s.runOffThreadScript();
