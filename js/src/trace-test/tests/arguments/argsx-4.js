actual = '';
expected = '[object Object] undefined undefined,[object Object] undefined undefined,';

function f() {
  g(arguments);
}

function g(a, b, c) {
  h(arguments);
  a = 1;
  b = 2;
  c = 3;
  h(arguments);
}

function h(a, b, c) {
  appendToActual(a + ' ' + b + ' ' + c);
}

f(4, 5, 6);


assertEq(actual, expected)
