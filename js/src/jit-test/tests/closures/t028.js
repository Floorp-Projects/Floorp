actual = '';
expected = 'undefined,';

function looper(f) {
  for (var i = 0; i < 10; ++i) {
    for (var j = 0; j < 10; ++j) {
      f();
    }
  }
}

function tester() {
  var x = 1;
  function f() {
    return x;
  }
  looper(f);
}

appendToActual(tester());


assertEq(actual, expected)
