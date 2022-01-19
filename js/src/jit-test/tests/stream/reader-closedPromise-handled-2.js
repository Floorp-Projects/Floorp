// |jit-test| skip-if: !this.hasOwnProperty("ReadableStream")
// Releasing a reader should not result in a promise being tracked as
// unhandled.

function test(readable) {
  // Create an errored stream.
  let controller;
  let stream = new ReadableStream({
    start(c) {
      controller = c;
    }
  });
  drainJobQueue();

  // Track promises.
  let status = new Map;
  setPromiseRejectionTrackerCallback((p, x) => { status.set(p, x); });

  // Per Streams spec 3.7.5 step 5, this creates a rejected promise
  // (reader.closed) but marks it as handled.
  let reader = stream.getReader();
  if (!readable) {
    controller.close();
  }
  reader.releaseLock();

  // Check that the promise's status is not 0 (unhandled);
  // it may be either 1 (handled) or undefined (never tracked).
  let result = status.get(reader.closed);
  assertEq(result === 1 || result === undefined, true);
}

test(true);
test(false);
