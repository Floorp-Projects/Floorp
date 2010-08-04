actual = '';
expected = '6,';

// tracing length

var g = 0;

function h(args) {
  g = args.length;
}

function f() {
  h(arguments);
}

for (var i = 0; i < 5; ++i) {
  f(10, 20, 30, 40, 50, 60);
}
appendToActual(g);


assertEq(actual, expected)
