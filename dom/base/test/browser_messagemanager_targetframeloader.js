function frameScript()
{
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
  ok(frameLoader !== null, "frameLoader looks okay");

  browser.messageManager.loadFrameScript("data:,(" + frameScript.toString() + ")()", false);

  browser.messageManager.addMessageListener("Test:Message", (msg) => {
    ok(msg.target === browser, "<browser> is correct");
    ok(msg.targetFrameLoader === frameLoader, "frameLoader is correct");
    ok(browser.frameLoader === msg.targetFrameLoader, "browser frameloader is correct");
  });

  browser.messageManager.addMessageListener("Test:Done", () => {
    info("Finished");
    gBrowser.removeCurrentTab();
    finish();
  });
}
