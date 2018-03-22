ChromeUtils.import("resource:///modules/SitePermissions.jsm");

ChromeUtils.defineModuleGetter(this, "SiteDataTestUtils",
                               "resource://testing-common/SiteDataTestUtils.jsm");
ChromeUtils.defineModuleGetter(this, "DownloadUtils",
                               "resource://gre/modules/DownloadUtils.jsm");

const TEST_ORIGIN = "https://example.com";
const TEST_SUB_ORIGIN = "https://test1.example.com";
const REMOVE_DIALOG_URL = "chrome://browser/content/preferences/siteDataRemoveSelected.xul";

// Test displaying and removing quota managed data.
add_task(async function test_SiteData() {
  await SiteDataTestUtils.addToIndexedDB(TEST_ORIGIN);

  await BrowserTestUtils.withNewTab(TEST_ORIGIN, async function(browser) {

    let totalUsage = await SiteDataTestUtils.getQuotaUsage(TEST_ORIGIN);
    Assert.greater(totalUsage, 0, "The total usage should not be 0");

    let pageInfo = BrowserPageInfo(TEST_ORIGIN, "securityTab");
    await BrowserTestUtils.waitForEvent(pageInfo, "load");

    let label = pageInfo.document.getElementById("security-privacy-sitedata-value");
    let clearButton = pageInfo.document.getElementById("security-clear-sitedata");

    let size = DownloadUtils.convertByteUnits(totalUsage);

    // The usage details are filled asynchronously, so we assert that they're present by
    // waiting for them to be filled in.
    // We only wait for the right unit to appear, since this number is intermittently
    // varying by slight amounts on infra machines.
    await BrowserTestUtils.waitForCondition(() => label.textContent.includes(size[1]),
      "Should show site data usage in the security section.");
    let siteDataUpdated = TestUtils.topicObserved("sitedatamanager:sites-updated");

    let removeDialogPromise = BrowserTestUtils.promiseAlertDialogOpen("accept", REMOVE_DIALOG_URL);
    clearButton.click();
    await removeDialogPromise;

    await siteDataUpdated;

    totalUsage = await SiteDataTestUtils.getQuotaUsage(TEST_ORIGIN);
    is(totalUsage, 0, "The total usage should be 0");

    await BrowserTestUtils.waitForCondition(() => label.textContent == "No",
      "Should show no site data usage in the security section.");

    pageInfo.close();
  });
});

// Test displaying and removing cookies.
add_task(async function test_Cookies() {
  // Add some test cookies.
  SiteDataTestUtils.addToCookies(TEST_ORIGIN, "test1", "1");
  SiteDataTestUtils.addToCookies(TEST_ORIGIN, "test2", "2");
  SiteDataTestUtils.addToCookies(TEST_SUB_ORIGIN, "test1", "1");

  await BrowserTestUtils.withNewTab(TEST_ORIGIN, async function(browser) {
    let pageInfo = BrowserPageInfo(TEST_ORIGIN, "securityTab");
    await BrowserTestUtils.waitForEvent(pageInfo, "load");

    let label = pageInfo.document.getElementById("security-privacy-sitedata-value");
    let clearButton = pageInfo.document.getElementById("security-clear-sitedata");

    // The usage details are filled asynchronously, so we assert that they're present by
    // waiting for them to be filled in.
    await BrowserTestUtils.waitForCondition(() => label.textContent.includes("cookies"),
      "Should show cookies in the security section.");

    let cookiesCleared = TestUtils.topicObserved("cookie-changed", (subj, data) => data == "deleted");

    let removeDialogPromise = BrowserTestUtils.promiseAlertDialogOpen("accept", REMOVE_DIALOG_URL);
    clearButton.click();
    await removeDialogPromise;

    await cookiesCleared;

    let uri = Services.io.newURI(TEST_ORIGIN);
    is(Services.cookies.countCookiesFromHost(uri.host), 0, "Cookies from the base domain should be cleared");

    await BrowserTestUtils.waitForCondition(() => label.textContent == "No",
      "Should show no cookies in the security section.");

    pageInfo.close();
  });
});

// Clean up in case we missed anything...
add_task(async function cleanup() {
  await SiteDataTestUtils.clear();
});
