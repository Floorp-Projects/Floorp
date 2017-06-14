if (helperThreadCount() === 0)
    quit();
offThreadCompileScript("");
startgc(0);
runOffThreadScript();
