actual = '';
expected = '1 2,1 2,1 2,1 2,1 2,1 2,1 2,1 2,1 undefined,1 undefined,1 undefined,1 undefined,1 undefined,1 undefined,1 undefined,1 undefined,';

function g(a, b) {
  appendToActual(a + ' ' + b);
}

function f() {
  for (var i = 0; i < 8; ++i) {
    g.apply(this, arguments);
  }
}

f(1, 2);
f(1);


assertEq(actual, expected)
