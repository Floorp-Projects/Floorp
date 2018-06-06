if (helperThreadCount() === 0)
    quit();
evalInWorker(`
    let m = parseModule("import.meta;");
    m.declarationInstantiation();
    m.evaluation();
`);
