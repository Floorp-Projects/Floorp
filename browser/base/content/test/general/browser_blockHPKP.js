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

const gSSService = Cc["@mozilla.org/ssservice;1"].getService(
  Ci.nsISiteSecurityService
);

const kPinningDomain = "include-subdomains.pinning-dynamic.example.com";
const khpkpPinninEnablePref =
  "security.cert_pinning.process_headers_from_non_builtin_roots";
const kpkpEnforcementPref = "security.cert_pinning.enforcement_level";
const kBadPinningDomain = "bad.include-subdomains.pinning-dynamic.example.com";
const kURLPath =
  "/browser/browser/base/content/test/general/pinning_headers.sjs?";

function test() {
  waitForExplicitFinish();
  // Enable enforcing strict pinning and processing headers from
  // non-builtin roots.
  Services.prefs.setIntPref(kpkpEnforcementPref, 2);
  Services.prefs.setBoolPref(khpkpPinninEnablePref, true);
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(kpkpEnforcementPref);
    Services.prefs.clearUserPref(khpkpPinninEnablePref);
    let uri = Services.io.newURI("https://" + kPinningDomain);
    gSSService.resetState(Ci.nsISiteSecurityService.HEADER_HPKP, uri, 0);
  });
  whenNewTabLoaded(window, loadPinningPage);
}

// Start by making a successful connection to a domain that will pin a site
function loadPinningPage() {
  BrowserTestUtils.loadURI(
    gBrowser.selectedBrowser,
    "https://" + kPinningDomain + kURLPath + "valid"
  ).then(function() {
    BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(() =>
      successfulPinningPageListener.handleEvent()
    );
  });
}

// After the site is pinned try to load with a subdomain site that should
// fail to validate
var successfulPinningPageListener = {
  handleEvent() {
    BrowserTestUtils.loadURI(
      gBrowser.selectedBrowser,
      "https://" + kBadPinningDomain
    )
      .then(function() {
        return BrowserTestUtils.waitForErrorPage(gBrowser.selectedBrowser);
      })
      .then(errorPageLoaded);
  },
};

// The browser should load about:neterror, when this happens, proceed
// to load the pinning domain again, this time removing the pinning information
function errorPageLoaded() {
  ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    let textElement = content.document.getElementById("errorShortDescText2");
    let text = textElement.innerHTML;
    ok(
      text.indexOf("MOZILLA_PKIX_ERROR_KEY_PINNING_FAILURE") > 0,
      "Got a pinning error page"
    );
  }).then(function() {
    BrowserTestUtils.loadURI(
      gBrowser.selectedBrowser,
      "https://" + kPinningDomain + kURLPath + "zeromaxagevalid"
    )
      .then(function() {
        return BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
      })
      .then(pinningRemovalLoaded);
  });
}

// After the pinning information has been removed (successful load) proceed
// to load again with the invalid pin domain.
function pinningRemovalLoaded() {
  BrowserTestUtils.loadURI(
    gBrowser.selectedBrowser,
    "https://" + kBadPinningDomain
  )
    .then(function() {
      return BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    })
    .then(badPinningPageLoaded);
}

// Finally, we should successfully load
// https://bad.include-subdomains.pinning-dynamic.example.com.
function badPinningPageLoaded() {
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  ok(true, "load complete");
  finish();
}
