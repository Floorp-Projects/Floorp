// |jit-test| skip-if: helperThreadCount() === 0 || !('oomTest' in this)
options('strict');
evaluate(`
    oomTest(() => {
        offThreadCompileScript("");
    });`);
