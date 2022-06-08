var counter = 0;
const iterable = {
  [Symbol.iterator]() {
    return {
      i: 0,

      // Force the return method into a dynamic slot.
      x0: 0, x1: 0, x2: 0, x3: 0, x4: 0, x5: 0, x6: 0, x7: 0, x8: 0,
      x9: 0, x10: 0, x11: 0, x12: 0, x13: 0, x14: 0, x15: 0, x16: 0,
      x17: 0, x18: 0, x19: 0, x20: 0, x21: 0, x22: 0, x23: 0, x24: 0,

      next() {
        return { value: this.i++, done: false }
      },
      return() {
        counter += 1;
        return { value: undefined, done: true };
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
  counter = 0;
  for (var i = 0; i < 100; i++) {
    closeIter(iterable)
  }
  assertEq(counter, 100)
}

with ({}) {}

test();

// Force an IC in Ion.
closeIter([1,2,3]);

test();
