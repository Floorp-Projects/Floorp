// getprop, proto and self, 3 shapes

var expected = "22,202,99;202,99,22;99,22,202;22,202,99;202,99,22;99,22,202;22,202,99;202,99,22;99,22,202;";
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
  o3.b = 99;
  var oa = [ o1, o2, o3 ];

  for (var i = 0; i < 9; ++i) {
    // Use 3 PICs so we start out with each type in one PIC.
    var i1 = i % 3;
    var i2 = (i+1) % 3;
    var i3 = (i+2) % 3;

    actual += oa[i1].b + ',';
    actual += oa[i2].b + ',';
    actual += oa[i3].b + ';';
  }
}

f();

assertEq(actual, expected);
