if (helperThreadCount() === 0)
    quit();
gczeal(10);
newGlobal();
offThreadCompileScript("let x = 1;");
abortgc();
