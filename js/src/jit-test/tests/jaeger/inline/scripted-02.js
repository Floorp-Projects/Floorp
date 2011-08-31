function getter(a, i) {
  return a[i];
}

function foo(a, n) {
  var res = 0;
  for (var i = 0; i < 10; i++) {
    res = 0;
    for (var j = 0; j < n; j++) {
      res += getter(a, j);
    }
  }
  return res;
}

var n = 100;
var a = Array(n);
for (var i = 0; i < n; i++)
  a[i] = i;

var q = foo(a, n);
assertEq(q, 4950);
