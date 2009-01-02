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
 * Portions created by the Initial Developer are Copyright (C) 2008
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

// This test makes sure that the about:privatebrowsing page is available inside
// and outside of the private mode.

function is_about_privatebrowsing_available() {
  try {
    var ios = Cc["@mozilla.org/network/io-service;1"].
              getService(Ci.nsIIOService);
    var channel = ios.newChannel("about:privatebrowsing", null, null);
    var input = channel.open();
    var sinput = Cc["@mozilla.org/scriptableinputstream;1"].
                 createInstance(Ci.nsIScriptableInputStream);
    sinput.init(input);
    while (true)
      if (!sinput.read(1024).length)
        break;
    sinput.close();
    input.close();
    return true;
  } catch (ex if ("result" in ex && ex.result == Cr.NS_ERROR_MALFORMED_URI)) { // expected
  }

  return false;
}

function run_test() {
  // initialization
  var pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  var prefBranch = Cc["@mozilla.org/preferences-service;1"].
                   getService(Ci.nsIPrefBranch);
  prefBranch.setBoolPref("browser.privatebrowsing.keep_current_session", true);

  try {
    // about:privatebrowsing should be available before entering the private mode
    do_check_true(is_about_privatebrowsing_available());

    // enter the private browsing mode
    pb.privateBrowsingEnabled = true;

    // about:privatebrowsing should be available inside the private mode
    do_check_true(is_about_privatebrowsing_available());

    // exit the private browsing mode
    pb.privateBrowsingEnabled = false;

    // about:privatebrowsing should be available after leaving the private mode
    do_check_true(is_about_privatebrowsing_available());
  } finally {
    prefBranch.clearUserPref("browser.privatebrowsing.keep_current_session");
  }
}
