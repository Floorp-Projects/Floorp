"use strict";

let gMessageManager;

function frameScript() {
  addMessageListener("Test:RequestFullscreen", () => {
    content.document.body.mozRequestFullScreen();
  });
  addMessageListener("Test:ExitFullscreen", () => {
    content.document.mozCancelFullScreen();
  });
  addMessageListener("Test:QueryFullscreenState", () => {
    sendAsyncMessage("Test:FullscreenState", {
      inDOMFullscreen: content.document.mozFullScreen,
      inFullscreen: content.fullScreen
    });
  });
  content.document.addEventListener("mozfullscreenchange", () => {
    sendAsyncMessage("Test:FullscreenChanged", {
      inDOMFullscreen: content.document.mozFullScreen,
      inFullscreen: content.fullScreen
    });
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

function listenOneMessage(aMsg, aListener) {
  function listener({ data }) {
    gMessageManager.removeMessageListener(aMsg, listener);
    aListener(data);
  }
  gMessageManager.addMessageListener(aMsg, listener);
}

function listenOneEvent(aEvent, aListener) {
  function listener(evt) {
    removeEventListener(aEvent, listener);
    aListener(evt);
  }
  addEventListener(aEvent, listener);
}

function queryFullscreenState() {
  return new Promise(resolve => {
    listenOneMessage("Test:FullscreenState", resolve);
    gMessageManager.sendAsyncMessage("Test:QueryFullscreenState");
  });
}

function captureUnexpectedFullscreenChange() {
  ok(false, "catched an unexpected fullscreen change");
}

const FS_CHANGE_DOM = 1 << 0;
const FS_CHANGE_SIZE = 1 << 1;
const FS_CHANGE_BOTH = FS_CHANGE_DOM | FS_CHANGE_SIZE;

function waitForFullscreenChanges(aFlags) {
  return new Promise(resolve => {
    let fullscreenData = null;
    let sizemodeChanged = false;
    function tryResolve() {
      if ((!(aFlags & FS_CHANGE_DOM) || fullscreenData) &&
          (!(aFlags & FS_CHANGE_SIZE) || sizemodeChanged)) {
        if (!fullscreenData) {
          queryFullscreenState().then(resolve);
        } else {
          resolve(fullscreenData);
        }
      }
    }
    if (aFlags & FS_CHANGE_SIZE) {
      listenOneEvent("sizemodechange", () => {
        sizemodeChanged = true;
        tryResolve();
      });
    }
    if (aFlags & FS_CHANGE_DOM) {
      gMessageManager.removeMessageListener(
        "Test:FullscreenChanged", captureUnexpectedFullscreenChange);
      listenOneMessage("Test:FullscreenChanged", data => {
        gMessageManager.addMessageListener(
          "Test:FullscreenChanged", captureUnexpectedFullscreenChange);
        fullscreenData = data;
        tryResolve();
      });
    }
  });
}

let gTests = [
  {
    desc: "document method",
    affectsFullscreenMode: false,
    exitFunc: () => {
      gMessageManager.sendAsyncMessage("Test:ExitFullscreen");
    }
  },
  {
    desc: "escape key",
    affectsFullscreenMode: false,
    exitFunc: () => {
      executeSoon(() => EventUtils.synthesizeKey("VK_ESCAPE", {}));
    }
  },
  {
    desc: "F11 key",
    affectsFullscreenMode: true,
    exitFunc: function () {
      executeSoon(() => EventUtils.synthesizeKey("VK_F11", {}));
    }
  }
];

add_task(function* () {
  let tab = gBrowser.addTab("about:robots");
  let browser = tab.linkedBrowser;
  gBrowser.selectedTab = tab;
  yield waitForDocLoadComplete();

  registerCleanupFunction(() => {
    if (browser.contentWindow.fullScreen) {
      BrowserFullScreen();
    }
    gBrowser.removeTab(tab);
  });

  gMessageManager = browser.messageManager;
  gMessageManager.loadFrameScript(
    "data:,(" + frameScript.toString() + ")();", false);
  gMessageManager.addMessageListener(
    "Test:FullscreenChanged", captureUnexpectedFullscreenChange);

  // Wait for the document being activated, so that
  // fullscreen request won't be denied.
  yield new Promise(resolve => listenOneMessage("Test:Activated", resolve));

  for (let test of gTests) {
    info("Testing exit DOM fullscreen via " + test.desc);

    var { inDOMFullscreen, inFullscreen } = yield queryFullscreenState();
    ok(!inDOMFullscreen, "Shouldn't have been in DOM fullscreen");
    ok(!inFullscreen, "Shouldn't have been in fullscreen");

    /* DOM fullscreen without fullscreen mode */

    // Enter DOM fullscreen
    gMessageManager.sendAsyncMessage("Test:RequestFullscreen");
    var { inDOMFullscreen, inFullscreen } =
      yield waitForFullscreenChanges(FS_CHANGE_BOTH);
    ok(inDOMFullscreen, "Should now be in DOM fullscreen");
    ok(inFullscreen, "Should now be in fullscreen");

    // Exit DOM fullscreen
    test.exitFunc();
    var { inDOMFullscreen, inFullscreen } =
      yield waitForFullscreenChanges(FS_CHANGE_BOTH);
    ok(!inDOMFullscreen, "Should no longer be in DOM fullscreen");
    ok(!inFullscreen, "Should no longer be in fullscreen");

    /* DOM fullscreen with fullscreen mode */

    // Enter fullscreen mode
    // Need to be asynchronous because sizemodechange event could be
    // dispatched synchronously, which would cause the event listener
    // miss that event and wait infinitely.
    executeSoon(() => BrowserFullScreen());
    var { inDOMFullscreen, inFullscreen } =
      yield waitForFullscreenChanges(FS_CHANGE_SIZE);
    ok(!inDOMFullscreen, "Shouldn't have been in DOM fullscreen");
    ok(inFullscreen, "Should now be in fullscreen mode");

    // Enter DOM fullscreen
    gMessageManager.sendAsyncMessage("Test:RequestFullscreen");
    var { inDOMFullscreen, inFullscreen } =
      yield waitForFullscreenChanges(FS_CHANGE_DOM);
    ok(inDOMFullscreen, "Should now be in DOM fullscreen");
    ok(inFullscreen, "Should still be in fullscreen");

    // Exit DOM fullscreen
    test.exitFunc();
    var { inDOMFullscreen, inFullscreen } =
      yield waitForFullscreenChanges(test.affectsFullscreenMode ?
                                     FS_CHANGE_BOTH : FS_CHANGE_DOM);
    ok(!inDOMFullscreen, "Should no longer be in DOM fullscreen");
    if (test.affectsFullscreenMode) {
      ok(!inFullscreen, "Should no longer be in fullscreen mode");
    } else {
      ok(inFullscreen, "Should still be in fullscreen mode");
    }

    /* Cleanup */

    // Exit fullscreen mode if we are still in
    if (browser.contentWindow.fullScreen) {
      executeSoon(() => BrowserFullScreen());
      yield waitForFullscreenChanges(FS_CHANGE_SIZE);
    }
  }
});
