let target = 3;
function throwOn(x) {
  with ({}) {}
  if (x == target) {
    throw 3;
  }
}

// Test try-finally without catch
function foo() {
  var x = 0;
  try {
    throwOn(0);
    x = 1;
    throwOn(1);
    x = 2;
    throwOn(2);
    x = 3;
  } finally {
    assertEq(x, target);
  }
}

with ({}) {}
for (var i = 0; i < 1000; i++) {
  try {
    foo();
  } catch {}
}

for (var i = 0; i < 4; i++) {
  target = i;
  try {
    foo();
  } catch {}
}
