/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that visiting a site pinned with HPKP headers does not succeed when it
// uses a certificate with a key not in the pinset. This should result in an
// about:neterror page
// Also verify that removal of the HPKP headers succeeds (via HPKP headers)
// and that after removal the visit to the site with the previously
// unauthorized pins succeeds.
//
// This test required three certs to be created in build/pgo/certs:
// 1. A new trusted root:
//   a. certutil -S -s "Alternate trusted authority" -s "CN=Alternate Trusted Authority" -t "C,," -x -m 1 -v 120 -n "alternateTrustedAuthority" -Z SHA256 -g 2048 -2 -d .
//   b. (export) certutil -L -d . -n "alternateTrustedAuthority" -a -o alternateroot.ca
//     (files ended in .ca are added as trusted roots by the mochitest harness)
// 2. A good pinning server cert (signed by the pgo root):
//   certutil -S -n "dynamicPinningGood" -s "CN=dynamic-pinning.example.com" -c "pgo temporary ca" -t "P,," -k rsa -g 2048 -Z SHA256 -m 8939454 -v 120 -8 "*.include-subdomains.pinning-dynamic.example.com,*.pinning-dynamic.example.com" -d .
// 3. A certificate with a different issuer, so as to cause a key pinning violation."
//   certutil -S -n "dynamicPinningBad" -s "CN=bad.include-subdomains.pinning-dynamic.example.com" -c "alternateTrustedAuthority" -t "P,," -k rsa -g 2048 -Z SHA256 -m 893945439 -v 120 -8 "bad.include-subdomains.pinning-dynamic.example.com" -d .

const gSSService = Cc["@mozilla.org/ssservice;1"]
                     .getService(Ci.nsISiteSecurityService);
const gIOService = Cc["@mozilla.org/network/io-service;1"]
                    .getService(Ci.nsIIOService);

const kPinningDomain = "include-subdomains.pinning-dynamic.example.com";
const khpkpPinninEnablePref = "security.cert_pinning.process_headers_from_non_builtin_roots";
const kpkpEnforcementPref = "security.cert_pinning.enforcement_level";
const kBadPinningDomain = "bad.include-subdomains.pinning-dynamic.example.com";
const kURLPath = "/browser/browser/base/content/test/general/pinning_headers.sjs?";

function test() {
  waitForExplicitFinish();
  // Enable enforcing strict pinning and processing headers from
  // non-builtin roots.
  Services.prefs.setIntPref(kpkpEnforcementPref, 2);
  Services.prefs.setBoolPref(khpkpPinninEnablePref, true);
  registerCleanupFunction(function () {
    Services.prefs.clearUserPref(kpkpEnforcementPref);
    Services.prefs.clearUserPref(khpkpPinninEnablePref);
    let uri = gIOService.newURI("https://" + kPinningDomain, null, null);
    gSSService.removeState(Ci.nsISiteSecurityService.HEADER_HPKP, uri, 0);
  });
  whenNewTabLoaded(window, loadPinningPage);
}

// Start by making a successful connection to a domain that will pin a site
function loadPinningPage() {
  gBrowser.selectedBrowser.addEventListener("load",
                                             successfulPinningPageListener,
                                             true);

  gBrowser.selectedBrowser.loadURI("https://" + kPinningDomain + kURLPath + "valid");
}

// After the site is pinned try to load with a subdomain site that should
// fail to validate
let successfulPinningPageListener = {
  handleEvent: function() {
    gBrowser.selectedBrowser.removeEventListener("load", this, true);
    gBrowser.addProgressListener(certErrorProgressListener);
    gBrowser.selectedBrowser.loadURI("https://" + kBadPinningDomain);
  }
};

// The browser should load about:neterror, when this happens, proceed
// to load the pinning domain again, this time removing the pinning information
let certErrorProgressListener = {
  buttonClicked: false,
  onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus) {
    if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
      let self = this;
      // Can't directly call button.click() in onStateChange
      executeSoon(function() {
        let button =   content.document.getElementById("errorTryAgain");
        // If about:neterror hasn't fully loaded, the button won't be present.
        // It will eventually be there, however.
        if (button && !self.buttonClicked) {
          gBrowser.removeProgressListener(self);
          gBrowser.selectedBrowser.addEventListener("load",
                                                    successfulPinningRemovalPageListener,
                                                    true);
          gBrowser.selectedBrowser.loadURI("https://" + kPinningDomain + kURLPath + "zeromaxagevalid");
        }
      });
    }
  }
};

// After the pinning information has been removed (successful load) proceed
// to load again with the invalid pin domain.
let successfulPinningRemovalPageListener = {
  handleEvent: function() {
    gBrowser.selectedBrowser.removeEventListener("load", this, true);
    gBrowser.selectedBrowser.addEventListener("load",
                                              successfulLoadListener,
                                              true);

    gBrowser.selectedBrowser.loadURI("https://" + kBadPinningDomain);
  }
};

// Finally, we should successfully load
// https://bad.include-subdomains.pinning-dynamic.example.com.
let successfulLoadListener = {
  handleEvent: function() {
    gBrowser.selectedBrowser.removeEventListener("load", this, true);
    gBrowser.removeTab(gBrowser.selectedTab);
    ok(true, "load complete");
    finish();
  }
};
