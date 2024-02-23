oomTest(() => {
    offThreadCompileToStencil(`try {} catch (NaN) {}`);
    var stencil = finishOffThreadStencil();
    evalStencil(stencil);
});
