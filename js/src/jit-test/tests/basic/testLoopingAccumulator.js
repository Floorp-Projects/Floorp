function addAccumulations(f) {
  var a = f();
  var b = f();
  return a() + b();
}

function loopingAccumulator() {
  var x = 0;
  return function () {
    for (var i = 0; i < 10; ++i) {
      ++x;
    }
    return x;
  }
}

function testLoopingAccumulator() {
    var x = addAccumulations(loopingAccumulator);
    return x;
}
assertEq(testLoopingAccumulator(), 20);
