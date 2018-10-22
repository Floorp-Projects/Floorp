"use strict";

// To make it easier to follow, this code is arranged so that the functions are
// arranged in the order they are called.

const worker = new Worker("suspendTimeouts_worker.js");
worker.onerror = error => {
  const message =
      `error from worker: ${error.filename}:${error.lineno}: ${error.message}`;
  throw new Error(message);
};

// Create a message channel. Send one end to the worker, and return the other to
// the mochitest.
/* exported create_channel */
function create_channel() {
  const { port1, port2 } = new MessageChannel();
  info(`sending port to worker`);
  worker.postMessage({ mochitestPort: port1 }, [ port1 ]);
  return port2;
}

// Provoke the worker into sending us a message, and then refuse to receive said
// message, causing it to be delayed for later delivery.
//
// The worker will also post a message to the MessagePort we sent it earlier.
// That message should not be delayed, as it is handled by the mochitest window,
// not the content window. Its receipt signals that the test can assume that the
// runnable for step 2) is in the main thread's event queue, so the test can
// prepare for step 3).
/* exported start_worker */
function start_worker() {
  worker.onmessage = handle_echo;

  // This should prevent worker.onmessage from being called, until
  // resumeTimeouts is called.
  //
  // This function is provided by test_suspendTimeouts.js.
  // eslint-disable-next-line no-undef
  suspendTimeouts();

  // The worker should echo this message back to us and to the mochitest.
  worker.postMessage("HALLOOOOOO"); // suitable message for echoing
  info(`posted message to worker`);
}

var resumeTimeouts_has_returned = false;

// Resume timeouts. After this call, the worker's message should not be
// delivered to our onmessage handler until control returns to the event loop.
/* exported resume_timeouts */
function resume_timeouts() {
  // This function is provided by test_suspendTimeouts.js.
  // eslint-disable-next-line no-undef
  resumeTimeouts(); // onmessage handlers should not run from this call.

  resumeTimeouts_has_returned = true;

  // When this JavaScript invocation returns to the main thread's event loop,
  // only then should onmessage handlers be invoked.
}

// The buggy code calls this handler from the resumeTimeouts call, before the
// main thread returns to the event loop. The correct code calls this only once
// the JavaScript invocation that called resumeTimeouts has run to completion.
function handle_echo({data}) {
  ok(resumeTimeouts_has_returned, "worker message delivered from main event loop");

  // Finish the mochitest.
  finish();
}
