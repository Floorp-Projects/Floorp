/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Private Browsing Tests.
 *
 * The Initial Developer of the Original Code is
 * Ehsan Akhgari.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
