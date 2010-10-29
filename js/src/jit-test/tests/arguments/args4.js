actual = '';
expected = '51,';

var g = 0;

function h(args) {
  g = args[1];
}

function f() {
  h(arguments);
}

for (var i = 0; i < 10000; ++i) {
  f(100, 51);
}

appendToActual(g);


assertEq(actual, expected)
