actual = '';
expected = '0,1,2,3,4,';

function g(a) {
}

function f(y) {
  var q;
  q = function() { appendToActual(y); };
  q();
}

for (var i = 0; i < 5; ++i) {
  f(i);
 }


assertEq(actual, expected)
