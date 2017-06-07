if (helperThreadCount() == 0)
    quit();
gczeal(0);
startgc(8301);
offThreadCompileScript("(({a,b,c}))");
gcparam("maxBytes", gcparam("gcBytes"));
