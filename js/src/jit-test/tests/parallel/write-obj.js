load(libdir + "parallelarray-helpers.js");

function buildSimple() {
  assertParallelModesCommute(["seq", "par"], function(m) {
    Array.buildPar(256, function(i) {
      let obj = { x: i, y: i + 1, z: i + 2 };
      obj.x += 1;
      obj.y += 1;
      obj.z += 1;
      return obj;
    }, m);
  });
}

if (getBuildConfiguration().parallelJS)
  buildSimple();
