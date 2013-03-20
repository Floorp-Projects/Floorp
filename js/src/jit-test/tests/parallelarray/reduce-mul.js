load(libdir + "parallelarray-helpers.js");

function testReduce() {
  // This test is interesting because during warmup v*p remains an
  // integer but this ceases to be true once real execution proceeds.
  // By the end, it will just be infinity.
  function mul(v, p) { return v*p; }

  var array = range(1, 513);
  var expected = array.reduce(mul);
  var parray = new ParallelArray(array);
  var modes = ["seq", "par"];
  for (var i = 0; i < 2; i++) {
    assertAlmostEq(expected, parray.reduce(mul, {mode: modes[i], expect: "success"}));
  }
  // assertParallelArrayModesEq(["seq", "par"], expected, function(m) {
  //   d = parray.reduce(sum, m);
  //   return d;
  // });
}

if (getBuildConfiguration().parallelJS) testReduce();
