function test() {
  waitForExplicitFinish();

  let testPath = getRootDirectory(gTestPath);

  let tab = gBrowser.addTab(testPath + "file_with_favicon.html");

  tab.linkedBrowser.addEventListener("DOMContentLoaded", function() {
    tab.linkedBrowser.removeEventListener("DOMContentLoaded", arguments.callee, true);

    let expectedIcon = testPath + "file_generic_favicon.ico";

    is(gBrowser.getIcon(tab), expectedIcon, "Correct icon before hash change.");
    tab.linkedBrowser.contentWindow.location.href += "#foo";
    is(gBrowser.getIcon(tab), expectedIcon, "Correct icon after hash change.");

    gBrowser.removeTab(tab);

    finish();
  }, true);
}
