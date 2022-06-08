var returnInvalid = false;

const iterable = {
  [Symbol.iterator]() {
    return {
      i: 0,
      next() {
        return { value: this.i++, done: false }
      },
      return() {
        if (returnInvalid) {
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
  for (var x of o) {
    if (x == 2) {
      break;
    }
  }
}

function test() {
  with ({}) {}
  for (var i = 0; i < 100; i++) {
    closeIter(iterable);
  }
  returnInvalid = true;
  var caught = false;
  try {
    closeIter(iterable);
  } catch {
    caught = true;
  }
  assertEq(caught, true);
  returnInvalid = false;
}

with ({}) {}

test();

// Force an IC in Ion.
closeIter([1,2,3,4,5]);

test();
