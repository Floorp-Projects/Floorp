if (!("startgc" in this &&
      "offThreadCompileScript" in this &&
      "runOffThreadScript" in this))
{
    quit();
}

if (helperThreadCount() == 0)
    quit();

if ("gczeal" in this)
   gczeal(0);

// Start an incremental GC that includes the atoms zone
startgc(0);
var g = newGlobal();

// Start an off thread compilation that will not run until GC has finished
if ("gcstate" in this)
   assertEq("mark", gcstate());
g.offThreadCompileScript('23;', {});

// Wait for the compilation to finish, which must finish the GC first
assertEq(23, g.runOffThreadScript());
if ("gcstate" in this)
   assertEq("none", gcstate());

print("done");
