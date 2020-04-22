/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_ORIGIN = "https://example.com";
const TEST_SUB_ORIGIN = "https://test1.example.com";
const REMOVE_DIALOG_URL =
  "chrome://browser/content/preferences/dialogs/siteDataRemoveSelected.xhtml";

// Greek IDN for 'example.test'.
const TEST_IDN_ORIGIN =
  "https://\u03C0\u03B1\u03C1\u03AC\u03B4\u03B5\u03B9\u03B3\u03BC\u03B1.\u03B4\u03BF\u03BA\u03B9\u03BC\u03AE";
const TEST_PUNY_ORIGIN = "https://xn--hxajbheg2az3al.xn--jxalpdlp/";
const TEST_PUNY_SUB_ORIGIN = "https://sub1.xn--hxajbheg2az3al.xn--jxalpdlp/";

ChromeUtils.defineModuleGetter(
  this,
  "SiteDataTestUtils",
  "resource://testing-common/SiteDataTestUtils.jsm"
);

async function testClearing(
  testQuota,
  testCookies,
  testURI,
  origin,
  subOrigin
) {
  // Add some test quota storage.
  if (testQuota) {
    await SiteDataTestUtils.addToIndexedDB(origin);
    await SiteDataTestUtils.addToIndexedDB(subOrigin);
  }

  // Add some test cookies.
  if (testCookies) {
    SiteDataTestUtils.addToCookies(origin, "test1", "1");
    SiteDataTestUtils.addToCookies(origin, "test2", "2");
    SiteDataTestUtils.addToCookies(subOrigin, "test3", "1");
  }

  await BrowserTestUtils.withNewTab(testURI, async function(browser) {
    // Verify we have added quota storage.
    if (testQuota) {
      let usage = await SiteDataTestUtils.getQuotaUsage(origin);
      Assert.greater(usage, 0, "Should have data for the base origin.");

      usage = await SiteDataTestUtils.getQuotaUsage(subOrigin);
      Assert.greater(usage, 0, "Should have data for the sub origin.");
    }

    // Open the identity popup.
    let { gIdentityHandler } = gBrowser.ownerGlobal;
    let promisePanelOpen = BrowserTestUtils.waitForEvent(
      gIdentityHandler._identityPopup,
      "popupshown"
    );
    gIdentityHandler._identityBox.click();
    await promisePanelOpen;

    let clearFooter = document.getElementById(
      "identity-popup-clear-sitedata-footer"
    );
    let clearButton = document.getElementById(
      "identity-popup-clear-sitedata-button"
    );
    TestUtils.waitForCondition(
      () => !clearFooter.hidden,
      "The clear data footer is not hidden."
    );

    let cookiesCleared;
    if (testCookies) {
      cookiesCleared = Promise.all([
        TestUtils.topicObserved(
          "cookie-changed",
          (subj, data) => data == "deleted" && subj.name == "test1"
        ),
        TestUtils.topicObserved(
          "cookie-changed",
          (subj, data) => data == "deleted" && subj.name == "test2"
        ),
        TestUtils.topicObserved(
          "cookie-changed",
          (subj, data) => data == "deleted" && subj.name == "test3"
        ),
      ]);
    }

    // Click the "Clear data" button.
    let siteDataUpdated = TestUtils.topicObserved(
      "sitedatamanager:sites-updated"
    );
    let hideEvent = BrowserTestUtils.waitForEvent(
      gIdentityHandler._identityPopup,
      "popuphidden"
    );
    let removeDialogPromise = BrowserTestUtils.promiseAlertDialogOpen(
      "accept",
      REMOVE_DIALOG_URL
    );
    clearButton.click();
    await hideEvent;
    await removeDialogPromise;

    await siteDataUpdated;

    // Check that cookies were deleted.
    if (testCookies) {
      await cookiesCleared;
      let uri = Services.io.newURI(origin);
      is(
        Services.cookies.countCookiesFromHost(uri.host),
        0,
        "Cookies from the base domain should be cleared"
      );
      uri = Services.io.newURI(subOrigin);
      is(
        Services.cookies.countCookiesFromHost(uri.host),
        0,
        "Cookies from the sub domain should be cleared"
      );
    }

    // Check that quota storage was deleted.
    if (testQuota) {
      await TestUtils.waitForCondition(async () => {
        let usage = await SiteDataTestUtils.getQuotaUsage(origin);
        return usage == 0;
      }, "Should have no data for the base origin.");

      let usage = await SiteDataTestUtils.getQuotaUsage(subOrigin);
      is(usage, 0, "Should have no data for the sub origin.");
    }

    // Open the site identity panel again to check that the button isn't shown anymore.
    promisePanelOpen = BrowserTestUtils.waitForEvent(
      gIdentityHandler._identityPopup,
      "popupshown"
    );
    gIdentityHandler._identityBox.click();
    await promisePanelOpen;

    // Wait for a second to see if the button is shown.
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(c => setTimeout(c, 1000));

    ok(
      clearFooter.hidden,
      "The clear data footer is hidden after clearing data."
    );
  });
}

// Test removing quota managed storage.
add_task(async function test_ClearSiteData() {
  await testClearing(true, false, TEST_ORIGIN, TEST_ORIGIN, TEST_SUB_ORIGIN);
});

// Test removing cookies.
add_task(async function test_ClearCookies() {
  await testClearing(false, true, TEST_ORIGIN, TEST_ORIGIN, TEST_SUB_ORIGIN);
});

// Test removing both.
add_task(async function test_ClearCookiesAndSiteData() {
  await testClearing(true, true, TEST_ORIGIN, TEST_ORIGIN, TEST_SUB_ORIGIN);
});

// Test IDN Domains
add_task(async function test_IDN_ClearCookiesAndSiteData() {
  await testClearing(
    true,
    true,
    TEST_IDN_ORIGIN,
    TEST_PUNY_ORIGIN,
    TEST_PUNY_SUB_ORIGIN
  );
});
