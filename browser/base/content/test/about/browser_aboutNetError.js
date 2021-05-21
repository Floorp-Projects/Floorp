/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const SSL3_PAGE = "https://ssl3.example.com/";
const TLS10_PAGE = "https://tls1.example.com/";
const TLS12_PAGE = "https://tls12.example.com/";

// This includes all the cipher suite prefs we have.
const CIPHER_SUITE_PREFS = [
  "security.ssl3.dhe_rsa_aes_128_sha",
  "security.ssl3.dhe_rsa_aes_256_sha",
  "security.ssl3.ecdhe_ecdsa_aes_128_gcm_sha256",
  "security.ssl3.ecdhe_ecdsa_aes_128_sha",
  "security.ssl3.ecdhe_ecdsa_aes_256_gcm_sha384",
  "security.ssl3.ecdhe_ecdsa_aes_256_sha",
  "security.ssl3.ecdhe_ecdsa_chacha20_poly1305_sha256",
  "security.ssl3.ecdhe_rsa_aes_128_gcm_sha256",
  "security.ssl3.ecdhe_rsa_aes_128_sha",
  "security.ssl3.ecdhe_rsa_aes_256_gcm_sha384",
  "security.ssl3.ecdhe_rsa_aes_256_sha",
  "security.ssl3.ecdhe_rsa_chacha20_poly1305_sha256",
  "security.ssl3.rsa_aes_128_sha",
  "security.ssl3.rsa_aes_256_sha",
  "security.ssl3.rsa_aes_128_gcm_sha256",
  "security.ssl3.rsa_aes_256_gcm_sha384",
  "security.ssl3.rsa_des_ede3_sha",
  "security.tls13.aes_128_gcm_sha256",
  "security.tls13.aes_256_gcm_sha384",
  "security.tls13.chacha20_poly1305_sha256",
];

function resetPrefs() {
  Services.prefs.clearUserPref("security.tls.version.min");
  Services.prefs.clearUserPref("security.tls.version.max");
  Services.prefs.clearUserPref("security.tls.version.enable-deprecated");
  Services.prefs.clearUserPref("security.certerrors.tls.version.show-override");
}

add_task(async function resetToDefaultConfig() {
  info(
    "Change TLS config to cause page load to fail, check that reset button is shown and that it works"
  );

  // Just twiddling version will trigger the TLS 1.0 offer.  So to test the
  // broader UX, disable all cipher suites to trigger SSL_ERROR_SSL_DISABLED.
  // This can be removed when security.tls.version.enable-deprecated is.
  CIPHER_SUITE_PREFS.forEach(suitePref => {
    Services.prefs.setBoolPref(suitePref, false);
  });

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

  CIPHER_SUITE_PREFS.forEach(suitePref => {
    Services.prefs.clearUserPref(suitePref);
  });
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

add_task(async function checkEnable10() {
  info(
    "Load a page with a deprecated TLS version, an option to enable TLS 1.0 is offered and it works"
  );

  Services.prefs.setIntPref("security.tls.version.min", 3);
  // Disable TLS 1.3 so that we trigger a SSL_ERROR_UNSUPPORTED_VERSION.
  // As NSS generates an alert rather than negotiating a lower version
  // if we use the supported_versions extension from TLS 1.3.
  Services.prefs.setIntPref("security.tls.version.max", 3);

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

  // Setup an observer for the target page.
  const finalLoadComplete = BrowserTestUtils.browserLoaded(
    browser,
    false,
    TLS10_PAGE
  );

  await SpecialPowers.spawn(browser, [], async function() {
    const doc = content.document;
    ok(
      doc.documentURI.startsWith("about:neterror"),
      "Should be showing error page"
    );

    const enableTls10Button = doc.getElementById("enableTls10Button");
    ok(
      ContentTaskUtils.is_visible(enableTls10Button),
      "Option to re-enable TLS 1.0 is visible"
    );
    enableTls10Button.click();

    // It should not also offer to reset preferences instead.
    const prefResetButton = doc.getElementById("prefResetButton");
    ok(
      !ContentTaskUtils.is_visible(prefResetButton),
      "prefResetButton should NOT be visible"
    );
  });

  info("Waiting for the TLS 1.0 page to load after the click");
  await finalLoadComplete;

  resetPrefs();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function dontOffer10WhenAlreadyEnabled() {
  info("An option to enable TLS 1.0 is not offered if already enabled");

  Services.prefs.setIntPref("security.tls.version.min", 3);
  Services.prefs.setIntPref("security.tls.version.max", 3);
  Services.prefs.setBoolPref("security.tls.version.enable-deprecated", true);

  let browser;
  let pageLoaded;
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    () => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, SSL3_PAGE);
      browser = gBrowser.selectedBrowser;
      pageLoaded = BrowserTestUtils.waitForErrorPage(browser);
    },
    false
  );

  info("Loading and waiting for the net error");
  await pageLoaded;

  await SpecialPowers.spawn(browser, [], async function() {
    const doc = content.document;
    ok(
      doc.documentURI.startsWith("about:neterror"),
      "Should be showing error page"
    );

    const enableTls10Button = doc.getElementById("enableTls10Button");
    ok(
      !ContentTaskUtils.is_visible(enableTls10Button),
      "Option to re-enable TLS 1.0 is not visible"
    );

    // It should offer to reset preferences instead.
    const prefResetButton = doc.getElementById("prefResetButton");
    ok(
      ContentTaskUtils.is_visible(prefResetButton),
      "prefResetButton should be visible"
    );
  });

  resetPrefs();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function overrideUIPref() {
  info("TLS 1.0 override option isn't shown when the pref is set to false");

  Services.prefs.setIntPref("security.tls.version.min", 3);
  Services.prefs.setIntPref("security.tls.version.max", 3);
  Services.prefs.setBoolPref(
    "security.certerrors.tls.version.show-override",
    false
  );

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

  await ContentTask.spawn(browser, null, async function() {
    const doc = content.document;
    ok(
      doc.documentURI.startsWith("about:neterror"),
      "Should be showing error page"
    );

    const enableTls10Button = doc.getElementById("enableTls10Button");
    ok(
      !ContentTaskUtils.is_visible(enableTls10Button),
      "Option to re-enable TLS 1.0 is not visible"
    );
  });

  resetPrefs();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
