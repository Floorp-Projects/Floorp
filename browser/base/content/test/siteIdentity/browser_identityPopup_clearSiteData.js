/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_ORIGIN = "https://example.com";
const TEST_SUB_ORIGIN = "https://test1.example.com";
const TEST_ORIGIN_2 = "https://example.net";
const REMOVE_DIALOG_URL =
  "chrome://browser/content/preferences/dialogs/siteDataRemoveSelected.xhtml";

// Greek IDN for 'example.test'.
const TEST_IDN_ORIGIN =
  "https://\u03C0\u03B1\u03C1\u03AC\u03B4\u03B5\u03B9\u03B3\u03BC\u03B1.\u03B4\u03BF\u03BA\u03B9\u03BC\u03AE";
const TEST_PUNY_ORIGIN = "https://xn--hxajbheg2az3al.xn--jxalpdlp/";
const TEST_PUNY_SUB_ORIGIN = "https://sub1.xn--hxajbheg2az3al.xn--jxalpdlp/";

ChromeUtils.defineESModuleGetters(this, {
  SiteDataTestUtils: "resource://testing-common/SiteDataTestUtils.sys.mjs",
});

async function testClearing(
  testQuota,
  testCookies,
  testURI,
  originA,
  subOriginA,
  originB
) {
  // Create a variant of originB which is partitioned under top level originA.
  let { scheme, host } = Services.io.newURI(originA);
  let partitionKey = `(${scheme},${host})`;

  let { origin: originBPartitioned } =
    Services.scriptSecurityManager.createContentPrincipal(
      Services.io.newURI(originB),
      { partitionKey }
    );

  // Add some test quota storage.
  if (testQuota) {
    await SiteDataTestUtils.addToIndexedDB(originA);
    await SiteDataTestUtils.addToIndexedDB(subOriginA);
    await SiteDataTestUtils.addToIndexedDB(originBPartitioned);
  }

  // Add some test cookies.
  if (testCookies) {
    SiteDataTestUtils.addToCookies({
      origin: originA,
      name: "test1",
      value: "1",
    });
    SiteDataTestUtils.addToCookies({
      origin: originA,
      name: "test2",
      value: "2",
    });
    SiteDataTestUtils.addToCookies({
      origin: subOriginA,
      name: "test3",
      value: "1",
    });

    SiteDataTestUtils.addToCookies({
      origin: originBPartitioned,
      name: "test4",
      value: "1",
    });
  }

  await BrowserTestUtils.withNewTab(testURI, async function (browser) {
    // Verify we have added quota storage.
    if (testQuota) {
      let usage = await SiteDataTestUtils.getQuotaUsage(originA);
      Assert.greater(usage, 0, "Should have data for the base origin.");

      usage = await SiteDataTestUtils.getQuotaUsage(subOriginA);
      Assert.greater(usage, 0, "Should have data for the sub origin.");

      usage = await SiteDataTestUtils.getQuotaUsage(originBPartitioned);
      Assert.greater(usage, 0, "Should have data for the partitioned origin.");
    }

    // Open the identity popup.
    let { gIdentityHandler } = gBrowser.ownerGlobal;
    let promisePanelOpen = BrowserTestUtils.waitForEvent(
      gBrowser.ownerGlobal,
      "popupshown",
      true,
      event => event.target == gIdentityHandler._identityPopup
    );
    gIdentityHandler._identityIconBox.click();
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
      let promises = ["test1", "test2", "test3", "test4"].map(cookieName =>
        TestUtils.topicObserved("cookie-changed", subj => {
          let notification = subj.QueryInterface(Ci.nsICookieNotification);
          return (
            notification.action == Ci.nsICookieNotification.COOKIE_DELETED &&
            notification.cookie.name == cookieName
          );
        })
      );
      cookiesCleared = Promise.all(promises);
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
      let uri = Services.io.newURI(originA);
      is(
        Services.cookies.countCookiesFromHost(uri.host),
        0,
        "Cookies from the base domain should be cleared"
      );
      uri = Services.io.newURI(subOriginA);
      is(
        Services.cookies.countCookiesFromHost(uri.host),
        0,
        "Cookies from the sub domain should be cleared"
      );
      ok(
        !SiteDataTestUtils.hasCookies(originBPartitioned),
        "Partitioned cookies should be cleared"
      );
    }

    // Check that quota storage was deleted.
    if (testQuota) {
      await TestUtils.waitForCondition(async () => {
        let usage = await SiteDataTestUtils.getQuotaUsage(originA);
        return usage == 0;
      }, "Should have no data for the base origin.");

      let usage = await SiteDataTestUtils.getQuotaUsage(subOriginA);
      is(usage, 0, "Should have no data for the sub origin.");

      usage = await SiteDataTestUtils.getQuotaUsage(originBPartitioned);
      is(usage, 0, "Should have no data for the partitioned origin.");
    }

    // Open the site identity panel again to check that the button isn't shown anymore.
    promisePanelOpen = BrowserTestUtils.waitForEvent(
      gIdentityHandler._identityPopup,
      "popupshown"
    );
    gIdentityHandler._identityIconBox.click();
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
  await testClearing(
    true,
    false,
    TEST_ORIGIN,
    TEST_ORIGIN,
    TEST_SUB_ORIGIN,
    TEST_ORIGIN_2
  );
});

// Test removing cookies.
add_task(async function test_ClearCookies() {
  await testClearing(
    false,
    true,
    TEST_ORIGIN,
    TEST_ORIGIN,
    TEST_SUB_ORIGIN,
    TEST_ORIGIN_2
  );
});

// Test removing both.
add_task(async function test_ClearCookiesAndSiteData() {
  await testClearing(
    true,
    true,
    TEST_ORIGIN,
    TEST_ORIGIN,
    TEST_SUB_ORIGIN,
    TEST_ORIGIN_2
  );
});

// Test IDN Domains
add_task(async function test_IDN_ClearCookiesAndSiteData() {
  await testClearing(
    true,
    true,
    TEST_IDN_ORIGIN,
    TEST_PUNY_ORIGIN,
    TEST_PUNY_SUB_ORIGIN,
    TEST_ORIGIN_2
  );
});
