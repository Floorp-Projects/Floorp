/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

function run_test_on_service() {
  // initialization
  var pb = Cc[PRIVATEBROWSING_CONTRACT_ID].
           getService(Ci.nsIPrivateBrowsingService);

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
}

// Support running tests on both the service itself and its wrapper
function run_test() {
  run_test_on_all_services();
}
