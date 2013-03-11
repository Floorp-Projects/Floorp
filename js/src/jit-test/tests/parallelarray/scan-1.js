
function testScan() {
  function f(v, p) { return v*p; }
  var a = [1,2,3,4,5];
  var p = new ParallelArray(a);
  var s = p.scan(f);
  for (var i = 0; i < p.length; i++) {
    var p2 = new ParallelArray(a.slice(0, i+1));
    assertEq(s[i], p2.reduce(f));
  }
}

testScan();
