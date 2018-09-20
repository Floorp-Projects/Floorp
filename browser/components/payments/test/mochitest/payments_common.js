"use strict";

/* exported asyncElementRendered, promiseStateChange, promiseContentToChromeMessage, deepClone,
   PTU, registerConsoleFilter, fillField, importDialogDependencies */

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

/**
 * Import the templates and stylesheets from the real shipping dialog to avoid
 * duplication in the tests.
 * @param {HTMLIFrameElement} templateFrame - Frame to copy the resources from
 * @param {HTMLElement} destinationEl - Where to append the copied resources
 */
function importDialogDependencies(templateFrame, destinationEl) {
  for (let template of templateFrame.contentDocument.querySelectorAll("template")) {
    let imported = document.importNode(template, true);
    destinationEl.appendChild(imported);
  }

  let baseURL = new URL("../../res/", window.location.href);
  let stylesheetLinks = templateFrame.contentDocument.querySelectorAll("link[rel~='stylesheet']");
  for (let stylesheet of stylesheetLinks) {
    let imported = document.importNode(stylesheet, true);
    imported.href = new URL(imported.getAttribute("href"), baseURL);
    destinationEl.appendChild(imported);
  }
}

function deepClone(obj) {
  return JSON.parse(JSON.stringify(obj));
}

/**
 * @param {HTMLElement} field
 * @param {string} value
 * @note This is async in case we need to make it async to handle focus in the future.
 * @note Keep in sync with the copy in head.js
 */
async function fillField(field, value) {
  field.focus();
  if (field.localName == "select") {
    if (field.value == value) {
      // Do nothing
      return;
    }
    field.value = value;
    field.dispatchEvent(new Event("input", {bubbles: true}));
    field.dispatchEvent(new Event("change", {bubbles: true}));
    return;
  }
  while (field.value) {
    sendKey("BACK_SPACE");
  }
  sendString(value);
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
