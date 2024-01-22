function testLinear() {
  var s = "\u{10000}\u{10001}\u{10002}\u{10003}\u{10004}\u{10005}\u{10006}\u{10007}";

  for (var i = 0; i < 200; ++i) {
    // Iterate over all possible string indices, which includes reading
    // unpaired lead surrogates.
    var index = i % s.length;

    var c = s.codePointAt(index);
    var e = ((index & 1) ? 0xdc00 : 0x10000) + (index >> 1);
    assertEq(c, e);
  }
}
for (var i = 0; i < 2; ++i) {
  testLinear();
}

function testLinearOnlyValidCodePoints() {
  var s = "\u{10000}\u{10001}\u{10002}\u{10003}\u{10004}\u{10005}\u{10006}\u{10007}";

  for (var i = 0; i < 200; ++i) {
    // Iterator over all valid code point indices. (Code points are at all even
    // string indices.)
    var index = (i % s.length) & ~1;

    var c = s.codePointAt(index);
    var e = ((index & 1) ? 0xdc00 : 0x10000) + (index >> 1);
    assertEq(c, e);
  }
}
for (var i = 0; i < 2; ++i) {
  testLinearOnlyValidCodePoints();
}

function testRope() {
  var left = "\u{10000}\u{10001}\u{10002}\u{10003}\u{10004}\u{10005}\u{10006}\u{10007}";
  var right = "\u{10008}\u{10009}\u{1000A}\u{1000B}\u{1000C}\u{1000D}\u{1000E}\u{1000F}";

  for (var i = 0; i < 200; ++i) {
    var s = newRope(left, right);

    // Iterate over all possible string indices, which includes reading
    // unpaired lead surrogates.
    var index = i % s.length;

    var c = s.codePointAt(index);
    var e = ((index & 1) ? 0xdc00 : 0x10000) + (index >> 1);
    assertEq(c, e);
  }
}
for (var i = 0; i < 2; ++i) {
  testRope();
}

function testRopeOnlyValidCodePoints() {
  var left = "\u{10000}\u{10001}\u{10002}\u{10003}\u{10004}\u{10005}\u{10006}\u{10007}";
  var right = "\u{10008}\u{10009}\u{1000A}\u{1000B}\u{1000C}\u{1000D}\u{1000E}\u{1000F}";

  for (var i = 0; i < 200; ++i) {
    var s = newRope(left, right);

    // Iterator over all valid code point indices. (Code points are at all even
    // string indices.)
    var index = (i % s.length) & ~1;

    var c = s.codePointAt(index);
    var e = ((index & 1) ? 0xdc00 : 0x10000) + (index >> 1);
    assertEq(c, e);
  }
}
for (var i = 0; i < 2; ++i) {
  testRopeOnlyValidCodePoints();
}

function testUnpairedLead() {
  var s = "\u{d800}\u{d801}\u{d802}\u{d803}\u{d804}\u{d805}\u{d806}\u{d807}";

  for (var i = 0; i < 200; ++i) {
    var index = i % s.length;

    var c = s.codePointAt(index);
    var e = 0xd800 + index;
    assertEq(c, e);
  }
}
for (var i = 0; i < 2; ++i) {
  testUnpairedLead();
}

function testUnpairedTail() {
  var s = "\u{dc00}\u{dc01}\u{dc02}\u{dc03}\u{dc04}\u{dc05}\u{dc06}\u{dc07}";

  for (var i = 0; i < 200; ++i) {
    var index = i % s.length;

    var c = s.codePointAt(index);
    var e = 0xdc00 + index;
    assertEq(c, e);
  }
}
for (var i = 0; i < 2; ++i) {
  testUnpairedTail();
}
