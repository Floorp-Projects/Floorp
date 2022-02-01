// |jit-test| skip-if: helperThreadCount() === 0
offThreadCompileModuleToStencil("");
var stencil = finishOffThreadCompileModuleToStencil();
instantiateModuleStencil(stencil);
