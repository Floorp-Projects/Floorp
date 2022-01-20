/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const SSL3_PAGE = "https://ssl3.example.com/";
const TLS10_PAGE = "https://tls1.example.com/";
const TLS12_PAGE = "https://tls12.example.com/";
const TRIPLEDES_PAGE = "https://3des.example.com/";

// This includes all the cipher suite prefs we have.
function resetPrefs() {
  Services.prefs.clearUserPref("security.tls.version.min");
  Services.prefs.clearUserPref("security.tls.version.max");
  Services.prefs.clearUserPref("security.tls.version.enable-deprecated");
}

add_task(async function resetToDefaultConfig() {
  info(
    "Change TLS config to cause page load to fail, check that reset button is shown and that it works"
  );

  // Set ourselves up for a TLS error.
  Services.prefs.setIntPref("security.tls.version.min", 1); // TLS 1.0
  Services.prefs.setIntPref("security.tls.version.max", 1);

  let browser;
  let pageLoaded;
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    () => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, TLS12_PAGE);
      browser = gBrowser.selectedBrowser;
      pageLoaded = BrowserTestUtils.waitForErrorPage(browser);
    },
    false
  );

  info("Loading and waiting for the net error");
  await pageLoaded;

  // Setup an observer for the target page.
  const finalLoadComplete = BrowserTestUtils.browserLoaded(
    browser,
    false,
    TLS12_PAGE
  );

  await SpecialPowers.spawn(browser, [], async function() {
    const doc = content.document;
    ok(
      doc.documentURI.startsWith("about:neterror"),
      "Should be showing error page"
    );

    const prefResetButton = doc.getElementById("prefResetButton");
    ok(
      ContentTaskUtils.is_visible(prefResetButton),
      "prefResetButton should be visible"
    );

    if (!Services.focus.focusedElement == prefResetButton) {
      await ContentTaskUtils.waitForEvent(prefResetButton, "focus");
    }

    Assert.ok(true, "prefResetButton has focus");

    prefResetButton.click();
  });

  info("Waiting for the page to load after the click");
  await finalLoadComplete;

  resetPrefs();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function checkLearnMoreLink() {
  info("Load an unsupported TLS page and check for a learn more link");

  // Set ourselves up for TLS error
  Services.prefs.setIntPref("security.tls.version.min", 3);
  Services.prefs.setIntPref("security.tls.version.max", 4);

  let browser;
  let pageLoaded;
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    () => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, TLS10_PAGE);
      browser = gBrowser.selectedBrowser;
      pageLoaded = BrowserTestUtils.waitForErrorPage(browser);
    },
    false
  );

  info("Loading and waiting for the net error");
  await pageLoaded;

  const baseURL = Services.urlFormatter.formatURLPref("app.support.baseURL");

  await SpecialPowers.spawn(browser, [baseURL], function(_baseURL) {
    const doc = content.document;
    ok(
      doc.documentURI.startsWith("about:neterror"),
      "Should be showing error page"
    );

    const tlsVersionNotice = doc.getElementById("tlsVersionNotice");
    ok(
      ContentTaskUtils.is_visible(tlsVersionNotice),
      "TLS version notice is visible"
    );

    const learnMoreLink = doc.getElementById("learnMoreLink");
    ok(
      ContentTaskUtils.is_visible(learnMoreLink),
      "Learn More link is visible"
    );
    is(learnMoreLink.getAttribute("href"), _baseURL + "connection-not-secure");

    const titleEl = doc.querySelector(".title-text");
    const actualDataL10nID = titleEl.getAttribute("data-l10n-id");
    is(
      actualDataL10nID,
      "nssFailure2-title",
      "Correct error page title is set"
    );

    const errorCodeEl = doc.querySelector("#errorShortDescText2");
    const actualDataL10Args = errorCodeEl.getAttribute("data-l10n-args");
    ok(
      actualDataL10Args.includes("SSL_ERROR_PROTOCOL_VERSION_ALERT"),
      "Correct error code is set"
    );
  });

  resetPrefs();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Test that ciphersuites that use 3DES (namely, TLS_RSA_WITH_3DES_EDE_CBC_SHA)
// can only be enabled when deprecated TLS is enabled.
add_task(async function onlyAllow3DESWithDeprecatedTLS() {
  // By default, connecting to a server that only uses 3DES should fail.
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async browser => {
      BrowserTestUtils.loadURI(browser, TRIPLEDES_PAGE);
      await BrowserTestUtils.waitForErrorPage(browser);
    }
  );

  // Enabling deprecated TLS should also enable 3DES.
  Services.prefs.setBoolPref("security.tls.version.enable-deprecated", true);
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async browser => {
      BrowserTestUtils.loadURI(browser, TRIPLEDES_PAGE);
      await BrowserTestUtils.browserLoaded(browser, false, TRIPLEDES_PAGE);
    }
  );

  // 3DES can be disabled separately.
  Services.prefs.setBoolPref(
    "security.ssl3.deprecated.rsa_des_ede3_sha",
    false
  );
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async browser => {
      BrowserTestUtils.loadURI(browser, TRIPLEDES_PAGE);
      await BrowserTestUtils.waitForErrorPage(browser);
    }
  );

  resetPrefs();
});
