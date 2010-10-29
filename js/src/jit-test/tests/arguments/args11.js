actual = '';
expected = 'true,true,true,true,true,';

function h() {
  var a = arguments;
  for (var i = 0; i < 5; ++i) {
    appendToActual(a == arguments);
  }
}

h();


assertEq(actual, expected)
