async function foo() {
  // Suspend this function until later.
  await undefined;

  // Capture the stack using JS::AllFrames.
  saveStack();

  // Capture the stack using JS::FirstSubsumedFrame.
  var g = newGlobal({principal: {}});
  captureFirstSubsumedFrame(g);
}

// Invoke foo. This will capture the stack and mark the bit
// on the top-level script, but we clear it below.
foo();

// Tier up to blinterp, clearing the hasCachedSavedFrame bit
// on the top-level script.
for (var i = 0; i < 20; i++) {}

// Continue execution of foo.
drainJobQueue();
