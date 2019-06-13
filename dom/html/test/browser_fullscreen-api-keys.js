"use strict";

// This test tends to trigger a race in the fullscreen time telemetry,
// where the fullscreen enter and fullscreen exit events (which use the
// same histogram ID) overlap. That causes TelemetryStopwatch to log an
// error.
SimpleTest.ignoreAllUncaughtExceptions(true);

/** Test for Bug 545812 **/

// List of key codes which should exit full-screen mode.
const kKeyList = [
  { key: "Escape", keyCode: "VK_ESCAPE", suppressed: true},
  { key: "F11",    keyCode: "VK_F11",    suppressed: false},
];

const kStrictKeyPressEvents =
  SpecialPowers.getBoolPref("dom.keyboardevent.keypress.dispatch_non_printable_keys_only_system_group_in_content");

/* eslint-disable mozilla/no-arbitrary-setTimeout */
function frameScript() {
  let doc = content.document;
  addMessageListener("Test:RequestFullscreen", () => {
    doc.body.requestFullscreen();
  });
  addMessageListener("Test:DispatchUntrustedKeyEvents", msg => {
    var evt = new content.CustomEvent("Test:DispatchKeyEvents", {
      detail: Cu.cloneInto({ code: msg.data }, content),
    });
    content.dispatchEvent(evt);
  });

  doc.addEventListener("fullscreenchange", () => {
    sendAsyncMessage("Test:FullscreenChanged", !!doc.fullscreenElement);
  });

  function keyHandler(evt) {
    sendAsyncMessage("Test:KeyReceived", {
      type: evt.type,
      keyCode: evt.keyCode
    });
  }
  doc.addEventListener("keydown", keyHandler, true);
  doc.addEventListener("keyup", keyHandler, true);
  doc.addEventListener("keypress", keyHandler, true);

  function waitUntilActive() {
    if (docShell.isActive && doc.hasFocus()) {
      sendAsyncMessage("Test:Activated");
    } else {
      setTimeout(waitUntilActive, 10);
    }
  }
  waitUntilActive();
}
/* eslint-enable mozilla/no-arbitrary-setTimeout */

var gMessageManager;

function listenOneMessage(aMsg, aListener) {
  function listener({ data }) {
    gMessageManager.removeMessageListener(aMsg, listener);
    aListener(data);
  }
  gMessageManager.addMessageListener(aMsg, listener);
}

function promiseOneMessage(aMsg) {
  return new Promise(resolve => listenOneMessage(aMsg, resolve));
}

function captureUnexpectedFullscreenChange() {
  ok(false, "Caught an unexpected fullscreen change");
}

async function temporaryRemoveUnexpectedFullscreenChangeCapture(callback) {
  gMessageManager.removeMessageListener(
    "Test:FullscreenChanged", captureUnexpectedFullscreenChange);
  await callback();
  gMessageManager.addMessageListener(
    "Test:FullscreenChanged", captureUnexpectedFullscreenChange);
}

function captureUnexpectedKeyEvent(type) {
  ok(false, `Caught an unexpected ${type} event`);
}

async function temporaryRemoveUnexpectedKeyEventCapture(callback) {
  gMessageManager.removeMessageListener(
    "Test:KeyReceived", captureUnexpectedKeyEvent);
  await callback();
  gMessageManager.addMessageListener(
    "Test:KeyReceived", captureUnexpectedKeyEvent);
}

function receiveExpectedKeyEvents(aKeyCode, aTrusted) {
  return new Promise(resolve => {
    let events = kStrictKeyPressEvents && aTrusted ? ["keydown", "keyup"] : ["keydown", "keypress", "keyup"];
    function listener({ data }) {
      let expected = events.shift();
      is(data.type, expected, `Should receive a ${expected} event`);
      is(data.keyCode, aKeyCode,
         `Should receive the event with key code ${aKeyCode}`);
      if (!events.length) {
        gMessageManager.removeMessageListener("Test:KeyReceived", listener);
        resolve();
      }
    }
    gMessageManager.addMessageListener("Test:KeyReceived", listener);
  });
}

const kPage = "http://example.org/browser/" +
              "dom/html/test/file_fullscreen-api-keys.html";

add_task(async function() {
  await pushPrefs(
    ["full-screen-api.transition-duration.enter", "0 0"],
    ["full-screen-api.transition-duration.leave", "0 0"]);

  let tab = BrowserTestUtils.addTab(gBrowser, kPage);
  let browser = tab.linkedBrowser;
  gBrowser.selectedTab = tab;
  registerCleanupFunction(() => gBrowser.removeTab(tab));
  await waitForDocLoadComplete();

  gMessageManager = browser.messageManager;
  gMessageManager.loadFrameScript(
    "data:,(" + frameScript.toString() + ")();", false);

  // Wait for the document being actived, so that
  // fullscreen request won't be denied.
  await promiseOneMessage("Test:Activated");

  // Register listener to capture unexpected events
  gMessageManager.addMessageListener(
    "Test:FullscreenChanged", captureUnexpectedFullscreenChange);
  gMessageManager.addMessageListener(
    "Test:KeyReceived", captureUnexpectedKeyEvent);
  registerCleanupFunction(() => {
    gMessageManager.removeMessageListener(
      "Test:FullscreenChanged", captureUnexpectedFullscreenChange);
    gMessageManager.removeMessageListener(
      "Test:KeyReceived", captureUnexpectedKeyEvent);
  });

  for (let {key, keyCode, suppressed} of kKeyList) {
    let keyCodeValue = KeyEvent["DOM_" + keyCode];
    info(`Test keycode ${key} (${keyCodeValue})`);

    info("Enter fullscreen");
    await temporaryRemoveUnexpectedFullscreenChangeCapture(async function() {
      gMessageManager.sendAsyncMessage("Test:RequestFullscreen");
      let state = await promiseOneMessage("Test:FullscreenChanged");
      ok(state, "The content should have entered fullscreen");
      ok(document.fullscreenElement,
         "The chrome should also be in fullscreen");
    });

    info("Dispatch untrusted key events from content");
    await temporaryRemoveUnexpectedKeyEventCapture(async function() {
      let promiseExpectedKeyEvents = receiveExpectedKeyEvents(keyCodeValue, false);
      gMessageManager.sendAsyncMessage("Test:DispatchUntrustedKeyEvents", keyCode);
      await promiseExpectedKeyEvents;
    });

    info("Send trusted key events");
    await temporaryRemoveUnexpectedFullscreenChangeCapture(async function() {
      await temporaryRemoveUnexpectedKeyEventCapture(async function() {
        let promiseExpectedKeyEvents = suppressed ?
          Promise.resolve() : receiveExpectedKeyEvents(keyCodeValue, true);
        EventUtils.synthesizeKey("KEY_" + key);
        await promiseExpectedKeyEvents;
        let state = await promiseOneMessage("Test:FullscreenChanged");
        ok(!state, "The content should have exited fullscreen");
        ok(!document.fullscreenElement,
           "The chrome should also have exited fullscreen");
      });
    });
  }
});
