actual = '';
expected = '2,4,8,16,32,undefined,64,128,256,512,1024,undefined,2048,4096,8192,16384,32768,undefined,65536,131072,262144,524288,1048576,undefined,2097152,4194304,8388608,16777216,33554432,undefined,';

var f = function() {
  var p = 1;
  
  function g() {
    for (var i = 0; i < 5; ++i) {
      p = p * 2;
      appendToActual(p);
    }
  }
  
  return g;
}

var g = f();
for (var i = 0; i < 5; ++i) {
  g();
  appendToActual();
}



assertEq(actual, expected)
