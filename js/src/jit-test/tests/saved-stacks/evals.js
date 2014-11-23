// Test that we can save stacks with direct and indirect eval calls.

const directEval = (function iife() {
  return eval("(" + function evalFrame() {
    return saveStack();
  } + "())");
}());

assertEq(directEval.source.includes("> eval"), true);
assertEq(directEval.functionDisplayName, "evalFrame");

assertEq(directEval.parent.source.includes("> eval"), true);

assertEq(directEval.parent.parent.source.includes("> eval"), false);
assertEq(directEval.parent.parent.functionDisplayName, "iife");

assertEq(directEval.parent.parent.parent.source.includes("> eval"), false);

assertEq(directEval.parent.parent.parent.parent, null);

const lave = eval;
const indirectEval = (function iife() {
  return lave("(" + function evalFrame() {
    return saveStack();
  } + "())");
}());

assertEq(indirectEval.source.includes("> eval"), true);
assertEq(indirectEval.functionDisplayName, "evalFrame");

assertEq(indirectEval.parent.source.includes("> eval"), true);

assertEq(indirectEval.parent.parent.source.includes("> eval"), false);
assertEq(indirectEval.parent.parent.functionDisplayName, "iife");

assertEq(indirectEval.parent.parent.parent.source.includes("> eval"), false);

assertEq(indirectEval.parent.parent.parent.parent, null);
