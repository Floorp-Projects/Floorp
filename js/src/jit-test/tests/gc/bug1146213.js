if (!("setGCCallback" in this &&
      "schedulegc" in this &&
      "gcslice" in this &&
      "newGlobal" in this &&
      "helperThreadCount" in this))
{
    quit();
}

if (helperThreadCount() == 0)
    quit();

setGCCallback({
  action: "majorGC",
});
schedulegc(this)
gcslice(3)
var lfGlobal = newGlobal();
lfGlobal.offThreadCompileScript("");
lfGlobal.runOffThreadScript();
