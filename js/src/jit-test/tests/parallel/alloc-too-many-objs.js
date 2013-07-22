load(libdir + "parallelarray-helpers.js");

function testMap() {

  // Note: This is the same kernel function as `alloc-many-objs`, but
  // with a larger bound.  This often fails par. exec. because it
  // triggers GC at inconvenient times.  But let's just test that it
  // doesn't crash or something!

  var ints = range(0, 100000);

  // The disqual occurs because each time we try to run we wind up
  // bailing due to running out of memory or requesting a GC.
  assertParallelExecWillBail(function (m) {
    ints.mapPar(kernel, m);
  });

  function kernel(v) {
    var x = [];

    if (inParallelSection()) {
      // don't bother to stress test the non-parallel paths!
      for (var i = 0; i < 50; i++) {
        x[i] = [];
        for (var j = 0; j < 1024; j++) {
          x[i][j] = j;
        }
      }
    }

    return x;
  }
}

if (getBuildConfiguration().parallelJS)
  testMap();

