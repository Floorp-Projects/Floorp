// Test that we can capture only the N newest frames.
// This is the maxFrameCount argument to JS::CaptureCurrentStack.

load(libdir + 'asserts.js');

function recur(n, limit) {
  if (n > 0)
    return recur(n - 1, limit);
  return saveStack(limit);
}

// Negative values are rejected.
assertThrowsInstanceOf(() => saveStack(-1), TypeError);

// Zero means 'no limit'.
assertEq(saveStack(0).parent, null);
assertEq(recur(0, 0).parent !== null, true);
assertEq(recur(0, 0).parent.parent, null);
assertEq(recur(1, 0).parent.parent.parent, null);
assertEq(recur(2, 0).parent.parent.parent.parent, null);
assertEq(recur(3, 0).parent.parent.parent.parent.parent, null);

// limit of 1
assertEq(saveStack(1).parent, null);
assertEq(recur(0, 1).parent, null);
assertEq(recur(0, 1).parent, null);
assertEq(recur(1, 1).parent, null);
assertEq(recur(2, 1).parent, null);

// limit of 2
assertEq(saveStack(2).parent, null);
assertEq(recur(0, 2).parent !== null, true);
assertEq(recur(0, 2).parent.parent, null);
assertEq(recur(1, 2).parent.parent, null);
assertEq(recur(2, 2).parent.parent, null);

// limit of 3
assertEq(saveStack(3).parent, null);
assertEq(recur(0, 3).parent !== null, true);
assertEq(recur(0, 3).parent.parent, null);
assertEq(recur(1, 3).parent.parent.parent, null);
assertEq(recur(2, 3).parent.parent.parent, null);
