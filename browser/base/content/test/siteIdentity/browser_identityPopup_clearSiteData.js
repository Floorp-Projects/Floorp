/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_ORIGIN = "https://example.com";
const TEST_SUB_ORIGIN = "https://test1.example.com";
const REMOVE_DIALOG_URL = "chrome://browser/content/preferences/siteDataRemoveSelected.xul";

ChromeUtils.defineModuleGetter(this, "SiteDataTestUtils",
                               "resource://testing-common/SiteDataTestUtils.jsm");

async function testClearing(testQuota, testCookies) {
  // Add some test quota storage.
  if (testQuota) {
    await SiteDataTestUtils.addToIndexedDB(TEST_ORIGIN);
    await SiteDataTestUtils.addToIndexedDB(TEST_SUB_ORIGIN);
  }

  // Add some test cookies.
  if (testCookies) {
    SiteDataTestUtils.addToCookies(TEST_ORIGIN, "test1", "1");
    SiteDataTestUtils.addToCookies(TEST_ORIGIN, "test2", "2");
    SiteDataTestUtils.addToCookies(TEST_SUB_ORIGIN, "test3", "1");
  }

  await BrowserTestUtils.withNewTab(TEST_ORIGIN, async function(browser) {
    // Verify we have added quota storage.
    if (testQuota) {
      let usage = await SiteDataTestUtils.getQuotaUsage(TEST_ORIGIN);
      Assert.greater(usage, 0, "Should have data for the base origin.");

      usage = await SiteDataTestUtils.getQuotaUsage(TEST_SUB_ORIGIN);
      Assert.greater(usage, 0, "Should have data for the sub origin.");
    }

    // Open the identity popup.
    let { gIdentityHandler } = gBrowser.ownerGlobal;
    let promisePanelOpen = BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "popupshown");
    let siteDataUpdated = TestUtils.topicObserved("sitedatamanager:sites-updated");
    gIdentityHandler._identityBox.click();
    await promisePanelOpen;
    await siteDataUpdated;

    let clearFooter = document.getElementById("identity-popup-clear-sitedata-footer");
    let clearButton = document.getElementById("identity-popup-clear-sitedata-button");
    ok(!clearFooter.hidden, "The clear data footer is not hidden.");

    let cookiesCleared;
    if (testCookies) {
      cookiesCleared = Promise.all([
        TestUtils.topicObserved("cookie-changed", (subj, data) => data == "deleted" && subj.name == "test1"),
        TestUtils.topicObserved("cookie-changed", (subj, data) => data == "deleted" && subj.name == "test2"),
        TestUtils.topicObserved("cookie-changed", (subj, data) => data == "deleted" && subj.name == "test3"),
      ]);
    }

    // Click the "Clear data" button.
    siteDataUpdated = TestUtils.topicObserved("sitedatamanager:sites-updated");
    let hideEvent = BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "popuphidden");
    let removeDialogPromise = BrowserTestUtils.promiseAlertDialogOpen("accept", REMOVE_DIALOG_URL);
    clearButton.click();
    await hideEvent;
    await removeDialogPromise;

    await siteDataUpdated;

    // Check that cookies were deleted.
    if (testCookies) {
      await cookiesCleared;
      let uri = Services.io.newURI(TEST_ORIGIN);
      is(Services.cookies.countCookiesFromHost(uri.host), 0, "Cookies from the base domain should be cleared");
      uri = Services.io.newURI(TEST_SUB_ORIGIN);
      is(Services.cookies.countCookiesFromHost(uri.host), 0, "Cookies from the sub domain should be cleared");
    }

    // Check that quota storage was deleted.
    if (testQuota) {
      await TestUtils.waitForCondition(async () => {
        let usage = await SiteDataTestUtils.getQuotaUsage(TEST_ORIGIN);
        return usage == 0;
      }, "Should have no data for the base origin.");

      let usage = await SiteDataTestUtils.getQuotaUsage(TEST_SUB_ORIGIN);
      is(usage, 0, "Should have no data for the sub origin.");
    }

    // Open the site identity panel again to check that the button isn't shown anymore.
    promisePanelOpen = BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "popupshown");
    siteDataUpdated = TestUtils.topicObserved("sitedatamanager:sites-updated");
    gIdentityHandler._identityBox.click();
    await promisePanelOpen;
    await siteDataUpdated;

    ok(clearFooter.hidden, "The clear data footer is hidden after clearing data.");
  });
}

// Test removing quota managed storage.
add_task(async function test_ClearSiteData() {
  await testClearing(true, false);
});

// Test removing cookies.
add_task(async function test_ClearCookies() {
  await testClearing(false, true);
});

// Test removing both.
add_task(async function test_ClearCookiesAndSiteData() {
  await testClearing(true, true);
});
