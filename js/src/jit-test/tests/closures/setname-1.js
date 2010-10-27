actual = '';
expected = '4,4,4,4,4,';

function f() {
  var k = 0;

  function g() {
    for (var i = 0; i < 5; ++i) {
      k = i;
    }
  }

  function h() {
    for (var i = 0; i < 5; ++i) {
      appendToActual(k);
    }
  }

  g();
  h();
}

f();


assertEq(actual, expected)
