/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  const URL_1 = "mozilla.org";
  const URL_2 = "mozilla.com";

  let openLocationLastURL = getLocationModule();
  let privateBrowsingService =
    Cc["@mozilla.org/privatebrowsing;1"].
      getService(Ci.nsIPrivateBrowsingService);

  function clearHistory() {
    Services.obs.notifyObservers(null, "browser:purge-session-history", "");
  }
  function testURL(aTestNumber, aValue) {
    is(openLocationLastURL.value, aValue,
       "Test: " + aTestNumber + ": Validate last url value.");
  }

  // Clean to start testing.
  is(typeof openLocationLastURL, "object", "Validate type of last url.");
  openLocationLastURL.reset();
  testURL(1, "");

  // Test without private browsing.
  openLocationLastURL.value = URL_1;
  testURL(2, URL_1);
  openLocationLastURL.value = "";
  testURL(3, "");
  openLocationLastURL.value = URL_2;
  testURL(4, URL_2);
  clearHistory();
  testURL(5, "");

  // Test changing private browsing.
  openLocationLastURL.value = URL_2;
  privateBrowsingService.privateBrowsingEnabled = true;
  testURL(6, "");
  privateBrowsingService.privateBrowsingEnabled = false;
  testURL(7, URL_2);
  privateBrowsingService.privateBrowsingEnabled = true;
  openLocationLastURL.value = URL_1;
  testURL(8, URL_1);
  privateBrowsingService.privateBrowsingEnabled = false;
  testURL(9, URL_2);
  privateBrowsingService.privateBrowsingEnabled = true;
  openLocationLastURL.value = URL_1;
  testURL(10, URL_1);

  // Test cleaning history.
  clearHistory();
  testURL(11, "");
  privateBrowsingService.privateBrowsingEnabled = false;
  testURL(12, "");
}

function getLocationModule() {
  let openLocationModule = {};
  Cu.import("resource:///modules/openLocationLastURL.jsm", openLocationModule);
  return new openLocationModule.OpenLocationLastURL(window);
}
