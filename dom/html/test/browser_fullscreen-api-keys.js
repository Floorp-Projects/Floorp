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
    doc.body.mozRequestFullScreen();
  });
  addMessageListener("Test:DispatchUntrustedKeyEvents", msg => {
    var evt = new content.CustomEvent("Test:DispatchKeyEvents", {
      detail: { code: msg.data }
    });
    content.dispatchEvent(evt);
  });

  doc.addEventListener("mozfullscreenchange", () => {
    sendAsyncMessage("Test:FullscreenChanged", !!doc.mozFullScreenElement);
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

function* receiveExpectedKeyEvents(keyCode) {
  info("Waiting for key events");
  let events = ["keydown", "keypress", "keyup"];
  while (events.length > 0) {
    let evt = yield promiseOneMessage("Test:KeyReceived");
    let expected = events.shift();
    is(evt.type, expected, `Should receive a ${expected} event`);
    is(evt.keyCode, keyCode,
       `Should receive the event with key code ${keyCode}`);
  }
}

const kPage = "http://example.org/browser/" +
              "dom/html/test/file_fullscreen-api-keys.html";

add_task(function* () {
  yield pushPrefs(
    ["full-screen-api.transition-duration.enter", "0 0"],
    ["full-screen-api.transition-duration.leave", "0 0"]);

  let tab = gBrowser.addTab(kPage);
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
      ok(document.mozFullScreenElement,
         "The chrome should also be in fullscreen");
    });

    info("Dispatch untrusted key events from content");
    yield* temporaryRemoveUnexpectedKeyEventCapture(function* () {
      gMessageManager.sendAsyncMessage("Test:DispatchUntrustedKeyEvents", code);
      yield* receiveExpectedKeyEvents(keyCode);
    });

    info("Send trusted key events");
    yield* temporaryRemoveUnexpectedFullscreenChangeCapture(function* () {
      yield* temporaryRemoveUnexpectedKeyEventCapture(function* () {
        EventUtils.synthesizeKey(code, {});
        if (!suppressed) {
          yield* receiveExpectedKeyEvents(keyCode);
        }
        let state = yield promiseOneMessage("Test:FullscreenChanged");
        ok(!state, "The content should have exited fullscreen");
        ok(!document.mozFullScreenElement,
           "The chrome should also have exited fullscreen");
      });
    });
  }
});
