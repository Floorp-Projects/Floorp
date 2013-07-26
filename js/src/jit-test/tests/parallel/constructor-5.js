// |jit-test| slow;
// ^^ This test is slow when --no-ion is used, specifically,
//    as part of TBPL.

function testCopyBigArray() {
  // Don't crash
  var a = new Array(1000 * 1000);
  var p = new ParallelArray(a);
  // Holes!
  var a = new Array(10000);
  for (var cnt = 0; cnt < a.length; cnt+=2) {
    a[cnt] = cnt;
    var p = new ParallelArray(a);
  }
}

if (getBuildConfiguration().parallelJS)
  testCopyBigArray();
