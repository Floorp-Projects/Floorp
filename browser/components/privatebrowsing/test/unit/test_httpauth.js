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

// This test makes sure the HTTP authenticated sessions are correctly cleared
// when entering and leaving the private browsing mode.

function run_test() {
  var prefBranch = Cc["@mozilla.org/preferences-service;1"].
                   getService(Ci.nsIPrefBranch);
  prefBranch.setBoolPref("browser.privatebrowsing.keep_current_session", true);

  var pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);

  var am = Cc["@mozilla.org/network/http-auth-manager;1"].
           getService(Ci.nsIHttpAuthManager);

  const kHost1 = "pbtest3.example.com";
  const kHost2 = "pbtest4.example.com";
  const kPort = 80;
  const kHTTP = "http";
  const kBasic = "basic";
  const kRealm = "realm";
  const kDomain = "example.com";
  const kUser = "user";
  const kUser2 = "user2";
  const kPassword = "pass";
  const kPassword2 = "pass2";
  const kEmpty = "";

  try {
    var domain = {value: kEmpty}, user = {value: kEmpty}, pass = {value: kEmpty};
    // simulate a login via HTTP auth outside of the private mode
    am.setAuthIdentity(kHTTP, kHost1, kPort, kBasic, kRealm, kEmpty, kDomain, kUser, kPassword);
    // make sure the recently added auth entry is available outside the private browsing mode
    am.getAuthIdentity(kHTTP, kHost1, kPort, kBasic, kRealm, kEmpty, domain, user, pass);
    do_check_eq(domain.value, kDomain);
    do_check_eq(user.value, kUser);
    do_check_eq(pass.value, kPassword);
    // enter private browsing mode
    pb.privateBrowsingEnabled = true;
    // make sure the added auth entry is no longer accessible
    domain = {value: kEmpty}, user = {value: kEmpty}, pass = {value: kEmpty};
    try {
      // should throw
      am.getAuthIdentity(kHTTP, kHost1, kPort, kBasic, kRealm, kEmpty, domain, user, pass);
      do_throw("Auth entry should not be retrievable after entering the private browsing mode");
    } catch (e) {
      do_check_eq(domain.value, kEmpty);
      do_check_eq(user.value, kEmpty);
      do_check_eq(pass.value, kEmpty);
    }

    // simulate a login via HTTP auth inside of the private mode
    am.setAuthIdentity(kHTTP, kHost2, kPort, kBasic, kRealm, kEmpty, kDomain, kUser2, kPassword2);
    // make sure the recently added auth entry is available outside the private browsing mode
    domain = {value: kEmpty}, user = {value: kEmpty}, pass = {value: kEmpty};
    am.getAuthIdentity(kHTTP, kHost2, kPort, kBasic, kRealm, kEmpty, domain, user, pass);
    do_check_eq(domain.value, kDomain);
    do_check_eq(user.value, kUser2);
    do_check_eq(pass.value, kPassword2);
    // exit private browsing mode
    pb.privateBrowsingEnabled = false;
    // make sure the added auth entry is no longer accessible
    domain = {value: kEmpty}, user = {value: kEmpty}, pass = {value: kEmpty};
    try {
      // should throw
      am.getAuthIdentity(kHTTP, kHost2, kPort, kBasic, kRealm, kEmpty, domain, user, pass);
      do_throw("Auth entry should not be retrievable after exiting the private browsing mode");
    } catch (e) {
      do_check_eq(domain.value, kEmpty);
      do_check_eq(user.value, kEmpty);
      do_check_eq(pass.value, kEmpty);
    }
  } catch (e) {
    do_throw("Unexpected exception while testing HTTP auth manager: " + e);
  } finally {
    prefBranch.clearUserPref("browser.privatebrowsing.keep_current_session");
  }
}
