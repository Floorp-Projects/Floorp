ChromeUtils.defineESModuleGetters(this, {
  DownloadUtils: "resource://gre/modules/DownloadUtils.sys.mjs",
  SiteDataTestUtils: "resource://testing-common/SiteDataTestUtils.sys.mjs",
});

const TEST_ORIGIN = "https://example.com";
// eslint-disable-next-line @microsoft/sdl/no-insecure-url
const TEST_HTTP_ORIGIN = "http://example.com";
const TEST_SUB_ORIGIN = "https://test1.example.com";
const REMOVE_DIALOG_URL =
  "chrome://browser/content/preferences/dialogs/siteDataRemoveSelected.xhtml";
const TEST_ORIGIN_CERT_ERROR = "https://expired.example.com";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

// Test opening the correct certificate information when clicking "Show certificate".
add_task(async function test_ShowCertificate() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_ORIGIN);
  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_SUB_ORIGIN
  );

  let pageInfo = BrowserPageInfo(TEST_SUB_ORIGIN, "securityTab");
  await BrowserTestUtils.waitForEvent(pageInfo, "load");
  let pageInfoDoc = pageInfo.document;
  let securityTab = pageInfoDoc.getElementById("securityTab");
  await TestUtils.waitForCondition(
    () => BrowserTestUtils.isVisible(securityTab),
    "Security tab should be visible."
  );

  async function openAboutCertificate() {
    let loaded = BrowserTestUtils.waitForNewTab(gBrowser, null, true);
    let viewCertButton = pageInfoDoc.getElementById("security-view-cert");
    await TestUtils.waitForCondition(
      () => BrowserTestUtils.isVisible(viewCertButton),
      "view cert button should be visible."
    );
    viewCertButton.click();
    await loaded;

    await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
      let certificateSection = await ContentTaskUtils.waitForCondition(() => {
        return content.document.querySelector("certificate-section");
      }, "Certificate section found");

      let commonName = certificateSection.shadowRoot
        .querySelector(".subject-name")
        .shadowRoot.querySelector(".common-name")
        .shadowRoot.querySelector(".info").textContent;
      is(commonName, "example.com", "Should have the same common name.");
    });

    gBrowser.removeCurrentTab(); // closes about:certificate
  }

  await openAboutCertificate();

  gBrowser.selectedTab = tab1;

  await openAboutCertificate();

  pageInfo.close();
  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});

// Test displaying website identity information when loading images.
add_task(async function test_image() {
  let url = TEST_PATH + "moz.png";
  await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  let pageInfo = BrowserPageInfo(url, "securityTab");
  await BrowserTestUtils.waitForEvent(pageInfo, "load");
  let pageInfoDoc = pageInfo.document;
  let securityTab = pageInfoDoc.getElementById("securityTab");

  await TestUtils.waitForCondition(
    () => BrowserTestUtils.isVisible(securityTab),
    "Security tab should be visible."
  );

  let owner = pageInfoDoc.getElementById("security-identity-owner-value");
  let verifier = pageInfoDoc.getElementById("security-identity-verifier-value");
  let domain = pageInfoDoc.getElementById("security-identity-domain-value");

  await TestUtils.waitForCondition(
    () => owner.value === "This website does not supply ownership information.",
    `Value of owner should be should be "This website does not supply ownership information." instead got "${owner.value}".`
  );

  await TestUtils.waitForCondition(
    () => verifier.value === "Mozilla Testing",
    `Value of verifier should be "Mozilla Testing", instead got "${verifier.value}".`
  );

  let browser = gBrowser.selectedBrowser;

  await TestUtils.waitForCondition(
    () => domain.value === browser.currentURI.displayHost,
    `Value of domain should be ${browser.currentURI.displayHost}, instead got "${domain.value}".`
  );

  pageInfo.close();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

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
  let pageInfoDoc = pageInfo.document;
  let securityTab = pageInfoDoc.getElementById("securityTab");

  await TestUtils.waitForCondition(
    () => BrowserTestUtils.isVisible(securityTab),
    "Security tab should be visible."
  );

  let owner = pageInfoDoc.getElementById("security-identity-owner-value");
  let verifier = pageInfoDoc.getElementById("security-identity-verifier-value");
  let domain = pageInfoDoc.getElementById("security-identity-domain-value");

  await TestUtils.waitForCondition(
    () => owner.value === "This website does not supply ownership information.",
    `Value of owner should be should be "This website does not supply ownership information." instead got "${owner.value}".`
  );

  await TestUtils.waitForCondition(
    () => verifier.value === "Not specified",
    `Value of verifier should be "Not specified", instead got "${verifier.value}".`
  );

  await TestUtils.waitForCondition(
    () => domain.value === browser.currentURI.displayHost,
    `Value of domain should be ${browser.currentURI.displayHost}, instead got "${domain.value}".`
  );

  pageInfo.close();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Test displaying website identity information on http pages.
add_task(async function test_SecurityHTTP() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_HTTP_ORIGIN);

  let pageInfo = BrowserPageInfo(TEST_HTTP_ORIGIN, "securityTab");
  await BrowserTestUtils.waitForEvent(pageInfo, "load");
  let pageInfoDoc = pageInfo.document;
  let securityTab = pageInfoDoc.getElementById("securityTab");
  await TestUtils.waitForCondition(
    () => BrowserTestUtils.isVisible(securityTab),
    "Security tab should be visible."
  );

  let owner = pageInfoDoc.getElementById("security-identity-owner-value");
  let verifier = pageInfoDoc.getElementById("security-identity-verifier-value");
  let domain = pageInfoDoc.getElementById("security-identity-domain-value");

  await TestUtils.waitForCondition(
    () => owner.value === "This website does not supply ownership information.",
    `Value of owner should be should be "This website does not supply ownership information." instead got "${owner.value}".`
  );

  await TestUtils.waitForCondition(
    () => verifier.value === "Not specified",
    `Value of verifier should be "Not specified", instead got "${verifier.value}".`
  );

  await TestUtils.waitForCondition(
    () => domain.value === gBrowser.selectedBrowser.currentURI.displayHost,
    `Value of domain should be ${gBrowser.selectedBrowser.currentURI.displayHost}, instead got "${domain.value}".`
  );

  pageInfo.close();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Test displaying valid certificate information in page info.
add_task(async function test_ValidCert() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_ORIGIN);

  let pageInfo = BrowserPageInfo(TEST_ORIGIN, "securityTab");
  await BrowserTestUtils.waitForEvent(pageInfo, "load");
  let pageInfoDoc = pageInfo.document;
  let securityTab = pageInfoDoc.getElementById("securityTab");
  await TestUtils.waitForCondition(
    () => BrowserTestUtils.isVisible(securityTab),
    "Security tab should be visible."
  );

  let owner = pageInfoDoc.getElementById("security-identity-owner-value");
  let verifier = pageInfoDoc.getElementById("security-identity-verifier-value");
  let domain = pageInfoDoc.getElementById("security-identity-domain-value");

  await TestUtils.waitForCondition(
    () => owner.value === "This website does not supply ownership information.",
    `Value of owner should be "This website does not supply ownership information.", got "${owner.value}".`
  );

  await TestUtils.waitForCondition(
    () => verifier.value === "Mozilla Testing",
    `Value of verifier should be "Mozilla Testing", got "${verifier.value}".`
  );

  await TestUtils.waitForCondition(
    () => domain.value === gBrowser.selectedBrowser.currentURI.displayHost,
    `Value of domain should be ${gBrowser.selectedBrowser.currentURI.displayHost}, instead got "${domain.value}".`
  );

  pageInfo.close();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Test displaying and removing quota managed data.
add_task(async function test_SiteData() {
  await SiteDataTestUtils.addToIndexedDB(TEST_ORIGIN);

  await BrowserTestUtils.withNewTab(TEST_ORIGIN, async function (browser) {
    let totalUsage = await SiteDataTestUtils.getQuotaUsage(TEST_ORIGIN);
    Assert.greater(totalUsage, 0, "The total usage should not be 0");

    let pageInfo = BrowserPageInfo(TEST_ORIGIN, "securityTab");
    await BrowserTestUtils.waitForEvent(pageInfo, "load");
    let pageInfoDoc = pageInfo.document;

    let label = pageInfoDoc.getElementById("security-privacy-sitedata-value");
    let clearButton = pageInfoDoc.getElementById("security-clear-sitedata");

    let size = DownloadUtils.convertByteUnits(totalUsage);

    // The usage details are filled asynchronously, so we assert that they're present by
    // waiting for them to be filled in.
    // We only wait for the right unit to appear, since this number is intermittently
    // varying by slight amounts on infra machines.
    await TestUtils.waitForCondition(
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

    await TestUtils.waitForCondition(
      () => label.textContent == "No",
      "Should show no site data usage in the security section."
    );

    pageInfo.close();
  });
});

// Test displaying and removing cookies.
add_task(async function test_Cookies() {
  // Add some test cookies.
  SiteDataTestUtils.addToCookies({
    origin: TEST_ORIGIN,
    name: "test1",
    value: "1",
  });
  SiteDataTestUtils.addToCookies({
    origin: TEST_ORIGIN,
    name: "test2",
    value: "2",
  });
  SiteDataTestUtils.addToCookies({
    origin: TEST_SUB_ORIGIN,
    name: "test1",
    value: "1",
  });

  await BrowserTestUtils.withNewTab(TEST_ORIGIN, async function (browser) {
    let pageInfo = BrowserPageInfo(TEST_ORIGIN, "securityTab");
    await BrowserTestUtils.waitForEvent(pageInfo, "load");

    let pageInfoDoc = pageInfo.document;

    let label = pageInfoDoc.getElementById("security-privacy-sitedata-value");
    let clearButton = pageInfoDoc.getElementById("security-clear-sitedata");

    // The usage details are filled asynchronously, so we assert that they're present by
    // waiting for them to be filled in.
    await TestUtils.waitForCondition(
      () => label.textContent.includes("cookies"),
      "Should show cookies in the security section."
    );

    let cookiesCleared = TestUtils.topicObserved(
      "cookie-changed",
      subj =>
        subj.QueryInterface(Ci.nsICookieNotification).action ==
        Ci.nsICookieNotification.COOKIE_DELETED
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

    await TestUtils.waitForCondition(
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
