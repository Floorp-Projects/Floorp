var acc = 0;
const loopCount = 100;

class A {
  #x = 1;
  static loopRead(o) {
    for (var i = 0; i < loopCount; i++) {
      // If this getelem were hoisted out of the loop,
      // we need the IC that is attached to that to
      // correctly throw if .#x is not in o.
      var b = o.#x;
      acc += 1;
    }
  }
};

// Two non-A objects, because we're concerned not about the first
// attempt to read .#x from a non A, but the second, because if
// we attach the wrong IC, we'll attach an IC that provides
// regular object semantics, which would be to return undefined.
var array = [new A, new A, new A, {}, {}];
for (var e of array) {
  acc = 0;
  try {
    A.loopRead(e);
    assertEq(acc, loopCount);
  } catch (e) {
    assertEq(e instanceof TypeError, true);
    assertEq(acc, 0);
  }
}
