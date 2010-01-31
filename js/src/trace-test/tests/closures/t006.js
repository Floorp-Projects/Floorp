actual = '';
expected = '2,';

function k(f_arg) { 
  (function() {
    for (var i = 0; i < 10; ++i) {
      f_arg();
    }
  })();
}

function t() {
  var x = 1;
  k(function () { x = 2; });
  appendToActual(x);
}

t();



assertEq(actual, expected)
