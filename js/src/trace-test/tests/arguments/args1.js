actual = '';
expected = 'a,';

function f() {
  arguments;
}

for (var i = 0; i < 1000; ++i) {
  f(1, 2);
}

appendToActual('a')


assertEq(actual, expected)
