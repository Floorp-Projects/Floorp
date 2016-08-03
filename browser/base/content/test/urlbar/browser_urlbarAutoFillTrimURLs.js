/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test ensures that autoFilled values are not trimmed, unless the user
// selects from the autocomplete popup.

function test() {
  waitForExplicitFinish();

  const PREF_TRIMURL = "browser.urlbar.trimURLs";
  const PREF_AUTOFILL = "browser.urlbar.autoFill";

  registerCleanupFunction(function () {
    Services.prefs.clearUserPref(PREF_TRIMURL);
    Services.prefs.clearUserPref(PREF_AUTOFILL);
    gURLBar.handleRevert();
  });
  Services.prefs.setBoolPref(PREF_TRIMURL, true);
  Services.prefs.setBoolPref(PREF_AUTOFILL, true);

  // Adding a tab would hit switch-to-tab, so it's safer to just add a visit.
  let callback = {
    handleError:  function () {},
    handleResult: function () {},
    handleCompletion: continue_test
  };
  let history = Cc["@mozilla.org/browser/history;1"]
                  .getService(Ci.mozIAsyncHistory);
  history.updatePlaces({ uri: NetUtil.newURI("http://www.autofilltrimurl.com/whatever")
                       , visits: [ { transitionType: Ci.nsINavHistoryService.TRANSITION_TYPED
                                   , visitDate:      Date.now() * 1000
                                   } ]
                       }, callback);
}

function continue_test() {
  function test_autoFill(aTyped, aExpected, aCallback) {
    info(`Testing with input: ${aTyped}`);
    gURLBar.inputField.value = aTyped.substr(0, aTyped.length - 1);
    gURLBar.focus();
    gURLBar.selectionStart = aTyped.length - 1;
    gURLBar.selectionEnd = aTyped.length - 1;

    EventUtils.synthesizeKey(aTyped.substr(-1), {});
    waitForSearchComplete(function () {
      info(`Got value: ${gURLBar.textValue}`);
      is(gURLBar.textValue, aExpected, "Autofilled value is as expected");
      aCallback();
    });
  }

  test_autoFill("http://", "http://", function () {
    test_autoFill("http://au", "http://autofilltrimurl.com/", function () {
      test_autoFill("http://www.autofilltrimurl.com", "http://www.autofilltrimurl.com/", function () {
        // Now ensure selecting from the popup correctly trims.
        is(gURLBar.controller.matchCount, 2, "Found the expected number of matches");
        EventUtils.synthesizeKey("VK_DOWN", {});
        is(gURLBar.textValue, "www.autofilltrimurl.com/whatever", "trim was applied correctly");
        gURLBar.closePopup();
        PlacesTestUtils.clearHistory().then(finish);
      });
    });
  });
}

var gOnSearchComplete = null;
function waitForSearchComplete(aCallback) {
  info("Waiting for onSearchComplete");
  if (!gOnSearchComplete) {
    gOnSearchComplete = gURLBar.onSearchComplete;
    registerCleanupFunction(() => {
      gURLBar.onSearchComplete = gOnSearchComplete;
    });
  }
  gURLBar.onSearchComplete = function () {
    ok(gURLBar.popupOpen, "The autocomplete popup is correctly open");
    gOnSearchComplete.apply(gURLBar);
    aCallback();
  }
}
