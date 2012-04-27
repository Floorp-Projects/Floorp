/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gPrivateBrowsing",
                                   PRIVATEBROWSING_CONTRACT_ID,
                                   "nsIPrivateBrowsingService");

function waitForTransition(aEnabled, aCallback) {
  Services.obs.addObserver(function PBT_transition(aSubject, aTopic, aData) {
    Services.obs.removeObserver(PBT_transition, aTopic, false);
    // Telemetry data is recorded just after the private browsing transition
    // observers are notified, thus we must wait for this observer to return.
    do_execute_soon(aCallback);
  }, "private-browsing-transition-complete", false);
  gPrivateBrowsing.privateBrowsingEnabled = aEnabled;
}

function checkHistogram(aId) {
  // Check that we have data either in the first bucket (that doesn't
  // count towards the sum) or one of the other buckets, by checking
  // the sum of the values.
  let snapshot = Services.telemetry.getHistogramById(aId).snapshot();
  do_check_true(snapshot.sum > 0 || snapshot.counts[0] > 0);
}

function do_test() {
  do_test_pending();

  waitForTransition(true, function PBT_enabled() {
    waitForTransition(false, function PBT_disabled() {
      checkHistogram("PRIVATE_BROWSING_TRANSITION_ENTER_PREPARATION_MS");
      checkHistogram("PRIVATE_BROWSING_TRANSITION_ENTER_TOTAL_MS");
      checkHistogram("PRIVATE_BROWSING_TRANSITION_EXIT_PREPARATION_MS");
      checkHistogram("PRIVATE_BROWSING_TRANSITION_EXIT_TOTAL_MS");

      do_test_finished();
    });
  });
}
