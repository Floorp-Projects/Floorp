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

// This tests the private browsing service to make sure it implements its
// documented interface correctly.

// This test should run before the rest of private browsing service unit tests,
// hence the naming used for this file.

function run_test_on_service() {
  // initialization
  var os = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);

  // the contract ID should be available
  do_check_true(PRIVATEBROWSING_CONTRACT_ID in Cc);

  // the interface should be available
  do_check_true("nsIPrivateBrowsingService" in Ci);

  // it should be possible to initialize the component
  try {
    var pb = Cc[PRIVATEBROWSING_CONTRACT_ID].
             getService(Ci.nsIPrivateBrowsingService);
  } catch (ex) {
    LOG("exception thrown when trying to get the service: " + ex);
    do_throw("private browsing service could not be initialized");
  }

  // private browsing should be turned off initially
  do_check_false(pb.privateBrowsingEnabled);
  // private browsing not auto-started
  do_check_false(pb.autoStarted);

  // it should be possible to toggle its status
  pb.privateBrowsingEnabled = true;
  do_check_true(pb.privateBrowsingEnabled);
  do_check_false(pb.autoStarted);
  pb.privateBrowsingEnabled = false;
  do_check_false(pb.privateBrowsingEnabled);
  do_check_false(pb.autoStarted);

  // test the private-browsing notification
  var observer = {
    observe: function(aSubject, aTopic, aData) {
      if (aTopic == kPrivateBrowsingNotification)
        this.data = aData;
    },
    data: null
  };
  os.addObserver(observer, kPrivateBrowsingNotification, false);
  pb.privateBrowsingEnabled = true;
  do_check_eq(observer.data, kEnter);
  pb.privateBrowsingEnabled = false;
  do_check_eq(observer.data, kExit);
  os.removeObserver(observer, kPrivateBrowsingNotification);

  // make sure that setting the private browsing mode from within an observer throws
  observer = {
    observe: function(aSubject, aTopic, aData) {
      if (aTopic == kPrivateBrowsingNotification) {
        try {
          pb.privateBrowsingEnabled = (aData == kEnter);
          do_throw("Setting privateBrowsingEnabled inside the " + aData +
            " notification should throw");
        } catch (ex) {
          if (!("result" in ex && ex.result == Cr.NS_ERROR_FAILURE))
            do_throw("Unexpected exception caught: " + ex);
        }
      }
    }
  };
  os.addObserver(observer, kPrivateBrowsingNotification, false);
  pb.privateBrowsingEnabled = true;
  do_check_true(pb.privateBrowsingEnabled); // the exception should not interfere with the mode change
  pb.privateBrowsingEnabled = false;
  do_check_false(pb.privateBrowsingEnabled); // the exception should not interfere with the mode change
  os.removeObserver(observer, kPrivateBrowsingNotification);

  // make sure that getting the private browsing mode from within an observer doesn't throw
  observer = {
    observe: function(aSubject, aTopic, aData) {
      if (aTopic == kPrivateBrowsingNotification) {
        try {
          var dummy = pb.privateBrowsingEnabled;
          if (aData == kEnter)
            do_check_true(dummy);
          else if (aData == kExit)
            do_check_false(dummy);
        } catch (ex) {
          do_throw("Unexpected exception caught: " + ex);
        }
      }
    }
  };
  os.addObserver(observer, kPrivateBrowsingNotification, false);
  pb.privateBrowsingEnabled = true;
  do_check_true(pb.privateBrowsingEnabled); // just a sanity check
  pb.privateBrowsingEnabled = false;
  do_check_false(pb.privateBrowsingEnabled); // just a sanity check
  os.removeObserver(observer, kPrivateBrowsingNotification);

  // check that the private-browsing-cancel-vote notification is sent before the
  // private-browsing notification
  observer = {
    observe: function(aSubject, aTopic, aData) {
      switch (aTopic) {
      case kPrivateBrowsingCancelVoteNotification:
      case kPrivateBrowsingNotification:
        this.notifications.push(aTopic + " " + aData);
      }
    },
    notifications: []
  };
  os.addObserver(observer, kPrivateBrowsingCancelVoteNotification, false);
  os.addObserver(observer, kPrivateBrowsingNotification, false);
  pb.privateBrowsingEnabled = true;
  do_check_true(pb.privateBrowsingEnabled); // just a sanity check
  pb.privateBrowsingEnabled = false;
  do_check_false(pb.privateBrowsingEnabled); // just a sanity check
  os.removeObserver(observer, kPrivateBrowsingNotification);
  os.removeObserver(observer, kPrivateBrowsingCancelVoteNotification);
  var reference_order = [
      kPrivateBrowsingCancelVoteNotification + " " + kEnter,
      kPrivateBrowsingNotification + " " + kEnter,
      kPrivateBrowsingCancelVoteNotification + " " + kExit,
      kPrivateBrowsingNotification + " " + kExit
    ];
  do_check_eq(observer.notifications.join(","), reference_order.join(","));

  // make sure that the private-browsing-cancel-vote notification can be used
  // to cancel the mode switch
  observer = {
    observe: function(aSubject, aTopic, aData) {
      switch (aTopic) {
      case kPrivateBrowsingCancelVoteNotification:
        do_check_neq(aSubject, null);
        try {
          aSubject.QueryInterface(Ci.nsISupportsPRBool);
        } catch (ex) {
          do_throw("aSubject in " + kPrivateBrowsingCancelVoteNotification +
            " should implement nsISupportsPRBool");
        }
        do_check_false(aSubject.data);
        aSubject.data = true; // cancel the mode switch

        // fall through
      case kPrivateBrowsingNotification:
        this.notifications.push(aTopic + " " + aData);
      }
    },
    nextPhase: function() {
      this.notifications.push("enter phase " + (++this._phase));
    },
    notifications: [],
    _phase: 0
  };
  os.addObserver(observer, kPrivateBrowsingCancelVoteNotification, false);
  os.addObserver(observer, kPrivateBrowsingNotification, false);
  pb.privateBrowsingEnabled = true;
  do_check_false(pb.privateBrowsingEnabled); // should have been canceled
  // temporarily disable the observer
  os.removeObserver(observer, kPrivateBrowsingCancelVoteNotification);
  observer.nextPhase();
  pb.privateBrowsingEnabled = true; // this time, should enter successfully
  do_check_true(pb.privateBrowsingEnabled); // should have been canceled
  // re-enable the observer
  os.addObserver(observer, kPrivateBrowsingCancelVoteNotification, false);
  pb.privateBrowsingEnabled = false;
  do_check_true(pb.privateBrowsingEnabled); // should have been canceled
  os.removeObserver(observer, kPrivateBrowsingCancelVoteNotification);
  observer.nextPhase();
  pb.privateBrowsingEnabled = false; // this time, should exit successfully
  do_check_false(pb.privateBrowsingEnabled);
  os.removeObserver(observer, kPrivateBrowsingNotification);
  reference_order = [
      kPrivateBrowsingCancelVoteNotification + " " + kEnter,
      "enter phase 1",
      kPrivateBrowsingNotification + " " + kEnter,
      kPrivateBrowsingCancelVoteNotification + " " + kExit,
      "enter phase 2",
      kPrivateBrowsingNotification + " " + kExit,
    ];
  do_check_eq(observer.notifications.join(","), reference_order.join(","));

  // make sure that the private browsing transition complete notification is
  // raised correctly.
  observer = {
    observe: function(aSubject, aTopic, aData) {
      this.notifications.push(aTopic + " " + aData);
    },
    notifications: []
  };
  os.addObserver(observer, kPrivateBrowsingNotification, false);
  os.addObserver(observer, kPrivateBrowsingTransitionCompleteNotification, false);
  pb.privateBrowsingEnabled = true;
  pb.privateBrowsingEnabled = false;
  os.removeObserver(observer, kPrivateBrowsingNotification);
  os.removeObserver(observer, kPrivateBrowsingTransitionCompleteNotification);
  reference_order = [
    kPrivateBrowsingNotification + " " + kEnter,
    kPrivateBrowsingTransitionCompleteNotification + " ",
    kPrivateBrowsingNotification + " " + kExit,
    kPrivateBrowsingTransitionCompleteNotification + " ",
  ];
  do_check_eq(observer.notifications.join(","), reference_order.join(","));
}

// Support running tests on both the service itself and its wrapper
function run_test() {
  run_test_on_all_services();
}
