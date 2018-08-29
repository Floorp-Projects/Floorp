// Verify that the title doesn't flicker if the icon takes too long to load.
// We expect to see events in the following order:
// "label" added to tab
// "busy" removed from tab
// icon available
// In all those cases the title should be in the same position.

function waitForAttributeChange(tab, attr) {
  info(`Waiting for attribute ${attr}`);
  return new Promise(resolve => {
    let listener = (event) => {
      if (event.detail.changed.includes(attr)) {
        tab.removeEventListener("TabAttrModified", listener);
        resolve();
      }
    };

    tab.addEventListener("TabAttrModified", listener);
  });
}

add_task(async () => {
  const testPath = "http://example.com/browser/browser/base/content/test/favicons/";

  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" }, async (browser) => {
    let tab = gBrowser.getTabForBrowser(browser);
    BrowserTestUtils.loadURI(browser, testPath + "file_with_slow_favicon.html");

    await waitForAttributeChange(tab, "label");
    ok(tab.hasAttribute("busy"), "Should have seen the busy attribute");
    let label = document.getAnonymousElementByAttribute(tab, "anonid", "tab-label");
    let bounds = label.getBoundingClientRect();

    await waitForAttributeChange(tab, "busy");
    ok(!tab.hasAttribute("busy"), "Should have seen the busy attribute removed");
    let newBounds = label.getBoundingClientRect();
    is(bounds.x, newBounds.left, "Should have seen the title in the same place.");

    await waitForFaviconMessage(true);
    newBounds = label.getBoundingClientRect();
    is(bounds.x, newBounds.left, "Should have seen the title in the same place.");
  });
});
