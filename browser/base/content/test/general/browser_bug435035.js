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

  gBrowser.loadURI(
    "https://example.com/browser/browser/base/content/test/general/test_bug435035.html"
  );
}
