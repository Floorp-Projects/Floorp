load(libdir + "parallelarray-helpers.js");

function buildComprehension() {
  var H = 96;
  var W = 96;
  var d = 4;
  // 3D 96x96x4 texture-like PA
  var p = new ParallelArray([H,W,d], function (i,j,k) { return i + j + k; });
  var a = [];
  for (var i = 0; i < H; i++) {
    for (var j = 0; j < W; j++) {
      for (var k = 0; k < d; k++) {
        a.push(i+j+k);
      }
    }
  }
  var p2 = new ParallelArray(a).partition(d).partition(W);
  assertEqParallelArray(p, p2);
}

if (getBuildConfiguration().parallelJS)
  buildComprehension();
