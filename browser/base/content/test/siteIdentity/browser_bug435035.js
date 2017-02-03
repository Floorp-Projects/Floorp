const TEST_URL = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "https://example.com") + "test_bug435035.html";

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(() => {
    is(document.getElementById("identity-box").className,
       "unknownIdentity mixedDisplayContent",
       "identity box has class name for mixed content");

    gBrowser.removeCurrentTab();
    finish();
  });

  gBrowser.loadURI(TEST_URL);
}
