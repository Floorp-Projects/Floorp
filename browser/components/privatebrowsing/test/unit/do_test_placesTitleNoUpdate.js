/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test to make sure that the visited page titles do not get updated inside the
// private browsing mode.

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PlacesUtils.jsm");

function do_test()
{
  let pb = Cc[PRIVATEBROWSING_CONTRACT_ID].
           getService(Ci.nsIPrivateBrowsingService);

  const TEST_URI = uri("http://mozilla.com/privatebrowsing");
  const TITLE_1 = "Title 1";
  const TITLE_2 = "Title 2";

  do_test_pending();
  waitForClearHistory(function () {
    let place = {
      uri: TEST_URI,
      title: TITLE_1,
      visits: [{
        visitDate: Date.now() * 1000,
        transitionType: Ci.nsINavHistoryService.TRANSITION_LINK
      }]
    };
    PlacesUtils.asyncHistory.updatePlaces(place, {
      handleError: function () do_throw("Unexpected error in adding visit."),
      handleResult: function () { },
      handleCompletion: function () afterAddFirstVisit()
    });
  });

  function afterAddFirstVisit()
  {
    do_check_eq(PlacesUtils.history.getPageTitle(TEST_URI), TITLE_1);

    waitForClearHistory(do_test_finished);
  }
}

// Support running tests on both the service itself and its wrapper
function run_test() {
  run_test_on_all_services();
}

function waitForClearHistory(aCallback) {
  let observer = {
    observe: function(aSubject, aTopic, aData) {
      Services.obs.removeObserver(this, PlacesUtils.TOPIC_EXPIRATION_FINISHED);
      aCallback();
    }
  };
  Services.obs.addObserver(observer, PlacesUtils.TOPIC_EXPIRATION_FINISHED, false);

  PlacesUtils.bhistory.removeAllPages();
}
