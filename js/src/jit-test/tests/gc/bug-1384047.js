if (!('oomTest' in this))
    quit();

newGlobal();
evalcx("oomTest(newGlobal);", newGlobal());
