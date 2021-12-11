if (helperThreadCount() == 0)
    quit();

ignoreUnhandledRejections();

gczeal(9);
function rejectionTracker(state) {}
setPromiseRejectionTrackerCallback(rejectionTracker)
  lfGlobal = newGlobal();
offThreadCompileScript(`new Promise(()=>rej)`);
lfGlobal.runOffThreadScript();
for (lfLocal in this);
