actual = '';
expected = 'g 0,g 1,g 2,g 3,g 4,';

function g(x) {
  appendToActual('g ' + x);
}

function f() {
  g.apply(this, arguments);
}

for (var i = 0; i < 5; ++i) {
  f(i);
}


assertEq(actual, expected)
