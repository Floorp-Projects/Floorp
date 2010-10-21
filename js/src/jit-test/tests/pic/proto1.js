// getprop, proto, 1 shape

var expected = "11,22,33,11,22,33,11,22,33,11,22,33,11,22,33,";
var actual = '';

var proto = { a: 11, b: 22, c: 33 };

function B() {
}
B.prototype = proto;

function f() {
  var o = new B();
  
  for (var i = 0; i < 5; ++i) {
    actual += o.a + ',';
    actual += o.b + ',';
    actual += o.c + ',';
  }
}

f();

assertEq(actual, expected);
