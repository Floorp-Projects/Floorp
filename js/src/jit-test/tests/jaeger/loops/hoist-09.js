
function foo(x, y) {
  for (var i = 0; i < x.length; i++) {
    x[i];
    if (i < 20)
      y[i + 1] = 0;
  }
}

var q = Array(1,2,3,4,5);
foo(q, []);
