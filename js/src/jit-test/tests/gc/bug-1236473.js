// |jit-test| skip-if: !('oomTest' in this)

oomTest(() => {
    offThreadCompileToStencil(`try {} catch (NaN) {}`);
    var stencil = finishOffThreadStencil();
    evalStencil(stencil);
});
