const iterable = {
  [Symbol.iterator]() {
    return {
      i: 0,
      next() {
        return { value: this.i++, done: false }
      },
      return(a, b, c, d) {
        assertEq(a, undefined);
        assertEq(b, undefined);
        assertEq(c, undefined);
        assertEq(d, undefined);
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
    closeIter(iterable)
  }
}

with ({}) {}

test();

// Force an IC in Ion.
closeIter([1,2,3,4,5]);

test();
