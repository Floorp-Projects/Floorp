function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function () {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    is(document.getElementById("identity-box").className,
       gIdentityHandler.IDENTITY_MODE_MIXED_CONTENT,
       "identity box has class name for mixed content");

    gBrowser.removeCurrentTab();
    finish();
  }, true);

  content.location = "https://example.com/browser/browser/base/content/test/test_bug435035.html";
}
