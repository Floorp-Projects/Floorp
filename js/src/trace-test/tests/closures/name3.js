actual = '';
expected = '0,1,2,3,4,';

function loop(f) {
  var p;
  for (var i = 0; i < 10; ++i) {
    p = f();
  }
  return p;
}

function f(j, k) {
  var g = function() { return k; }

  k = j;
  appendToActual(loop(g));
}

for (var i = 0; i < 5; ++i) {
  f(i);
}


assertEq(actual, expected)
