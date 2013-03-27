load(libdir + "parallelarray-helpers.js");

function buildSimple() {

    assertParallelArrayModesCommute(["seq", "par"], function(m) {
        return new ParallelArray([256], function(i) {
            return { x: i, y: i + 1, z: i + 2 };
        }, m);
    });

}

if (getBuildConfiguration().parallelJS)
  buildSimple();
