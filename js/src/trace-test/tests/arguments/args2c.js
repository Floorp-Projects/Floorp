actual = '';
expected = 'g 0 0,g 1 2,g 2 4,g 3 6,g 4 8,';

function g(x, y) {
  appendToActual('g ' + x + ' ' + y);
  //appendToActual('g ' + x + ' ' + y);
  //appendToActual('g ' + y);
}

function f() {
  g.apply(this, arguments);
}

for (var i = 0; i < 5; ++i) {
  f(i, i*2);
}


assertEq(actual, expected)
