oomTest(function () {
    offThreadCompileModuleToStencil('');
    var stencil = finishOffThreadStencil();
    instantiateModuleStencil(stencil);
});
