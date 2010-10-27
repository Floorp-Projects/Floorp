actual = '';
expected = '0,0,0,0,0,0,0,1,1,1,1,1,1,1,2,2,2,2,2,2,2,3,3,3,3,3,3,3,4,4,4,4,4,4,4,';

function g(a) {
  a();
}

function f(y) {
  for (var i = 0; i < 7; ++i) {
    var q;
    q = function() { appendToActual(y); };
    g(q);
  }
}

for (var i = 0; i < 5; ++i) {
  f(i);
 }


assertEq(actual, expected)
