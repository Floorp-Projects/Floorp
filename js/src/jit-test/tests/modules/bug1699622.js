// |jit-test| skip-if: helperThreadCount() === 0
offThreadCompileModuleToStencil("");
var stencil = finishOffThreadStencil();
instantiateModuleStencil(stencil);
