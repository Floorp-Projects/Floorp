
function bar(n) {
  var a;
  if (n < 50)
    a = n;
  return a;
}

function foo() {
  for (var i = 0; i < 100; i++) {
    var q = bar(i);
    if (i < 50)
      assertEq(q, i);
    else
      assertEq(q, undefined);
  }
}

foo();
