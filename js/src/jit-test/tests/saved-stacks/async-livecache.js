// Async stacks should not supplant LiveSavedFrameCache hits.

top();

// An ordinary function, to give the frame a convenient name.
function top() {
  // Perform an async call. F will run in an activation that has an async stack
  // supplied.
  f().catch(catchError);
}

async function f() {
  // Perform an ordinary call. Its parent frame will be a LiveSavedFrameCache
  // hit.
  g();
}

function g() {
  // Populate the LiveSavedFrameCache.
  saveStack();

  // Capturing the stack again should find f (if not g) in the cache. The async
  // stack supplied below the call to f should not supplant f's own frame.
  let frame = saveStack();

  assertEq(frame.functionDisplayName, 'g');
  assertEq(parent(frame).functionDisplayName, 'f');
  assertEq(parent(parent(frame)).functionDisplayName, 'top');
}

// Return the parent of |frame|, skipping self-hosted code and following async
// parent links.
function parent(frame) {
  do {
    frame = frame.parent || frame.asyncParent;
  } while (frame.source.match(/self-hosted/));
  return frame;
}

function catchError(e) {
  print(`${e}\n${e.stack}`);
  quit(1)
}
