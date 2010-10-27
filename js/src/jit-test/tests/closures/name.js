actual = '';
expected = '2,5,';

function loop(f) {
  var p;
  for (var i = 0; i < 10; ++i) {
    p = f();
  }
  return p;
}

function f(i, k) {
  var g = function() { return k; }

  k = 2;
  appendToActual(loop(g));
  k = 5;
  appendToActual(loop(g));
}

f(0, 0);
	


assertEq(actual, expected)
