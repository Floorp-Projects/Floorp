// length, various types

var expected = "4,5,44,5,44,4,44,4,5,4,5,44,5,44,4,44,4,5,";
var actual = '';

function f() {
  var a = [ "abcd", [1, 2, 3, 4, 5], { length: 44 } ];

  for (var i = 0; i < 6; ++i) {
    // Use 3 PICs so we start out with each type in one PIC.
    var i1 = i % 3;
    var i2 = (i+1) % 3;
    var i3 = (i+2) % 3;
    actual += a[i1].length + ',';
    actual += a[i2].length + ',';
    actual += a[i3].length + ',';
  }
}

f();

assertEq(actual, expected);
