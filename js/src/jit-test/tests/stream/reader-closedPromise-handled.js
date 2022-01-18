// |jit-test| skip-if: !this.hasOwnProperty("ReadableStream")
// Creating a reader from an errored stream should not result in a promise
// being tracked as unhandled.

// Create an errored stream.
let stream = new ReadableStream({
  start(controller) {
    controller.error(new Error("splines insufficiently reticulated"));
  }
});
drainJobQueue();

// Track promises.
let status = new Map;
setPromiseRejectionTrackerCallback((p, x) => { status.set(p, x); });

// Per Streams spec 3.7.4 step 5.c, this creates a rejected promise
// (reader.closed) but marks it as handled.
let reader = stream.getReader();

// Check that the promise's status is not 0 (unhandled);
// it may be either 1 (handled) or undefined (never tracked).
let result = status.get(reader.closed);
assertEq(result === 1 || result === undefined, true);
