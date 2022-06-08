var returnInvalid = false;

const iterable = {
  [Symbol.iterator]() {
    return {
      i: 0,
      next() {
        return { value: this.i++, done: false }
      },
      return() {
        return returnInvalid ? undefined : { value: "return", done: true };
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
  for (var i = 0; i < 1000; i++) {
    returnInvalid = i % 10 == 0;
    var caught = false;
    try {
      closeIter(iterable);
    } catch {
      caught = true;
    }
    assertEq(caught, returnInvalid);
  }
}

with ({}) {}

test();

// Force an IC in Ion.
closeIter([1,2,3,4,5]);

test();
