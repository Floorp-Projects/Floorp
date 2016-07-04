if (helperThreadCount() === 0)
    quit(0);
startgc(45);
offThreadCompileScript("print(1)");
