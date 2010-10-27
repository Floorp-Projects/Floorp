// getprop, proto, 3 shapes

var expected = "22,202,202,22,202,202,22,202,202,";
var actual = '';

var protoB = { a: 11, b: 22, c: 33 };

function B() {
}
B.prototype = protoB;

var protoC = { a: 101, b: 202, c: 303 };

function C() {
}
C.prototype = protoC;

function f() {
  var o1 = new B();
  var o2 = new C();
  var o3 = new C();
  o3.q = 99;
  var oa = [ o1, o2, o3 ];

  for (var i = 0; i < 9; ++i) {
    actual += oa[i%3].b + ',';
  }
}

f();

assertEq(actual, expected);
