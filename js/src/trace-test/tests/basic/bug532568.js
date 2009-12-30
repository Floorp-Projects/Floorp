var actual = "";

function f() {
  var x = 10;
  
  var g = function(p) {
    for (var i = 0; i < 10; ++i)
      x = p + i;
  }
  
  for (var i = 0; i < 10; ++i) {
    g(100 * i + x);
    actual += x + ',';
  }
}

f();

assertEq(actual, "19,128,337,646,1055,1564,2173,2882,3691,4600,");
