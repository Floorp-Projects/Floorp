"use strict";

// The debugger uses nsIDOMWindowUtils::suspendTimeouts and ...::resumeTimeouts
// to ensure that content event handlers do not run while a JavaScript
// invocation is stepping or paused at a breakpoint. If a worker thread sends
// messages to the content while the content is paused, those messages must not
// run until the JavaScript invocation interrupted by the debugger has completed.
//
// Bug 1426467 is that calling nsIDOMWindowUtils::resumeTimeouts actually
// delivers deferred messages itself, calling the content's 'onmessage' handler.
// But the debugger calls suspend/resume around each individual interruption of
// the debuggee -- each step, say -- meaning that hitting the "step into" button
// causes you to step from the debuggee directly into an onmessage handler,
// since the onmessage handler is the next function call the debugger sees.
//
// In other words, delivering deferred messages from resumeTimeouts, as it is
// used by the debugger, breaks the run-to-completion rule. They must not be
// delivered until after the JavaScript invocation at hand is complete. That's
// what this test checks.
//
// For this test to detect the bug, the following steps must take place in
// order:
//
// 1) The content page must call suspendTimeouts.
// 2) A runnable conveying a message from the worker thread must attempt to
//    deliver the message, see that the content page has suspended such things,
//    and hold the message for later delivery.
// 3) The content page must call resumeTimeouts.
//
// In a correct implementation, the message from the worker thread is delivered
// only after the main thread returns to the event loop after calling
// resumeTimeouts in step 3). In the buggy implementation, the onmessage handler
// is called directly from the call to resumeTimeouts, so that the onmessage
// handlers run in the midst of whatever JavaScript invocation resumed timeouts
// (say, stepping in the debugger), in violation of the run-to-completion rule.
//
// In this specific bug, the handlers are called from resumeTimeouts, but
// really, running them any time before that invocation returns to the main
// event loop would be a bug.
//
// Posting the message and calling resumeTimeouts take place in different
// threads, but if 2) and 3) don't occur in that order, the worker's message
// will never be delayed and the test will pass spuriously. But the worker
// can't communicate with the content page directly, to let it know that it
// should proceed with step 3): the purpose of suspendTimeouts is to pause
// all such communication.
//
// So instead, the content page creates a MessageChannel, and passes one
// MessagePort to the worker and the other to this mochitest (which has its
// own window, separate from the one calling suspendTimeouts). The worker
// notifies the mochitest when it has posted the message, and then the
// mochitest calls into the content to carry out step 3).

// To help you follow all the callbacks and event handlers, this code pulls out
// event handler functions so that control flows from top to bottom.

window.onload = function () {
  // This mochitest is not complete until we call SimpleTest.finish. Don't just
  // exit as soon as we return to the main event loop.
  SimpleTest.waitForExplicitFinish();

  const iframe = document.createElement("iframe");
  iframe.src =
    "http://mochi.test:8888/chrome/devtools/server/tests/chrome/suspendTimeouts_content.html";
  iframe.onload = iframe_onload_handler;
  document.body.appendChild(iframe);

  function iframe_onload_handler() {
    const content = iframe.contentWindow.wrappedJSObject;

    const windowUtils = iframe.contentWindow.windowUtils;

    // Hand over the suspend and resume functions to the content page, along
    // with some testing utilities.
    content.suspendTimeouts = function () {
      SimpleTest.info("test_suspendTimeouts", "calling suspendTimeouts");
      windowUtils.suspendTimeouts();
    };
    content.resumeTimeouts = function () {
      windowUtils.resumeTimeouts();
      SimpleTest.info("test_suspendTimeouts", "resumeTimeouts called");
    };
    content.info = function (message) {
      SimpleTest.info("suspendTimeouts_content.js", message);
    };
    content.ok = SimpleTest.ok;
    content.finish = finish;

    SimpleTest.info(
      "Disappointed with National Tautology Day? Well, it is what it is."
    );

    // Once the worker has sent a message to its parent (which should get delayed),
    // it sends us a message directly on this channel.
    const workerPort = content.create_channel();
    workerPort.onmessage = handle_worker_echo;

    // Have content send the worker a message that it should echo back to both
    // content and us. The echo to content should get delayed; the echo to us
    // should cause our handle_worker_echo to be called.
    content.start_worker();

    function handle_worker_echo({ data }) {
      info(`mochitest received message from worker: ${data}`);

      // As it turns out, it's not correct to assume that, if the worker posts a
      // message to its parent via the global `postMessage` function, and then
      // posts a message to the mochitest via the MessagePort, those two
      // messages will be delivered in the order they were sent.
      //
      // - Messages sent via the worker's global's postMessage go through two
      //   ThrottledEventQueues (one in the worker, and one on the parent), and
      //   eventually find their way into the thread's primary event queue,
      //   which is a PrioritizedEventQueue.
      //
      // - Messages sent via a MessageChannel whose ports are owned by different
      //   threads are passed as IPDL messages.
      //
      // There's basically no reliable way to ensure that delivery to content
      // has been attempted and the runnable deferred; there are too many
      // variables affecting the order in which things are processed. Delaying
      // for a second is the best I could think of.
      //
      // Fortunately, this tactic failing can only cause spurious test passes
      // (the runnable never gets deferred, so things work by accident), not
      // spurious failures. Without some kind of trustworthy notification that
      // the runnable has been deferred, perhaps via some special white-box
      // testing API, we can't do better.
      setTimeout(() => {
        content.resume_timeouts();
      }, 1000);
    }

    function finish() {
      SimpleTest.info("suspendTimeouts_content.js", "called finish");
      SimpleTest.finish();
    }
  }
};
