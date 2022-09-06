// |jit-test| skip-if: !('oomTest' in this) || helperThreadCount() === 0

oomTest(() => {
    offThreadCompileToStencil(`
        function f(x) {
            class of extends ("ABCDEFGHIJK") {
                test() { return true; };
                static get() {};
                static get() {};
            }
            return 1 + f(x - 1);
        }
        return g("try{}catch(e){}", n);
        `);
    var stencil = finishOffThreadStencil();
    evalStencil(stencil);
});
