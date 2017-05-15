"use strict";

/** Test for Bug 545812 **/

// List of key codes which should exit full-screen mode.
const kKeyList = [
  { code: "VK_ESCAPE", suppressed: true},
  { code: "VK_F11",    suppressed: false},
];

function frameScript() {
  let doc = content.document;
  addMessageListener("Test:RequestFullscreen", () => {
    doc.body.requestFullscreen();
  });
  addMessageListener("Test:DispatchUntrustedKeyEvents", msg => {
    var evt = new content.CustomEvent("Test:DispatchKeyEvents", {
      detail: { code: msg.data }
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
    if (doc.docShell.isActive && doc.hasFocus()) {
      sendAsyncMessage("Test:Activated");
    } else {
      setTimeout(waitUntilActive, 10);
    }
  }
  waitUntilActive();
}

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

function* temporaryRemoveUnexpectedFullscreenChangeCapture(callback) {
  gMessageManager.removeMessageListener(
    "Test:FullscreenChanged", captureUnexpectedFullscreenChange);
  yield* callback();
  gMessageManager.addMessageListener(
    "Test:FullscreenChanged", captureUnexpectedFullscreenChange);
}

function captureUnexpectedKeyEvent(type) {
  ok(false, `Caught an unexpected ${type} event`);
}

function* temporaryRemoveUnexpectedKeyEventCapture(callback) {
  gMessageManager.removeMessageListener(
    "Test:KeyReceived", captureUnexpectedKeyEvent);
  yield* callback();
  gMessageManager.addMessageListener(
    "Test:KeyReceived", captureUnexpectedKeyEvent);
}

function receiveExpectedKeyEvents(keyCode) {
  return new Promise(resolve => {
    let events = ["keydown", "keypress", "keyup"];
    function listener({ data }) {
      let expected = events.shift();
      is(data.type, expected, `Should receive a ${expected} event`);
      is(data.keyCode, keyCode,
         `Should receive the event with key code ${keyCode}`);
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

add_task(function* () {
  yield pushPrefs(
    ["full-screen-api.transition-duration.enter", "0 0"],
    ["full-screen-api.transition-duration.leave", "0 0"]);

  let tab = BrowserTestUtils.addTab(gBrowser, kPage);
  let browser = tab.linkedBrowser;
  gBrowser.selectedTab = tab;
  registerCleanupFunction(() => gBrowser.removeTab(tab));
  yield waitForDocLoadComplete();

  gMessageManager = browser.messageManager;
  gMessageManager.loadFrameScript(
    "data:,(" + frameScript.toString() + ")();", false);

  // Wait for the document being actived, so that
  // fullscreen request won't be denied.
  yield promiseOneMessage("Test:Activated");

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

  for (let {code, suppressed} of kKeyList) {
    var keyCode = KeyEvent["DOM_" + code];
    info(`Test keycode ${code} (${keyCode})`);

    info("Enter fullscreen");
    yield* temporaryRemoveUnexpectedFullscreenChangeCapture(function* () {
      gMessageManager.sendAsyncMessage("Test:RequestFullscreen");
      let state = yield promiseOneMessage("Test:FullscreenChanged");
      ok(state, "The content should have entered fullscreen");
      ok(document.fullscreenElement,
         "The chrome should also be in fullscreen");
    });

    info("Dispatch untrusted key events from content");
    yield* temporaryRemoveUnexpectedKeyEventCapture(function* () {
      let promiseExpectedKeyEvents = receiveExpectedKeyEvents(keyCode);
      gMessageManager.sendAsyncMessage("Test:DispatchUntrustedKeyEvents", code);
      yield promiseExpectedKeyEvents;
    });

    info("Send trusted key events");
    yield* temporaryRemoveUnexpectedFullscreenChangeCapture(function* () {
      yield* temporaryRemoveUnexpectedKeyEventCapture(function* () {
        let promiseExpectedKeyEvents = suppressed ?
          Promise.resolve() : receiveExpectedKeyEvents(keyCode);
        EventUtils.synthesizeKey(code, {});
        yield promiseExpectedKeyEvents;
        let state = yield promiseOneMessage("Test:FullscreenChanged");
        ok(!state, "The content should have exited fullscreen");
        ok(!document.fullscreenElement,
           "The chrome should also have exited fullscreen");
      });
    });
  }
});
