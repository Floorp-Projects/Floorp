actual = '';
expected = '10,19,1000028,3000037,6000046,10000055,15000064,21000073,28000082,36000091,45000100,';

function f() {
  var x = 10;
  
  var g = function(p) {
    for (var i = 0; i < 10; ++i)
      x = p + i;
  }
  
  for (var i = 0; i < 10; ++i) {
    appendToActual(x);
    g(1000000 * i + x);
  }

  appendToActual(x);
}

f();


assertEq(actual, expected)
