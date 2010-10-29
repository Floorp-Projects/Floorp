actual = '';
expected = '5,4,3,2,1,';

function f() {
  while (arguments.length > 0) {
    appendToActual(arguments[arguments.length-1]);
    arguments.length--;
  }
}

f(1, 2, 3, 4, 5);


assertEq(actual, expected)
