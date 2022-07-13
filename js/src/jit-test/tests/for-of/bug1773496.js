var shouldBail = false;
function foo() {
  if (shouldBail) {
    bailout();
    return 0;
  }
  for (var i = 0; i < 1000; i++) {}
  return { value: undefined, done: true }
}

const iterable = {
  [Symbol.iterator]() {
    return {
      i: 0,
      next() {
        return { value: this.i++, done: false }
      },
      return() {
        return foo();
      }
    };
  }
}

function closeIter() {
  with ({}) {}
  for (var x of iterable) {
    if (x == 2) {
      break;
    }
  }
}

with ({}) {}
for (var i = 0; i < 100; i++) {
  closeIter();
}

shouldBail = true;

caught = false;
try {
  closeIter();
} catch {
  caught = true;
}
assertEq(caught, true);
