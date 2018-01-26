if (helperThreadCount() === 0)
    quit();

offThreadCompileScript("");
evalInCooperativeThread("");
runOffThreadScript();
