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
      break;
    }
  }
}

function test() {
  with ({}) {}
  finallyCount = 0;

  for (var i = 0; i < 100; i++) {
    closeIter(gen());
  }
  assertEq(finallyCount, 100);
}

with ({}) {}

test();

// Force an IC in Ion.
closeIter([1,2,3,4,5]);

test();
