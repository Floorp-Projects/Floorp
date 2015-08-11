if (helperThreadCount() == 0)
   quit();

gczeal(0);
gc();

schedulegc(this);
startgc(0, "shrinking");
var g = newGlobal();
g.offThreadCompileScript('debugger;', {});
g.runOffThreadScript();
