// Test that SavedFrame instances get removed from the SavedStacks frames cache
// after a GC.

const FUZZ_FACTOR = 3;

function assertAboutEq(actual, expected) {
  if (Math.abs(actual - expected) > FUZZ_FACTOR)
    throw new Error("Assertion failed: expected about " + expected + ", got " + actual +
                    ". FUZZ_FACTOR = " + FUZZ_FACTOR);
}

const stacks = [];

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

assertAboutEq(getSavedFrameCount(), 50);

while (stacks.length) {
  stacks.pop();
}
gc();

stacks = null;
gc();

assertAboutEq(getSavedFrameCount(), 0);
