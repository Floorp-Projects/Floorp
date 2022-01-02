function multiple(a) {
  if (a > 10)
    return a * 20;
  return 0;
}

function deeper(a, b) {
  return multiple(a + b);
}

function foo() {
  var a = 0;
  for (var i = 0; i < 10; i++)
    a += deeper(0x7ffffff0, i);
  return a;
}

var q = foo();
assertEq(q, 429496727300);
