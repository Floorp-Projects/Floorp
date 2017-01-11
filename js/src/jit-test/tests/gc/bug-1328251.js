if (helperThreadCount() == 0)
    quit();

evalInWorker(`
    if (!('gczeal' in this))
        quit();
    gczeal(2);
    for (let i = 0; i < 30; i++) {
        var a = [1, 2, 3];
        a.indexOf(1);
        relazifyFunctions(); }
`);
