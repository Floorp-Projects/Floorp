/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure the HTTP authenticated sessions are correctly cleared
// when entering and leaving the private browsing mode.

function run_test_on_service() {
  var pb = Cc[PRIVATEBROWSING_CONTRACT_ID].
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
  }
}

// Support running tests on both the service itself and its wrapper
function run_test() {
  run_test_on_all_services();
}
