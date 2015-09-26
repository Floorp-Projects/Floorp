function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function () {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    is(document.getElementById("identity-box").className,
       "unknownIdentity mixedDisplayContent",
       "identity box has class name for mixed content");

    gBrowser.removeCurrentTab();
    finish();
  }, true);

  content.location = "https://example.com/browser/browser/base/content/test/general/test_bug435035.html";
}
