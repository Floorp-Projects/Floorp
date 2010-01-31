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

  for (k = 0; k < 5; ++k) {
    appendToActual(loop(g));
  }
}

f();


assertEq(actual, expected)
