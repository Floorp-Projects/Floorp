actual = '';
expected = '0,1,0,1,0,1,';

function f() {
  for (var i = 0; i < arguments.length; ++i) {
    appendToActual(i);
  }
}

f(1, 2);
f(1, 2);
f(2, 2);


assertEq(actual, expected)
