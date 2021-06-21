/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Interactions } = ChromeUtils.import(
  "resource:///modules/Interactions.jsm"
);

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "pageViewIdleTime",
  "browser.places.interactions.pageViewIdleTime",
  60
);

/**
 * Register the mock idleSerice.
 *
 * @param {Fun} registerCleanupFunction
 */
function disableIdleService() {
  let idleService = Cc["@mozilla.org/widget/useridleservice;1"].getService(
    Ci.nsIUserIdleService
  );
  idleService.removeIdleObserver(Interactions, pageViewIdleTime);

  registerCleanupFunction(() => {
    idleService.addIdleObserver(Interactions, pageViewIdleTime);
  });
}

async function assertDatabaseValues(expected) {
  await BrowserTestUtils.waitForCondition(
    () => Interactions._updateDatabase.callCount == expected.length,
    "Should have saved to the database"
  );

  let args = Interactions._updateDatabase.args;
  for (let i = 0; i < expected.length; i++) {
    let actual = args[i][0];
    Assert.equal(
      actual.url,
      expected[i].url,
      "Should have saved the page into the database"
    );
    if (expected[i].exactTotalViewTime) {
      Assert.equal(
        actual.totalViewTime,
        expected[i].exactTotalViewTime,
        "Should have kept the exact time"
      );
    } else {
      Assert.greater(
        actual.totalViewTime,
        expected[i].totalViewTime,
        "Should have stored the interaction time"
      );
    }
    if (expected[i].maxViewTime) {
      Assert.less(
        actual.totalViewTime,
        expected[i].maxViewTime,
        "Should have recorded an interaction below the maximum expected"
      );
    }
  }
}
