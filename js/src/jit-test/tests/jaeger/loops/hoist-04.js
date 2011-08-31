function bar(x, i) {
  if (i == 50)
    x.length = 0;
}

function foo(x, j, n) {
  var a = 0;
  for (var i = 0; i < n; i++) {
    var q = x[j];
    bar(x, i);
    if (typeof q == 'undefined')
      a++;
  }
  return a;
}

var a = foo([1,2,3,4], 3, 100);
assertEq(a, 49);
