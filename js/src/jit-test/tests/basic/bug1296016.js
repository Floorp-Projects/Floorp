// |jit-test| skip-if: helperThreadCount() === 0
offThreadCompileToStencil(``);
evalInWorker(`
var stencil = finishOffThreadCompileToStencil();
evalStencil(stencil);
`);
