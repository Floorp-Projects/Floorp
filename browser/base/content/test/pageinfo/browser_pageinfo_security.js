ChromeUtils.defineModuleGetter(
  this,
  "SiteDataTestUtils",
  "resource://testing-common/SiteDataTestUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "DownloadUtils",
  "resource://gre/modules/DownloadUtils.jsm"
);

const TEST_ORIGIN = "https://example.com";
const TEST_SUB_ORIGIN = "https://test1.example.com";
const REMOVE_DIALOG_URL =
  "chrome://browser/content/preferences/siteDataRemoveSelected.xul";
const TEST_ORIGIN_CERT_ERROR = "https://expired.example.com";

// Test displaying website identity information on certificate error pages.
add_task(async function test_CertificateError() {
  let browser;
  let pageLoaded;
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    () => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(
        gBrowser,
        TEST_ORIGIN_CERT_ERROR
      );
      browser = gBrowser.selectedBrowser;
      pageLoaded = BrowserTestUtils.waitForErrorPage(browser);
    },
    false
  );

  await pageLoaded;

  let pageInfo = BrowserPageInfo(TEST_ORIGIN_CERT_ERROR, "securityTab");
  await BrowserTestUtils.waitForEvent(pageInfo, "load");
  let securityTab = pageInfo.document.getElementById("securityTab");

  await TestUtils.waitForCondition(
    () => BrowserTestUtils.is_visible(securityTab),
    "Security tab should be visible."
  );

  let owner = pageInfo.document.getElementById("security-identity-owner-value");
  let verifier = pageInfo.document.getElementById(
    "security-identity-verifier-value"
  );
  let domain = pageInfo.document.getElementById(
    "security-identity-domain-value"
  );

  await TestUtils.waitForCondition(
    () =>
      owner.textContent ===
      "This website does not supply ownership information.",
    `Value of owner should be should be "This website does not supply ownership information." instead got "${
      verifier.textContent
    }".`
  );

  await TestUtils.waitForCondition(
    () => verifier.textContent === "Not specified",
    `Value of verifier should be "Not specified", instead got "${
      verifier.textContent
    }".`
  );

  await TestUtils.waitForCondition(
    () => domain.value === browser.currentURI.displayHost,
    `Value of domain should be ${
      browser.currentURI.displayHost
    }, instead got "${domain.value}".`
  );

  pageInfo.close();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Test displaying and removing quota managed data.
add_task(async function test_SiteData() {
  await SiteDataTestUtils.addToIndexedDB(TEST_ORIGIN);

  await BrowserTestUtils.withNewTab(TEST_ORIGIN, async function(browser) {
    let totalUsage = await SiteDataTestUtils.getQuotaUsage(TEST_ORIGIN);
    Assert.greater(totalUsage, 0, "The total usage should not be 0");

    let pageInfo = BrowserPageInfo(TEST_ORIGIN, "securityTab");
    await BrowserTestUtils.waitForEvent(pageInfo, "load");

    let label = pageInfo.document.getElementById(
      "security-privacy-sitedata-value"
    );
    let clearButton = pageInfo.document.getElementById(
      "security-clear-sitedata"
    );

    let size = DownloadUtils.convertByteUnits(totalUsage);

    // The usage details are filled asynchronously, so we assert that they're present by
    // waiting for them to be filled in.
    // We only wait for the right unit to appear, since this number is intermittently
    // varying by slight amounts on infra machines.
    await BrowserTestUtils.waitForCondition(
      () => label.textContent.includes(size[1]),
      "Should show site data usage in the security section."
    );
    let siteDataUpdated = TestUtils.topicObserved(
      "sitedatamanager:sites-updated"
    );

    let removeDialogPromise = BrowserTestUtils.promiseAlertDialogOpen(
      "accept",
      REMOVE_DIALOG_URL
    );
    clearButton.click();
    await removeDialogPromise;

    await siteDataUpdated;

    totalUsage = await SiteDataTestUtils.getQuotaUsage(TEST_ORIGIN);
    is(totalUsage, 0, "The total usage should be 0");

    await BrowserTestUtils.waitForCondition(
      () => label.textContent == "No",
      "Should show no site data usage in the security section."
    );

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

    let label = pageInfo.document.getElementById(
      "security-privacy-sitedata-value"
    );
    let clearButton = pageInfo.document.getElementById(
      "security-clear-sitedata"
    );

    // The usage details are filled asynchronously, so we assert that they're present by
    // waiting for them to be filled in.
    await BrowserTestUtils.waitForCondition(
      () => label.textContent.includes("cookies"),
      "Should show cookies in the security section."
    );

    let cookiesCleared = TestUtils.topicObserved(
      "cookie-changed",
      (subj, data) => data == "deleted"
    );

    let removeDialogPromise = BrowserTestUtils.promiseAlertDialogOpen(
      "accept",
      REMOVE_DIALOG_URL
    );
    clearButton.click();
    await removeDialogPromise;

    await cookiesCleared;

    let uri = Services.io.newURI(TEST_ORIGIN);
    is(
      Services.cookies.countCookiesFromHost(uri.host),
      0,
      "Cookies from the base domain should be cleared"
    );

    await BrowserTestUtils.waitForCondition(
      () => label.textContent == "No",
      "Should show no cookies in the security section."
    );

    pageInfo.close();
  });
});

// Clean up in case we missed anything...
add_task(async function cleanup() {
  await SiteDataTestUtils.clear();
});
