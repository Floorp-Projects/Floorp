function makeRope() {
  var left = newRope("@ABCDEFGHIJKLMNO", "PQRSTUVWXYZ[\\]^_");
  var right = newRope("`abcdefghijklmno", "pqrstuvwxyz{|}~");
  var rope = newRope(left, right);
  return {left, right, rope};
}

// Load a character from the left rope child using a constant index. The input
// to String.prototype.charCodeAt is always rope.
function testLeftChildConstant() {
  for (var i = 0; i < 200; ++i) {
    var {rope} = makeRope();

    var ch = rope.charCodeAt(0);
    assertEq(ch, 0x40);
  }
}
for (var i = 0; i < 2; ++i) {
  testLeftChildConstant();
}

// Load a character from the right rope child using a constant index. The input
// to String.prototype.charCodeAt is always rope.
function testRightChildConstant() {
  for (var i = 0; i < 200; ++i) {
    var {rope} = makeRope();

    var ch = rope.charCodeAt(32);
    assertEq(ch, 0x60);
  }
}
for (var i = 0; i < 2; ++i) {
  testRightChildConstant();
}

// Load a character from the left rope child using a variable index. The input
// to String.prototype.charCodeAt is always rope.
function testLeftChildVariable() {
  for (var i = 0; i < 200; ++i) {
    var {left, rope} = makeRope();

    var idx = i % left.length;
    var ch = rope.charCodeAt(idx);
    assertEq(ch, 0x40 + idx);
  }
}
for (var i = 0; i < 2; ++i) {
  testLeftChildVariable();
}

// Load a character from the right rope child using a variable index. The input
// to String.prototype.charCodeAt is always rope.
function testRightChildVariable() {
  for (var i = 0; i < 200; ++i) {
    var {left, right, rope} = makeRope();

    var idx = i % right.length;
    var ch = rope.charCodeAt(left.length + idx);
    assertEq(ch, 0x60 + idx);
  }
}
for (var i = 0; i < 2; ++i) {
  testRightChildVariable();
}

// Load all characters from both child ropes. This covers the case when the
// call to String.prototype.charCodeAt linearizes the rope. 
function testBothChildren() {
  for (var i = 0; i < 200; ++i) {
    var {rope} = makeRope();

    for (var j = 0; j < rope.length; ++j) {
      var ch = rope.charCodeAt(j);
      assertEq(ch, 0x40 + j);
    }
  }
}
for (var i = 0; i < 2; ++i) {
  testBothChildren();
}
