/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test checks if we are correctly fixing https URLs by prefixing
// with www. when we encounter a SSL_ERROR_BAD_CERT_DOMAIN error.
// For example, https://example.com -> https://www.example.com.

const PREF_BAD_CERT_DOMAIN_FIX_ENABLED =
  "security.bad_cert_domain_error.url_fix_enabled";
const PREF_ALLOW_HIJACKING_LOCALHOST =
  "network.proxy.allow_hijacking_localhost";

const BAD_CERT_DOMAIN_ERROR_URL = "https://badcertdomain.example.com:443";
const FIXED_URL = "https://www.badcertdomain.example.com/";

const BAD_CERT_DOMAIN_ERROR_URL2 =
  "https://mismatch.badcertdomain.example.com:443";
const IPV4_ADDRESS = "https://127.0.0.3:433";
const BAD_CERT_DOMAIN_ERROR_PORT = "https://badcertdomain.example.com:82";

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

// Test that "www." is prefixed to a https url when we encounter a bad cert domain
// error if the "www." form is included in the certificate's subjectAltNames.
add_task(async function prefixBadCertDomain() {
  // Turn off the pref and ensure that we show the error page as expected.
  Services.prefs.setBoolPref(PREF_BAD_CERT_DOMAIN_FIX_ENABLED, false);

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  await verifyErrorPage(BAD_CERT_DOMAIN_ERROR_URL);
  info("Cert error is shown as expected when the fixup pref is disabled");

  // Turn on the pref and test that we fix the HTTPS URL.
  Services.prefs.setBoolPref(PREF_BAD_CERT_DOMAIN_FIX_ENABLED, true);
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  let loadSuccessful = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    FIXED_URL
  );
  BrowserTestUtils.startLoadingURIString(gBrowser, BAD_CERT_DOMAIN_ERROR_URL);
  await loadSuccessful;

  info("The URL was fixed as expected");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Test that we don't prefix "www." to a https url when we encounter a bad cert domain
// error under certain conditions.
add_task(async function ignoreBadCertDomain() {
  Services.prefs.setBoolPref(PREF_BAD_CERT_DOMAIN_FIX_ENABLED, true);
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  // Test for when "www." form is not present in the certificate.
  await verifyErrorPage(BAD_CERT_DOMAIN_ERROR_URL2);
  info("Certificate error was shown as expected");

  // Test that urls with IP addresses are not fixed.
  Services.prefs.setBoolPref(PREF_ALLOW_HIJACKING_LOCALHOST, true);
  await verifyErrorPage(IPV4_ADDRESS);
  Services.prefs.clearUserPref(PREF_ALLOW_HIJACKING_LOCALHOST);
  info("Certificate error was shown as expected for an IP address");

  // Test that urls with ports are not fixed.
  await verifyErrorPage(BAD_CERT_DOMAIN_ERROR_PORT);
  info("Certificate error was shown as expected for a host with port");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
