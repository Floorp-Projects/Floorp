// |jit-test| skip-if: !('oomTest' in this) || helperThreadCount() === 0

oomTest(() => {
    offThreadCompileToStencil("function a(x) {");
    var stencil = finishOffThreadStencil();
    evalStencil(stencil);
});
