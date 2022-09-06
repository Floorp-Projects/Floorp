if (helperThreadCount() == 0)
    quit();

ignoreUnhandledRejections();

gczeal(9);
function rejectionTracker(state) {}
setPromiseRejectionTrackerCallback(rejectionTracker)
  lfGlobal = newGlobal();
offThreadCompileToStencil(`new Promise(()=>rej)`);
var stencil = lfGlobal.finishOffThreadStencil();
lfGlobal.evalStencil(stencil);
for (lfLocal in this);
