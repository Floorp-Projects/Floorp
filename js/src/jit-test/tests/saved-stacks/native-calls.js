// Test that we can save stacks with native code on the stack.

// Unlike Array.prototype.map, Array.prototype.filter is not self-hosted.
const filter = (function iife() {
  try {
    callFunctionFromNativeFrame(n => { throw saveStack() });
  } catch (s) {
    return s;
  }
}());

assertEq(filter.parent.functionDisplayName, "iife");
