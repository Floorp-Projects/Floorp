var invalidate = false;
var finallyCount = 0;

const iterable = {
  [Symbol.iterator]() {
    return {
      i: 0,
      next() {
        return { value: this.i++, done: false }
      },
      return() {
        if (invalidate) {
          // Invalidate Ion scripts.
          gc(closeIter, 'shrinking');
          return undefined;
        }
        return { value: "return", done: true };
      }
    };
  }
}

function closeIter(o) {
  try {
    for (var x of o) {
      if (x == 2) {
        break;
      }
    }
  } finally {
    finallyCount++;
  }
}

function test() {
  with ({}) {}
  for (var i = 0; i < 100; i++) {
    closeIter(iterable);
  }
  invalidate = true;
  try {
    closeIter(iterable);
  } catch {}
  invalidate = false;
}

with ({}) {}

test();

// Force an IC in Ion.
closeIter([1,2,3,4,5]);

test();
assertEq(finallyCount, 203);
