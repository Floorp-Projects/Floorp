// |jit-test| slow;

load(libdir + "parallelarray-helpers.js")

function buildComprehension() {
  var pa1 = Array.buildPar(256, function (idx) { return idx; });
  for (var i = 0; i < 20000; i++) {
    print(i);
    buildAndCompare();
  }

  function buildAndCompare() {
    // this will be ion-generated:
    var pa2 = Array.buildPar(256, function (idx) { return idx; });
    assertStructuralEq(pa1, pa2);
  }
}

if (getBuildConfiguration().parallelJS)
  buildComprehension();
