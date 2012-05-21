/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This tests the private browsing service to make sure it no longer switches
// the offline status (see bug 463256).

function run_test_on_service() {
  // initialization
  var pb = Cc[PRIVATEBROWSING_CONTRACT_ID].
           getService(Ci.nsIPrivateBrowsingService);
  var os = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);

  var observer = {
    observe: function (aSubject, aTopic, aData) {
      if (aTopic == "network:offline-status-changed")
        this.events.push(aData);
    },
    events: []
  };
  os.addObserver(observer, "network:offline-status-changed", false);

  // enter the private browsing mode, and wait for the about:pb page to load
  pb.privateBrowsingEnabled = true;
  do_check_eq(observer.events.length, 0);

  // leave the private browsing mode, and wait for the SSL page to load again
  pb.privateBrowsingEnabled = false;
  do_check_eq(observer.events.length, 0);

  os.removeObserver(observer, "network:offline-status-changed", false);
}

// Support running tests on both the service itself and its wrapper
function run_test() {
  run_test_on_all_services();
}
