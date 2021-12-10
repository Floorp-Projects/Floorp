// |jit-test| skip-if: !('gczeal' in this) || helperThreadCount() === 0

gczeal(15,1);
setGCCallback({
  action: "majorGC",
});
gcslice(3);
var lfGlobal = newGlobal();
lfGlobal.offThreadCompileToStencil("");

