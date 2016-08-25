if (helperThreadCount() === 0)
    quit(0);
offThreadCompileScript(``);
evalInWorker(`runOffThreadScript()`);
