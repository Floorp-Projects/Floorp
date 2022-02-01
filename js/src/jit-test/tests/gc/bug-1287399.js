// |jit-test| skip-if: typeof gczeal !== 'function' || helperThreadCount() === 0

var lfGlobal = newGlobal();
gczeal(4);
for (lfLocal in this) {}
lfGlobal.offThreadCompileToStencil(`
  var desc = {
    value: 'bar',
    value: false,
  };
`);
