// Load Latin-1 and Two-Byte strings.
function latin1AndTwoByte() {
  for (var i = 0; i <= 0x03FF; ++i) {
    var s = String.fromCodePoint(i);
    assertEq(s[0], s);
    assertEq(s.charAt(0), s);
  }
}

for (var i = 0; i < 2; ++i) {
  latin1AndTwoByte();
}

// Test bi-morphic loads.
function stringAndArray() {
  var xs = [["\u0100"], "\u0100"];
  for (var i = 0; i < 200; ++i) {
    var x = xs[i & 1];
    var s = x[0];
    assertEq(s.length, 1);
    assertEq(s, "\u0100");
  }
}

for (var i = 0; i < 2; ++i) {
  stringAndArray();
}

function outOfBoundsFailureThenException() {
  var name = "Object";

  var j = 0;
  while (true) {
    // Access out-of-bounds character and then trigger an exception,
    // when accessing a property on the undefined value.
    name[j++].does_not_exist;
  }
}

for (var i = 0; i < 2; ++i) {
  try {
    outOfBoundsFailureThenException();
  } catch {}
}
