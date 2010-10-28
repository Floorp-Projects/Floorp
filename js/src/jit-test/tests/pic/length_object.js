// length, object

var expected = "777,777,777,777,777,";
var actual = '';

function f() {
  var o = { a: 11, length: 777, b: 22 };

  for (var i = 0; i < 5; ++i) {
    actual += o.length + ',';
  }
}

f();

assertEq(actual, expected);
