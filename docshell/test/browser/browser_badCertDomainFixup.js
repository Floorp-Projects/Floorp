/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test checks if we are correctly fixing https URLs by prefixing
// with www. when we encounter a SSL_ERROR_BAD_CERT_DOMAIN error.
// For example, https://example.com -> https://www.example.com.

async function verifyErrorPage(errorPageURL) {
  let certErrorLoaded = BrowserTestUtils.waitForErrorPage(
    gBrowser.selectedBrowser
  );
  BrowserTestUtils.startLoadingURIString(gBrowser, errorPageURL);
  await certErrorLoaded;

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    let ec;
    await ContentTaskUtils.waitForCondition(() => {
      ec = content.document.getElementById("errorCode");
      return ec.textContent;
    }, "Error code has been set inside the advanced button panel");
    is(
      ec.textContent,
      "SSL_ERROR_BAD_CERT_DOMAIN",
      "Correct error code is shown"
    );
  });
}

// Turn off the pref and ensure that we show the error page as expected.
add_task(async function testNoFixupDisabledByPref() {
  await SpecialPowers.pushPrefEnv({
    set: [["security.bad_cert_domain_error.url_fix_enabled", false]],
  });
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  await verifyErrorPage("https://badcertdomain.example.com");
  await verifyErrorPage("https://www.badcertdomain2.example.com");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await SpecialPowers.popPrefEnv();
});

// Test that "www." is prefixed to a https url when we encounter a bad cert domain
// error if the "www." form is included in the certificate's subjectAltNames.
add_task(async function testAddPrefixForBadCertDomain() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  let loadSuccessful = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    "https://www.badcertdomain.example.com/"
  );
  BrowserTestUtils.startLoadingURIString(
    gBrowser,
    "https://badcertdomain.example.com"
  );
  await loadSuccessful;

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Test that we don't prefix "www." to a https url when we encounter a bad cert domain
// error under certain conditions.
add_task(async function testNoFixupCases() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  // Test for when "www." form is not present in the certificate.
  await verifyErrorPage("https://mismatch.badcertdomain.example.com");

  // Test that urls with IP addresses are not fixed.
  await SpecialPowers.pushPrefEnv({
    set: [["network.proxy.allow_hijacking_localhost", true]],
  });
  await verifyErrorPage("https://127.0.0.3:433");
  await SpecialPowers.popPrefEnv();

  // Test that urls with ports are not fixed.
  await verifyErrorPage("https://badcertdomain.example.com:82");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Test removing "www." prefix if the "www."-less form is included in the
// certificate's subjectAltNames.
add_task(async function testRemovePrefixForBadCertDomain() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  let loadSuccessful = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    "https://badcertdomain2.example.com/"
  );
  BrowserTestUtils.startLoadingURIString(
    gBrowser,
    "https://www.badcertdomain2.example.com"
  );
  await loadSuccessful;

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
