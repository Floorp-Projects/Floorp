actual = '';
expected = '0,0,2,2,4,4,6,6,8,8,';

function g(b) {
  for (var i = 0; i < 10; ++i) {
  }
}

function f(xa_arg) {
  var xa = xa_arg;
  for (var i = 0; i < 5; ++i) {
    var j = i + xa[i];
    appendToActual(j);
    g();
    appendToActual(j);
  }
}

f([ 0, 1, 2, 3, 4 ]);


assertEq(actual, expected)
