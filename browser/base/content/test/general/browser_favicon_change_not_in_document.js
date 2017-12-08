"use strict";

const TEST_URL = "http://mochi.test:8888/browser/browser/base/content/test/general/file_favicon_change_not_in_document.html";

/*
 * This test tests a link element won't fire DOMLinkChanged/DOMLinkAdded unless
 * it is added to the DOM. See more details in bug 1083895.
 *
 * Note that there is debounce logic in ContentLinkHandler.jsm, adding a new
 * icon link after the icon parsing timeout will trigger a new icon extraction
 * cycle. Hence, there should be two favicons loads in this test as it appends
 * a new link to the DOM in the timeout callback defined in the test HTML page.
 * However, the not-yet-added link element with href as "http://example.org/other-icon"
 * should not fire the DOMLinkAdded event, nor should it fire the DOMLinkChanged
 * event after its href gets updated later.
 */
add_task(async function() {
  let extraTab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  let domLinkAddedFired = 0;
  let domLinkChangedFired = 0;
  const linkAddedHandler = event => domLinkAddedFired++;
  const linkChangedhandler = event => domLinkChangedFired++;
  BrowserTestUtils.addContentEventListener(gBrowser.selectedBrowser, "DOMLinkAdded", linkAddedHandler);
  BrowserTestUtils.addContentEventListener(gBrowser.selectedBrowser, "DOMLinkChanged", linkChangedhandler);
  extraTab.linkedBrowser.loadURI(TEST_URL);
  let expectedFavicon = "http://example.org/yet-another-icon";
  await promiseTabLoaded(extraTab);

  // Make sure the new added favicon link gets loaded.
  try {
    await BrowserTestUtils.waitForCondition(() => {
      return gBrowser.getIcon(extraTab) === expectedFavicon;
    }, "wait for favicon load to finish", 1000, 5);
    ok(true, "Should load the added favicon");
  } catch (e) {
    ok(false, "Should've loaded the new added favicon.");
  }

  is(domLinkAddedFired, 2, "Should fire the correct number of DOMLinkAdded event.");
  is(domLinkChangedFired, 0, "Should not fire any DOMLinkChanged event.");

  gBrowser.removeTab(extraTab);
});
