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
  checkControlPanelIcons();
  let certOverrideService = Cc["@mozilla.org/security/certoverride;1"]
                              .getService(Ci.nsICertOverrideService);
  certOverrideService.clearValidityOverride("expired.example.com", -1);
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Check for the correct icons in the identity box and control center.
function checkControlPanelIcons() {
  let { gIdentityHandler } = gBrowser.ownerGlobal;
  gIdentityHandler._identityBox.click();
  document.getElementById("identity-popup-security-expander").click();

  is_element_visible(document.getElementById("connection-icon"), "Should see connection icon");
  let connectionIconImage = gBrowser.ownerGlobal
        .getComputedStyle(document.getElementById("connection-icon"))
        .getPropertyValue("list-style-image");
  let securityViewBG = gBrowser.ownerGlobal
        .getComputedStyle(document.getElementById("identity-popup-securityView"))
        .getPropertyValue("background-image");
  let securityContentBG = gBrowser.ownerGlobal
        .getComputedStyle(document.getElementById("identity-popup-security-content"))
        .getPropertyValue("background-image");
  is(connectionIconImage,
     "url(\"chrome://browser/skin/connection-mixed-passive-loaded.svg\")",
     "Using expected icon image in the identity block");
  is(securityViewBG,
     "url(\"chrome://browser/skin/connection-mixed-passive-loaded.svg\")",
     "Using expected icon image in the Control Center main view");
  is(securityContentBG,
     "url(\"chrome://browser/skin/connection-mixed-passive-loaded.svg\")",
     "Using expected icon image in the Control Center subview");

  gIdentityHandler._identityPopup.hidden = true;
}

