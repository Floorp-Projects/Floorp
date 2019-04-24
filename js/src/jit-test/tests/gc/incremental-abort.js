// |jit-test| skip-if: !getBuildConfiguration()['has-gczeal']

// Test aborting an incremental GC in all possible states

gczeal(0);
gc();

function testAbort(zoneCount, objectCount, sliceCount, abortState)
{
    // Allocate objectCount objects in zoneCount zones and run a incremental
    // shrinking GC.

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
    startgc(sliceCount, "shrinking");
    while (gcstate() !== "NotActive") {
        var state = gcstate();
        if (state == abortState) {
            abortgc();
            didAbort = true;
            break;
        }

        gcslice(sliceCount);
    }

    assertEq(gcstate(), "NotActive");
    if (abortState)
        assertEq(didAbort, true);

    return zones;
}

gczeal(0);
testAbort(10, 10000, 10000);
testAbort(10, 10000, 10000, "Mark");
testAbort(10, 10000, 10000, "Sweep");
testAbort(10, 10000, 10000, "Compact");
// Note: we do not yield automatically before Finalize or Decommit, as they yield internally.
// Thus, we may not witness an incremental state in this phase and cannot test it explicitly.
