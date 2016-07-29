// Exercise incremental compacting GC
// Run with MOZ_GCTIMER to see the timings

if (!("gcstate" in this && "gczeal" in this))
    quit();

gczeal(0);

function testCompacting(zoneCount, objectCount, sliceCount)
{
    // Allocate objectCount objects in zoneCount zones
    // On linux64 debug builds we will move them all
    // Run compacting GC with multiple slices

    var zones = [];
    for (var i = 0; i < zoneCount; i++) {
        var zone = newGlobal();
        evaluate("var objects; " +
                 "function makeObjectGraph(objectCount) { " +
                 "    objects = []; " +
                 "    for (var i = 0; i < objectCount; i++) " +
                 "        objects.push({ serial: i }); " +
                "}",
                 { global: zone });
        zone.makeObjectGraph(objectCount);
        zones.push(zone);
    }

    // Finish any alloc-triggered incremental GC
    if (gcstate() !== "NotActive")
        gc();

    startgc(sliceCount, "shrinking");
    while (gcstate() !== "NotActive") {
        gcslice(sliceCount);
    }

    return zones;
}

testCompacting(1, 100000, 100000);
testCompacting(2, 100000, 100000);
testCompacting(4, 50000,  100000);
testCompacting(2, 100000, 50000);
