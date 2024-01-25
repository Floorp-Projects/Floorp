function frameScript() {
  sendSyncMessage("Test:Message");
  sendAsyncMessage("Test:Message");
  sendAsyncMessage("Test:Done");
}

function test() {
  waitForExplicitFinish();

  var newTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  gBrowser.selectedTab = newTab;

  let browser = newTab.linkedBrowser;
  let frameLoader = browser.frameLoader;
  Assert.notStrictEqual(frameLoader, null, "frameLoader looks okay");

  browser.messageManager.loadFrameScript(
    "data:,(" + frameScript.toString() + ")()",
    false
  );

  browser.messageManager.addMessageListener("Test:Message", msg => {
    Assert.strictEqual(msg.target, browser, "<browser> is correct");
    Assert.strictEqual(
      msg.targetFrameLoader,
      frameLoader,
      "frameLoader is correct"
    );
    Assert.strictEqual(
      browser.frameLoader,
      msg.targetFrameLoader,
      "browser frameloader is correct"
    );
  });

  browser.messageManager.addMessageListener("Test:Done", () => {
    info("Finished");
    gBrowser.removeCurrentTab();
    finish();
  });
}
