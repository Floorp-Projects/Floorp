// Test comparison between Boolean x {String, Double}.

// Boolean R String <=> ToNumber(Boolean) R ToNumber(String)
// Boolean R Number <=> ToNumber(Boolean) R Number

// The entries in the first halves of xs, ys, and zs should be loose-equal to each other.

var xs = [
  // ToNumber(bool) == 0
  false, false, false, false,

  // ToNumber(bool) == 1
  true, true, true, true,

  // Filler
  false, false, false, false,
  true, true, true, true,
];

var ys = [
  // ToNumber(str) == 0
  "", "0", "0.0", ".0",

  // ToNumber(str) == 1
  "1", "1.0", "0x1", "  1\t\r\n",

  // ToNumber(str) != {0, 1}
  // (Duplicate entries to ensure they're neither equal to |true| nor to |false|.)
  "not-a-number", "NaN", "Infinity", "2",
  "not-a-number", "NaN", "Infinity", "2",
];

function Double(x) {
  // numberToDouble always returns a Double valued number.
  return numberToDouble(x);
}

var zs = [
  // = 0
  Double(0), Double(0), -0, -0,

  // = 1
  Double(1), Double(1), Double(1), Double(1),

  // != {0, 1}
  // (Duplicate entries to ensure they're neither equal to |true| nor to |false|.)
  NaN, Infinity, Double(2), Double(-1.5),
  NaN, Infinity, Double(2), Double(-1.5),
];

function testLooseEqual() {
  for (var i = 0; i < 100; ++i) {
    var j = i % xs.length;
    var x = xs[j];
    var y = ys[j];
    var z = zs[j];

    var r = j < (xs.length >> 1);

    assertEq(x == y, r);
    assertEq(y == x, r);

    assertEq(x == z, r);
    assertEq(z == x, r);
  }
}
testLooseEqual();

function testLooseNotEqual() {
  for (var i = 0; i < 100; ++i) {
    var j = i % xs.length;
    var x = xs[j];
    var y = ys[j];
    var z = zs[j];

    var r = j < (xs.length >> 1);

    assertEq(x != y, !r);
    assertEq(y != x, !r);

    assertEq(x != z, !r);
    assertEq(z != x, !r);
  }
}
testLooseNotEqual();

function testLessThan() {
  for (var i = 0; i < 100; ++i) {
    var j = i % xs.length;
    var x = xs[j];
    var y = ys[j];
    var z = zs[j];

    assertEq(x < y, +x < +y);
    assertEq(y < x, +y < +x);

    assertEq(x < z, +x < +z);
    assertEq(z < x, +z < +x);
  }
}
testLessThan();

function testLessThanEqual() {
  for (var i = 0; i < 100; ++i) {
    var j = i % xs.length;
    var x = xs[j];
    var y = ys[j];
    var z = zs[j];

    assertEq(x <= y, +x <= +y);
    assertEq(y <= x, +y <= +x);

    assertEq(x <= z, +x <= +z);
    assertEq(z <= x, +z <= +x);
  }
}
testLessThanEqual();

function testGreaterThan() {
  for (var i = 0; i < 100; ++i) {
    var j = i % xs.length;
    var x = xs[j];
    var y = ys[j];
    var z = zs[j];

    assertEq(x > y, +x > +y);
    assertEq(y > x, +y > +x);

    assertEq(x > z, +x > +z);
    assertEq(z > x, +z > +x);
  }
}
testGreaterThan();

function testGreaterThanEqual() {
  for (var i = 0; i < 100; ++i) {
    var j = i % xs.length;
    var x = xs[j];
    var y = ys[j];
    var z = zs[j];

    assertEq(x >= y, +x >= +y);
    assertEq(y >= x, +y >= +x);

    assertEq(x >= z, +x >= +z);
    assertEq(z >= x, +z >= +x);
  }
}
testGreaterThanEqual();
