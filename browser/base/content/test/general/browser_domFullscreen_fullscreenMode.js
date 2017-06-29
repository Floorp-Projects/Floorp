/* eslint-env mozilla/frame-script */

"use strict";

var gMessageManager;

function frameScript() {
  addMessageListener("Test:RequestFullscreen", () => {
    content.document.body.requestFullscreen();
  });
  addMessageListener("Test:ExitFullscreen", () => {
    content.document.exitFullscreen();
  });
  addMessageListener("Test:QueryFullscreenState", () => {
    sendAsyncMessage("Test:FullscreenState", {
      inDOMFullscreen: !!content.document.fullscreenElement,
      inFullscreen: content.fullScreen
    });
  });
  addMessageListener("Test:WaitActivated", () => {
    waitUntilActive();
  });
  content.document.addEventListener("fullscreenchange", () => {
    sendAsyncMessage("Test:FullscreenChanged", {
      inDOMFullscreen: !!content.document.fullscreenElement,
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

function waitForDocActivated() {
  return new Promise(resolve => {
    listenOneMessage("Test:Activated", resolve);
    gMessageManager.sendAsyncMessage("Test:WaitActivated");
  });
}

function waitForFullscreenChanges(aFlags) {
  return new Promise(resolve => {
    let fullscreenData = null;
    let sizemodeChanged = false;
    function tryResolve() {
      if ((!(aFlags & FS_CHANGE_DOM) || fullscreenData) &&
          (!(aFlags & FS_CHANGE_SIZE) || sizemodeChanged)) {
        // In the platforms that support reporting occlusion state (e.g. Mac),
        // enter/exit fullscreen mode will trigger docshell being set to
        // non-activate and then set to activate back again.
        // For those platform, we should wait until the docshell has been
        // activated again, otherwise, the fullscreen request might be denied.
        waitForDocActivated().then(() => {
          if (!fullscreenData) {
            queryFullscreenState().then(resolve);
          } else {
            resolve(fullscreenData);
          }
        });
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

var gTests = [
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
    exitFunc() {
      executeSoon(() => EventUtils.synthesizeKey("VK_F11", {}));
    }
  }
];

function checkState(expectedStates, contentStates) {
  is(contentStates.inDOMFullscreen, expectedStates.inDOMFullscreen,
     "The DOM fullscreen state of the content should match");
  // TODO window.fullScreen is not updated as soon as the fullscreen
  //      state flips in child process, hence checking it could cause
  //      anonying intermittent failure. As we just want to confirm the
  //      fullscreen state of the browser window, we can just check the
  //      that on the chrome window below.
  // is(contentStates.inFullscreen, expectedStates.inFullscreen,
  //    "The fullscreen state of the content should match");
  is(!!document.fullscreenElement, expectedStates.inDOMFullscreen,
     "The DOM fullscreen state of the chrome should match");
  is(window.fullScreen, expectedStates.inFullscreen,
     "The fullscreen state of the chrome should match");
}

const kPage = "http://example.org/browser/browser/" +
              "base/content/test/general/dummy_page.html";

add_task(async function() {
  await pushPrefs(
    ["full-screen-api.transition-duration.enter", "0 0"],
    ["full-screen-api.transition-duration.leave", "0 0"]);

  registerCleanupFunction(async function() {
    if (window.fullScreen) {
      executeSoon(() => BrowserFullScreen());
      await waitForFullscreenChanges(FS_CHANGE_SIZE);
    }
  });

  let tab = await BrowserTestUtils.openNewForegroundTab({ gBrowser, url: kPage });
  let browser = tab.linkedBrowser;

  gMessageManager = browser.messageManager;
  gMessageManager.loadFrameScript(
    "data:,(" + frameScript.toString() + ")();", false);
  gMessageManager.addMessageListener(
    "Test:FullscreenChanged", captureUnexpectedFullscreenChange);

  // Wait for the document being activated, so that
  // fullscreen request won't be denied.
  await new Promise(resolve => listenOneMessage("Test:Activated", resolve));

  for (let test of gTests) {
    let contentStates;
    info("Testing exit DOM fullscreen via " + test.desc);

    contentStates = await queryFullscreenState();
    checkState({inDOMFullscreen: false, inFullscreen: false}, contentStates);

    /* DOM fullscreen without fullscreen mode */

    info("> Enter DOM fullscreen");
    gMessageManager.sendAsyncMessage("Test:RequestFullscreen");
    contentStates = await waitForFullscreenChanges(FS_CHANGE_BOTH);
    checkState({inDOMFullscreen: true, inFullscreen: true}, contentStates);

    info("> Exit DOM fullscreen");
    test.exitFunc();
    contentStates = await waitForFullscreenChanges(FS_CHANGE_BOTH);
    checkState({inDOMFullscreen: false, inFullscreen: false}, contentStates);

    /* DOM fullscreen with fullscreen mode */

    info("> Enter fullscreen mode");
    // Need to be asynchronous because sizemodechange event could be
    // dispatched synchronously, which would cause the event listener
    // miss that event and wait infinitely.
    executeSoon(() => BrowserFullScreen());
    contentStates = await waitForFullscreenChanges(FS_CHANGE_SIZE);
    checkState({inDOMFullscreen: false, inFullscreen: true}, contentStates);

    info("> Enter DOM fullscreen in fullscreen mode");
    gMessageManager.sendAsyncMessage("Test:RequestFullscreen");
    contentStates = await waitForFullscreenChanges(FS_CHANGE_DOM);
    checkState({inDOMFullscreen: true, inFullscreen: true}, contentStates);

    info("> Exit DOM fullscreen in fullscreen mode");
    test.exitFunc();
    contentStates = await waitForFullscreenChanges(
      test.affectsFullscreenMode ? FS_CHANGE_BOTH : FS_CHANGE_DOM);
    checkState({
      inDOMFullscreen: false,
      inFullscreen: !test.affectsFullscreenMode
    }, contentStates);

    /* Cleanup */

    // Exit fullscreen mode if we are still in
    if (window.fullScreen) {
      info("> Cleanup");
      executeSoon(() => BrowserFullScreen());
      await waitForFullscreenChanges(FS_CHANGE_SIZE);
    }
  }

  await BrowserTestUtils.removeTab(tab);
});
