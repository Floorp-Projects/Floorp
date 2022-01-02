actual = '';
expected = 'undefined,undefined,undefined,undefined,undefined,';

function h() {
  for (var i = 0; i < 5; ++i) {
    var a = arguments;
    appendToActual(a[100]);
  }
}

h();


assertEq(actual, expected)
