/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const PREF_RESTORE_ON_DEMAND = "browser.sessionstore.restore_on_demand";

function test() {
  TestRunner.run();
}

function runTests() {
  Services.prefs.setBoolPref(PREF_RESTORE_ON_DEMAND, false);
  registerCleanupFunction(function () {
    Services.prefs.clearUserPref(PREF_RESTORE_ON_DEMAND);
  });

  let state = { windows: [{ tabs: [
    { entries: [{ url: "http://example.com" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.com" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.com" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.com" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.com" }], extData: { "uniq": r() } },
    { entries: [{ url: "http://example.com" }], extData: { "uniq": r() } }
  ] }] };

  let expectedCounts = [
    [3, 3, 0],
    [2, 3, 1],
    [1, 3, 2],
    [0, 3, 3],
    [0, 2, 4],
    [0, 1, 5]
  ];

  let loadCount = 0;
  gProgressListener.setCallback(function (aBrowser, aNeedRestore, aRestoring, aRestored) {
    loadCount++;
    let expected = expectedCounts[loadCount - 1];

    is(aNeedRestore, expected[0], "load " + loadCount + " - # tabs that need to be restored");
    is(aRestoring, expected[1], "load " + loadCount + " - # tabs that are restoring");
    is(aRestored, expected[2], "load " + loadCount + " - # tabs that has been restored");

    if (loadCount == state.windows[0].tabs.length) {
      gProgressListener.unsetCallback();
      executeSoon(next);
    }
  });

  yield ss.setBrowserState(JSON.stringify(state));
}
