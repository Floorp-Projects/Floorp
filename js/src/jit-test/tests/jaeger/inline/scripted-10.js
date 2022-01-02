function copied(x, y) {
  return x + y;
}

function foo(x) {
  var a = 0;
  for (var i = 0; i < 100; i++)
    a += copied(x, x);
  return a;
}

var q = foo(5);
assertEq(q, 1000);
