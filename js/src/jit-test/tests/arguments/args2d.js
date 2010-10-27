actual = '';
expected = 'g 0 0 0,g 1 2 1,g 2 4 4,g 3 6 9,g 4 8 16,';

function g(x, y, z) {
  appendToActual('g ' + x + ' ' + y + ' ' + z);
}

function f() {
  g.apply(this, arguments);
}

for (var i = 0; i < 5; ++i) {
  f(i, i*2, i*i);
}


assertEq(actual, expected)
