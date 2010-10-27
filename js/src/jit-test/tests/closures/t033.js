actual = '';
expected = '2,';

function k(f_arg) { 
  for (var i = 0; i < 100; ++i) {
    f_arg();
  }
}

function t() {
  var x = 1;
  k(function () { x = 2; });
  appendToActual(x);
}

t();


assertEq(actual, expected)
