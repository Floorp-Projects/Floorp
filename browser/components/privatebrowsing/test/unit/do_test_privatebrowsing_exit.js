/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the private browsing mode is left at application
// shutdown.

function do_test() {
  // initialization
  var os = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);
  var pb = Cc[PRIVATEBROWSING_CONTRACT_ID].
           getService(Ci.nsIPrivateBrowsingService);

  var expectedQuitting;
  var called = 0;
  var observer = {
    observe: function(aSubject, aTopic, aData) {
      if (aTopic == kPrivateBrowsingNotification &&
          aData == kExit) {
        // increment the call counter
        ++ called;

        do_check_neq(aSubject, null);
        try {
          aSubject.QueryInterface(Ci.nsISupportsPRBool);
        } catch (ex) {
          do_throw("aSubject was not null, but wasn't an nsISupportsPRBool");
        }
        // check the "quitting" argument
        do_check_eq(aSubject.data, expectedQuitting);

        // finish up the test
        if (expectedQuitting) {
          os.removeObserver(this, kPrivateBrowsingNotification);
          do_test_finished();
        }
      }
    }
  };

  // set the observer
  os.addObserver(observer, kPrivateBrowsingNotification, false);

  // enter the private browsing mode
  pb.privateBrowsingEnabled = true;

  // exit the private browsing mode
  expectedQuitting = false;
  pb.privateBrowsingEnabled = false;
  do_check_eq(called, 1);

  // enter the private browsing mode
  pb.privateBrowsingEnabled = true;

  // Simulate an exit
  expectedQuitting = true;
  do_test_pending();
  os.notifyObservers(null, "quit-application-granted", null);
}
