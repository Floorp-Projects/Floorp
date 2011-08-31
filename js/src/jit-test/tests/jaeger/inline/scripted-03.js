function choose(x, y, z) {
  return x ? y : z;
}

function foo(x, y, z) {
  var a = 0;
  for (var i = 0; i < 100; i++) {
    a += choose(x, y, z);
  }
  return a;
}

var q = foo(true, 10, 0);
assertEq(q, 1000);
