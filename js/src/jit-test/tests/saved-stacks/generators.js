// Test that we can save stacks which have generator frames.

const { value: frame } = (function iife1() {
  return (function* generator() {
    yield (function iife2() {
      return saveStack();
    }());
  }()).next();
}());

// Bug 1102498 - toString does not include self-hosted frames, which can appear
// depending on GC timing. This may end up changing in the future, see
// bug 1103155.

var lines = frame.toString().split("\n");
assertEq(lines[0].startsWith("iife2@"), true);
assertEq(lines[1].startsWith("generator@"), true);
assertEq(lines[2].startsWith("iife1@"), true);
assertEq(lines[3].startsWith("@"), true);
