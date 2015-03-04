if (helperThreadCount() == 0)
   quit();

schedulegc(this);
startgc(0, "shrinking");
var g = newGlobal();
g.offThreadCompileScript('debugger;', {});
g.runOffThreadScript();
