if (helperThreadCount() === 0)
    quit();
gczeal(4);
offThreadCompileScript("let x = 1;");
