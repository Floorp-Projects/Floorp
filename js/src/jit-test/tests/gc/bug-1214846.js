// |jit-test| skip-if: !hasFunction.oomTest || helperThreadCount() === 0

enableGeckoProfiling();
var s = newGlobal();
s.offThreadCompileToStencil('oomTest(() => {});');
var stencil = s.finishOffThreadStencil();
s.evalStencil(stencil);
