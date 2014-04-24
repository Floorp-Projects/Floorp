// Test that we can save stacks which have generator frames.

const { value: frame } = (function iife1() {
  return (function* generator() {
    yield (function iife2() {
      return saveStack();
    }());
  }()).next();
}());

assertEq(frame.functionDisplayName, "iife2");
assertEq(frame.parent.functionDisplayName, "generator");
assertEq(frame.parent.parent.functionDisplayName, "iife1");
assertEq(frame.parent.parent.parent.functionDisplayName, null);
assertEq(frame.parent.parent.parent.parent, null);
