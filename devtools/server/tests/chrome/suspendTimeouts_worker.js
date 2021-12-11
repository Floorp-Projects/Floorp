"use strict";

// Once content sends us a port connected to the mochitest, we simply echo every
// message we receive back to content and the mochitest.
onmessage = ({ data: { mochitestPort } }) => {
  onmessage = ({ data }) => {
    // Send a message to both content and the mochitest, which the main thread's
    // event loop will attempt to deliver as step 2).
    postMessage(`worker echo to content: ${data}`);
    mochitestPort.postMessage(`worker echo to port: ${data}`);
  };
};
