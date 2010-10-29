// Test stack reconstruction after a nested exit
function testNestedExitStackInner(j, counter) {
  ++counter;
  var b = 0;
  for (var i = 1; i <= RUNLOOP; i++) {
    ++b;
    var a;
    // Make sure that once everything has been traced we suddenly switch to
    // a different control flow the first time we run the outermost tree,
    // triggering a side exit.
    if (j < RUNLOOP)
      a = 1;
    else
      a = 0;
    ++b;
    b += a;
  }
  return counter + b;
}
function testNestedExitStackOuter() {
  var counter = 0;
  for (var j = 1; j <= RUNLOOP; ++j) {
    for (var k = 1; k <= RUNLOOP; ++k) {
      counter = testNestedExitStackInner(j, counter);
    }
  }
  return counter;
}
//assertEq(testNestedExitStackOuter(), 81);
