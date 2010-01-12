actual = '';
expected = '5,';

function f() {
  var p = 0;

  function g() {
    for (var i = 0; i < 5; ++i) {
      p++;
    }
  }

  g();

  appendToActual(p);
}

f();


assertEq(actual, expected)
