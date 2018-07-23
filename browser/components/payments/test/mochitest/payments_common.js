"use strict";

/* exported asyncElementRendered, promiseStateChange, promiseContentToChromeMessage, deepClone,
   PTU, registerConsoleFilter */

const PTU = SpecialPowers.Cu.import("resource://testing-common/PaymentTestUtils.jsm", {})
                            .PaymentTestUtils;

/**
 * A helper to await on while waiting for an asynchronous rendering of a Custom
 * Element.
 * @returns {Promise}
 */
function asyncElementRendered() {
  return Promise.resolve();
}

function promiseStateChange(store) {
  return new Promise(resolve => {
    store.subscribe({
      stateChangeCallback(state) {
        store.unsubscribe(this);
        resolve(state);
      },
    });
  });
}

/**
 * Wait for a message of `messageType` from content to chrome and resolve with the event details.
 * @param {string} messageType of the expected message
 * @returns {Promise} when the message is dispatched
 */
function promiseContentToChromeMessage(messageType) {
  return new Promise(resolve => {
    document.addEventListener("paymentContentToChrome", function onCToC(event) {
      if (event.detail.messageType != messageType) {
        return;
      }
      document.removeEventListener("paymentContentToChrome", onCToC);
      resolve(event.detail);
    });
  });
}

function deepClone(obj) {
  return JSON.parse(JSON.stringify(obj));
}

/**
 * If filterFunction is a function which returns true given a console message
 * then the test won't fail from that message.
 */
let filterFunction = null;
function registerConsoleFilter(filterFn) {
  filterFunction = filterFn;
}

// Listen for errors to fail tests
SpecialPowers.registerConsoleListener(function onConsoleMessage(msg) {
  if (msg.isWarning || !msg.errorMessage || msg.errorMessage == "paymentRequest.xhtml:") {
    // Ignore warnings and non-errors.
    return;
  }
  if (msg.category == "CSP_CSPViolationWithURI" && msg.errorMessage.includes("at inline")) {
    // Ignore unknown CSP error.
    return;
  }
  if (msg.message == "SENTINEL") {
    filterFunction = null;
  }
  if (filterFunction && filterFunction(msg)) {
    return;
  }
  ok(false, msg.message || msg.errorMessage);
});

SimpleTest.registerCleanupFunction(function cleanup() {
  SpecialPowers.postConsoleSentinel();
});
