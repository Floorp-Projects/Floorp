actual = '';
expected = '10.5,19.5,128.5,337.5,646.5,1055.5,1564.5,2173.5,2882.5,3691.5,4600.5,';

function f() {
  var x = 10.5;
  
  var g = function(p) {
    for (var i = 0; i < 10; ++i)
      x = p + i;
  }
  
  for (var i = 0; i < 10; ++i) {
    appendToActual(x);
    g(100 * i + x);
  }

  appendToActual(x);
}

f();


assertEq(actual, expected)
