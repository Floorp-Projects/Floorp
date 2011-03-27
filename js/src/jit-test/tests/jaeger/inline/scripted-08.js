function first(a, b) {
  return second(a, b);
}

function second(a, b) {
  return third(a, b, a + b);
}

function third(a, b, c) {
  return a + b + c;
}

function foo(x) {
  var a = 0;
  for (var i = 0; i < 100; i++)
    a += first(x, i);
  return a;
}

var q = foo(10);
assertEq(q, 11900);
