// Load a character from the left rope child using a constant index. The input
// to String.prototype.charAt is always rope.
function testLeftChildConstant() {
  for (var i = 0; i < 200; ++i) {
    var left = newRope("abc", "def");
    var right = newRope("ghi", "jkl");
    var s = newRope(left, right);

    var ch = s.charAt(0);
    assertEq(ch, "a");
  }
}
for (var i = 0; i < 2; ++i) {
  testLeftChildConstant();
}

// Load a character from the right rope child using a constant index. The input
// to String.prototype.charAt is always rope.
function testRightChildConstant() {
  for (var i = 0; i < 200; ++i) {
    var left = newRope("abc", "def");
    var right = newRope("ghi", "jkl");
    var s = newRope(left, right);

    var ch = s.charAt(6);
    assertEq(ch, "g");
  }
}
for (var i = 0; i < 2; ++i) {
  testRightChildConstant();
}

// Load a character from the left rope child using a variable index. The input
// to String.prototype.charAt is always rope.
function testLeftChildVariable() {
  for (var i = 0; i < 200; ++i) {
    var left = newRope("abc", "def");
    var right = newRope("ghi", "jkl");
    var s = newRope(left, right);

    var idx = i % left.length;
    var ch = s.charAt(idx);
    assertEq(ch, String.fromCharCode(0x61 + idx));
  }
}
for (var i = 0; i < 2; ++i) {
  testLeftChildVariable();
}

// Load a character from the right rope child using a variable index. The input
// to String.prototype.charAt is always rope.
function testRightChildVariable() {
  for (var i = 0; i < 200; ++i) {
    var left = newRope("abc", "def");
    var right = newRope("ghi", "jkl");
    var s = newRope(left, right);

    var idx = i % right.length;
    var ch = s.charAt(left.length + idx);
    assertEq(ch, String.fromCharCode(0x61 + 6 + idx));
  }
}
for (var i = 0; i < 2; ++i) {
  testRightChildVariable();
}

// Load all characters from both child ropes. This covers the case when the
// call to String.prototype.charAt linearizes the rope. 
function testBothChildren() {
  for (var i = 0; i < 200; ++i) {
    var left = newRope("abc", "def");
    var right = newRope("ghi", "jkl");
    var s = newRope(left, right);

    for (var j = 0; j < s.length; ++j) {
      var ch = s.charAt(j);
      assertEq(ch, String.fromCharCode(0x61 + j));
    }
  }
}
for (var i = 0; i < 2; ++i) {
  testBothChildren();
}
