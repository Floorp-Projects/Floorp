// Test for one annoying case of the EliminateUnreachableCode
// optimization.  Here the dominator of print("Goodbye") changes to be
// the print("Hello") after optimization.

function test1(v) {
  if (v) {
    if (v) {
      assertEq(v, v);
    } else {
      assertEq(0, 1);
    }
  } else {
    if (v) {
      assertEq(0, 1);
    } else {
      assertEq(v, v);
    }
  }
  assertEq(v, v);
}

function test() {
  test1(true);
  test1(false);
}

for (var i = 0; i < 100; i++)
  test();
