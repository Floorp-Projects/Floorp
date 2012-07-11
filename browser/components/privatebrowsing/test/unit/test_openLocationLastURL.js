/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test the correct behavior of the openLocationLastURL.jsm JS module.

function run_test_on_service()
{
  let openLocationModule = {};
  // This variable fakes the window required for getting the PB flag
  let window = { gPrivateBrowsingUI: { privateWindow: false } };
  Cu.import("resource:///modules/openLocationLastURL.jsm", openLocationModule);
  let gOpenLocationLastURL = new openLocationModule.OpenLocationLastURL(window);
  
  function clearHistory() {
    // simulate clearing the private data
    Cc["@mozilla.org/observer-service;1"].
    getService(Ci.nsIObserverService).
    notifyObservers(null, "browser:purge-session-history", "");
  }
  
  let pb = Cc[PRIVATEBROWSING_CONTRACT_ID].
           getService(Ci.nsIPrivateBrowsingService);
  let pref = Cc["@mozilla.org/preferences-service;1"].
             getService(Ci.nsIPrefBranch);
  gOpenLocationLastURL.reset();

  do_check_eq(typeof gOpenLocationLastURL, "object");
  do_check_eq(gOpenLocationLastURL.value, "");

  function switchPrivateBrowsing(flag) {
    pb.privateBrowsingEnabled = flag;
    window.gPrivateBrowsingUI.privateWindow = flag;
  }

  const url1 = "mozilla.org";
  const url2 = "mozilla.com";

  gOpenLocationLastURL.value = url1;
  do_check_eq(gOpenLocationLastURL.value, url1);

  gOpenLocationLastURL.value = "";
  do_check_eq(gOpenLocationLastURL.value, "");

  gOpenLocationLastURL.value = url2;
  do_check_eq(gOpenLocationLastURL.value, url2);

  clearHistory();
  do_check_eq(gOpenLocationLastURL.value, "");
  gOpenLocationLastURL.value = url2;

  switchPrivateBrowsing(true);
  do_check_eq(gOpenLocationLastURL.value, "");
  
  switchPrivateBrowsing(false);
  do_check_eq(gOpenLocationLastURL.value, url2);
  switchPrivateBrowsing(true);

  gOpenLocationLastURL.value = url1;
  do_check_eq(gOpenLocationLastURL.value, url1);

  switchPrivateBrowsing(false);
  do_check_eq(gOpenLocationLastURL.value, url2);

  switchPrivateBrowsing(true);
  gOpenLocationLastURL.value = url1;
  do_check_neq(gOpenLocationLastURL.value, "");
  clearHistory();
  do_check_eq(gOpenLocationLastURL.value, "");

  switchPrivateBrowsing(false);
  do_check_eq(gOpenLocationLastURL.value, "");
}

// Support running tests on both the service itself and its wrapper
function run_test() {
  run_test_on_all_services();
}
