// Test that we can save stacks that cross contexts.

const stack = (function iife() {
  return evaluate("(" + function evalFrame() {
    return saveStack();
  } + "())", {
    newContext: true,
    fileName: "evaluated"
  });
}());

print(stack);

assertEq(stack.functionDisplayName, "evalFrame");
assertEq(stack.source, "evaluated");

assertEq(stack.parent.source, "evaluated");

assertEq(stack.parent.parent.functionDisplayName, "iife");
assertEq(/new-context\.js$/.test(stack.parent.parent.source), true);

assertEq(/new-context\.js$/.test(stack.parent.parent.parent.source), true);

assertEq(stack.parent.parent.parent.parent, null);
