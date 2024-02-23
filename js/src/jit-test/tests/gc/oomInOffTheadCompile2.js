// |jit-test| skip-if: helperThreadCount() === 0

oomTest(() => {
    offThreadCompileToStencil("function a(x) {");
    var stencil = finishOffThreadStencil();
    evalStencil(stencil);
});
