if (!getBuildConfiguration().parallelJS)
  quit(0);

function testReduce() {
  function sum(a, b) {
    var r = a + b;
  }
  var array = build(8 * 0X0aaec , function() { return 1; });
  var parResult = array.reducePar(sum);
}
for (var ix = 0; ix < 3; ++ix) {
  testReduce();
}
function build(n, f) {
  var result = [];
  for (var i = 0; i < n; i++)
    result.push(f(i));
  return result;
}
function seq_scan(array, f) {
  for (var i = 1; i < array.length; i++) {
  }
}
function assertAlmostEq(v1, v2) {
    if (e1 instanceof Array && e2 instanceof Array) {
      for (prop in e1) {
        if (e1.hasOwnProperty(prop)) {        }
      }
    }
}
function assertEqArray(a, b) {
    for (var i = 0, l = a.length; i < l; i++) {
      try {      } catch (e) {      }
    }
}
function assertParallelExecWillRecover(opFunction) {
assertParallelExecSucceeds(
    function(m) {},
    function(r) {}
);
}
