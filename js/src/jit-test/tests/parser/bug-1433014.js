// |jit-test| skip-if: helperThreadCount() === 0 || !('oomTest' in this)
evaluate(`
    oomTest(() => {
        offThreadCompileToStencil("");
    });`);
