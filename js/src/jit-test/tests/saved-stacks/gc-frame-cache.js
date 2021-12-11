// Test that SavedFrame instances get removed from the SavedStacks frames cache
// after a GC.

const FUZZ_FACTOR = 3;

function isAboutEq(actual, expected) {
  return Math.abs(actual - expected) <= FUZZ_FACTOR;
}

var stacks = [];

(function () {
  // Use an IIFE here so that we don't keep these saved stacks alive in the
  // frame cache when we test that they all go away at the end of the test.

  var startCount = getSavedFrameCount();
  print("startCount = " + startCount);

  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());
  stacks.push(saveStack());

  gc();

  var endCount = getSavedFrameCount();
  print("endCount = " + endCount);

  assertEq(isAboutEq(endCount - startCount, 50), true);
}());

while (stacks.length) {
  stacks.pop();
}
gc();

stacks = null;
gc();

assertEq(isAboutEq(getSavedFrameCount(), 0), true);
