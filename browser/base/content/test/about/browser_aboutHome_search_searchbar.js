/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

ChromeUtils.import("resource://testing-common/CustomizableUITestUtils.jsm", this);
let gCUITestUtils = new CustomizableUITestUtils(window);

ignoreAllUncaughtExceptions();

add_task(async function test_setup() {
  await gCUITestUtils.addSearchBar();
  registerCleanupFunction(() => {
    gCUITestUtils.removeSearchBar();
  });
});

add_task(async function() {
  info("Cmd+k should focus the search box in the toolbar when it's present");

  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:home" }, async function(browser) {
    await BrowserTestUtils.synthesizeMouseAtCenter("#brandLogo", {}, browser);

    let doc = window.document;
    let searchInput = BrowserSearch.searchBar.textbox.inputField;
    isnot(searchInput, doc.activeElement, "Search bar should not be the active element.");

    EventUtils.synthesizeKey("k", { accelKey: true });
    await TestUtils.waitForCondition(() => doc.activeElement === searchInput);
    is(searchInput, doc.activeElement, "Search bar should be the active element.");
  });
});
