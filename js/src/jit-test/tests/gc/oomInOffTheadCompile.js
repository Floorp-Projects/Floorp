// |jit-test| skip-if: !('oomTest' in this) || helperThreadCount() === 0

oomTest(() => {
    offThreadCompileToStencil(
        `
        function f(x) {
            if (x == 0)
                return "foobar";
            return 1 + f(x - 1);
        }
        f(5);
        `);
    var stencil = finishOffThreadStencil();
    evalStencil(stencil);
});
