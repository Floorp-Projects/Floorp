/* Make sure <link rel="..."> isn't respected in sub-frames. */

function test() {
  waitForExplicitFinish();

  let testPath = getRootDirectory(gTestPath);

  let tab = BrowserTestUtils.addTab(gBrowser, testPath + "file_bug970276_popup1.html");

  tab.linkedBrowser.addEventListener("load", function() {
    let expectedIcon = testPath + "file_bug970276_favicon1.ico";
    let icon;

    // Because there is debounce logic in ContentLinkHandler.jsm to reduce the
    // favicon loads, we have to wait some time before checking that icon was
    // stored properly.
    BrowserTestUtils.waitForCondition(() => {
      icon = gBrowser.getIcon(tab);
      return icon != null;
    }, "wait for favicon load to finish", 100, 5)
    .then(() => {
      is(icon, expectedIcon, "Correct icon.");
    })
    .catch(() => {
      ok(false, "Can't get the correct icon.");
    })
    .then(() => {
      gBrowser.removeTab(tab);
      finish();
    });
  }, {capture: true, once: true});
}
