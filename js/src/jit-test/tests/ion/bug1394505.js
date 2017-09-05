if (helperThreadCount() === 0)
    quit();
if (!('oomTest' in this))
    quit();

for (let j = 0; j < 50; j++) {
    if (j === 1)
        oomTest(function() {});
    evalInWorker(`
        for (let i = 0; i < 30; i++)
            relazifyFunctions();
    `);
}
