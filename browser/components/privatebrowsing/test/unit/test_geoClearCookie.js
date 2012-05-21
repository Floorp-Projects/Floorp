/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test to ensure the geolocation token is cleared when changing the private
// browsing mode

const accessToken = '{"location":{"latitude":51.5090332,"longitude":-0.1212726,"accuracy":150.0},"access_token":"2:jVhRZJ-j6PiRchH_:RGMrR0W1BiwdZs12"}'
function run_test_on_service() {
  var prefBranch = Cc["@mozilla.org/preferences-service;1"].
                   getService(Ci.nsIPrefBranch);
  var pb = Cc[PRIVATEBROWSING_CONTRACT_ID].
           getService(Ci.nsIPrivateBrowsingService);
  prefBranch.setCharPref("geo.wifi.access_token.test", accessToken);
  var token = prefBranch.getCharPref("geo.wifi.access_token.test");
  do_check_eq(token, accessToken);
  pb.privateBrowsingEnabled = true;
  token = "";
  try {
    token = prefBranch.getCharPref("geo.wifi.access_token.test");
  }
  catch(e){}
  finally {
    do_check_eq(token, "");
  }
  token = "";
  prefBranch.setCharPref("geo.wifi.access_token.test", accessToken);
  pb.privateBrowsingEnabled = false;
  try {
    token = prefBranch.getCharPref("geo.wifi.access_token.test");
  }
  catch(e){}
  finally {
    do_check_eq(token, "");
  }
}

// Support running tests on both the service itself and its wrapper
function run_test() {
  run_test_on_all_services();
}
