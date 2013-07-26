load(libdir + "parallelarray-helpers.js");

function buildSimple() {
    assertParallelModesCommute(["seq", "par"], function(m) {
        return Array.buildPar(256, function(i) {
            return { x: i, y: i + 1, z: i + 2 };
        }, m);
    });
}

if (getBuildConfiguration().parallelJS)
  buildSimple();
