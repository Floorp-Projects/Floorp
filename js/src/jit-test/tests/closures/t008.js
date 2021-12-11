actual = '';
expected = '900,';

function k(f_arg) { 
  for (var i = 0; i < 10; ++i) {
    f_arg(i);
  }
}

function t() {
  var x = 1;
  k(function (i) { x = i; });
  return x;
}

var ans = 0;
for (var j = 0; j < 10; ++j) {
  for (var i = 0; i < 10; ++i) {
    ans += t();
  }
}
appendToActual(ans);



assertEq(actual, expected)
