actual = '';
expected = '1,';

function createCounter() {
  var i = 0;

  var counter = function() {
    return ++i;
  }

  return counter;
}

function f() {
  var counter;
  for (var i = 0; i < 100; ++i) {
    counter = createCounter();
  }
  appendToActual(counter());
}

f();


assertEq(actual, expected)
