/* Make sure <link rel="..."> isn't respected in sub-frames. */

function test() {
  waitForExplicitFinish();

  let testPath = getRootDirectory(gTestPath);

  let tab = BrowserTestUtils.addTab(gBrowser, testPath + "file_bug970276_popup1.html");

  tab.linkedBrowser.addEventListener("load", function() {
    let expectedIcon = testPath + "file_bug970276_favicon1.ico";
    is(gBrowser.getIcon(tab), expectedIcon, "Correct icon.");

    gBrowser.removeTab(tab);

    finish();
  }, {capture: true, once: true});
}
