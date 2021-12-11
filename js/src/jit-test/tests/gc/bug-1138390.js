// |jit-test| skip-if: helperThreadCount() === 0

gczeal(0);
gc();

// Start an incremental GC that includes the atoms zone
startgc(0);
var g = newGlobal();

// Start an off thread compilation that will not run until GC has finished
if ("gcstate" in this)
  assertEq(gcstate() === "NotActive", false);

g.evaluate(`
var entry = cacheEntry("23;");
evaluate(entry, { saveIncrementalBytecode: true });
offThreadDecodeScript(entry);
`);

// Wait for the compilation to finish, which must finish the GC first
assertEq(23, g.evaluate(`runOffThreadDecodedScript()`));
if ("gcstate" in this)
   assertEq(gcstate(), "NotActive");

print("done");
