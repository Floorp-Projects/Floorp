actual = '';
expected = '99,';

function g(p) {
  appendToActual(p());
}

function d(k) {
  return function() { return k; }
}

function f(k) {
  var p;
  
  for (var i = 0; i < 1000; ++i) {
    p = d(k);
  }

  g(p);
}

f(99);


assertEq(actual, expected)
