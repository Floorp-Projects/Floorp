/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Tests for Bug 1535210 - Set SSL STATE_IS_BROKEN flag for TLS1.0 and TLS 1.1 connections
 */

const HTTPS_TLS1_0 = "https://tls1.example.com";
const HTTPS_TLS1_1 = "https://tls11.example.com";
const HTTPS_TLS1_2 = "https://tls12.example.com";
const HTTPS_TLS1_3 = "https://tls13.example.com";


function getIdentityMode(aWindow = window) {
  return aWindow.document.getElementById("identity-box").className;
}

function getConnectionState() {
  // Prevents items that are being lazy loaded causing issues
  document.getElementById("identity-box").click();
  gIdentityHandler.refreshIdentityPopup();
  return document.getElementById("identity-popup").getAttribute("connection");
}

add_task(async function() {
  await BrowserTestUtils.withNewTab("about:blank", async function(browser) {
    // Try deprecated versions
    await BrowserTestUtils.loadURI(browser, HTTPS_TLS1_0);
    await BrowserTestUtils.browserLoaded(browser);
    isSecurityState(browser, "broken");
    is(getIdentityMode(), "unknownIdentity weakCipher", "Identity should be unknownIdentity");
    is(getConnectionState(), "not-secure", "connectionState should be not-secure");

    await BrowserTestUtils.loadURI(browser, HTTPS_TLS1_1);
    await BrowserTestUtils.browserLoaded(browser);
    isSecurityState(browser, "broken");
    is(getIdentityMode(), "unknownIdentity weakCipher", "Identity should be unknownIdentity");
    is(getConnectionState(), "not-secure", "connectionState should be not-secure");

    // Transition to secure
    await BrowserTestUtils.loadURI(browser, HTTPS_TLS1_2);
    await BrowserTestUtils.browserLoaded(browser);
    isSecurityState(browser, "secure");
    is(getIdentityMode(), "verifiedDomain", "Identity should be verified");
    is(getConnectionState(), "secure", "connectionState should be secure");

    // Transition back to broken
    await BrowserTestUtils.loadURI(browser, HTTPS_TLS1_1);
    await BrowserTestUtils.browserLoaded(browser);
    isSecurityState(browser, "broken");
    is(getIdentityMode(), "unknownIdentity weakCipher", "Identity should be unknownIdentity");
    is(getConnectionState(), "not-secure", "connectionState should be not-secure");

    // TLS1.3 for completeness
    await BrowserTestUtils.loadURI(browser, HTTPS_TLS1_3);
    await BrowserTestUtils.browserLoaded(browser);
    isSecurityState(browser, "secure");
    is(getIdentityMode(), "verifiedDomain", "Identity should be verified");
    is(getConnectionState(), "secure", "connectionState should be secure");
  });
});
