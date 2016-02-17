function test() {
  waitForExplicitFinish();

  let testPath = getRootDirectory(gTestPath);

  let tab = gBrowser.addTab(testPath + "file_with_favicon.html");

  tab.linkedBrowser.addEventListener("DOMContentLoaded", function() {
    tab.linkedBrowser.removeEventListener("DOMContentLoaded", arguments.callee, true);

    let expectedIcon = testPath + "file_generic_favicon.ico";

    is(gBrowser.getIcon(tab), expectedIcon, "Correct icon before pushState.");
    tab.linkedBrowser.contentWindow.history.pushState("page2", "page2", "page2");
    is(gBrowser.getIcon(tab), expectedIcon, "Correct icon after pushState.");

    gBrowser.removeTab(tab);

    finish();
  }, true);
}
