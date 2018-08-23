if (helperThreadCount() === 0)
    quit();
evalInWorker(`
    let m = parseModule("import.meta;");
    instantiateModule(m);
    evaluateModule(m);
`);
