/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test adding a certificate exception by attempting to browse to a site with
// a bad certificate, being redirected to the internal about:certerror page,
// using the button contained therein to load the certificate exception
// dialog, using that to add an exception, and finally successfully visiting
// the site, including showing the right identity box and control center icons.
add_task(async function() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser);
  await loadBadCertPage("https://expired.example.com");

  let { gIdentityHandler } = gBrowser.ownerGlobal;
  let promisePanelOpen = BrowserTestUtils.waitForEvent(
    gBrowser.ownerGlobal,
    "popupshown",
    true,
    event => event.target == gIdentityHandler._identityPopup
  );
  gIdentityHandler._identityIconBox.click();
  await promisePanelOpen;

  let promiseViewShown = BrowserTestUtils.waitForEvent(
    gIdentityHandler._identityPopup,
    "ViewShown"
  );
  document.getElementById("identity-popup-security-button").click();
  await promiseViewShown;

  is_element_visible(
    document.getElementById("identity-icon"),
    "Should see identity icon"
  );
  let identityIconImage = gBrowser.ownerGlobal
    .getComputedStyle(document.getElementById("identity-icon"))
    .getPropertyValue("list-style-image");
  let securityViewBG = gBrowser.ownerGlobal
    .getComputedStyle(
      document
        .getElementById("identity-popup-securityView")
        .getElementsByClassName("identity-popup-security-connection")[0]
    )
    .getPropertyValue("background-image");
  let securityContentBG = gBrowser.ownerGlobal
    .getComputedStyle(
      document
        .getElementById("identity-popup-mainView")
        .getElementsByClassName("identity-popup-security-connection")[0]
    )
    .getPropertyValue("background-image");
  is(
    identityIconImage,
    'url("chrome://global/skin/icons/security-warning.svg")',
    "Using expected icon image in the identity block"
  );
  is(
    securityViewBG,
    'url("chrome://global/skin/icons/security-warning.svg")',
    "Using expected icon image in the Control Center main view"
  );
  is(
    securityContentBG,
    'url("chrome://global/skin/icons/security-warning.svg")',
    "Using expected icon image in the Control Center subview"
  );

  gIdentityHandler._identityPopup.hidePopup();

  let certOverrideService = Cc[
    "@mozilla.org/security/certoverride;1"
  ].getService(Ci.nsICertOverrideService);
  certOverrideService.clearValidityOverride("expired.example.com", -1, {});
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
