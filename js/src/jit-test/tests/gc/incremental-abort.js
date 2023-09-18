// |jit-test| skip-if: !getBuildConfiguration("has-gczeal") || (getBuildConfiguration("osx") && getBuildConfiguration("arm64"))

// Test aborting an incremental GC in all possible states

gczeal(0);
gc();

// Allocate objectCount objects in zoneCount zones and run a incremental
// shrinking GC with slices with a work budget of sliceBudget until we reach
// GC state abortState at which point, abort the GC.
function testAbort(zoneCount, objectCount, sliceBudget, abortState)
{

  var zones = [];
  for (var i = 0; i < zoneCount; i++) {
    var zone = newGlobal({newCompartment: true});
    evaluate("var objects; " +
             "function makeObjectGraph(objectCount) { " +
             "    objects = []; " +
             "    for (var i = 0; i < objectCount; i++) " +
             "        objects.push({i: i}); " +
             "}",
             { global: zone });
    zone.makeObjectGraph(objectCount);
    zones.push(zone);
  }

  gc();

  var didAbort = false;
  startgc(sliceBudget, "shrinking");
  assertEq(currentgc().isShrinking, true);
  while (gcstate() !== "NotActive") {
    if (gcstate() == abortState) {
      abortgc();
      didAbort = true;
      break;
    }

    gcslice(sliceBudget);
  }

  assertEq(gcstate(), "NotActive");
  if (abortState)
    assertEq(didAbort, true);

  return zones;
}

gczeal(0);
testAbort(10, 10000, 10000);
testAbort(10, 10000, 10000, "Mark");
testAbort(10, 10000, 1000, "Sweep");
testAbort(10, 10000, 10000, "Compact");
// Note: we do not yield automatically before Finalize or Decommit, as they
// yield internally.  Thus, we may not witness an incremental state in this
// phase and cannot test it explicitly.
