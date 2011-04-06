function foo(x, j, n) {
  var a = 0;
  for (var i = 0; i < n; i++)
    a += x[j];
  return a;
}

var a = foo([1,2,3,4], 3, 100);
assertEq(a, 400);

var b = foo([1,2,3,4], 5, 100);
assertEq(b, NaN);
