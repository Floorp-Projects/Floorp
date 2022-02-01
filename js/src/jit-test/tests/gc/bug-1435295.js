// |jit-test| skip-if: helperThreadCount() === 0 || !('oomTest' in this)

oomTest(new Function(`function execOffThread(source) {
    offThreadCompileModuleToStencil(source);
    var stencil = finishOffThreadCompileModuleToStencil();
    return instantiateModuleStencil(stencil);
}
b = execOffThread("[1, 2, 3]")
`));
