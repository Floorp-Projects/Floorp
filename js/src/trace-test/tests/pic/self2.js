// getprop, self, 2 shapes

var expected = "22,303,22,303,22,303,22,303,";
var actual = '';

function f() {
  var o1 = { a: 11, b: 22, c: 33 };
  var o2 = { x: 101, y: 202, b: 303 };
  var oa = [ o1, o2 ];

  for (var i = 0; i < 8; ++i) {
    actual += oa[i%2].b + ',';
  }
}

f();

assertEq(actual, expected);
