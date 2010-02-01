actual = '';
expected = '4,4,4,';

function k(f_arg) { 
  for (var i = 0; i < 5; ++i) {
    f_arg(i);
  }
}

function t() {
  var x = 1;
  k(function (i) { x = i; });
  appendToActual(x);
}

t();
t();
t();


assertEq(actual, expected)
