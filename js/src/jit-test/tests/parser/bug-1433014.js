if (helperThreadCount() === 0)
    quit();

if (!('oomTest' in this))
    quit();

options('strict');
evaluate(`
    oomTest(() => {
        offThreadCompileScript("");
    });`);
