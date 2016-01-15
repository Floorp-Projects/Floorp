if (!('oomTest' in this))
    quit();

oomTest(() => evalInWorker("1"));
