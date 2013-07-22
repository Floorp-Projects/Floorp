load(libdir + "parallelarray-helpers.js")

function test() {
  var pa1 = Array.buildPar(256, function (x) {
    return Array.buildPar(256, function(y) { return x*1000 + y; });
  });

  for (var x = 0; x < 256; x++) {
    var pax = pa1[x];
    for (var y = 0; y < 256; y++) {
      assertEq(pax[y], x * 1000 + y);
    }
  }
}

if (getBuildConfiguration().parallelJS)
  test();
