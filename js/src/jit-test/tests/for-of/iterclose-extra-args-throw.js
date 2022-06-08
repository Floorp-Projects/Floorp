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
      throw "good";
    }
  }
}

function test() {
  with ({}) {}
  for (var i = 0; i < 100; i++) {
    var caught = "bad";
    try {
      closeIter(iterable)
    } catch (e) {
      caught = e;
    }
    assertEq(caught, "good");
  }
}

with ({}) {}

test();

// Force an IC in Ion.
try {
  closeIter([1,2,3,4,5]);
} catch {}

test();
