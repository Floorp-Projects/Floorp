if (!('gczeal' in this))
    quit();
if (helperThreadCount() == 0)
    quit();

gczeal(15,1);
setGCCallback({
  action: "majorGC",
});
gcslice(3);
var lfGlobal = newGlobal();
lfGlobal.offThreadCompileScript("");

