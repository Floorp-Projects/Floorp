"use strict"; // Use strict-mode to ensure |this| arguments aren't converted to objects.

var emptyIterator = {
  next() {
    return {done: true};
  }
};

Object.defineProperty(Number.prototype, Symbol.iterator, {
  configurable: true,
  get() {
    assertEq(typeof this, "number");
    return function() {
      assertEq(typeof this, "number");
      return emptyIterator;
    }
  }
});

Promise.all(0);
Promise.allSettled(0);
Promise.race(0);

if (typeof reportCompare === "function")
  reportCompare(0, 0);
