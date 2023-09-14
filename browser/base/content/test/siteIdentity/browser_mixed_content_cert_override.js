/*
 * Bug 1253771 - check mixed content blocking in combination with overriden certificates
 */

"use strict";

const MIXED_CONTENT_URL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://self-signed.example.com"
  ) + "test-mixedcontent-securityerrors.html";

function getConnectionState() {
  return document.getElementById("identity-popup").getAttribute("connection");
}

function getPopupContentVerifier() {
  return document.getElementById("identity-popup-content-verifier");
}

function getIdentityIcon() {
  return window.getComputedStyle(document.getElementById("identity-icon"))
    .listStyleImage;
}

function checkIdentityPopup(icon) {
  gIdentityHandler.refreshIdentityPopup();
  is(getIdentityIcon(), `url("chrome://global/skin/icons/${icon}")`);
  is(getConnectionState(), "secure-cert-user-overridden");
  isnot(
    getPopupContentVerifier().style.display,
    "none",
    "Overridden certificate warning is shown"
  );
  ok(
    getPopupContentVerifier().textContent.includes("security exception"),
    "Text shows overridden certificate warning."
  );
}

add_task(async function () {
  await BrowserTestUtils.openNewForegroundTab(gBrowser);

  // check that a warning is shown when loading a page with mixed content and an overridden certificate
  await loadBadCertPage(MIXED_CONTENT_URL);
  checkIdentityPopup("security-warning.svg");

  // check that the crossed out icon is shown when disabling mixed content protection
  gIdentityHandler.disableMixedContentProtection();
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  checkIdentityPopup("security-broken.svg");

  // check that a warning is shown even without mixed content
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    "https://self-signed.example.com"
  );
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  checkIdentityPopup("security-warning.svg");

  // remove cert exception
  let certOverrideService = Cc[
    "@mozilla.org/security/certoverride;1"
  ].getService(Ci.nsICertOverrideService);
  certOverrideService.clearValidityOverride("self-signed.example.com", -1, {});

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
