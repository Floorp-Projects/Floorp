/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that certificate exceptions UI behaves correctly
// inside the private browsing mode, based on whether it's opened from the prefs
// window or from the SSL error page (see bug 461627).

function test() {
  // initialization
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);

  const EXCEPTIONS_DLG_URL = 'chrome://pippki/content/exceptionDialog.xul';
  const EXCEPTIONS_DLG_FEATURES = 'chrome,centerscreen';
  const INVALID_CERT_LOCATION = 'https://nocert.example.com/';
  waitForExplicitFinish();

  // enter private browsing mode
  pb.privateBrowsingEnabled = true;

  step1();

  // Test the certificate exceptions dialog as it is invoked from about:certerror
  function step1() {
    let params = {
      exceptionAdded : false,
      location: INVALID_CERT_LOCATION,
      handlePrivateBrowsing : true,
      prefetchCert: true,
    };
    function testCheckbox() {
      win.removeEventListener("load", testCheckbox, false);
      Services.obs.addObserver(function (aSubject, aTopic, aData) {
        Services.obs.removeObserver(arguments.callee, "cert-exception-ui-ready", false);
        ok(win.gCert, "The certificate information should be available now");

        let checkbox = win.document.getElementById("permanent");
        ok(checkbox.hasAttribute("disabled"),
          "the permanent checkbox should be disabled when handling the private browsing mode");
        ok(!checkbox.hasAttribute("checked"),
          "the permanent checkbox should not be checked when handling the private browsing mode");
        win.close();
        step2();
      }, "cert-exception-ui-ready", false);
    }
    var win = openDialog(EXCEPTIONS_DLG_URL, "", EXCEPTIONS_DLG_FEATURES, params);
    win.addEventListener("load", testCheckbox, false);
  }

  // Test the certificate excetions dialog as it is invoked from the Preferences dialog
  function step2() {
    let params = {
      exceptionAdded : false,
      location: INVALID_CERT_LOCATION,
      prefetchCert: true,
    };
    function testCheckbox() {
      win.removeEventListener("load", testCheckbox, false);
      Services.obs.addObserver(function (aSubject, aTopic, aData) {
        Services.obs.removeObserver(arguments.callee, "cert-exception-ui-ready", false);
        ok(win.gCert, "The certificate information should be available now");

        let checkbox = win.document.getElementById("permanent");
        ok(!checkbox.hasAttribute("disabled"),
          "the permanent checkbox should not be disabled when not handling the private browsing mode");
        ok(checkbox.hasAttribute("checked"),
          "the permanent checkbox should be checked when not handling the private browsing mode");
        win.close();
        cleanup();
      }, "cert-exception-ui-ready", false);
    }
    var win = openDialog(EXCEPTIONS_DLG_URL, "", EXCEPTIONS_DLG_FEATURES, params);
    win.addEventListener("load", testCheckbox, false);
  }

  function cleanup() {
    // leave the private browsing mode
    pb.privateBrowsingEnabled = false;
    finish();
  }
}
