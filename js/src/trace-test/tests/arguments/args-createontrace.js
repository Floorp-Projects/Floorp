actual = '';
expected = '5,4,3,2,1,X,5,4,3,2,1,Y,5,4,3,2,1,';

function f() {
  for (var i = 0; i < 5; ++i) {
    var args = arguments;
    appendToActual(args[i]);
  }
}

f(5, 4, 3, 2, 1);
appendToActual("X");
f(5, 4, 3, 2, 1);
appendToActual("Y");
f(5, 4, 3, 2, 1);


assertEq(actual, expected)
