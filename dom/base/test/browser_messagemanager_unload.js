function frameScript() {
  function eventHandler(e) {
    if (!docShell) {
      sendAsyncMessage("Test:Fail", "docShell is null");
    }

    sendAsyncMessage("Test:Event", [
      e.type,
      e.target === content.document,
      e.eventPhase,
    ]);
  }

  let outerID = content.docShell.outerWindowID;
  function onOuterWindowDestroyed(subject, topic, data) {
    if (docShell) {
      sendAsyncMessage("Test:Fail", "docShell is non-null");
    }

    let id = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
    sendAsyncMessage("Test:Event", ["outer-window-destroyed", id == outerID]);
    if (id == outerID) {
      Services.obs.removeObserver(
        onOuterWindowDestroyed,
        "outer-window-destroyed"
      );
    }
  }

  let url =
    "https://example.com/browser/dom/base/test/file_messagemanager_unload.html";

  content.location = url;
  addEventListener(
    "load",
    e => {
      if (e.target.location != url) {
        return;
      }

      addEventListener("unload", eventHandler, false);
      addEventListener("unload", eventHandler, true);
      addEventListener("pagehide", eventHandler, false);
      addEventListener("pagehide", eventHandler, true);
      Services.obs.addObserver(
        onOuterWindowDestroyed,
        "outer-window-destroyed"
      );

      sendAsyncMessage("Test:Ready");
    },
    true
  );
}

const EXPECTED = [
  // Unload events on the BrowserChildGlobal. These come first so that the
  // docshell is available.
  ["unload", false, 2],
  ["unload", false, 2],

  // pagehide and unload events for the top-level page.
  ["pagehide", true, 1],
  ["pagehide", true, 3],
  ["unload", true, 1],

  // pagehide and unload events for the iframe.
  ["pagehide", false, 1],
  ["pagehide", false, 3],
  ["unload", false, 1],

  // outer-window-destroyed for both pages.
  ["outer-window-destroyed", false],
  ["outer-window-destroyed", true],
];

function test() {
  waitForExplicitFinish();

  var newTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  gBrowser.selectedTab = newTab;

  let browser = newTab.linkedBrowser;
  let frameLoader = browser.frameLoader;
  ok(frameLoader !== null, "frameLoader looks okay");

  browser.messageManager.loadFrameScript(
    "data:,(" + frameScript.toString() + ")()",
    false
  );

  browser.messageManager.addMessageListener(
    "Test:Fail",
    msg => {
      ok(false, msg.data);
    },
    true
  );

  let index = 0;
  browser.messageManager.addMessageListener(
    "Test:Event",
    msg => {
      ok(msg.target === browser, "<browser> is correct");
      ok(msg.targetFrameLoader === frameLoader, "frameLoader is correct");
      ok(
        browser.frameLoader === null,
        "browser frameloader null during teardown"
      );

      info(JSON.stringify(msg.data));

      is(
        JSON.stringify(msg.data),
        JSON.stringify(EXPECTED[index]),
        "results match"
      );
      index++;

      if (index == EXPECTED.length) {
        finish();
      }
    },
    true
  );

  browser.messageManager.addMessageListener("Test:Ready", () => {
    info("Got ready message");
    gBrowser.removeCurrentTab();
  });
}
