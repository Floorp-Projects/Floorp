if (helperThreadCount() === 0)
    quit();
gczeal(0);
offThreadCompileScript("");
startgc(0);
runOffThreadScript();
