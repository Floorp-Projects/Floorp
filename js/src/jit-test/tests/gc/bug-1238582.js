if (!('oomTest' in this))
    quit();

oomTest(() => { let a = [2147483651]; [a[0], a[undefined]].sort(); });
