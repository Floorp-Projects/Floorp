actual = '';
expected = '';

function k(f_arg) { 
  for (var i = 0; i < 10; ++i) {
    f_arg(i);
  }
}

function t() {
  var x = 1;
  k(function (i) { x = i; });
}

for (var i = 0; i < 100; ++i) {
  t();
}


assertEq(actual, expected)
