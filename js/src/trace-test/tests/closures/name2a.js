actual = '';
expected = '0,1,2,3,4,';

function loop(f) {
  var p;
  for (var i = 0; i < 10; ++i) {
    p = f(1, 2, 3);
  }
  return p;
}

function f(j, k) {
  var g = function(a, b, c) { return k; }

  for (k = 0; k < 5; ++k) {
    appendToActual(loop(g));
  }
}

f();


assertEq(actual, expected)
