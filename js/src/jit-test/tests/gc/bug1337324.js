// |jit-test| skip-if: !('oomTest' in this)
oomTest(function () {
    offThreadCompileModuleToStencil('');
    var stencil = finishOffThreadStencil();
    instantiateModuleStencil(stencil);
});
