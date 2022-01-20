// |jit-test| skip-if: !('oomTest' in this) || helperThreadCount() === 0

enableGeckoProfiling();
var s = newGlobal();
s.offThreadCompileToStencil('oomTest(() => {});');
var stencil = s.finishOffThreadCompileToStencil();
s.evalStencil(stencil);
