// |jit-test| skip-if: !('oomTest' in this) || helperThreadCount() === 0

enableGeckoProfiling();
var s = newGlobal();
s.offThreadCompileScript('oomTest(() => {});');
s.runOffThreadScript();
