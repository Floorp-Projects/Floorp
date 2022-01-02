// Test for one annoying case of the EliminateUnreachableCode
// optimization.  Here the dominators change and also phis are
// eliminated.

function test1(v) {
  var i = 0;
  if (v) {
    if (v) {
      i += 1;
    } else {
      i += 10;
    }
    i += 100;
  } else {
    if (v) {
      i += 1000;
    } else {
      i += 10000;
    }
    i += 100000;
  }
  i += 1000000;
  return i;
}

function test() {
  assertEq(test1(true), 1000101);
  assertEq(test1(false), 1110000);
}

for (var i = 0; i < 100; i++)
  test();
