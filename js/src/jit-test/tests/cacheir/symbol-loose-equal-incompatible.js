// Test loose equality comparison between Symbols and String/Boolean/Int32/Double/BigInt.

var xs = [
  Symbol(), Symbol(), Symbol(), Symbol(),
  Symbol(), Symbol(), Symbol(), Symbol(),
];

var ys = [
  "", "test", true, false,
  123, 123.5, NaN, 456n,
];

function testLooseEqual() {
  for (var i = 0; i < 100; ++i) {
    var j = i % xs.length;
    var x = xs[j];
    var y = ys[j];

    assertEq(x == y, false);
    assertEq(y == x, false);
  }
}
testLooseEqual();

function testLooseNotEqual() {
  for (var i = 0; i < 100; ++i) {
    var j = i % xs.length;
    var x = xs[j];
    var y = ys[j];

    assertEq(x != y, true);
    assertEq(y != x, true);
  }
}
testLooseNotEqual();
