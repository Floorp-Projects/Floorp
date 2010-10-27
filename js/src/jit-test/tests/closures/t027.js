actual = '';
expected = '99,';

function looper(f) {
  for (var i = 0; i < 10; ++i) {
    for (var j = 0; j < 10; ++j) {
      f(10*i + j);
    }
  }
}

function tester() {
  var x = 1;
  looper(function(y) { x = y; });
  return x;
}

appendToActual(tester());


assertEq(actual, expected)
