// |jit-test| skip-if: helperThreadCount() === 0

evalInWorker(`
    let m = parseModule("import.meta;");
    m.declarationInstantiation();
    m.evaluation();
`);
