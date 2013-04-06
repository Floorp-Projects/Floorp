/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that certificate exceptions UI behaves correctly
// in private browsing windows, based on whether it's opened from the prefs
// window or from the SSL error page (see bug 461627).

function test() {
  const EXCEPTIONS_DLG_URL = 'chrome://pippki/content/exceptionDialog.xul';
  const EXCEPTIONS_DLG_FEATURES = 'chrome,centerscreen';
  const INVALID_CERT_LOCATION = 'https://nocert.example.com/';
  waitForExplicitFinish();

  // open a private browsing window
  var pbWin = OpenBrowserWindow({private: true});
  pbWin.addEventListener("load", function onLoad() {
    pbWin.removeEventListener("load", onLoad, false);
    doTest();
  }, false);

  // Test the certificate exceptions dialog
  function doTest() {
    let params = {
      exceptionAdded : false,
      location: INVALID_CERT_LOCATION,
      prefetchCert: true,
    };
    function testCheckbox() {
      win.removeEventListener("load", testCheckbox, false);
      Services.obs.addObserver(function onCertUI(aSubject, aTopic, aData) {
        Services.obs.removeObserver(onCertUI, "cert-exception-ui-ready");
        ok(win.gCert, "The certificate information should be available now");

        let checkbox = win.document.getElementById("permanent");
        ok(checkbox.hasAttribute("disabled"),
          "the permanent checkbox should be disabled when handling the private browsing mode");
        ok(!checkbox.hasAttribute("checked"),
          "the permanent checkbox should not be checked when handling the private browsing mode");
        win.close();
        cleanup();
      }, "cert-exception-ui-ready", false);
    }
    var win = pbWin.openDialog(EXCEPTIONS_DLG_URL, "", EXCEPTIONS_DLG_FEATURES, params);
    win.addEventListener("load", testCheckbox, false);
  }

  function cleanup() {
    // close the private browsing window
    pbWin.close();
    finish();
  }
}
