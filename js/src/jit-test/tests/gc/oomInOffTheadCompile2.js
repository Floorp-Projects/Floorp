// |jit-test| skip-if: !('oomTest' in this) || helperThreadCount() === 0

oomTest(() => {
    offThreadCompileToStencil("function a(x) {");
    var stencil = finishOffThreadCompileToStencil();
    evalStencil(stencil);
});
