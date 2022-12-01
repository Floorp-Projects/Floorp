// Load a character from the left rope child using a constant index. The input
// to String.prototype.charCodeAt is always rope.
function testLeftChildConstant() {
  for (var i = 0; i < 200; ++i) {
    var left = newRope("abc", "def");
    var right = newRope("ghi", "jkl");
    var s = newRope(left, right);

    var ch = s.charCodeAt(0);
    assertEq(ch, 0x61);
  }
}
for (var i = 0; i < 2; ++i) {
  testLeftChildConstant();
}

// Load a character from the right rope child using a constant index. The input
// to String.prototype.charCodeAt is always rope.
function testRightChildConstant() {
  for (var i = 0; i < 200; ++i) {
    var left = newRope("abc", "def");
    var right = newRope("ghi", "jkl");
    var s = newRope(left, right);

    var ch = s.charCodeAt(6);
    assertEq(ch, 0x61 + 6);
  }
}
for (var i = 0; i < 2; ++i) {
  testRightChildConstant();
}

// Load a character from the left rope child using a variable index. The input
// to String.prototype.charCodeAt is always rope.
function testLeftChildVariable() {
  for (var i = 0; i < 200; ++i) {
    var left = newRope("abc", "def");
    var right = newRope("ghi", "jkl");
    var s = newRope(left, right);

    var idx = i % left.length;
    var ch = s.charCodeAt(idx);
    assertEq(ch, 0x61 + idx);
  }
}
for (var i = 0; i < 2; ++i) {
  testLeftChildVariable();
}

// Load a character from the right rope child using a variable index. The input
// to String.prototype.charCodeAt is always rope.
function testRightChildVariable() {
  for (var i = 0; i < 200; ++i) {
    var left = newRope("abc", "def");
    var right = newRope("ghi", "jkl");
    var s = newRope(left, right);

    var idx = i % right.length;
    var ch = s.charCodeAt(left.length + idx);
    assertEq(ch, 0x61 + 6 + idx);
  }
}
for (var i = 0; i < 2; ++i) {
  testRightChildVariable();
}

// Load all characters from both child ropes. This covers the case when the
// call to String.prototype.charCodeAt linearizes the rope. 
function testBothChildren() {
  for (var i = 0; i < 200; ++i) {
    var left = newRope("abc", "def");
    var right = newRope("ghi", "jkl");
    var s = newRope(left, right);

    for (var j = 0; j < s.length; ++j) {
      var ch = s.charCodeAt(j);
      assertEq(ch, 0x61 + j);
    }
  }
}
for (var i = 0; i < 2; ++i) {
  testBothChildren();
}
