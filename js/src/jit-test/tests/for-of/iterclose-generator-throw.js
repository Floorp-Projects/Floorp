var finallyCount = 0;

function* gen(o) {
  try {
    yield 1;
    yield 2;
    yield 3;
  } finally {
    finallyCount++;
  }
}

function closeIter(o) {
  for (var x of o) {
    if (x == 2) {
      throw "good"
    }
  }
}

function test() {
  with ({}) {}
  finallyCount = 0;

  for (var i = 0; i < 100; i++) {
    var caught = "bad";
    try {
      closeIter(gen());
    } catch (e) {
      caught = e;
    }
    assertEq(caught, "good");
  }
  assertEq(finallyCount, 100);
}

with ({}) {}

test();

// Force an IC in Ion.
try {
  closeIter([1,2,3,4,5]);
} catch {}

test();
