/**
 * Bug 1685807 - Testing that the window.name won't be reset when loading an
 *               about:blank page to a window which had loaded non-about:blank
 *               page. And other case that window.name should be reset if
 *               the document.domain has changed.
 */

"use strict";

const EMPTY_URI =
  "https://test1.example.com/browser/dom/tests/browser/file_empty.html";
const TEST_URI =
  "https://test1.example.com/browser/dom/tests/browser/file_bug1685807.html";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.window.name.update.enabled", true]],
  });
});

add_task(async function doTests() {
  for (let testDocDomain of [false, true]) {
    // Open an empty tab.
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, EMPTY_URI);
    let browser = tab.linkedBrowser;

    // Create a promise in order to wait loading of the about:blank page.
    let loadedPromise = BrowserTestUtils.browserLoaded(
      browser,
      false,
      "about:blank"
    );

    // Set the window.name and document.domain.
    SpecialPowers.spawn(
      browser,
      [TEST_URI, testDocDomain],
      (aTestURI, aTestDocDomain) => {
        content.name = "Test";

        if (aTestDocDomain) {
          content.document.domain = "example.com";
        }

        // Open the page which will trigger the loading of the about:blank page.
        content.open(aTestURI);
      }
    );

    // Wait until the about:blank page is loaded.
    await loadedPromise;

    // Check the window.name.
    await SpecialPowers.spawn(browser, [testDocDomain], aTestDocDomain => {
      if (aTestDocDomain) {
        // The window.name should be reset if the document.domain was set to a
        // cross-origin.
        is(content.name, "", "The window.name should be reset.");
      } else {
        is(content.name, "Test", "The window.name shouldn't be reset.");
      }
    });

    let awaitPageShow = BrowserTestUtils.waitForContentEvent(
      browser,
      "pageshow"
    );
    browser.goBack();
    await awaitPageShow;

    // Check the window.name.
    await SpecialPowers.spawn(browser, [], () => {
      is(content.name, "Test", "The window.name is correct.");
    });

    BrowserTestUtils.removeTab(tab);
  }
});
