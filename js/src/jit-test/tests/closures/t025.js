actual = '';
expected = '2,5,';

function loop(f) {
  var p;
  for (var i = 0; i < 1000; ++i) {
    p = f();
  }
  return p;
}

function f(k) {
  function g() {
    return k;
  }

  k = 2;
  appendToActual(loop(g));
  k = 5;
  appendToActual(loop(g));
}

f();


assertEq(actual, expected)
