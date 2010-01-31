actual = '';
expected = '150,';

var g = 0;

function h(args) {
  var ans = 0;
  for (var i = 0; i < 5; ++i) {
    ans += args[i];
  }
  g = ans;
}

function f() {
  h(arguments);
}

for (var i = 0; i < 5; ++i) {
  f(10, 20, 30, 40, 50);
}
appendToActual(g);


assertEq(actual, expected)
