// |jit-test| slow;

load(libdir + "parallelarray-helpers.js");

function testMap() {

  // Note: This is the same kernel function as `alloc-many-objs`, but
  // with a larger bound.  This often fails par. exec. because it
  // triggers GC at inconvenient times.  But let's just test that it
  // doesn't crash or something!

  var ints = range(0, 100000);
  var pints = new ParallelArray(ints);

  // The bailout occurs because at some point during execution we will
  // request a GC.  The GC is actually deferred until after execution
  // fails.
  pints.map(kernel, {mode: "par", expect: "bailout"});

  function kernel(v) {
    var x = [];

    for (var i = 0; i < 50; i++) {
      x[i] = [];
      for (var j = 0; j < 1024; j++) {
        x[i][j] = j;
      }
    }

    return x;
  }
}

if (getBuildConfiguration().parallelJS)
  testMap();

