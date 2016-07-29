if (!("startgc" in this &&
      "offThreadCompileScript" in this &&
      "runOffThreadScript" in this))
{
    quit();
}

if (helperThreadCount() == 0)
    quit();

gczeal(0);
gc();

// Start an incremental GC that includes the atoms zone
startgc(0);
var g = newGlobal();

// Start an off thread compilation that will not run until GC has finished
if ("gcstate" in this)
   assertEq(gcstate(), "Mark");
g.offThreadCompileScript('23;', {});

// Wait for the compilation to finish, which must finish the GC first
assertEq(23, g.runOffThreadScript());
if ("gcstate" in this)
   assertEq(gcstate(), "NotActive");

print("done");
