if (!('oomTest' in this) || helperThreadCount() === 0)
    quit();

enableGeckoProfiling();
var s = newGlobal();
s.offThreadCompileScript('oomTest(() => {});');
s.runOffThreadScript();
