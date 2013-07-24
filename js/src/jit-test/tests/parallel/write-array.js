load(libdir + "parallelarray-helpers.js");

function buildSimple() {
  assertParallelModesCommute(["seq", "par"], function(m) {
    return Array.buildPar(256, function(i) {
      let obj = [0, 1, 2];
      obj[0] += 1;
      obj[1] += 1;
      obj[2] += 1;
      return obj;
    }, m);
  });
}

if (getBuildConfiguration().parallelJS)
  buildSimple();
