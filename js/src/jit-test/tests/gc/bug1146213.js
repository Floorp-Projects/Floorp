// |jit-test| skip-if: !(getBuildConfiguration("has-gczeal")) || helperThreadCount() === 0
setGCCallback({
  action: "majorGC",
});
schedulezone(this)
gcslice(3)
var lfGlobal = newGlobal();
lfGlobal.offThreadCompileToStencil("");
var stencil = lfGlobal.finishOffThreadStencil();
lfGlobal.evalStencil(stencil);
