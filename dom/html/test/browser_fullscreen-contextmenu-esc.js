"use strict";

function frameScript() {
  addMessageListener("Test:RequestFullscreen", () => {
    content.document.body.requestFullscreen();
  });
  content.document.addEventListener("fullscreenchange", () => {
    sendAsyncMessage("Test:FullscreenChanged",
                     !!content.document.fullscreenElement);
  });
  addMessageListener("Test:QueryFullscreenState", () => {
    sendAsyncMessage("Test:FullscreenState",
                     !!content.document.fullscreenElement);
  });
  function waitUntilActive() {
    let doc = content.document;
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

const kPage = "http://example.org/browser/dom/html/test/dummy_page.html";

add_task(function* () {
  yield pushPrefs(
    ["full-screen-api.transition-duration.enter", "0 0"],
    ["full-screen-api.transition-duration.leave", "0 0"]);

  let tab = BrowserTestUtils.addTab(gBrowser, kPage);
  registerCleanupFunction(() => gBrowser.removeTab(tab));
  let browser = tab.linkedBrowser;
  gBrowser.selectedTab = tab;
  yield waitForDocLoadComplete();

  gMessageManager = browser.messageManager;
  gMessageManager.loadFrameScript(
    "data:,(" + frameScript.toString() + ")();", false);

  // Wait for the document being activated, so that
  // fullscreen request won't be denied.
  yield promiseOneMessage("Test:Activated");

  let contextMenu = document.getElementById("contentAreaContextMenu");
  ok(contextMenu, "Got context menu");

  let state;
  info("Enter DOM fullscreen");
  gMessageManager.sendAsyncMessage("Test:RequestFullscreen");
  state = yield promiseOneMessage("Test:FullscreenChanged");
  ok(state, "The content should have entered fullscreen");
  ok(document.fullscreenElement, "The chrome should also be in fullscreen");
  gMessageManager.addMessageListener(
    "Test:FullscreenChanged", captureUnexpectedFullscreenChange);

  info("Open context menu");
  is(contextMenu.state, "closed", "Should not have opened context menu");
  let popupShownPromise = promiseWaitForEvent(window, "popupshown");
  EventUtils.synthesizeMouse(browser, screen.width / 2, screen.height / 2,
                             {type: "contextmenu", button: 2}, window);
  yield popupShownPromise;
  is(contextMenu.state, "open", "Should have opened context menu");

  info("Send the first escape");
  let popupHidePromise = promiseWaitForEvent(window, "popuphidden");
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield popupHidePromise;
  is(contextMenu.state, "closed", "Should have closed context menu");

  // Wait a small time to confirm that the first ESC key
  // does not exit fullscreen.
  yield new Promise(resolve => setTimeout(resolve, 1000));
  gMessageManager.sendAsyncMessage("Test:QueryFullscreenState");
  state = yield promiseOneMessage("Test:FullscreenState");
  ok(state, "The content should still be in fullscreen");
  ok(document.fullscreenElement, "The chrome should still be in fullscreen");

  info("Send the second escape");
  gMessageManager.removeMessageListener(
    "Test:FullscreenChanged", captureUnexpectedFullscreenChange);
  let fullscreenExitPromise = promiseOneMessage("Test:FullscreenChanged");
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  state = yield fullscreenExitPromise;
  ok(!state, "The content should have exited fullscreen");
  ok(!document.fullscreenElement, "The chrome should have exited fullscreen");
});
