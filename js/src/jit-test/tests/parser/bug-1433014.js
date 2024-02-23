// |jit-test| skip-if: helperThreadCount() === 0
evaluate(`
    oomTest(() => {
        offThreadCompileToStencil("");
    });`);
