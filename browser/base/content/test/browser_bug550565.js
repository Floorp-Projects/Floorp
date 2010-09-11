function test() {
  waitForExplicitFinish();

  let testPath = getRootDirectory(gTestPath);

  let tab = gBrowser.addTab(testPath + "file_bug550565_popup.html");

  tab.linkedBrowser.addEventListener('DOMContentLoaded', function() {
    let expectedIcon = testPath + "file_bug550565_favicon.ico";

    is(gBrowser.getIcon(tab), expectedIcon, "Correct icon before pushState.");
    tab.linkedBrowser.contentWindow.history.pushState("page2", "page2", "page2");
    is(gBrowser.getIcon(tab), expectedIcon, "Correct icon after pushState.");

    gBrowser.removeTab(tab);

    finish();
  }, true);
}
