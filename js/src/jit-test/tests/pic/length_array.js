// length, string

var expected = "3,6,4,3,6,4,3,6,4,3,6,4,";
var actual = '';

function f() {
  var ss = [ [1, 2, 3], [1, 2, 3, 4, 5, 6], [1, 2, 3, 4] ];

  for (var i = 0; i < 12; ++i) {
    actual += ss[i%3].length + ',';
  }
}

f();

assertEq(actual, expected);
