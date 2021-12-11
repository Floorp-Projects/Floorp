function foo(x, n) {
  for (var i = 0; i < n; i++)
    x[i] = i;
  var q = 0;
  for (var i = 0; i < 10; i++) {
    for (var j = 0; j < n; j++)
      q += x[j];
  }
  return q;
}

var a = foo([], 100);
assertEq(a, 49500);

function basic1(x) {
  var q = 0;
  for (var i = 0; i < 4; i++)
    q += x[i];
  return q;
}

var b = basic1([1,2,3,4]);
assertEq(b, 10);

ARRAY = [1,2,3,4];

function basic2() {
  var q = 0;
  for (var i = 0; i < 4; i++)
    q += ARRAY[i];
  return q;
}

var c = basic2();
assertEq(c, 10);
