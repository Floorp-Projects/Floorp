if (helperThreadCount() === 0)
    quit();

options('strict');
evaluate(`
    oomTest(() => {
        offThreadCompileScript("");
    });`);
