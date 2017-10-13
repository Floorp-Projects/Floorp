if (helperThreadCount() === 0)
    quit();

evalInCooperativeThread(`
    (new Promise(function(resolve, reject) { resolve(); })).then(() => {});

    drainJobQueue();
    `);
