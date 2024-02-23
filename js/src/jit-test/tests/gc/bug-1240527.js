// |jit-test| skip-if: helperThreadCount() === 0

offThreadCompileToStencil(`
 oomTest(() => "".search(/d/));
 fullcompartmentchecks(3);
`);
var stencil = finishOffThreadStencil();
evalStencil(stencil);
