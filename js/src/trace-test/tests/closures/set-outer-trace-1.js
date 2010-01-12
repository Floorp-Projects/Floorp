actual = '';
expected = '10,19,128,337,646,1055,1564,2173,2882,3691,4600,';

function f() {
  var x = 10;
  
  var g = function(p) {
    for (var i = 0; i < 10; ++i) {
      x = p + i;
    }
  }
  
  for (var i = 0; i < 10; ++i) {
    appendToActual(x);
    g(100 * i + x);
  }

  appendToActual(x);
}

f();


assertEq(actual, expected)
