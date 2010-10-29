// getprop, self, 3 shapes

var expected = "22,303,1001,22,303,1001,22,303,";
var actual = '';

function f() {
  var o1 = { a: 11, b: 22, c: 33 };
  var o2 = { x: 101, y: 202, b: 303 };
  var o3 = { b: 1001, x: 2002, y: 3003 };
  var oa = [ o1, o2, o3 ];

  for (var i = 0; i < 8; ++i) {
    actual += oa[i%3].b + ',';
  }
}

f();

assertEq(actual, expected);
