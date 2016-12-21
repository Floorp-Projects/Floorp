if (helperThreadCount() == 0)
    quit();
startgc(8301);
offThreadCompileScript("(({a,b,c}))");
gcparam("maxBytes", gcparam("gcBytes"));
