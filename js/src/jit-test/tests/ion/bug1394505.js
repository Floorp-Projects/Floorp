// |jit-test| skip-if: helperThreadCount() === 0 || !('oomTest' in this)

for (let j = 0; j < 50; j++) {
    if (j === 1)
        oomTest(function() {});
    evalInWorker(`
        for (let i = 0; i < 30; i++)
            relazifyFunctions();
    `);
}
